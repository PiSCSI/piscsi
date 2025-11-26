// Command extract-translations extracts the English messages localized by the
// web renderer and writes a GNU gettext POT file.
package main

import (
	"flag"
	"fmt"
	"go/ast"
	"go/parser"
	"go/token"
	"html"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"unicode"

	xhtml "golang.org/x/net/html"
)

type message struct {
	id         string
	references map[string]struct{}
}

var (
	templateAction = regexp.MustCompile(`(?s){{.*?}}`)
	space          = regexp.MustCompile(`\s+`)
)

func main() {
	output := flag.String("output", "messages.pot", "POT file to create")
	root := flag.String("root", ".", "piscsi-web module directory")
	flag.Parse()

	messages := make(map[string]*message)
	if err := extractTemplates(*root, messages); err != nil {
		fatal(err)
	}
	if err := extractGoMessages(*root, messages); err != nil {
		fatal(err)
	}
	if err := writePOT(*output, messages); err != nil {
		fatal(err)
	}
}

func extractTemplates(root string, messages map[string]*message) error {
	names, err := filepath.Glob(filepath.Join(root, "web", "templates", "*.html"))
	if err != nil {
		return err
	}
	for _, name := range names {
		data, err := os.ReadFile(name)
		if err != nil {
			return err
		}
		relative, err := filepath.Rel(root, name)
		if err != nil {
			return err
		}
		tokenizer := xhtml.NewTokenizer(strings.NewReader(string(data)))
		line := 1
		skipText := false
		for {
			tokenType := tokenizer.Next()
			raw := string(tokenizer.Raw())
			tokenLine := line
			line += strings.Count(raw, "\n")
			switch tokenType {
			case xhtml.ErrorToken:
				if tokenizer.Err() == nil {
					return fmt.Errorf("tokenize %s", relative)
				}
				goto nextTemplate
			case xhtml.StartTagToken, xhtml.SelfClosingTagToken:
				token := tokenizer.Token()
				if token.Data == "script" || token.Data == "style" {
					skipText = true
				}
				inputType := ""
				for _, attribute := range token.Attr {
					if attribute.Key == "type" {
						inputType = attribute.Val
					}
				}
				for _, attribute := range token.Attr {
					if attribute.Key == "placeholder" || attribute.Key == "title" ||
						(attribute.Key == "value" && (inputType == "submit" || inputType == "button")) {
						addMessage(messages, cleanTemplateText(attribute.Val), relative, tokenLine)
					}
				}
			case xhtml.EndTagToken:
				token := tokenizer.Token()
				if token.Data == "script" || token.Data == "style" {
					skipText = false
				}
			case xhtml.TextToken:
				if !skipText {
					addMessage(messages, cleanTemplateText(tokenizer.Token().Data), relative, tokenLine)
				}
			}
		}
	nextTemplate:
	}
	return nil
}

func cleanTemplateText(value string) string {
	if strings.Contains(value, "<style") || strings.Contains(value, "</style>") {
		return ""
	}
	value = templateAction.ReplaceAllString(value, " ")
	value = html.UnescapeString(value)
	return strings.TrimSpace(space.ReplaceAllString(value, " "))
}

func extractGoMessages(root string, messages map[string]*message) error {
	names, err := filepath.Glob(filepath.Join(root, "internal", "server", "*.go"))
	if err != nil {
		return err
	}
	files := token.NewFileSet()
	for _, name := range names {
		if strings.HasSuffix(name, "_test.go") {
			continue
		}
		parsed, err := parser.ParseFile(files, name, nil, 0)
		if err != nil {
			return err
		}
		relative, err := filepath.Rel(root, name)
		if err != nil {
			return err
		}
		ast.Inspect(parsed, func(node ast.Node) bool {
			switch value := node.(type) {
			case *ast.KeyValueExpr:
				key, ok := value.Key.(*ast.Ident)
				if ok && key.Name == "Message" {
					if id, ok := messageExpression(value.Value); ok {
						addMessage(messages, id, relative, files.Position(value.Pos()).Line)
					}
				}
			case *ast.CallExpr:
				selector, ok := value.Fun.(*ast.SelectorExpr)
				if ok && selector.Sel.Name == "Translate" && len(value.Args) >= 2 {
					if id, ok := stringLiteral(value.Args[1]); ok {
						addMessage(messages, id, relative, files.Position(value.Pos()).Line)
					}
				}
			}
			return true
		})
	}
	return nil
}

