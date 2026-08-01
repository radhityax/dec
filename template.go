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
	Title    string
	Subtitle string
}

type Template struct {
	Title string
	Date  string
	Body  string
	Site  Site
}

type IndexData struct {
	Site       Site
	Posts      []Post
	Pagination Pagination
}

type Post struct {
	Title string
	Date  string
	URL   string
}

type Pagination struct {
	CurrentPage int
	TotalPages  int
	HasPrev     bool
	HasNext     bool
	PrevURL     string
	NextURL     string
}

func (t *Template) render(file, header, footer, outpath, htmlContent string, page *Page) error {

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

	title, ok := page.Meta["title"].(string)
	if !ok {
		return fmt.Errorf("missing title in frontmatter")
	}

	var dateStr string
	switch d := page.Meta["date"].(type) {
	case string:
		dateStr = d
	case time.Time:
		dateStr = d.Format("2006-01-02")
	case nil:
		dateStr = ""
	default:
		return fmt.Errorf("invalid date type in frontmatter")
	}

	data := Template{
		Title: title,
		Date:  dateStr,
		Body:  htmlContent,
		Site:  Site{Title: conf.Main.Title, Subtitle: conf.Main.Subtitle},
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
