cmake_minimum_required(VERSION 3.12.4)

project(lvgl C CXX)

# =============================================================================
# Project options
# =============================================================================

# -----------------------------------------------------------------------------
# LVGL
# -----------------------------------------------------------------------------

# LVGL version/tag to fetch from GitHub.
#
# Examples:
#   v9.6.0
#   v9.5.0
#   master
#
set(LVGL_VERSION
    "v9.6.0"
    CACHE STRING "LVGL Git tag/version to use")

# -----------------------------------------------------------------------------
# LVGL Pro
# -----------------------------------------------------------------------------

set(LVGL_PRO_PROJECT_DIR
    ""
    CACHE PATH "Path to LVGL Pro Project folder")

# -----------------------------------------------------------------------------
# FreeRTOS
# -----------------------------------------------------------------------------

set(FREERTOS_PORT
    GCC_POSIX
    CACHE STRING "Port for FreeRTOS on Posix environment")

option(
    USE_FREERTOS
    "Enable FreeRTOS"
    OFF
)

# -----------------------------------------------------------------------------
# LVGL features
# -----------------------------------------------------------------------------

option(
    LV_USE_DRAW_SDL
    "Use SDL draw unit"
    OFF
)

option(
    LV_USE_LIBPNG
    "Use libpng to decode PNG"
    OFF
)

option(
    LV_USE_LIBJPEG_TURBO
    "Use libjpeg turbo to decode JPEG"
    OFF
)

option(
    LV_USE_FFMPEG
    "Use FFmpeg to display video using lv_ffmpeg"
    OFF
)

option(
    LV_USE_FREETYPE
    "Use FreeType library"
    OFF
)

# -----------------------------------------------------------------------------
# Debugging
# -----------------------------------------------------------------------------

option(
    ASAN
    "Enable AddressSanitizer"
    OFF
)

# =============================================================================
# Language standards
# =============================================================================

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# Output paths
# =============================================================================

set(
    EXECUTABLE_OUTPUT_PATH
    ${PROJECT_SOURCE_DIR}/bin
)

set(
    WORKING_DIRECTORY
    ${PROJECT_SOURCE_DIR}
)

# =============================================================================
# FreeRTOS
# =============================================================================

if(USE_FREERTOS)

    message(STATUS "FreeRTOS is enabled")

    # -------------------------------------------------------------------------
    # FreeRTOS configuration
    # -------------------------------------------------------------------------

    add_library(
        freertos_config
        INTERFACE
    )

    target_include_directories(
        freertos_config
        SYSTEM
        INTERFACE
            ${PROJECT_SOURCE_DIR}/config
    )

    target_compile_definitions(
        freertos_config
        INTERFACE
            projCOVERAGE_TEST=0
    )

    # -------------------------------------------------------------------------
    # FreeRTOS source tree
    # -------------------------------------------------------------------------

    add_subdirectory(
        FreeRTOS
    )

    include_directories(
        ${PROJECT_SOURCE_DIR}/FreeRTOS/include
        ${PROJECT_SOURCE_DIR}/FreeRTOS/portable/ThirdParty/GCC/Posix
        ${PROJECT_SOURCE_DIR}/config
    )

    # -------------------------------------------------------------------------
    # FreeRTOS sources
    # -------------------------------------------------------------------------

    file(
        GLOB
        FREERTOS_SOURCES

        "${PROJECT_SOURCE_DIR}/FreeRTOS/*.c"
        "${PROJECT_SOURCE_DIR}/FreeRTOS/portable/MemMang/heap_4.c"
        "${PROJECT_SOURCE_DIR}/FreeRTOS/portable/ThirdParty/GCC/Posix/*.c"
    )

else()

    message(STATUS "FreeRTOS is disabled")

    set(
        FREERTOS_SOURCES
        ""
    )

endif()

# =============================================================================
# Project include directories
# =============================================================================

include_directories(
    ${PROJECT_SOURCE_DIR}/main/inc
)

# =============================================================================
# SDL2
# =============================================================================

find_package(
    SDL2
    REQUIRED
)

# =============================================================================
# LVGL compile definitions
# =============================================================================

add_compile_definitions(
    $<$<BOOL:${LV_USE_DRAW_SDL}>:LV_USE_DRAW_SDL=1>
    $<$<BOOL:${LV_USE_LIBPNG}>:LV_USE_LIBPNG=1>
    $<$<BOOL:${LV_USE_LIBJPEG_TURBO}>:LV_USE_LIBJPEG_TURBO=1>
    $<$<BOOL:${LV_USE_FFMPEG}>:LV_USE_FFMPEG=1>
)

# =============================================================================
# Fetch LVGL from GitHub
# =============================================================================
#
# LVGL is NOT expected to exist in the project source tree.
#
# CMake will download it into:
#
#   build/_deps/lvgl-src
#
# and build it as part of this project.
# =============================================================================

include(
    FetchContent
)

message(
    STATUS
    "Fetching LVGL ${LVGL_VERSION} from GitHub"
)

