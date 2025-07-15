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

std::vector<std::string> splitStr(std::string str, std::string separator);

std::vector<std::filesystem::path> getDesktopFileSearchPaths();

static std::vector<std::filesystem::path> searchPaths = getDesktopFileSearchPaths(); 

std::vector<std::filesystem::path> findDesktopFiles();

static std::vector<std::filesystem::path> DesktopFiles = findDesktopFiles();

std::string trim(const std::string& str);

std::string cleanExecCommand(const std::string& cmd);

std::string findIconPath(const std::string& iconName);

AppEntry parseDesktopFile(const std::filesystem::path& desktopFile);

std::string exec(const std::string& command);

std::vector<AppInstance> getRunningInstances();

std::string to_lower(const std::string& s);

std::string normalizeString(const std::string& input);

bool find_case_insensitive(const std::string& str, const std::string& substr);

bool equals_case_insensitive(const std::string& str1, const std::string& str2);

bool fieldIsRelevant(const std::string& line);

std::string getSmallestString(const std::vector<std::string>& strings);

std::string getDesktopFileOfInstances(const std::vector<AppInstance>& instances);
