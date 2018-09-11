# c4d-nr-toolbox

This is a collection of Cinema 4D C++ plugins, most of which are prototypes
that have not been released before.

The collection also includes the TimeHide and SafeFrame plugins which I used
to sell on my website.

## Download

Check the [Releases] page for prebuilt binaries.

[Releases]: https://github.com/NiklasRosenstein/c4d-nr-toolbox/releases

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
