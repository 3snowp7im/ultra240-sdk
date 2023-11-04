# ULTRA240 SDK

This SDK provides tools for compiling [Tiled](https://www.mapeditor.org) project
files into ULTRA240 binaries.

While I plan on eventually providing this software with an open source
license, it is currently UNLICENSED. You may use and modify it as you see fit,
however you may not distribute it in any form for any reason.

## Building

### Build dependencies

To install the build dependencies on Ubuntu:

```shell
$ sudo apt-get install build-essential libjsoncpp-dev libpng++-dev \
  librapidxml-dev
```

### Building a non-release version

There are currently no release versions of the code. To build from the current
working version, install the GNU autotools.

To install autotools on Ubuntu:

```shell
$ sudo apt-get install autoconf automake
```

Next, checkout the software and generate the `configure` script:

```shell
$ autoreconf --install
```

It is recommended to build the software outside the source directories:

```shell
$ mkdir build
$ cd build
$ ../configure
```

Finally, build and install the software:

```shell
$ make
$ sudo make install
```

## Utilities

### ultra-sdk-tileset

Compile a Tiled tileset file into an ULTRA240 binary.
See the [tileset readme](src/ultra-sdk-tileset/README.md) for specification.

### ultra-sdk-img

Converts a PNG image file to an RGBA BMP.
With an optional tileset file, will produce an image without the margin and
spacing of its input.

### ultra-sdk-sheet

Generate 16 x 16 tiled sprite sheet with every cell labeled with a tile id.

### ultra-sdk-world

Compile a Tiled world file into an ULTRA240 binary.
See the [world readme](src/ultra-sdk-world/README.md) for specification.
