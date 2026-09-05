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
		{"as", "á"},
		{"af", "à"},
		{"ar", "ả"},
		{"ax", "ã"},
		{"aj", "ạ"},
		{"hienr", "hiển"},
		{"hienj", "hiện"},
		{"vietj", "việt"},
		{"muons", "muốn"},
		{"cuocj", "cuộc"},
		{"tieur", "tiểu"},
		{"chuois", "chuối"},
		{"dd", "đ"},
		{"ddd", "dd"},
		{"eee", "ee"},
		{"ooo", "oo"},
		{"[", "ươ"},
		{"[[", "["},
		{"]", "ư"},
		{"]]", "]"},
		{"test", "test"},
		{"post", "post"},
		{"fast", "fast"},
		{"fish", "fish"},
	}

	for _, tc := range tests {
		got := typeString(tc.input, true)
		if got != tc.expected {
			t.Errorf("typeString(%q) = %q, expected %q", tc.input, got, tc.expected)
		}
	}
}
