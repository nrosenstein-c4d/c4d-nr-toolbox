# c4d-nr-toolbox

This is a collection of Cinema 4D C++ plugins, most of which are prototypes
that have not been released before. The collection also includes the
**TimeHide** and **SafeFrame** plugins which I used to sell on my website.

I chose to open source all of it as I'm no longer planning on a commercial
release on any of these.

## Contributions are Welcome!

You don't have to code to contribute &ndash; donating icons would be pretty
cool, too!

## Download & Installation Instructions

Check the [Releases] page for prebuilt binaries. Unzip the appropriate ZIP
archive into your Cinema 4D plugins folder.

[Releases]: https://github.com/NiklasRosenstein/c4d-nr-toolbox/releases
[niklasrosenstein.com]: https://www.niklasrosenstein.com/

## Documentation

The documentation is a bit lacking -- but at least it should give you a rough
overview of what is available in the nr-toolbox.

[>> View Full Documentation](docs/README.md)

## Build Instructions

You need the [Craftr 4][Craftr] build system. On Windows, due to a problem
with [dlib, MSVC and the /vms compiler option](https://github.com/davisking/dlib/issues/1479),
you also need [LLVM for Windows](http://releases.llvm.org/download.html).

[Craftr]: https://github.com/craftr-build/craftr

    cd /path/to/Cinema4D/plugins
    git clone https://github.com/NiklasRosenstein/c4d-nr-toolbox.git --recursive
    cd c4d-nr-toolbox
    craftr -cb --variant=release

## Third party libraries

* libwebp
* dlib

Check the [LICENSE.txt](LICENSE.txt) for license information.

## Disclaimer

Most of this is really old code (dating back to 2013 with TeaPresso and
the Pr1mitive modules). I've decided to release all my prototypes and
previously commercial plugins as a collection and effort to make them
compatible with R20.

Note that this is free software, licensed under the MIT license. If something
isn't working, or even if the plugin destroys your PC, car or home, it may
be my fault, but you can not hold me liable.

---

<p align="center">Copyright &copy; 2018 Niklas Rosenstein</p>
