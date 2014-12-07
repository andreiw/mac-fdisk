//
// partition_map.c - partition map routines
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
#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <sys/stat.h>
#endif

#include "partition_map.h"
#include "pdisk.h"
#include "convert.h"
#include "io.h"
#include "errors.h"


//
// Defines
//
// #define TEST_COMPUTE


//
// Types
//


//
// Global Constants
//
const char * kFreeType	= "Apple_Free";
const char * kMapType	= "Apple_partition_map";
const char * kUnixType	= "Apple_UNIX_SVR2";

const char * kFreeName = "Extra";

enum add_action {
    kReplace = 0,
    kAdd = 1,
    kSplit = 2
};

//
// Global Variables
//


//
// Forward declarations
//
int add_data_to_map(struct dpme *data, long index, partition_map_header *map);
void coerce_block0(partition_map_header *map);
int contains_driver(partition_map *entry);
void combine_entry(partition_map *entry);
long compute_device_size(int fd);
DPME* create_data(const char *name, const char *dptype, u32 base, u32 length);
partition_map_header* create_partition_map(char *name);
void delete_entry(partition_map *entry);
void insert_in_base_order(partition_map *entry);
void insert_in_disk_order(partition_map *entry);
int read_partition_map(partition_map_header *map);
void remove_from_disk_order(partition_map *entry);
void renumber_disk_addresses(partition_map_header *map);


//
// Routines
//
partition_map_header *
open_partition_map(char *name, int *valid_file)
{
    int fd;
    partition_map_header * map;
    int writeable;
    unsigned long length;
#ifdef __linux__
    struct stat info;
#endif

    fd = open_device(name, (rflag)?O_RDONLY:O_RDWR);
    if (fd < 0) {
	fd = open_device(name, O_RDONLY);
	if (fd < 0) {
	    error(errno, "can't open file '%s'", name);
	    *valid_file = 0;
	    return NULL;
	} else {
	    writeable = 0;
	}
    } else {
	writeable = 1;
    }
    *valid_file = 1;

    map = (partition_map_header *) malloc(sizeof(partition_map_header));
    if (map == NULL) {
	error(errno, "can't allocate memory for open partition map");
	close_device(fd);
	return NULL;
    }
    map->fd = fd;
    map->name = name;
    map->writeable = (rflag)?0:writeable;
    map->changed = 0;
    map->disk_order = NULL;
    map->base_order = NULL;
    map->blocks_in_map = 0;
    map->maximum_in_map = -1;
    map->media_size = compute_device_size(fd);

#ifdef __linux__
    if (fstat(fd, &info) < 0) {
	error(errno, "can't stat file '%s'", name);
	map->regular_file = 0;
    } else {
	map->regular_file = S_ISREG(info.st_mode);
    }
#else
    map->regular_file = 0;
#endif

    map->misc = (Block0 *) malloc(PBLOCK_SIZE);
    if (map->misc == NULL) {
	error(errno, "can't allocate memory for block zero buffer");
    } else if (read_block(fd, 0, (char *)map->misc, 0) == 0
	    || convert_block0(map->misc, 1)) {
	// if I can't read block 0 I might as well give up
    } else if (read_partition_map(map) < 0) {
	// some sort of failure reading the map
    } else {
	// got it!
	coerce_block0(map);
	return map;
    }
    close_partition_map(map);
    return NULL;
}


void
close_partition_map(partition_map_header *map)
{
    partition_map * entry;
    partition_map * next;

    if (map == NULL) {
	return;
    }

    free(map->misc);

    for (entry = map->disk_order; entry != NULL; entry = next) {
	next = entry->next_on_disk;
	free(entry->data);
	free(entry);
    }
    close_device(map->fd);
    free(map);
}


