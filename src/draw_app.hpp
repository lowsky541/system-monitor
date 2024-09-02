#ifndef __IMPRINT_HPP__
#define __IMPRINT_HPP__

#include "refresh_data.hpp"
#include "utilities.hpp"
#include <imgui.h>

void draw_app_system_window(std::shared_ptr<RefreshData> data);
void draw_app_storage_window(std::shared_ptr<RefreshData> data);
void draw_app_network_window(std::shared_ptr<RefreshData> data);
void draw_app(std::shared_ptr<RefreshData> data, ImVec2& display);

#endif