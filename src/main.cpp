#include <iostream>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <string>
#include <map>
#include <set>
#include <limits>
#include <algorithm>
#include <fstream>
#include <array>
#include <chrono>
#include <thread>
#include <filesystem>
#include <memory>
#include <gtkmm-4.0/gtkmm.h>
#include "utils.h"
#include "wm-specific.h"

/*
    wayland: bool checking if XDG_SESSION_TYPE is wayland
    current_instances: list off all instances i. e. wm managed windows / programs returned by list_windows.bash
    running: for second thread that updates dock entries that tracks running status
*/

bool wayland = false;
std::vector<AppInstance> current_instances = {};
std::atomic<bool> running(true);

/*
    getEntries returns a vector of all wm managed applications each entry has a vector instances(windows) that share the same class
    instances vector gets used to find desktop file which then fills out the rest of the AppEntry struct using parseDesktopFile()
    icon getss found using gtk in findIconPath()
*/

std::vector<AppEntry> getEntries(bool isolated, int monIdx)
{
    std::vector<AppEntry> res = {};
    bool singleInstance = getIfThisIsOnlyInstance();
    // (class, entry)
    std::unordered_map<std::string, AppEntry> entries = {};
    if (current_instances.size() == 0) current_instances = getRunningInstances();

    for (AppInstance& inst : current_instances)
    {
        if (singleInstance || (inst.monitorIdx == monIdx))
            entries[inst.wclass].instances.push_back(inst);
    }

    for (auto& pair : entries)
    {
        pair.second.count_instances = pair.second.instances.size();
        pair.second.desktopFile = getDesktopFileOfInstances(pair.second.instances);

        AppEntry tempEntry = parseDesktopFile(pair.second.desktopFile);

        pair.second.name = tempEntry.name;
        pair.second.execCmd = tempEntry.execCmd;
        pair.second.iconPath = findIconPath(tempEntry.iconPath);
        
        res.push_back(pair.second);
    }

    return res;
}

/*
    Win: is the Dock Window class
*/

class Win : public Gtk::Window 
{
    public:
        // crucial data for application packaged in one struct for comfort
        struct AppContext
        {
            int icon_size = 0;
            int icon_bg_size = 0;
            int padding = 0;
            int winW = 0;
            int winH = 0;
            int dockH = 0;
            int dockW = 0;
            int displayIdx = 0;
            int hotspot_height = 0;
            bool drawLauncher = false;
            int timeout = 0;
            int duration = 0;
            int edgeMargin = 0;
            bool autohide = false;
            std::string launcher_cmd = "";
            bool isolated_to_monitor = false;
            DockEdge edge = DockEdge::EDGEBOTTOM;
            std::vector <AppEntry> entries = {};
        } appCtx;

        // State Machine for animating the Dock to reduce buggy behaviour
        enum class DockState { Hidden, Visible, Hiding, Showing};
        DockState state = DockState::Visible;
        DockState wanted_state = DockState::Hidden;
        int64_t timeWhenMouseLeftDock = 0;


