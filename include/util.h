/*
 * ============================================================================
 *
 *       Filename:  util.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2017-09-21 12:39:52 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Prashant Pandey (), ppandey@cs.stonybrook.edu
 *   Organization:  Stony Brook University
 *
 * ============================================================================
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <iostream>
#include <cstring>
#include <vector>
#include <cassert>
#include <fstream>

#include <inttypes.h>

#ifdef DEBUG
#define PRINT_DEBUG 1
#else
#define PRINT_DEBUG 0
#endif

#define DEBUG_CDBG(x) do { \
	  if (PRINT_DEBUG) { std::cerr << x << std::endl; } \
} while (0)

std::string last_part(std::string str, char c);
std::string first_part(std::string str, char c);
/* Print elapsed time using the start and end timeval */
void print_time_elapsed(std::string desc, struct timeval* start, struct
												timeval* end);
#endif
