cmake_minimum_required(VERSION 3.16)
project(XellTerminal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Find SDL2 and SDL2_ttf -------------------------------------------------

# Try pkg-config first (works on most Linux/macOS)
find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
    pkg_check_modules(SDL2_TTF IMPORTED_TARGET SDL2_ttf)
endif()

# Fallback: CMake find_package
if(NOT SDL2_FOUND)
    find_package(SDL2 REQUIRED)
endif()

if(NOT SDL2_TTF_FOUND)
    find_package(SDL2_ttf REQUIRED)
endif()

# ---- Source files ------------------------------------------------------------

# Xell core sources needed by the terminal IDE (lexer for syntax highlighting)
set(XELL_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

set(XELL_CORE_SOURCES
    ${XELL_CORE_DIR}/lexer/lexer.cpp
)

set(TERMINAL_SOURCES
    src/main.cpp
    src/terminal/screen_buffer.cpp
    src/terminal/vt_parser.cpp
    src/terminal/input_handler.cpp
    src/renderer/renderer.cpp
    ${XELL_CORE_SOURCES}
)

# Platform-specific PTY implementation
if(WIN32)
    list(APPEND TERMINAL_SOURCES src/pty/pty_win.cpp)
else()
    list(APPEND TERMINAL_SOURCES src/pty/pty_unix.cpp)
endif()

# ---- Executable --------------------------------------------------------------

add_executable(xell-terminal ${TERMINAL_SOURCES})

target_include_directories(xell-terminal PRIVATE src)

# Include Xell core headers (for lexer, token, etc.)
target_include_directories(xell-terminal PRIVATE ${XELL_CORE_DIR})

# ---- Link libraries ----------------------------------------------------------

if(PkgConfig_FOUND AND TARGET PkgConfig::SDL2 AND TARGET PkgConfig::SDL2_TTF)
    target_link_libraries(xell-terminal PRIVATE PkgConfig::SDL2 PkgConfig::SDL2_TTF)
else()
    target_include_directories(xell-terminal PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})
    target_link_libraries(xell-terminal PRIVATE ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES})
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(xell-terminal PRIVATE kernel32 user32)
elseif(APPLE)
    target_link_libraries(xell-terminal PRIVATE util)
else()
    # Linux: util for forkpty, pthread for threads
    target_link_libraries(xell-terminal PRIVATE util pthread)
endif()

# ---- Copy font asset to build directory --------------------------------------

set(FONT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/JetBrainsMonoNerdFont-Regular.ttf")
set(FONT_DST "${CMAKE_CURRENT_BINARY_DIR}/assets/fonts/JetBrainsMonoNerdFont-Regular.ttf")

add_custom_command(TARGET xell-terminal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FONT_SRC}" "${FONT_DST}"
    COMMENT "Copying font to build directory"
)

# ---- Copy terminal_colors.json to build directory ----------------------------

set(COLORS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../Extensions/xell-vscode/color_customizer/terminal_colors.json")
set(COLORS_DST "${CMAKE_CURRENT_BINARY_DIR}/assets/terminal_colors.json")

add_custom_command(TARGET xell-terminal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${COLORS_SRC}" "${COLORS_DST}"
    COMMENT "Copying terminal_colors.json to build directory"
)

# ---- Install -----------------------------------------------------------------

install(TARGETS xell-terminal DESTINATION bin)
install(DIRECTORY assets/fonts/ DESTINATION share/xell-terminal/fonts
    FILES_MATCHING PATTERN "*.ttf")
