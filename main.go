package main

import (
	"flag"
	"fmt"
	"github.com/BurntSushi/toml"
	"html/template"
	"io/fs"
	"log"
	"math"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

var conf Config
var p Page
var t Template

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: dec <command> [flags]")
		os.Exit(1)
	}

	switch os.Args[1] {
	case "build":
		buildCmd := flag.NewFlagSet("build", flag.ExitOnError)
		draft := buildCmd.Bool("draft", false, "")
		configPath := buildCmd.String("config", "", "config file")
		buildCmd.Parse(os.Args[2:])

		path := "dec.toml"
		if *configPath != "" {
			path = *configPath
		}

		_, err := toml.DecodeFile(path, &conf)
		if err != nil {
			os.Exit(1)
		}

		if conf.Main.Amount <= 0 {
			conf.Main.Amount = 10
		}

		pages := p.build(*draft)
		indexBuild(pages)

		fmt.Println("building site...")

	default:
		fmt.Fprintln(os.Stderr, "usage: dec <command> [flags]")
		os.Exit(1)
	}
}

func (p *Page) build(draft bool) []Page {
	os.RemoveAll(conf.Main.Output)
	os.MkdirAll(conf.Main.Output, 0755)
	if err := os.CopyFS(conf.Main.Output, os.DirFS("static")); err != nil {
		log.Printf("failed to copy static files: %v", err)
	}

	var pages []Page

	err := filepath.WalkDir("content", func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return nil
		}
		if d.IsDir() || filepath.Ext(path) != ".md" {
			return nil
		}

		var page Page
		page.FilePath = path
		if err := page.parse(path); err != nil {
			log.Printf("skip %s: %v", path, err)
			return nil
		}

		if isDraft, ok := page.Meta["draft"].(bool); ok && isDraft && !draft {
			return nil
		}
		basename := strings.TrimSuffix(filepath.Base(page.FilePath), ".md")
		var outpath string
		if basename == "index" {
			outpath = filepath.Join(conf.Main.Output, "index.html")
		} else {
			outpath = filepath.Join(conf.Main.Output, basename, "index.html")
		}

		htmlContent, err := page.WriteHTML(page.Content)
		if err != nil {
			log.Printf("%s: %v", path, err)
			return nil
		}

		if err := t.render("layouts/single.html", "layouts/header.html",
			"layouts/footer.html", outpath, htmlContent, &page); err != nil {
			log.Printf("render error %s: %v", path, err)
		}
		pages = append(pages, page)
		return nil
	})
	if err != nil {
		log.Printf("%v", err)
	}
	return pages
}

func indexBuild(pages []Page) error {
	sort.Slice(pages, func(i, j int) bool {
		di, oki := pages[i].Meta["date"].(time.Time)
		dj, okj := pages[j].Meta["date"].(time.Time)
		if !oki || !okj {
			return false
		}
		return di.After(dj)
	})

	var posts []Post

	for _, page := range pages {
		if filepath.Base(page.FilePath) == "index.md" {
			continue
		}

		title, _ := page.Meta["title"].(string)
		date, _ := page.Meta["date"].(time.Time)

		basename := strings.TrimSuffix(filepath.Base(page.FilePath), ".md")
		url := "/" + basename + "/"

		posts = append(posts, Post{
			Title: title,
			Date:  date.Format("2006-01-02"),
			URL:   url,
		})
	}

	amount := conf.Main.Amount
	totalPosts := len(posts)
	totalPages := int(math.Ceil(float64(totalPosts) / float64(amount)))
	if totalPages == 0 {
		totalPages = 1
	}
	funcMap := template.FuncMap{
		"dateFormat": func(format string, t time.Time) string { return t.Format(format) },
		"now":        func() time.Time { return time.Now() },
	}

	tmpl, err := template.New("blog.html").Funcs(funcMap).ParseFiles("layouts/blog.html")
	if err != nil {
		return fmt.Errorf("failed to parse template: %w", err)
	}

	for page := 1; page <= totalPages; page++ {
		start := (page - 1) * amount
		end := start + amount
		if end > totalPosts {
			end = totalPosts
		}

		pagePosts := posts[start:end]

		pagination := Pagination{
			CurrentPage: page,
			TotalPages:  totalPages,
			HasPrev:     page > 1,
			HasNext:     page < totalPages,
		}

		if pagination.HasPrev {
			if page == 2 {
				pagination.PrevURL = "/blog/"
			} else {
				pagination.PrevURL = fmt.Sprintf("/blog/%d/", page-1)
			}
		}

		if pagination.HasNext {
			pagination.NextURL = fmt.Sprintf("/blog/%d/", page+1)
		}

		data := IndexData{
			Site:       Site{Title: conf.Main.Title},
			Posts:      pagePosts,
			Pagination: pagination,
		}

		var outpath string
		if page == 1 {
			outpath = filepath.Join(conf.Main.Output, "blog", "index.html")
		} else {
			outpath = filepath.Join(conf.Main.Output, "blog", fmt.Sprintf("%d", page), "index.html")
		}

		if err := os.MkdirAll(filepath.Dir(outpath), 0755); err != nil {
			return fmt.Errorf("failed to create dir: %w", err)
		}

		outFile, err := os.Create(outpath)
		if err != nil {
			return fmt.Errorf("failed to create output file: %w", err)
		}

		if err := tmpl.Execute(outFile, data); err != nil {
			outFile.Close()
			return fmt.Errorf("failed to execute template: %w", err)
		}
		outFile.Close()
	}

	return nil
}
