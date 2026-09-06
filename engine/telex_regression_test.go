package engine

import "testing"

func TestTelexCircumflexPreservesCaseAndTone(t *testing.T) {
	tests := []struct {
		input string
		key   rune
		want  string
	}{
		{"á", 'a', "ấ"}, {"é", 'e', "ế"}, {"ó", 'o', "ố"},
		{"Á", 'A', "Ấ"}, {"É", 'E', "Ế"}, {"Ó", 'O', "Ố"},
		{"ấ", 'a', "áa"}, {"ế", 'e', "ée"}, {"ố", 'o', "óo"},
		{"Ấ", 'a', "Áa"}, {"Ế", 'E', "ÉE"}, {"Ố", 'O', "ÓO"},
	}
	for _, modern := range []bool{false, true} {
		for _, tt := range tests {
			got, consumed := TelexTransform([]rune(tt.input), tt.key, modern)
			if !consumed || string(got) != tt.want {
				t.Errorf("TelexTransform(%q, %q, %v) = (%q, %v), want (%q, true)", tt.input, tt.key, modern, string(got), consumed, tt.want)
			}
		}
	}
}

func TestAutoPromoteDiphthongPreservesMarks(t *testing.T) {
	tests := []struct{ input, want string }{
		{"hien", "hiên"}, {"chuyen", "chuyên"}, {"muon", "muôn"},
		{"tieu", "tiêu"}, {"yeu", "yêu"}, {"chuoi", "chuôi"},
		{"HIÉN", "HIẾN"}, {"MUÓN", "MUỐN"}, {"TIÉU", "TIẾU"},
		{"uơn", "uơn"}, {"ưon", "ưon"}, {"uơi", "uơi"}, {"ưoi", "ưoi"},
		{"ie", "ie"}, {"uo", "uo"}, {"", ""},
	}
	for _, tt := range tests {
		input := []rune(tt.input)
		if got := string(autoPromoteDiphthong(input)); got != tt.want {
			t.Errorf("autoPromoteDiphthong(%q) = %q, want %q", tt.input, got, tt.want)
		}
		if string(input) != tt.input {
			t.Errorf("input changed: %q", string(input))
		}
	}
}

// Dấu sắc trước phụ âm cuối không được hoàn tác khi đã có dấu mũ/móc/trăng.
func TestTelexMarkedVowelBeforeFinalConsonant(t *testing.T) {
	for _, modern := range []bool{false, true} {
		for _, tc := range []struct{ input, want string }{
			{"bawst", "bắt"}, {"bawts", "bắt"}, {"BAWST", "BẮT"},
			{"caast", "cất"}, {"coost", "cốt"}, {"bowst", "bớt"},
			{"test", "test"}, {"post", "post"}, {"fast", "fast"}, {"fish", "fish"}, {"task", "task"},
		} {
			if got := TransformStringTelex(tc.input, modern); got != tc.want {
				t.Errorf("TransformStringTelex(%q, %v) = %q, want %q", tc.input, modern, got, tc.want)
			}
		}
	}
}
