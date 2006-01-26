/*
 * cutpaste.c - routines to deal with cut & paste buffers / selection.
 */

/*
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc code Copyright (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#import <Cocoa/Cocoa.h>

#include <stdio.h>
#include <pthread.h>
#include "rfb.h"

// Currently there is a problem when OSXvnc is run PRIOR to the pbs (which starts when a user logs in)
// the OSXvnc process is NOT connected to the pbs port - this is an OS X security measure which we aren't certain
// how to work around

// We might be able to register with the port later
// Restart VNC on login (of course this kills sessions)
// or spawn a little agent at login -- modify the /etc/ttys and add a -LoginHook process
// or possibly run with -inetd...hmmm...

// This is the global VNC change count
NSLock *pasteboardLock = nil;
NSString *pasteboardString = nil;
NSString *clientCutText = nil;

int pasteBoardLastChangeCount=-1;
NSStringEncoding pasteboardStringEncoding = NSWindowsCP1252StringEncoding; // RFBProto 003.008

void initPasteboard() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	pasteboardLock = [[NSLock alloc] init];
	
	if (![NSPasteboard generalPasteboard]) {
		rfbLog("Pasteboard Inaccessible - Pasteboard sharing disabled");
		pasteBoardLastChangeCount = 0; // This will signal that we don't have pasteboard access
		pasteboardString = [[NSString alloc] initWithString:@"\e<PASTEBOARD INACCESSIBLE>\e"];
	}
    [pool release];
}

void initPasteboardForClient(rfbClientPtr cl) {
    /* REDSTONE - Have new client keep his PB currently */
	cl->pasteBoardLastChange = -1;
}

// This notifies us that the VNCclient set some new pasteboard
void rfbSetCutText(rfbClientPtr cl, char *str, int len) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
	if (pasteBoardLastChangeCount != 0) {
		[pasteboardLock lock];
		[clientCutText release];
		clientCutText = [[NSString alloc] initWithData:[NSData dataWithBytes:str length:len] encoding: pasteboardStringEncoding];
		cl->pasteBoardLastChange = -1; // Don't resend to original client
		[pasteboardLock unlock];
		// Since subsequent operations might require the pasteboard, we'll stall until it gets picked up
		while (clientCutText)
			usleep(10000);
	}
			
    [pool release];
}

// We call this to see if we have a new pasteboard change and should notify clients to do an update
void rfbCheckForPasteboardChange() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	[pasteboardLock lock];
	
	if (clientCutText) {
		if ([[NSPasteboard generalPasteboard] declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil]) {
			NS_DURING
				[[NSPasteboard generalPasteboard] setString:clientCutText forType:NSStringPboardType];
			NS_HANDLER
				NSLog(@"Problem Writing Cut Text To Pasteboard: %@", localException);
			NS_ENDHANDLER
		}
		else {
			NSLog(@"Problem Writing Cut Text To Pasteboard");
		}
		[clientCutText release];
		clientCutText = nil;
	}
	
	// First Let's see if we have new info on the pasteboard - if so we'll send an update to each client
	if (pasteBoardLastChangeCount != [[NSPasteboard generalPasteboard] changeCount]) {
		rfbClientPtr cl;
		rfbClientIteratorPtr iterator = rfbGetClientIterator();
		
		// Record first in case another event comes in after notifying clients
		pasteBoardLastChangeCount = [[NSPasteboard generalPasteboard] changeCount];
		[pasteboardString release];
		pasteboardString = nil;
		// Let's grab a copy of it here in the Main/Event Thread so that the output threads don't have to deal with the PB directly
		if ([[NSPasteboard generalPasteboard] availableTypeFromArray:[NSArray arrayWithObject:NSStringPboardType]]) {
			pasteboardString = [[[NSPasteboard generalPasteboard] stringForType:NSStringPboardType] copy];
		}
		
		//NSLog(@"New PB notify clients to send %d", [[NSPasteboard generalPasteboard] changeCount]);
		
		// Notify each client
		while ((cl = rfbClientIteratorNext(iterator)) != NULL) {
			pthread_cond_signal(&cl->updateCond);
		}
		rfbReleaseClientIterator(iterator);
	}
	
	[pasteboardLock unlock];
	[pool release];
}

// Each client output thread will come here to get the PB and send it
void rfbClientUpdatePasteboard(rfbClientPtr cl) {
	// They must have passed initialization and be in the Normal operational mode
	if (cl->state == RFB_NORMAL) {
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		[pasteboardLock lock];
		// New clients don't get the latest clipboard, they keep their own
		if (cl->pasteBoardLastChange == -1)
			cl->pasteBoardLastChange = pasteBoardLastChangeCount;
		
		if (cl->pasteBoardLastChange != pasteBoardLastChangeCount) {
			if (pasteboardString) {
				NSData *encodedString = [pasteboardString dataUsingEncoding:pasteboardStringEncoding allowLossyConversion:YES];

				if ([encodedString length])
					rfbSendServerCutText(cl, (char *) [encodedString bytes], [encodedString length]);
			}
			
			cl->pasteBoardLastChange = pasteBoardLastChangeCount;
		}
		[pasteboardLock unlock];
		
		[pool release];
	}
}
