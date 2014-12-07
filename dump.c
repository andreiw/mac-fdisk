//
// dump.c - dumping partition maps
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
#include <unistd.h>
#endif
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "io.h"
#include "errors.h"
#include "partition_map.h"
#include "dump.h"


//
// Defines
//
#define ONE_KILOBYTE_IN_BLOCKS	(1024.0/PBLOCK_SIZE)


//
// Types
//
typedef struct names {
    char *abbr;
    char *full;
} NAMES;


//
// Global Constants
//
NAMES plist[] = {
    "Drvr", "Apple_Driver",
    "Free", "Apple_Free",
    " HFS", "Apple_HFS",
    " MFS", "Apple_MFS",
    "PDOS", "Apple_PRODOS",
    "junk", "Apple_Scratch",
    "unix", "Apple_UNIX_SVR2",
    " map", "Apple_partition_map",
    0,	0
};

const char * kStringEmpty	= "";
const char * kStringNot		= " not";


//
// Global Variables
//


//
// Forward declarations
//
void dump_block_zero(partition_map_header *map);
void dump_partition_entry(partition_map *entry, int digits);


//
// Routines
//
void
dump(char *name)
{
    partition_map_header *map;
    int junk;

    map = open_partition_map(name, &junk);
    if (map == NULL) {
	return;
    }

    dump_partition_map(map, 1);

    close_partition_map(map);
}


void
dump_block_zero(partition_map_header *map)
{
    Block0 *p;
    DDMap *m;
    int i;

    p = map->misc;
    if (p->sbSig != BLOCK0_SIGNATURE) {
	return;
    }
    printf("\nBlock size=%u, Number of Blocks=%u\n",
	    p->sbBlkSize, p->sbBlkCount);
    printf("DeviceType=0x%x, DeviceId=0x%x\n",
	    p->sbDevType, p->sbDevId);
    if (p->sbDrvrCount > 0) {
	printf("Drivers-\n");
	m = (DDMap *) p->sbMap;
	for (i = 0; i < p->sbDrvrCount; i++) {
	    printf("%u: @ %u for %u, type=0x%x\n", i+1, m[i].ddBlock,
		    m[i].ddSize, m[i].ddType);
	}
    }
    printf("\n");
}


void
dump_partition_map(partition_map_header *map, int disk_order)
{
    partition_map * entry;
    int j;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    printf("%s\n", map->name);

    j = number_of_digits(map->media_size);
    if (j < 7) {
	j = 7;
    }
    printf("   #:                 type name               "
	    "%*s   %-*s ( size )\n", j, "length", j, "base");

    if (disk_order) {
	for (entry = map->disk_order; entry != NULL;
		entry = entry->next_on_disk) {

	    dump_partition_entry(entry, j);
	}
    } else {
	for (entry = map->base_order; entry != NULL;
		entry = entry->next_by_base) {

	    dump_partition_entry(entry, j);
	}
    }
    dump_block_zero(map);
}


