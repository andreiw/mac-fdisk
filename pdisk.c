//
// pdisk - an editor for Apple format partition tables
//
// Written by Eryk Vershen (eryk@apple.com)
//
// Still under development (as of 20 Dec 1996)
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
#ifdef __linux__
#include <getopt.h>
#else
#include <stdlib.h>
#include <unistd.h>
#include <SIOUX.h>
#endif
#include <string.h>
#include <errno.h>

#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#endif

#include "pdisk.h"
#include "io.h"
#include "errors.h"
#include "partition_map.h"
#include "dump.h"
#include "version.h"


//
// Defines
//
#define ARGV_CHUNK 5
#ifdef __linux__
#define std_main main
#endif


//
// Types
//


//
// Global Constants
//
enum getopt_values {
    kLongOption = 0,
    kBadOption = '?',
    kOptionArg = 1000,
    kListOption = 1001
};


//
// Global Variables
//
int lflag;
char *lfile;
int vflag;
int hflag;
int dflag;
int rflag;


//
// Forward declarations
//
void do_add_intel_partition(partition_map_header *map);
void do_change_map_size(partition_map_header *map);
void do_create_partition(partition_map_header *map, int get_type);
void do_delete_partition(partition_map_header *map);
int do_expert(partition_map_header *map);
void do_reorder(partition_map_header *map);
void do_write_partition_map(partition_map_header *map);
void edit(char *name);
int get_base_argument(long *number, partition_map_header *map);
int get_command_line(int *argc, char ***argv);
int get_size_argument(long *number, partition_map_header *map);
int get_options(int argc, char **argv);
void print_notes();


//
// Routines
//
#ifdef __linux__
int
main(int argc, char **argv)
{
    int name_index;

    if (sizeof(DPME) != PBLOCK_SIZE) {
	fatal(-1, "Size of partion map entry (%d) "
		"is not equal to block size (%d)\n",
		sizeof(DPME), PBLOCK_SIZE);
    }
    if (sizeof(Block0) != PBLOCK_SIZE) {
	fatal(-1, "Size of block zero structure (%d) "
		"is not equal to block size (%d)\n",
		sizeof(Block0), PBLOCK_SIZE);
    }

    name_index = get_options(argc, argv);

    if (vflag) {
	printf("version " VERSION " (" RELEASE_DATE ")\n");
    }
    if (hflag) {
 	do_help();
    } else if (lflag) {
	if (lfile != NULL) {
	    dump(lfile);
	} else if (name_index < argc) {
	    while (name_index < argc) {
		dump(argv[name_index++]);
	    }
	} else {
	    list_all_disks();
	}
    } else if (name_index < argc) {
	while (name_index < argc) {
	    edit(argv[name_index++]);
	}
    } else if (!vflag) {
	usage("no device argument");
 	do_help();
    }
}
#else
main()
{
    char *name;
    int command;
    int first = 1;

    printf("This app uses the SIOUX console library\n");
    printf("Choose 'Quit' from the file menu to quit.\n\n");
    printf("Use MkLinux style disk names (i.e. /dev/sda, /dev/sdb, etc.).\n\n");
 
    SIOUXSettings.autocloseonquit = 0;	/* Do we close the SIOUX window on program termination ... */
    SIOUXSettings.asktosaveonclose = 0;	/* Do we offer to save on a close ... */
    
    if (sizeof(DPME) != PBLOCK_SIZE) {
	fatal(-1, "Size of partion map entry (%d) "
		"is not equal to block size (%d)\n",
		sizeof(DPME), PBLOCK_SIZE);
    }
    if (sizeof(Block0) != PBLOCK_SIZE) {
	fatal(-1, "Size of block zero structure (%d) "
		"is not equal to block size (%d)\n",
		sizeof(Block0), PBLOCK_SIZE);
    }
    init_program_name(NULL);

    while (get_command("Top level command (? for help): ", first, &command)) {
	first = 0;

	switch (command) {
	case '?':
	    print_notes();
	case 'H':
	case 'h':
	    printf("Commands are:\n");
	    printf("  h    print help\n");
	    printf("  v    print the version number and release date\n");
	    printf("  l    list device's map\n");
	    printf("  L    list all devices' maps\n");
	    printf("  e    edit device's map\n");
	    printf("  r    toggle readonly flag\n");
	    printf("  q    quit the program\n");
	    break;
	case 'Q':
	case 'q':
	    goto finis;
	    break;
	case 'V':
	case 'v':
	    printf("version " VERSION " (" RELEASE_DATE ")\n");
	    break;
	case 'L':
	    list_all_disks();
	    break;
	case 'l':
	    if (get_string_argument("Name of device: ", &name, 1) == 0) {
		bad_input("Bad name");
		break;
	    }
	    dump(name);
	    break;
	case 'E':
	case 'e':
	    if (get_string_argument("Name of device: ", &name, 1) == 0) {
		bad_input("Bad name");
		break;
	    }
	    edit(name);
	    break;
	case 'R':
	case 'r':
	    if (rflag) {
		rflag = 0;
	    } else {
		rflag = 1;
	    }
	    printf("Now in %s mode.\n", (rflag)?"readonly":"read/write");
	    break;
	default:
	    bad_input("No such command (%c)", command);
	    break;
	}
    }
finis:

    printf("Processing stopped: Choose 'Quit' from the file menu to quit.\n\n");
}
#endif


