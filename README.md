# XTMON

An EWMH compliant X window title monitoring tool.

## Installation

1. Install `libxcb` with your package manager (prerequisite)
2. Download / clone this repository
3. Run `make`
4. Run `make install`

## Usage

There are no command line arguments to xtmon, so you just have to run `xtmon` in
your shell.

```
$ xtmon

initial_title 0x02e00007 Alacritty
initial_title 0x01200046 Mozilla Firefox
initial_focus 0x01200046 Mozilla Firefox
title_changed 0x01200046 ✔️ ❤️ ★ Unicode® Character Table - Mozilla Firefox
focus_changed 0x02e00007 Alacritty
new_window 0x03e00006 feh [1 of 10] - /tmp/images/0001.jpg
focus_changed 0x03e00006 feh [1 of 10] - /tmp/images/0001.jpg
title_changed 0x03e00006 feh [2 of 10] - /tmp/images/0002.jpg
title_changed 0x03e00006 feh [3 of 10] - /tmp/images/0003.jpg
removed_window 0x03e00006 
focus_changed 0x01200046 ✔️ ❤️ ★ Unicode® Character Table - Mozilla Firefox
```

## Contributing

Anyone is welcome to contribute to `xtmon`, take a look at
[CONTRIBUTING.md](CONTRIBUTING.md) for more details.
