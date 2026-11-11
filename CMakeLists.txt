cmake_minimum_required(VERSION 3.16)
project(Adderall VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ADDERALL_BUILD_APP "Build the SDL3 application" ON)
option(ADDERALL_BUILD_TESTS "Build the tests" ON)
option(ADDERALL_FETCH_DEPS "Download and build SDL3 / SDL3_ttf when they are not installed" ON)

if(MSVC)
  add_compile_options(/W3 /utf-8)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# Core: everything that does not need a window. Fully covered by the tests.
# ---------------------------------------------------------------------------
add_library(adderall_core STATIC
  src/date.c
  src/raster.c
  src/board.c
  src/model.c
  src/hotkeys.c
  src/format.c
  src/platform.c
  src/store.c
)
target_include_directories(adderall_core PUBLIC include)
if(UNIX)
  target_link_libraries(adderall_core PUBLIC m)
endif()

# ---------------------------------------------------------------------------
# The application.
# ---------------------------------------------------------------------------
if(ADDERALL_BUILD_APP)
  find_package(SDL3 CONFIG QUIET)
  find_package(SDL3_ttf CONFIG QUIET)

  if(NOT SDL3_FOUND OR NOT SDL3_ttf_FOUND)
    if(NOT ADDERALL_FETCH_DEPS)
      message(FATAL_ERROR "SDL3 and SDL3_ttf were not found. Install them, or use -DADDERALL_FETCH_DEPS=ON.")
    endif()
    include(FetchContent)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
    set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
    set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
    set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-3.4.8
      GIT_SHALLOW TRUE)
    FetchContent_Declare(SDL3_ttf
      GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
      GIT_TAG release-3.2.2
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
    # ALIAS targets only exist inside the directory that made them, so repeat
    # the two SDL_ttf looks for here, where SDL_ttf's directory can see them.
    if(NOT TARGET SDL3::SDL3)
      add_library(SDL3::SDL3 ALIAS SDL3-static)
    endif()
    if(NOT TARGET SDL3::Headers)
      add_library(SDL3::Headers ALIAS SDL3_Headers)
    endif()
    FetchContent_MakeAvailable(SDL3_ttf)
  endif()

  add_executable(adderall WIN32 MACOSX_BUNDLE
    src/main.c
    src/gfx.c
    src/ui.c
    src/app.c
  )
  if(TARGET SDL3_ttf::SDL3_ttf)
    set(ADDERALL_TTF_TARGET SDL3_ttf::SDL3_ttf)
  else()
    set(ADDERALL_TTF_TARGET SDL3_ttf-static)
  endif()
  target_link_libraries(adderall PRIVATE adderall_core ${ADDERALL_TTF_TARGET} SDL3::SDL3)
  target_compile_definitions(adderall PRIVATE
    ADDERALL_VERSION="${PROJECT_VERSION}"
    ADDERALL_SOURCE_DATA_DIR="${CMAKE_SOURCE_DIR}/data")

  # Bundled resources (font, ...) live in a "data" folder next to the program.
  add_custom_command(TARGET adderall POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:adderall>/data)

  install(TARGETS adderall RUNTIME DESTINATION bin BUNDLE DESTINATION .)
  install(DIRECTORY data DESTINATION bin)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(ADDERALL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
