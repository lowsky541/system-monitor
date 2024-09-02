#include "imgui.h"
#include "imgui_freetype.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <numeric>

#include "draw_app.hpp"
#include "seguiemj.font.hpp"

const float REFRESH_RATE = 1.0f;

static void glfw_error_callback(int error, const char* description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**) {
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
  // GL ES 2.0 + GLSL 100
  const char* glsl_version = "#version 100";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
  // GL 3.2 + GLSL 150
  const char* glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
#else
  // GL 3.0 + GLSL 130
  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+
  // only glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // 3.0+ only
#endif

  GLFWwindow* window =
      glfwCreateWindow(1280, 720, "system-monitor", nullptr, nullptr);
  if (window == nullptr)
    return 1;

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = NULL;

  ImGui::StyleColorsClassic();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  io.Fonts->AddFontDefault();
  static ImWchar ranges[] = {0x1, 0x1FFFF, 0};
  static ImFontConfig cfg;
  cfg.OversampleH = cfg.OversampleV = 1;
  cfg.MergeMode = true;
  cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
  io.Fonts->AddFontFromMemoryCompressedBase85TTF(
      SegoeUIEmoji_compressed_data_base85, 13.0f, &cfg, ranges);

  ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 0.0f);

  std::shared_ptr<RefreshData> refresh_data_ptr = RefreshData::init();

  RefreshData* rd = refresh_data_ptr.get();
  rd->refresh_operating_system();
  rd->refresh_user();
  rd->refresh_hostname();
  rd->refresh_cpu_info();

  assert(rd->setup_refresh_battery("/sys/class/power_supply/BAT0/energy_now",
                                   "/sys/class/power_supply/BAT0/energy_full",
                                   "/sys/class/power_supply/BAT0/status"));
  assert(rd->setup_refresh_thermal("/sys/class/thermal/thermal_zone0/temp"));
  assert(rd->setup_refresh_fan("/sys/class/hwmon/hwmon5/fan1_input"));
  assert(rd->setup_proc());

  rd->refresh_cpu_stat(true);
  rd->refresh_cpu_graph_stat(true);
  rd->refresh_processes(true);
  rd->refresh_battery_full();

  float refresh_rate = REFRESH_RATE, refresh_accumulator = refresh_rate;
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    if (refresh_accumulator >= refresh_rate) {
      RefreshData* refresh_data = refresh_data_ptr.get();
      refresh_data->refresh_cpu_stat();
      refresh_data->refresh_memory();
      refresh_data->refresh_storages();
      refresh_data->refresh_interfaces();

      // Do not refresh the processes if there is a selection
      if (refresh_data->processes_selection.size() == 0) {
        refresh_data->refresh_processes();
      }

      if (!rd->graph.animated) {
        refresh_data->refresh_battery();
        refresh_data->refresh_thermal();
        refresh_data->refresh_fan();
      }

      refresh_accumulator = 0.0;
    }
    refresh_accumulator += io.DeltaTime;

    if (rd->graph.refresh_accumulator >= (1.f / rd->graph.fps) &&
        rd->graph.animated) {
      RefreshData* refresh_data = refresh_data_ptr.get();
      refresh_data->refresh_cpu_graph_stat();
      refresh_data->refresh_battery();
      refresh_data->refresh_thermal();
      refresh_data->refresh_fan();

      rd->graph.refresh_accumulator = 0.0;
    }
    rd->graph.refresh_accumulator += io.DeltaTime;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImVec2 display = io.DisplaySize;
    draw_app(refresh_data_ptr, display);
    ImGui::Render();

    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                 clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
