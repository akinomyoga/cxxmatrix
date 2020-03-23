# C++ Matrix in terminal

I wrote a simple terminal program of Matrix digital rain.
A part of the purpose of this program is to test the performance of terminal emulators.
Another purpose is just for fun.
Enjoy it with fast terminals (e.g., urxvt, alacritty, terminology, etc.)!

**Features**

- Hankaku kana characters as in the original film
- Ten levels of "green"s using terminal 256 color support
- Twinkling effects by adding random brightness fluctuations
- Diffused reflection effects by cell background colors

**Scenes**

The following scenes will be visited in turn.
If you want to see the Mandelbrot set, you need to hold on for a few minutes.

1. Number falls
2. Banner - Show command line arguments by shining characters
3. "The Matrix" rain - [Wikipedia](https://en.wikipedia.org/wiki/Matrix_digital_rain)
4. Conway's Game of Life - [Wikipedia](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life)
5. The Mandelbrot set - [Wikipedia](https://en.wikipedia.org/wiki/Mandelbrot_set)
6. (End scene) "The Matrix" rain

## Demo

- cxxmatrix in 80x28 - [Youtube](https://www.youtube.com/watch?v=DeKuT8txldc)
- The Mandelbrot set in 479x186 - [YouTube](https://www.youtube.com/watch?v=RtMy4ltebKw)
- Highlight scenes - See the animated GIF below

![Captures](https://raw.githubusercontent.com/wiki/akinomyoga/cxxmatrix/images/cxxmatrix-version01sA.gif)

## Usage

This program is provided under the [MIT License](LICENSE.md).

**Requirements**:

- git, C++17 compiler, GNU make, GNU awk
- UTF-8 support of the system
- a fast terminal with 256color and UTF-8 support

```console
$ git clone https://github.com/akinomyoga/cxxmatrix.git
$ cd cxxmatrix
$ make
$ ./cxxmatrix 'The Matrix' 'Reloaded'
```

**Options**

Check the help with the option `--help` for details:

```console
$ ./cxxmatrix --help
```

**Keyboard**

Quit: <kbd>C-c</kbd>; Suspend: <kbd>C-z</kbd>; Menu: <kbd>RET</kbd>, <kbd>C-m</kbd>

## Install

The default install prefix is `/usr/local`. `/usr/local/bin/cxxmatrix` and `/usr/local/share/man/man1/cxxmatrix.1.gz` will be created.

```console
$ sudo make install
```

To change the install prefix, please specify the make variable `PREFIX`:

```bash
# Example 1
sudo make PREFIX=/opt/cxxmatrix install

# Example 2
make PREFIX=~/.local install
```


# Similar programs

Related tags in GitHub

- [`#matrix-rain`](https://github.com/topics/matrix-rain)
- [`#matrix-digital-rain`](https://github.com/topics/matrix-digital-rain)

## The Matrix rains in terminals

- [abishekvashok/cmatrix](https://github.com/abishekvashok/cmatrix) - [Demo](https://github.com/abishekvashok/cmatrix#screencasts) in C (1124)
- [will8211/unimatrix](https://github.com/will8211/unimatrix) - [Demo](https://github.com/will8211/unimatrix#screenshots) in Python3 (1010)
- [M4444/TMatrix](https://github.com/M4444/TMatrix) - [Demo](https://github.com/M4444/TMatrix#how-it-looks) in C++ (202)
- [GeertJohan/gomatrix](https://github.com/GeertJohan/gomatrix) - [Youtube](https://www.youtube.com/watch?v=mUXFxSmZMis) in Go (188)
- [nojvek/matrix-rain](https://github.com/nojvek/matrix-rain) - [Demo](https://github.com/nojvek/matrix-rain#screenshots) in Node (58)
- [torch2424/wasm-matrix](https://github.com/torch2424/wasm-matrix) - [Demo](https://github.com/torch2424/wasm-matrix#wasm-matrix) in WASM (47)
- [levithomason/cmatrix](https://github.com/levithomason/cmatrix) - [Demo](https://github.com/levithomason/cmatrix#cmatrix) in C (44)
- [jsbueno/terminal_matrix](https://github.com/jsbueno/terminal_matrix) - [Demo](https://github.com/jsbueno/terminal_matrix#python-script-to-simulate-the-matrix-screensaver-effect-in-a-posix-terminal) in Python (19)
- [b166erobot/matrix](https://github.com/b166erobot/matrix) in Python3 (16)
- [amstrad/oh-my-matrix](https://github.com/amstrad/oh-my-matrix) - [Demo](https://github.com/amstrad/oh-my-matrix/blob/master/oh-my-matrix.gif) in Python (13)
- [txstc55/matrix_viewer](https://github.com/txstc55/matrix_viewer) - [Demo](https://github.com/txstc55/matrix_viewer#matrix-viewer) in C++ (11)
- [JaydenL33/cmatrix2.0](https://github.com/JaydenL33/cmatrix2.0) in C (3)
- [gurushida/matrixmirror](https://github.com/gurushida/matrixmirror) - [Demo](https://github.com/gurushida/matrixmirror#matrixmirror) in Objective-C (0)

## The Matrix rains in browsers

- [tidwall/digitalrain](https://github.com/tidwall/digitalrain) - [Demo](https://tidwall.com/digitalrain/) in HTML5 Canvas (314)
- [winterbe/github-matrix](https://github.com/winterbe/github-matrix) - [Demo](https://winterbe.com/projects/github-matrix/) in HTML5 Canvas (171)
- [emilyxxie/green_rain](https://github.com/emilyxxie/green_rain#matrix-digital-rain) - [Demo](http://xie-emily.com/generative_art/green_rain.html) in HTML5 Canvas (165)
- [neilcarpenter/Matrix-code-rain](https://github.com/neilcarpenter/Matrix-code-rain) - [Demo](http://neilcarpenter.com/demos/canvas/matrix/) in HTML5 Canvas (74)
- [Rezmason/matrix](https://github.com/Rezmason/matrix) - [Demo](https://rezmason.github.io/matrix/) in HTML5 Canvas (42)
- [syropian/HTML5-Matrix-Code-Rain](https://github.com/syropian/HTML5-Matrix-Code-Rain) - [Demo](https://codepen.io/syropian/pen/bLzAi) in HTML5 Canvas (29)
- [raphaklaus/matrix-fx](https://github.com/raphaklaus/matrix-fx) - [Demo](https://raphaklaus.com/matrix-fx/) in CSS3 (14)
- [lhartikk/BtcTxMatrix](https://github.com/lhartikk/BtcTxMatrix) - [Demo](http://lhartikk.github.io/btctxmatrix/) in HTML5 Canvas (13)
- [pazdera/matrix-vr](https://github.com/pazdera/matrix-vr) - [Demo](https://radek.io/matrix-vr/) in WebVR (10)
- [anderspitman/redpill](https://github.com/anderspitman/redpill) - [Demo](https://anderspitman.net/apps/redpill/) in HTML5 Canvas (6)
- [pmutua/Matrix-Rain](https://github.com/pmutua/Matrix-Rain) - [Demo](https://pmutua.github.io/Matrix-Rain/) in HTML5 Canvas (3)
- [zhaihaoran/digital-rain](https://github.com/zhaihaoran/digital-rain) - [Demo](https://zhaihaoran.github.io/digital-rain/) in HTML5 Canvas (1)
- [Workvictor/pixi-digital-rain](https://github.com/Workvictor/pixi-digital-rain) - [Demo](https://victorpunko.ru/development/digital-rain-v3/) in HTML5 Canvas (0)
- [azder/ES2017-Matrix-Rain](https://github.com/azder/ES2017-Matrix-Rain) - [Demo](https://azder.github.io/ES2017-Matrix-Rain/) in HTML5 Canvas (0)
- [codingotaku/7-Segment-Digital-Matrix-Rain](https://github.com/codingotaku/7-Segment-Digital-Matrix-Rain) - [Demo](https://codingotaku.com/7-Segment-Digital-Matrix-Rain/) in HTML5 Canvas (0)

## Others

- [winterbe/github-matrix-screensaver](https://github.com/winterbe/github-matrix-screensaver) (github-matrix/WebSaver?) in JavaScript (391)
- [tremby/Kaleidoscope-LEDEffect-DigitalRain](https://github.com/tremby/Kaleidoscope-LEDEffect-DigitalRain) in Kaleidoscope? (20)
- [nathanchere/MatrixSaver](https://github.com/nathanchere/MatrixSaver) in C# (18)
- [sapandang/Matrix-Rain-Live-Wallpaper](https://github.com/sapandang/Matrix-Rain-Live-Wallpaper) in Android Wallpaper (13)
- [artgl42/MatrixDigitalRain](https://github.com/artgl42/MatrixDigitalRain) - [Demo](https://github.com/artgl42/MatrixDigitalRain#matrixdigitalrain-dll) in C# (0)
- [Sullivan008/CSharp-MatrixRain](https://github.com/Sullivan008/CSharp-MatrixRain) in C# (0)
