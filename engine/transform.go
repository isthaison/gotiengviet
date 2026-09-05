package engine

// applyMark applies tone to word buffer
func applyMark(buf []rune, tone int, modern bool) ([]rune, bool) {
	if tone == ToneNone {
		return buf, false
	}
	pos := findTonePosition(buf, modern)
	if pos == -1 {
		return buf, false
	}
	// If already has same tone, consume key but no change (return true with same buf)
	currentTone := getTone(buf[pos])
	if currentTone == tone {
		return buf, true
	}
	// Clear previous tones first
	buf = removeAllTones(append([]rune(nil), buf...))
	// Re-find position after clearing (position may stay same, but ensure)
	pos = findTonePosition(buf, modern)
	if pos == -1 {
		return buf, false
	}
	newBuf := make([]rune, len(buf))
	copy(newBuf, buf)
	newBuf = applyToneAt(newBuf, pos, tone)
	return newBuf, true
}

// tryRemove: z or 0 removes tone first, then diacritic
func tryRemove(buf []rune) ([]rune, bool) {
	if len(buf) == 0 {
		return buf, false
	}
	// Check if any vowel has tone
	hasTone := false
	for _, r := range buf {
		if getTone(r) != ToneNone {
			hasTone = true
			break
		}
	}
	if hasTone {
		// remove all tones (but spec removes tone at position only; clearing all is also okay since only one vowel has tone)
		newBuf := make([]rune, len(buf))
		copy(newBuf, buf)
		newBuf = removeAllTones(newBuf)
		return newBuf, true
	}
	// No tone: try to remove diacritic from last vowel with diacritic
	for i := len(buf) - 1; i >= 0; i-- {
		r := buf[i]
		if !isVowel(r) {
			continue
		}
		dia := getDiacritic(r)
		if dia == DiacriticNone || dia == DiacriticStroke {
			continue
		}
		var nr rune
		var ok bool
		switch dia {
		case DiacriticBreve:
			nr, ok = toggleBreve(r)
		case DiacriticCircumflex:
			nr, ok = toggleCircumflex(r)
		case DiacriticHorn:
			nr, ok = toggleHorn(r)
		default:
			continue
		}
		if ok {
			newBuf := make([]rune, len(buf))
			copy(newBuf, buf)
			newBuf[i] = nr
			return newBuf, true
		}
	}
	return buf, false
}

// Helpers for diacritic transforms on last applicable vowel

func tryCircumflexLast(buf []rune) ([]rune, bool) {
	// Find last vowel that can toggle circumflex (a,e,o)
	for i := len(buf) - 1; i >= 0; i-- {
		r := buf[i]
		if !isVowel(r) && toLower(r) != 'a' && toLower(r) != 'e' && toLower(r) != 'o' {
			continue
		}
		// Also consider plain letters 'a','e','o' that may not be marked as vowel if they are currently consonant? But a/e/o are vowels.
		// Check if toggle possible
		bare := bareLower(r)
		if bare != 'a' && bare != 'e' && bare != 'o' {
			continue
		}
		// Ensure isVowel or plain a/e/o
		if _, ok := charInfoMap[r]; !ok {
			continue
		}
		nr, ok := toggleCircumflex(r)
		if ok {
			newBuf := make([]rune, len(buf))
			copy(newBuf, buf)
			newBuf[i] = nr
			return newBuf, true
		} else {
			// If vowel is 'a' with breve (ă) cannot toggle circumflex, try previous vowel
			continue
		}
	}
	return buf, false
}

func tryBreveLast(buf []rune) ([]rune, bool) {
	for i := len(buf) - 1; i >= 0; i-- {
		r := buf[i]
		bare := bareLower(r)
		if bare != 'a' {
			continue
		}
		nr, ok := toggleBreve(r)
		if ok {
			newBuf := make([]rune, len(buf))
			copy(newBuf, buf)
			newBuf[i] = nr
			return newBuf, true
		}
	}
	return buf, false
}

func tryHornLast(buf []rune, targetBare rune) ([]rune, bool) {
	// targetBare 'o' or 'u' or 0 means any
	for i := len(buf) - 1; i >= 0; i-- {
		r := buf[i]
		bare := bareLower(r)
		if targetBare != 0 && bare != targetBare {
			continue
		}
		if bare != 'o' && bare != 'u' {
			continue
		}
		nr, ok := toggleHorn(r)
		if ok {
			newBuf := make([]rune, len(buf))
			copy(newBuf, buf)
			newBuf[i] = nr
			return newBuf, true
		}
	}
	return buf, false
}

func tryStrokeLast(buf []rune) ([]rune, bool) {
	for i := len(buf) - 1; i >= 0; i-- {
		r := buf[i]
		if bareLower(r) != 'd' {
			continue
		}
		nr, ok := toggleStroke(r)
		if ok {
			newBuf := make([]rune, len(buf))
			copy(newBuf, buf)
			newBuf[i] = nr
			return newBuf, true
		}
	}
	return buf, false
}

// Telex specific try functions that consider the incoming key char

func telexIsToneKey(r rune) (int, bool) {
	switch toLower(r) {
	case 's':
		return ToneSac, true
	case 'f':
		return ToneHuyen, true
	case 'r':
		return ToneHoi, true
	case 'x':
		return ToneNga, true
	case 'j':
		return ToneNang, true
	}
	return ToneNone, false
}

func vniToneKey(r rune) (int, bool) {
	switch r {
	case '1':
		return ToneSac, true
	case '2':
		return ToneHuyen, true
	case '3':
		return ToneHoi, true
	case '4':
		return ToneNga, true
	case '5':
		return ToneNang, true
	}
	return ToneNone, false
}
