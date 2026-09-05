package engine

import (
	"bytes"
	"encoding/json"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"
)

// AIProvider interface cho AI local nhẹ (qwen2:0.5b) + fallback Rule
type AIProvider interface {
	Suggest(preedit, ctx string) []string
	IsAvailable() bool
}

type OllamaProvider struct {
	URL   string
	Model string
}

type RuleProvider struct{}

func NewAIProvider() AIProvider {
	home, _ := os.UserHomeDir()
	cfgPath := filepath.Join(home, ".config", "gotiengviet", "ai.conf")
	// Đọc ai.conf: [ai] port=55602, url=http://localhost:55602, model=qwen2:0.5b, provider=ollama
	url := "http://localhost:55602"
	model := "qwen2:0.5b"
	provider := "ollama"
	if data, err := os.ReadFile(cfgPath); err == nil {
		for _, line := range strings.Split(string(data), "\n") {
			line = strings.TrimSpace(line)
			if strings.HasPrefix(line, "url=") {
				url = strings.TrimSpace(strings.SplitN(line, "=", 2)[1])
			}
			if strings.HasPrefix(line, "port=") {
				port := strings.TrimSpace(strings.SplitN(line, "=", 2)[1])
				if port != "" {
					url = "http://localhost:" + port
				}
			}
			if strings.HasPrefix(line, "model=") {
				model = strings.TrimSpace(strings.SplitN(line, "=", 2)[1])
			}
			if strings.HasPrefix(line, "provider=") {
				provider = strings.TrimSpace(strings.SplitN(line, "=", 2)[1])
			}
		}
	}
	if provider == "rule" {
		return &RuleProvider{}
	}
	return &OllamaProvider{URL: url, Model: model}
}

func (o *OllamaProvider) IsAvailable() bool {
	client := http.Client{Timeout: 1 * time.Second}
	resp, err := client.Get(o.URL + "/api/tags")
	if err != nil {
		return false
	}
	defer resp.Body.Close()
	return resp.StatusCode == 200
}

func (o *OllamaProvider) Suggest(preedit, ctx string) []string {
	if !o.IsAvailable() {
		return (&RuleProvider{}).Suggest(preedit, ctx)
	}
	// Gọi ollama generate với timeout 1.5s
	prompt := "Sửa chính tả tiếng Việt: '" + preedit + "' (ngữ cảnh: '" + ctx + "') -> chỉ trả về từ đúng, không giải thích:"
	payload := map[string]interface{}{"model": o.Model, "prompt": prompt, "stream": false}
	data, _ := json.Marshal(payload)
	client := http.Client{Timeout: 1500 * time.Millisecond}
	resp, err := client.Post(o.URL+"/api/generate", "application/json", bytes.NewBuffer(data))
	if err != nil {
		return (&RuleProvider{}).Suggest(preedit, ctx)
	}
	defer resp.Body.Close()
	var out map[string]interface{}
	if json.NewDecoder(resp.Body).Decode(&out) != nil {
		return (&RuleProvider{}).Suggest(preedit, ctx)
	}
	if r, ok := out["response"].(string); ok {
		r = strings.TrimSpace(r)
		if r != "" && r != preedit {
			return []string{r}
		}
	}
	return (&RuleProvider{}).Suggest(preedit, ctx)
}

func (r *RuleProvider) IsAvailable() bool { return true }
func (r *RuleProvider) Suggest(preedit, ctx string) []string {
	// Fallback dùng spell.go
	if IsValidVietnameseWord(preedit) {
		return nil
	}
	return SuggestCorrections(preedit)
}
