package engine

import "unicode"

// findTonePosition returns index in word where tone should be placed
// modern=true uses new style (Bộ GD 2018), false old style (not fully implemented difference mainly oa/oe)
func findTonePosition(word []rune, modern bool) int {
	if len(word) == 0 {
		return -1
	}
	// Collect vowel indices, handling qu/gi special
	vowelIndices := []int{}
	for i, r := range word {
		if isVowel(r) {
			vowelIndices = append(vowelIndices, i)
		}
	}
	if len(vowelIndices) == 0 {
		return -1
	}
	if len(vowelIndices) == 1 {
		return vowelIndices[0]
	}

	// Handle qu: if word starts with q/Q followed by u/U, exclude that u from vowelIndices for placement
	lowerWord := make([]rune, len(word))
	for i, r := range word {
		lowerWord[i] = toLower(r)
	}
	hasQu := len(lowerWord) >= 2 && lowerWord[0] == 'q' && lowerWord[1] == 'u'
	hasGi := len(lowerWord) >= 2 && lowerWord[0] == 'g' && lowerWord[1] == 'i'

	if hasQu {
		// Find index of 'u' after 'q' (position 1) and remove from vowelIndices if present
		for idx, vi := range vowelIndices {
			if vi == 1 && bareLower(word[vi]) == 'u' {
				// remove
				vowelIndices = append(vowelIndices[:idx], vowelIndices[idx+1:]...)
				break
			}
		}
		if len(vowelIndices) == 0 {
			return -1
		}
		if len(vowelIndices) == 1 {
			return vowelIndices[0]
		}
	}
	if hasGi {
		// similar for gi: remove i at 1 if more than 1 vowel and word length >2
		if len(vowelIndices) > 1 {
			for idx, vi := range vowelIndices {
				if vi == 1 && bareLower(word[vi]) == 'i' {
					// Check if word has at least 3 chars and next char is vowel (e.g., "gia", "gio")
					// Only remove if there is vowel after
					if len(vowelIndices) >= 2 {
						vowelIndices = append(vowelIndices[:idx], vowelIndices[idx+1:]...)
						break
					}
				}
			}
		}
		if len(vowelIndices) == 0 {
			return -1
		}
		if len(vowelIndices) == 1 {
			return vowelIndices[0]
		}
	}

	// Priority 1: if any vowel has diacritic (breve/circumflex/horn) -> tone there
	// Count vowels with diacritic
	withDia := []int{}
	for _, vi := range vowelIndices {
		dia := getDiacritic(word[vi])
		if dia == DiacriticCircumflex || dia == DiacriticHorn || dia == DiacriticBreve {
			withDia = append(withDia, vi)
		}
	}
	if len(withDia) == 1 {
		return withDia[0]
	}
	if len(withDia) > 1 {
		// Multiple with diacritic: special handling for ươ, uô etc.
		// Prefer circumflex first, then horn
		// For ươ (both have horn), tone on ơ (the 'o' with horn)
		// For uô (ư + ô)?? Not typical.
		// Simplify: return first circumflex if exists, else last horn? For ươ return the o position (second diacritic)
		for _, vi := range withDia {
			if getDiacritic(word[vi]) == DiacriticCircumflex {
				return vi
			}
		}
		// Horn case: if we have o-horn and u-horn together (ươ)
		// Find o-horn
		for _, vi := range withDia {
			info, _ := charInfoMap[word[vi]]
			if info.Bare == 'o' && info.Diacritic == DiacriticHorn {
				return vi
			}
		}
		// fallback: return last withDia (e.g., ưu -> ư? Actually ưu tone on ư? For "lưu" tone on ư? Should be ư)
		// For ưu (u+ư) -> only second has horn, so len==1 already handled. This multi case is ươ
		return withDia[len(withDia)-1]
	}

	// Build vowel string for special pairs detection
	// Extract contiguous vowel cluster? Assume vowelIndices are consecutive indices in word
	// For best, check if vowels are contiguous (no consonant between first and last vowel)
	// If not contiguous, maybe only consider last cluster
	firstVowel := vowelIndices[0]
	lastVowel := vowelIndices[len(vowelIndices)-1]
	// Check contiguity
	contiguous := (lastVowel-firstVowel+1 == len(vowelIndices))
	var vowelStrLower string
	if contiguous {
		runes := word[firstVowel : lastVowel+1]
		tmp := make([]rune, len(runes))
		for i, r := range runes {
			tmp[i] = toLower(rune(bareLower(r)))
		}
		vowelStrLower = string(tmp)
	} else {
		// non-contiguous: collect bare lowers
		tmp := make([]rune, len(vowelIndices))
		for i, vi := range vowelIndices {
			tmp[i] = toLower(rune(bareLower(word[vi])))
		}
		vowelStrLower = string(tmp)
	}

	specialPairsModern := []string{"oa", "oe", "uy"}
	specialPairsOther := []string{"oo", "uo", "ie", "ua", "uơ", "ươ"}
	for _, p := range specialPairsModern {
		if contains(vowelStrLower, p) {
			if modern {
				// Modern: hoà, hoè, thuỳ
				if len(vowelIndices) == 2 {
					return vowelIndices[1]
				}
				if len(vowelIndices) == 3 {
					return vowelIndices[1]
				}
				return vowelIndices[1]
			} else {
				// Traditional: hòa, hòe, thùy
				if len(vowelIndices) == 2 {
					return vowelIndices[0]
				}
				if len(vowelIndices) == 3 {
					return vowelIndices[1]
				}
				return vowelIndices[0]
			}
		}
	}
	for _, p := range specialPairsOther {
		if contains(vowelStrLower, p) {
			if len(vowelIndices) == 2 {
				return vowelIndices[1]
			}
			if len(vowelIndices) == 3 {
				return vowelIndices[1]
			}
			return vowelIndices[1]
		}
	}

	// Check if word ends with vowel cluster with no final consonant
	// hasFinal = last char in word is not vowel => has final consonant
	hasFinal := false
	if len(word) > 0 {
		lastChar := word[len(word)-1]
		if !isVowel(lastChar) && isLetter(lastChar) {
			// if last char is consonant, has final
			// Need to ensure lastVowel is before lastChar
			if lastVowel < len(word)-1 {
				hasFinal = true
			}
		} else if isVowel(lastChar) {
			hasFinal = false
		} else {
			// ends with non-letter? Should not happen for word buffer
			hasFinal = true
		}
	}

	// Rules for remaining double/triple without special pairs/diacritics
	if len(vowelIndices) == 2 {
		if !hasFinal {
			// open syllable ending with 2 vowels -> tone on first vowel (ai, ao, au, eo, etc.)
			// But oa/oe/uy already handled above as second, so this correctly returns first for others
			return vowelIndices[0]
		}
		// closed syllable (has final consonant) -> tone on second vowel? Actually rule: double vowel with final -> tone on second? Check "hoan" -> tone on a (second)
		// For modern style, double with final -> second
		return vowelIndices[1]
	}
	if len(vowelIndices) == 3 {
		// triple vowel -> tone on middle
		return vowelIndices[1]
	}
	// fallback: second vowel
	if len(vowelIndices) >= 2 {
		return vowelIndices[1]
	}
	return vowelIndices[0]
}

func contains(s, substr string) bool {
	// simple contains
	if len(substr) > len(s) {
		return false
	}
	for i := 0; i <= len(s)-len(substr); i++ {
		if s[i:i+len(substr)] == substr {
			return true
		}
	}
	return false
}

func hasFinalConsonant(word []rune) bool {
	if len(word) == 0 {
		return false
	}
	last := word[len(word)-1]
	if isVowel(last) {
		return false
	}
	if unicode.IsLetter(last) {
		return true
	}
	return false
}