int
read_partition_map(partition_map_header *map)
{
    DPME *data;
    u32 limit;
    int index;

    data = (DPME *) malloc(PBLOCK_SIZE);
    if (data == NULL) {
	error(errno, "can't allocate memory for disk buffers");
	return -1;
    }

    if (read_block(map->fd, 1, (char *)data, 0) == 0) {
	free(data);
	return -1;
    } else if (convert_dpme(data, 1)
	    || data->dpme_signature != DPME_SIGNATURE) {
	free(data);
	return -1;
    } else {
	limit = data->dpme_map_entries;
	index = 1;
	while (1) {
	    if (add_data_to_map(data, index, map) == 0) {
		free(data);
		return -1;
	    }

	    if (index >= limit) {
		break;
	    } else {
		index++;
	    }

	    data = (DPME *) malloc(PBLOCK_SIZE);
	    if (data == NULL) {
		error(errno, "can't allocate memory for disk buffers");
		return -1;
	    }

	    if (read_block(map->fd, index, (char *)data, 0) == 0) {
		free(data);
		return -1;
	    } else if (convert_dpme(data, 1)
		    || data->dpme_signature != DPME_SIGNATURE
		    || data->dpme_map_entries != limit) {
		free(data);
		return -1;
	    }
	}
    }
    return 0;
}


void
write_partition_map(partition_map_header *map)
{
    int fd;
    char *block;
    partition_map * entry;
    int i;
    int saved_errno;

    fd = map->fd;
    if (map->misc != NULL) {
	convert_block0(map->misc, 0);
	write_block(fd, 0, (char *)map->misc);
	convert_block0(map->misc, 1);
    } else {
	block = (char *) calloc(1, PBLOCK_SIZE);
	if (block != NULL) {
	    write_block(fd, 0, block);
	    free(block);
	}
    }
    for (entry = map->disk_order; entry != NULL; entry = entry->next_on_disk) {
	convert_dpme(entry->data, 0);
	write_block(fd, entry->disk_address, (char *)entry->data);
	convert_dpme(entry->data, 1);
	i = entry->disk_address;
    }
	// zap the block after the map (if possible) to get around a bug.
    if (map->maximum_in_map > 0 &&  i < map->maximum_in_map) {
	i += 1;
	block = (char *) malloc(PBLOCK_SIZE);
	if (block != NULL) {
	    if (read_block(fd, i, block, 1)) {
		block[0] = 0;
		write_block(fd, i, block);
	    }
	    free(block);
	}
    }
    printf("The partition table has been altered!\n\n");

#ifdef __linux__
    if (map->regular_file) {
	close_device(map->fd);
    } else {
	// printf("Calling ioctl() to re-read partition table.\n"
	//       "(Reboot to ensure the partition table has been updated.)\n");
	sync();
	sleep(2);
	if ((i = ioctl(fd, BLKRRPART)) != 0) {
	    saved_errno = errno;
	} else {
	    // some kernel versions (1.2.x) seem to have trouble
	    // rereading the partition table, but if asked to do it
	    // twice, the second time works. - biro@yggdrasil.com */
	    sync();
	    sleep(2);
	    if ((i = ioctl(fd, BLKRRPART)) != 0) {
		saved_errno = errno;
	    }
	}
	close_device(map->fd);

	// printf("Syncing disks.\n");
	sync();
	sleep(4);		/* for sync() */

	if (i < 0) {
	    error(saved_errno, "Re-read of partition table failed");
	    printf("Reboot your system to ensure the "
		    "partition table is updated.\n");
	}
    }
#else
    close_device(map->fd);
#endif
    map->fd = open_device(map->name, (map->writeable)?O_RDWR:O_RDONLY);
    if (map->fd < 0) {
	fatal(errno, "can't re-open file '%s' for %sing", map->name,
		(rflag)?"read":"writ");
    }

}


