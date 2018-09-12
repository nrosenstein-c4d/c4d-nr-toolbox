# c4d-nr-toolbox

This is a collection of Cinema 4D C++ plugins, most of which are prototypes
that have not been released before. The collection also includes the
**TimeHide** and **SafeFrame** plugins which I used to sell on my website.

I chose to open source all of it as I'm no longer planning on a commercial
release on any of these.

> __Disclaimer__: Most of this is really, really old code! I've made an effort
> to make it available for R20. If something isn't working or if the plugin
> destroys your PC, car and house, it may be my fault, but you can not hold me
> accountable for it!

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

You need the [Craftr 4][Craftr] build system. Clone the repository
recursively and build.

[Craftr]: https://github.com/craftr-build/craftr

    cd /path/to/Cinema4D/plugins
    git clone https://github.com/NiklasRosenstein/c4d-nr-toolbox.git
    cd c4d-nr-toolbox
    craftr -cb --variant=release

## Third party libraries

* libwebp
* dlib

Check the [LICENSE.txt](LICENSE.txt) for license information.

---

<p align="center">Copyright &copy; 2018 Niklas Rosenstein</p>
