cmake_minimum_required(VERSION 3.13)

project(magicseteditor VERSION 2.6.4)

add_definitions(-DUNOFFICIAL_BUILD)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Upstream 2.6 wants wx >= 3.3.3. This fork's desktop build is only a test host
# (ADR 0007), so it accepts the system wx 3.2 and bridges the gap in
# src/util/wx_compat.hpp.
find_package(wxWidgets 3.2 CONFIG QUIET COMPONENTS core base net)
if(NOT wxWidgets_FOUND)
  find_package(wxWidgets 3.2 MODULE REQUIRED COMPONENTS core base net)
endif()
# Boost >= 1.70 ships BoostConfig.cmake; CMake 4 deprecates the FindBoost module (CMP0167).
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()
find_package(Boost REQUIRED COMPONENTS regex json)

# minizip-ng (third_party/minizip-ng, a submodule pinned to 4.2.2) reads and writes
# the zip packages (src/util/io/zip.cpp). Deflate through the system zlib only: no
# other codecs, no encryption, no compat layer, nothing fetched, nothing installed.
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/third_party/minizip-ng/CMakeLists.txt")
  message(FATAL_ERROR "third_party/minizip-ng is empty: run git submodule update --init third_party/minizip-ng")
endif()
find_package(ZLIB REQUIRED)
# Let the plain variables below set its options and cache entries.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)
set(MZ_COMPAT OFF)
set(MZ_ZLIB ON)
set(MZ_ZLIB_FLAVOR "zlib")
set(MZ_BZIP2 OFF)
set(MZ_LZMA OFF)
set(MZ_PPMD OFF)
set(MZ_ZSTD OFF)
set(MZ_LIBCOMP OFF)
set(MZ_FETCH_LIBS OFF)
set(MZ_FORCE_FETCH_LIBS OFF)
set(MZ_PKCRYPT OFF)
set(MZ_WZAES OFF)
set(MZ_OPENSSL OFF)
set(MZ_LIBBSD OFF)
set(MZ_ICONV OFF)
set(MZ_BUILD_TESTS OFF)
set(MZ_BUILD_UNIT_TESTS OFF)
set(MZ_BUILD_FUZZ_TESTS OFF)
set(SKIP_INSTALL_ALL ON)
add_subdirectory(third_party/minizip-ng EXCLUDE_FROM_ALL)

find_package(PkgConfig)

# find hunspell
pkg_check_modules(HUNSPELL hunspell)
if( NOT HUNSPELL_FOUND )
	message("-- Cannot find Hunspell via pkg-config, checking directly...")
	find_path(HUNSPELL_INCLUDE_DIRS hunspell.hxx)
	message("-- HUNSPELL_INCLUDE_DIRS = ${HUNSPELL_INCLUDE_DIRS}")
	find_library(HUNSPELL_LIBRARIES NAMES hunspell libhunspell)
	message("-- HUNSPELL_LIBRARIES = ${HUNSPELL_LIBRARIES}")
	if ( ${HUNSPELL_INCLUDE_DIRS} STREQUAL "HUNSPELL_INCLUDE_DIRS-NOTFOUND" )
		message(FATAL_ERROR "Hunspell cannot be found")
	else()
		message("-- Found Hunspell at ${HUNSPELL_LIBRARIES}")
	endif()
endif()

# You will most likely get a message about being unable to open hunspell-1.7.lib because pkgconf forgets to add the actual path to HUNSPELL_LIBRARIES.
# If so, uncomment the below line and point it to the correct vcpkg root folder/library. (For debug builds, add "debug\\" just before "lib\\")
#set(HUNSPELL_LIBRARIES "C:\\PATH\\TO\\ROOT\\vcpkg\\installed\\${VCPKG_TARGET_TRIPLET}\\lib\\hunspell-1.7.lib")

