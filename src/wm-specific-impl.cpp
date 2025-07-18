#include "wm-specific.h"
#include <gtk4-layer-shell.h>
#include <gtkmm-4.0/gtkmm.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/x11/gdkx.h>

bool check_layer_shell_support()
{
    auto window = Gtk::make_managed<Gtk::Window>();
    gtk_layer_init_for_window(GTK_WINDOW(window->gobj()));
    bool supported = gtk_layer_is_supported();
    return supported;
}

void onrealizeXDock(Gtk::Window * win, int dispIdx, int winW, int winH, int edgeMargin, DockEdge edge, DockAlignment alignment)
{
    Display * disp = XOpenDisplay(0);
    unsigned long x_window = gdk_x11_surface_get_xid(GDK_SURFACE(win->get_surface()->gobj()));

    struct {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
    } hints;
    
    hints.flags = (1 << 1); // MWM_HINTS_DECORATIONS
    hints.decorations = 0;  // 0 means no decorations

    Atom property = XInternAtom(disp, "_MOTIF_WM_HINTS", False);
    XChangeProperty(disp, x_window, property, property, 32, PropModeReplace, (unsigned char*)&hints, 5);
    XSetWindowAttributes swa;
    swa.override_redirect = True;

    XChangeWindowAttributes(disp, x_window, CWOverrideRedirect, &swa);

    GdkMonitor * monitor = GDK_MONITOR((Gdk::Display::get_default()->get_monitors()->get_object(dispIdx))->gobj());
    
    GdkRectangle g;
    gdk_monitor_get_geometry(monitor, &g);
    int x, y = 0;

    if (edge == DockEdge::EDGELEFT)
    {
        x = g.x + edgeMargin;
        
        if (alignment == DockAlignment::CENTER)
            y = g.y + (g.height - winH) / 2;
        else if (alignment == DockAlignment::TOP)
            y = g.y;
        else if (alignment == DockAlignment::BOTTOM)
            y = g.y + (g.height - winH);
    }
    else if (edge == DockEdge::EDGETOP)
    {
        y = g.y + edgeMargin;

        if (alignment == DockAlignment::CENTER)
            x = g.x + (g.width - winW) / 2;
        else if (alignment == DockAlignment::LEFT)
            x = g.x;
        else if (alignment == DockAlignment::RIGHT)
            x = g.x + (g.width - winW);
    }
    else if (edge == DockEdge::EDGERIGHT)
    {
        x = g.x + g.width - edgeMargin - winW;
        
        if (alignment == DockAlignment::CENTER)
            y = g.y + (g.height - winH) / 2;
        else if (alignment == DockAlignment::TOP)
            y = g.y;
        else if (alignment == DockAlignment::BOTTOM)
            y = g.y + (g.height - winH);
    }
    else if (edge == DockEdge::EDGEBOTTOM)
    {
        y = g.y + g.height - winH - edgeMargin;

        if (alignment == DockAlignment::CENTER)
            x = g.x + (g.width - winW) / 2;
        else if (alignment == DockAlignment::LEFT)
            x = g.x;
        else if (alignment == DockAlignment::RIGHT)
            x = g.x + (g.width - winW);
    }

    XMoveWindow(disp, x_window, x,y);

    XFlush(disp);
}

void GLS_setup_top_layer(Gtk::Window * win, int dispIdx, int edgeMargin, const std::string& name, DockEdge edge, DockAlignment alignment)
{
    GtkLayerShellEdge ed = (edge == DockEdge::EDGELEFT) ? GTK_LAYER_SHELL_EDGE_LEFT : (edge == DockEdge::EDGETOP) ? GTK_LAYER_SHELL_EDGE_TOP : (edge == DockEdge::EDGERIGHT) ? GTK_LAYER_SHELL_EDGE_RIGHT : GTK_LAYER_SHELL_EDGE_BOTTOM;

    gtk_layer_init_for_window(GTK_WINDOW(win->gobj()));
    gtk_layer_set_anchor(GTK_WINDOW(win->gobj()), ed, true);

    if (alignment == DockAlignment::BOTTOM)
        gtk_layer_set_anchor(GTK_WINDOW(win->gobj()), GTK_LAYER_SHELL_EDGE_BOTTOM, true);
    else if (alignment == DockAlignment::LEFT)
        gtk_layer_set_anchor(GTK_WINDOW(win->gobj()), GTK_LAYER_SHELL_EDGE_LEFT, true);
    else if (alignment == DockAlignment::RIGHT)
        gtk_layer_set_anchor(GTK_WINDOW(win->gobj()), GTK_LAYER_SHELL_EDGE_RIGHT, true);
    else if (alignment == DockAlignment::TOP)
        gtk_layer_set_anchor(GTK_WINDOW(win->gobj()), GTK_LAYER_SHELL_EDGE_TOP, true);

    gtk_layer_set_layer(GTK_WINDOW(win->gobj()), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_namespace(GTK_WINDOW(win->gobj()), name.c_str());
    gtk_layer_set_margin(GTK_WINDOW(win->gobj()), ed, edgeMargin);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(win->gobj()), -1);
    gtk_layer_set_monitor(GTK_WINDOW(win->gobj()), GDK_MONITOR(Gdk::Display::get_default()->get_monitors()->get_object(dispIdx)->gobj()));
}

