package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"strings"

	"github.com/isthaison/gotiengviet/engine"
)

func main() {
	fmt.Println("GoTiengViet Demo - Gõ Telex/VNI (thuần Go stdlib, không lib ngoài)")
	fmt.Println("Ví dụ: as -> á, aa -> â, ow -> ơ, dd -> đ, hoaf -> hoà, tieensg -> tiếng")
	fmt.Println("Chế độ mặc định: Telex. Gõ 'mode vni' để đổi, 'mode telex' để về, 'quit' để thoát.")
	fmt.Println()

	e := engine.NewEngine(engine.ModeTelex)
	reader := bufio.NewReader(os.Stdin)

	// Test cố định
	tests := [][3]string{
		{"as", "á", "telex"},
		{"aw", "ă", "telex"},
		{"aa", "â", "telex"},
		{"dd", "đ", "telex"},
		{"chaof", "chào", "telex"},
		{"hoaf", "hoà", "telex"},
		{"tieensg", "tiếng", "telex"},
		{"dduwowcj", "được", "telex"},
		{"a1", "á", "vni"},
		{"a6", "â", "vni"},
		{"o75", "ợ", "vni"},
	}
	fmt.Println("=== Test tự động ===")
	for _, t := range tests {
		var out string
		if t[2] == "telex" {
			out = engine.TransformStringTelex(t[0], true)
		} else {
			out = engine.TransformStringVNI(t[0], true)
		}
		ok := "✓"
		if out != t[1] {
			ok = "✗"
		}
		fmt.Printf("%s %s %q -> %q (expect %q)\n", ok, t[2], t[0], out, t[1])
	}
	fmt.Println()

	for {
		modeStr := "Telex"
		if e.Mode == engine.ModeVNI {
			modeStr = "VNI"
		}
		fmt.Printf("[%s] Nhập chuỗi > ", modeStr)
		line, err := reader.ReadString('\n')
		if err != nil {
			if err == io.EOF {
				if strings.TrimSpace(line) == "" {
					break
				}
			} else {
				break
			}
		}
		line = strings.TrimSpace(line)
		if line == "quit" || line == "exit" {
			break
		}
		if line == "mode vni" {
			e.SetMode(engine.ModeVNI)
			fmt.Println("Đã chuyển sang VNI (1-5 dấu, 6-9 ăâêôơưđ)")
			continue
		}
		if line == "mode telex" {
			e.SetMode(engine.ModeTelex)
			fmt.Println("Đã chuyển sang Telex (sfrxj, aa ee oo aw ow uw dd, z xóa)")
			continue
		}
		if line == "" {
			continue
		}
		// Dùng engine stateful để mô phỏng gõ từng phím
		e.Reset()
		var committed string
		for _, r := range []rune(line) {
			if r == ' ' {
				committed += e.Buffer() + " "
				e.Reset()
				continue
			}
			comp, _, commit := e.ProcessKey(r)
			if commit != "" {
				committed += commit
			} else {
				_ = comp
			}
		}
		committed += e.Buffer()
		fmt.Printf("Kết quả: %s\n", committed)
		// So sánh với transform stateless
		var direct string
		if e.Mode == engine.ModeTelex {
			direct = engine.TransformStringTelex(line, true)
		} else {
			direct = engine.TransformStringVNI(line, true)
		}
		if direct != committed {
			fmt.Printf(" (direct: %s)\n", direct)
		}
	}
}
