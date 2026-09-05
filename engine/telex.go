package engine

// tryEnglishRestore khôi phục từ tiếng Anh khi gõ tiếp phụ âm kết thúc:
// test (tés+t), post (pós+t), fast (fás+t), fish (fís+h), task (tás+k), desk (dés+k), risk (rís+k)
func tryEnglishRestore(buf []rune, key rune) ([]rune, bool) {
	if len(buf) == 0 {
		return buf, false
	}
	lk := toLower(key)
	if lk != 't' && lk != 'h' && lk != 'k' && lk != 'p' {
		return buf, false
	}
	last := buf[len(buf)-1]
	if isVowel(last) && getTone(last) == ToneSac {
		if bareChar, ok := lookup(bareLower(last), getDiacritic(last), ToneNone, isUpper(last)); ok {
			newBuf := make([]rune, len(buf)-1, len(buf)+2)
			copy(newBuf, buf[:len(buf)-1])
			newBuf = append(newBuf, bareChar)
			sChar := 's'
			if isUpper(last) && isUpper(key) {
				sChar = 'S'
			}
			newBuf = append(newBuf, sChar, key)
			return newBuf, true
		}
	}
	return buf, false
}

// autoPromoteDiphthong tự động nâng cấp nguyên âm đôi/ba khi có phụ âm cuối hoặc bán nguyên âm cuối:
// i + e + [coda] -> iê + [coda] (hienr -> hiển, hienj -> hiện, vietj -> việt, tiens -> tiến, tieur -> tiểu)
// u + o + [coda] -> uô + [coda] (muons -> muốn, cuocj -> cuộc, buonc -> buồn, chuois -> chuối)
// y + e + [coda] -> yê + [coda] (chuyenr -> chuyển, khuyen -> khuyên)
func autoPromoteDiphthong(buf []rune) []rune {
	if len(buf) < 3 {
		return buf
	}
	n := len(buf)

	// 1. Bán nguyên âm cuối (triphthong): ...ieu, ...yeu, ...uoi
	lastChar := bareLower(buf[n-1])
	prevChar := bareLower(buf[n-2])
	prev2Char := bareLower(buf[n-3])

	if lastChar == 'u' && prevChar == 'e' && (prev2Char == 'i' || prev2Char == 'y') {
		v2 := buf[n-2]
		if getDiacritic(v2) == DiacriticNone {
			if nc, ok := toggleCircumflex(v2); ok {
				newBuf := make([]rune, n)
				copy(newBuf, buf)
				newBuf[n-2] = nc
				return newBuf
			}
		}
	}
	if lastChar == 'i' && prevChar == 'o' && prev2Char == 'u' {
		v1 := buf[n-3]
		v2 := buf[n-2]
		if getDiacritic(v1) == DiacriticNone && getDiacritic(v2) == DiacriticNone {
			if nc, ok := toggleCircumflex(v2); ok {
				newBuf := make([]rune, n)
				copy(newBuf, buf)
				newBuf[n-2] = nc
				return newBuf
			}
		}
	}

	// 2. Có phụ âm cuối
	lastV := -1
	for i := n - 1; i >= 0; i-- {
		if isVowel(buf[i]) {
			lastV = i
			break
		}
	}
	if lastV <= 0 || lastV >= n-1 {
		return buf
	}
	v2 := buf[lastV]
	v1 := buf[lastV-1]
	if bareLower(v1) == 'i' && bareLower(v2) == 'e' && getDiacritic(v2) == DiacriticNone {
		if nc, ok := toggleCircumflex(v2); ok {
			newBuf := make([]rune, n)
			copy(newBuf, buf)
			newBuf[lastV] = nc
			return newBuf
		}
	} else if bareLower(v1) == 'y' && bareLower(v2) == 'e' && getDiacritic(v2) == DiacriticNone {
		if nc, ok := toggleCircumflex(v2); ok {
			newBuf := make([]rune, n)
			copy(newBuf, buf)
			newBuf[lastV] = nc
			return newBuf
		}
	} else if bareLower(v1) == 'u' && bareLower(v2) == 'o' && getDiacritic(v1) == DiacriticNone && getDiacritic(v2) == DiacriticNone {
		if nc, ok := toggleCircumflex(v2); ok {
			newBuf := make([]rune, n)
			copy(newBuf, buf)
			newBuf[lastV] = nc
			return newBuf
		}
	}
	return buf
}

