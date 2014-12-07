/*
   NetBSD disklabel editor for Linux fdisk
   Written by Bernhard Fastenrath (fasten@informatik.uni-bonn.de)
   with code from the NetBSD disklabel command:
  
   Copyright (c) 1987, 1988 Regents of the University of California.
   All rights reserved.
  
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. All advertising materials mentioning features or use of this software
      must display the following acknowledgement:
  	This product includes software developed by the University of
  	California, Berkeley and its contributors.
   4. Neither the name of the University nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.
  
   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <setjmp.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

#include <sys/ioctl.h>
#include <sys/param.h>

#include "kernel-defs.h"

#include "fdisk.h"
#define DKTYPENAMES
#include "fdisklabel.h"

static void bsd_delete_part (void);
static void bsd_new_part (void);
static void bsd_print_disklabel (int show_all);
static void bsd_write_disklabel (void);
static int bsd_create_disklabel (void);
static void bsd_edit_disklabel (void);
static void bsd_write_bootstrap (void);
static void bsd_change_fstype (void);
static int bsd_get_part_index (int max);
static int bsd_check_new_partition (int *i);
static void bsd_list_types (void);
static u_short bsd_dkcksum (struct disklabel *lp);
static int bsd_initlabel  (struct partition *p, struct disklabel *d, int pindex);
static int bsd_readlabel  (struct partition *p, struct disklabel *d);
static int bsd_writelabel (struct partition *p, struct disklabel *d);
static void sync_disks (void);
#if defined (i386) || defined (__x86_64__)
static int bsd_translate_fstype (int linux_type);
static void bsd_link_part (void);
#endif
#if defined (__alpha__)
void alpha_bootblock_checksum (char *boot);
#endif

static struct disklabel bsd_dlabel;
static char buffer[BSD_BBSIZE];
#if defined (i386) || defined (__x86_64__)
static struct partition *bsd_part;
static int bsd_part_index;
#endif

void
bmenu (void)
{
  puts ("Command action\n"
	"   d   delete a BSD partition\n"
	"   e   edit drive data\n"
	"   i   install bootstrap\n"
	"   l   list known filesystem types\n"
	"   m   print this menu\n"
	"   n   add a new BSD partition\n"
	"   p   print BSD partition table\n"
	"   q   quit without saving changes\n"
#if defined (i386) || defined (__x86_64__)
	"   r   return to main menu\n"
#endif
	"   s   show complete disklabel\n"
	"   t   change a partition's filesystem id\n"
	"   w   write disklabel to disk\n"
#if defined (i386) || defined (__x86_64__)
	"   x   link BSD partition to non-BSD partition"
#endif
	);
}

void
bselect (void)
{
#if defined (i386) || defined (__x86_64__)
  int t;

  for (t=0; t<4; t++)
    if (part_table[t] -> sys_ind == NETBSD_PARTITION)
    {
      bsd_part = part_table[t];
      bsd_part_index = t;
      if (bsd_part -> start_sect == 0)
      {
        fprintf (stderr, "Partition %s%d has invalid starting sector 0.\n",
        	 disk_device, t+1);
        return;
      }
      printf ("Reading disklabel of %s%d at sector %d.\n",
	      disk_device, t+1, bsd_part -> start_sect + BSD_LABELSECTOR);
      if (bsd_readlabel (bsd_part, &bsd_dlabel) == 0)
        if (bsd_create_disklabel () == 0)
	  return;
      break;
    }

  if (t == 4)
  {
    printf ("There is no NetBSD partition on %s.\n", disk_device);
    return;
  }

#elif defined (__alpha__) || defined (__powerpc__)

  if (bsd_readlabel (NULL, &bsd_dlabel) == 0)
    if (bsd_create_disklabel () == 0)
      exit ( EXIT_SUCCESS );

#endif

  while (1)
  {
    putchar ('\n');
    switch (tolower (read_char ("BSD disklabel command (m for help): ")))
    {
      case 'd':
        bsd_delete_part ();
	break;
      case 'e':
        bsd_edit_disklabel ();
	break;
      case 'i':
        bsd_write_bootstrap ();
	break;
      case 'l':
        bsd_list_types ();
	break;
      case 'n':
	bsd_new_part ();
	break;
      case 'p':
	bsd_print_disklabel (0);
	break;
      case 'q':
	close (fd);
	exit ( EXIT_SUCCESS );
      case 's':
	bsd_print_disklabel (1);
	break;
      case 't':
        bsd_change_fstype ();
	break;
      case 'w':
	bsd_write_disklabel ();
	break;
#if defined (i386) || defined (__x86_64__)
      case 'r':
	return;
      case 'x':
	bsd_link_part ();
	break;
#endif
      default:
	bmenu ();
	break;
    }
  }
}

static void
bsd_delete_part (void)
{
  int i;

  i = bsd_get_part_index (bsd_dlabel.d_npartitions);
  bsd_dlabel.d_partitions[i].p_size   = 0;
  bsd_dlabel.d_partitions[i].p_offset = 0;
  bsd_dlabel.d_partitions[i].p_fstype = BSD_FS_UNUSED;  
  if (bsd_dlabel.d_npartitions == i + 1)
    while (bsd_dlabel.d_partitions[bsd_dlabel.d_npartitions-1].p_size == 0)
      bsd_dlabel.d_npartitions--;
}

static void
bsd_new_part (void)
{
  uint begin, end;
  char mesg[48];
  int i;

  if (!bsd_check_new_partition (&i))
    return;

#if defined (i386) || defined (__x86_64__)
  begin = bsd_part -> start_sect;
  end = begin + bsd_part -> nr_sects - 1;
#elif defined (__alpha__) || defined (__powerpc__)
  begin = 0;
  end = bsd_dlabel.d_secperunit;
#endif

  sprintf (mesg, "First %s", str_units());
  begin = read_int (cround (begin), cround (begin), cround (end), ignore, mesg);

  sprintf (mesg, "Last %s or +size or +sizeM or +sizeK", str_units());
  end = read_int (cround (begin), cround (end), cround (end), ignore, mesg);

  if (unit_flag)
  {
    begin = (begin - 1) * display_factor;
    end = end * display_factor - 1;
  }
  bsd_dlabel.d_partitions[i].p_size   = end - begin + 1;
  bsd_dlabel.d_partitions[i].p_offset = begin;
  bsd_dlabel.d_partitions[i].p_fstype = BSD_FS_UNUSED; 
}

static void
bsd_print_disklabel (int show_all)
{
  struct disklabel *lp = &bsd_dlabel;
  struct bsd_partition *pp;
  FILE *f = stdout;
  int i, j;

  if (show_all)
  {
#if defined (i386) || defined (__x86_64__)
    fprintf(f, "# %s%d:\n", disk_device, bsd_part_index+1);
#elif defined (__alpha__) || defined (__powerpc__)
    fprintf(f, "# %s:\n", disk_device);
#endif
    if ((unsigned) lp->d_type < BSD_DKMAXTYPES)
      fprintf(f, "type: %s\n", bsd_dktypenames[lp->d_type]);
    else
      fprintf(f, "type: %d\n", lp->d_type);
    fprintf(f, "disk: %.*s\n", sizeof(lp->d_typename), lp->d_typename);
    fprintf(f, "label: %.*s\n", sizeof(lp->d_packname), lp->d_packname);
    fprintf(f, "flags:");
    if (lp->d_flags & BSD_D_REMOVABLE)
      fprintf(f, " removable");
    if (lp->d_flags & BSD_D_ECC)
      fprintf(f, " ecc");
    if (lp->d_flags & BSD_D_BADSECT)
      fprintf(f, " badsect");
    fprintf(f, "\n");
    fprintf(f, "bytes/sector: %d\n", lp->d_secsize);
    fprintf(f, "sectors/track: %d\n", lp->d_nsectors);
    fprintf(f, "tracks/cylinder: %d\n", lp->d_ntracks);
    fprintf(f, "sectors/cylinder: %d\n", lp->d_secpercyl);
    fprintf(f, "cylinders: %d\n", lp->d_ncylinders);
    fprintf(f, "rpm: %d\n", lp->d_rpm);
    fprintf(f, "interleave: %d\n", lp->d_interleave);
    fprintf(f, "trackskew: %d\n", lp->d_trackskew);
    fprintf(f, "cylinderskew: %d\n", lp->d_cylskew);
    fprintf(f, "headswitch: %d\t\t# milliseconds\n", lp->d_headswitch);
    fprintf(f, "track-to-track seek: %d\t# milliseconds\n", lp->d_trkseek);
    fprintf(f, "drivedata: ");
    for (i = NDDATA - 1; i >= 0; i--)
      if (lp->d_drivedata[i])
	break;
    if (i < 0)
      i = 0;
    for (j = 0; j <= i; j++)
      fprintf(f, "%d ", lp->d_drivedata[j]);
  }
  fprintf (f, "\n%d partitions:\n", lp->d_npartitions);
  fprintf (f, "#        size   offset    fstype   [fsize bsize   cpg]\n");
  pp = lp->d_partitions;
  for (i = 0; i < lp->d_npartitions; i++, pp++) {
    if (pp->p_size) {
      fprintf(f, "  %c: %8d %8d  ", 'a' + i,
	      pp->p_size, pp->p_offset);
      if ((unsigned) pp->p_fstype < BSD_FSMAXTYPES)
	fprintf(f, "%8.8s", bsd_fstypes[pp->p_fstype].name);
      else
	fprintf(f, "%8x", pp->p_fstype);
      switch (pp->p_fstype)
      {
        case BSD_FS_UNUSED:
	  fprintf(f, "    %5d %5d %5.5s ",
		  pp->p_fsize, pp->p_fsize * pp->p_frag, "");
	  break;
	  
	case BSD_FS_BSDFFS:
	  fprintf(f, "    %5d %5d %5d ",
		  pp->p_fsize, pp->p_fsize * pp->p_frag,
		  pp->p_cpg);
	  break;
	  
	default:
	  fprintf(f, "%20.20s", "");
	  break;
      }
      fprintf(f, "\t# (Cyl. %4d",
#if 0
	      pp->p_offset / lp->d_secpercyl); /* differs from Linux fdisk */
