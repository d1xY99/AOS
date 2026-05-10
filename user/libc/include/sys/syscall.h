#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Low-level syscall function, takes 6 arguments where the first is the syscall
 * number as defined in the kernel syscall definitions.
 * Unused arguments can be filled up with 0x00s.
 * e.g. __syscall(sc_exit, status, 0x00, 0x00, 0x00, 0x00);
 * DO NOT CHANGE SIGNATURE.
 */
extern size_t __syscall(size_t arg1, size_t arg2, size_t arg3, size_t arg4, size_t arg5,
                        size_t arg6);

#ifdef __cplusplus
}
#endif