// TelexTransform attempts to transform buffer + key according to Telex rules.
// Returns new buffer and whether key was consumed as transform (true) vs should be appended (false)
// modern: tone placement style
func TelexTransform(buf []rune, key rune, modern bool) ([]rune, bool) {
	if len(buf) == 0 && key == 'w' {
		// w alone -> ư
		return []rune{'ư'}, true
	}
	if len(buf) == 0 && key == 'W' {
		return []rune{'Ư'}, true
	}
	// Emoji: nếu đang trong :xxx chưa đóng, không áp dụng Telex cho nội dung emoji
	if len(buf) > 0 && buf[0] == ':' {
		// Kiểm tra đã có : đóng chưa (tìm : thứ 2)
		hasClosing := false
		for i := 1; i < len(buf); i++ {
			if buf[i] == ':' {
				hasClosing = true
				break
			}
		}
		if !hasClosing {
			// Đang trong emoji code như :heart, :smile -> không transform
			return buf, false
		}
	}

	// Khôi phục từ tiếng Anh (test, post, fast...)
	if newBuf, ok := tryEnglishRestore(buf, key); ok {
		return newBuf, true
	}

	// Bracket shortcuts: [ -> ươ, [[ -> [, ] -> ư, ]] -> ]
	if key == '[' {
		if len(buf) >= 2 && buf[len(buf)-2] == 'ư' && buf[len(buf)-1] == 'ơ' {
			return append(buf[:len(buf)-2], '['), true
		}
		return append(buf, 'ư', 'ơ'), true
	}
	if key == ']' {
		if len(buf) >= 1 && buf[len(buf)-1] == 'ư' {
			return append(buf[:len(buf)-1], ']'), true
		}
		return append(buf, 'ư'), true
	}
	if key == '{' {
		if len(buf) >= 2 && buf[len(buf)-2] == 'Ư' && buf[len(buf)-1] == 'Ơ' {
			return append(buf[:len(buf)-2], '{'), true
		}
		return append(buf, 'Ư', 'Ơ'), true
	}
	if key == '}' {
		if len(buf) >= 1 && buf[len(buf)-1] == 'Ư' {
			return append(buf[:len(buf)-1], '}'), true
		}
		return append(buf, 'Ư'), true
	}

	// 1. Stroke dd -> đ, ddd -> dd
	if toLower(key) == 'd' {
		// Check if last char is d/đ
		if len(buf) > 0 {
			last := buf[len(buf)-1]
			if bareLower(last) == 'd' {
				if getDiacritic(last) == DiacriticStroke {
					dBare := 'd'
					if isUpper(last) {
						dBare = 'D'
					}
					newBuf := make([]rune, len(buf))
					copy(newBuf, buf)
					newBuf[len(newBuf)-1] = dBare
					return append(newBuf, key), true
				}
				nr, ok := toggleStroke(last)
				if ok {
					newBuf := make([]rune, len(buf))
					copy(newBuf, buf)
					newBuf[len(newBuf)-1] = nr
					return newBuf, true
				}
			}
		}
	}

	// 2. Diacritic tone modifiers: aa, ee, oo, aw, ow, uw
	// We attempt to apply based on key
	lk := toLower(key)
	// aa -> â, aaa -> aa
	if lk == 'a' && len(buf) > 0 {
		last := buf[len(buf)-1]
		if bareLower(last) == 'a' {
			if getDiacritic(last) == DiacriticCircumflex {
				tone := getTone(last)
				var nc rune
				if r, ok := lookup('a', DiacriticNone, tone, isUpper(last)); ok {
					nc = r
				} else {
					nc = 'a'
					if isUpper(last) {
						nc = 'A'
					}
				}
				newBuf := make([]rune, len(buf))
				copy(newBuf, buf)
				newBuf[len(newBuf)-1] = nc
				return append(newBuf, key), true
			} else if getDiacritic(last) == DiacriticNone {
				nr, ok := toggleCircumflex(last)
				if ok {
					newBuf := make([]rune, len(buf))
					copy(newBuf, buf)
					newBuf[len(newBuf)-1] = nr
					return newBuf, true
				}
			}
		}
	}
	if lk == 'e' && len(buf) > 0 {
		last := buf[len(buf)-1]
		if bareLower(last) == 'e' {
			if getDiacritic(last) == DiacriticCircumflex {
				tone := getTone(last)
				var nc rune
				if r, ok := lookup('e', DiacriticNone, tone, isUpper(last)); ok {
					nc = r
				} else {
					nc = 'e'
					if isUpper(last) {
						nc = 'E'
					}
				}
				newBuf := make([]rune, len(buf))
				copy(newBuf, buf)
				newBuf[len(newBuf)-1] = nc
				return append(newBuf, key), true
			} else if getDiacritic(last) == DiacriticNone {
				nr, ok := toggleCircumflex(last)
				if ok {
					newBuf := make([]rune, len(buf))
					copy(newBuf, buf)
					newBuf[len(newBuf)-1] = nr
					return newBuf, true
				}
			}
		}
	}
	if lk == 'o' && len(buf) > 0 {
		last := buf[len(buf)-1]
		if bareLower(last) == 'o' {
			if getDiacritic(last) == DiacriticCircumflex {
				tone := getTone(last)
				var nc rune
				if r, ok := lookup('o', DiacriticNone, tone, isUpper(last)); ok {
					nc = r
				} else {
					nc = 'o'
					if isUpper(last) {
						nc = 'O'
					}
				}
				newBuf := make([]rune, len(buf))
				copy(newBuf, buf)
				newBuf[len(newBuf)-1] = nc
				return append(newBuf, key), true
			} else if getDiacritic(last) == DiacriticNone {
				nr, ok := toggleCircumflex(last)
				if ok {
					newBuf := make([]rune, len(buf))
					copy(newBuf, buf)
					newBuf[len(newBuf)-1] = nr
					return newBuf, true
				}
			}
		}
	}
	// w handling: uow -> ươ shortcut (must be before single o/u handling)
	if lk == 'w' && len(buf) >= 2 {
		secondLast := buf[len(buf)-2]
		last := buf[len(buf)-1]
		if bareLower(secondLast) == 'u' && bareLower(last) == 'o' && getDiacritic(secondLast) == DiacriticNone && getDiacritic(last) == DiacriticNone {
			newBuf := make([]rune, len(buf))
			copy(newBuf, buf)
			if nr1, ok1 := toggleHorn(secondLast); ok1 {
				newBuf[len(newBuf)-2] = nr1
			}
			if nr2, ok2 := toggleHorn(last); ok2 {
				newBuf[len(newBuf)-1] = nr2
			}
			return newBuf, true
		}
	}
	// w handling: tìm vowel thích hợp, cho phép gõ dấu sau (thuaw->thưa, hoacw->hoăc)
	if lk == 'w' && len(buf) > 0 {
		// Xác định có phụ âm cuối không (để phân biệt thuaw vs hoacw)
		hasFinal := false
		if len(buf) > 0 {
			last := buf[len(buf)-1]
			if isConsonant(last) {
				for i := len(buf) - 1; i >= 0; i-- {
					if isVowel(buf[i]) {
						if i < len(buf)-1 {
							hasFinal = true
						}
						break
					}
				}
			}
		}
		// Nếu có phụ âm cuối và pattern oa/ua, ưu tiên a (breve) trước: hoac + w -> hoăc
		if hasFinal {
			for i := len(buf) - 1; i >= 0; i-- {
				c := buf[i]
				if bareLower(c) == 'a' {
					dia := getDiacritic(c)
					if dia == DiacriticNone || dia == DiacriticBreve {
						if dia == DiacriticCircumflex {
							continue
						}
						nr, ok := toggleBreve(c)
						if ok {
							newBuf := make([]rune, len(buf))
							copy(newBuf, buf)
							newBuf[i] = nr
							return newBuf, true
						}
					}
				}
			}
			// Không có a, tìm u/o
			for i := len(buf) - 1; i >= 0; i-- {
				c := buf[i]
				bare := bareLower(c)
				if bare == 'u' || bare == 'o' {
					dia := getDiacritic(c)
					if dia == DiacriticNone {
						nr, ok := toggleHorn(c)
						if ok {
							newBuf := make([]rune, len(buf))
							copy(newBuf, buf)
							newBuf[i] = nr
							return newBuf, true
						}
					} else if dia == DiacriticHorn {
					// Nguyên tắc gõ lại để xóa: ư + w -> u, ơ + w -> o
					nr, ok := toggleHorn(c)
					if ok {
						newBuf := make([]rune, len(buf))
						copy(newBuf, buf)
						newBuf[i] = nr
						return newBuf, true
					}
				}
				}
			}
		} else {
			// Không có phụ âm cuối: ưu tiên u/o trước (thuaw -> thưa)
			for i := len(buf) - 1; i >= 0; i-- {
				c := buf[i]
				bare := bareLower(c)
				if bare == 'u' || bare == 'o' {
					dia := getDiacritic(c)
					if dia == DiacriticNone {
						nr, ok := toggleHorn(c)
						if ok {
							newBuf := make([]rune, len(buf))
							copy(newBuf, buf)
							newBuf[i] = nr
							return newBuf, true
						}
					} else if dia == DiacriticHorn {
					// Nguyên tắc gõ lại để xóa: ư + w -> u, ơ + w -> o
					nr, ok := toggleHorn(c)
					if ok {
						newBuf := make([]rune, len(buf))
						copy(newBuf, buf)
						newBuf[i] = nr
						return newBuf, true
					}
				}
				}
			}
			for i := len(buf) - 1; i >= 0; i-- {
				c := buf[i]
				if bareLower(c) == 'a' {
					dia := getDiacritic(c)
					if dia == DiacriticNone || dia == DiacriticBreve {
						if dia == DiacriticCircumflex {
							continue
						}
						nr, ok := toggleBreve(c)
						if ok {
							newBuf := make([]rune, len(buf))
							copy(newBuf, buf)
							newBuf[i] = nr
							return newBuf, true
						}
					}
				}
			}
		}
		// w literal + w -> ư (vòng lặp)
		for i := len(buf) - 1; i >= 0; i-- {
			c := buf[i]
			if c == 'w' || c == 'W' {
				var uw rune
				if isUpper(c) {
					uw = 'Ư'
				} else {
					uw = 'ư'
				}
				newBuf := make([]rune, len(buf))
				copy(newBuf, buf)
				newBuf[i] = uw
				return newBuf, true
			}
		}
	}

	// 3. Tone marks s f r x j - cho phép gõ t e s s t -> test (double s để ra s thường)
	if tone, ok := telexIsToneKey(key); ok {
		hasVowel := false
		for _, r := range buf {
			if isVowel(r) {
				hasVowel = true
				break
			}
		}
		if hasVowel {
			buf = autoPromoteDiphthong(buf)
			pos := findTonePosition(buf, modern)
			if pos != -1 && getTone(buf[pos]) == tone {
				// Cùng dấu đã có -> gõ s lần 2 để ra s thường (xóa dấu và thêm s)
				newBuf := make([]rune, len(buf))
				copy(newBuf, buf)
				if nr, ok := lookup(bareLower(newBuf[pos]), getDiacritic(newBuf[pos]), ToneNone, isUpper(newBuf[pos])); ok {
					newBuf[pos] = nr
				} else {
					newBuf = removeAllTones(newBuf)
				}
				newBuf = append(newBuf, key)
				return newBuf, true
			}
			newBuf, transformed := applyMark(buf, tone, modern)
			if transformed {
				return newBuf, true
			}
		}
	}

	// 4. Remove z
	if lk == 'z' {
		newBuf, ok := tryRemove(buf)
		if ok {
			return newBuf, true
		}
	}

	// 5. W as vowel: standalone w -> ư after failed horn/breve
	if lk == 'w' {
		// If buffer empty handled above, now if last char is consonant or buffer ends with consonant cluster
		// We allow w -> ư when buffer last char is consonant or empty
		// Check if last char is consonant (including not vowel)
		should := false
		if len(buf) == 0 {
			should = true
		} else {
			last := buf[len(buf)-1]
			if isConsonant(last) && bareLower(last) != 'd' {
				should = true
			} else if !isVowel(last) && !isLetter(last) {
				// after space/punct already committed, but buf would be empty in engine, so not here
				should = true
			}
		}
		if should {
			var r rune
			if isUpper(key) {
				r = 'Ư'
			} else {
				r = 'ư'
			}
			newBuf := append([]rune(nil), buf...)
			newBuf = append(newBuf, r)
			return newBuf, true
		}
	}
	// No transform consumed
	return buf, false
}

// TransformStringTelex simulates typing whole word via Telex
func TransformStringTelex(input string, modern bool) string {
	buf := []rune{}
	for _, r := range []rune(input) {
		newBuf, consumed := TelexTransform(buf, r, modern)
		if consumed {
			buf = newBuf
		} else {
			buf = append(buf, r)
		}
	}
	return string(buf)
}
