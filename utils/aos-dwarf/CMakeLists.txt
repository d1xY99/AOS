cmake_minimum_required(VERSION 3.10.0)

set(CMAKE_CXX_STANDARD 11)

add_subdirectory(elf)
add_subdirectory(dwarf)

add_executable(aos-dwarf aos-dwarf.cc)
target_include_directories(aos-dwarf PRIVATE elf dwarf)
target_link_libraries(aos-dwarf elf++ dwarf++)
 
target_compile_features(aos-dwarf INTERFACE cxx_range_for)