void GLS_chngMargin(Gtk::Window * win, int newMargin, DockEdge edge)
{
    GtkLayerShellEdge ed = (edge == DockEdge::EDGELEFT) ? GTK_LAYER_SHELL_EDGE_LEFT : (edge == DockEdge::EDGETOP) ? GTK_LAYER_SHELL_EDGE_TOP : (edge == DockEdge::EDGERIGHT) ? GTK_LAYER_SHELL_EDGE_RIGHT : GTK_LAYER_SHELL_EDGE_BOTTOM;
    gtk_layer_set_margin(GTK_WINDOW(win->gobj()), ed, newMargin);
}

void openInstance(AppInstance i)
{
    if (std::getenv("HYPRLAND_INSTANCE_SIGNATURE") != NULL)
    {
        std::system(((std::string)"ADDRESS=$(hyprctl -j clients | jq -r '.[] | select(.title == \"" + i.title + "\") | .address') && hyprctl dispatch focuswindow \"address:$ADDRESS\"").c_str());
    } else if (!(strcmp(std::getenv("XDG_SESSION_TYPE"), "wayland") == 0))
    {
        std::system(("wmctrl -a \"" + i.title + "\"").c_str());
    }
}

void closeInstance(std::vector<AppInstance> instances)
{
    if (std::getenv("HYPRLAND_INSTANCE_SIGNATURE") != NULL)
    {
        if (instances.size() > 1)
        {
            std::system(((std::string)"for addr in $(hyprctl -j clients | jq -r '.[] | select(.class == \"" + instances[0].wclass + "\").address'); do hyprctl dispatch closewindow class:\"^(" + instances[0].wclass + ")$\" && sleep 0.01s; done").c_str());
        } else
        {
            std::system(((std::string)"hyprctl dispatch closewindow title:\"^(" + instances[0].title + ")$\" && sleep 0.01s").c_str());
        }
        return;
    } else if (!(strcmp(std::getenv("XDG_SESSION_TYPE"), "wayland") == 0))
    {
        for (AppInstance& i : instances)
        {
            std::system(((std::string)"xdotool search --name \"" + i.title + "\" windowclose && sleep 0.01s").c_str());
        }
    }
}

void populateInstanceMenuWithWMSpecific(Gtk::Box* popover_box, AppInstance inst)
{
    if (std::getenv("HYPRLAND_INSTANCE_SIGNATURE") != NULL)
    {
        auto button2 = Gtk::make_managed<Gtk::Button>("Toggle Floating");
        
        button2->signal_clicked().connect([inst](){
            std::system(("hyprctl dispatch togglefloating \"title:" + inst.title + "\" &").c_str());
        });

        button2->add_css_class("mbutton");
        popover_box->append(*button2);
    } else if(!(strcmp(std::getenv("XDG_SESSION_TYPE"), "wayland") == 0))
    {
        auto button2 = Gtk::make_managed<Gtk::Button>(inst.fullscreen ? "Minimize" : "Maximize");
        
        button2->signal_clicked().connect([inst](){
            if (inst.fullscreen) std::system(("wmctrl -r \"" + inst.title + "\" -b add,maximized_vert,maximized_horz &").c_str());
            else std::system(("wmctrl -r \"" + inst.title + "\" -b add,hidden &").c_str());
        });

        button2->add_css_class("mbutton");
        popover_box->append(*button2);
    }
}
      