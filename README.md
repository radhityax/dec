dec
====

simple static site generator inspired by [werc](https://cat-v.org) 
and written in Go. it has plain web server to see the result.

build
-----

make sure you have installed the libraries. then run the makefile.

```
$ make
$ sudo make install
```

usage
-----

```
$ dec build
```

```
$ dec help
```

template
--------

Please look into the "layouts" directory.

format
-------

i follow the hugo-style post in toml format

```
+++
title = "foo"
date = "2006-01-02"
+++

hello world!
```

library
-------

dec would not exist without these libraries. thank you for the hardwork.
- burntsushi/toml
- yuin/goldmark