#else
	      pp->p_offset / lp->d_secpercyl + 1);
#endif
      if (pp->p_offset % lp->d_secpercyl)
	putc('*', f);
      else
	putc(' ', f);
      fprintf(f, "- %d",
	      (pp->p_offset + 
	       pp->p_size + lp->d_secpercyl - 1) /
#if 0
	      lp->d_secpercyl - 1); /* differs from Linux fdisk */
#else
	      lp->d_secpercyl);
#endif
      if (pp->p_size % lp->d_secpercyl)
	putc('*', f);
      fprintf(f, ")\n");
    }
  }
}

static void
bsd_write_disklabel (void)
{
#if defined (i386) || defined (__x86_64__)
  printf ("Writing disklabel to %s%d.\n", disk_device, bsd_part_index+1);
  bsd_writelabel (bsd_part, &bsd_dlabel);
#elif defined (__alpha__) || defined (__powerpc__)
  printf ("Writing disklabel to %s.\n", disk_device);
  bsd_writelabel (NULL, &bsd_dlabel);
#endif
}

static int
bsd_create_disklabel (void)
{
  char c;

#if defined (i386) || defined (__x86_64__)
  fprintf (stderr, "%s%d contains no disklabel.\n",
	   disk_device, bsd_part_index+1);
#elif defined (__alpha__) || defined (__powerpc__)
  fprintf (stderr, "%s contains no disklabel.\n", disk_device);
#endif

  while (1)
    if ((c = tolower (read_char ("Do you want to create a disklabel? (y/n) "))) == 'y')
    {
#if defined (i386) || defined (__x86_64__)
      if (bsd_initlabel (bsd_part, &bsd_dlabel, bsd_part_index) == 1)
#elif defined (__alpha__) || defined (__powerpc__) || defined (__mc68000__)
      if (bsd_initlabel (NULL, &bsd_dlabel, 0) == 1)
#endif
      {
        bsd_print_disklabel (1);
	return 1;
      }
      else
	return 0;
    }
    else if (c == 'n')
      return 0;
}

