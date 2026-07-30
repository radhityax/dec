package main

import (
	"flag"
	"fmt"
	"github.com/BurntSushi/toml"
	"os"
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

		fmt.Printf("Draft: %v\nConfig: %s\n", *draft, *configPath)
		fmt.Println("building site...")
	default:
		fmt.Fprintln(os.Stderr, "usage: dec <command> [flags]")
		os.Exit(1)
	}

	fmt.Printf("title: %s\nurl: %s\noutput: %s\n", conf.Main.Title, conf.Main.Url, conf.Main.Output)

	p.parse("content/sample_hugo.md")
	fmt.Printf("%s\n", p.Meta)
	fmt.Printf("%s\n", p.Content)
}
