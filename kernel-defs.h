/* from asm/types.h */
typedef unsigned short __u16;
typedef unsigned int __u32;

/* from linux/hdreg.h */
#define HDIO_GETGEO		0x0301	/* get device geometry */

struct hd_geometry {
      unsigned char heads;
      unsigned char sectors;
      unsigned short cylinders;
      unsigned long start;
};

/* from asm/ioctl.h */
/* #define _IOC_NRBITS	8 */
/* #define _IOC_TYPEBITS	8 */
/* #define _IOC_SIZEBITS	13 */
/* #define _IOC_DIRBITS	3 */

#define _IOC_NRMASK	((1 << _IOC_NRBITS)-1)
#define _IOC_TYPEMASK	((1 << _IOC_TYPEBITS)-1)
#define _IOC_SIZEMASK	((1 << _IOC_SIZEBITS)-1)
#define _IOC_DIRMASK	((1 << _IOC_DIRBITS)-1)

#define _IOC_NRSHIFT	0
#define _IOC_TYPESHIFT	(_IOC_NRSHIFT+_IOC_NRBITS)
#define _IOC_SIZESHIFT	(_IOC_TYPESHIFT+_IOC_TYPEBITS)
#define _IOC_DIRSHIFT	(_IOC_SIZESHIFT+_IOC_SIZEBITS)

#ifdef __powerpc__
#define _IOC_NONE	1U
#define _IOC_READ	2U
#define _IOC_WRITE	4U
#else
#define _IOC_NONE	0U
#define _IOC_READ	2U
#define _IOC_WRITE	1U
#endif

#define _IOC(dir,type,nr,size) \
	(((dir)  << _IOC_DIRSHIFT) | \
	 ((type) << _IOC_TYPESHIFT) | \
	 ((nr)   << _IOC_NRSHIFT) | \
	 ((size) << _IOC_SIZESHIFT))
#define _IO(type,nr)		_IOC(_IOC_NONE,(type),(nr),0)

/* from linux/fs.h */
#define BLKRRPART  _IO(0x12,95)      /* re-read partition table */
#define BLKFLSBUF  _IO(0x12,97)      /* flush buffer cache */

/* from linux/genhd.h */
struct partition {
	unsigned char boot_ind;		/* 0x80 - active */
	unsigned char head;		/* starting head */
	unsigned char sector;		/* starting sector */
	unsigned char cyl;		/* starting cylinder */
	unsigned char sys_ind;		/* What partition type */
	unsigned char end_head;		/* end head */
	unsigned char end_sector;	/* end sector */
	unsigned char end_cyl;		/* end cylinder */
	unsigned int start_sect;	/* starting sector counting from 0 */
	unsigned int nr_sects;		/* nr of sectors in partition */
} __attribute__((packed));
