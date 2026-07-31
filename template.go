package main

import (
	"fmt"
	"html/template"
	"os"
	"path/filepath"
	"strings"
	"time"
)

type Site struct {
	Title string
}

type Template struct {
	Title string
	Date  string
	Body  string
	Site  Site
}

func (t *Template) render(file, header, footer, outpath, htmlContent string) error {

	funcMap := template.FuncMap{
		"dateFormat": func(format string, t time.Time) string { return t.Format(format) },
		"safeHTML":   func(s string) template.HTML { return template.HTML(s) },
		"urlize":     func(s string) string { return strings.ToLower(strings.ReplaceAll(s, " ", "-")) },
		"now":        func() time.Time { return time.Now() },
	}

	tmpl, err := template.New("").Funcs(funcMap).ParseFiles(file, header, footer)
	if err != nil {
		return fmt.Errorf("%w\n", err)
	}

	title, ok := p.Meta["title"].(string)
	if !ok {
		return fmt.Errorf("missing title in frontmatter")
	}

	var dateStr string
	switch d := p.Meta["date"].(type) {
	case string:
		dateStr = d
	case time.Time:
		dateStr = d.Format("2006-01-02")
	default:
		return fmt.Errorf("invalid date type in frontmatter")
	}

	data := Template{
		Title: title,
		Date:  dateStr,
		Body:  htmlContent,
		Site:  Site{Title: conf.Main.Title},
	}

	if err := os.MkdirAll(filepath.Dir(outpath), 0755); err != nil {
		return fmt.Errorf("%w", err)
	}
	outFile, err := os.Create(outpath)
	if err != nil {
		return fmt.Errorf("%w", err)
	}
	defer outFile.Close()

	return tmpl.ExecuteTemplate(outFile, filepath.Base(file), data)
}
