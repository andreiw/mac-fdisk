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
#include <stdio.h>
#include "SCSIStuff.h"

int
DoTestUnitReady(
	UInt8               targetID
    )
{
    OSErr                   status;
    Str255                  errorText;
    char*       msg;
    static const SCSI_6_Byte_Command gTestUnitReadyCommand = {
	kScsiCmdTestUnitReady, 0, 0, 0, 0, 0
    };
    SCSI_Sense_Data         senseData;
    DeviceIdent scsiDevice;
    int rtn_value;

    scsiDevice.diReserved = 0;
    scsiDevice.bus = kOriginalSCSIBusAdaptor;
    scsiDevice.targetID = targetID;
    scsiDevice.LUN = 0;

    status = DoSCSICommand(
		scsiDevice,
		"\pTest Unit Ready",
		(SCSI_CommandPtr) &gTestUnitReadyCommand,
		NULL,
		0,
		scsiDirectionNone,
		NULL,
		&senseData,
		errorText
	    );
    if (status == scsiNonZeroStatus) {
	msg = "Unknown problem";
	switch (senseData.senseKey & kScsiSenseKeyMask) {
	case kScsiSenseIllegalReq:
	    msg = "Logical Unit Not Supported";
	    break;
	case kScsiSenseNotReady:
	    switch ((senseData.additionalSenseCode << 8)
		    | senseData.additionalSenseQualifier) {
	    case 0x0500:
		msg = "Logical Unit does not respond to selection";
		break;
	    case 0x0401:
		msg = "Logical Unit is becoming ready";
		break;
	    case 0x0400:
		msg = "Logical Unit is not ready. No specific cause.";
		break;
	    case 0x0402:
		msg = "Logical Unit is not ready. Unit needs start command.";
		break;
	    case 0x0403:
		msg = "Logical Unit is not ready. Unit needs manual intervention.";
		break;
	    case 0x0404:
		msg = "Logical Unit is not ready. Format in progress";
		break;
	    case 0x2500:
		msg = "Logical Unit is not supported";
		break;
	    }
	}
	rtn_value = -1;
    } else if (status != noErr) {
	msg = "Test Unit Ready failed";
	rtn_value = 0;
    } else {
	msg = "Okay - device is ready";
	rtn_value = 1;
    }
    //printf("%s\n", msg);
    return rtn_value;
}



