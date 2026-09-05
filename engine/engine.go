package engine

import (
	"bufio"
	"os"
	"path/filepath"
	"strings"
	"unicode"
)

type InputMode int

const (
	ModeTelex InputMode = 0
	ModeVNI   InputMode = 1
)

type Engine struct {
	Mode            InputMode
	Modern          bool // true = new style hòa
	SpellCheck      bool
	LastSuggestions []string
	buffer          []rune
}

func loadModernFromConfig() bool {
	home, err := os.UserHomeDir()
	if err != nil {
		return true
	}
	cfgPath := filepath.Join(home, ".config", "gotiengviet", "config")
	f, err := os.Open(cfgPath)
	if err != nil {
		return true
	}
	defer f.Close()
	sc := bufio.NewScanner(f)
	for sc.Scan() {
		line := strings.TrimSpace(sc.Text())
		if strings.HasPrefix(line, "modern=") {
			v := strings.TrimSpace(strings.SplitN(line, "=", 2)[1])
			return v == "true" || v == "True" || v == "1"
		}
	}
	return true
}

func loadSpellCheckFromConfig() bool {
	home, err := os.UserHomeDir()
	if err != nil {
		return true
	}
	cfgPath := filepath.Join(home, ".config", "gotiengviet", "config")
	f, err := os.Open(cfgPath)
	if err != nil {
		return true
	}
	defer f.Close()
	sc := bufio.NewScanner(f)
	for sc.Scan() {
		line := strings.TrimSpace(sc.Text())
		if strings.HasPrefix(line, "spellcheck=") {
			v := strings.TrimSpace(strings.SplitN(line, "=", 2)[1])
			return v == "true" || v == "True" || v == "1"
		}
	}
	return true
}

func loadMethodFromConfig() InputMode {
	home, err := os.UserHomeDir()
	if err != nil {
		return ModeTelex
	}
	cfgPath := filepath.Join(home, ".config", "gotiengviet", "config")
	f, err := os.Open(cfgPath)
	if err != nil {
		return ModeTelex
	}
	defer f.Close()
	sc := bufio.NewScanner(f)
	for sc.Scan() {
		line := strings.TrimSpace(sc.Text())
		if strings.HasPrefix(line, "method=") {
			v := strings.TrimSpace(strings.SplitN(line, "=", 2)[1])
			if v == "vni" || v == "VNI" {
				return ModeVNI
			}
		}
	}
	return ModeTelex
}

func NewEngine(mode InputMode) *Engine {
	return &Engine{Mode: mode, Modern: loadModernFromConfig(), SpellCheck: loadSpellCheckFromConfig(), buffer: []rune{}}
}

func NewEngineFromConfig() *Engine {
	return &Engine{Mode: loadMethodFromConfig(), Modern: loadModernFromConfig(), SpellCheck: loadSpellCheckFromConfig(), buffer: []rune{}}
}

func (e *Engine) SetMode(mode InputMode) { e.Mode = mode }
func (e *Engine) Reset()                 { e.buffer = []rune{} }
func (e *Engine) Buffer() string         { return string(e.buffer) }

// isWordBreak decides if rune terminates current syllable (commit)
func isWordBreak(r rune) bool {
	if r == ' ' || r == '\t' || r == '\n' || r == '\r' {
		return true
	}
	// Đừng coi ':' là word break để :smile: được xử lý như một từ cho emoji
	if r == ':' {
		return false
	}
	if unicode.IsPunct(r) || unicode.IsSymbol(r) {
		return true
	}
	return false
}

// ProcessKey processes one incoming keystroke.
// Returns: newComposing (current buffer after processing), backspaces (how many chars to delete before output), commit (string to commit if word break), consumed
// For IBus: if commit != "" -> commit text and clear composing
// If backspaces>0 -> need to delete and retype newComposing
func (e *Engine) ProcessKey(key rune) (newComposing string, backspaces int, commit string) {
	// Backspace handling
	if key == '\b' || key == 127 { // backspace
		if len(e.buffer) > 0 {
			e.buffer = e.buffer[:len(e.buffer)-1]
			return string(e.buffer), 1, ""
		}
		return "", 0, ""
	}

	// Word boundary: space, punctuation, enter - xử lý macro/emoji trước khi commit
	if isWordBreak(key) {
		word := string(e.buffer)
		// Macro/Emoji: vn + space -> Việt Nam, :smile: + space -> 😊
		if expanded, ok := ExpandMacro(word); ok {
			word = expanded
		} else if expanded, ok := ExpandEmoji(word); ok {
			word = expanded
		}
		if e.SpellCheck && word != "" && !IsValidVietnameseWord(word) {
			e.LastSuggestions = SuggestCorrections(word)
		} else {
			e.LastSuggestions = nil
		}
		committed := word + string(key)
		e.buffer = []rune{}
		return "", 0, committed
	}

	// For VNI mode, digits 0-9 are potential control keys, don't treat as word break
	// Try transform
	oldLen := len(e.buffer)
	oldStr := string(e.buffer)
	var newBuf []rune
	var consumed bool

	switch e.Mode {
	case ModeTelex:
		newBuf, consumed = TelexTransform(e.buffer, key, e.Modern)
		if consumed {
			e.buffer = newBuf
			if string(newBuf) == oldStr {
				return string(newBuf), 0, ""
			}
			return string(newBuf), oldLen, ""
		}
		// Not consumed -> append
		e.buffer = append(e.buffer, key)
		return string(e.buffer), 0, ""

	case ModeVNI:
		// Check if key is VNI control digit
		if key >= '0' && key <= '9' {
			newBuf, consumed = VNITransform(e.buffer, key, e.Modern)
			if consumed {
				e.buffer = newBuf
				if string(newBuf) == oldStr {
					return string(newBuf), 0, ""
				}
				return string(newBuf), oldLen, ""
			}
			// Not consumed digit -> if digit should be literal, append it as word char? But digits are usually separate.
			// We'll append digit and also trigger commit? For VNI, typing digit without vowel should remain digit.
			e.buffer = append(e.buffer, key)
			return string(e.buffer), 0, ""
		}
		// Letter key
		e.buffer = append(e.buffer, key)
		return string(e.buffer), 0, ""
	}

	return string(e.buffer), 0, ""
}