func messageExpression(expression ast.Expr) (string, bool) {
	if value, ok := stringLiteral(expression); ok {
		return value, true
	}
	switch value := expression.(type) {
	case *ast.BinaryExpr:
		if value.Op != token.ADD {
			return "", false
		}
		left, leftOK := messagePart(value.X, 1)
		right, rightOK := messagePart(value.Y, 2)
		return left + right, leftOK || rightOK
	case *ast.CallExpr:
		selector, ok := value.Fun.(*ast.SelectorExpr)
		if !ok || selector.Sel.Name != "Sprintf" || len(value.Args) == 0 {
			return "", false
		}
		format, ok := stringLiteral(value.Args[0])
		if !ok {
			return "", false
		}
		return replaceFormatDirectives(format, value.Args[1:]), true
	}
	return "", false
}

func messagePart(expression ast.Expr, index int) (string, bool) {
	if value, ok := messageExpression(expression); ok {
		return value, true
	}
	return fmt.Sprintf("%%(value_%d)s", index), false
}

func replaceFormatDirectives(format string, arguments []ast.Expr) string {
	var output strings.Builder
	argument := 0
	for index := 0; index < len(format); {
		if format[index] != '%' || index+1 >= len(format) {
			output.WriteByte(format[index])
			index++
			continue
		}
		if format[index+1] == '%' {
			output.WriteByte('%')
			index += 2
			continue
		}
		end := index + 1
		for end < len(format) && !unicode.IsLetter(rune(format[end])) {
			end++
		}
		if end == len(format) {
			output.WriteString(format[index:])
			break
		}
		name := fmt.Sprintf("value_%d", argument+1)
		if argument < len(arguments) {
			name = expressionName(arguments[argument], name)
		}
		output.WriteString("%(" + name + ")s")
		argument++
		index = end + 1
	}
	return output.String()
}

func expressionName(expression ast.Expr, fallback string) string {
	switch value := expression.(type) {
	case *ast.Ident:
		return value.Name
	case *ast.SelectorExpr:
		return value.Sel.Name
	case *ast.CallExpr:
		if selector, ok := value.Fun.(*ast.SelectorExpr); ok {
			return selector.Sel.Name
		}
	}
	return fallback
}

func stringLiteral(expression ast.Expr) (string, bool) {
	literal, ok := expression.(*ast.BasicLit)
	if !ok || literal.Kind != token.STRING {
		return "", false
	}
	value, err := strconv.Unquote(literal.Value)
	return value, err == nil
}

func addMessage(messages map[string]*message, id, file string, line int) {
	if id == "" || id == ".json" || !strings.ContainsFunc(id, unicode.IsLetter) {
		return
	}
	entry, ok := messages[id]
	if !ok {
		entry = &message{id: id, references: make(map[string]struct{})}
		messages[id] = entry
	}
	entry.references[fmt.Sprintf("%s:%d", filepath.ToSlash(file), line)] = struct{}{}
}

func writePOT(name string, messages map[string]*message) error {
	entries := make([]*message, 0, len(messages))
	for _, entry := range messages {
		entries = append(entries, entry)
	}
	sort.Slice(entries, func(i, j int) bool { return entries[i].id < entries[j].id })

	var output strings.Builder
	output.WriteString(`msgid ""
msgstr ""
"Project-Id-Version: PiSCSI Web Interface\n"
"Report-Msgid-Bugs-To: https://github.com/PiSCSI/piscsi/issues\n"
"POT-Creation-Date: YEAR-MO-DA HO:MI+ZONE\n"
"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\n"
"Last-Translator: FULL NAME <EMAIL@ADDRESS>\n"
"Language-Team: LANGUAGE <LL@li.org>\n"
"Language: \n"
"MIME-Version: 1.0\n"
"Content-Type: text/plain; charset=UTF-8\n"
"Content-Transfer-Encoding: 8bit\n"

`)
	for _, entry := range entries {
		references := make([]string, 0, len(entry.references))
		for reference := range entry.references {
			references = append(references, reference)
		}
		sort.Strings(references)
		output.WriteString("#: " + strings.Join(references, " ") + "\n")
		if strings.Contains(entry.id, "%(") {
			output.WriteString("#, python-format\n")
		}
		output.WriteString("msgid " + strconv.Quote(entry.id) + "\n")
		output.WriteString("msgstr \"\"\n\n")
	}
	return os.WriteFile(name, []byte(output.String()), 0o644)
}

func fatal(err error) {
	fmt.Fprintln(os.Stderr, "Error:", err)
	os.Exit(1)
}
