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

enum class DockEdge
{
    EDGELEFT = 0,
    EDGETOP,
    EDGERIGHT,
    EDGEBOTTOM
};

// utility functions

// split string ex. "a-b-c-d" into {"a", "b", "c", "d"}
std::vector<std::string> splitStr(std::string str, std::string separator);

// gets all paths which have .desktop files
std::vector<std::filesystem::path> getDesktopFileSearchPaths();

// never changes so make it a static vriable
static std::vector<std::filesystem::path> searchPaths = getDesktopFileSearchPaths(); 

// find all desktop files in searchPaths
std::vector<std::filesystem::path> findDesktopFiles();

// never changes so make it a static vriable
static std::vector<std::filesystem::path> DesktopFiles = findDesktopFiles();

//removes leading and trailing whitespace characters (spaces and tabs) from a given string
std::string trim(const std::string& str);

std::string cleanExecCommand(const std::string& cmd);

std::string findIconPath(const std::string& iconName);

AppEntry parseDesktopFile(const std::filesystem::path& desktopFile);

std::string exec(const std::string& command);

// parses result of list_windows.bash into vector of AppInstance
std::vector<AppInstance> getRunningInstances();

// string chars to lower case
std::string to_lower(const std::string& s);

// to_lower + no special chars ex. "SomeVery-weirdApP-name-or-class" --> "someveryweirdappnameorclass"
std::string normalizeString(const std::string& input);

// find if normalizeString(substr) is found in normalizeString(str)
bool find_case_insensitive(const std::string& str, const std::string& substr);

std::string getSmallestString(const std::vector<std::string>& strings);

// finds .desktop file of instances using various heuristics
std::string getDesktopFileOfInstances(const std::vector<AppInstance>& instances);

bool getIfThisIsOnlyInstance();

std::string getRes(std::string file);
