package engine

import "testing"

func typeString(s string, modern bool) string {
	var buf []rune
	for _, ch := range s {
		newBuf, consumed := TelexTransform(buf, ch, modern)
		if consumed {
			buf = newBuf
		} else {
			buf = append(buf, ch)
		}
	}
	return string(buf)
}

func TestTelexTransform(t *testing.T) {
	tests := []struct {
		input    string
		expected string
	}{
		// 1. Dấu thanh cơ bản
		{"as", "á"},
		{"af", "à"},
		{"ar", "ả"},
		{"ax", "ã"},
		{"aj", "ạ"},

		// 2. Nâng cấp nguyên âm đôi/ba
		{"hienr", "hiển"},
		{"hienj", "hiện"},
		{"vietj", "việt"},
		{"muons", "muốn"},
		{"cuocj", "cuộc"},
		{"tieur", "tiểu"},
		{"chuois", "chuối"},

		// 3. Gõ phím lặp hoàn tác (Universal Repeat-Key Undo)
		{"dd", "đ"},
		{"ddd", "dd"},
		{"reddit", "reddit"},
		{"aa", "â"},
		{"aaa", "aa"},
		{"ee", "ê"},
		{"eee", "ee"},
		{"meet", "mêt"},
		{"meeet", "meet"},
		{"oo", "ô"},
		{"ooo", "oo"},
		{"book", "bôk"},
		{"boook", "book"},

		// 4. Phím w hoàn tác
		{"w", "ư"},
		{"ww", "w"},
		{"www", "ww"},
		{"wwww", "www"},
		{"uw", "ư"},
		{"ow", "ơ"},
		{"oww", "ow"},
		{"showw", "show"},
		{"aw", "ă"},
		{"aww", "aw"},
		{"raww", "raw"},
		{"sw", "sư"},
		{"sww", "sw"},

		// 5. Phím dấu thanh lặp hoàn tác
		{"pass", "pass"},
		{"buff", "buff"},
		{"kiss", "kiss"},
		{"boss", "boss"},
		{"toanss", "toans"},

		// 6. Gõ tự do (dấu trước/sau phụ âm cuối)
		{"phaps", "pháp"},
		{"phasp", "pháp"},
		{"toans", "toán"},
		{"toasn", "toán"},
		{"hoacw", "hoăc"},
		{"hoacwj", "hoặc"},

		// 7. Từ tiếng Anh không bị bóp méo
		{"password", "password"},
		{"passw", "passw"},
		{"class", "class"},
		{"test", "test"},
		{"post", "post"},
		{"fast", "fast"},
		{"fish", "fish"},
		{"code", "code"},
		{"game", "game"},

		// 8. Phím tắt ngoặc vuông
		{"[", "ươ"},
		{"[[", "["},
		{"]", "ư"},
		{"]]", "]"},
	}

	for _, tc := range tests {
		got := typeString(tc.input, true)
		if got != tc.expected {
			t.Errorf("typeString(%q) = %q, expected %q", tc.input, got, tc.expected)
		}
	}
}
