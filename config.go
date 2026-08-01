package main

import (
	_ "github.com/BurntSushi/toml"
)

type MainConfig struct {
	Title    string
	Subtitle string
	Url      string
	Output   string
	Amount   int
}
type Config struct {
	Main MainConfig
}
