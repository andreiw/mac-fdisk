//
// io.c - simple io and input parsing routines
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
#include <fcntl.h>
#include <SCSI.h>
#endif
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "pdisk.h"
#include "io.h"
#include "errors.h"


//
// Defines
//
#define BAD_DIGIT 17	/* must be greater than any base */
#define	STRING_CHUNK	16
#define UNGET_MAX_COUNT 10
#ifndef __linux__
#define SCSI_FD 8
#define loff_t long
#define llseek lseek
#endif


//
// Types
//


//
// Global Constants
//
const long kDefault = -1;


//
// Global Variables
//
short unget_buf[UNGET_MAX_COUNT+1];
int unget_count;


//
// Forward declarations
//
long get_number(int first_char);
char* get_string(int eos);
#ifndef __linux__
int DoTestUnitReady(UInt8 targetID);
int DoRead(UInt8 targetID, UInt32 block, UInt16 count, char* addr);
int DoWrite(UInt8 targetID, UInt32 block, UInt16 count, char* addr);
#endif


//
// Routines
//
int
getch()
{
    if (unget_count > 0) {
	return (unget_buf[--unget_count]);
    } else {
	return (getc(stdin));
    }
}


void
ungetch(int c)
{
    // In practice there is never more than one character in
    // the unget_buf, but what's a little overkill among friends?

    if (unget_count < UNGET_MAX_COUNT) {
	unget_buf[unget_count++] = c;
    } else {
	fatal(-1, "Programmer error in ungetch().");
    }
}

	
void
flush_to_newline(int keep_newline)
{
    int		c;

    for (;;) {
	c = getch();

	if (c <= 0) {
	    break;
	} else if (c == '\n') {
	    if (keep_newline) {
		ungetch(c);
	    }
	    break;
	} else {
	    // skip
	}
    }
    return;
}


int
get_okay(char *prompt, int default_value)
{
    int		c;

    flush_to_newline(0);
    printf(prompt);

    for (;;) {
	c = getch();

	if (c <= 0) {
	    break;
	} else if (c == ' ' || c == '\t') {
	    // skip blanks and tabs
	} else if (c == '\n') {
	    ungetch(c);
	    return default_value;
	} else if (c == 'y' || c == 'Y') {
	    return 1;
	} else if (c == 'n' || c == 'N') {
	    return 0;
	} else {
	    flush_to_newline(0);
	    printf(prompt);
	}
    }
    return -1;
}

	
int
get_command(char *prompt, int promptBeforeGet, int *command)
{
    int		c;

    if (promptBeforeGet) {
	printf(prompt);
    }	
    for (;;) {
	c = getch();

	if (c <= 0) {
	    break;
	} else if (c == ' ' || c == '\t') {
	    // skip blanks and tabs
	} else if (c == '\n') {
	    printf(prompt);
	} else {
	    *command = c;
	    return 1;
	}
    }
    return 0;
}

	
int
get_number_argument(char *prompt, long *number, long default_value)
{
    int c;
    int result = 0;

    for (;;) {
	c = getch();

	if (c <= 0) {
	    break;
	} else if (c == ' ' || c == '\t') {
	    // skip blanks and tabs
	} else if (c == '\n') {
	    if (default_value < 0) {
		printf(prompt);
	    } else {
		ungetch(c);
		*number = default_value;
		result = 1;
		break;
	    }
	} else if ('0' <= c && c <= '9') {
	    *number = get_number(c);
	    result = 1;
	    break;
	} else {
	    ungetch(c);
	    *number = 0;
	    break;
	}
    }
    return result;
}


long
get_number(int first_char)
{
    register int c;
    int base;
    int digit;
    int ret_value;

    if (first_char != '0') {
	c = first_char;
	base = 10;
	digit = BAD_DIGIT;
    } else if ((c=getch()) == 'x' || c == 'X') {
	c = getch();
	base = 16;
	digit = BAD_DIGIT;
    } else {
	c = first_char;
	base = 8;
	digit = 0;
    }
    ret_value = 0;
    for (ret_value = 0; ; c = getch()) {
	if (c >= '0' && c <= '9') {
	    digit = c - '0';
	} else if (c >='A' && c <= 'F') {
	    digit = 10 + (c - 'A');
	} else if (c >='a' && c <= 'f') {
	    digit = 10 + (c - 'a');
	} else {
	    digit = BAD_DIGIT;
	}
	if (digit >= base) {
	    break;
	}
	ret_value = ret_value * base + digit;
    }
    ungetch(c);
    return(ret_value);
}

	
int
get_string_argument(char *prompt, char **string, int reprompt)
{
    int c;
    int result = 0;

    for (;;) {
	c = getch();

	if (c <= 0) {
	    break;
	} else if (c == ' ' || c == '\t') {
	    // skip blanks and tabs
	} else if (c == '\n') {
	    if (reprompt) {
		printf(prompt);
	    } else {
		ungetch(c);
		*string = NULL;
		break;
	    }
	} else if (c == '"' || c == '\'') {
	    *string = get_string(c);
	    result = 1;
	    break;
	} else if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')
		|| (c == '-' || c == '/')) {
	    ungetch(c);
	    *string = get_string(' ');
	    result = 1;
	    break;
	} else {
	    ungetch(c);
	    *string = NULL;
	    break;
	}
    }
    return result;
}


