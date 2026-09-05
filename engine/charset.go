package engine

import "unicode"

// Diacritic types
const (
	DiacriticNone       = 0
	DiacriticBreve      = 1 // ă
	DiacriticCircumflex = 2 // â ê ô
	DiacriticHorn       = 3 // ơ ư
	DiacriticStroke     = 4 // đ
)

// Tone types: 0 none, 1 sắc, 2 huyền, 3 hỏi, 4 ngã, 5 nặng
const (
	ToneNone  = 0
	ToneSac   = 1 // s / 1
	ToneHuyen = 2 // f / 2
	ToneHoi   = 3 // r / 3
	ToneNga   = 4 // x / 4
	ToneNang  = 5 // j / 5
)

type CharInfo struct {
	Bare      rune // 'a','e','i','o','u','y','d' lower
	Diacritic int
	Tone      int
	IsUpper   bool
}

var charInfoMap map[rune]CharInfo
var reverseMap map[CharInfo]rune

func init() {
	charInfoMap = make(map[rune]CharInfo)
	reverseMap = make(map[CharInfo]rune)

	// Helper to register
	register := func(r rune, bare rune, dia, tone int, upper bool) {
		info := CharInfo{Bare: bare, Diacritic: dia, Tone: tone, IsUpper: upper}
		charInfoMap[r] = info
		reverseMap[info] = r
	}

	// Define groups: each string first char is base without tone, following 5 are tones 1-5
	// Lowercase groups
	lowerGroups := []struct {
		bare rune
		dia  int
		chars string // len 6: base + 5 toned
	}{
		{'a', DiacriticNone, "aáàảãạ"},
		{'a', DiacriticBreve, "ăắằẳẵặ"},
		{'a', DiacriticCircumflex, "âấầẩẫậ"},
		{'e', DiacriticNone, "eéèẻẽẹ"},
		{'e', DiacriticCircumflex, "êếềểễệ"},
		{'i', DiacriticNone, "iíìỉĩị"},
		{'o', DiacriticNone, "oóòỏõọ"},
		{'o', DiacriticCircumflex, "ôốồổỗộ"},
		{'o', DiacriticHorn, "ơớờởỡợ"},
		{'u', DiacriticNone, "uúùủũụ"},
		{'u', DiacriticHorn, "ưứừửữự"},
		{'y', DiacriticNone, "yýỳỷỹỵ"},
	}
	for _, g := range lowerGroups {
		rs := []rune(g.chars)
		for idx, r := range rs {
			register(r, g.bare, g.dia, idx, false) // idx is tone
		}
	}
	// Uppercase groups
	upperGroups := []struct {
		bare rune
		dia  int
		chars string
	}{
		{'a', DiacriticNone, "AÁÀẢÃẠ"},
		{'a', DiacriticBreve, "ĂẮẰẲẴẶ"},
		{'a', DiacriticCircumflex, "ÂẤẦẨẪẬ"},
		{'e', DiacriticNone, "EÉÈẺẼẸ"},
		{'e', DiacriticCircumflex, "ÊẾỀỂỄỆ"},
		{'i', DiacriticNone, "IÍÌỈĨỊ"},
		{'o', DiacriticNone, "OÓÒỎÕỌ"},
		{'o', DiacriticCircumflex, "ÔỐỒỔỖỘ"},
		{'o', DiacriticHorn, "ƠỚỜỞỠỢ"},
		{'u', DiacriticNone, "UÚÙỦŨỤ"},
		{'u', DiacriticHorn, "ƯỨỪỬỮỰ"},
		{'y', DiacriticNone, "YÝỲỶỸỴ"},
	}
	for _, g := range upperGroups {
		rs := []rune(g.chars)
		for idx, r := range rs {
			register(r, g.bare, g.dia, idx, true)
		}
	}
	// Stroke đ/Đ (no tone)
	register('d', 'd', DiacriticNone, ToneNone, false)
	register('đ', 'd', DiacriticStroke, ToneNone, false)
	register('D', 'd', DiacriticNone, ToneNone, true)
	register('Đ', 'd', DiacriticStroke, ToneNone, true)

	// Also register plain consonants as themselves? For isVowel check we use map
}

func lookup(bare rune, dia, tone int, isUpper bool) (rune, bool) {
	info := CharInfo{Bare: bare, Diacritic: dia, Tone: tone, IsUpper: isUpper}
	r, ok := reverseMap[info]
	return r, ok
}

// isVowel checks if rune is Vietnamese vowel (including toned variants)
func isVowel(r rune) bool {
	info, ok := charInfoMap[r]
	if !ok {
		return false
	}
	// đ is not vowel even though in map
	if info.Bare == 'd' {
		return false
	}
	return true
}

