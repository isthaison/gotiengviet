package engine

// isVietnameseCoda kiểm tra phụ âm cuối tiếng Việt hợp lệ:
// Rỗng hoặc: c, ch, m, n, ng, nh, p, t
func isVietnameseCoda(s string) bool {
	switch s {
	case "", "c", "ch", "m", "n", "ng", "nh", "p", "t":
		return true
	default:
		return false
	}
}

// isForeignWord kiểm tra từ mang cấu trúc ngoại lai / tiếng Anh:
// 1. Bắt đầu bằng 'w'/'W'
// 2. Chứa ký tự ngoại lai: f, j, z (hoặc w đơn lẻ giữa các phụ âm)
// 3. Chứa phụ âm đôi (ss, ll, tt, pp, ff, kk, bb, mm, nn, cc...) ngoại trừ dd (đã thành đ)
// 4. Có phụ âm sau nguyên âm mà không phải coda hợp lệ tiếng Việt
func isForeignWord(buf []rune) bool {
	if len(buf) == 0 {
		return false
	}
	if buf[0] == 'w' || buf[0] == 'W' {
		return true
	}
	lastV := -1
	for i, r := range buf {
		if isVowel(r) {
			lastV = i
		}
		lc := bareLower(r)
		if lc == 'f' || lc == 'j' || lc == 'z' {
			return true
		}
		if i+1 < len(buf) {
			next := buf[i+1]
			if lc == bareLower(next) && isConsonant(r) && lc != 'd' {
				return true
			}
		}
	}
	return lastV >= 0 && lastV < len(buf)-1 && !isVietnameseCoda(string(buf[lastV+1:]))
}

// autoPromoteDiphthong tự động nâng cấp nguyên âm đôi/ba khi có phụ âm cuối:
// i + e + [coda] -> iê + [coda], u + o + [coda] -> uô + [coda], y + e + [coda] -> yê + [coda]
func autoPromoteDiphthong(buf []rune) []rune {
	if len(buf) < 3 {
		return buf
	}
	n := len(buf)
	// Ưu tiên bán nguyên âm cuối: ieu, yeu, uoi.
	last, prev, first := bareLower(buf[n-1]), bareLower(buf[n-2]), bareLower(buf[n-3])
	if (last == 'u' && prev == 'e' && (first == 'i' || first == 'y')) ||
		(last == 'i' && prev == 'o' && first == 'u') {
		if promoted, ok := promoteVowelPair(buf, n-2); ok {
			return promoted
		}
	}

	// Nếu có phụ âm cuối, nâng nguyên âm ngay trước nó.
	lastV := lastVowelIndex(buf)
	if lastV > 0 && lastV < n-1 {
		if promoted, ok := promoteVowelPair(buf, lastV); ok {
			return promoted
		}
	}
	return buf
}

func lastVowelIndex(buf []rune) int {
	for i := len(buf) - 1; i >= 0; i-- {
		if isVowel(buf[i]) {
			return i
		}
	}
	return -1
}

// promoteVowelPair nâng ie/ye thành iê/yê và uo thành uô tại vị trí pos.
func promoteVowelPair(buf []rune, pos int) ([]rune, bool) {
	first, last := buf[pos-1], buf[pos]
	pairMatches := (bareLower(last) == 'e' && (bareLower(first) == 'i' || bareLower(first) == 'y')) ||
		(bareLower(last) == 'o' && bareLower(first) == 'u' && getDiacritic(first) == DiacriticNone)
	if pairMatches && getDiacritic(last) == DiacriticNone {
		if nr, ok := toggleCircumflex(last); ok {
			result := append([]rune(nil), buf...)
			result[pos] = nr
			return result, true
		}
	}
	return buf, false
}

