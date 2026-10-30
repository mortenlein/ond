# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Morten Lein
# Ond as a plugin (VST3, CLAP) with DPF. Built by tools/build_plugin.sh (the generated headers in build/gen first).
cmake_minimum_required(VERSION 3.15)
project(ond-plugin C CXX)

set(OND_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_subdirectory(${OND_ROOT}/third_party/DPF ${CMAKE_BINARY_DIR}/dpf)

dpf_add_plugin(Ond
  TARGETS vst3 clap
  FILES_DSP
      OndPlugin.cpp
      ${OND_ROOT}/host/ond_engine.c)

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
endif()
