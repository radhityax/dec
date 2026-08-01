package main

import (
	"fmt"
	"log"
	"net/http"
)

var portDesired string = "1204"

func live() {
	folder := conf.Main.Output
	fileserver := http.FileServer(http.Dir(folder))
	port := ":" + portDesired
	fmt.Println("-> ", port)
	http.Handle("/", fileserver)

	err := http.ListenAndServe(port, nil)
	if err != nil {
		log.Fatal(err)
	}
}