func isConsonant(r rune) bool {
	if isVowel(r) {
		return false
	}
	// letter and not vowel => consonant (including đ? đ considered consonant)
	if unicode.IsLetter(r) {
		return true
	}
	return false
}

func isLetter(r rune) bool { return unicode.IsLetter(r) }

func toLower(r rune) rune { return unicode.ToLower(r) }

func applyToneAt(word []rune, pos int, tone int) []rune {
	if pos < 0 || pos >= len(word) {
		return word
	}
	r := word[pos]
	info, ok := charInfoMap[r]
	if !ok {
		return word
	}
	if info.Bare == 'd' {
		return word
	}
	// Preserve diacritic, change tone
	newRune, ok := lookup(info.Bare, info.Diacritic, tone, info.IsUpper)
	if !ok {
		return word
	}
	word[pos] = newRune
	return word
}

func removeAllTones(word []rune) []rune {
	for i, r := range word {
		info, ok := charInfoMap[r]
		if !ok {
			continue
		}
		if info.Tone != ToneNone {
			if nr, ok := lookup(info.Bare, info.Diacritic, ToneNone, info.IsUpper); ok {
				word[i] = nr
			}
		}
	}
	return word
}

func toggleBreve(r rune) (rune, bool) {
	info, ok := charInfoMap[r]
	if !ok {
		return r, false
	}
	if info.Bare != 'a' {
		return r, false
	}
	var newDia int
	if info.Diacritic == DiacriticBreve {
		newDia = DiacriticNone
	} else if info.Diacritic == DiacriticNone || info.Diacritic == DiacriticCircumflex {
		newDia = DiacriticBreve
	} else {
		return r, false
	}
	nr, ok := lookup(info.Bare, newDia, info.Tone, info.IsUpper)
	if !ok {
		return r, false
	}
	return nr, true
}

func toggleCircumflex(r rune) (rune, bool) {
	info, ok := charInfoMap[r]
	if !ok {
		return r, false
	}
	var newDia int
	switch info.Bare {
	case 'a':
		if info.Diacritic == DiacriticCircumflex {
			newDia = DiacriticNone
		} else if info.Diacritic == DiacriticNone || info.Diacritic == DiacriticBreve {
			newDia = DiacriticCircumflex
		} else {
			return r, false
		}
	case 'e':
		if info.Diacritic == DiacriticCircumflex {
			newDia = DiacriticNone
		} else if info.Diacritic == DiacriticNone {
			newDia = DiacriticCircumflex
		} else {
			return r, false
		}
	case 'o':
		if info.Diacritic == DiacriticCircumflex {
			newDia = DiacriticNone
		} else if info.Diacritic == DiacriticNone || info.Diacritic == DiacriticHorn {
			newDia = DiacriticCircumflex
		} else {
			return r, false
		}
	default:
		return r, false
	}
	nr, ok := lookup(info.Bare, newDia, info.Tone, info.IsUpper)
	if !ok {
		return r, false
	}
	return nr, true
}

func toggleHorn(r rune) (rune, bool) {
	info, ok := charInfoMap[r]
	if !ok {
		return r, false
	}
	var newDia int
	switch info.Bare {
	case 'o':
		if info.Diacritic == DiacriticHorn {
			newDia = DiacriticNone
		} else if info.Diacritic == DiacriticNone || info.Diacritic == DiacriticCircumflex {
			newDia = DiacriticHorn
		} else {
			return r, false
		}
	case 'u':
		if info.Diacritic == DiacriticHorn {
			newDia = DiacriticNone
		} else if info.Diacritic == DiacriticNone {
			newDia = DiacriticHorn
		} else {
			return r, false
		}
	default:
		return r, false
	}
	nr, ok := lookup(info.Bare, newDia, info.Tone, info.IsUpper)
	if !ok {
		return r, false
	}
	return nr, true
}

func toggleStroke(r rune) (rune, bool) {
	info, ok := charInfoMap[r]
	if !ok {
		return r, false
	}
	if info.Bare != 'd' {
		return r, false
	}
	var newDia int
	if info.Diacritic == DiacriticStroke {
		newDia = DiacriticNone
	} else {
		newDia = DiacriticStroke
	}
	nr, ok := lookup(info.Bare, newDia, ToneNone, info.IsUpper)
	if !ok {
		return r, false
	}
	return nr, true
}

func getTone(r rune) int {
	info, ok := charInfoMap[r]
	if !ok {
		return ToneNone
	}
	return info.Tone
}

func getDiacritic(r rune) int {
	info, ok := charInfoMap[r]
	if !ok {
		return DiacriticNone
	}
	return info.Diacritic
}

func isUpper(r rune) bool { return unicode.IsUpper(r) }

// helper to get bare lower
func bareLower(r rune) rune {
	info, ok := charInfoMap[r]
	if !ok {
		return toLower(r)
	}
	return info.Bare
}
