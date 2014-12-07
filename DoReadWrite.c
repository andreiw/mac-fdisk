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
#include "SCSIStuff.h"
int DoRead(UInt8 targetID, UInt32 block, UInt16 count, char* addr);
int DoWrite(UInt8 targetID, UInt32 block, UInt16 count, char* addr);

int
DoRead(
	UInt8               targetID,
	UInt32              block,
	UInt16              count,
	char *              addr
    )
{
    OSErr                   status;
    Str255                  errorText;
    char*       msg;
    static SCSI_10_Byte_Command gReadCommand = {
	kScsiCmdRead10, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    SCSI_Sense_Data         senseData;
    DeviceIdent scsiDevice;
    int rtn_value;

    scsiDevice.diReserved = 0;
    scsiDevice.bus = kOriginalSCSIBusAdaptor;
    scsiDevice.targetID = targetID;
    scsiDevice.LUN = 0;

    gReadCommand.lbn4 = (block >> 24) & 0xFF;
    gReadCommand.lbn3 = (block >> 16) & 0xFF;
    gReadCommand.lbn2 = (block >> 8) & 0xFF;
    gReadCommand.lbn1 = block & 0xFF;

    gReadCommand.len2 = (count >> 8) & 0xFF;
    gReadCommand.len1 = count & 0xFF;

    status = DoSCSICommand(
			    scsiDevice,
			    "\pRead",
			    (SCSI_CommandPtr) &gReadCommand,
			    (Ptr) addr,
			    count * 512,
			    scsiDirectionIn,
			    NULL,
			    &senseData,
			    errorText
	    );
    if (status == noErr) {
	rtn_value = 1;
    } else {
	rtn_value = 0;
    }
    return rtn_value;
}

int
DoWrite(
	UInt8               targetID,
	UInt32              block,
	UInt16              count,
	char *              addr
    )
{
    OSErr                   status;
    Str255                  errorText;
    char*       msg;
    static SCSI_10_Byte_Command gWriteCommand = {
	kScsiCmdWrite10, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    SCSI_Sense_Data         senseData;
    DeviceIdent scsiDevice;
    int rtn_value;

    scsiDevice.diReserved = 0;
    scsiDevice.bus = 0xff;
    scsiDevice.targetID = targetID;
    scsiDevice.LUN = 0;

    gWriteCommand.lbn4 = (block >> 24) & 0xFF;
    gWriteCommand.lbn3 = (block >> 16) & 0xFF;
    gWriteCommand.lbn2 = (block >> 8) & 0xFF;
    gWriteCommand.lbn1 = block & 0xFF;

    gWriteCommand.len2 = (count >> 8) & 0xFF;
    gWriteCommand.len1 = count & 0xFF;

    status = DoSCSICommand(
			    scsiDevice,
			    "\pWrite",
			    (SCSI_CommandPtr) &gWriteCommand,
			    (Ptr) addr,
			    count * 512,
			    scsiDirectionOut,
			    NULL,
			    &senseData,
			    errorText
	    );
    if (status == noErr) {
	rtn_value = 1;
    } else {
	rtn_value = 0;
    }
    return rtn_value;
}



