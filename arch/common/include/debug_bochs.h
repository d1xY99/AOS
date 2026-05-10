#pragma once
#include "types.h"

/**
 * writes a char to the bochs terminal
 *
 */
void writeChar2Bochs( char char2Write );

/**
 * writes a string/line to the bochs terminal
 * max length is 250
 *
 */
void writeLineDebug( const char *line2Write );