FetchContent_Declare(
    lvgl

    GIT_REPOSITORY
        https://github.com/lvgl/lvgl.git

    GIT_TAG
        ${LVGL_VERSION}

    GIT_SHALLOW
        TRUE
)

FetchContent_MakeAvailable(
    lvgl
)

message(
    STATUS
    "LVGL source directory: ${lvgl_SOURCE_DIR}"
)

# =============================================================================
# LVGL target
# =============================================================================

if(NOT TARGET lvgl)

    message(
        FATAL_ERROR
        "LVGL was downloaded but the 'lvgl' CMake target was not created."
    )

endif()

message(
    STATUS
    "LVGL target found"
)

# SDL2 include directories are needed by the application/LVGL.
target_include_directories(
    lvgl
    PUBLIC
        ${SDL2_INCLUDE_DIRS}
)

# =============================================================================
# Main sources
# =============================================================================

set(
    MAIN_SOURCES

    src/mouse_cursor_icon.c
    src/hal/hal.c
)

# =============================================================================
# Main libraries
# =============================================================================

set(
    MAIN_LIBS

    lvgl
    ${SDL2_LIBRARIES}
)

# -----------------------------------------------------------------------------
# LVGL examples
# -----------------------------------------------------------------------------

if(TARGET lvgl::examples)

    list(
        APPEND
        MAIN_LIBS
        lvgl::examples
    )

elseif(TARGET lvgl_examples)

    list(
        APPEND
        MAIN_LIBS
        lvgl_examples
    )

endif()

# -----------------------------------------------------------------------------
# LVGL demos
# -----------------------------------------------------------------------------

if(TARGET lvgl::demos)

    list(
        APPEND
        MAIN_LIBS
        lvgl::demos
    )

elseif(TARGET lvgl_demos)

    list(
        APPEND
        MAIN_LIBS
        lvgl_demos
    )

endif()

# -----------------------------------------------------------------------------
# ThorVG
# -----------------------------------------------------------------------------

if(TARGET lvgl::thorvg)

    list(
        APPEND
        MAIN_LIBS
        lvgl::thorvg
    )

elseif(TARGET lvgl_thorvg)

    list(
        APPEND
        MAIN_LIBS
        lvgl_thorvg
    )

endif()

# =============================================================================
# Application entry point
# =============================================================================

if(USE_FREERTOS)

    list(
        APPEND
        MAIN_SOURCES

        src/freertos_main.c
        src/freertos/freertos_posix_port.c

        ${FREERTOS_SOURCES}
    )

    list(
        APPEND
        MAIN_LIBS

        freertos_config
        freertos_kernel
    )

else()

    list(
        APPEND
        MAIN_SOURCES

        src/main.c
    )

endif()

# =============================================================================
# Platform libraries
# =============================================================================

if(NOT MSVC)

    list(
        APPEND
        MAIN_LIBS

        m
        pthread
    )

endif()

# =============================================================================
# SDL2_image
# =============================================================================

if(LV_USE_DRAW_SDL)

    set(
        CMAKE_MODULE_PATH
        ${CMAKE_MODULE_PATH}
        "${CMAKE_SOURCE_DIR}/cmake"
    )

    find_package(
        SDL2_image
        REQUIRED
    )

    target_include_directories(
        lvgl
        PUBLIC
            ${SDL2_IMAGE_INCLUDE_DIRS}
    )

    list(
        APPEND
        MAIN_LIBS
        ${SDL2_IMAGE_LIBRARIES}
    )

endif()

# =============================================================================
# libpng
# =============================================================================

if(LV_USE_LIBPNG)

    find_package(
        PNG
        REQUIRED
    )

    target_include_directories(
        lvgl
        PUBLIC
            ${PNG_INCLUDE_DIRS}
    )

    list(
        APPEND
        MAIN_LIBS
        ${PNG_LIBRARIES}
    )

endif()

# =============================================================================
# libjpeg-turbo
# =============================================================================

if(LV_USE_LIBJPEG_TURBO)

    find_package(
        JPEG
        REQUIRED
    )

    target_include_directories(
        lvgl
        PUBLIC
            ${JPEG_INCLUDE_DIRS}
    )

    list(
        APPEND
        MAIN_LIBS
        ${JPEG_LIBRARIES}
    )

endif()

# =============================================================================
# FFmpeg
# =============================================================================

if(LV_USE_FFMPEG)

    list(
        APPEND
        MAIN_LIBS

        avformat
        avcodec
        avutil
        swscale
        z
    )

endif()

# =============================================================================
# FreeType
# =============================================================================

if(LV_USE_FREETYPE)

    find_package(
        Freetype
        REQUIRED
    )

    target_include_directories(
        lvgl
        PUBLIC
            ${FREETYPE_INCLUDE_DIRS}
    )

    list(
        APPEND
        MAIN_LIBS
        ${FREETYPE_LIBRARIES}
    )

endif()

# =============================================================================
# LVGL Pro Project
# =============================================================================

