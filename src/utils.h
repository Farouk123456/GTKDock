#pragma once
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

struct AppInstance
{
    int monitorIdx = 0;
    std::string title = "";
    std::string wclass = "";
    bool fullscreen = false;
    int pid = -1;
};

struct DesktopEntry
{
    std::string name = "";
    std::string execCmd = "";
    std::string iconPath = "";
    std::string desktopFile = "";

    bool operator==(DesktopEntry& other) const {
        return (name == other.name && execCmd == other.execCmd);
    }
};

struct AppEntry
{
    int count_instances = 0;
    bool isPinned = false;
    DesktopEntry app;
    std::vector <AppInstance> instances = {};
};

enum class DockEdge
{
    EDGELEFT = 0,
    EDGETOP,
    EDGERIGHT,
    EDGEBOTTOM
};

enum class DockAlignment
{
    CENTER = 0,
    LEFT,
    TOP,
    RIGHT,
    BOTTOM
};

// utility functions

// split string ex. "a-b-c-d" into {"a", "b", "c", "d"}
std::vector<std::string> splitStr(std::string str, std::string separator);

// gets all paths which have .desktop files
std::vector<std::filesystem::path> getDesktopFileSearchPaths();

// never changes so make it a static vriable
static std::vector<std::filesystem::path> searchPaths = getDesktopFileSearchPaths(); 

// find all desktop files in searchPaths
std::vector<DesktopEntry> findDesktopFiles();

std::string cleanExecCommand(const std::string& cmd);

std::string findIconPath(const std::string& iconName);

DesktopEntry parseDesktopFile(const std::filesystem::path& desktopFile);

std::string exec(const std::string& command);

// parses result of list_windows.bash into vector of AppInstance
std::vector<AppInstance> getRunningInstances();

// find if normalizeString(substr) is found in normalizeString(str)
bool find_case_insensitive(const std::string& str, const std::string& substr);

std::string getSmallestString(const std::vector<std::string>& strings);

// finds .desktop file of instances using various heuristics
DesktopEntry getEntryOfInstances(const std::vector<AppInstance>& instances, std::vector<DesktopEntry> DesktopFiles);

bool getIfThisIsOnlyInstance();

std::string getRes(std::string file);