static int
edit_int (int def, char *mesg)
{
  do {
    fputs (mesg, stdout);
    printf (" (%d): ", def);
    if (!read_line ())
      return def;
  }
  while (!isdigit (*line_ptr));
  return atoi (line_ptr);
} 

static void
bsd_edit_disklabel (void)
{
  struct disklabel *d;

  d = &bsd_dlabel;

#if defined (__alpha__) || defined (__powerpc__)
  d -> d_secsize    = (u_long) edit_int ((u_long) d -> d_secsize     ,"bytes/sector");
  d -> d_nsectors   = (u_long) edit_int ((u_long) d -> d_nsectors    ,"sectors/track");
  d -> d_ntracks    = (u_long) edit_int ((u_long) d -> d_ntracks     ,"tracks/cylinder");
  d -> d_ncylinders = (u_long) edit_int ((u_long) d -> d_ncylinders  ,"cylinders");
#endif

  /* d -> d_secpercyl can be != d -> d_nsectors * d -> d_ntracks */
  while (1)
  {
    d -> d_secpercyl = (u_long) edit_int ((u_long) d -> d_nsectors * d -> d_ntracks,
					  "sectors/cylinder");
    if (d -> d_secpercyl <= d -> d_nsectors * d -> d_ntracks)
      break;

    printf ("Must be <= sectors/track * tracks/cylinder (default).\n");
  }
  d -> d_rpm        = (u_short) edit_int ((u_short) d -> d_rpm       ,"rpm");
  d -> d_interleave = (u_short) edit_int ((u_short) d -> d_interleave,"interleave");
  d -> d_trackskew  = (u_short) edit_int ((u_short) d -> d_trackskew ,"trackskew");
  d -> d_cylskew    = (u_short) edit_int ((u_short) d -> d_cylskew   ,"cylinderskew");
  d -> d_headswitch = (u_long) edit_int ((u_long) d -> d_headswitch  ,"headswitch");
  d -> d_trkseek    = (u_long) edit_int ((u_long) d -> d_trkseek     ,"track-to-track seek");

  d -> d_secperunit = d -> d_secpercyl * d -> d_ncylinders;
}

