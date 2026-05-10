project(userspace_tests)
include(../../arch/${ARCH}/CMakeLists.user)

# Put userspace libraries and executables in seperated paths
set(LIBRARY_OUTPUT_PATH  "${LIBRARY_OUTPUT_PATH}/userspace")
set(EXECUTABLE_OUTPUT_PATH "${EXECUTABLE_OUTPUT_PATH}/userspace")

# Reset userspace program names
set($ENV{USER_NAMES} "")

set(USERSPACE_TESTS_COMPILE_OPTIONS)
set(USERSPACE_TESTS_LINKER_OPTIONS -Wl,-Ttext=0x8000000 -Wl,--build-id=none -Wl,-whole-archive)

set(FINAL_USERSPACE_TESTS_COMPILE_OPTIONS ${ARCH_USERSPACE_COMPILE_OPTIONS} ${USERSPACE_TESTS_COMPILE_OPTIONS})
set(FINAL_USERSPACE_TESTS_LINKER_OPTIONS ${ARCH_LD_ARGUMENTS} ${ARCH_USERSPACE_LINKER_OPTIONS} ${USERSPACE_TESTS_LINKER_OPTIONS})

file(GLOB user_tests_SOURCES ${SOURCE_WILDCARDS})

# Create own executable for every .c file and link with libc
foreach(curFile ${user_tests_SOURCES})
  get_filename_component(curName ${curFile} NAME_WE)

  add_executable(${curName}.elf ${curFile})
  target_link_libraries(${curName}.elf ${FINAL_USERSPACE_TESTS_LINKER_OPTIONS} user_libc ${ARCH_APPEND_LD_ARGUMENTS})

  ADD_DEBUG_INFO(${curName}.elf)

  # Remember the userspace program names for dependency checking in the root CMakeLists
  set(ENV{USER_NAMES} "$ENV{USER_NAMES};${curName}.elf")
  set(ENV{USERSPACE_NAMES_EXE2MINIX} "$ENV{USERSPACE_NAMES_EXE2MINIX};${EXECUTABLE_OUTPUT_PATH}/${curName}.elf;${curName}.elf")
endforeach(curFile)

file(GLOB user_tests_PROJECTS */)

foreach(curProject ${user_tests_PROJECTS})
file(GLOB curProject_SOURCES ${curProject}/*.c)

  if(curProject_SOURCES)
    get_filename_component(exename ${curProject} NAME)
    add_executable(${exename}.elf ${curProject_SOURCES})
    target_link_libraries(${exename}.elf ${FINAL_USERSPACE_TESTS_LINKER_OPTIONS} "-Wl,-whole-archive" user_libc ${ARCH_APPEND_LD_ARGUMENTS})

    ADD_DEBUG_INFO(${exename}.elf)

    set(ENV{USER_NAMES} "$ENV{USER_NAMES};${exename}.elf")
    set(ENV{USERSPACE_NAMES_EXE2MINIX} "$ENV{USERSPACE_NAMES_EXE2MINIX};${EXECUTABLE_OUTPUT_PATH}/${exename}.elf;${exename}.elf")
  endif(curProject_SOURCES)
endforeach(curProject)
