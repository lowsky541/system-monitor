#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "imgui_freetype.h"

#include <SDL2/SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL2/SDL_opengles2.h>
#else
#include <SDL2/SDL_opengl.h>
#endif

#include <iostream>
#include <array>
#include <numeric>

#include "humanize.hpp"
#include "system.hpp"
#include "storage.hpp"
#include "network.hpp"
#include "processes.hpp"
#include "utilities.hpp"

#include "seguiemj.font.hpp"

// NOTES
//  sm = System monitor
//  gr = graphs

pid_t PID = getpid();

// --- cpu times for current cycle and last cycle (see refresh rate) ---
System::proc::cpu_stat_t sm_cpu_stat_past, sm_cpu_stat_present;
System::proc::cpu_stat_t gr_cpu_stat_past, gr_cpu_stat_present;
std::map<pid_t, process_t> sm_processes_past, sm_processes;

char processes_filter[64] = "";
ImVector<pid_t> processes_selection;

// --- fixed refresh interval ---
// Refresh the data every 1 second (cpu, processes, etc...)
float sm_refresh_rate = 1.0f, sm_refresh_accu = 0.0f;
std::vector<Network::Interface> sm_interfaces;
std::map<std::string, std::array<uint32_t, 16>> sm_rxtx;

// --- shared graph variables ---
bool gr_animate = true;
float gr_yscale = 10.0;
float gr_fps = 10.0f, gr_accu = 0.0f;

uint64_t power_now = 0.0f;
uint64_t power_full = 0.0f;

float thermal_max = 100.0f;

std::array<float, 60> values;

std::array<float, 60> cpu_values;
std::array<float, 60> power_values;
std::array<float, 60> thermal_values;

struct GraphSine
{
    int frequency;
    int amplitude;
} harmony = {.frequency = 440, .amplitude = 500};