static int
bsd_get_bootstrap (char *path, void *ptr, int size)
{
  int fd;

  if ((fd = open (path, O_RDONLY)) < 0)
  {
    perror (path);
    return 0;
  }
  if (read (fd, ptr, size) < 0)
  {
    perror (path);
    close (fd);
    return 0;
  }
  printf (" ... %s\n", path);
  close (fd);
  return 1;
}

static void
bsd_write_bootstrap (void)
{
  char *bootdir = BSD_LINUX_BOOTDIR;
  char path[MAXPATHLEN];
  char *dkbasename;
  struct disklabel dl;
  char *d, *p, *e;
  int sector;

  if (bsd_dlabel.d_type == BSD_DTYPE_SCSI)
    dkbasename = "sd";
  else
    dkbasename = "wd";

  printf ("Bootstrap: %sboot -> boot%s (%s): ", dkbasename, dkbasename, dkbasename);
  if (read_line ())
  {
    line_ptr[strlen (line_ptr)-1] = '\0';
    dkbasename = line_ptr;
  }
  sprintf (path, "%s/%sboot", bootdir, dkbasename);
  if (!bsd_get_bootstrap (path, buffer, (int) bsd_dlabel.d_secsize))
    return;

  /* We need a backup of the disklabel (bsd_dlabel might have changed). */
  d = &buffer[BSD_LABELSECTOR * SECTOR_SIZE];
  bcopy (d, &dl, sizeof (struct disklabel));

  /* The disklabel will be overwritten by 0's from bootxx anyway */
  bzero (d, sizeof (struct disklabel));

  sprintf (path, "%s/boot%s", bootdir, dkbasename);
  if (!bsd_get_bootstrap (path, &buffer[bsd_dlabel.d_secsize],
			  (int) bsd_dlabel.d_bbsize - bsd_dlabel.d_secsize))
    return;

  e = d + sizeof (struct disklabel);
  for (p=d; p < e; p++)
    if (*p)
    {
      fprintf (stderr, "Bootstrap overlaps with disk label!\n");
      exit ( EXIT_FAILURE );
    }

  bcopy (&dl, d, sizeof (struct disklabel));

#if defined (i386) || defined (__x86_64__)
  sector = bsd_part -> start_sect;
#elif defined (__powerpc__)
  sector = 0;
#elif defined (__alpha__)
  sector = 0;
  alpha_bootblock_checksum (buffer);
#endif

  if (lseek64 (fd, sector * SECTOR_SIZE, SEEK_SET) == -1)
    fatal (unable_to_seek);
  if (BSD_BBSIZE != write (fd, buffer, BSD_BBSIZE))
    fatal (unable_to_write);

#if defined (i386) || defined (__x86_64__)
  printf ("Bootstrap installed on %s%d.\n", disk_device, bsd_part_index+1);
#elif defined (__alpha__) || defined (__powerpc__)
  printf ("Bootstrap installed on %s.\n", disk_device);
#endif

  sync_disks ();
}

static void
bsd_change_fstype (void)
{
  int i;

  i = bsd_get_part_index (bsd_dlabel.d_npartitions);
  bsd_dlabel.d_partitions[i].p_fstype = read_hex (bsd_fstypes, BSD_FSMAXTYPES);
}

