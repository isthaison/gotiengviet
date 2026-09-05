package engine

// VNITransform handles VNI input method (numbers)
func VNITransform(buf []rune, key rune, modern bool) ([]rune, bool) {
	// Emoji: đang trong :xxx chưa đóng thì không transform
	if len(buf) > 0 && buf[0] == ':' {
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
	// VNI mappings:
	// 6 -> circumflex â ê ô
	// 7 -> horn ơ ư
	// 8 -> breve ă
	// 9 -> stroke đ
	// 1-5 -> tones
	// 0 -> remove

	switch key {
	case '6':
		newBuf, ok := tryCircumflexLast(buf)
		if ok {
			return newBuf, true
		}
		return buf, false
	case '7':
		// Prefer last o/u with horn toggle
		// Search for o first then u? Actually find last applicable
		newBuf, ok := tryHornLast(buf, 0)
		if ok {
			return newBuf, true
		}
		return buf, false
	case '8':
		newBuf, ok := tryBreveLast(buf)
		if ok {
			return newBuf, true
		}
		return buf, false
	case '9':
		newBuf, ok := tryStrokeLast(buf)
		if ok {
			return newBuf, true
		}
		return buf, false
	case '0':
		newBuf, ok := tryRemove(buf)
		if ok {
			return newBuf, true
		}
		return buf, false
	case '1', '2', '3', '4', '5':
		tone, _ := vniToneKey(key)
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
				// Nguyên tắc gõ lại để xóa: a1->á, á1->a (xóa, không thêm 1)
				newBuf := make([]rune, len(buf))
				copy(newBuf, buf)
				if nr, ok := lookup(bareLower(newBuf[pos]), getDiacritic(newBuf[pos]), ToneNone, isUpper(newBuf[pos])); ok {
					newBuf[pos] = nr
				} else {
					newBuf = removeAllTones(newBuf)
				}
				return newBuf, true
			}
			newBuf, transformed := applyMark(buf, tone, modern)
			if transformed {
				return newBuf, true
			}
		}
		return buf, false
	}
	return buf, false
}

func TransformStringVNI(input string, modern bool) string {
	buf := []rune{}
	for _, r := range []rune(input) {
		newBuf, consumed := VNITransform(buf, r, modern)
		if consumed {
			buf = newBuf
		} else {
			// If key is digit but not consumed (no vowel), append digit as literal?
			// Check if key is VNI control digit and not consumed -> we should append digit literally only if not transformed
			// For consistency, if it's a digit that could be VNI but buffer has no vowel, treat as literal digit?
			// We'll append r anyway except when r is control that was consumed
			// But our VNITransform already returned false for those, so we append r below
			// For letters, just append
			buf = append(buf, r)
		}
	}
	return string(buf)
}