char *
get_string(int eos)
{
    int c;
    char *s;
    char *ret_value;
    char *limit;
    int length;

    ret_value = (char *) malloc(STRING_CHUNK);
    if (ret_value == NULL) {
	error(errno, "can't allocate memory for string buffer");
	return NULL;
    }
    length = STRING_CHUNK;
    limit = ret_value + length;

    c = getch();
    for (s = ret_value; ; c = getch()) {
	if (s >= limit) {
	    // expand string
	    limit = (char *) malloc(length+STRING_CHUNK);
	    if (limit == NULL) {
		error(errno, "can't allocate memory for string buffer");
		ret_value[length-1] = 0;
		break;
	    }
	    strncpy(limit, ret_value, length);
	    free(ret_value);
	    s = limit + (s - ret_value);
	    ret_value = limit;
	    length += STRING_CHUNK;
	    limit = ret_value + length;
	}
	if (c <= 0 || c == eos || (eos == ' ' && c == '\t')) {
	    *s++ = 0;
	    break;
	} else if (c == '\n') {
	    *s++ = 0;
	    ungetch(c);
	    break;
	} else {
	    *s++ = c;
	}
    }
    return(ret_value);
}


long
get_multiplier(long divisor)
{
    int c;
    int result;

    c = getch();

    if (c <= 0 || divisor <= 0) {
	result = 0;
    } else if (c == 'g' || c == 'G') {
	result = 1024*1024*1024;
    } else if (c == 'm' || c == 'M') {
	result = 1024*1024;
    } else if (c == 'k' || c == 'K') {
	result = 1024;
    } else {
	ungetch(c);
	result = 1;
    }
    if (result > 1) {
	if (result >= divisor) {
	    result /= divisor;
	} else {
	    result = 1;
	}
    }
    return result;
}


int
number_of_digits(unsigned long value)
{
    int j;

    j = 1;
    while (value > 9) {
	j++;
	value = value / 10;
    }
    return j;
}


//
// Print a message on standard error & flush the input.
//
void
bad_input(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    flush_to_newline(1);
}


int
read_block(int fd, unsigned long num, char *buf, int quiet)
{
    loff_t x;
    long t;

#ifndef __linux__
    if (fd <= SCSI_FD) {
    	//printf("Read block %d of scsi device %d\n", num, fd);
    	return DoRead(fd, num, 1, buf);
    } else {
#else
    {
#endif
	x = num * PBLOCK_SIZE;
	if ((x = llseek(fd, x, 0)) < 0) {
	    if (quiet == 0) {
		error(errno, "Can't seek on file");
	    }
	    return 0;
	}
	if ((t = read(fd, buf, PBLOCK_SIZE)) != PBLOCK_SIZE) {
	    if (quiet == 0) {
		error((t<0?errno:0), "Can't read block %u from file", num);
	    }
	    return 0;
	}
	return 1;
    }
}


int
write_block(int fd, unsigned long num, char *buf)
{
    loff_t x;
    long t;

    if (rflag) {
	printf("Can't write block %u to file", num);
	return 0;
    }
#ifndef __linux__
    if (fd <= SCSI_FD) {
    	//printf("Write block %d of scsi device %d\n", num, fd);
    	return DoWrite(fd, num, 1, buf);
    } else {
#else
    {
#endif
	x = num * PBLOCK_SIZE;
	if ((x = lseek(fd, x, 0)) < 0) {
	    error(errno, "Can't seek on file");
	    return 0;
	}
	if ((t = write(fd, buf, PBLOCK_SIZE)) != PBLOCK_SIZE) {
	    error((t<0?errno:0), "Can't write block %u to file", num);
	    return 0;
	}
	return 1;
    }
}


int
close_device(int fildes)
{
#ifndef __linux__
    if (fildes <= SCSI_FD) {
    	//printf("Close of scsi device %d\n", fildes);
    	return 1;
    } else {
#else
    {
#endif
	return close(fildes);
    }
}


int
open_device(const char *path, int oflag)
{
#ifndef __linux__
    int id;
    int fd;
    DeviceIdent scsiDevice;
    
    if (strncmp("/dev/sd", path, 7) == 0
    	    && path[7] >= 'a' && path[7] <= 'g'
    	    && path[8] == 0) {
    	id = path[7] - 'a';
    	//printf("Open scsi device %d\n", id);

	if (DoTestUnitReady(id) > 0) {
	    return id;
	} else {
	    return -1;
	}
    } else {
#else
    {
#endif
	return open(path, oflag);
    }
}
