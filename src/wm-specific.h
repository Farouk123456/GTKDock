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
#include "utils.h"

void onrealizeXDock(Gtk::Window * win, int dispIdx, int winW, int winH, int edgeMargin, DockEdge edge);

void GLS_setup_top_layer(Gtk::Window * win, int dispIdx, int edgeMargin, const std::string& name, DockEdge edge);

void GLS_chngMargin(Gtk::Window * win, int newMargin, DockEdge edge);

void openInstance(AppInstance i);

void closeInstance(std::vector<AppInstance> instances);

bool check_layer_shell_support();

void populateInstanceMenuWithWMSpecific(Gtk::Box* popover_box, AppInstance inst);
