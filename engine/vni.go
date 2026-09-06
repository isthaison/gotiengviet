package engine

// VNITransform xử lý gõ VNI theo mô hình máy trạng thái âm tiết tiếng Việt thống nhất.
func VNITransform(buf []rune, key rune, modern bool) ([]rune, bool) {
	if len(buf) == 0 {
		return buf, false
	}
	// Bỏ qua nội dung trong emoji :smile:
	if buf[0] == ':' {
		hasClosing := false
		for i := 1; i < len(buf); i++ {
			if buf[i] == ':' {
				hasClosing = true
				break
			}
		}
		if !hasClosing {
			return buf, false
		}
	}

	if key < '0' || key > '9' {
		return buf, false
	}

	// =========================================================================
	// PHA 1: NGUYÊN TẮC HOÀN TÁC PHÍM LẶP THỐNG NHẤT (UNIVERSAL REPEAT-KEY UNDO)
	// Gõ lại đúng phím số đó thì xóa dấu và khôi phục ký tự thô kèm số
	// =========================================================================

	// 1.1. Lặp phím 9: đ + 9 -> d9 (hoặc Đ + 9 -> D9)
	if key == '9' {
		for i := len(buf) - 1; i >= 0; i-- {
			r := buf[i]
			if bareLower(r) == 'd' && getDiacritic(r) == DiacriticStroke {
				dBare := 'd'
				if isUpper(r) {
					dBare = 'D'
				}
				newBuf := append([]rune(nil), buf...)
				newBuf[i] = dBare
				return append(newBuf, key), true
			}
		}
	}

	// 1.2. Lặp phím 8: ă + 8 -> a8
	if key == '8' {
		for i := len(buf) - 1; i >= 0; i-- {
			r := buf[i]
			if bareLower(r) == 'a' && getDiacritic(r) == DiacriticBreve {
				tone := getTone(r)
				var nc rune
				if nr, ok := lookup('a', DiacriticNone, tone, isUpper(r)); ok {
					nc = nr
				} else {
					nc = 'a'
					if isUpper(r) {
						nc = 'A'
					}
				}
				newBuf := append([]rune(nil), buf...)
				newBuf[i] = nc
				return append(newBuf, key), true
			}
		}
	}

	// 1.3. Lặp phím 7: ươ + 7 -> uo7, ư + 7 -> u7, ơ + 7 -> o7
	if key == '7' {
		// Cặp ươ
		for i := len(buf) - 1; i >= 1; i-- {
			c1 := buf[i-1]
			c2 := buf[i]
			if bareLower(c1) == 'u' && getDiacritic(c1) == DiacriticHorn &&
				bareLower(c2) == 'o' && getDiacritic(c2) == DiacriticHorn {
				uBare := 'u'
				if isUpper(c1) {
					uBare = 'U'
				}
				tone2 := getTone(c2)
				var oBare rune
				if nr, ok := lookup('o', DiacriticNone, tone2, isUpper(c2)); ok {
					oBare = nr
				} else {
					oBare = 'o'
					if isUpper(c2) {
						oBare = 'O'
					}
				}
				newBuf := append([]rune(nil), buf...)
				newBuf[i-1] = uBare
				newBuf[i] = oBare
				return append(newBuf, key), true
			}
		}
		// Đơn lẻ u hoặc o
		for i := len(buf) - 1; i >= 0; i-- {
			r := buf[i]
			bare := bareLower(r)
			if (bare == 'u' || bare == 'o') && getDiacritic(r) == DiacriticHorn {
				tone := getTone(r)
				var nc rune
				if nr, ok := lookup(bare, DiacriticNone, tone, isUpper(r)); ok {
					nc = nr
				} else {
					nc = bare
					if isUpper(r) {
						nc = bare - ('a' - 'A')
					}
				}
				newBuf := append([]rune(nil), buf...)
				newBuf[i] = nc
				return append(newBuf, key), true
			}
		}
	}

	// 1.4. Lặp phím 6: â + 6 -> a6, ê + 6 -> e6, ô + 6 -> o6
	if key == '6' {
		for i := len(buf) - 1; i >= 0; i-- {
			r := buf[i]
			bare := bareLower(r)
			if (bare == 'a' || bare == 'e' || bare == 'o') && getDiacritic(r) == DiacriticCircumflex {
				tone := getTone(r)
				var nc rune
				if nr, ok := lookup(bare, DiacriticNone, tone, isUpper(r)); ok {
					nc = nr
				} else {
					nc = bare
					if isUpper(r) {
						nc = bare - ('a' - 'A')
					}
				}
				newBuf := append([]rune(nil), buf...)
				newBuf[i] = nc
				return append(newBuf, key), true
			}
		}
	}

	// 1.5. Lặp phím 1..5: xóa dấu thanh và thêm số tương ứng (bán + 1 -> ban1)
	if key >= '1' && key <= '5' {
		tone := int(key - '0')
		pos := findTonePosition(buf, modern)
		if pos != -1 && getTone(buf[pos]) == tone {
			newBuf := append([]rune(nil), buf...)
			if nr, ok := lookup(bareLower(newBuf[pos]), getDiacritic(newBuf[pos]), ToneNone, isUpper(newBuf[pos])); ok {
				newBuf[pos] = nr
			} else {
				newBuf = removeAllTones(newBuf)
			}
			return append(newBuf, key), true
		}
	}

	// 1.6. Phím 0: xóa dấu thanh
	if key == '0' {
		if newBuf, ok := tryRemove(buf); ok {
			return newBuf, true
		}
		return buf, false
	}

	// =========================================================================
	// PHA 2: BẢO VỆ TỪ NGOẠI LAI (FOREIGN WORD BYPASS)
	// Từ ngoại lai (win, password, test, class...) không can thiệp số
	// =========================================================================
	if isForeignWord(buf) {
		return buf, false
	}

	// =========================================================================
	// PHA 3: BIẾN ĐỔI MŨ / MÓC / TRĂNG / GẠCH (6, 7, 8, 9)
	// =========================================================================

	// 3.1. Phím 9 -> đ
	if key == '9' {
		for i := len(buf) - 1; i >= 0; i-- {
			r := buf[i]
			if bareLower(r) == 'd' && getDiacritic(r) == DiacriticNone {
				if nr, ok := toggleStroke(r); ok {
					newBuf := append([]rune(nil), buf...)
					newBuf[i] = nr
					return newBuf, true
				}
			}
		}
	}

	// 3.2. Phím 8 -> ă
	if key == '8' {
		for i := len(buf) - 1; i >= 0; i-- {
			r := buf[i]
			if bareLower(r) == 'a' && getDiacritic(r) == DiacriticNone {
				if nr, ok := toggleBreve(r); ok {
					newBuf := append([]rune(nil), buf...)
					newBuf[i] = nr
					return newBuf, true
				}
			}
		}
	}

	// 3.3. Phím 7 -> móc (ư, ơ, ươ)
	if key == '7' {
		// uo -> ươ
		for i := len(buf) - 1; i >= 1; i-- {
			c1 := buf[i-1]
			c2 := buf[i]
			if bareLower(c1) == 'u' && getDiacritic(c1) == DiacriticNone &&
				bareLower(c2) == 'o' && getDiacritic(c2) == DiacriticNone {
				newBuf := append([]rune(nil), buf...)
				if nr1, ok1 := toggleHorn(c1); ok1 {
					newBuf[i-1] = nr1
				}
				if nr2, ok2 := toggleHorn(c2); ok2 {
					newBuf[i] = nr2
				}
				return newBuf, true
			}
		}
		// u hoặc o
		for i := len(buf) - 1; i >= 0; i-- {
			r := buf[i]
			bare := bareLower(r)
			if (bare == 'u' || bare == 'o') && getDiacritic(r) == DiacriticNone {
				if nr, ok := toggleHorn(r); ok {
					newBuf := append([]rune(nil), buf...)
					newBuf[i] = nr
					return newBuf, true
				}
			}
		}
	}

	// 3.4. Phím 6 -> mũ (â, ê, ô)
	if key == '6' {
		for i := len(buf) - 1; i >= 0; i-- {
			r := buf[i]
			bare := bareLower(r)
			if (bare == 'a' || bare == 'e' || bare == 'o') && getDiacritic(r) == DiacriticNone {
				if nr, ok := toggleCircumflex(r); ok {
					newBuf := append([]rune(nil), buf...)
					newBuf[i] = nr
					return newBuf, true
				}
			}
		}
	}

	// =========================================================================
	// PHA 4: BIẾN ĐỔI DẤU THANH (1, 2, 3, 4, 5)
	// =========================================================================
	if key >= '1' && key <= '5' {
		tone := int(key - '0')
		hasVowel := false
		for _, r := range buf {
			if isVowel(r) {
				hasVowel = true
				break
			}
		}
		if hasVowel {
			buf = autoPromoteDiphthong(buf)
			if newBuf, transformed := applyMark(buf, tone, modern); transformed {
				return newBuf, true
			}
		}
	}

	return buf, false
}

// TransformStringVNI mô phỏng gõ chuỗi qua VNI
func TransformStringVNI(input string, modern bool) string {
	buf := []rune{}
	for _, r := range []rune(input) {
		newBuf, consumed := VNITransform(buf, r, modern)
		if consumed {
			buf = newBuf
		} else {
			buf = append(buf, r)
		}
	}
	return string(buf)
}
