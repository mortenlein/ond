# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Morten Lein
# Ond as a plugin (VST3, CLAP) with DPF. Built by tools/build_plugin.sh (the generated headers in build/gen first).
cmake_minimum_required(VERSION 3.15)
project(ond-plugin C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OND_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_subdirectory(${OND_ROOT}/third_party/DPF ${CMAKE_BINARY_DIR}/dpf)

if(EMSCRIPTEN)                                    # (012: the web build: the standalone only, one Ond a page)
  set(OND_TARGETS jack)
else()
  set(OND_TARGETS vst3 clap jack)
endif()
dpf_add_plugin(Ond
  TARGETS ${OND_TARGETS}
  UI_TYPE opengl
  FORCE_NATIVE_AUDIO_FALLBACK                     # (the standalone Ond.exe: Windows' own audio and MIDI, no JACK)
  FILES_COMMON
      OndPresets.cpp
  FILES_DSP
      OndPlugin.cpp
      ${OND_ROOT}/host/ond_engine.c
  FILES_UI
      OndUI.cpp)

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

# (0.18) the engine as a library of its own: each Ond loads a copy (OndEngineLoader.hpp), its own globals, so any
# number of Onds play in one Live set. Next to each binary: in the VST3 bundle, beside the CLAP and Ond.exe
if(NOT EMSCRIPTEN)                                # (the web: one Ond a page, the plugin's own engine)
add_library(ond-engine SHARED ${OND_ROOT}/host/ond_engine.c)
set_target_properties(ond-engine PROPERTIES PREFIX "" OUTPUT_NAME "ond-engine"
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
target_include_directories(ond-engine PRIVATE ${OND_ROOT}/host ${OND_ROOT}/build/gen ${OND_ROOT}/firmware/src
  ${OND_ROOT}/firmware/hal)
target_compile_options(ond-engine PRIVATE -w -O2)
foreach(t Ond-vst3 Ond-clap Ond-jack)
  add_dependencies(${t} ond-engine)
endforeach()
add_custom_command(TARGET Ond-vst3 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ond-engine> $<TARGET_FILE_DIR:Ond-vst3>/)
endif()

if(WIN32)
  target_link_options(ond-engine PRIVATE -static)
  # one self-contained DLL: no libc++ / libunwind DLLs beside it for Live to look for
  target_link_options(Ond-vst3 PRIVATE -static)
  target_link_options(Ond-clap PRIVATE -static)
  # the presets' folder (OndPresets.cpp: Documents, Explorer)
  target_link_libraries(Ond-vst3 PRIVATE shell32 ole32 uuid)
  target_link_libraries(Ond-clap PRIVATE shell32 ole32 uuid)
  target_link_options(Ond-jack PRIVATE -static)
  target_link_libraries(Ond-jack PRIVATE shell32 ole32 uuid)
endif()
if(EMSCRIPTEN)                                    # (the web: memory grows as needed; fixed, the BEAT view ran out: OOM)
  target_link_options(Ond-jack PRIVATE -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864)
endif()
