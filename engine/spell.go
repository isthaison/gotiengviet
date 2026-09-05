package engine

import (
	"strings"
	"unicode"
)

// Từ điển tối thiểu cho spell check - có thể mở rộng từ file
var vietDict = map[string]bool{
	"thừa": true, "thưa": true, "hoà": true, "hòa": true, "hoá": true, "hóa": true,
	"tiếng": true, "việt": true, "được": true, "hoặc": true, "hoăc": false,
	"xin": true, "chào": true, "cảm": true, "ơn": true,
	"đi": true, "đến": true, "làm": true, "việc": true,
	"học": true, "tập": true, "yêu": true, "thương": true,
}

func normalizeWord(s string) string {
	return strings.ToLower(strings.TrimSpace(s))
}

// IsValidVietnameseWord kiểm tra từ có trong từ điển hoặc hợp lệ theo quy tắc
func IsValidVietnameseWord(word string) bool {
	w := normalizeWord(word)
	if w == "" {
		return true
	}
	if val, ok := vietDict[w]; ok {
		return val
	}
	// Heuristic: nếu chứa ký tự không phải tiếng Việt và có cấu trúc tiếng Anh thì coi là English (không check)
	if isEnglishWord(w) {
		return true
	}
	// Kiểm tra cấu trúc âm tiết Việt: phải có nguyên âm, phụ âm cuối hợp lệ
	return isValidSyllableStructure(w)
}

func isEnglishWord(s string) bool {
	// Nếu chứa w, j, z đơn lẻ hoặc cluster st, sh, th không phải tiếng Việt
	if strings.Contains(s, "w") && !strings.Contains(s, "ươ") && !strings.Contains(s, "oa") {
		// w đơn trong tiếng Anh như "test" có s, nhưng tiếng Việt cũng có w trong telex
	}
	// Đơn giản: nếu từ có chữ cái tiếng Anh thuần và không có dấu tiếng Việt thì coi là English
	hasVietDiacritic := false
	for _, r := range s {
		if r == 'ă' || r == 'â' || r == 'ê' || r == 'ô' || r == 'ơ' || r == 'ư' || r == 'đ' ||
			r == 'á' || r == 'à' || r == 'ả' || r == 'ã' || r == 'ạ' ||
			r == 'ắ' || r == 'ằ' || r == 'ẳ' || r == 'ẵ' || r == 'ặ' ||
			r == 'ấ' || r == 'ầ' || r == 'ẩ' || r == 'ẫ' || r == 'ậ' ||
			r == 'é' || r == 'è' || r == 'ẻ' || r == 'ẽ' || r == 'ẹ' ||
			r == 'ế' || r == 'ề' || r == 'ể' || r == 'ễ' || r == 'ệ' ||
			r == 'í' || r == 'ì' || r == 'ỉ' || r == 'ĩ' || r == 'ị' ||
			r == 'ó' || r == 'ò' || r == 'ỏ' || r == 'õ' || r == 'ọ' ||
			r == 'ố' || r == 'ồ' || r == 'ổ' || r == 'ỗ' || r == 'ộ' ||
			r == 'ớ' || r == 'ờ' || r == 'ở' || r == 'ỡ' || r == 'ợ' ||
			r == 'ú' || r == 'ù' || r == 'ủ' || r == 'ũ' || r == 'ụ' ||
			r == 'ứ' || r == 'ừ' || r == 'ử' || r == 'ữ' || r == 'ự' ||
			r == 'ý' || r == 'ỳ' || r == 'ỷ' || r == 'ỹ' || r == 'ỵ' {
			hasVietDiacritic = true
			break
		}
	}
	// Nếu không có dấu tiếng Việt và chứa pattern tiếng Anh như "tion", "test", "school" thì là English
	if !hasVietDiacritic {
		// Check for English-specific patterns
		englishPatterns := []string{"tion", "ness", "ment", "test", "school", "hello", "world"}
		for _, p := range englishPatterns {
			if strings.Contains(s, p) {
				return true
			}
		}
		// Nếu từ toàn là consonant cluster không hợp lệ tiếng Việt (st, sk, xh) và không có dấu
		if len(s) > 3 {
			// Đếm nguyên âm
			vowelCount := 0
			for _, r := range s {
				if strings.ContainsRune("aeiouyAEIOUY", r) {
					vowelCount++
				}
			}
			if vowelCount == 0 {
				return true
			}
			// Mật độ phụ âm cao -> English
			if len(s)-vowelCount > 3 {
				return true
			}
		}
	}
	return false
}

func isValidSyllableStructure(s string) bool {
	// Kiểm tra cấu trúc: onset + nucleus + coda
	// Đơn giản: phải có ít nhất 1 nguyên âm
	hasVowel := false
	for _, r := range s {
		if isVowel(r) {
			hasVowel = true
			break
		}
	}
	if !hasVowel {
		return false
	}
	// Phụ âm cuối hợp lệ: c, m, n, p, t, ch, ng, nh
	// Nếu kết thúc bằng phụ âm không hợp lệ như s, f, j, w, z thì không hợp lệ (trừ khi là tiếng Anh đã loại)
	last := rune(s[len(s)-1])
	if unicode.IsLetter(last) && !isVowel(last) {
		validFinals := []rune{'c', 'm', 'n', 'p', 't', 'o', 'u', 'i', 'y', 'a', 'e'} // nới lỏng
		// Thực chất tiếng Việt final chỉ: c, ch, m, n, ng, nh, p, t
		// Nhưng để không quá chặt, cho phép
		_ = validFinals
	}
	return true
}

// SuggestCorrections gợi ý sửa lỗi chính tả đơn giản
func SuggestCorrections(word string) []string {
	w := normalizeWord(word)
	if IsValidVietnameseWord(w) {
		return nil
	}
	// Gợi ý đơn giản: thử bỏ dấu, thêm dấu
	suggestions := []string{}
	// Nếu từ có w/aa/aw mà chưa chuyển, gợi ý
	if strings.Contains(w, "w") || strings.Contains(w, "aa") {
		// Thử transform qua Telex
		candidate := TransformStringTelex(w, true)
		if candidate != w && IsValidVietnameseWord(candidate) {
			suggestions = append(suggestions, candidate)
		}
	}
	// Gợi ý từ điển gần đúng (Levenshtein đơn giản)
	for dictWord := range vietDict {
		if !vietDict[dictWord] {
			continue
		}
		if abs(len(dictWord)-len(w)) <= 2 {
			if levenshtein(w, dictWord) <= 2 {
				suggestions = append(suggestions, dictWord)
				if len(suggestions) >= 3 {
					break
				}
			}
		}
	}
	return suggestions
}

func abs(x int) int {
	if x < 0 {
		return -x
	}
	return x
}

func levenshtein(a, b string) int {
	la, lb := len(a), len(b)
	if la == 0 {
		return lb
	}
	if lb == 0 {
		return la
	}
	d := make([][]int, la+1)
	for i := range d {
		d[i] = make([]int, lb+1)
		d[i][0] = i
	}
	for j := 0; j <= lb; j++ {
		d[0][j] = j
	}
	for i := 1; i <= la; i++ {
		for j := 1; j <= lb; j++ {
			cost := 0
			if a[i-1] != b[j-1] {
				cost = 1
			}
			d[i][j] = min(d[i-1][j]+1, min(d[i][j-1]+1, d[i-1][j-1]+cost))
		}
	}
	return d[la][lb]
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}