        Win(int argc, char **argv)
        {
            // inits win by first filling out AppContext struct
            {
                std::ifstream conf("conf/settings.conf");
                std::string line;
                
                while (std::getline(conf, line))
                {
                    if (!line.empty())
                    {
                        auto values = splitStr(line, ":");
                        
                        if (values[0] == "icon_size")
                        {
                            appCtx.icon_size = std::stoi(values[1]);
                        } else if (values[0] == "padding")
                        {
                            appCtx.padding = std::stoi(values[1]);
                        } else if (values[0] == "hotspot_height")
                        {
                            appCtx.hotspot_height = std::stoi(values[1]);
                        } else if (values[0] == "autohide_timeout")
                        {
                            appCtx.timeout = std::stoi(values[1]);
                        } else if (values[0] == "autohide_duration")
                        {
                            appCtx.duration = std::stoi(values[1]);
                        } else if (values[0] == "draw_launcher")
                        {
                            appCtx.drawLauncher = (bool)std::stoi(values[1]);
                        } else if (values[0] == "edge_margin")
                        {
                            appCtx.edgeMargin = std::stoi(values[1]);
                        } else if (values[0] == "autohide")
                        {
                            appCtx.autohide = (bool)std::stoi(values[1]);
                        } else if (values[0] == "launcher_cmd")
                        {
                            appCtx.launcher_cmd = values[1];
                        } else if (values[0] == "isolated_to_monitor")
                        {
                            appCtx.isolated_to_monitor = (bool)std::stoi(values[1]);
                        }
                    }
                }

                conf.close();

                appCtx.icon_bg_size = appCtx.icon_size * (4.f/3.f);
                appCtx.winH = appCtx.icon_bg_size + 2 * appCtx.padding;
                appCtx.dockH = appCtx.icon_bg_size;

                for (int i = 0; i < argc; i++)
                {
                    if (argv[i][0] == '-' && argv[i][1] == 'd')
                    {
                        int mon = std::stoi(((std::string)argv[i]).substr(2));
                        unsigned int n_monitors = Gdk::Display::get_default()->get_monitors()->get_n_items();

                        if (mon >= 0 && mon < n_monitors) appCtx.displayIdx = mon;
                    }
                    if (argv[i][0] == '-' && argv[i][1] == 'e')
                    {
                        int edgeIdx = std::stoi(((std::string)argv[i]).substr(2)) % 4;
                        if (edgeIdx >= 0 && edgeIdx < 4) appCtx.edge = (DockEdge)edgeIdx;
                    }
                    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
                    {
                        std::cout << "GTKDock - Linux Application Dock\n\nUsage: GTKDock -d[monIdx] -e[edgeIdx]\n\n -d[monIdx]: ex. -d0\n -e[edgeIdx]: ex. -e3\n\nDock Edge Possible values: 0 = left 1 = top 2 = right 3 = bottom" << std::endl;
                        std::exit(0);
                    }
                }
                
                appCtx.entries = loadEntries();
                appCtx.dockW = (appCtx.entries.size()) * (appCtx.icon_bg_size + appCtx.padding);
                
                bool sep = false;
                for (AppEntry& e : appCtx.entries)
                {
                    if (e.name == "line")
                    {
                        sep = true;
                        break;
                    }
                }

                if (sep) appCtx.dockW -= appCtx.icon_bg_size + appCtx.padding - 6;

                appCtx.winW = appCtx.dockW + appCtx.padding;

                if (appCtx.edge == DockEdge::EDGELEFT || appCtx.edge == DockEdge::EDGERIGHT)
                {
                    int dW, dH, wW, wH = 0;
                    dW = appCtx.dockW;
                    dH = appCtx.dockH;
                    wW = appCtx.winW;
                    wH = appCtx.winH;

                    appCtx.dockW = dH;
                    appCtx.dockH = dW;
                    appCtx.winW = wH;
                    appCtx.winH = wW;
                }
            }

            // either use gtk-layer-shell protocol to put window on top or add hook to use x11 specific functions to do the same thing
            if (wayland)
            {    
                GLS_setup_top_layer(this, appCtx.displayIdx, appCtx.edgeMargin, "GTKDock", appCtx.edge);
            } else
            {
                signal_realize().connect(sigc::mem_fun(*this, &Win::on_realizeX));
            }
                
            set_default_size(appCtx.winW, appCtx.winH);
            set_title("GTKDock");

            // populate Dock with widgets
            buildDock();

            // logic for auto hide functionality
            auto motion_controllerWin = Gtk::EventControllerMotion::create();

            motion_controllerWin->signal_enter().connect([this](double x, double y) mutable {
                if (this->state == Win::DockState::Hiding || this->wanted_state == Win::DockState::Hidden)
                {
                    this->wanted_state = Win::DockState::Visible;
                }
            });

            motion_controllerWin->signal_leave().connect([this]() mutable {
                if (this->state == Win::DockState::Visible || this->state == Win::DockState::Showing)
                {
                    this->timeWhenMouseLeftDock = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();
                    this->wanted_state = Win::DockState::Hidden;
                }
            });

            if (!wayland)
            {
                motion_controllerWin->signal_motion().connect([this](double x, double y) mutable {
                    if (y > this->appCtx.winH - this->appCtx.hotspot_height)
                    {
                        if (this->state == Win::DockState::Hidden || this->state == Win::DockState::Hiding)
                        {
                            this->wanted_state = Win::DockState::Visible;                        
                        }
                    }
                });
            }
            
            if (appCtx.autohide)
            {
                add_tick_callback([this, last_time = int64_t{0}](const Glib::RefPtr<Gdk::FrameClock>& clock) mutable {
                    double frame_time_ms = 0;

                    {
                        int64_t current_time = clock->get_frame_time();  // microseconds

                        if (last_time != 0) {
                            int64_t delta_us = current_time - last_time;
                            frame_time_ms = delta_us / 1000.0;
                        }

                        last_time = current_time;
                    }
                    
                    if (wanted_state == Win::DockState::Hidden && (state == Win::DockState::Visible || state == Win::DockState::Showing))
                    {
                        if (std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count() - this->timeWhenMouseLeftDock > this->appCtx.timeout)
                        {
                            this->state = Win::DockState::Hiding;
                        }
                    }

                    if (wanted_state == Win::DockState::Visible && (state == Win::DockState::Hiding || state == Win::DockState::Hidden))
                    {
                        this->state = Win::DockState::Showing;
                    }

                    if (state == Win::DockState::Hiding)
                    {   
                        if (!animateOut( frame_time_ms / appCtx.duration )) this->state = Win::DockState::Hidden;
                    }

                    if (state == Win::DockState::Showing)
                    {   
                        if (!animateIn( frame_time_ms / appCtx.duration )) this->state = Win::DockState::Visible;
                    }

                    return true;
                });
            }
            add_controller(motion_controllerWin);

            // relying on polling because i havent found a wm agnostic way to poll fow window client changes
            Glib::signal_timeout().connect([this]() {
                updateDock();
                return true;
            }, 500);
        }

        /*
            updates Dock by checking if AppEntry vector has changed then rebuilding the Dock
        */

