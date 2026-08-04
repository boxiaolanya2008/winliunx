cmake_minimum_required(VERSION 3.16)

project(winlinux
    VERSION 0.1.0
    DESCRIPTION "Native Win32 shell that executes Linux commands via Git Bash"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

option(WINLINUX_BUILD_TESTS "Build unit tests" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(cmake/CompilerWarnings.cmake)

add_executable(winlinux
    src/main.c
    src/ui/window.c
    src/core/backend.c
    src/core/pipeline.c
    src/core/executor.c
)

target_include_directories(winlinux PRIVATE src)
set_project_warnings(winlinux)

find_package(Threads REQUIRED)

if(WIN32)
    target_link_libraries(winlinux PRIVATE
        user32
        gdi32
        comdlg32
        ole32
    )
endif()

if(WINLINUX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
