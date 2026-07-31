package main

import (
	"flag"
	"fmt"
	"github.com/BurntSushi/toml"
	"io/fs"
	"log"
	"os"
	"path/filepath"
	"strings"
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

		p.build(*draft)
		fmt.Println("building site...")

	default:
		fmt.Fprintln(os.Stderr, "usage: dec <command> [flags]")
		os.Exit(1)
	}
}

func (p *Page) build(draft bool) {
	if err := os.CopyFS(conf.Main.Output, os.DirFS("static")); err != nil {
		log.Printf("failed to copy static files: %v", err)
	}

	err := filepath.WalkDir("content", func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return nil
		}
		if d.IsDir() || filepath.Ext(path) != ".md" {
			return nil
		}

		p.FilePath = path
		if err := p.parse(path); err != nil {
			log.Printf("skip %s: %v", path, err)
			return nil
		}

		if isDraft, ok := p.Meta["draft"].(bool); ok && isDraft && !draft {
			fmt.Println("ada draft")
			return nil
		}
		basename := strings.TrimSuffix(filepath.Base(p.FilePath), ".md")
		outpath := filepath.Join(conf.Main.Output, basename, "index.html")

		htmlContent, err := p.WriteHTML(p.Content)
		if err != nil {
			log.Printf("%s: %v", path, err)
			return nil
		}

		if err := t.render("layouts/single.html", "layouts/header.html",
			"layouts/footer.html", outpath, htmlContent); err != nil {
			log.Printf("render error %s: %v", path, err)
		}

		return nil
	})
	if err != nil {
		log.Printf("%v", err)
	}
}
