include_directories(../../incl/filesystem)

add_aos_library(core_filesystem)

add_subdirectory(devicefs)
add_subdirectory(minixfs)
add_subdirectory(ramfs)