        void updateDock() {
            auto newEntries = loadEntries();
            
            // Check if entries changed
            if (newEntries.size() != appCtx.entries.size() || 
                !std::equal(newEntries.begin(), newEntries.end(), appCtx.entries.begin(),
                    [](const AppEntry& a, const AppEntry& b)
                    {
                        if (a.count_instances != b.count_instances || a.name != b.name )
                        {    
                            return false;
                        }

                        for (int i = 0; i < a.count_instances; i++)
                        {
                            if (a.instances[i].title != b.instances[i].title || a.instances[i].fullscreen != b.instances[i].fullscreen)
                            {
                                return false;
                            }
                        }

                        return true;
                    }
            ))
            {
                if (newEntries.size() != appCtx.entries.size()) wanted_state = Win::DockState::Visible;
                cleanupDock();
 
                // Only rebuild if entries changed
                for (auto child : dock_box.get_children())
                {
                    dock_box.remove(*child);  // Remove all children
                }

                appCtx.entries = newEntries;

                if (appCtx.edge == DockEdge::EDGELEFT || appCtx.edge == DockEdge::EDGERIGHT)
                {
                    appCtx.dockH = (appCtx.entries.size()) * (appCtx.icon_bg_size + appCtx.padding);
                    
                    bool sep = false;
                    for (AppEntry& e : appCtx.entries)
                    {
                        if (e.name == "line")
                        {
                            sep = true;
                            break;
                        }
                    }

                    if (sep) appCtx.dockH -= appCtx.icon_bg_size + appCtx.padding - 6;

                    appCtx.winH = appCtx.dockH + appCtx.padding;
                }
                else
                {
                    appCtx.dockW = (appCtx.entries.size()) * (appCtx.icon_bg_size + appCtx.padding);
                    
                    bool sep = false;
                    for (AppEntry& e : appCtx.entries)
                    {
                        if (e.name == "line")
                        {
                            sep = true;
                            break;
                        }
                    }

                    if (sep) appCtx.dockW -= appCtx.icon_bg_size + appCtx.padding - 6;

                    appCtx.winW = appCtx.dockW + appCtx.padding;
                }

                this->set_default_size(appCtx.winW, appCtx.winH);

                buildDock();

                Glib::signal_timeout().connect([this]() {
                    if (state == Win::DockState::Visible)
                    {
                        timeWhenMouseLeftDock = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();
                        wanted_state = Win::DockState::Hidden;
                        return false;
                    }
                    
                    return true;  // Keep the timer running
                }, 250);  // Update every 1 second
            }
        }

