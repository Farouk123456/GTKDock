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

#####################
### Most X11 WM's ###
#####################

if [ "$XDG_SESSION_TYPE" = "x11" ]; then
    # Get monitor information
    mapfile -t monitors < <(xrandr --listmonitors | tail -n +2 | awk '{print $1 $4}')

    # List windows
    wmctrl -lpG | while read -r win_id desk pid x y w h host title; do
        # Get WM_CLASS
        wm_class=$(xprop -id "$win_id" WM_CLASS | cut -d '"' -f2)

        # Get fullscreen status
        fullscreen=0
        if xprop -id "$win_id" _NET_WM_STATE | grep -q '_NET_WM_STATE_FULLSCREEN'; then
            fullscreen=1
        fi

        # Get monitor index (fallback based on window position)
        monitor_idx=0
        for i in "${!monitors[@]}"; do
            geometry=${monitors[$i]#*:}
            IFS='+' read -r res x_off y_off <<< "$geometry"
            IFS='x' read -r width height <<< "$res"
            if (( x >= x_off && x < x_off + width && y >= y_off && y < y_off + height )); then
                monitor_idx=$i
                break
            fi
        done

        echo "$monitor_idx-:-$title-:-$wm_class-:-$fullscreen-:-$pid"
    done

    exit 0
fi

######################################
## Add your WM implemenataion here: ##
######################################


echo "ERROR: Unable to find WM specific solution" >&2
exit 1
