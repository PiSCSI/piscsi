// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package web

import (
	"bufio"
	"bytes"
	"fmt"
	stdhtml "html"
	"html/template"
	"io"
	"net/http"
	"regexp"
	"sort"
	"strconv"
	"strings"

	"github.com/gin-gonic/gin"
	"github.com/gin-gonic/gin/render"
	xhtml "golang.org/x/net/html"
	"golang.org/x/text/language"
)

var (
	SupportedLocales = []string{"en", "de", "sv", "fr", "es", "zh"}
	localeMatcher    = language.NewMatcher([]language.Tag{
		language.English,
		language.German,
		language.Swedish,
		language.French,
		language.Spanish,
		language.Chinese,
	})
)

// Catalog contains the gettext messages used by the browser UI.
type Catalog struct {
	messages     map[string]map[string]string
	interpolated map[string][]interpolatedMessage
	markup       map[string][]message
}

type message struct {
	msgid  string
	msgstr string
}

type interpolatedMessage struct {
	expression  *regexp.Regexp
	translation string
	fields      []string
}

// LoadCatalog loads the copied Python-client PO catalogs.
func LoadCatalog() (*Catalog, error) {
	catalog := &Catalog{
		messages:     make(map[string]map[string]string),
		interpolated: make(map[string][]interpolatedMessage),
		markup:       make(map[string][]message),
	}
	for _, locale := range SupportedLocales {
		catalog.messages[locale] = make(map[string]string)
		if locale == "en" {
			continue
		}

		name := fmt.Sprintf("translations/%s/LC_MESSAGES/messages.po", locale)
		data, err := content.ReadFile(name)
		if err != nil {
			return nil, fmt.Errorf("read %s catalog: %w", locale, err)
		}
		catalog.messages[locale] = parsePO(data)
		for msgid, msgstr := range catalog.messages[locale] {
			switch {
			case strings.Contains(msgid, "%("):
				if compiled, ok := compileInterpolatedMessage(msgid, msgstr); ok {
					catalog.interpolated[locale] = append(catalog.interpolated[locale], compiled)
				}
			case strings.Contains(msgid, "<"):
				catalog.markup[locale] = append(catalog.markup[locale], message{msgid: msgid, msgstr: msgstr})
			}
		}
		sort.Slice(catalog.markup[locale], func(i, j int) bool {
			return len(catalog.markup[locale][i].msgid) > len(catalog.markup[locale][j].msgid)
		})
	}
	return catalog, nil
}

// IsSupportedLocale reports whether locale is one of the UI locales.
func IsSupportedLocale(locale string) bool {
	for _, supported := range SupportedLocales {
		if locale == supported {
			return true
		}
	}
	return false
}

// MatchLocale chooses the closest supported locale from Accept-Language.
func MatchLocale(acceptLanguage string) string {
	tags, _, err := language.ParseAcceptLanguage(acceptLanguage)
	if err != nil || len(tags) == 0 {
		return "en"
	}
	_, index, confidence := localeMatcher.Match(tags...)
	if confidence == language.No {
		return "en"
	}
	return SupportedLocales[index]
}

// Translate returns the localized message, falling back to the English msgid.
func (c *Catalog) Translate(locale, msgid string) string {
	if c == nil || locale == "en" {
		return msgid
	}
	if translated := c.messages[locale][msgid]; translated != "" {
		return translated
	}
	return msgid
}

// LocalizedRenderer translates visible template strings after template
// execution. This keeps the gettext catalogs as the source of truth while
// allowing the Go templates to retain their normal html/template syntax.
type LocalizedRenderer struct {
	Template *template.Template
	Catalog  *Catalog
}

func (r LocalizedRenderer) Instance(name string, data any) render.Render {
	locale := "en"
	if values, ok := data.(gin.H); ok {
		if selected, ok := values["Locale"].(string); ok && IsSupportedLocale(selected) {
			locale = selected
		}
	}
	return localizedHTML{
		template: r.Template,
		name:     name,
		data:     data,
		locale:   locale,
		catalog:  r.Catalog,
	}
}

type localizedHTML struct {
	template *template.Template
	name     string
	data     any
	locale   string
	catalog  *Catalog
}

func (r localizedHTML) Render(w http.ResponseWriter) error {
	var rendered bytes.Buffer
	var err error
	if r.name == "" {
		err = r.template.Execute(&rendered, r.data)
	} else {
		err = r.template.ExecuteTemplate(&rendered, r.name, r.data)
	}
	if err != nil {
		return err
	}

	localized, err := localizeHTML(rendered.Bytes(), r.locale, r.catalog)
	if err != nil {
		return err
	}
	_, err = w.Write(localized)
	return err
}

func (r localizedHTML) WriteContentType(w http.ResponseWriter) {
	header := w.Header()
	if header.Get("Content-Type") == "" {
		header["Content-Type"] = []string{"text/html; charset=utf-8"}
	}
}

