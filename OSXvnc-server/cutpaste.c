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

#include <Cocoa/Cocoa.h>

#include <stdio.h>
#include <pthread.h>
#include "rfb.h"

// Currently there is a problem when OSXvnc is run PRIOR to the pbs (which starts when a user logs in)
// the OSXvnc process is NOT connected to the pbs port - this is an OS X security measure which we aren't certain
// how to work around

// We might be able to register with the port later
// Restart VNC on login (of course this kills sessions)
// or spawn a little agent at login -- modify the /etc/ttys and add a -LoginHook process

// This is the global VNC change count
int pasteBoardLastChangeCount=-1;
NSStringEncoding pasteboardStringEncoding = 0;

// This notifies us that the VNCclient set some new pasteboard
void rfbSetCutText(rfbClientPtr cl, char *str, int len) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSString *clientCutText = [[NSString alloc] initWithCString:str length:len];

    // Don't need to send it back to same client (only others)
    cl->pasteBoardLastChange = [[NSPasteboard generalPasteboard] declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
    if (cl->pasteBoardLastChange)
        [[NSPasteboard generalPasteboard] setString:clientCutText forType:NSStringPboardType];
    else
        NSLog(@"Problem Writing Cut Text To Pasteboard");

    [pool release];
}

// We call this to see if we have a new pasteboard change and should notify clients to do an update
void rfbCheckForPasteboardChange() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    // REDSTONE
    // First Let's see if we have new info on the pasteboard - if so we'll send an update to each client
    if (pasteBoardLastChangeCount != [[NSPasteboard generalPasteboard] changeCount]) {
        rfbClientPtr cl;
        rfbClientIteratorPtr iterator = rfbGetClientIterator();

        // Record first in case another event comes in after notifying clients
        pasteBoardLastChangeCount = [[NSPasteboard generalPasteboard] changeCount];

        //NSLog(@"New PB notify clients to send %d", [[NSPasteboard generalPasteboard] changeCount]);

        // Notify each client
        while ((cl = rfbClientIteratorNext(iterator)) != NULL) {
            pthread_cond_signal(&cl->updateCond);
        }
        rfbReleaseClientIterator(iterator);
    }
    [pool release];
}

// Each client output thread will come here to get the PB and send it
void rfbClientUpdatePasteboard(rfbClientPtr cl) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    if (cl->pasteBoardLastChange == -1)
        cl->pasteBoardLastChange = [[NSPasteboard generalPasteboard] changeCount];

    if (cl->pasteBoardLastChange != [[NSPasteboard generalPasteboard] changeCount]) {
        const char *pbString = NULL;
        int length = 0;

        // First make sure it has NSStringPboardType type data
        if ([[NSPasteboard generalPasteboard] availableTypeFromArray:[NSArray arrayWithObject:NSStringPboardType]]) {
            if (pasteboardStringEncoding) {
                NSData *encodedString = [[[NSPasteboard generalPasteboard] stringForType:NSStringPboardType]
 dataUsingEncoding:pasteboardStringEncoding allowLossyConversion:YES];
                pbString = [encodedString bytes];
            }
            else
                pbString = [[[NSPasteboard generalPasteboard] stringForType:NSStringPboardType] lossyCString];

            if (pbString) {
                length = strlen(pbString);
                rfbSendServerCutText(cl, (char *) pbString, length);
            }
        }
            
        cl->pasteBoardLastChange = [[NSPasteboard generalPasteboard] changeCount];
    }
    [pool release]; 
}