void
dump_partition_entry(partition_map *entry, int digits)
{
    partition_map_header *map;
    int j;
    DPME *p;
    BZB *bp;
    char *s;
    int aflag = 0;
    int pflag = 1;
    u32 size;
    double bytes;

    map = entry->the_map;
    p = entry->data;
    if (aflag) {
	s = "????";
	for (j = 0; plist[j].abbr != 0; j++) {
	    if (strcmp(p->dpme_type, plist[j].full) == 0) {
		s = plist[j].abbr;
		break;
	    }
	}
	printf("%4d: %.4s %-18.32s ", entry->disk_address, s, p->dpme_name);
    } else {
	printf("%4d: %20.32s %-18.32s ",
		entry->disk_address, p->dpme_type, p->dpme_name);
    }

    if (pflag) {
	printf("%*u ", digits, p->dpme_pblocks);
	size = p->dpme_pblocks;
    } else if (p->dpme_lblocks + p->dpme_lblock_start != p->dpme_pblocks) {
	printf("%*u+", digits, p->dpme_lblocks);
	size = p->dpme_lblocks;
    } else if (p->dpme_lblock_start != 0) {
	printf("%*u ", digits, p->dpme_lblocks);
	size = p->dpme_lblocks;
    } else {
	printf("%*u ", digits, p->dpme_pblocks);
	size = p->dpme_pblocks;
    }
    if (pflag || p->dpme_lblock_start == 0) {
	printf("@ %-*u", digits, p->dpme_pblock_start);
    } else {
	printf("@~%-*u", digits, p->dpme_pblock_start + p->dpme_lblock_start);
    }
    
    bytes = size / ONE_KILOBYTE_IN_BLOCKS;
    if (bytes >= 1024.0) {
	bytes = bytes / 1024.0;
	if (bytes < 1024.0) {
	    j = 'M';
	} else {
	    bytes = bytes / 1024.0;
	    j = 'G';
	}
	printf(" (%#5.1f%c)", bytes, j);
    }

#if 0
    // Old A/UX fields that no one pays attention to anymore.
    bp = (BZB *) (p->dpme_bzb);
    j = -1;
    if (bp->bzb_magic == BZBMAGIC) {
	switch (bp->bzb_type) {
	case FSTEFS:
	    s = "EFS";
	    break;
	case FSTSFS:
	    s = "SFS";
	    j = 1;
	    break;
	case FST:
	default:
	    if (bzb_root_get(bp) != 0) {
		if (bzb_usr_get(bp) != 0) {
		    s = "RUFS";
		} else {
		    s = "RFS";
		}
		j = 0;
	    } else if (bzb_usr_get(bp) != 0) {
		s = "UFS";
		j = 2;
	    } else {
		s = "FS";
	    }
	    break;
	}
	if (bzb_slice_get(bp) != 0) {
	    printf(" s%1d %4s", bzb_slice_get(bp)-1, s);
	} else if (j >= 0) {
	    printf(" S%1d %4s", j, s);
	} else {
	    printf("    %4s", s);
	}
	if (bzb_crit_get(bp) != 0) {
	    printf(" K%1d", bp->bzb_cluster);
	} else if (j < 0) {
	    printf("   ");
	} else {
	    printf(" k%1d", bp->bzb_cluster);
	}
	if (bp->bzb_mount_point[0] != 0) {
	    printf("  %.64s", bp->bzb_mount_point);
	}
    }
#endif
    printf("\n");
}


void
list_all_disks()
{
    char name[20];
    int i;
    int fd;
    DPME * data;
    long t;

    data = (DPME *) malloc(PBLOCK_SIZE);
    if (data == NULL) {
	error(errno, "can't allocate memory for try buffer");
	return;
    }
    for (i = 0; i < 7; i++) {
	sprintf(name, "/dev/sd%c", 'a'+i);
	if ((fd = open_device(name, O_RDONLY)) < 0) {
#ifdef __linux__
	    if (errno == EACCES) {
		error(errno, "can't open file '%s'", name);
	    }
#else
	    error(errno, "can't open file '%s'", name);
#endif
	    continue;
	}
	if (read_block(fd, 1, (char *)data, 1) == 0) {
	    close_device(fd);
	    continue;
	}
	close_device(fd);

	dump(name);
    }
    free(data);
}


