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
#include "wm-specific.h"

bool wayland = false;

std::vector<std::string> splitStr(std::string str, std::string separator)
{
    std::vector<std::string> result;
    size_t pos = 0;
    size_t separatorLength = separator.length();

    if (separatorLength == 0) {
        // If separator is empty, return the original string as single element
        result.push_back(str);
        return result;
    }

    while ((pos = str.find(separator)) != std::string::npos) {
        result.push_back(str.substr(0, pos));
        str.erase(0, pos + separatorLength);
    }

    // Add the remaining part of the string
    if (!str.empty()) {
        result.push_back(str);
    }

    return result;
}

struct AppInstance
{
    int monitorIdx = 0;
    std::string title = "";
    std::string wclass = "";
    bool fullscreen = false;
    int pid = -1;
};

struct AppEntry
{
    int count_instances = 0;
    bool isPinned = false;
    std::string name = "";
    std::string execCmd = "";
    std::string iconPath = "";
    std::string desktopFile = "";
    std::vector <AppInstance> instances = {};
};

std::vector<std::filesystem::path> getDesktopFileSearchPaths()
{
	const char * XDG_DATA_HOME = getenv("XDG_DATA_HOME");
	const char * XDG_DATA_DIRS = getenv("XDG_DATA_DIRS");

    // Standard locations for desktop files
    std::vector<std::filesystem::path> searchPaths = {
        "/usr/share/applications",
        "/usr/local/share/applications",
        Glib::get_home_dir() + "/.local/share/applications",
        Glib::get_home_dir() + "/.local/share/flatpak/exports/share/applications",
        "/var/lib/flatpak/exports/share/applications"
    };
    
    if (XDG_DATA_HOME != NULL)
    {
		searchPaths.push_back(std::string(XDG_DATA_HOME) + "/applications");
	}

    if (XDG_DATA_DIRS != NULL)
    {
        for (std::string& dir : splitStr(XDG_DATA_DIRS, ":"))
        {
            if (dir[dir.size()-1] == '/') searchPaths.push_back(dir + "applications");
            else searchPaths.push_back(dir + "/applications");
        }
    }

    return searchPaths;
}

static std::vector<std::filesystem::path> searchPaths = getDesktopFileSearchPaths(); 

std::vector<std::filesystem::path> findDesktopFiles() {
    std::vector<std::filesystem::path> desktopFiles;

    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path)) {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.path().extension() == ".desktop") {
                    desktopFiles.push_back(entry.path());
                }
            }
        }
    }
    
    return desktopFiles;
}

static std::vector<std::filesystem::path> DesktopFiles = findDesktopFiles();

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

std::string cleanExecCommand(const std::string& cmd) {
    std::string result = cmd;
    // Remove % parameters
    size_t percent = result.find('%');
    if (percent != std::string::npos) {
        result = result.substr(0, percent);
    }
    // Remove leading/trailing whitespace
    result = trim(result);
    return result;
}

std::string findIconPath(const std::string& iconName) {
    // Check if it's already an absolute path
    if (std::filesystem::exists(iconName)) {
        return iconName;
    }

    // Try theme icons first
    try {
        auto iconTheme = Gtk::IconTheme::get_for_display(Gdk::Display::get_default());
        auto iconInfo = iconTheme->lookup_icon(iconName, 48);
        return iconInfo->get_file()->get_path();
    } catch (...) {
        // Fallback to common paths
        std::vector<std::string> extensions = {".png", ".svg", ".xpm"};
        std::vector<std::filesystem::path> searchPaths = {
            "/usr/share/pixmaps",
            "/usr/share/icons/hicolor/48x48/apps",
            "/usr/share/icons/hicolor/scalable/apps",
            "/usr/share/icons/Adwaita/48x48/apps",
            "/usr/share/icons"
        };

        for (const auto& path : searchPaths) {
            if (!std::filesystem::exists(path)) continue;
            
            for (const auto& ext : extensions) {
                std::filesystem::path iconPath = path / (iconName + ext);
                if (std::filesystem::exists(iconPath)) {
                    return iconPath.string();
                }
            }
        }
    }
    return "";
}

