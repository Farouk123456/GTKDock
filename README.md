# GTKDock

GTKDock is a GTKmm4 based Dock that tries to be WM agnostic inspired by [nwg-dock](https://github.com/nwg-piotr/nwg-dock-hyprland).

It is very much still in development.
Right now it works best on hyprland, it works on X11 but animations are choppy.

If WM is wayland it has to support gtk-layer-shell

# Dependencies:

Compiletime: `X11(Xlib) GLM GTKmm4 gtk-layer-shell`

Runtime: `X11: wmctrl xdotool xprop  Wayland: hyprctl (for hyprland)`

# Building:

`make clean && make -j $(nproc --ignore=2)`

# Usage:

`./GTKDock -d(monitor Index)`

conf/pinnedApps stores the pinned Apps and their order in the Format

`name:execCmd:iconPath:desptopFilePath`

you can add reorder the lines to change the ordering of pinned Apps in the Dock

# Example:

`./GTKDock -d1`

# WM Support and Compatibility
GTKDock has been tested on Hyprland and GNOME on Xorg

to add support to other WM's you'd need to
1. add functionlity for function in `wm-specific-impl.cpp`
2. extend list_windows.bash to work for your WM (if it doesn't)

# Cutomization
launcher.png is used for the launcher button and style.css is used to style all the elements in the Dock.

You might want to use GTK_DEBUG=interactive to help with customization :)
