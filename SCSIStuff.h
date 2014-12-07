/*
 * Copyright 1993-97 by Apple Computer, Inc.
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

#ifndef __SCSIExplorer__
#define __SCSIExplorer__
#define DEBUG_INFOLIST          0

#if !defined(__NewTypesDefined__)
#define __NewTypesDefined__
typedef signed char     SInt8;
typedef signed short    SInt16;
typedef signed long     SInt32;
typedef unsigned char   UInt8;
typedef unsigned short  UInt16;
typedef unsigned long   UInt32;
typedef unsigned long   ItemCount;
typedef unsigned long   ByteCount;
#endif

/*
 * Note: this must be SCSI.h from Universal Headers 2.0 - the current version
 * of Think C headers still has the "old" header without SCSI Manager 4.3 support.
 */
#include <SCSI.h>

#include "MacSCSICommand.h"

#ifndef LOG
#define LOG(what)           /* Nothing */
#endif /* LOG */

/*
 *** Common definitions
 */
#ifndef EXTERN
#define EXTERN              extern
#endif
#ifndef TRUE
#define TRUE                1
#define FALSE               0
#endif
#ifndef NULL
#define NULL                0
#endif

enum {
    bit0 = (1 << 0),
    bit1 = (1 << 1),
    bit2 = (1 << 2),
    bit3 = (1 << 3),
    bit4 = (1 << 4),
    bit5 = (1 << 5),
    bit6 = (1 << 6),
    bit7 = (1 << 7)
};

#define kOriginalSCSIBusAdaptor (0xFF)
#define kEndOfLine              0x0D    /* Return           */


#define UNUSED(what) do {   \
	what = what;        \
    } while (0)


#define FourBytes(hiByteAddr)   (                       \
    ( (((UInt32) (((UInt8 *) &(hiByteAddr)))[0]) << 24) \
    | (((UInt32) (((UInt8 *) &(hiByteAddr)))[1]) << 16) \
    | (((UInt32) (((UInt8 *) &(hiByteAddr)))[2]) <<  8) \
    | (((UInt32) (((UInt8 *) &(hiByteAddr)))[3]) <<  0) \
    ))
#define ThreeBytes(hiByteAddr)  (                       \
    ( (((UInt32) (((UInt8 *) &(hiByteAddr)))[0]) << 16) \
    | (((UInt32) (((UInt8 *) &(hiByteAddr)))[1]) <<  8) \
    | (((UInt32) (((UInt8 *) &(hiByteAddr)))[2]) <<  0) \
    ))
#define TwoBytes(hiByteAddr)    (                       \
    ( (((UInt32) (((UInt8 *) &(hiByteAddr)))[0]) <<  8) \
    | (((UInt32) (((UInt8 *) &(hiByteAddr)))[1]) <<  0) \
    ))

#define SameSCSIDevice(a, b) ((*((UInt32 *) &a)) == (*((UInt32 *) &b)))
int DoTestUnitReady(UInt8 targetID);

Boolean                     IsIllegalRequest(
	OSErr                   scsiStatus,
	const SCSI_Sense_Data   *senseDataPtr
    );
Boolean                     IsNoMedia(
	OSErr                   scsiStatus,
	const SCSI_Sense_Data   *senseDataPtr
    );

/*
 * All SCSI Commands come here.
 *  if scsiDevice.busID == kOriginalSCSIBusAdaptor, IM-IV SCSI will be called.
 *  scsiFlags should be scsiDirectionNone, scsiDirectionIn, or scsiDirectionOut
 *  actualTransferCount may be NULL if you don't care.
 *  Both old and new SCSI return SCSI Manager 4.3 errors.
 *
 * DoSCSICommand throws really serious errors, but returns SCSI errors such
 * as dataRunError and scsiDeviceNotThere.
 */
OSErr                       DoSCSICommand(
	DeviceIdent             scsiDevice,
	ConstStr255Param        currentAction,
	const SCSI_CommandPtr   callerSCSICommand,
	Ptr                     dataBuffer,
	ByteCount               dataLength,
	UInt32                  scsiFlags,
	ByteCount               *actualTransferCount,
	SCSI_Sense_Data         *sensePtr,
	StringPtr               senseMessage
    );

/*
 * Cheap 'n dirty memory clear routine.
 */
#define CLEAR(dst)          ClearMemory((void *) &dst, sizeof dst)
void                        ClearMemory(
	void                    *dataArea,
	ByteCount               dataSize
    );

/*
 * Global values
 */
EXTERN SCSIExecIOPB     *gSCSIExecIOPBPtr;
EXTERN UInt32           gSCSIExecIOPBPtrLen;

#endif  /* __SCSIExplorer__ */
