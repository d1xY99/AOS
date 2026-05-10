target_include_directories(user_libc
  PUBLIC
    include
    include/sys
)

add_subdirectory(src)
