package engine

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

	// 1. Stroke dd -> đ
	if toLower(key) == 'd' {
		// Check if last char is d/đ
		if len(buf) > 0 {
			last := buf[len(buf)-1]
			if bareLower(last) == 'd' {
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
	// aa -> â
	if lk == 'a' && len(buf) > 0 {
		last := buf[len(buf)-1]
		// If last is 'a' (any tone) with No diacritic, try circumflex
		if bareLower(last) == 'a' && getDiacritic(last) == DiacriticNone {
			// Toggle a -> â
			nr, ok := toggleCircumflex(last)
			if ok {
				newBuf := make([]rune, len(buf))
				copy(newBuf, buf)
				newBuf[len(newBuf)-1] = nr
				return newBuf, true
			}
		} else if bareLower(last) == 'a' && getDiacritic(last) == DiacriticCircumflex {
			// â + a -> a (revert)
			nr, ok := toggleCircumflex(last)
			if ok {
				newBuf := make([]rune, len(buf))
				copy(newBuf, buf)
				newBuf[len(newBuf)-1] = nr
				return newBuf, true
			}
		}
	}
	if lk == 'e' && len(buf) > 0 {
		last := buf[len(buf)-1]
		if bareLower(last) == 'e' {
			dia := getDiacritic(last)
			if dia == DiacriticNone {
				nr, ok := toggleCircumflex(last)
				if ok {
					newBuf := make([]rune, len(buf))
					copy(newBuf, buf)
					newBuf[len(newBuf)-1] = nr
					return newBuf, true
				}
			} else if dia == DiacriticCircumflex {
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
			dia := getDiacritic(last)
			if dia == DiacriticNone {
				nr, ok := toggleCircumflex(last)
				if ok {
					newBuf := make([]rune, len(buf))
					copy(newBuf, buf)
					newBuf[len(newBuf)-1] = nr
					return newBuf, true
				}
			} else if dia == DiacriticCircumflex {
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
