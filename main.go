package main

import (
	"flag"
	"fmt"
	"github.com/BurntSushi/toml"
	"io/fs"
	"log"
	"os"
	"path/filepath"
)

var conf Config
var p Page

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
	_ = filepath.WalkDir("content", func(path string, d fs.DirEntry, err error) error {
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
			return nil
		}

		if err := p.WriteHTML(p.Content); err != nil {
			log.Printf("%s: %v", path, err)
		}
		return nil
	})
}
