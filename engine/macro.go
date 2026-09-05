package engine

import (
	"strings"
)

// Macro table - có thể load từ file ~/.config/gotiengviet/macro.conf
var macroTable = map[string]string{
	"vn":  "Việt Nam",
	"hn":  "Hà Nội",
	"hcm": "Hồ Chí Minh",
	"dc":  "được",
	"ko":  "không",
	"ntn": "như thế nào",
	"cx":  "cũng",
	"cb":  "chuẩn bị",
}

var emojiTable = map[string]string{
	":smile:":   "😊",
	":heart:":   "❤️",
	":laugh:":   "😂",
	":sad:":     "😢",
	":angry:":   "😠",
	":thumbsup:": "👍",
	":fire:":    "🔥",
	":star:":    "⭐",
	":check:":   "✅",
	":vim:":     "💚",
}

// ExpandMacro kiểm tra từ có phải macro không, nếu có trả về expanded + true
func ExpandMacro(word string) (string, bool) {
	w := strings.ToLower(strings.TrimSpace(word))
	if expanded, ok := macroTable[w]; ok {
		return expanded, true
	}
	return word, false
}

// ExpandEmoji kiểm tra từ có phải emoji code không
func ExpandEmoji(word string) (string, bool) {
	w := strings.TrimSpace(word)
	if expanded, ok := emojiTable[w]; ok {
		return expanded, true
	}
	// Thử lower
	w2 := strings.ToLower(w)
	if expanded, ok := emojiTable[w2]; ok {
		return expanded, true
	}
	return word, false
}
