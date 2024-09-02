#include "draw_app.hpp"

typedef void (*DrawWindowCB)(std::shared_ptr<RefreshData>);

void draw_app_system_window(std::shared_ptr<RefreshData> data) {
  ImGui::Text("Operating System: %s", data->operating_system.c_str());
  ImGui::Text("Hostname: %s", data->hostname.c_str());
  ImGui::Text("User: %s", data->user.c_str());
  ImGui::Text("Working processes: %d", data->processes.processes.size());
  ImGui::Text("CPU: %s", data->cpu_info.c_str());

  ImGui::Separator();

  if (ImGui::BeginTabBar("##Tabs", 0)) {

    if (ImGui::BeginTabItem("CPU")) {
      ImGui::Checkbox("Animate", &data->graph.animated);
      ImGui::SliderFloat("FPS", &data->graph.fps, 1.0f, 60.0f);
      ImGui::SliderFloat("Scale", &data->graph.yscale, 0.0f, 100.0f);

      ImGui::Separator();

      char overlay[255];
      sprintf(overlay, "CPU avg: %.0f%%",
              average(data->cpu_graph.values.data(),
                      data->cpu_graph.values.size()));
      ImGui::PlotLines("CPU", data->cpu_graph.values.data(),
                       data->cpu_graph.values.size(), 0, overlay, 0,
                       data->graph.yscale, ImVec2(0, 160.0f));

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Battery")) {
      ImGui::Text("Status: %s", data->battery.status.c_str());
      ImGui::Text("Capacity: %d A/h [%d mA/h]", data->battery.now / 1000,
                  data->battery.full);
      ImGui::Text("Current charge: %d A/h [%d mA/h]", data->battery.now / 1000,
                  data->battery.now);

      ImGui::Separator();

      ImGui::Checkbox("Animate", &data->graph.animated);
      ImGui::SliderFloat("FPS", &data->graph.fps, 1.0f, 60.0f);
      ImGui::SliderFloat("Scale", &data->graph.yscale, 0.0f, 100.0f);

      ImGui::Separator();

      char overlay[255];
      sprintf(overlay, "Battery: %.2f%%", data->battery.values.back());
      ImGui::PlotLines("Battery", data->battery.values.data(),
                       data->battery.values.size(), 0, overlay, 0,
                       data->graph.yscale, ImVec2(0, 160.0f));

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Fan")) {

      ImGui::Checkbox("Animate", &data->graph.animated);
      ImGui::SliderFloat("FPS", &data->graph.fps, 1.0f, 60.0f);
      ImGui::SliderFloat("Scale", &data->graph.yscale, 0.0f, 100.0f);

      ImGui::Separator();

      char overlay[255];
      sprintf(overlay, "Fan: %.2f RPM", data->fan.values.back());
      ImGui::PlotLines("Fan", data->fan.values.data(), data->fan.values.size(),
                       0, overlay, 0, data->graph.yscale, ImVec2(0, 160.0f));

      ImGui::TextColored(
          ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
          "Thrice upon a time, the fan wasn't there but now there is one ! "
          "Lot of 🍪s !");

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Thermal")) {
      ImGui::Checkbox("Animate", &data->graph.animated);
      ImGui::SliderFloat("FPS", &data->graph.fps, 1.0f, 60.0f);
      ImGui::SliderFloat("Scale", &data->graph.yscale, 0.0f, 100.0f);

      ImGui::Separator();

      char overlay[255];
      sprintf(overlay, "Thermal: %.2f°C", data->thermal.values.back());
      ImGui::PlotLines("Thermal", data->thermal.values.data(),
                       data->thermal.values.size(), 0, overlay, 0,
                       data->graph.yscale, ImVec2(0, 160.0f));

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }
}

void draw_app_storage_window(std::shared_ptr<RefreshData> data_ptr) {
  RefreshData* data = data_ptr.get();

  if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextWrapped("Physical (RAM):");
    ImGui::ProgressBar(data->memory.phys_percent, ImVec2(0.0f, 0.0f));
    ImGui::SameLine();
    ImGui::TextWrapped("%s / %s",
                       human_readable(data->memory.phys_used).c_str(),
                       human_readable(data->memory.phys_total).c_str());

    ImGui::TextWrapped("Virtual (SWAP):");
    ImGui::ProgressBar(data->memory.virt_percent, ImVec2(0.0f, 0.0f));
    ImGui::SameLine();
    ImGui::TextWrapped("%s / %s",
                       human_readable(data->memory.virt_used).c_str(),
                       human_readable(data->memory.virt_total).c_str());
  }

  if (ImGui::CollapsingHeader("Storage", ImGuiTreeNodeFlags_DefaultOpen)) {
    for (const auto storage : data->storages) {
      ImGui::TextWrapped("HDD/SSD [%s]:", storage.device.c_str());
      ImGui::ProgressBar(storage.percent, ImVec2(0.0f, 0.0f));
      ImGui::SameLine();
      ImGui::TextWrapped("%s / %s", human_readable(storage.used).c_str(),
                         human_readable(storage.total).c_str());
    }
  }

  if (ImGui::CollapsingHeader("Processes")) {
    ImGui::InputText("##processes_filter", data->processes_filter,
                     IM_ARRAYSIZE(data->processes_filter), 0);

    ImGui::SameLine();

    char label[50];
    snprintf(label, 50, "cls (%d/%s)", data->processes_selection.size(),
             data->processes_selection.size() != 0 ? "p" : "rt");

    if (ImGui::Button(label)) {
      if (data->processes_selection.size() == 0) {
        ImGui::OpenPopup("Information");
      } else {
        data->processes_selection.clear();
      }
    }

    std::vector<process_t> filtered_processes;

    for (auto process : data->processes.processes) {
      if (process.name.find(data->processes_filter) != std::string::npos)
        filtered_processes.push_back(process);
    }

    if (ImGui::BeginTable("##processes", 5,
                          ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Pid");
      ImGui::TableSetupColumn("Name");
      ImGui::TableSetupColumn("State");
      ImGui::TableSetupColumn("CPU %");
      ImGui::TableSetupColumn("Mem %");
      ImGui::TableHeadersRow();

      for (const auto& process : filtered_processes) {
        const bool item_is_selected =
            std::find(data->processes_selection.begin(),
                      data->processes_selection.end(),
                      process.pid) != data->processes_selection.end();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        char pid_label[6];
        snprintf(pid_label, 6, "%d", process.pid);
        if (ImGui::Selectable(pid_label, item_is_selected,
                              ImGuiSelectableFlags_SpanAllColumns)) {
          if (ImGui::GetIO().KeyCtrl) {
            if (item_is_selected)
              data->processes_selection.erase(
                  std::remove(data->processes_selection.begin(),
                              data->processes_selection.end(), process.pid),
                  data->processes_selection.end());
            else
              data->processes_selection.push_back(process.pid);
          } else {
            data->processes_selection.clear();
            data->processes_selection.push_back(process.pid);
          }
        }

        ImGui::TableSetColumnIndex(1);
        if (process.pid == data->pid)
          ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                             process.name.c_str());
        else
          ImGui::Text("%s", process.name.c_str());

        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%c", process.state);

        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.1f%%", process.cpu);

        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.1f%%", process.mem);
      }

      ImGui::EndTable();
    }
  }

  if (ImGui::BeginPopupModal("Information")) {
    ImGui::Text("There is actually no selected processes.");
    if (ImGui::Button("Close"))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}

void draw_app_network_window(std::shared_ptr<RefreshData> data) {
  if (ImGui::CollapsingHeader("Interfaces", ImGuiTreeNodeFlags_None)) {
    if (ImGui::BeginTable("##nics", 4, ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Interface name");
      ImGui::TableSetupColumn("Protocol");
      ImGui::TableSetupColumn("Address");
      ImGui::TableSetupColumn("State");
      ImGui::TableHeadersRow();

      for (const auto& interface : data->network.interfaces) {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%s", interface.name.c_str());

        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%s", interface.is_ipv6 ? "Ipv6" : "Ipv4");

        ImGui::TableSetColumnIndex(2);
        if (ImGui::Selectable(interface.addr.c_str())) {
          ImGui::SetClipboardText(interface.addr.c_str());
        }

        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%s", interface.is_up ? "Up" : "Down");
      }

      ImGui::EndTable();
    }
  }

  if (ImGui::CollapsingHeader("Receive (RX) / Transmit (TX)",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::TreeNode("Receive (RX)")) {
      if (ImGui::BeginTable("##rx", 9, ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Interface");
        ImGui::TableSetupColumn("Bytes");
        ImGui::TableSetupColumn("Packets");
        ImGui::TableSetupColumn("Errors");
        ImGui::TableSetupColumn("Dropped");
        ImGui::TableSetupColumn("FIFO");
        ImGui::TableSetupColumn("Frame");
        ImGui::TableSetupColumn("Compressed");
        ImGui::TableSetupColumn("Multicast");
        ImGui::TableHeadersRow();

        for (const auto& interface : data->network.interfaces) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::Text("%s", interface.name.c_str());

          for (int i = 0; i < 8; i++) {
            uint32_t v = interface.values[i];
            ImGui::TableSetColumnIndex(i + 1);
            ImGui::Text("%d", v);
          }
        }
        ImGui::EndTable();
      }

      ImGui::TreePop();
    }

    if (ImGui::TreeNode("Visual Receive (RX)")) {
      for (const auto& interface : data->network.interfaces) {
        ImGui::Text("%s", interface.name.c_str());

        const auto total = interface.values.front();
        const std::string total_hr = human_readable_megabyte(total);

        char overlay[50];
        sprintf(overlay, "%s / 2 GB", total_hr.c_str());
        ImGui::ProgressBar(total / 2e+9, ImVec2(-1, 0), overlay);
      }

      ImGui::TreePop();
    }

    if (ImGui::TreeNode("Transmit (TX)")) {
      if (ImGui::BeginTable("##tx", 9, ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Interface");
        ImGui::TableSetupColumn("Bytes");
        ImGui::TableSetupColumn("Packets");
        ImGui::TableSetupColumn("Errors");
        ImGui::TableSetupColumn("Dropped");
        ImGui::TableSetupColumn("FIFO");
        ImGui::TableSetupColumn("Collisions");
        ImGui::TableSetupColumn("Carrier");
        ImGui::TableSetupColumn("Compressed");
        ImGui::TableHeadersRow();

        for (const auto& interface : data->network.interfaces) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::Text("%s", interface.name.c_str());

          for (int i = 8; i < 16; i++) {
            uint32_t v = interface.values[i];
            ImGui::TableSetColumnIndex((i - 8) + 1);
            ImGui::Text("%d", v);
          }
        }
        ImGui::EndTable();
      }

      ImGui::TreePop();
    }

    if (ImGui::TreeNode("Visual Transmit (TX)")) {
      for (const auto& interface : data->network.interfaces) {
        ImGui::Text("%s", interface.name.c_str());

        const auto total = interface.values.at(8);
        const std::string total_hr = human_readable_megabyte(total);

        char overlay[50];
        sprintf(overlay, "%s / 2 GB", total_hr.c_str());
        ImGui::ProgressBar(total / 2e+9, ImVec2(-1, 0), overlay);
      }

      ImGui::TreePop();
    }
  }
}

static void draw_app_window(std::shared_ptr<RefreshData> data, const char* n,
                            ImVec2 s, ImVec2 p, DrawWindowCB cb) {
  ImGui::Begin(n, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
  ImGui::SetWindowSize(n, s);
  ImGui::SetWindowPos(n, p);
  cb(data);
  ImGui::End();
};

void draw_app(std::shared_ptr<RefreshData> data, ImVec2& display) {
  draw_app_window(data, "System",
                  ImVec2((display.x / 2) - 10, (display.y / 2) + 30),
                  ImVec2(10, 10), draw_app_system_window);
  draw_app_window(data, "Storage and Processes",
                  ImVec2((display.x / 2) - 20, (display.y / 2) + 30),
                  ImVec2((display.x / 2) + 10, 10), draw_app_storage_window);
  draw_app_window(data, "Network", ImVec2(display.x - 20, (display.y / 2) - 60),
                  ImVec2(10, (display.y / 2) + 50), draw_app_network_window);
}
