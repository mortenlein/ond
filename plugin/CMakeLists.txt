# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Morten Lein
# Ond as a plugin (VST3, CLAP) with DPF. Built by tools/build_plugin.sh (the generated headers in build/gen first).
cmake_minimum_required(VERSION 3.15)
project(ond-plugin C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OND_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_subdirectory(${OND_ROOT}/third_party/DPF ${CMAKE_BINARY_DIR}/dpf)

dpf_add_plugin(Ond
  TARGETS vst3 clap
  UI_TYPE opengl
  FILES_DSP
      OndPlugin.cpp
      ${OND_ROOT}/host/ond_engine.c
  FILES_UI
      OndUI.cpp
      OndPresets.cpp)

# (DPF's opengl UI adds its Windows WebView though USE_WEB_VIEW is off: its if() sees the generator expression as
# true. We have no web view; the file does not build with llvm-mingw's headers. Left out.)
set_source_files_properties(${OND_ROOT}/third_party/DPF/dgl/src/WebViewWin32.cpp PROPERTIES HEADER_FILE_ONLY TRUE)
foreach(prop COMPILE_DEFINITIONS INTERFACE_COMPILE_DEFINITIONS)
  get_target_property(defs dgl-opengl ${prop})
  if(defs)
    list(REMOVE_ITEM defs DGL_USE_WEB_VIEW)
    set_target_properties(dgl-opengl PROPERTIES ${prop} "${defs}")
  endif()
endforeach()

target_include_directories(Ond PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OND_ROOT}/host
  ${OND_ROOT}/build/gen
  ${OND_ROOT}/firmware/src
  ${OND_ROOT}/firmware/hal)

# the firmware's sources as they are (their own warnings are the FM-1 build's business)
set_source_files_properties(${OND_ROOT}/host/ond_engine.c PROPERTIES COMPILE_OPTIONS "-w;-O2")

if(WIN32)
  # one self-contained DLL: no libc++ / libunwind DLLs beside it for Live to look for
  target_link_options(Ond-vst3 PRIVATE -static)
  target_link_options(Ond-clap PRIVATE -static)
  # the presets' folder (OndPresets.cpp: Documents, Explorer)
  target_link_libraries(Ond-vst3 PRIVATE shell32 ole32 uuid)
  target_link_libraries(Ond-clap PRIVATE shell32 ole32 uuid)
endif()
