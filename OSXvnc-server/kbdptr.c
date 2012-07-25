/*
 * kbdptr.c - deal with keyboard and pointer device over TCP & UDP.
 *
 *
 */

/*
 *  OSXvnc Copyright (C) 2001 Dan McGuirk
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
#import <Carbon/Carbon.h>

#import <stdio.h>
#import <ApplicationServices/ApplicationServices.h>

#import "VNCServer.h"

#import <X11/keysym.h>
#import "rfb.h"

#import "kbdptr.h"


void KbdAddEvent(Bool down, KeySym keySym, rfbClientPtr cl) {
    [[VNCServer sharedServer] handleKeyboard:(Bool) down forSym: (KeySym) keySym forClient: (rfbClientPtr) cl];
}

void keyboardReleaseKeysForClient(rfbClientPtr cl) {
    [[VNCServer sharedServer] releaseModifiersForClient: (rfbClientPtr) cl];
}

void PtrAddEvent(int buttonMask, int x, int y, rfbClientPtr cl) {
    [[VNCServer sharedServer] handleMouseButtons:buttonMask atPoint:NSMakePoint(x, y) forClient: (rfbClientPtr) cl];
}