static int
bsd_get_part_index (int max)
{
  char prompt[40];
  char l;

  sprintf (prompt, "Partition (a-%c): ", 'a' + max - 1);
  do
     l = tolower (read_char (prompt));
  while (l < 'a' || l > 'a' + max - 1);
  return l - 'a';
}

static int
bsd_check_new_partition (int *i)
{
  int t;

  if (bsd_dlabel.d_npartitions == BSD_MAXPARTITIONS)
  {
    for (t=0; t < BSD_MAXPARTITIONS; t++)
      if (bsd_dlabel.d_partitions[t].p_size == 0)
	break;

    if (t == BSD_MAXPARTITIONS)
    {
      fprintf (stderr, "The maximum number of partitions has been created\n");
      return 0;
    }
  }
  *i = bsd_get_part_index (BSD_MAXPARTITIONS);

  if (*i >= bsd_dlabel.d_npartitions)
    bsd_dlabel.d_npartitions = (*i) + 1;

  if (bsd_dlabel.d_partitions[*i].p_size != 0)
  {
    fprintf (stderr, "This partition already exists.\n");
    return 0;
  }
  return 1;
}

static void
bsd_list_types (void)
{
  list_types (bsd_fstypes, BSD_FSMAXTYPES);
}

static u_short
bsd_dkcksum (struct disklabel *lp)
{
  register u_short *start, *end;
  register u_short sum = 0;
  
  start = (u_short *)lp;
  end = (u_short *)&lp->d_partitions[lp->d_npartitions];
  while (start < end)
    sum ^= *start++;
  return (sum);
}

static int
bsd_initlabel (struct partition *p, struct disklabel *d, int pindex)
{
  struct hd_geometry geometry;
  struct bsd_partition *pp;

  if (ioctl (fd, HDIO_GETGEO, &geometry) == -1)
  {
    perror ("ioctl");
    return 0;
  }
  bzero (d, sizeof (struct disklabel));

  d -> d_magic = BSD_DISKMAGIC;

  if (strncmp (disk_device, "/dev/sd", 7) == 0)
    d -> d_type = BSD_DTYPE_SCSI;
  else
    d -> d_type = BSD_DTYPE_ST506;

#if 0 /* not used (at least not written to disk) by NetBSD/i386 1.0 */
  d -> d_subtype = BSD_DSTYPE_INDOSPART & pindex;
#endif

#if defined (i386) || defined (__x86_64__)
  d -> d_flags = BSD_D_DOSPART;
#else
  d -> d_flags = 0;
#endif
  d -> d_secsize = SECTOR_SIZE;       /* bytes/sector  */
  d -> d_nsectors = geometry.sectors; /* sectors/track */
  d -> d_ntracks = geometry.heads;    /* tracks/cylinder (heads) */
  d -> d_ncylinders = geometry.cylinders;
  d -> d_secpercyl  = geometry.sectors * geometry.heads; /* sectors/cylinder */
  d -> d_secperunit = d -> d_secpercyl * d -> d_ncylinders;

  d -> d_rpm = 3600;
  d -> d_interleave = 1;
  d -> d_trackskew = 0;
  d -> d_cylskew = 0;
  d -> d_headswitch = 0;
  d -> d_trkseek = 0;

  d -> d_magic2 = BSD_DISKMAGIC;
  d -> d_bbsize = BSD_BBSIZE;
  d -> d_sbsize = BSD_SBSIZE;

#if defined (i386) || defined (__x86_64__)
  d -> d_npartitions = 4;
  pp = &d -> d_partitions[2]; /* Partition C should be the NetBSD partition */
  pp -> p_offset = p -> start_sect;
  pp -> p_size   = p -> nr_sects;
  pp -> p_fstype = BSD_FS_UNUSED;
  pp = &d -> d_partitions[3]; /* Partition D should be the whole disk */
  pp -> p_offset = 0;
  pp -> p_size   = d -> d_secperunit;
  pp -> p_fstype = BSD_FS_UNUSED;
#elif defined (__alpha__) || defined (__powerpc__)
  d -> d_npartitions = 3;
  pp = &d -> d_partitions[2]; /* Partition C should be the whole disk */
  pp -> p_offset = 0;
  pp -> p_size   = d -> d_secperunit;
  pp -> p_fstype = BSD_FS_UNUSED;  
#endif

  return 1;
}

