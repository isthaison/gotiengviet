package engine

import "testing"

func typeStringVNI(s string, modern bool) string {
	var buf []rune
	for _, ch := range s {
		newBuf, consumed := VNITransform(buf, ch, modern)
		if consumed {
			buf = newBuf
		} else {
			buf = append(buf, ch)
		}
	}
	return string(buf)
}

func TestVNITransform(t *testing.T) {
	tests := []struct {
		input    string
		expected string
	}{
		// 1. Dấu thanh cơ bản
		{"a1", "á"},
		{"a2", "à"},
		{"a3", "ả"},
		{"a4", "ã"},
		{"a5", "ạ"},

		// 2. Ký tự biến đổi mũ, móc, trăng, gạch
		{"a6", "â"},
		{"e6", "ê"},
		{"o6", "ô"},
		{"o7", "ơ"},
		{"u7", "ư"},
		{"uo7", "ươ"},
		{"a8", "ă"},
		{"d9", "đ"},

		// 3. Nâng cấp nguyên âm đôi/ba
		{"hien2", "hiền"},
		{"viet5", "việt"},
		{"muon1", "muốn"},
		{"tieu3", "tiểu"},
		{"chuoi1", "chuối"},
		{"d9uoc75", "được"},
		{"d9uong72", "đường"},

		// 4. Gõ phím lặp hoàn tác (Universal Repeat-Key Undo)
		{"d99", "d9"},
		{"a88", "a8"},
		{"o77", "o7"},
		{"u77", "u7"},
		{"uo77", "uo7"},
		{"a66", "a6"},
		{"e66", "e6"},
		{"o66", "o6"},
		{"a11", "a1"},
		{"ban11", "ban1"},

		// 5. Gõ tự do (dấu trước/sau phụ âm cuối)
		{"toan1", "toán"},
		{"toa1n", "toán"},
		{"phap1", "pháp"},
		{"pha1p", "pháp"},
		{"hoac85", "hoặc"},

		// 6. Từ tiếng Anh không bị bóp méo dấu
		{"win10", "win10"},
		{"password1", "password1"},
		{"test1", "test1"},
		{"fast8", "fast8"},

		// 7. Phím 0 xóa dấu
		{"a10", "a"},
		{"toan10", "toan"},
	}

	for _, tc := range tests {
		got := typeStringVNI(tc.input, true)
		if got != tc.expected {
			t.Errorf("typeStringVNI(%q) = %q, expected %q", tc.input, got, tc.expected)
		}
	}
}
