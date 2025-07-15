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

void onrealizeXDock(Gtk::Window * win, int dispIdx, int winW, int winH);

void GLS_setup_top_layer_bottomEdge(Gtk::Window * win, int dispIdx, const std::string& name);

void GLS_chngMargin(Gtk::Window * win, int newMargin);
