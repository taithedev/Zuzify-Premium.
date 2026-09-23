cmake_minimum_required(VERSION 3.24)
project(ZuzifyPremium VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG master
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG develop
)
FetchContent_MakeAvailable(json)

set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_BACKENDS
  ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
  ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

add_executable(ZuzifyPremium WIN32
  src/main.cpp
  src/app.cpp
  src/app.hpp
  src/supabase.cpp
  src/supabase.hpp
  src/system_stats.cpp
  src/system_stats.hpp
  src/updater.cpp
  src/updater.hpp
  src/theme.hpp
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_BACKENDS}
)

target_include_directories(ZuzifyPremium PRIVATE
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(ZuzifyPremium PRIVATE
  ZUZIFY_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(ZuzifyPremium PRIVATE
  d3d11
  dxgi
  dwmapi
  winhttp
  version
  crypt32
  shell32
)

target_link_libraries(ZuzifyPremium PRIVATE nlohmann_json::nlohmann_json)

if (MSVC)
  target_compile_options(ZuzifyPremium PRIVATE /W4 /permissive- /EHsc)
endif()

set_property(TARGET ZuzifyPremium PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