if(LVGL_PRO_PROJECT_DIR)

    if(EXISTS "${LVGL_PRO_PROJECT_DIR}/CMakeLists.txt")

        message(
            STATUS
            "LVGL Pro Project enabled from: ${LVGL_PRO_PROJECT_DIR}"
        )

        add_subdirectory(
            ${LVGL_PRO_PROJECT_DIR}
            ui
        )

        list(
            APPEND
            MAIN_LIBS
            lib-ui
        )

        # ---------------------------------------------------------------------
        # LVGL Pro compile definitions
        # ---------------------------------------------------------------------

        target_compile_definitions(
            lib-ui
            PUBLIC
                LV_LVGL_H_INCLUDE_SIMPLE
        )

        # ---------------------------------------------------------------------
        # LVGL Pro includes
        # ---------------------------------------------------------------------
        #
        # Do NOT use:
        #
        #   ${CMAKE_SOURCE_DIR}/lvgl
        #
        # because LVGL is fetched into CMake's build directory.
        #
        # Linking against lvgl propagates its include directories.
        # ---------------------------------------------------------------------

        target_include_directories(
            lib-ui
            PUBLIC
                ${LVGL_PRO_PROJECT_DIR}
        )

        target_link_libraries(
            lib-ui
            PUBLIC
                lvgl
        )

    else()

        message(
            WARNING
            "LVGL_PRO_PROJECT_DIR was specified, but no CMakeLists.txt "
            "was found at: ${LVGL_PRO_PROJECT_DIR}"
        )

    endif()

endif()

# =============================================================================
# Main executable
# =============================================================================

add_executable(
    main

    ${MAIN_SOURCES}

    # -------------------------------------------------------------------------
    # HAL
    # -------------------------------------------------------------------------

    HAL/temp_sensor_mock.c
    HAL/wifi_manager_mock.c
    HAL/wifi_manager.c
    HAL/temp_sensor.c

    # -------------------------------------------------------------------------
    # UI
    # -------------------------------------------------------------------------

    UI/screen_main.c
    UI/screen_settings.c
    UI/ui.c
)

# =============================================================================
# Main compile definitions
# =============================================================================

target_compile_definitions(
    main
    PRIVATE
        LV_CONF_INCLUDE_SIMPLE
)

# =============================================================================
# Main include directories
# =============================================================================

target_include_directories(
    main
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/main/inc
)

# =============================================================================
# Main libraries
# =============================================================================

target_link_libraries(
    main
    PRIVATE
        ${MAIN_LIBS}
)

# =============================================================================
# Run target
# =============================================================================

add_custom_target(
    run

    COMMAND
        ${EXECUTABLE_OUTPUT_PATH}/main

    DEPENDS
        main

    WORKING_DIRECTORY
        ${WORKING_DIRECTORY}
)

# =============================================================================
# Windows console
# =============================================================================

if(WIN32)

    # On Windows, GUI applications do not show a console by default.
    # Force a console so application logs remain visible.

    if(MSVC)

        target_link_options(
            main
            PRIVATE
                "/SUBSYSTEM:CONSOLE"
        )

    else()

        target_link_options(
            main
            PRIVATE
                "-mconsole"
        )

    endif()

endif()

# =============================================================================
# Debug compiler warnings
# =============================================================================

if(CMAKE_BUILD_TYPE STREQUAL "Debug")

    message(
        STATUS
        "Debug mode enabled"
    )

    target_compile_options(
        lvgl
        PRIVATE

            -pedantic-errors
            -Wall
            -Wclobbered
            -Wdeprecated
            -Wdouble-promotion
            -Wempty-body
            -Wextra
            -Wformat-security
            -Wmaybe-uninitialized

            # -Wmissing-prototypes

            -Wpointer-arith
            -Wmultichar
            -Wno-pedantic
            -Wreturn-type
            -Wshadow
            -Wshift-negative-value
            -Wsizeof-pointer-memaccess
            -Wtype-limits
            -Wundef
            -Wuninitialized
            -Wunreachable-code
            -Wfloat-conversion
            -Wstrict-aliasing
    )

endif()

# =============================================================================
# AddressSanitizer
# =============================================================================

if(ASAN)

    message(
        STATUS
        "AddressSanitizer enabled"
    )

    add_compile_options(
        -fsanitize=address
        -fno-omit-frame-pointer
    )

    add_link_options(
        -fsanitize=address
    )

else()

    message(
        STATUS
        "AddressSanitizer disabled"
    )

endif()

# =============================================================================
# Configuration summary
# =============================================================================

message("")
message("============================================================")
message("Project configuration")
message("============================================================")
message("LVGL version:       ${LVGL_VERSION}")
message("LVGL source:        ${lvgl_SOURCE_DIR}")
message("FreeRTOS:           ${USE_FREERTOS}")
message("SDL draw:           ${LV_USE_DRAW_SDL}")
message("libpng:             ${LV_USE_LIBPNG}")
message("libjpeg-turbo:      ${LV_USE_LIBJPEG_TURBO}")
message("FFmpeg:             ${LV_USE_FFMPEG}")
message("FreeType:           ${LV_USE_FREETYPE}")
message("AddressSanitizer:   ${ASAN}")
message("============================================================")
message("")
