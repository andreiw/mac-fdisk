//
// io.h - simple io and input parsing routines
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


//
// Defines
//
#define	PBLOCK_SIZE	512


//
// Types
//


//
// Global Constants
//
extern const long kDefault;


//
// Global Variables
//


//
// Forward declarations
//
void bad_input(char *fmt, ...);
int close_device(int fildes);
void flush_to_newline(int keep_newline);
int get_command(char *prompt, int promptBeforeGet, int *command);
long get_multiplier(long divisor);
int get_number_argument(char *prompt, long *number, long default_value);
int get_okay(char *prompt, int default_value);
int get_string_argument(char *prompt, char **string, int reprompt);
int getch();
int number_of_digits(unsigned long value);
int open_device(const char *path, int oflag);
int read_block(int fd, unsigned long num, char *buf, int quiet);
void ungetch(int c);
int write_block(int fd, unsigned long num, char *buf);