#ifdef __linux__
int
get_options(int argc, char **argv)
{
    int c;
    static struct option long_options[] =
    {
	// name		has_arg			&flag	val
	{"help",	no_argument,		0,	'h'},
	{"list",	optional_argument,	0,	kListOption},
	{"version",	no_argument,		0,	'v'},
	{"debug",	no_argument,		0,	'd'},
	{"readonly",	no_argument,		0,	'r'},
	{0, 0, 0, 0}
    };
    int option_index = 0;
    extern int optind;
    extern char *optarg;
    int flag = 0;

    init_program_name(argv);

    lflag = 0;
    lfile = NULL;
    vflag = 0;
    hflag = 0;
    dflag = 0;
    rflag = 0;

    optind = 0;	// reset option scanner logic
    while ((c = getopt_long(argc, argv, "hlvdr", long_options,
	    &option_index)) >= 0) {
	switch (c) {
	case kLongOption:
	    // option_index would be used here
	    break;
	case 'h':
	    hflag = 1;
	    break;
	case kListOption:
	    if (optarg != NULL) {
		lfile = optarg;
	    }
	    // fall through
	case 'l':
	    lflag = 1;
	    break;
	case 'v':
	    vflag = 1;
	    break;
	case 'd':
	    dflag = 1;
	    break;
	case 'r':
	    rflag = 1;
	    break;
	case kBadOption:
	default:
	    flag = 1;
	    break;
	}
    }
    if (flag) {
	usage("bad arguments");
    }
    return optind;
}
#endif

//
// Edit the file
//
void
edit(char *name)
{
    partition_map_header *map;
    int command;
#ifdef __linux__
    int first = 1;
#else
    int first = 0;
#endif
    int order;
    int get_type;
    int valid_file;

    map = open_partition_map(name, &valid_file);
    if (!valid_file) {
    	return;
    }

    printf("%s\n", name);

    while (get_command("Command (? for help): ", first, &command)) {
	first = 0;
	order = 1;
	get_type = 0;

	switch (command) {
	case '?':
	    print_notes();
	case 'H':
	case 'h':
	    printf("Commands are:\n");
	    printf("  h    help\n");
	    printf("  p    print the partition table\n");
	    printf("  P    (print ordered by base address)\n");
	    printf("  i    initialize partition map\n");
	    printf("  s    change size of partition map\n");
	    printf("  c    create new partition\n");
	    printf("  C    (create with type also specified)\n");
	    printf("  d    delete a partition\n");
	    printf("  r    reorder partition entry in map\n");
	    if (!rflag) {
		printf("  w    write the partition table\n");
	    }
	    printf("  q    quit editing (don't save changes)\n");
	    if (dflag) {
		printf("  x    extra extensions for experts\n");
	    }
	    break;
	case 'P':
	    order = 0;
	    // fall through
	case 'p':
	    dump_partition_map(map, order);
	    break;
	case 'Q':
	case 'q':
	    flush_to_newline(1);
	    goto finis;
	    break;
	case 'I':
	case 'i':
	    map = init_partition_map(name, map);
	    break;
	case 'C':
	    get_type = 1;
	    // fall through
	case 'c':
	    do_create_partition(map, get_type);
	    break;
	case 'D':
	case 'd':
	    do_delete_partition(map);
	    break;
	case 'R':
	case 'r':
	    do_reorder(map);
	    break;
	case 'S':
	case 's':
	    do_change_map_size(map);
	    break;
	case 'X':
	case 'x':
	    if (!dflag) {
		goto do_error;
	    } else if (do_expert(map)) {
		flush_to_newline(0);
		goto finis;
	    }
	    break;
	case 'W':
	case 'w':
	    if (!rflag) {
		do_write_partition_map(map);
		break;
	    }
	default:
	do_error:
	    bad_input("No such command (%c)", command);
	    break;
	}
    }
finis:

    close_partition_map(map);
}


void
do_create_partition(partition_map_header *map, int get_type)
{
    long base;
    long length;
    long mult;
    char *name;
    char *type_name;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (!rflag && map->writeable == 0) {
	printf("The map is not writeable.\n");
    }
// XXX add help feature (i.e. '?' in any argument routine prints help string)
    if (get_base_argument(&base, map) == 0) {
	return;
    }
    if (get_size_argument(&length, map) == 0) {
	return;
    }

    if (get_string_argument("Name of partition: ", &name, 1) == 0) {
	bad_input("Bad name");
	return;
    }
    if (get_type == 0) {
	add_partition_to_map(name, kUnixType, base, length, map);

    } else if (get_string_argument("Type of partition: ", &type_name, 1) == 0) {
	bad_input("Bad type");
	return;
    } else {
	if (strncmp(type_name, kFreeType, DPISTRLEN) == 0) {
	    bad_input("Can't create a partition with the Free type");
	    return;
	}
	if (strncmp(type_name, kMapType, DPISTRLEN) == 0) {
	    bad_input("Can't create a partition with the Map type");
	    return;
	}
	add_partition_to_map(name, type_name, base, length, map);
    }
}