int
add_data_to_map(struct dpme *data, long index, partition_map_header *map)
{
    partition_map *entry;

    entry = (partition_map *) malloc(sizeof(partition_map));
    if (entry == NULL) {
	error(errno, "can't allocate memory for map entries");
	return 0;
    }
    entry->next_on_disk = NULL;
    entry->prev_on_disk = NULL;
    entry->next_by_base = NULL;
    entry->prev_by_base = NULL;
    entry->disk_address = index;
    entry->the_map = map;
    entry->data = data;

    insert_in_disk_order(entry);
    insert_in_base_order(entry);

    map->blocks_in_map++;
    if (map->maximum_in_map < 0) {
	if (strncmp(data->dpme_type, kMapType, DPISTRLEN) == 0) {
	    map->maximum_in_map = data->dpme_pblocks;
	}
    }

    return 1;
}


partition_map_header *
init_partition_map(char *name, partition_map_header* oldmap)
{
    partition_map_header *map;

    if (oldmap != NULL) {
	printf("map already exists\n");
	if (get_okay("do you want to reinit? [n/y]: ", 0) != 1) {
	    return oldmap;
	}
    }

    map = create_partition_map(name);
    if (map == NULL) {
	return oldmap;
    }
    close_partition_map(oldmap);

    add_partition_to_map("Apple", kMapType,
	    1, (map->media_size <= 128? 2: 63), map);
    return map;
}


partition_map_header *
create_partition_map(char *name)
{
    int fd;
    partition_map_header * map;
    unsigned long length;
    DPME *data;
    int ok;
    unsigned long number;
#ifdef __linux__
    struct stat info;
#endif

    fd = open_device(name, (rflag)?O_RDONLY:O_RDWR);
    if (fd < 0) {
	error(errno, "can't open file '%s' for %sing", name,
		(rflag)?"read":"writ");
	return NULL;
    }

    map = (partition_map_header *) malloc(sizeof(partition_map_header));
    if (map == NULL) {
	error(errno, "can't allocate memory for open partition map");
	close_device(fd);
	return NULL;
    }
    map->fd = fd;
    map->name = name;
    map->writeable = (rflag)?0:1;
    map->changed = 0;
    map->disk_order = NULL;
    map->base_order = NULL;
    map->blocks_in_map = 0;
    map->maximum_in_map = -1;

    number = compute_device_size(fd);
    printf("size of 'device' is %u blocks: ", number);
    flush_to_newline(0);
    get_number_argument("what should be the size? ", (long *)&number, number);
    if (number < 4) {
	number = 4;
    }
    printf("new size of 'device' is %u blocks\n", number);
    map->media_size = number;

#ifdef __linux__
    if (fstat(fd, &info) < 0) {
	error(errno, "can't stat file '%s'", name);
	map->regular_file = 0;
    } else {
	map->regular_file = S_ISREG(info.st_mode);
    }
#else
    map->regular_file = 0;
#endif

    map->misc = (Block0 *) malloc(PBLOCK_SIZE);
    if (map->misc == NULL) {
	error(errno, "can't allocate memory for block zero buffer");
    } else {
	// got it!
	data = (DPME *) calloc(1, PBLOCK_SIZE);
	if (data == NULL) {
	    error(errno, "can't allocate memory for disk buffers");
	} else {
	    // set data into entry
	    data->dpme_signature = DPME_SIGNATURE;
	    data->dpme_map_entries = 1;
	    data->dpme_pblock_start = 1;
	    data->dpme_pblocks = map->media_size - 1;
	    strncpy(data->dpme_name, kFreeName, DPISTRLEN);
	    strncpy(data->dpme_type, kFreeType, DPISTRLEN);
	    data->dpme_lblock_start = 0;
	    data->dpme_lblocks = data->dpme_pblocks;
	    dpme_writable_set(data, 1);
	    dpme_readable_set(data, 1);
	    dpme_bootable_set(data, 0);
	    dpme_in_use_set(data, 0);
	    dpme_allocated_set(data, 0);
	    dpme_valid_set(data, 1);

	    if (add_data_to_map(data, 1, map) == 0) {
		free(data);
	    } else {
		map->changed = 1;
		coerce_block0(map);
		return map;
	    }
	}
    }
    close_partition_map(map);
    return NULL;
}