static int
bsd_readlabel (struct partition *p, struct disklabel *d)
{
  int t, sector;

#if defined (i386) || defined (__x86_64__)
  sector = p -> start_sect;
#elif defined (__alpha__) || defined (__powerpc__)
  sector = 0;
#endif

  if (lseek64 (fd, sector * SECTOR_SIZE, SEEK_SET) == -1)
    fatal (unable_to_seek);
  if (BSD_BBSIZE != read (fd, buffer, BSD_BBSIZE))
    fatal (unable_to_read);

  bcopy (&buffer[BSD_LABELSECTOR * SECTOR_SIZE + BSD_LABELOFFSET],
	 d, sizeof (struct disklabel));

  for (t = d -> d_npartitions; t < BSD_MAXPARTITIONS; t++)
  {
    d -> d_partitions[t].p_size   = 0;
    d -> d_partitions[t].p_offset = 0;
    d -> d_partitions[t].p_fstype = BSD_FS_UNUSED;  
  }
  if (d -> d_magic != BSD_DISKMAGIC || d -> d_magic2 != BSD_DISKMAGIC)
    return 0;

  if (d -> d_npartitions > BSD_MAXPARTITIONS)
    fprintf (stderr, "Warning: too many partitions (%d, maximum is %d).\n",
	     d -> d_npartitions, BSD_MAXPARTITIONS);
  return 1;
}

static int
bsd_writelabel (struct partition *p, struct disklabel *d)
{
  int sector;

#if defined (i386) || defined (__x86_64__)
  sector = p -> start_sect + BSD_LABELSECTOR;
#elif defined (__alpha__) || defined (__powerpc__)
  sector = BSD_LABELSECTOR;
#endif

  d -> d_checksum = 0;
  d -> d_checksum = bsd_dkcksum (d);

  /* This is necessary if we want to write the bootstrap later,
     otherwise we'd write the old disklabel with the bootstrap.
  */
  bcopy (d, &buffer[BSD_LABELSECTOR * SECTOR_SIZE + BSD_LABELOFFSET],
	 sizeof (struct disklabel));

#if defined (__alpha__) && BSD_LABELSECTOR == 0
  alpha_bootblock_checksum (buffer);
  if (lseek64 (fd, 0, SEEK_SET) == -1)
    fatal (unable_to_seek);
  if (BSD_BBSIZE != write (fd, buffer, BSD_BBSIZE))
    fatal (unable_to_write);
#else
  if (lseek64 (fd, sector * SECTOR_SIZE + BSD_LABELOFFSET, SEEK_SET) == -1)
    fatal (unable_to_seek);
  if (sizeof (struct disklabel) != write (fd, d, sizeof (struct disklabel)))
    fatal (unable_to_write);
#endif

  sync_disks ();

  return 1;
}

static void
sync_disks (void)
{
  printf ("\nSyncing disks.\n");
  sync ();
  sleep (4);
}

#if defined (i386) || defined (__x86_64__)
static int
bsd_translate_fstype (int linux_type)
{
  switch (linux_type)
  {
    case 0x01: /* DOS 12-bit FAT   */
    case 0x04: /* DOS 16-bit <32M  */
    case 0x06: /* DOS 16-bit >=32M */
    case 0xe1: /* DOS access       */
    case 0xe3: /* DOS R/O          */
    case 0xf2: /* DOS secondary    */
      return BSD_FS_MSDOS;
    case 0x07: /* OS/2 HPFS        */
      return BSD_FS_HPFS;
    default:
      return BSD_FS_OTHER;
  }
}

static void
bsd_link_part (void)
{
  int k, i;

  k = get_partition (1, partitions);

  if (!bsd_check_new_partition (&i))
    return;

  bsd_dlabel.d_partitions[i].p_size   = part_table[k] -> nr_sects;
  bsd_dlabel.d_partitions[i].p_offset = part_table[k] -> start_sect;
  bsd_dlabel.d_partitions[i].p_fstype =
    bsd_translate_fstype (part_table[k] -> sys_ind);
}
#endif

#if defined (__alpha__)
typedef unsigned long long u_int64_t;

void
alpha_bootblock_checksum (char *boot)
{
  u_int64_t *dp, sum;
  int i;
  
  dp = (u_int64_t *)boot;
  sum = 0;
  for (i = 0; i < 63; i++)
    sum += dp[i];
  dp[63] = sum;
}
#endif /* __alpha__ */
