#ifndef PR7_H
#define PR7_H

/* CMPSC 311 Project 7
 *
 * Authors:    Shane Tully and Gage Ames
 * Emails:     spt5063@psu.edu and gra5028@psu.edu
 *
 */

/*----------------------------------------------------------------------------*/

#include <limits.h>

/*
 *  MAX_LINE            input line length
 *  MAX_PATH            directory name length
 *  MAX_CHILDREN        number of child processes
 */

#ifdef _POSIX_C_SOURCE
/* use the minimal value for maximal portability */

#define MAX_LINE        _POSIX_MAX_INPUT
#define MAX_PATH        _POSIX_PATH_MAX
#define MAX_CHILDREN    _POSIX_CHILD_MAX

#else
/* use the default value for this system */

#define MAX_LINE        MAX_INPUT
#define MAX_PATH        PATH_MAX
#define MAX_CHILDREN    CHILD_MAX

/* Alternative versions can be obtained from
 *   #include <unistd.h>
 *   sysconf(_SC_LINE_MAX)
 *   sysconf(_SC_CHILD_MAX)
 */

#endif

/*----------------------------------------------------------------------------*/

#endif

