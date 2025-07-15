# GTKDock

GTKDock is a GTKmm4 based Dock that tries to be WM agnostic.

It is very much still in development.
Right now it works best on wayland.

If WM is wayland it has to support gtk-layer-shell

# Building:

`make clean && make -j $(nproc --ignore=2)`

# Usage:

`./GTKDock -d(monitor Index)`

# Example:

`./GTKDock -d1`


