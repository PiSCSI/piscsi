package web

import (
	"strings"
	"testing"
)

func TestCatalogLoadsCopiedTranslations(t *testing.T) {
	catalog, err := LoadCatalog()
	if err != nil {
		t.Fatal(err)
	}

	if got := catalog.Translate("de", "Appearance"); got != "Erscheinungsbild" {
		t.Fatalf("German translation = %q", got)
	}
	if got := catalog.Translate("en", "Logging"); got != "Logging" {
		t.Fatalf("English fallback = %q", got)
	}
}

func TestMatchLocale(t *testing.T) {
	tests := map[string]string{
		"sv-SE,sv;q=0.9,en;q=0.8": "sv",
		"zh-CN,zh;q=0.9":          "zh",
		"it-IT,it;q=0.9":          "en",
		"":                        "en",
	}
	for header, want := range tests {
		if got := MatchLocale(header); got != want {
			t.Errorf("MatchLocale(%q) = %q, want %q", header, got, want)
		}
	}
}

func TestLocalizeHTML(t *testing.T) {
	catalog, err := LoadCatalog()
	if err != nil {
		t.Fatal(err)
	}

	input := []byte(`<!doctype html><html lang="en"><body><h1>Appearance</h1><p>Logged in as <em>alice</em></p><input value="Change Theme"><script>if (a < b) {}</script></body></html>`)
	output, err := localizeHTML(input, "de", catalog)
	if err != nil {
		t.Fatal(err)
	}
	rendered := string(output)
	for _, want := range []string{`lang="de"`, "Erscheinungsbild", `Als <em>alice</em> angemeldet`, `value="Theme ändern"`, `if (a < b) {}`} {
		if !strings.Contains(rendered, want) {
			t.Errorf("localized HTML does not contain %q:\n%s", want, rendered)
		}
	}
}

func TestLocalizeHTMLPreservesEscapedText(t *testing.T) {
	catalog, err := LoadCatalog()
	if err != nil {
		t.Fatal(err)
	}

	input := []byte(`<div>Theme changed to &#39;modern&#39;.</div>`)
	output, err := localizeHTML(input, "en", catalog)
	if err != nil {
		t.Fatal(err)
	}
	if got := string(output); got != string(input) {
		t.Fatalf("localized HTML = %q, want %q", got, input)
	}
}