void
show_data_structures(partition_map_header *map)
{
    Block0 *zp;
    DDMap *m;
    int i;
    int j;
    partition_map * entry;
    DPME *p;
    BZB *bp;
    char *s;

    if (map == NULL) {
	printf("No partition map exists\n");
	return;
    }
    printf("Header:\n");
    printf("fd=%d (%s)\n", map->fd, (map->regular_file)?"file":"device");
    printf("map %d blocks out of %d,  media %u blocks\n",
	    map->blocks_in_map, map->maximum_in_map, map->media_size);
    printf("Map is%s writeable", (map->writeable)?kStringEmpty:kStringNot);
    printf(", but%s changed\n", (map->changed)?kStringEmpty:kStringNot);
    printf("\n");

    if (map->misc == NULL) {
	printf("No block zero\n");
    } else {
	zp = map->misc;

	printf("Block0:\n");
	printf("signature 0x%x", zp->sbSig);
	if (zp->sbSig == BLOCK0_SIGNATURE) {
	    printf("\n");
	} else {
	    printf(" should be 0x%x\n", BLOCK0_SIGNATURE);
	}
	printf("Block size=%u, Number of Blocks=%u\n",
		zp->sbBlkSize, zp->sbBlkCount);
	printf("DeviceType=0x%x, DeviceId=0x%x, sbData=0x%x\n",
		zp->sbDevType, zp->sbDevId, zp->sbData);
	if (zp->sbDrvrCount == 0) {
	    printf("No drivers\n");
	} else {
	    printf("%u driver%s-\n", zp->sbDrvrCount,
		    (zp->sbDrvrCount>1)?"s":kStringEmpty);
	    m = (DDMap *) zp->sbMap;
	    for (i = 0; i < zp->sbDrvrCount; i++) {
		printf("%u: @ %u for %u, type=0x%x\n", i+1, m[i].ddBlock,
			m[i].ddSize, m[i].ddType);
	    }
	}
    }
    printf("\n");

/*
u32     dpme_boot_args[32]      ;
u32     dpme_reserved_3[62]     ;
*/
    printf(" #:                 type  length   base    "
	    "flags     (logical)\n");
    for (entry = map->disk_order; entry != NULL; entry = entry->next_on_disk) {
	p = entry->data;
	printf("%2d: %20.32s ",
		entry->disk_address, p->dpme_type);
	printf("%7u @ %-7u ", p->dpme_pblocks, p->dpme_pblock_start);
	printf("%c%c%c%c%c%c%c%c%c ",
		(dpme_valid_get(p))?'V':'v',
		(dpme_allocated_get(p))?'A':'a',
		(dpme_in_use_get(p))?'I':'i',
		(dpme_bootable_get(p))?'B':'b',
		(dpme_readable_get(p))?'R':'r',
		(dpme_writable_get(p))?'W':'w',
		(dpme_os_pic_code_get(p))?'P':'p',
		(dpme_os_specific_1_get(p))?'1':'.',
		(dpme_os_specific_2_get(p))?'2':'.');
	if (p->dpme_lblock_start != 0 || p->dpme_pblocks != p->dpme_lblocks) {
	    printf("(%u @ %u)", p->dpme_lblocks, p->dpme_lblock_start);
	}
	printf("\n");
    }
    printf("\n");
    printf(" #:  booter   bytes      load_address      "
	    "goto_address checksum processor\n");
    for (entry = map->disk_order; entry != NULL; entry = entry->next_on_disk) {
	p = entry->data;
	printf("%2d: ", entry->disk_address);
	printf("%7u ", p->dpme_boot_block);
	printf("%7u ", p->dpme_boot_bytes);
	printf("%8x ", p->dpme_load_addr);
	printf("%8x ", p->dpme_load_addr_2);
	printf("%8x ", p->dpme_goto_addr);
	printf("%8x ", p->dpme_goto_addr_2);
	printf("%8x ", p->dpme_checksum);
	printf("%.32s", p->dpme_process_id);
	printf("\n");
    }
    printf("\n");
/*
xx: cccc RU *dd s...
*/
    for (entry = map->disk_order; entry != NULL; entry = entry->next_on_disk) {
	p = entry->data;
	printf("%2d: ", entry->disk_address);

	bp = (BZB *) (p->dpme_bzb);
	j = -1;
	if (bp->bzb_magic == BZBMAGIC) {
	    switch (bp->bzb_type) {
	    case FSTEFS:
		s = "esch";
		break;
	    case FSTSFS:
		s = "swap";
		j = 1;
		break;
	    case FST:
	    default:
		s = "fsys";
		if (bzb_root_get(bp) != 0) {
		    j = 0;
		} else if (bzb_usr_get(bp) != 0) {
		    j = 2;
		}
		break;
	    }
	    printf("%4s ", s);
	    printf("%c%c ",
		    (bzb_root_get(bp))?'R':' ',
		    (bzb_usr_get(bp))?'U':' ');
	    if (bzb_slice_get(bp) != 0) {
		printf("  %2d", bzb_slice_get(bp)-1);
	    } else if (j >= 0) {
		printf(" *%2d", j);
	    } else {
		printf("    ");
	    }
	    if (bp->bzb_mount_point[0] != 0) {
		printf(" %.64s", bp->bzb_mount_point);
	    }
	}
	printf("\n");
    }
}


