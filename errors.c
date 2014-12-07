//
// errors.c - error & help routines
//
// Written by Eryk Vershen (eryk@apple.com)
//

/*
 * Copyright 1996,1997 by Apple Computer, Inc.
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */

#include <stdio.h>
#ifndef __linux__
#include <stdlib.h>
#endif
#include <string.h>
#include <stdarg.h>

#include "errors.h"
#include "pdisk.h"


//
// Defines
//


//
// Types
//


//
// Global Constants
//


//
// Global Variables
//
char *program_name;


//
// Forward declarations
//


//
// Routines
//
void
init_program_name(char **argv)
{
#ifdef __linux__
    if ((program_name = strrchr(argv[0], '/')) != (char *)NULL) {
	program_name++;
    } else {
	program_name = argv[0];
    }
#else
    program_name = "pdisk";
#endif
}


void
do_help()
{
    printf("\t%s [-h|--help]\n", program_name);
    printf("\t%s [-v|--version]\n", program_name);
    printf("\t%s [-l|--list [name ...]]\n", program_name);
    printf("\t%s [-r|--readonly] name ...\n", program_name);
    printf("\t%s name ...\n", program_name);
}


void
usage(char *kind)
{
    error(-1, "bad usage - %s\n", kind);
    hflag = 1;
}


//
// Print a message on standard error and exit with value.
// Values in the range of system error numbers will add
// the perror(3) message.
//
void
fatal(int value, char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s: ", program_name);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

#ifdef __linux__
    if (value > 0 && value < sys_nerr) {
	fprintf(stderr, "  (%s)\n", sys_errlist[value]);
    } else {
	fprintf(stderr, "\n");
    }
#else
    fprintf(stderr, "\n");
#endif

    exit(value);
}


//
// Print a message on standard error.
// Values in the range of system error numbers will add
// the perror(3) message.
//
void
error(int value, char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s: ", program_name);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

#ifdef __linux__
    if (value > 0 && value < sys_nerr) {
	fprintf(stderr, "  (%s)\n", sys_errlist[value]);
    } else {
	fprintf(stderr, "\n");
    }
#else
    fprintf(stderr, "\n");
#endif
}