// tryEnglishRestore khôi phục các đuôi tiếng Anh -st, -sh, -sk (test, post, fast, fish, task)
// Tuyệt đối không áp dụng cho 'p' vì 'p' là phụ âm cuối tiếng Việt (pháp, tháp, cáp).
func tryEnglishRestore(buf []rune, key rune) ([]rune, bool) {
	if len(buf) == 0 {
		return buf, false
	}
	lk := toLower(key)
	if lk != 't' && lk != 'h' && lk != 'k' {
		return buf, false
	}
	last := buf[len(buf)-1]
	// Nguyên âm đã có mũ/móc/trăng là tiếng Việt: giữ dấu khi gõ phụ âm cuối.
	if isVowel(last) && getTone(last) == ToneSac && getDiacritic(last) == DiacriticNone {
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

// TelexTransform xử lý gõ phím theo mô hình máy trạng thái âm tiết tiếng Việt thống nhất.
func TelexTransform(buf []rune, key rune, modern bool) ([]rune, bool) {
	lk := toLower(key)

	// Phím gõ tắt ngoặc vuông: [, ], {, }
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

	// Bỏ qua nội dung trong mã emoji :smile:
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

	// PHA 1: NGUYÊN TẮC HOÀN TÁC PHÍM LẶP THỐNG NHẤT (UNIVERSAL REPEAT-KEY UNDO)
	// Khi gõ lại đúng phím điều khiển vừa tác động -> xóa dấu và trả về ký tự thô

	// Lặp phím w: xóa dấu móc/trăng hoặc hoàn tác về w / uw / ow / aw
	if lk == 'w' && len(buf) > 0 {
		last := buf[len(buf)-1]
		dia := getDiacritic(last)
		bare := bareLower(last)

		if bare == 'u' && dia == DiacriticHorn {
			// Ký tự cuối là ư/Ư
			if len(buf) == 1 {
				// ww -> w
				wChar := 'w'
				if isUpper(last) || isUpper(key) {
					wChar = 'W'
				}
				return []rune{wChar}, true
			}
			prev := buf[len(buf)-2]
			if isConsonant(prev) {
				// Sau phụ âm: passư + w -> passw, sư + w -> sw
				wChar := 'w'
				if isUpper(last) || isUpper(key) {
					wChar = 'W'
				}
				newBuf := append([]rune(nil), buf...)
				newBuf[len(newBuf)-1] = wChar
				return newBuf, true
			}
			// Sau nguyên âm: u + w + w -> uw
			uBare := 'u'
			if isUpper(last) {
				uBare = 'U'
			}
			wChar := 'w'
			if isUpper(last) && isUpper(key) {
				wChar = 'W'
			}
			newBuf := append([]rune(nil), buf...)
			newBuf[len(newBuf)-1] = uBare
			return append(newBuf, wChar), true
		} else if bare == 'o' && dia == DiacriticHorn {
			// o + w + w -> ow (khôi phục o + w, ví dụ showw -> show)
			oBare := 'o'
			if isUpper(last) {
				oBare = 'O'
			}
			wChar := 'w'
			if isUpper(last) && isUpper(key) {
				wChar = 'W'
			}
			newBuf := append([]rune(nil), buf...)
			newBuf[len(newBuf)-1] = oBare
			return append(newBuf, wChar), true
		} else if bare == 'a' && dia == DiacriticBreve {
			// a + w + w -> aw (khôi phục a + w, ví dụ raww -> raw)
			aBare := 'a'
			if isUpper(last) {
				aBare = 'A'
			}
			wChar := 'w'
			if isUpper(last) && isUpper(key) {
				wChar = 'W'
			}
			newBuf := append([]rune(nil), buf...)
			newBuf[len(newBuf)-1] = aBare
			return append(newBuf, wChar), true
		}
	}

	// Lặp phím d: đ + d -> dd (reddit, hidden...)
	if lk == 'd' && len(buf) > 0 {
		last := buf[len(buf)-1]
		if bareLower(last) == 'd' && getDiacritic(last) == DiacriticStroke {
			dBare := 'd'
			if isUpper(last) {
				dBare = 'D'
			}
			newBuf := append([]rune(nil), buf...)
			newBuf[len(newBuf)-1] = dBare
			return append(newBuf, key), true
		}
	}

	// Lặp a/e/o: bỏ mũ, giữ dấu thanh và trả lại phím thô.
	if (lk == 'a' || lk == 'e' || lk == 'o') && len(buf) > 0 {
		last := buf[len(buf)-1]
		if bareLower(last) == lk && getDiacritic(last) == DiacriticCircumflex {
			nc, ok := lookup(lk, DiacriticNone, getTone(last), isUpper(last))
			if !ok {
				nc = lk
				if isUpper(last) {
					nc = lk - 'a' + 'A'
				}
			}
			newBuf := append([]rune(nil), buf...)
			newBuf[len(newBuf)-1] = nc
			return append(newBuf, key), true
		}
	}

	// Lặp phím dấu thanh (s, f, r, x, j): gõ lại đúng phím đó thì xóa dấu thanh
	if tone, ok := telexIsToneKey(key); ok {
		pos := findTonePosition(buf, modern)
		if pos != -1 && getTone(buf[pos]) == tone {
			newBuf := append([]rune(nil), buf...)
			if nr, ok := lookup(bareLower(newBuf[pos]), getDiacritic(newBuf[pos]), ToneNone, isUpper(newBuf[pos])); ok {
				newBuf[pos] = nr
			} else {
				newBuf = removeAllTones(newBuf)
			}
			// Nếu dấu thanh ở ngay cuối từ (pa + s -> pá), gõ s lần 2 khôi phục cả 2 chữ: pass, buff, kiss...
			if pos == len(buf)-1 {
				newBuf = append(newBuf, key, key)
			} else {
				newBuf = append(newBuf, key)
			}
			return newBuf, true
		}
	}

	// Phím z: xóa dấu thanh
	if lk == 'z' {
		if newBuf, ok := tryRemove(buf); ok {
			return newBuf, true
		}
	}

	// Khôi phục đuôi tiếng Anh -st, -sh, -sk (test, post, fast, fish, task)
	// Tuyệt đối không áp dụng cho 'p' vì 'p' là phụ âm cuối tiếng Việt (pháp, tháp, cáp)
	if newBuf, ok := tryEnglishRestore(buf, key); ok {
		return newBuf, true
	}

	// PHA 2: BẢO VỆ TỪ NGOẠI LAI (FOREIGN WORD BYPASS)
	// Khi từ đã mang cấu trúc tiếng Anh (password, class, word...), không can thiệp
	if isForeignWord(buf) {
		return buf, false
	}

	// PHA 3: BIẾN ĐỔI MŨ / MÓC / TRĂNG (a, e, o, w, d)

	// Phím d -> đ
	if lk == 'd' && len(buf) > 0 {
		last := buf[len(buf)-1]
		if bareLower(last) == 'd' && getDiacritic(last) == DiacriticNone {
			if nr, ok := toggleStroke(last); ok {
				newBuf := append([]rune(nil), buf...)
				newBuf[len(newBuf)-1] = nr
				return newBuf, true
			}
		}
	}

	// a/e/o biến đổi nguyên âm cùng loại ngay trước phím đang gõ.
	if (lk == 'a' || lk == 'e' || lk == 'o') && len(buf) > 0 {
		last := buf[len(buf)-1]
		if bareLower(last) == lk && getDiacritic(last) == DiacriticNone {
			if nr, ok := toggleCircumflex(last); ok {
				newBuf := append([]rune(nil), buf...)
				newBuf[len(newBuf)-1] = nr
				return newBuf, true
			}
		}
	}

	// Phím w biến đổi:
	if lk == 'w' {
		// w đơn lẻ ở đầu từ -> ư
		if len(buf) == 0 {
			wRune := 'ư'
			if isUpper(key) {
				wRune = 'Ư'
			}
			return []rune{wRune}, true
		}

		// uow -> ươ shortcut
		if len(buf) >= 2 {
			secondLast := buf[len(buf)-2]
			last := buf[len(buf)-1]
			if bareLower(secondLast) == 'u' && bareLower(last) == 'o' &&
				getDiacritic(secondLast) == DiacriticNone && getDiacritic(last) == DiacriticNone {
				newBuf := append([]rune(nil), buf...)
				if nr1, ok1 := toggleHorn(secondLast); ok1 {
					newBuf[len(newBuf)-2] = nr1
				}
				if nr2, ok2 := toggleHorn(last); ok2 {
					newBuf[len(newBuf)-1] = nr2
				}
				return newBuf, true
			}
		}

		// Kiểm tra có phụ âm cuối tiếng Việt hợp lệ không
		hasFinal := false
		last := buf[len(buf)-1]
		if isConsonant(last) {
			lastV := lastVowelIndex(buf)
			if lastV >= 0 && lastV < len(buf)-1 {
				coda := string(buf[lastV+1:])
				if isVietnameseCoda(coda) && coda != "" {
					hasFinal = true
				}
			}
		}

		if hasFinal {
			// Có phụ âm cuối hợp lệ: ưu tiên biến a -> ă (hoacw -> hoặc, ngoacw -> ngoặc)
			for i := len(buf) - 1; i >= 0; i-- {
				c := buf[i]
				if bareLower(c) == 'a' && getDiacritic(c) == DiacriticNone {
					if nr, ok := toggleBreve(c); ok {
						newBuf := append([]rune(nil), buf...)
						newBuf[i] = nr
						return newBuf, true
					}
				}
			}
			// Nếu không có a, gắn móc cho u hoặc o
			for i := len(buf) - 1; i >= 0; i-- {
				c := buf[i]
				bare := bareLower(c)
				if (bare == 'u' || bare == 'o') && getDiacritic(c) == DiacriticNone {
					if nr, ok := toggleHorn(c); ok {
						newBuf := append([]rune(nil), buf...)
						newBuf[i] = nr
						return newBuf, true
					}
				}
			}
		} else if isVowel(buf[len(buf)-1]) {
			// Không có phụ âm cuối: ký tự cuối PHẢI là nguyên âm (thuaw -> thưa, aw -> ă)
			for i := len(buf) - 1; i >= 0; i-- {
				c := buf[i]
				bare := bareLower(c)
				if (bare == 'u' || bare == 'o') && getDiacritic(c) == DiacriticNone {
					if nr, ok := toggleHorn(c); ok {
						newBuf := append([]rune(nil), buf...)
						newBuf[i] = nr
						return newBuf, true
					}
				}
			}
			for i := len(buf) - 1; i >= 0; i-- {
				c := buf[i]
				if bareLower(c) == 'a' && getDiacritic(c) == DiacriticNone {
					if nr, ok := toggleBreve(c); ok {
						newBuf := append([]rune(nil), buf...)
						newBuf[i] = nr
						return newBuf, true
					}
				}
			}
		} else {
			// w đóng vai trò nguyên âm 'ư' sau phụ âm đầu (chưa có nguyên âm nào trong từ: sư, tư, như...)

			if lastVowelIndex(buf) == -1 && isConsonant(last) && bareLower(last) != 'd' && bareLower(last) != 'w' {
				wRune := 'ư'
				if isUpper(key) {
					wRune = 'Ư'
				}
				return append(buf, wRune), true
			}
		}
	}

	// PHA 4: BIẾN ĐỔI DẤU THANH (s, f, r, x, j)
	if tone, ok := telexIsToneKey(key); ok {

		if lastVowelIndex(buf) >= 0 {
			buf = autoPromoteDiphthong(buf)
			if newBuf, transformed := applyMark(buf, tone, modern); transformed {
				return newBuf, true
			}
		}
	}

	// Không có biến đổi nào được tiêu thụ -> phím sẽ được append thông thường
	return buf, false
}

// TransformStringTelex mô phỏng gõ chuỗi qua Telex
func TransformStringTelex(input string, modern bool) string {
	buf := []rune{}
	for _, r := range input {
		newBuf, consumed := TelexTransform(buf, r, modern)
		if consumed {
			buf = newBuf
		} else {
			buf = append(buf, r)
		}
	}
	return string(buf)
}
