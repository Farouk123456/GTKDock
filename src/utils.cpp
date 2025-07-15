#include "utils.h"

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

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

std::string cleanExecCommand(const std::string& cmd) {
    std::string result = cmd;

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

bool getIfThisIsOnlyInstance()
{
    std::vector<pid_t> pids = {};
    
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (!entry.is_directory()) continue;
        
        // Check if directory name is a PID
        std::string dirName = entry.path().filename();
        if (std::all_of(dirName.begin(), dirName.end(), ::isdigit)) {
            pid_t pid = std::stoi(dirName);
            
            // Read the cmdline file
            std::ifstream cmdlineFile(entry.path() / "cmdline");
            std::string cmdline;
            if (cmdlineFile) {
                std::getline(cmdlineFile, cmdline);
                if (!cmdline.empty()) {
                    // cmdline is null-separated, take the first part
                    size_t nullPos = cmdline.find('\0');
                    if (nullPos != std::string::npos) {
                        cmdline = cmdline.substr(0, nullPos);
                    }
                    
                    // Get just the executable name
                    size_t slashPos = cmdline.rfind('/');
                    if (slashPos != std::string::npos) {
                        cmdline = cmdline.substr(slashPos + 1);
                    }
                    
                    if (cmdline == "GTKDock") {
                        pids.push_back(pid);
                    }
                }
            }
        }
    }
    
    return ((pids.size()) == 1);
}
