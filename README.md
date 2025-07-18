# GTKDock

GTKDock is a GTKmm4 based Dock that tries to be WM agnostic inspired by [nwg-dock](https://github.com/nwg-piotr/nwg-dock-hyprland).

It is very much still in development.
Right now it works best on hyprland, it works on X11 but animations are choppy.

If WM is wayland it has to support gtk-layer-shell

<p align="center">
  <img width="1920" height="1080" src="https://private-user-images.githubusercontent.com/60853263/466617425-412293b9-2cc6-4e4b-afbe-db8c36df097e.png?jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3NTI3ODAzOTgsIm5iZiI6MTc1Mjc4MDA5OCwicGF0aCI6Ii82MDg1MzI2My80NjY2MTc0MjUtNDEyMjkzYjktMmNjNi00ZTRiLWFmYmUtZGI4YzM2ZGYwOTdlLnBuZz9YLUFtei1BbGdvcml0aG09QVdTNC1ITUFDLVNIQTI1NiZYLUFtei1DcmVkZW50aWFsPUFLSUFWQ09EWUxTQTUzUFFLNFpBJTJGMjAyNTA3MTclMkZ1cy1lYXN0LTElMkZzMyUyRmF3czRfcmVxdWVzdCZYLUFtei1EYXRlPTIwMjUwNzE3VDE5MjEzOFomWC1BbXotRXhwaXJlcz0zMDAmWC1BbXotU2lnbmF0dXJlPWVlZWIyMzQyMjc0ZjY3MjM4Y2VjN2Q4YzM1OGZiZmM5MGY1OGQ2YWQ1OWE5NWVjNGUzZGQxM2YyMjBlMGMwYTMmWC1BbXotU2lnbmVkSGVhZGVycz1ob3N0In0.qUCPL1tKN2P-x5wBBBMKEkMzj1Vi0AxSnpWvQjyVMzU">
</p>

<p align="center">
  <img width="623" height="312" src="https://github.com/user-attachments/assets/5b1ddcb1-9d7f-442f-a886-23d14b7b89da">
</p>

## Dependencies:

Compiletime: `X11(Xlib) GLM GTKmm4 gtk-layer-shell`

Runtime: `X11: wmctrl xdotool xprop  Wayland: hyprctl (for hyprland)`

## Building and Installing:

Build with make: `make clean && make -j $(nproc --ignore=2)`

Installation: `ln -s $(pwd)/GTKDock $HOME/.local/bin`\
Or (if ~/.config/GTKDock exists): `mv ./GTKDock ~/.local/bin/`

## Usage:

`GTKDock -d[monIdx] -e[edgeIdx] (Dock Edge Possible values: 0 = left 1 = top 2 = right 3 = bottom)`

conf/pinnedApps stores the pinned Apps and their order in the Format

`name:execCmd:iconPath:desptopFilePath`

you can add reorder the lines to change the ordering of pinned Apps in the Dock
futher configuration is available in `conf/settings.conf`

There are two ways of using GTKDock either leaving it in its Project Dir and linking to it\
or moving config and imgs folders to ~/.config/GTKDock so one can use the executable anywhere

## Example:

`./GTKDock -d1 -e3`

## WM Support and Compatibility
GTKDock has been tested on Hyprland and GNOME on Xorg

to add support to other WM's you'd need to
1. add functionlity for function in `wm-specific-impl.cpp`
2. extend list_windows.bash to work for your WM (if it doesn't)

### WM specific File: list_windows.bash

Bash script that queries the wm for running applications\
Returned format should be:

`monitorIdx-:-specificWindowTitle-:-windowClass-:-isFullscreen (0 or 1)-:-PID`

most x11 WMs that support wmctrl xprop and xrandr are implemented as well as hyprland\
each line is directly tied to an AppInstance in GTKDock


### WM specific File: wm-specific.h

defines the wm-specific functions these function are then implemented in wm-specific-impl.cpp

#### `void onrealizeXDock(Gtk::Window * win, int dispIdx, int winW, int winH, int edgeMargin, DockEdge edge)`

Is run on X11 to make window have no decorations be always on top and get ignored by the WM (CWOverrideRedirect)\
It also moves the window to its correct position


If any of those doesn't happen for you this is likely the cause.

#### `void GLS_setup_top_layer(Gtk::Window * win, int dispIdx, int edgeMargin, const std::string& name, DockEdge edge)`

Is run on wayland to make window appear in the TOP layer and be anchored to a display edge

#### `void GLS_chngMargin(Gtk::Window * win, int newMargin, DockEdge edge)`

Chganges the distance between the window edge and the display edge on wayland

#### `void openInstance(AppInstance i)`

makes WM get the instance i.e. focus on it / bring it back from minimized status ...

#### `void closeInstance(std::vector<AppInstance> instances)`

makes WM close the instance(window)

#### `bool check_layer_shell_support()`

checks if gtk-layer-shell protocol is supported

#### `void populateInstanceMenuWithWMSpecific(Gtk::Box* popover_box, AppInstance inst)`

adds buttons to the context menu of an instance that can do arbitrary functionality

for example toggle floating on hyprland

## Cutomization
imgs/launcher.png is used for the launcher button and conf/style.css is used to style all the elements in the Dock.\
conf/settings.conf controlls:
1. icon size
2. padding
3. hotspot height i. e. region that activates the dock
4. auto hide toimeout (time to wait before hiding dick) and duration (of animation)
5. to draw launcher btn or not
6. margin from screen edge
7. cmd to be executed when launcher btn is pressed
8. if dock is isolated to the apps running in its monitor or whether it should show windows on all screens

You might want to use GTK_DEBUG=interactive to help with customization :)