        // builds the Dock
        void buildDock()
        {
            dock_box = Gtk::Fixed();
            container = Gtk::Fixed();
            dock_box.get_style_context()->add_class("dock");

            if (appCtx.edge == DockEdge::EDGELEFT || appCtx.edge == DockEdge::EDGERIGHT)
            {
                GdkRectangle dock = {(int)((appCtx.winW - appCtx.dockW) * 0.5), (int)((appCtx.winH - appCtx.dockH) * 0.5), appCtx.dockW, appCtx.dockH};
                double sx = dock.x;
                double sy = dock.y;
                double sl = appCtx.icon_bg_size;
                
                auto settings = Gtk::Settings::get_default();
                
                // Add N buttons with padding
                for (int i = 0; i < appCtx.entries.size(); ++i) {
                    float ssx = sx + (sl - appCtx.icon_size) * 0.5;
                    float ssy = sy + (sl - appCtx.icon_size) * 0.5;

                    if (appCtx.entries[i].name != "line")
                    {
                        auto btn = Gtk::make_managed<Gtk::MenuButton>();
                        btn->set_size_request(sl, sl);
                        btn->add_css_class("btn");
                        btn->set_tooltip_text(appCtx.entries[i].name);
                        
                        auto pm = get_Menu(appCtx.entries[i]);
                        
                        pm->set_parent(*btn);
                        pm->signal_realize().connect([pm, this]() {
                            auto motion_controller = Gtk::EventControllerMotion::create();

                            motion_controller->signal_enter().connect([pm, this](double x, double y) {
                                this->wanted_state = Win::DockState::Visible;                        
                            });

                            motion_controller->signal_leave().connect([pm, this]() {
                                this->wanted_state = Win::DockState::Visible;               
                            });
                            
                            pm->add_controller(motion_controller);
                        });

                        auto click_gesture = Gtk::GestureClick::create();
                        
                        click_gesture->set_button(0);
                        click_gesture->signal_released().connect([this, i, btn, click_gesture, pm](int n_press, double x, double y) {
                            guint button = click_gesture->get_current_button();
                            
                            if (button == GDK_BUTTON_PRIMARY)
                            {
                                pm->popdown();
                                if (this->appCtx.entries[i].count_instances == 0) std::system(("cd ~/  && " + this->appCtx.entries[i].execCmd + " &").c_str());
                                else 
                                {
                                    openInstance(this->appCtx.entries[i].instances[0]);
                                }
                            } else if (button == GDK_BUTTON_SECONDARY && this->state == Win::DockState::Visible)
                            {
                                pm->popup(); // Show the dropdown
                            }
                        });

                        btn->add_controller(click_gesture);
                        auto empty_box = Gtk::make_managed<Gtk::Box>();
                        btn->set_child(*empty_box);  // No icon shown

                        add_widget_to_dock_box(*btn, sx, sy);

                        auto img = Gtk::make_managed<Gtk::Image>(appCtx.entries[i].iconPath);
                        img->set_size_request(appCtx.icon_size,appCtx.icon_size);
                        img->set_can_target(false);

                        add_widget_to_dock_box(*img, ssx, ssy);
                        
                        if (appCtx.entries[i].count_instances > 0)
                        {
                            auto dot_box = Gtk::make_managed<Gtk::DrawingArea>();
                            dot_box->set_size_request(sl, appCtx.icon_size / 8.f - 2);
                            dot_box->set_can_target(false);
                            dot_box->set_content_width(sl);
                            dot_box->set_content_height(appCtx.icon_size / 8.f - 2);
                            dot_box->add_css_class("dotbox");

                            dot_box->set_draw_func([this, i](const Cairo::RefPtr<Cairo::Context>& cr, int width, int height){
                                cr->set_source_rgba(1.0, 1.0, 1.0, 1.0);

                                if (this->appCtx.entries[i].count_instances == 1)
                                {
                                    cr->arc ( width / 2.0, height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                } else if (this->appCtx.entries[i].count_instances == 2)
                                {
                                    cr->arc ( width / 2.0 - (height / 2.0 + 1), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                    cr->arc ( width / 2.0 + (height / 2.0 + 1), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                } else if (this->appCtx.entries[i].count_instances == 3)
                                {
                                    cr->arc ( width / 2.0, height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                    cr->arc ( width / 2.0 - (height + 1), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                    cr->arc ( width / 2.0 + (height + 1), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                } else
                                {
                                    cr->arc ( width / 2.0 - (height / 2.0 + 1), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                    cr->arc ( width / 2.0 + (height / 2.0 + 1), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                    cr->arc ( width / 2.0 - 1.5*(height + 2), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                    cr->arc ( width / 2.0 + 1.5*(height + 2), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                }

                                cr->fill();
                            });

                            add_widget_to_dock_box(*dot_box, sx, sy + sl - appCtx.icon_size / 8.f + 1);
                        }
                        sy += sl + appCtx.padding;
                    }
                    else
                    {
                        auto sep = Gtk::make_managed<Gtk::Box>();
                        sep->set_size_request(appCtx.dockW - 16, 1);
                        sep->add_css_class("sep");
                        add_widget_to_dock_box(*sep, sx + 8, sy);
                        sy += 6;
                    }
                }
            }
            else
            {
                GdkRectangle dock = {(int)((appCtx.winW - appCtx.dockW) * 0.5), (int)((appCtx.winH - appCtx.dockH) * 0.5), appCtx.dockW, appCtx.dockH};
                double sx = dock.x;
                double sy = dock.y;
                double sl = appCtx.icon_bg_size;
                
                auto settings = Gtk::Settings::get_default();
                
                // Add N buttons with padding
                for (int i = 0; i < appCtx.entries.size(); ++i) {
                    float ssx = sx + (sl - appCtx.icon_size) * 0.5;
                    float ssy = sy + (sl - appCtx.icon_size) * 0.5;

                    if (appCtx.entries[i].name != "line")
                    {
                        auto btn = Gtk::make_managed<Gtk::MenuButton>();
                        btn->set_size_request(sl, sl);
                        btn->add_css_class("btn");
                        btn->set_tooltip_text(appCtx.entries[i].name);
                        
                        auto pm = get_Menu(appCtx.entries[i]);
                        
                        pm->set_parent(*btn);
                        pm->signal_realize().connect([pm, this]() {
                            auto motion_controller = Gtk::EventControllerMotion::create();

                            motion_controller->signal_enter().connect([pm, this](double x, double y) {
                                this->wanted_state = Win::DockState::Visible;                        
                            });

                            motion_controller->signal_leave().connect([pm, this]() {
                                this->wanted_state = Win::DockState::Visible;               
                            });
                            
                            pm->add_controller(motion_controller);
                        });

                        auto click_gesture = Gtk::GestureClick::create();
                        
                        click_gesture->set_button(0);
                        click_gesture->signal_released().connect([this, i, btn, click_gesture, pm](int n_press, double x, double y) {
                            guint button = click_gesture->get_current_button();
                            
                            if (button == GDK_BUTTON_PRIMARY)
                            {
                                pm->popdown();
                                if (this->appCtx.entries[i].count_instances == 0) std::system(("cd ~/  && " + this->appCtx.entries[i].execCmd + " &").c_str());
                                else 
                                {
                                    openInstance(this->appCtx.entries[i].instances[0]);
                                }
                            } else if (button == GDK_BUTTON_SECONDARY && this->state == Win::DockState::Visible)
                            {
                                pm->popup(); // Show the dropdown
                            }
                        });

                        btn->add_controller(click_gesture);
                        auto empty_box = Gtk::make_managed<Gtk::Box>();
                        btn->set_child(*empty_box);  // No icon shown

                        add_widget_to_dock_box(*btn, sx, sy);

                        auto img = Gtk::make_managed<Gtk::Image>(appCtx.entries[i].iconPath);
                        img->set_size_request(appCtx.icon_size,appCtx.icon_size);
                        img->set_can_target(false);

                        add_widget_to_dock_box(*img, ssx, ssy);
                        
                        if (appCtx.entries[i].count_instances > 0)
                        {
                            auto dot_box = Gtk::make_managed<Gtk::DrawingArea>();
                            dot_box->set_size_request(sl, appCtx.icon_size / 8.f - 2);
                            dot_box->set_can_target(false);
                            dot_box->set_content_width(sl);
                            dot_box->set_content_height(appCtx.icon_size / 8.f - 2);
                            dot_box->add_css_class("dotbox");

                            dot_box->set_draw_func([this, i](const Cairo::RefPtr<Cairo::Context>& cr, int width, int height){
                                cr->set_source_rgba(1.0, 1.0, 1.0, 1.0);

                                if (this->appCtx.entries[i].count_instances == 1)
                                {
                                    cr->arc ( width / 2.0, height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                } else if (this->appCtx.entries[i].count_instances == 2)
                                {
                                    cr->arc ( width / 2.0 - (height / 2.0 + 1), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                    cr->arc ( width / 2.0 + (height / 2.0 + 1), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                } else if (this->appCtx.entries[i].count_instances == 3)
                                {
                                    cr->arc ( width / 2.0, height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                    cr->arc ( width / 2.0 - (height + 1), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                    cr->arc ( width / 2.0 + (height + 1), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                } else
                                {
                                    cr->arc ( width / 2.0 - (height / 2.0 + 1), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                    cr->arc ( width / 2.0 + (height / 2.0 + 1), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                    cr->arc ( width / 2.0 - 1.5*(height + 2), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                    cr->arc ( width / 2.0 + 1.5*(height + 2), height / 2.0,  height / 2.0,      0, 2 * G_PI);
                                }

                                cr->fill();
                            });

                            add_widget_to_dock_box(*dot_box, sx, sy + sl - appCtx.icon_size / 8.f + 1);
                        }
                        sx += sl + appCtx.padding;
                    }
                    else
                    {
                        auto sep = Gtk::make_managed<Gtk::Box>();
                        sep->set_size_request(1, appCtx.dockH - 16);
                        sep->add_css_class("sep");
                        add_widget_to_dock_box(*sep, sx, sy + 8);
                        sx += 6;
                    }
                }
            }
        
            container.put(dock_box, 0, 0);
            set_child(container);
        }

        // cleans up docks widgets and their children and handles popovers
        void cleanupDock() {
            // Unparent all popovers first
            for (auto* popover : popoversofpopovers) {
                if (popover->get_parent()) {
                    popover->unparent();
                }
            }
            popoversofpopovers.clear();

            for (auto* popover : popovers) {
                if (popover->get_parent()) {
                    popover->unparent();
                }
            }

            popovers.clear();
            widget_positions.clear();
            
            // Now safely remove all children
            while (auto* child = dock_box.get_first_child()) {
                dock_box.remove(*child);
            }
        }

        void add_widget_to_dock_box(Gtk::Widget& w, double x, double y)
        {
            dock_box.put(w, x, y);
            widget_positions.push_back({x,y});
        }

        std::vector<Gtk::Popover *> popovers;
        std::vector<Gtk::Popover *> popoversofpopovers;

        Gtk::Fixed dock_box;
        Gtk::Fixed container;
        float t1 = 0;
        float t2 = 0;

        struct point { double x, y; };
        std::vector<point> widget_positions = {};

        // x11 specific way of moving dock
        int offset_y = 0;
        void moveToOffset()
        {
            if (appCtx.edge == DockEdge::EDGEBOTTOM) dock_box.set_margin_top(offset_y);
            else if (appCtx.edge == DockEdge::EDGETOP) dock_box.set_margin_top(-offset_y);

            else
            {
                container.move(dock_box, offset_y, 0);   
            }
            /* BUG
                set_margin_start/end dont work
                on gnome x11 top edge doesnt hide fully (won't fix)
                no idea what to do to move dock side to side
                maybe use move but that will be sloow    
            */
        }
        
        bool animateOut(float delta)
        {
            if (t1 <= 1)
            {
                if (wayland)
                {
                    if (appCtx.edge == DockEdge::EDGEBOTTOM || appCtx.edge == DockEdge::EDGETOP)
                        GLS_chngMargin(this, -(this->appCtx.winH + this->appCtx.edgeMargin) * t1, appCtx.edge);
                    else
                        GLS_chngMargin(this, -(this->appCtx.winW + this->appCtx.edgeMargin) * t1, appCtx.edge);
                }
                else
                {
                    if (appCtx.edge == DockEdge::EDGEBOTTOM || appCtx.edge == DockEdge::EDGETOP)
                        offset_y = (this->appCtx.winH + this->appCtx.edgeMargin) * t1;
                    else
                        offset_y = (this->appCtx.winW + this->appCtx.edgeMargin) * t1;
                    moveToOffset();
                }
            
                t1 += delta;
            }
            else
            {
                if (wayland)
                {
                    if (appCtx.edge == DockEdge::EDGEBOTTOM || appCtx.edge == DockEdge::EDGETOP)
                        GLS_chngMargin(this, -(this->appCtx.winH + this->appCtx.edgeMargin), appCtx.edge);
                    else
                        GLS_chngMargin(this, -(this->appCtx.winW + this->appCtx.edgeMargin), appCtx.edge);
                }
                else
                {
                    if (appCtx.edge == DockEdge::EDGEBOTTOM || appCtx.edge == DockEdge::EDGETOP)
                        offset_y = (this->appCtx.winH + this->appCtx.edgeMargin) * t1;
                    else
                        offset_y = (this->appCtx.winW + this->appCtx.edgeMargin) * t1;
                    moveToOffset();
                }
                
                t1 = 0;
                return false;
            }

            return true;  
        }

        bool animateIn(float delta)
        {
            if (t2 <= 1)
            {
                if (wayland)
                {
                    if (appCtx.edge == DockEdge::EDGEBOTTOM || appCtx.edge == DockEdge::EDGETOP)
                        GLS_chngMargin(this, -(this->appCtx.winH + this->appCtx.edgeMargin) * (1 - t2) + this->appCtx.edgeMargin, appCtx.edge);
                    else
                        GLS_chngMargin(this, -(this->appCtx.winW + this->appCtx.edgeMargin) * (1 - t2) + this->appCtx.edgeMargin, appCtx.edge);    
                }
                else
                {
                    if (appCtx.edge == DockEdge::EDGEBOTTOM || appCtx.edge == DockEdge::EDGETOP)
                        offset_y = (this->appCtx.winH + this->appCtx.edgeMargin) * (1 - t2) + this->appCtx.edgeMargin;
                    else
                        offset_y = (this->appCtx.winW + this->appCtx.edgeMargin) * (1 - t2) + this->appCtx.edgeMargin;
                    moveToOffset();
                }

                t2 += delta;
            }
            else
            {
                if (wayland)
                {
                    GLS_chngMargin(this, this->appCtx.edgeMargin, appCtx.edge);
                }
                else
                {
                    offset_y = this->appCtx.edgeMargin;
                    moveToOffset();
                }

                t2 = 0;
                return false;
            }

            return true;  
        }

        //populates instance menu with widgets
        void populateInstanceMenu(Gtk::Popover* i_popover, AppInstance inst)
        {
            auto popover_box =  Gtk::make_managed<Gtk::Box>();

            i_popover->set_child(*popover_box);
            popover_box->set_orientation(Gtk::Orientation::VERTICAL);
            popover_box->set_spacing(5);
            popover_box->set_expand(false);

            auto button = Gtk::make_managed<Gtk::Button>(inst.title);
            button->signal_clicked().connect([inst](){
                openInstance(inst);
            });

            button->add_css_class("mbutton");
            popover_box->append(*button);   

            auto separator1 = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL);
            separator1->add_css_class("sepe");
            popover_box->append(*separator1);

            auto button1 = Gtk::make_managed<Gtk::Button>("Close Window");
            button1->signal_clicked().connect([inst](){
                closeInstance({inst});
            });

            button1->add_css_class("mbutton");
            popover_box->append(*button1);

            // too wm specific ... tiled vs stacked
            /*auto button2 = Gtk::make_managed<Gtk::Button>((inst.fullscreen) ? "Minimize" : "Maximize");
            button2->signal_clicked().connect([this, inst](){
                toggleMaximizeMinimizeWindow(inst);
            });

            button2->add_css_class("mbutton");
            popover_box->append(*button2);*/
        }

        // creates popvermenu from an appentry
        Gtk::Popover * get_Menu(AppEntry& e)
        {
            auto m_popover = Gtk::make_managed<Gtk::Popover>();
            popovers.push_back(m_popover);
            m_popover->set_size_request(3*appCtx.icon_bg_size, -1);
            m_popover->set_expand(false);
            
            if (appCtx.edge == DockEdge::EDGERIGHT) m_popover->set_position(Gtk::PositionType::LEFT);
            else if (appCtx.edge == DockEdge::EDGEBOTTOM) m_popover->set_position(Gtk::PositionType::TOP);
            else if (appCtx.edge == DockEdge::EDGELEFT) m_popover->set_position(Gtk::PositionType::RIGHT);
            else if (appCtx.edge == DockEdge::EDGETOP) m_popover->set_position(Gtk::PositionType::BOTTOM);

            auto m_popover_box =  Gtk::make_managed<Gtk::Box>();

            m_popover->set_child(*m_popover_box);
            m_popover_box->set_orientation(Gtk::Orientation::VERTICAL);
            m_popover_box->set_spacing(5);

            if (e.name != "Launcher")
            {
                // Add content to the popover
                auto label = Gtk::make_managed<Gtk::Label>(e.name);
                label->set_ellipsize(Pango::EllipsizeMode::END);
                label->set_max_width_chars(20);
                label->add_css_class("applabel");
                m_popover_box->append(*label);
                

                auto separator1 = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL);
                separator1->add_css_class("sepe");
                m_popover_box->append(*separator1);

                for (AppInstance& instance : e.instances)
                {
                    auto menubtn = Gtk::make_managed<Gtk::Button>();
                    menubtn->add_css_class("mbutton");
                    
                    auto click_gesture = Gtk::GestureClick::create();
                    
                    auto i_popover = Gtk::make_managed<Gtk::Popover>();
                    popoversofpopovers.push_back(i_popover);

                    populateInstanceMenu(i_popover, instance);

                    i_popover->set_position(Gtk::PositionType::RIGHT);
                    if (appCtx.edge == DockEdge::EDGERIGHT) i_popover->set_position(Gtk::PositionType::LEFT);

                    i_popover->set_parent(*menubtn);
                    i_popover->signal_realize().connect([i_popover, this]() {
                        auto motion_controller = Gtk::EventControllerMotion::create();

                        motion_controller->signal_enter().connect([i_popover, this](double x, double y) {
                            this->wanted_state = Win::DockState::Visible;                        
                        });

                        motion_controller->signal_leave().connect([i_popover, this]() {
                            this->wanted_state = Win::DockState::Visible;               
                        });
                        
                        i_popover->add_controller(motion_controller);
                    });

                    menubtn->signal_clicked().connect([this, menubtn, click_gesture, instance, i_popover](){
                        i_popover->popup(); // Show the dropdown
                    });


                    auto box =  Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
                    box->set_spacing(5);
                    
                    auto label = Gtk::make_managed<Gtk::Label>(instance.title);
                    label->set_hexpand(true);
                    label->set_ellipsize(Pango::EllipsizeMode::END);
                    label->set_max_width_chars(20);
                    label->set_halign(Gtk::Align::START);
                    
                    auto img = Gtk::make_managed<Gtk::Image>(Gio::Icon::create("pan-end-symbolic"));
                    img->set_halign(Gtk::Align::END);

                    box->append(*label);
                    box->append(*img);

                    menubtn->set_child(*box);
                    m_popover_box->append(*menubtn);
                }

                auto separator2 = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL);
                separator2->add_css_class("sepe");
                m_popover_box->append(*separator2);

                auto button1 = Gtk::make_managed<Gtk::Button>("New Window");
                button1->signal_clicked().connect([e](){
                    system(("cd ~/  && " + e.execCmd + " &").c_str());
                });

                button1->add_css_class("mbutton");
                m_popover_box->append(*button1);

                auto button2 = Gtk::make_managed<Gtk::Button>((e.instances.size() > 1) ? "Close All Windows" : "Close Window");
                button2->signal_clicked().connect([e](){
                    closeInstance(e.instances);
                });

                button2->add_css_class("mbutton");
                m_popover_box->append(*button2);

                auto button3 = Gtk::make_managed<Gtk::Button>((e.isPinned) ? "Unpin" : "Pin");
                button3->signal_clicked().connect([e](){
                    if (!e.isPinned)
                    {
                        std::ofstream file("conf/pinnedApps", std::ios_base::app);
                        
                        if (file.is_open()) {
                            file << (e.name + ":" + e.execCmd + ":" + e.iconPath + ":" + e.desktopFile) << "\n";  // Add newline if needed
                            file.close();
                        } else {
                            // Handle error - couldn't open file
                            std::cerr << "Unable to open file: conf/pinnedApps" << std::endl;
                        }
                    } else
                    {
                        std::ofstream temp("temp.txt");
                        std::ifstream file("conf/pinnedApps");
                        
                        std::string line;
                        
                        if (file.is_open() && temp.is_open()) {
                            while (std::getline(file, line)) {
                                // Write to temp file only if condition is NOT met
                                if (!(splitStr(line, ":")[3] == e.desktopFile)) {
                                    temp << line << "\n";
                                }
                            }
                            
                            file.close();
                            temp.close();
                            
                            // Remove original file and rename temp file
                            std::remove("conf/pinnedApps");
                            std::rename("temp.txt", "conf/pinnedApps");
                        } else {
                            std::cerr << "Error opening files!" << std::endl;
                        }                    }
                });

                button3->add_css_class("mbutton");
                m_popover_box->append(*button3);
            } else
            {
                auto label = Gtk::make_managed<Gtk::Label>("GTKDock");
                m_popover_box->append(*label);

                auto separator1 = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL);
                separator1->add_css_class("sepe");
                m_popover_box->append(*separator1);

                auto button1 = Gtk::make_managed<Gtk::Button>("Open Config Dir");
                button1->signal_clicked().connect([](){
                    g_print("New\n");
                });

                button1->add_css_class("mbutton");
                m_popover_box->append(*button1);

                auto button2 = Gtk::make_managed<Gtk::Button>("Close Dock");
                button2->signal_clicked().connect([this](){
                    std::exit(0);
                });

                button2->add_css_class("mbutton");
                m_popover_box->append(*button2);

            }
            
            return m_popover;
        }

        // creates entries vector orders them correctly (pinned seperator unpinned launcher) and adds launcher
        std::vector<AppEntry> loadEntries()
        {
            std::vector<AppEntry> entries = getEntries(appCtx.isolated_to_monitor, appCtx.displayIdx);

            std::vector <AppEntry> pinned = {};
            std::ifstream file("conf/pinnedApps");

            // Check if the file was opened successfully
            if (!file.is_open()) {
                std::cerr << "Error: Could not open Pinned apps file " << std::endl;
            }

            std::string line;

            while (std::getline(file, line))
            {
                std::vector<std::string> values = splitStr(line, ":");
        
                AppEntry e = {0, true, values[0], values[1], values[2], values[3]};
                e.count_instances = 0;
                pinned.push_back(e);
            }

            file.close();

            for (AppEntry& pentry : pinned)
            {
                int i = 0;
                for (AppEntry& entry : entries)
                {
                    if (pentry.name == entry.name)
                    {
                        entry.isPinned = true;
                        pentry = entry;
                        entries.erase(entries.begin()+i);
                    }
                    i++;
                }
            }

            if (pinned.size() > 0 && entries.size() > 0) pinned.push_back( {0, false, "line"} );
            entries.insert(entries.begin(), pinned.begin(), pinned.end());

            if (appCtx.drawLauncher)
            {
                entries.push_back( {
                    0, true, "Launcher", appCtx.launcher_cmd, "imgs/launcher.png"
                } );
            }

            return entries;
        }

        void on_realizeX()
        {
            onrealizeXDock(this, appCtx.displayIdx, appCtx.winW, appCtx.winH, appCtx.edgeMargin, appCtx.edge);
        }
};

/*
    wayland only cus X11 window repositioning is too slow so just move all window contents down
    sits above everything and does nothing but make dock visible when entered
*/

class Hotspot : public Gtk::Window 
{
    int mon = -1;
    Win * win = nullptr;
    double last_x = 0;
    double last_y = 0;

    public:
        Hotspot(int argc, char **argv, Win * win)
        {
            this->win = win;
            for (int i = 0; i < argc; i++)
            {
                if (argv[i][0] == '-' && argv[i][1] == 'd')
                {
                    int m = std::stoi(((std::string)argv[i]).substr(2));
                    unsigned int n_monitors = Gdk::Display::get_default()->get_monitors()->get_n_items();

                    if (m >= 0 && m < n_monitors) mon = m;
                }
            }
            
            GLS_setup_top_layer(this, mon, 0, "GTKDock", win->appCtx.edge);

            auto motion = Gtk::EventControllerMotion::create();

            motion->signal_enter().connect([this, win] (double, double) {
                if (win->state == Win::DockState::Hidden || win->state == Win::DockState::Hiding)
                {
                    win->wanted_state = Win::DockState::Visible;                        
                }
                if (win->wanted_state == Win::DockState::Hidden)
                {
                    win->wanted_state = Win::DockState::Visible;                        
                }
            });

            motion->signal_motion().connect([this](double x, double y) mutable {
                last_x = x;
                last_y = y;
            });


            motion->signal_leave().connect([this, win] () {

                if ((win->appCtx.edge == DockEdge::EDGEBOTTOM || win->appCtx.edge == DockEdge::EDGETOP))
                {
                    if ((last_x <= win->appCtx.padding || last_x >= win->appCtx.winW - win->appCtx.padding) && (win->state == Win::DockState::Visible || win->state == Win::DockState::Showing))
                    {
                        win->timeWhenMouseLeftDock = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();
                        win->wanted_state = Win::DockState::Hidden;
                    }
                } else
                {
                    if ((last_y <= win->appCtx.padding || last_y >= win->appCtx.winH - win->appCtx.padding) && (win->state == Win::DockState::Visible || win->state == Win::DockState::Showing))
                    {
                        win->timeWhenMouseLeftDock = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();
                        win->wanted_state = Win::DockState::Hidden;
                    }
                }
            });
            
            motion->signal_leave().connect([] () {});
            add_controller(motion);
        
            set_decorated(false);

            if ((win->appCtx.edge == DockEdge::EDGEBOTTOM || win->appCtx.edge == DockEdge::EDGETOP))
                set_default_size(win->appCtx.winW, win->appCtx.hotspot_height);
            else
                set_default_size(win->appCtx.hotspot_height, win->appCtx.winH);

            set_title("hotspot");
            add_css_class("hotspot");
            win->property_default_width().signal_changed().connect([this](){
                int w,h;
                this->win->get_default_size(w, h);
                this->set_default_size(w, this->win->appCtx.hotspot_height);
            });

            win->property_default_height().signal_changed().connect([this](){
                int w,h;
                this->win->get_default_size(w, h);
                this->set_default_size(this->win->appCtx.hotspot_height, h);
            });
        }
};

int main (int argc, char **argv)
{
    wayland = (strcmp(std::getenv("XDG_SESSION_TYPE"), "wayland") == 0);
 
    auto app = Gtk::Application::create();
   
    if (wayland && !check_layer_shell_support())
    {
        std::cout << "gtk-layer-shell protocol is not supported on your wayland WM" << std::endl;
        std::exit(0);
    }

    app->signal_startup().connect([app, argc, argv](){
        auto win = Gtk::make_managed<Win>(argc, argv);
        
        Glib::RefPtr<Gtk::CssProvider> css_provider = Gtk::CssProvider::create();
        css_provider->load_from_path("conf/style.css");
            
        win->get_style_context()->add_provider_for_display(Gdk::Display::get_default(), css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        
        app->add_window(*win);
        win->present();

        if (wayland)
        {
            auto hotspot = Gtk::make_managed<Hotspot>(argc,argv, win);
            hotspot->get_style_context()->add_provider_for_display(Gdk::Display::get_default(), css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            app->add_window(*hotspot);
            hotspot->present();
        }
    });

    std::thread monitoringThread([](){
        while (running) {
            current_instances = getRunningInstances();
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    });

    return app->run();
}