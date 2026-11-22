# Mobile Set Editor - the app's native library (ADR 0006)
# SPDX-License-Identifier: GPL-3.0-or-later
#
# libmse.so: the repository's wx-free core and facade with the Skia backends (its CMakeLists.txt,
# built for Android), plus the JNI layer. Skia itself is built before this by the Gradle tasks
# buildSkia-<abi> (tools/skia/build.sh).

cmake_minimum_required(VERSION 3.22)
project(mse_app C CXX)

get_filename_component(MSE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
add_subdirectory("${MSE_ROOT}" mse EXCLUDE_FROM_ALL)

target_sources(mse PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/mse_jni.cpp")
target_link_libraries(mse PRIVATE jnigraphics)