include_directories("${PROJECT_BINARY_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${HUNSPELL_INCLUDE_DIRS})

# The core: everything under src/ except the desktop program (gui/, cli/, main.cpp) and the
# facade. An object library, so every object file (and its static initialisers) goes into each
# program that uses it, exactly as when the core was compiled into the executable.

file(GLOB_RECURSE core_sources src/*.cpp)
list(FILTER core_sources EXCLUDE REGEX "/src/(gui|cli|facade)/")
list(FILTER core_sources EXCLUDE REGEX "/src/main\\.cpp$")
list(FILTER core_sources EXCLUDE REGEX "/src/gfx/skia/")
add_library(mse_core OBJECT ${core_sources})
target_link_libraries(mse_core PUBLIC wxWidgets::wxWidgets Boost::regex Boost::json MINIZIP::minizip ${HUNSPELL_LIBRARIES})
target_precompile_headers(mse_core PRIVATE src/util/prec.hpp)

# The Skia backends (ADR 0004), built from the pinned submodules by tools/skia/build.sh. Skia's
# headers need C++20, so the files that include them are a separate C++20 library. With
# MSE_SKIA_CODEC the desktop decodes and encodes images through Skia instead of wx (#41).
option(MSE_SKIA_CODEC "Decode and encode images with Skia (needs tools/skia/build.sh linux-x64)" OFF)
set(MSE_SKIA_LIB_DIR "${PROJECT_SOURCE_DIR}/out/skia/linux-x64" CACHE PATH "Skia build output (tools/skia/build.sh)")
if(MSE_SKIA_CODEC)
  if(NOT EXISTS "${MSE_SKIA_LIB_DIR}/libskia.a")
    message(FATAL_ERROR "No Skia build in ${MSE_SKIA_LIB_DIR}: run tools/skia/build.sh linux-x64")
  endif()
  # Only std and Skia headers in here, no MSE ones (gfx/skia/skia_codec.hpp)
  add_library(mse_skia STATIC src/gfx/skia/skia_codec.cpp)
  set_target_properties(mse_skia PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  target_compile_options(mse_skia PRIVATE -fno-rtti) # as Skia itself (it subclasses SkWStream)
  target_include_directories(mse_skia PRIVATE "${PROJECT_SOURCE_DIR}/third_party/skia")
  foreach(lib skia png jpeg freetype2 zlib skcms)
    target_link_libraries(mse_skia PUBLIC "${MSE_SKIA_LIB_DIR}/lib${lib}.a")
  endforeach()
  # Keep Skia's libpng/libjpeg/zlib symbols out of the dynamic symbol table, so wx's shared
  # libraries keep using the system's
  target_link_options(mse_skia INTERFACE "LINKER:--exclude-libs,ALL")
  target_sources(mse_core PRIVATE src/gfx/skia/skia_image_codec.cpp)
  target_link_libraries(mse_core PUBLIC mse_skia)
  target_compile_definitions(mse_core PUBLIC MSE_SKIA_CODEC)
endif()

# On wxGTK the shared font store's files are given to fontconfig with wxFont::AddPrivateFont, and
# Pango has to be told to reread the configuration afterwards (gfx/wx_canvas.cpp)
if(UNIX AND NOT APPLE AND PkgConfig_FOUND)
  pkg_check_modules(PANGOFC IMPORTED_TARGET pangocairo pangofc)
  if(PANGOFC_FOUND)
    set_source_files_properties(src/gfx/wx_canvas.cpp PROPERTIES
      INCLUDE_DIRECTORIES "${PANGOFC_INCLUDE_DIRS}" COMPILE_DEFINITIONS MSE_HAVE_PANGOFC)
    target_link_libraries(mse_core PUBLIC PkgConfig::PANGOFC)
  endif()
endif()

# The wx-free facade over the core (ADR 0006, ADR 0009). Its public header uses only std types;
# a host (the desktop test host below, later Android) installs the drawing and image backends.

file(GLOB facade_sources src/facade/*.cpp)
add_library(mse_facade OBJECT ${facade_sources})
target_link_libraries(mse_facade PUBLIC mse_core)
target_precompile_headers(mse_facade REUSE_FROM mse_core)

# Magic Set Editor executable

add_executable(magicseteditor WIN32)
target_link_libraries(${PROJECT_NAME} PRIVATE mse_core)

file(GLOB_RECURSE sources src/gui/*.cpp src/cli/*.cpp)
list(FILTER sources EXCLUDE REGEX win32_cli_wrapper.cpp)
target_sources(magicseteditor PRIVATE ${sources} src/main.cpp)
target_precompile_headers(magicseteditor REUSE_FROM mse_core)

# Headless tests of the facade, on a desktop host that installs the wx backends (test/tests.cmake)

add_executable(facade_tests test/facade/facade_tests.cpp src/facade/desktop/desktop_host.cpp)
target_link_libraries(facade_tests PRIVATE mse_facade mse_core)
target_precompile_headers(facade_tests REUSE_FROM mse_core)

# The Skia image codec against the wx one (#41)
if(MSE_SKIA_CODEC)
  add_executable(image_codec_tests test/image_codec/image_codec_tests.cpp)
  target_link_libraries(image_codec_tests PRIVATE mse_core)
  target_precompile_headers(image_codec_tests REUSE_FROM mse_core)
endif()

configure_file(src/config.hpp.in src/config.hpp)

# resource file

target_sources(magicseteditor PRIVATE resource/win32_res.rc)

# Halian trying Mac stuff

if(BUILD_APPLE_BUNDLE)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/magicseteditor.app/Contents/PlugIns")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
  set_target_properties(
  "magicseteditor"
    PROPERTIES
    MACOSX_BUNDLE_NAME                             "magicseteditor"
    MACOSX_BUNDLE_VERSION                          "${PROJECT_VERSION}"
    MACOSX_BUNDLE_GUI_IDENTIFIER                   "org.magicseteditor.gui"
    MACOSX_BUNDLE_ICON_FILE                        "mse.icns"
    MACOSX_BUNDLE_INFO_PLIST                       "MacOSXBundleInfo.plist.in"
    MACOSX_BUNDLE                                  "TRUE"
    # MACHO_COMPATIBILITY_VERSION                  "12.0.0"
    RESOURCE                                       "${RESOURCE_FILES}"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY             ""
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED          NO
    XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT       dwarf
    XCODE_ATTRIBUTE_GCC_INLINES_ARE_PRIVATE_EXTERN YES
    XCODE_ATTRIBUTE_CLANG_LINK_OBJC_RUNTIME        NO
  )
  add_custom_command(
    TARGET "magicseteditor"
    POST_BUILD
    COMMAND dylibbundler ARGS -od -ns -b -x "${CMAKE_BINARY_DIR}/magicseteditor.app/Contents/MacOS/magicseteditor" -d "${CMAKE_BINARY_DIR}/magicseteditor.app/Contents/libs"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Changing library search path from absolute to relative…"
  )
  add_custom_command(
    TARGET "magicseteditor"
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/magicseteditor.app/Contents/Resources"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PROJECT_SOURCE_DIR}/mse.icns" "${CMAKE_BINARY_DIR}/magicseteditor.app/Contents/Resources/mse.icns"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PROJECT_SOURCE_DIR}/mse-set.icns" "${CMAKE_BINARY_DIR}/magicseteditor.app/Contents/Resources/mse-set.icns"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Copying icons…"
  )
  add_custom_command(
    TARGET "magicseteditor"
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/magicseteditor.app/Contents/SharedSupport"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_SOURCE_DIR}/resource" "${CMAKE_BINARY_DIR}/magicseteditor.app/Contents/SharedSupport/resource"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Copying application resources…"
  )
  add_custom_command(
    TARGET "magicseteditor"
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -DBUNDLE_PATH="${CMAKE_BINARY_DIR}/magicseteditor.app" -P "${PROJECT_SOURCE_DIR}/cmake/SignAppleBundle.cmake"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Ad-hoc signing application bundle…"
  )
endif()

# magicseteditor.com: wrapper to enable command line executable on windows

if(WIN32)
  add_executable(magicseteditor-cli)
  target_sources(magicseteditor-cli PRIVATE src/cli/win32_cli_wrapper.cpp)
  add_custom_command(TARGET magicseteditor-cli POST_BUILD COMMAND
    ${CMAKE_COMMAND} -E copy "${PROJECT_BINARY_DIR}/magicseteditor-cli.exe" "${PROJECT_BINARY_DIR}/magicseteditor.com"
  )
endif()

# Magic Set Editor Updater executable: needs wx >= 3.3 (wxWebRequestSync,
# SetAppearance) and self-updates a desktop install, so it is not built for the
# wx 3.2 test host.

if(wxWidgets_VERSION VERSION_GREATER_EQUAL 3.3)
  add_executable(magicseteditor-updater WIN32)
  target_link_libraries(magicseteditor-updater PRIVATE wxWidgets::wxWidgets)
  target_sources(magicseteditor-updater PRIVATE src_updater/main.cpp src_updater/win32_res.rc src_updater/updater.ico)
endif()

# warnings
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Update if necessary
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wsuggest-override -Wstrict-null-sentinel -Wno-comment -Wno-unused-parameter")
endif()

# visual studio debugger
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT magicseteditor)

# Static linking on windows
if (${VCPKG_TARGET_TRIPLET} MATCHES ".*-static")
  message("Static linking")
  set(CompilerFlags CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE)
  foreach(CompilerFlag ${CompilerFlags})
    string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
  endforeach()
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /GL")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
  # Defines
  add_definitions(-DSTATIC)
  add_definitions(-DHUNSPELL_STATIC)
endif()

# Test suite
include(test/tests.cmake)

# Debug Message
message("-- Does this have a full path? If not, and it's just a file name, it's broken: ${HUNSPELL_LIBRARIES}")
