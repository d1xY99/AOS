
include_directories(common/include
    ../core/incl
    ../core/incl/cache
    ../core/incl/display
    ../core/incl/filesystem
    ../core/incl/filesystem/device
    ../core/incl/filesystem/inodes
    ../core/incl/filesystem/minix
    ../core/incl/filesystem/unixfs
    ../core/incl/kernel
    ../core/incl/memory
    ../core/incl/aostl
    ../core/incl/utility
)

include(${ARCH}/CMakeLists.subfolders)

