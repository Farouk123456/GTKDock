#!/bin/bash

#   Bash script to allow wm agnostic querying of running applications
#   the format of the returned text should be of the format:
#       monitorIdx-:-specificWindowTitle-:-windowClass-:-isFullscreen (0 or 1)-:-PID"

################
### Hyprland ###
################

if [ -n "$HYPRLAND_INSTANCE_SIGNATURE" ]; then
    hyprctl clients -j | jq -r '.[] | "\(.monitor)-:-\(.title)-:-\(.class)-:-\(.fullscreen)-:-\(.pid)"'
    exit 0
fi

######################################
## Add your WM implemenataion here: ##
######################################


echo "ERROR: Unable to find WM specific solution" >&2
exit 1