func localizeHTML(document []byte, locale string, catalog *Catalog) ([]byte, error) {
	document = catalog.translateStructured(document, locale)
	tokenizer := xhtml.NewTokenizer(bytes.NewReader(document))
	var output bytes.Buffer

	for {
		tokenType := tokenizer.Next()
		switch tokenType {
		case xhtml.ErrorToken:
			if err := tokenizer.Err(); err != io.EOF {
				return nil, fmt.Errorf("tokenize rendered HTML: %w", err)
			}
			return output.Bytes(), nil
		case xhtml.TextToken:
			raw := bytes.Clone(tokenizer.Raw())
			token := tokenizer.Token()
			translated := translateText(token.Data, locale, catalog)
			if translated == token.Data {
				output.Write(raw)
			} else {
				output.WriteString(stdhtml.EscapeString(translated))
			}
		case xhtml.StartTagToken, xhtml.SelfClosingTagToken:
			token := tokenizer.Token()
			if token.Data == "html" {
				for i := range token.Attr {
					if token.Attr[i].Key == "lang" {
						token.Attr[i].Val = locale
					}
				}
			}
			for i := range token.Attr {
				switch token.Attr[i].Key {
				case "placeholder", "title", "value":
					token.Attr[i].Val = catalog.Translate(locale, token.Attr[i].Val)
				}
			}
			output.WriteString(token.String())
		default:
			output.Write(tokenizer.Raw())
		}
	}
}

var placeholderPattern = regexp.MustCompile(`%\(([^)]+)\)[#0 +\-]?(?:\d+)?(?:\.\d+)?[a-zA-Z]`)

func compileInterpolatedMessage(msgid, msgstr string) (interpolatedMessage, bool) {
	matches := placeholderPattern.FindAllStringSubmatchIndex(msgid, -1)
	if len(matches) == 0 {
		return interpolatedMessage{}, false
	}

	var expression strings.Builder
	var fields []string
	expression.WriteString(regexp.QuoteMeta(msgid[:matches[0][0]]))
	for i, match := range matches {
		fields = append(fields, msgid[match[2]:match[3]])
		expression.WriteString(`(.*?)`)
		end := match[1]
		if i+1 < len(matches) {
			expression.WriteString(regexp.QuoteMeta(msgid[end:matches[i+1][0]]))
		} else {
			expression.WriteString(regexp.QuoteMeta(msgid[end:]))
		}
	}
	compiled, err := regexp.Compile(expression.String())
	if err != nil {
		return interpolatedMessage{}, false
	}
	return interpolatedMessage{expression: compiled, translation: msgstr, fields: fields}, true
}

func (c *Catalog) translateStructured(document []byte, locale string) []byte {
	if c == nil || locale == "en" {
		return document
	}
	output := string(document)
	for _, entry := range c.interpolated[locale] {
		output = entry.expression.ReplaceAllStringFunc(output, func(match string) string {
			parts := entry.expression.FindStringSubmatch(match)
			if len(parts) != len(entry.fields)+1 {
				return match
			}
			translated := entry.translation
			for i, field := range entry.fields {
				fieldPattern := regexp.MustCompile(
					regexp.QuoteMeta("%("+field+")") + `[#0 +\-]?(?:\d+)?(?:\.\d+)?[a-zA-Z]`,
				)
				translated = fieldPattern.ReplaceAllStringFunc(translated, func(string) string {
					return parts[i+1]
				})
			}
			return translated
		})
	}
	for _, entry := range c.markup[locale] {
		output = strings.ReplaceAll(output, entry.msgid, entry.msgstr)
	}
	return []byte(output)
}

func translateText(text, locale string, catalog *Catalog) string {
	trimmed := strings.TrimSpace(text)
	if trimmed == "" {
		return text
	}
	translated := catalog.Translate(locale, trimmed)
	if translated == trimmed {
		return text
	}
	start := strings.Index(text, trimmed)
	return text[:start] + translated + text[start+len(trimmed):]
}

func parsePO(data []byte) map[string]string {
	messages := make(map[string]string)
	scanner := bufio.NewScanner(bytes.NewReader(data))
	var msgid, msgstr strings.Builder
	field := ""
	fuzzy := false

	flush := func() {
		if !fuzzy && msgid.Len() > 0 && msgstr.Len() > 0 {
			messages[msgid.String()] = msgstr.String()
		}
		msgid.Reset()
		msgstr.Reset()
		field = ""
		fuzzy = false
	}

	for scanner.Scan() {
		line := scanner.Text()
		if line == "" {
			flush()
			continue
		}
		if strings.HasPrefix(line, "#,") && strings.Contains(line, "fuzzy") {
			fuzzy = true
			continue
		}
		switch {
		case strings.HasPrefix(line, "msgid "):
			field = "msgid"
			appendPOString(&msgid, strings.TrimPrefix(line, "msgid "))
		case strings.HasPrefix(line, "msgstr "):
			field = "msgstr"
			appendPOString(&msgstr, strings.TrimPrefix(line, "msgstr "))
		case strings.HasPrefix(line, "\""):
			if field == "msgid" {
				appendPOString(&msgid, line)
			} else if field == "msgstr" {
				appendPOString(&msgstr, line)
			}
		}
	}
	flush()
	return messages
}

func appendPOString(builder *strings.Builder, quoted string) {
	value, err := strconv.Unquote(quoted)
	if err == nil {
		builder.WriteString(value)
	}
}
