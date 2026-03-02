cmake_minimum_required(VERSION 3.16)
project(Xell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Core library (everything except main.cpp) ----
add_library(xell_lib
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/interpreter/xobject.cpp
    src/interpreter/interpreter.cpp
    src/os/fs.cpp
    src/os/env.cpp
    src/os/process.cpp
)
target_include_directories(xell_lib PUBLIC src)

# ---- Main executable ----
add_executable(xell src/main.cpp)
target_link_libraries(xell xell_lib)

# ---- Install target (sudo make install) ----
install(TARGETS xell DESTINATION bin)

# Install customizer data to share directory
install(DIRECTORY Extensions/xell-vscode/color_customizer/
    DESTINATION share/xell/color_customizer
    FILES_MATCHING PATTERN "*.py" PATTERN "*.html" PATTERN "*.json")

# ---- Tests ----
enable_testing()

add_executable(parser_test test/parser_test.cpp)
target_link_libraries(parser_test xell_lib)
add_test(NAME parser_test COMMAND parser_test)

add_executable(error_test test/error_test.cpp)
target_link_libraries(error_test xell_lib)
add_test(NAME error_test COMMAND error_test)

add_executable(xobject_test test/xobject_test.cpp)
target_link_libraries(xobject_test xell_lib)
add_test(NAME xobject_test COMMAND xobject_test)

add_executable(interpreter_test test/interpreter_test.cpp)
target_link_libraries(interpreter_test xell_lib)
add_test(NAME interpreter_test COMMAND interpreter_test)

add_executable(os_test test/os_test.cpp)
target_link_libraries(os_test xell_lib)
add_test(NAME os_test COMMAND os_test)

add_executable(bring_test test/bring_test.cpp)
target_link_libraries(bring_test xell_lib)
add_test(NAME bring_test COMMAND bring_test)

add_executable(shell_test test/shell_test.cpp)
target_link_libraries(shell_test xell_lib)
add_test(NAME shell_test COMMAND shell_test)

add_executable(builtin_test test/builtin_test.cpp)
target_link_libraries(builtin_test xell_lib)
add_test(NAME builtin_test COMMAND builtin_test)