// systemWindow, display information for the System monitorization
void systemWindow(const char *id, ImVec2 size, ImVec2 position)
{
    ImGui::Begin(id);
    ImGui::SetWindowSize(id, size);
    ImGui::SetWindowPos(id, position);

    ImGui::Text("Operating System: %s", OperatingSystem().c_str());
    ImGui::Text("Computer name: %s", Hostname().c_str());
    ImGui::Text("Logged user: %s", LoggedUser().c_str());
    ImGui::Text("Working processes: %d", sm_processes.size());
    ImGui::Text("CPU: %s", CPUInfo().c_str());

    ImGui::Separator();

    if (ImGui::BeginTabBar("##Tabs", 0))
    {
        if (ImGui::BeginTabItem("CPU"))
        {
            ImGui::Checkbox("Animate", &gr_animate);
            ImGui::SliderFloat("FPS", &gr_fps, 1.0f, 60.0f);
            ImGui::SliderFloat("Scale", &gr_yscale, 0.0f, 100.0f);

            ImGui::Separator();

            char overlay[255];
            sprintf(overlay, "CPU avg: %.0f%%", average(cpu_values.data(), cpu_values.size()));
            ImGui::PlotLines("CPU", cpu_values.data(), cpu_values.size(), 0, overlay, 0, gr_yscale, ImVec2(0, 160.0f));

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Power"))
        {
            ImGui::Text("Status: %s", System::Power::Status().c_str());
            ImGui::Text("Capacity: %d A/h [%d mA/h]", power_full / 1000, power_full);
            ImGui::Text("Current charge: %d A/h [%d mA/h]", power_now / 1000, power_now);

            ImGui::Separator();

            ImGui::Checkbox("Animate", &gr_animate);
            ImGui::SliderFloat("FPS", &gr_fps, 1.0f, 60.0f);
            ImGui::SliderFloat("Scale", &gr_yscale, 0.0f, 100.0f);

            ImGui::Separator();

            char overlay[255];
            sprintf(overlay, "Power: %.2f%%", power_values.back());
            ImGui::PlotLines("Power", power_values.data(), power_values.size(), 0, overlay, 0, gr_yscale, ImVec2(0, 160.0f));

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Fan"))
        {
            ImGui::Text("Sorry! There is no fan on this computer !!!");
            ImGui::Text("Well........ there probably is one... well, very certainly...");
            ImGui::Text("But, there is no sensor to read any value of speed from.");
            ImGui::Text("So... sorry, lad. No fan for ya...");
            ImGui::TextColored(
                ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                "Please ! Take a cookie... 🍪");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Thermal"))
        {
            ImGui::Checkbox("Animate", &gr_animate);
            ImGui::SliderFloat("FPS", &gr_fps, 1.0f, 60.0f);
            ImGui::SliderFloat("Scale", &gr_yscale, 0.0f, thermal_max);

            ImGui::Separator();

            char overlay[255];
            sprintf(overlay, "Thermal: %.2f°C", thermal_values.back());
            ImGui::PlotLines("Thermal", thermal_values.data(), thermal_values.size(), 0, overlay, 0, gr_yscale, ImVec2(0, 160.0f));

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(".sinf()"))
        {
            constexpr int values_max = 500;
            ImGui::SliderInt("Amplitude", &harmony.amplitude, 1, values_max);
            ImGui::SliderInt("Frequency", &harmony.frequency, 1, 1500);

            static float values[values_max] = {};
            static int values_offset = 0, values_count = 0;
            static double refresh_time = 0.0;
            if (!gr_animate || refresh_time == 0.0)
                refresh_time = ImGui::GetTime();

            while (refresh_time < ImGui::GetTime()) // Create data at fixed 60 Hz rate for the demo
            {
                values[values_offset] = harmony.amplitude * sinf(values_count * (M_PI * 2) / harmony.frequency);
                values_offset = (values_offset + 1) % IM_ARRAYSIZE(values);
                values_count++;

                refresh_time += 1.0f / 60.0f;
            }

            ImGui::PlotLines("sinf()", values, IM_ARRAYSIZE(values), values_offset, NULL, -values_max, values_max, ImVec2(0, 100.0f));

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(".rand()"))
        {
            ImGui::Checkbox("Animate", &gr_animate);
            ImGui::SliderFloat("FPS", &gr_fps, 1.0f, 60.0f);
            ImGui::SliderFloat("Scale", &gr_yscale, 1.0f, 100.0f);

            ImGui::Separator();

            char overlay[255];
            sprintf(overlay, "rand(): %.0f%%", values.back());
            ImGui::PlotLines("rand()", values.data(), values.size(), 0, overlay, 0, gr_yscale, ImVec2(0, 160.0f));

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void memoryProcessesWindow(const char *id, ImVec2 size, ImVec2 position)
{
    ImGui::Begin(id);
    ImGui::SetWindowSize(id, size);
    ImGui::SetWindowPos(id, position);

    if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen))
    {
        System::StorageStats physical_stats, virtual_stats;
        System::Memory::GetStatistics(physical_stats, virtual_stats);

        ImGui::TextWrapped("Physical (RAM):");
        ImGui::ProgressBar(physical_stats.fraction, ImVec2(0.0f, 0.0f));
        ImGui::SameLine();
        ImGui::TextWrapped("%s / %s", human_readable(physical_stats.used).c_str(), human_readable(physical_stats.total).c_str());

        ImGui::TextWrapped("Virtual (SWAP):");
        ImGui::ProgressBar(virtual_stats.fraction, ImVec2(0.0f, 0.0f));
        ImGui::SameLine();
        ImGui::TextWrapped("%s / %s", human_readable(virtual_stats.used).c_str(), human_readable(virtual_stats.total).c_str());
    }

    if (ImGui::CollapsingHeader("Storage", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto stats = System::Disk::GetAllStatistics();
        for (std::map<std::string, System::StorageStats>::iterator it = stats.begin(); it != stats.end(); it++)
        {
            ImGui::TextWrapped("HDD/SSD [%s]:", it->first.c_str());
            ImGui::ProgressBar(it->second.fraction, ImVec2(0.0f, 0.0f));
            ImGui::SameLine();
            ImGui::TextWrapped("%s / %s", human_readable(it->second.used).c_str(), human_readable(it->second.total).c_str());
        }
    }

    if (ImGui::CollapsingHeader("Processes"))
    {
        ImGui::InputText("##processes_filter", processes_filter, IM_ARRAYSIZE(processes_filter), 0);
        ImGui::SameLine();

        char label[50];
        snprintf(label, 50, "cls (%d/%s)", processes_selection.size(), processes_selection.size() != 0 ? "p" : "rt");
        if (ImGui::Button(label)) 
        {
            if (processes_selection.size() == 0) {
                ImGui::OpenPopup("Information");
            } else {
                processes_selection.clear();
            }
        }

        auto processes = sm_processes;
        for (auto it = processes.begin(); it != processes.end();)
        {
            if (it->second.name.find(processes_filter) == std::string::npos)
                processes.erase(it++);
            else
                ++it;
        }


        if (ImGui::BeginTable("##processes", 5, ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders))
        {
            ImGui::TableSetupColumn("Pid");
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("State");
            ImGui::TableSetupColumn("CPU %");
            ImGui::TableSetupColumn("Mem %");
            ImGui::TableHeadersRow();

            for (const auto &[pid, prc] : processes)
            {
                const bool item_is_selected = processes_selection.contains(pid);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                char pid_label[6];
                snprintf(pid_label, 6, "%d", pid);
                if (ImGui::Selectable(pid_label, item_is_selected, ImGuiSelectableFlags_SpanAllColumns))
                {
                    if (ImGui::GetIO().KeyCtrl)
                    {
                        if (item_is_selected)
                            processes_selection.find_erase_unsorted(pid);
                        else
                            processes_selection.push_back(pid);
                    }
                    else
                    {
                        processes_selection.clear();
                        processes_selection.push_back(pid);
                    }
                }

                ImGui::TableSetColumnIndex(1);
                if (prc.pid == PID)
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), prc.name.c_str());
                else
                    ImGui::Text("%s", prc.name.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%c", prc.state);

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.1f%%", prc.usage.cpu);

                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.1f%%", prc.usage.mem);
            }

            ImGui::EndTable();
        }
    }

    if (ImGui::BeginPopupModal("Information"))
    {
        ImGui::Text("There is actually no selected processes.");
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::End();
}

// network, display information network information
void networkWindow(const char *id, ImVec2 size, ImVec2 position)
{
    ImGui::Begin(id);
    ImGui::SetWindowSize(id, size);
    ImGui::SetWindowPos(id, position);

    if (ImGui::CollapsingHeader("Interfaces", ImGuiTreeNodeFlags_None))
    {
        if (ImGui::BeginTable("##nics", 4, ImGuiTableFlags_Borders))
        {
            ImGui::TableSetupColumn("Interface name");
            ImGui::TableSetupColumn("Protocol");
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("State");
            ImGui::TableHeadersRow();

            for (const Network::Interface &itfc : sm_interfaces)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", itfc.name.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", itfc.isIpv6 ? "Ipv6" : "Ipv4");

                ImGui::TableSetColumnIndex(2);
                if (ImGui::Selectable(itfc.addr.c_str()))
                {
                    ImGui::SetClipboardText(itfc.addr.c_str());
                }

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", itfc.isUp ? "Up" : "Down");
            }

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Receive (RX) / Transmit (TX)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::TreeNode("Receive (RX)"))
        {
            if (ImGui::BeginTable("##rx", 9, ImGuiTableFlags_Borders))
            {
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

                for (const std::pair<const std::string, std::array<uint32_t, 16>> &nd : sm_rxtx)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", nd.first.c_str());

                    for (int i = 0; i < 8; i++)
                    {
                        uint32_t v = nd.second[i];
                        ImGui::TableSetColumnIndex(i + 1);
                        ImGui::Text("%d", v);
                    }
                }
                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Visual Receive (RX)"))
        {
            for (const std::pair<const std::string, std::array<uint32_t, 16>> &nd : sm_rxtx)
            {
                ImGui::Text("%s", nd.first.c_str());

                const auto total = nd.second.front();
                const std::string total_hr = human_readable_megabyte(total);

                char overlay[50];
                sprintf(overlay, "%s / 2 GB", total_hr.c_str());
                ImGui::ProgressBar(total/2e+9, ImVec2(-1, 0), overlay);
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Transmit (TX)"))
        {
            if (ImGui::BeginTable("##tx", 9, ImGuiTableFlags_Borders))
            {
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

                for (const std::pair<const std::string, std::array<uint32_t, 16UL>> &nd : sm_rxtx)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", nd.first.c_str());

                    for (int i = 8; i < 16; i++)
                    {
                        uint32_t v = nd.second[i];
                        ImGui::TableSetColumnIndex((i - 8) + 1);
                        ImGui::Text("%d", v);
                    }
                }
                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Visual Transmit (TX)"))
        {
            for (const std::pair<const std::string, std::array<uint32_t, 16>> &nd : sm_rxtx)
            {
                ImGui::Text("%s", nd.first.c_str());

                const auto total = nd.second.at(8);
                const std::string total_hr = human_readable_megabyte(total);

                char overlay[50];
                sprintf(overlay, "%s / 2 GB", total_hr.c_str());
                ImGui::ProgressBar(total/2e+9, ImVec2(-1, 0), overlay);
            }

            ImGui::TreePop();
        }
    }

    ImGui::End();
}

int main(int, char **)
{
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow("system-monitor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL;

    // ImGui::StyleColorsDark();
    ImGui::StyleColorsClassic();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    io.Fonts->AddFontDefault();
    static ImWchar ranges[] = { 0x1, 0x1FFFF, 0 };
    static ImFontConfig cfg;
    cfg.OversampleH = cfg.OversampleV = 1;
    cfg.MergeMode = true;
    cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
    io.Fonts->AddFontFromMemoryCompressedBase85TTF(SegoeUIEmoji_compressed_data_base85, 13.0f, &cfg, ranges);

    ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 0.0f);

    auto cpu_sensor = System::Thermal::FindCPUSensor();
    System::Thermal::CPUTemperatureMax(cpu_sensor, thermal_max);

    const long pages = sysconf(_SC_PHYS_PAGES);
    const long processors = sysconf(_SC_NPROCESSORS_ONLN);
    const long page_size = sysconf(_SC_PAGE_SIZE);
    const unsigned long long total_memory = pages * page_size;

    bool done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        gr_accu += io.DeltaTime;
        sm_refresh_accu += io.DeltaTime;

        if (gr_accu >= (1.f / gr_fps) && gr_animate)
        {
            // Graph => CPU
            System::proc::read_stat(gr_cpu_stat_present);
            const float cpu_usage = System::proc::cpu_diff(gr_cpu_stat_past, gr_cpu_stat_present, true);
            std::rotate(cpu_values.begin(), cpu_values.begin() + 1, cpu_values.end());
            cpu_values.back() = cpu_usage;
            gr_cpu_stat_past = gr_cpu_stat_present;

            // Graph => Power
            power_full = System::Power::ChargeFull();
            power_now = System::Power::ChargeNow();
            std::rotate(power_values.begin(), power_values.begin() + 1, power_values.end());
            power_values.back() = 100.00f * ((float)power_now / (float)power_full);

            // Graph => Thermal
            std::rotate(thermal_values.begin(), thermal_values.begin() + 1, thermal_values.end());
            thermal_values.back() = System::Thermal::CPUTemperature(cpu_sensor);

            // Graph => rand()
            std::rotate(values.begin(), values.begin() + 1, values.end());
            values.back() = rand() % 100 + 1;

            gr_accu = 0.0f;
        }

        if (sm_refresh_accu >= sm_refresh_rate)
        {
            time_t tm;
            time(&tm);
            printf("UP %ld\n", tm);

            System::proc::read_stat(sm_cpu_stat_present);

            // Processes
            if (processes_selection.size() == 0)
            {
                auto sm_processes_present = System::Processes();
                sm_processes = intersect_apply<pid_t, process_t>(sm_processes_past, sm_processes_present, [&](const process_t &past, const process_t &pres) -> process_t
                                                                 {
                uint32_t cpu_times_past = past.proc.pstat.utime + past.proc.pstat.stime;
                uint32_t cpu_times_pres = pres.proc.pstat.utime + pres.proc.pstat.stime;

                float cpu_usage = (processors * (cpu_times_pres - cpu_times_past) * 100) / (float)(sm_cpu_stat_present.total - sm_cpu_stat_past.total);
                float resident = pres.proc.pstat.rss * page_size;
                float mem_usage = 100 * (resident / total_memory);

                process_t p = pres;
                p.usage.cpu = ceilf32(cpu_usage);
                p.usage.mem = mem_usage;
                return p; });

                sm_processes_past = sm_processes_present;
            }

            // Network interfaces
            sm_interfaces = Network::GetInterfaces();
            sm_cpu_stat_past = sm_cpu_stat_present;

            sm_rxtx = Network::NetworkDevices();

            sm_refresh_accu = 0.0f;
        }

        ImVec2 mainDisplay = io.DisplaySize;
        memoryProcessesWindow(
            "== Memory and Processes ==",
            ImVec2((mainDisplay.x / 2) - 20, (mainDisplay.y / 2) + 30),
            ImVec2((mainDisplay.x / 2) + 10, 10));

        systemWindow(
            "== System ==",
            ImVec2((mainDisplay.x / 2) - 10, (mainDisplay.y / 2) + 30),
            ImVec2(10, 10));

        networkWindow(
            "== Network ==",
            ImVec2(mainDisplay.x - 20, (mainDisplay.y / 2) - 60),
            ImVec2(10, (mainDisplay.y / 2) + 50));

        ImGui::Render();

        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
