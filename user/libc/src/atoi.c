#include "stdlib.h"

/**
 * parses the given string and returns its value as integer
 * @param string the string for parsing
 * @return the parsed value
 *
 */
int atoi(const char *string)
{
  // taken from kprintf.cpp
  int number = 0;
  int base = 10;
  int negative = 0;

  if(*string == '-')
  {
    negative = 1;
    ++string;
  }
  else
    if(*string == '+')
      ++string;

  while(*string >= '0' && *string <= '9')
  {
    number *= base;
    number += *string - '0';
    ++string;
  }

  if(negative)
    number *= -1;

  return number;
}

/**
 * parses the given string and returns its value as long
 * @param string the string for parsing
 * @return the parsed value
 *
 */
long atol(const char *string)
{
  // taken from kprintf.cpp
  long number = 0;
  long base = 10;
  int negative = 0;

  if(*string == '-')
  {
    negative = 1;
    ++string;
  }
  else
    if(*string == '+')
      ++string;

  while(*string >= '0' && *string <= '9')
  {
    number *= base;
    number += *string - '0';
    ++string;
  }

  if(negative)
    number *= -1;

  return number;
}
