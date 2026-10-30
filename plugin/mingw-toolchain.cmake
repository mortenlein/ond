# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Morten Lein
# Windows x86-64 from WSL / Linux with llvm-mingw (github.com/mstorsjo/llvm-mingw): LLVM_MINGW, default ~/.llvm-mingw/current
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
if(DEFINED ENV{LLVM_MINGW})
  set(_lm $ENV{LLVM_MINGW})
else()
  set(_lm $ENV{HOME}/.llvm-mingw/current)
endif()
set(CMAKE_C_COMPILER ${_lm}/bin/x86_64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER ${_lm}/bin/x86_64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER ${_lm}/bin/x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH ${_lm}/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
