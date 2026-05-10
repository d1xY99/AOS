cmake_minimum_required(VERSION 3.10.0)

file(GLOB util_aos_mkimg_SOURCES *.cpp
                                   ../../core/src/utility/Bitmap.cpp
                                   ../../core/src/filesystem/Inode.cpp
                                   ../../core/src/filesystem/Dentry.cpp
                                   ../../core/src/filesystem/FileDescriptor.cpp
                                   ../../core/src/filesystem/FsContext.cpp
                                   ../../core/src/filesystem/FsTypeReg.cpp
                                   ../../core/src/filesystem/Superblock.cpp
                                   ../../core/src/filesystem/File.cpp
                                   ../../core/src/filesystem/PathWalker.cpp
                                   ../../core/src/filesystem/Path.cpp
                                   ../../core/src/filesystem/MountPoint.cpp
                                   ../../core/src/filesystem/FsOps.cpp
                                   ../../core/src/filesystem/minixfs/MinixFile.cpp
                                   ../../core/src/filesystem/minixfs/MinixNode.cpp
                                   ../../core/src/filesystem/minixfs/MinixSuper.cpp
                                   ../../core/src/filesystem/minixfs/MinixZone.cpp
                                   ../../core/src/filesystem/minixfs/MinixZoneAlloc.cpp
                                   ../../core/src/filesystem/minixfs/ZoneAllocBase.cpp)

add_executable(aos-mkimg ${util_aos_mkimg_SOURCES})

target_include_directories(aos-mkimg
  PRIVATE
    .
    aostl
    ../../core/incl/utility
    ../../core/incl/filesystem
    ../../core/incl/filesystem/minixfs
    ../../core/incl/display
)

target_compile_definitions(aos-mkimg PRIVATE AOS_MKIMG=1)
