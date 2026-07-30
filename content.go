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
	"strings"
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

func (p *Page) WriteHTML(text string) error {
	basename := strings.TrimSuffix(filepath.Base(p.FilePath), ".md")
	outpath := filepath.Join(conf.Main.Output, basename, "index.html")
	if err := os.MkdirAll(filepath.Dir(outpath), 0755); err != nil {
		return fmt.Errorf("failed to create dir: %w", err)
	}

	md := goldmark.New(goldmark.WithRendererOptions(html.WithUnsafe()))
	var buf bytes.Buffer
	if err := md.Convert([]byte(text), &buf); err != nil {
		return fmt.Errorf("%w", err)
	}

	if err := os.WriteFile(outpath, buf.Bytes(), 0644); err != nil {
		return fmt.Errorf("%w", err)
	}

	return nil
}
