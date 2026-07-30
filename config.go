package main

import (
	_ "github.com/BurntSushi/toml"
)

type MainConfig struct {
	Title  string
	Url    string
	Output string
}
type Config struct {
	Main MainConfig
}