void
coerce_block0(partition_map_header *map)
{
    Block0 *p;

    p = map->misc;
    if (p == NULL) {
	return;
    }
    if (p->sbSig != BLOCK0_SIGNATURE) {
	p->sbSig = BLOCK0_SIGNATURE;
	p->sbBlkSize = 512;
	p->sbBlkCount = map->media_size;
	p->sbDevType = 0;
	p->sbDevId = 0;
	p->sbData = 0;
	p->sbDrvrCount = 0;
    }
}


int
add_partition_to_map(const char *name, const char *dptype, u32 base, u32 length,
	partition_map_header *map)
{
    partition_map * cur;
    DPME *data;
    enum add_action act;
    int limit;
    u32 adjusted_base;
    u32 adjusted_length;
    u32 new_base;
    u32 new_length;

	// find a block that starts includes base and length
    cur = map->base_order;
    while (cur != NULL) {
	if (cur->data->dpme_pblock_start <= base 
		&& (base + length) <=
		    (cur->data->dpme_pblock_start + cur->data->dpme_pblocks)) {
	    break;
	} else {
	    cur = cur->next_by_base;
	}
    }
	// if it is not Extra then punt
    if (cur == NULL
	    || strncmp(cur->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
	printf("requested base and length is not "
		"within an existing free partition\n");
	return 0;
    }
	// figure out what to do and sizes
    data = cur->data;
    if (data->dpme_pblock_start == base) {
	// replace or add
	if (data->dpme_pblocks == length) {
	    act = kReplace;
	} else {
	    act = kAdd;
	    adjusted_base = base + length;
	    adjusted_length = data->dpme_pblocks - length;
	}
    } else {
	// split or add
	if (data->dpme_pblock_start + data->dpme_pblocks == base + length) {
	    act = kAdd;
	    adjusted_base = data->dpme_pblock_start;
	    adjusted_length = base - adjusted_base;
	} else {
	    act = kSplit;
	    new_base = data->dpme_pblock_start;
	    new_length = base - new_base;
	    adjusted_base = base + length;
	    adjusted_length = data->dpme_pblocks - (length + new_length);
	}
    }
	// if the map will overflow then punt
    if (map->maximum_in_map < 0) {
	limit = map->media_size;
    } else {
	limit = map->maximum_in_map;
    }
    if (map->blocks_in_map + act > limit) {
	printf("the map is not big enough\n");
	return 0;
    }

    data = create_data(name, dptype, base, length);
    if (data == NULL) {
	return 0;
    }
    if (act == kReplace) {
	free(cur->data);
	cur->data = data;
    } else {
	    // adjust this block's size
	cur->data->dpme_pblock_start = adjusted_base;
	cur->data->dpme_pblocks = adjusted_length;
	cur->data->dpme_lblocks = adjusted_length;
	    // insert new with block address equal to this one
	if (add_data_to_map(data, cur->disk_address, map) == 0) {
	    free(data);
	} else if (act == kSplit) {
	    data = create_data(kFreeName, kFreeType, new_base, new_length);
	    if (data != NULL) {
		    // insert new with block address equal to this one
		if (add_data_to_map(data, cur->disk_address, map) == 0) {
		    free(data);
		}
	    }
	}
    }
	// renumber disk addresses
    renumber_disk_addresses(map);
	// mark changed
    map->changed = 1;
    return 1;
}


DPME *
create_data(const char *name, const char *dptype, u32 base, u32 length)
{
    DPME *data;

    data = (DPME *) calloc(1, PBLOCK_SIZE);
    if (data == NULL) {
	error(errno, "can't allocate memory for disk buffers");
    } else {
	// set data into entry
	data->dpme_signature = DPME_SIGNATURE;
	data->dpme_map_entries = 1;
	data->dpme_pblock_start = base;
	data->dpme_pblocks = length;
	strncpy(data->dpme_name, name, DPISTRLEN);
	strncpy(data->dpme_type, dptype, DPISTRLEN);
	data->dpme_lblock_start = 0;
	data->dpme_lblocks = data->dpme_pblocks;
	dpme_writable_set(data, 1);
	dpme_readable_set(data, 1);
	dpme_bootable_set(data, 0);
	dpme_in_use_set(data, 0);
	dpme_allocated_set(data, 1);
	dpme_valid_set(data, 1);
    }
    return data;
}


void
renumber_disk_addresses(partition_map_header *map)
{
    partition_map * cur;
    long index;

	// reset disk addresses
    cur = map->disk_order;
    index = 1;
    while (cur != NULL) {
	cur->disk_address = index++;
	cur->data->dpme_map_entries = map->blocks_in_map;
	cur = cur->next_on_disk;
    }
}


long
compute_device_size(int fd)
{
#ifdef TEST_COMPUTE
    unsigned long length;
    struct hd_geometry geometry;
    struct stat info;
    loff_t pos;
#endif
    char* data;
    unsigned long l, r, x;
    int valid;

#ifdef TEST_COMPUTE
    printf("\n");
    if (fstat(fd, &info) < 0) {
	printf("stat of device failed\n");
    } else {
	printf("stat: mode = 0%o, type=%s\n", info.st_mode, 
		(S_ISREG(info.st_mode)? "Regular":
		(S_ISBLK(info.st_mode)?"Block":"Other")));
	printf("size = %d, blocks = %d\n",
		info.st_size, info.st_size/PBLOCK_SIZE);
    }

    if (ioctl(fd, BLKGETSIZE, &length) < 0) {
	printf("get device size failed\n");
    } else {
	printf("BLKGETSIZE:size in blocks = %u\n", length);
    }

    if (ioctl(fd, HDIO_GETGEO, &geometry) < 0) {
	printf("get device geometry failed\n");
    } else {
	printf("HDIO_GETGEO: heads=%d, sectors=%d, cylinders=%d, start=%d,  total=%d\n",
		geometry.heads, geometry.sectors,
		geometry.cylinders, geometry.start,
		geometry.heads*geometry.sectors*geometry.cylinders);
    }

    if ((pos = llseek(fd, 0, SEEK_END)) < 0) {
	printf("llseek to end of device failed\n");
    } else if ((pos = llseek(fd, 0, SEEK_CUR)) < 0) {
	printf("llseek to end of device failed on second try\n");
    } else {
	printf("llseek: pos = %d, blocks=%d\n", pos, pos/PBLOCK_SIZE);
    }
#endif

    data = (char *) malloc(PBLOCK_SIZE);
    if (data == NULL) {
	error(errno, "can't allocate memory for try buffer");
	x = 0;
    } else {
	// double till off end
	l = 0;
	r = 1024;
	while (read_block(fd, r, data, 1) != 0) {
	    l = r;
	    if (r <= 1024) {
		r = r * 1024;
	    } else {
		r = r * 2;
	    }
	    if (r >= (1024*1024*1024)) {
		break;
	    }
	}
	// binary search for end
	while (l <= r) {
	    x = (l + r) / 2;
	    if ((valid = read_block(fd, x, data, 1)) != 0) {
		l = x + 1;
	    } else {
		if (x > 0) {
		    r = x - 1;
		} else {
		    break;
		}
	    }
	}
	if (valid != 0) {
	    x = x + 1;
	}
	// printf("size in blocks = %d\n", x);
	free(data);
    }

    return x;
}


void
delete_partition_from_map(partition_map *entry)
{
    partition_map_header *map;
    DPME *data;

    if (strncmp(entry->data->dpme_type, kMapType, DPISTRLEN) == 0) {
	printf("Can't delete entry for the map itself\n");
	return;
    }
    if (contains_driver(entry)) {
	printf("Can't delete entry for a driver (yet).\n");
	return;
    }
    data = create_data(kFreeName, kFreeType,
	    entry->data->dpme_pblock_start, entry->data->dpme_pblocks);
    if (data == NULL) {
	return;
    }
    free(entry->data);
    entry->data = data;
    combine_entry(entry);
    map = entry->the_map;
    renumber_disk_addresses(map);
    map->changed = 1;
}


int
contains_driver(partition_map *entry)
{
    partition_map_header *map;
    Block0 *p;
    DDMap *m;
    int i;

    map = entry->the_map;
    p = map->misc;
    if (p == NULL) {
	return 0;
    }
    if (p->sbSig != BLOCK0_SIGNATURE) {
	return 0;
    }
    if (p->sbDrvrCount > 0) {
	m = (DDMap *) p->sbMap;
	for (i = 0; i < p->sbDrvrCount; i++) {
	    if (entry->data->dpme_pblock_start <= m[i].ddBlock
		    && (m[i].ddBlock + m[i].ddSize)
			<= (entry->data->dpme_pblock_start
			+ entry->data->dpme_pblocks)) {
		return 1;
	    }
	}
    }
    return 0;
}


void
combine_entry(partition_map *entry)
{
    partition_map *p;

    if (entry == NULL
	    || strncmp(entry->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
	return;
    }
    if (entry->next_by_base != NULL) {
	p = entry->next_by_base;
	if (strncmp(p->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
	    // next is not free
	} else if (entry->data->dpme_pblock_start + entry->data->dpme_pblocks
		!= p->data->dpme_pblock_start) {
	    // next is not contiguous (XXX this is bad)
	} else {
	    entry->data->dpme_pblocks += p->data->dpme_pblocks;
	    entry->data->dpme_lblocks = entry->data->dpme_pblocks;
	    delete_entry(p);
	}
    }
    if (entry->prev_by_base != NULL) {
	p = entry->prev_by_base;
	if (strncmp(p->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
	    // previous is not free
	} else if (p->data->dpme_pblock_start + p->data->dpme_pblocks
		!= entry->data->dpme_pblock_start) {
	    // previous is not contiguous (XXX this is bad)
	} else {
	    entry->data->dpme_pblock_start = p->data->dpme_pblock_start;
	    entry->data->dpme_pblocks += p->data->dpme_pblocks;
	    entry->data->dpme_lblocks = entry->data->dpme_pblocks;
	    delete_entry(p);
	}
    }
}


void
delete_entry(partition_map *entry)
{
    partition_map_header *map;
    partition_map *p;

    map = entry->the_map;
    map->blocks_in_map--;

    remove_from_disk_order(entry);

    p = entry->next_by_base;
    if (map->base_order == entry) {
	map->base_order = p;
    }
    if (p != NULL) {
	p->prev_by_base = entry->prev_by_base;
    }
    if (entry->prev_by_base != NULL) {
	entry->prev_by_base->next_by_base = p;
    }

    free(entry->data);
    free(entry);
}


partition_map *
find_entry_by_disk_address(long index, partition_map_header *map)
{
    partition_map * cur;

    cur = map->disk_order;
    while (cur != NULL) {
	if (cur->disk_address == index) {
	    break;
	}
	cur = cur->next_on_disk;
    }
    return cur;
}


void
move_entry_in_map(long old_index, long index, partition_map_header *map)
{
    partition_map * cur;

    cur = find_entry_by_disk_address(old_index, map);
    if (cur == NULL) {
	printf("No such partition\n");
    } else {
	remove_from_disk_order(cur);
	cur->disk_address = index;
	insert_in_disk_order(cur);
	renumber_disk_addresses(map);
	map->changed = 1;
    }
}


void
remove_from_disk_order(partition_map *entry)
{
    partition_map_header *map;
    partition_map *p;

    map = entry->the_map;
    p = entry->next_on_disk;
    if (map->disk_order == entry) {
	map->disk_order = p;
    }
    if (p != NULL) {
	p->prev_on_disk = entry->prev_on_disk;
    }
    if (entry->prev_on_disk != NULL) {
	entry->prev_on_disk->next_on_disk = p;
    }
    entry->next_on_disk = NULL;
    entry->prev_on_disk = NULL;
}


void
insert_in_disk_order(partition_map *entry)
{
    partition_map_header *map;
    partition_map * cur;

    // find position in disk list & insert
    map = entry->the_map;
    cur = map->disk_order;
    if (cur == NULL || entry->disk_address <= cur->disk_address) {
	map->disk_order = entry;
	entry->next_on_disk = cur;
	if (cur != NULL) {
	    cur->prev_on_disk = entry;
	}
	entry->prev_on_disk = NULL;
    } else {
	for (cur = map->disk_order; cur != NULL; cur = cur->next_on_disk) {
	    if (cur->disk_address <= entry->disk_address
		    && (cur->next_on_disk == NULL
		    || entry->disk_address <= cur->next_on_disk->disk_address)) {
		entry->next_on_disk = cur->next_on_disk;
		cur->next_on_disk = entry;
		entry->prev_on_disk = cur;
		if (entry->next_on_disk != NULL) {
		    entry->next_on_disk->prev_on_disk = entry;
		}
		break;
	    }
	}
    }
}


void
insert_in_base_order(partition_map *entry)
{
    partition_map_header *map;
    partition_map * cur;

    // find position in base list & insert
    map = entry->the_map;
    cur = map->base_order;
    if (cur == NULL
	    || entry->data->dpme_pblock_start <= cur->data->dpme_pblock_start) {
	map->base_order = entry;
	entry->next_by_base = cur;
	if (cur != NULL) {
	    cur->prev_by_base = entry;
	}
	entry->prev_by_base = NULL;
    } else {
	for (cur = map->base_order; cur != NULL; cur = cur->next_by_base) {
	    if (cur->data->dpme_pblock_start <= entry->data->dpme_pblock_start
		    && (cur->next_by_base == NULL
		    || entry->data->dpme_pblock_start
			<= cur->next_by_base->data->dpme_pblock_start)) {
		entry->next_by_base = cur->next_by_base;
		cur->next_by_base = entry;
		entry->prev_by_base = cur;
		if (entry->next_by_base != NULL) {
		    entry->next_by_base->prev_by_base = entry;
		}
		break;
	    }
	}
    }
}


void
resize_map(long new_size, partition_map_header *map)
{
    partition_map * entry;
    partition_map * next;
    int incr;

    // find map entry
    entry = map->base_order;
    while (entry != NULL) {
	if (strncmp(entry->data->dpme_type, kMapType, DPISTRLEN) == 0) {
	    break;
	}
	entry = entry->next_by_base;
    }
    if (entry == NULL) {
	printf("Couldn't find entry for map!\n");
	return;
    }
    next = entry->next_by_base;

	// same size
    if (new_size == entry->data->dpme_pblocks) {
	// do nothing
	return;
    }

	// make it smaller
    if (new_size < entry->data->dpme_pblocks) {
	if (next == NULL
		|| strncmp(next->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
	    incr = 1;
	} else {
	    incr = 0;
	}
	if (new_size < map->blocks_in_map + incr) {
	    printf("New size would be too small\n");
	    return;
	}
	entry->data->dpme_type[0] = 0;
	delete_partition_from_map(entry);
	add_partition_to_map("Apple", kMapType, 1, new_size, map);
	return;
    }

	// make it larger
    if (next == NULL
	    || strncmp(next->data->dpme_type, kFreeType, DPISTRLEN) != 0) {
	printf("No free space to expand into\n");
	return;
    }
    if (entry->data->dpme_pblock_start + entry->data->dpme_pblocks
	    != next->data->dpme_pblock_start) {
	printf("No contiguous free space to expand into\n");
	return;
    }
    if (new_size > entry->data->dpme_pblocks + next->data->dpme_pblocks) {
	printf("No enough free space\n");
	return;
    }
    entry->data->dpme_type[0] = 0;
    delete_partition_from_map(entry);
    add_partition_to_map("Apple", kMapType, 1, new_size, map);
}