AppEntry parseDesktopFile(const std::filesystem::path& desktopFile) {
    AppEntry entry;
    //entry.desktopFile = desktopFile.string();
    
    std::ifstream file(desktopFile);
    std::string line;
    bool mainSection = false;
    
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        
        if (line == "[Desktop Entry]") {
            mainSection = true;
            continue;
        } else if (line[0] == '[') {
            mainSection = false;
            continue;
        }
        
        if (!mainSection) continue;
        
        size_t delim = line.find('=');
        if (delim == std::string::npos) continue;
        
        std::string key = line.substr(0, delim);
        std::string value = line.substr(delim + 1);
        
        if (key == "Name") {
            entry.name = value;
        } else if (key == "Exec") {
            entry.execCmd = cleanExecCommand(value);
        } else if (key == "Icon") {
            entry.iconPath = findIconPath(value);
        }
    }
    
    return entry;
}

std::string exec(const std::string& command)
{
    char buffer[256];
    std::string result = "";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "popen failed!";
    }
    try {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    int status = pclose(pipe);
    if (status == -1) {
        return "Error closing the pipe!";
    }
    return result;
}

std::vector<AppInstance> current_instances = {};
std::atomic<bool> running(true);

std::vector<AppInstance> getRunningInstances()
{
    std::vector<AppInstance> inst = {};
    
    char buffer[256];
    
    FILE* pipe = popen("bash conf/list_windows.bash", "r");
    if (!pipe) {
        std::cout << "popen failed!" << std::endl;
        return inst;
    }

    try {
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            std::vector<std::string> s = splitStr(buffer, "-:-");
            inst.push_back( { std::stoi(s[0]), s[1], s[2], (bool)std::stoi(s[3]), std::stoi(s[4]) } );
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    
    pclose(pipe);
    return inst;
}

std::string to_lower(const std::string& s) {
    std::string result;
    std::transform(s.begin(), s.end(), std::back_inserter(result),
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string normalizeString(const std::string& input) {
    std::string result;
    
    for (char c : input) {
        // Convert to lowercase
        char lower = tolower(c);
        
        // Keep only alphanumeric characters
        if (isalnum(lower)) {
            result += lower;
        }
    }
    
    return result;
}

bool find_case_insensitive(const std::string& str, const std::string& substr) {
    std::string lower_str = normalizeString(str);
    std::string lower_sub = normalizeString(substr);

    return (lower_str.find(lower_sub) != std::string::npos);
}

bool equals_case_insensitive(const std::string& str1, const std::string& str2)
{
    return (to_lower(str1) == to_lower(str2));
}

/*
struct DesktopMatchScore {
    int score = 0;
    std::string path;
    
    bool operator<(const DesktopMatchScore& other) const {
        // Higher scores first, then prefer shorter paths (more specific)
        return score != other.score ? score > other.score 
                                   : path.length() < other.path.length();
    }
};

std::string lastResortFindDesktopfile(const std::vector<AppInstance>& instances) {
    const auto& instance = instances[0];
    std::string title = instance.title;
    std::string wclass = instance.wclass;

    std::string exeName;
    
    // Get process information
    std::string cmdlinePath = "/proc/" + std::to_string(instance.pid) + "/cmdline";
    std::ifstream cmdlineFile(cmdlinePath);
    if (cmdlineFile) {
        std::string cmdline;
        std::getline(cmdlineFile, cmdline);
        exeName = std::filesystem::path(cmdline.substr(0, cmdline.find('\0'))).filename().string();
    }

    std::vector<DesktopMatchScore> scoredMatches;

    for (const auto& desktopFile : DesktopFiles) {
        DesktopMatchScore match;
        match.path = desktopFile.string();
        
        auto desktopInfo = parseDesktopFile(desktopFile);
        std::string desktopName = desktopFile.stem().string();
        std::string Name = desktopInfo.name;
        std::string desktopExe = desktopInfo.execCmd;

        
        if (desktopExe.empty()) {
            continue;
        }

        // Window title matching (lower weight as it's less reliable)
        if (!title.empty()) {
            if (find_case_insensitive(Name, title) != std::string::npos) match.score += 10;
            if (find_case_insensitive(desktopName, title) != std::string::npos) match.score += 40;
        }
        
        // Window class matching (medium weight)
        if (!wclass.empty()) {
            if (find_case_insensitive(Name, wclass) != std::string::npos) match.score += 30;
            if (find_case_insensitive(desktopName, wclass) != std::string::npos) match.score += 40;
        }
        
        // Executable matching (highest weight)
        if (!exeName.empty()) {
            // Exact match of executable name
            if (equals_case_insensitive(desktopExe, exeName)) match.score += 100;
            
            // Contains match
            if (find_case_insensitive(desktopExe, exeName) != std::string::npos) match.score += 10;
            if (find_case_insensitive(desktopName, exeName) != std::string::npos) match.score += 10;
            
            // Bonus for matching the basename exactly
            if (equals_case_insensitive(std::filesystem::path(desktopExe).stem().string(), exeName)) {
                match.score += 15;
            }
        }

        if (match.score > 0) {
            scoredMatches.push_back(match);
        }
    }

    if (!scoredMatches.empty()) {
        std::sort(scoredMatches.begin(), scoredMatches.end());
        return scoredMatches[0].path;
    }

    return "";
}


std::string lastResortFindDesktopfile(const std::vector<AppInstance>& instances)
{
    std::string title = instances[0].title;
   
    std::vector <std::string> dFiles = {};

    for (const auto& desktopFile : DesktopFiles)
    {
        std::string desktopName = desktopFile.stem().string();
        std::string Name = parseDesktopFile(desktopFile).name;
        
        if (!title.empty() && 
            (find_case_insensitive(Name, title) != std::string::npos || 
            find_case_insensitive(desktopName, title) != std::string::npos)) {
            
            
            dFiles.push_back(desktopFile);
        }
    }

    //assumes all instances have smae pid

    // Get process information
    std::string cmdlinePath = "/proc/" + std::to_string(instances[0].pid) + "/cmdline";
    std::ifstream cmdlineFile(cmdlinePath);
    std::string cmdline;
    std::getline(cmdlineFile, cmdline);
    
    // Clean up the command line
    std::string exeName = std::filesystem::path(cmdline.substr(0, cmdline.find('\0'))).filename().string();
    
    for (const auto& desktopFile : DesktopFiles)
    {
        std::string desktopName = desktopFile.stem().string();
        std::string desktopExe = parseDesktopFile(desktopFile).execCmd;
        
        if (!desktopExe.empty() && 
            find_case_insensitive(desktopExe, exeName) != std::string::npos || 
            find_case_insensitive(desktopName, exeName) != std::string::npos) {
            
            dFiles.push_back(desktopFile);
        }
    }



    return (dFiles.size() == 0) ? "" : dFiles[0];
}*/

bool fieldIsRelevant(const std::string& line)
{
    return (line.substr(0, 7) != "Comment" && line.substr(0, 4) != "Type" && line.substr(0, 8) != "MimeType");
}

std::string getSmallestString(const std::vector<std::string>& strings)
{
    int ind = 0;
    int size = 0;
    
    for (int i = 0; i < strings.size(); i++)
    {
        std::string str = strings[i];
        if (str.size() > size)
        {
            ind = i;
            size = str.size();
        }
    }

    return strings[ind];
}

std::string getDesktopFileOfInstances(const std::vector<AppInstance>& instances)
{
    std::string wclass = instances[0].wclass;
    std::string title = instances[0].title;
    std::vector <std::string> lastFiles = {};

    for (const auto& path : searchPaths)
    {
        if (std::filesystem::exists(path / ((wclass) + ".desktop")))
        {
            return (path / ((wclass) + ".desktop")).string();
        }
        else if (std::filesystem::exists(path / (to_lower(wclass) + ".desktop")))
        {
            return (path / (to_lower(wclass) + ".desktop")).string();
        }
    }

    for (const auto& desktopFile : DesktopFiles)
    {
        std::string desktopFileName = desktopFile.stem().string();
        bool addedToLastResort = false;

        if (find_case_insensitive(desktopFileName, wclass))
        {
            return desktopFile;
        }
    
        if (!addedToLastResort && find_case_insensitive(desktopFile.stem().string(), splitStr(title, " ")[0]))
        {
            lastFiles.emplace_back(desktopFile);
            addedToLastResort = true;
        }

    }

    // heuristic smallest filename is best
    return (lastFiles.size() > 0) ? getSmallestString(lastFiles) : "";
}

std::vector<AppEntry> getEntries()
{
    std::vector<AppEntry> res = {};
    // (class, entry)
    std::unordered_map<std::string, AppEntry> entries = {};
    if (current_instances.size() == 0) current_instances = getRunningInstances();

    for (AppInstance& inst : current_instances)
    {
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

void openInstance(AppInstance i)
{
    if (wayland)
    {
        std::system(((std::string)"ADDRESS=$(hyprctl -j clients | jq -r '.[] | select(.title == \"" + i.title + "\") | .address') && hyprctl dispatch focuswindow \"address:$ADDRESS\"").c_str());
    } else
    {
        std::system(("wmctrl -a \"" + i.title + "\"").c_str());
    }
}

void closeInstance(std::vector<AppInstance> instances)
{
    if (wayland)
    {
        if (instances.size() > 1)
        {
            std::system(((std::string)"for addr in $(hyprctl -j clients | jq -r '.[] | select(.class == \"" + instances[0].wclass + "\").address'); do hyprctl dispatch closewindow class:\"^(" + instances[0].wclass + ")$\" && sleep 0.01s; done").c_str());
        } else
        {
            std::system(((std::string)"hyprctl dispatch closewindow title:\"^(" + instances[0].title + ")$\" && sleep 0.01s").c_str());
        }
        return;
    }
    for (AppInstance& i : instances)
    {
        std::system(((std::string)"xdotool search --name \"" + i.title + "\" windowclose && sleep 0.01s").c_str());
    }
}


class Win : public Gtk::Window 
{
    public:
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

            std::vector <AppEntry> entries = {};
        } appCtx;

        
        enum class DockState { Hidden, Visible, Hiding, Showing};
        DockState state = DockState::Visible;
        DockState wanted_state = DockState::Hidden;
        int64_t timeWhenMouseLeftDock = 0;
        int timeout = 300;
        int duration = 300;

        Win(int argc, char **argv)
        {
            {
                appCtx.icon_size = 48;
                appCtx.padding = 5;
                appCtx.hotspot_height = 5;
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
                }
                
                appCtx.drawLauncher = true;
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
            }

            if (wayland)
            {    
                GLS_setup_top_layer_bottomEdge(this, appCtx.displayIdx, "GTKDock");
            } else
            {
                signal_realize().connect(sigc::mem_fun(*this, &Win::on_realizeX));
            }
                
            set_default_size(appCtx.winW, appCtx.winH);
            set_title("GTKDock");
            buildDock();

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
                    if (std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count() - this->timeWhenMouseLeftDock > this->timeout)
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
                    if (!animateOut( frame_time_ms / duration )) this->state = Win::DockState::Hidden;
                }

                if (state == Win::DockState::Showing)
                {   
                    if (!animateIn( frame_time_ms / duration )) this->state = Win::DockState::Visible;
                }

                return true;
            });

            add_controller(motion_controllerWin);

            // relying on polling because i havent found a wm agnostic way to poll fow window client changes
            Glib::signal_timeout().connect([this]() {
                updateDock();
                return true;  // Keep the timer running
            }, 500);
            
        }

        void updateDock() {
            //int64_t t0 = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();
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

                popovers.clear();
                widget_positions.clear();
                appCtx.entries = newEntries;

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

            //std::cout << std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count() - t0 << std::endl;
        }

        void buildDock()
        {
            dock_box = Gtk::Fixed();
            container = Gtk::Fixed();
            dock_box.get_style_context()->add_class("dock");


            GdkRectangle dock = {(int)((appCtx.winW - appCtx.dockW) * 0.5), appCtx.padding, appCtx.dockW, appCtx.dockH};
            double sx = appCtx.padding;
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
            
            container.put(dock_box, 0, 0);
            set_child(container);
        }

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
            
            // Now safely remove all children
            while (auto* child = dock_box.get_first_child()) {
                dock_box.remove(*child);
            }
            widget_positions.clear();
        }

        struct point { double x, y; };

        int offset_y = 0;
        std::vector<point> widget_positions = {};

        void add_widget_to_dock_box(Gtk::Widget& w, double x, double y)
        {
            dock_box.put(w, x, y);
            widget_positions.push_back({x,y});
        }

        void moveToOffset()
        {
            dock_box.set_margin_top(offset_y);
        }

        std::vector<Gtk::Popover *> popovers;
        std::vector<Gtk::Popover *> popoversofpopovers;

        Gtk::Fixed dock_box;
        Gtk::Fixed container;
        float t1 = 0;
        float t2 = 0;
        
        bool animateOut(float delta)
        {
            if (t1 <= 1)
            {
                if (wayland)
                {
                    GLS_chngMargin(this, - this->appCtx.winH * t1);
                }
                else
                {
                    offset_y = this->appCtx.winH * t1;
                    moveToOffset();
                }
            
                t1 += delta;
            }
            else
            {
                if (wayland)
                {
                    GLS_chngMargin(this, -this->appCtx.winH);
                }
                else
                {
                    offset_y = this->appCtx.winH;
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
                    GLS_chngMargin(this, -this->appCtx.winH * (1 - t2));
                }
                else
                {
                    offset_y = this->appCtx.winH * (1 - t2);
                    moveToOffset();
                }

                t2 += delta;
            }
            else
            {
                if (wayland)
                {
                    GLS_chngMargin(this, 0);
                }
                else
                {
                    offset_y = 0;
                    moveToOffset();
                }

                t2 = 0;
                return false;
            }

            return true;  
        }

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

        Gtk::Popover * get_Menu(AppEntry& e)
        {
            auto m_popover = Gtk::make_managed<Gtk::Popover>();
            popovers.push_back(m_popover);
            m_popover->set_size_request(3*appCtx.icon_bg_size, -1);
            m_popover->set_expand(false);

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
                    this->get_application()->quit();
                });

                button2->add_css_class("mbutton");
                m_popover_box->append(*button2);

            }
            
            return m_popover;
        }

        std::vector<AppEntry> loadEntries()
        {
            std::vector<AppEntry> entries = getEntries();

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
                    0, true, "Launcher", "nwg-drawer", "imgs/launcher.png"
                } );
            }

            return entries;
        }

        void on_realizeX()
        {
            onrealizeXDock(this, appCtx.displayIdx, appCtx.winW, appCtx.winH);
        }
};

// wayland only cus X11 window repositioning is too slow so just move all window contents down
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
            
            GLS_setup_top_layer_bottomEdge(this, mon, "GTKDock");

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

                if (last_x <= 5 || last_x >= win->appCtx.winW - 5)
                {
                    if (win->state == Win::DockState::Visible || win->state == Win::DockState::Showing)
                    {
                        win->timeWhenMouseLeftDock = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();
                        win->wanted_state = Win::DockState::Hidden;
                    }
                }
            });
            
            motion->signal_leave().connect([] () {});
            add_controller(motion);
        
            set_decorated(false);
            set_default_size(win->appCtx.winW, win->appCtx.hotspot_height);
            set_title("hotspot");
            add_css_class("hotspot");
            win->property_default_width().signal_changed().connect([this](){
                int w,h;
                this->win->get_default_size(w, h);
                this->set_default_size(w, this->win->appCtx.hotspot_height);
            });
        }
};

int main (int argc, char **argv)
{
    wayland = (strcmp(std::getenv("XDG_SESSION_TYPE"), "wayland") == 0);
    auto app = Gtk::Application::create();

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