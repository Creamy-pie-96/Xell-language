cmake_minimum_required(VERSION 3.16)
project(Xell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Core library (everything except main.cpp) ----
add_library(xell_lib
    src/lexer/lexer.cpp
    src/parser/parser.cpp
)
target_include_directories(xell_lib PUBLIC src)

# ---- Main executable ----
add_executable(xell src/main.cpp)
target_link_libraries(xell xell_lib)

# ---- Tests ----
enable_testing()

add_executable(parser_test test/parser_test.cpp)
target_link_libraries(parser_test xell_lib)
add_test(NAME parser_test COMMAND parser_test)
