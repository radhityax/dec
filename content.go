package main

import (
	"bufio"
	"bytes"
	"fmt"
	"github.com/BurntSushi/toml"
	"github.com/yuin/goldmark"
	"github.com/yuin/goldmark/renderer/html"
	"log"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

type Page struct {
	Meta     map[string]any // title, date, tags, draft
	Content  string
	RawBody  string
	URL      string
	FilePath string
}

func (p *Page) parse(filecontent string) error {
	file, err := os.Open(filecontent)
	if err != nil {
		log.Fatalf("failed to open file: %v", err)
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)

	if !scanner.Scan() {
		fmt.Println("file is empty or gone?")
		return nil
	}

	delimiter := strings.TrimSpace(scanner.Text())

	var frontmatter strings.Builder
	// read frontmatter until the second delimiter :)
	for scanner.Scan() {
		line := scanner.Text()
		if strings.TrimSpace(line) == delimiter {
			break
		}
		frontmatter.WriteString(line + "\n")
	}

	if frontmatter.Len() > 0 {
		if _, err := toml.Decode(frontmatter.String(), &p.Meta); err != nil {
			log.Fatalf("failed to parse frontmatter: %v", err)
		}
	}

	var content strings.Builder
	for scanner.Scan() {
		content.WriteString(scanner.Text() + "\n")
	}

	p.Content = content.String()

	return nil
}

func (p *Page) WriteHTML(text string) (string, error) {
	md := goldmark.New(goldmark.WithRendererOptions(html.WithUnsafe()))
	var buf bytes.Buffer
	if err := md.Convert([]byte(text), &buf); err != nil {
		return "", err
	}

	return buf.String(), nil
}

func generateRSS(pages []Page) error {
	var posts []Page
	for _, page := range pages {
		relPath, _ := filepath.Rel("content", page.FilePath)
		if strings.HasPrefix(relPath, "blog/") && filepath.Base(page.FilePath) != "index.md" {
			posts = append(posts, page)
		}
	}
	sort.Slice(posts, func(i, j int) bool {
		di, _ := posts[i].Meta["date"].(time.Time)
		dj, _ := posts[j].Meta["date"].(time.Time)
		return di.After(dj)
	})

	if len(posts) > 20 {
		posts = posts[:20]
	}
	var buf bytes.Buffer
	buf.WriteString(`<?xml version="1.0" encoding="UTF-8"?>`)
	buf.WriteString(`<rss version="2.0"><channel>`)
	buf.WriteString(fmt.Sprintf(`<title>%s</title>`, conf.Main.Title))
	buf.WriteString(fmt.Sprintf(`<link>%s</link>`, conf.Main.Url))
	buf.WriteString(`<description>Recent posts</description>`)

	for _, post := range posts {
		title, _ := post.Meta["title"].(string)
		date, _ := post.Meta["date"].(time.Time)
		basename := strings.TrimSuffix(filepath.Base(post.FilePath), ".md")
		url := fmt.Sprintf("%s/%s/", conf.Main.Url, basename)

		buf.WriteString(`<item>`)
		buf.WriteString(fmt.Sprintf(`<title>%s</title>`, title))
		buf.WriteString(fmt.Sprintf(`<link>%s</link>`, url))
		buf.WriteString(fmt.Sprintf(`<pubDate>%s</pubDate>`, date.Format(time.RFC1123Z)))
		buf.WriteString(fmt.Sprintf(`<guid>%s</guid>`, url))
		buf.WriteString(`</item>`)
	}

	buf.WriteString(`</channel></rss>`)

	outpath := filepath.Join(conf.Main.Output, "index.xml")
	return os.WriteFile(outpath, buf.Bytes(), 0644)
}