int
get_base_argument(long *number, partition_map_header *map)
{
    partition_map * entry;
    int c;
    int result = 0;

    if (get_number_argument("First block: ", number, kDefault) == 0) {
	bad_input("Bad block number");
    } else {
	result = 1;
	c = getch();

	if (c == 'p' || c == 'P') {
	    entry = find_entry_by_disk_address(*number, map);
	    if (entry == NULL) {
		bad_input("Bad partition number");
		result = 0;
	    } else {
		*number = entry->data->dpme_pblock_start;
	    }
	} else if (c > 0) {
	    ungetch(c);
	}
    }
    return result;
}


int
get_size_argument(long *number, partition_map_header *map)
{
    partition_map * entry;
    int c;
    int result = 0;
    long multiple;

    if (get_number_argument("Length in blocks: ", number, kDefault) == 0) {
	bad_input("Bad length");
    } else {
	result = 1;
	multiple = get_multiplier(PBLOCK_SIZE);
	if (multiple != 1) {
	    *number *= multiple;
	} else {
	    c = getch();

	    if (c == 'p' || c == 'P') {
		entry = find_entry_by_disk_address(*number, map);
		if (entry == NULL) {
		    bad_input("Bad partition number");
		    result = 0;
		} else {
		    *number = entry->data->dpme_pblocks;
		}
	    } else if (c > 0) {
		ungetch(c);
	    }
	}
    }
    return result;
}


void
do_delete_partition(partition_map_header *map)
{
    partition_map * cur;
    long index;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (!rflag && map->writeable == 0) {
	printf("The map is not writeable.\n");
    }
    if (get_number_argument("Partition number: ", &index, kDefault) == 0) {
	bad_input("Bad partition number");
	return;
    }

	// find partition and delete it
    cur = find_entry_by_disk_address(index, map);
    if (cur == NULL) {
	printf("No such partition\n");
    } else {
	delete_partition_from_map(cur);
    }
}


void
do_reorder(partition_map_header *map)
{
    partition_map * cur;
    long old_index;
    long index;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (!rflag && map->writeable == 0) {
	printf("The map is not writeable.\n");
    }
    if (get_number_argument("Partition number: ", &old_index, kDefault) == 0) {
	bad_input("Bad partition number");
	return;
    }
    if (get_number_argument("New number: ", &index, kDefault) == 0) {
	bad_input("Bad partition number");
	return;
    }

    move_entry_in_map(old_index, index, map);
}


void
do_write_partition_map(partition_map_header *map)
{
    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (map->changed == 0) {
	bad_input("The map has not been changed.");
	return;
    }
    if (map->writeable == 0) {
	bad_input("The map is not writeable.");
	return;
    }
    printf("Writing the map destroys what was there before. ");
    if (get_okay("Is that okay? [n/y]: ", 0) != 1) {
	return;
    }

    write_partition_map(map);

    // exit(0);
}

int
do_expert(partition_map_header *map)
{
    int command;
    int first = 0;
    int quit = 0;

    while (get_command("Expert command (? for help): ", first, &command)) {
	first = 0;

	switch (command) {
	case '?':
	    print_notes();
	case 'H':
	case 'h':
	    printf("Commands are:\n");
	    printf("  h    print help\n");
	    printf("  x    return to main menu\n");
	    printf("  p    print the partition table\n");
	    if (dflag) {
		printf("  P    (show data structures  - debugging)\n");
	    }
	    printf("  s    change size of partition map\n");
	    if (!rflag) {
		printf("  w    write the partition table\n");
	    }
	    printf("  q    quit without saving changes\n");
	    break;
	case 'X':
	case 'x':
	    flush_to_newline(1);
	    goto finis;
	    break;
	case 'Q':
	case 'q':
	    quit = 1;
	    goto finis;
	    break;
	case 'S':
	case 's':
	    do_change_map_size(map);
	    break;
	case 'P':
	    if (dflag) {
		show_data_structures(map);
		break;
	    }
	    // fall through
	case 'p':
	    dump_partition_map(map, 1);
	    break;
	case 'W':
	case 'w':
	    if (!rflag) {
		do_write_partition_map(map);
		break;
	    }
	default:
	    bad_input("No such command (%c)", command);
	    break;
	}
    }
finis:
    return quit;
}

void
do_change_map_size(partition_map_header *map)
{
    long size;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (!rflag && map->writeable == 0) {
	printf("The map is not writeable.\n");
    }
    if (get_number_argument("New size: ", &size, kDefault) == 0) {
	bad_input("Bad size");
	return;
    }
    resize_map(size, map);
}


void
print_notes()
{
    printf("Notes:\n");
    printf("  Base and length fields are blocks, which are 512 bytes long.\n");
    printf("  The name of a partition is descriptive text.\n");
    printf("\n");
}
