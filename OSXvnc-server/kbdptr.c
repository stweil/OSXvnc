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

#import <X11/keysym.h>
#import "rfb.h"

#import "kbdptr.h"
#import "RFBBundleProtocol.h"

CGKeyCode keyTable[keyTableSize];
unsigned char keyTableMods[keyTableSize]; // 8 Bits for Modifier Keys

// This flag will try to change the modifier key state to the required set for the unicode key that came in

// It will be turned on by JaguarExtensions unless specified
BOOL pressModsForKeys = FALSE;

void *alternateKeyboardHandler = nil;

static int mouseWheelDistance;

void loadKeyTable() {
    unsigned int i;

    // Initialize them all to 0xFFFF
    for (i = 0; i < keyTableSize; i++) {
        keyTable[i] = 0xFFFF;
        keyTableMods[i] = 0;
    }

    // This is the old US only keyboard mapping
    // Map the above key table into a static array so we can just look them up directly
    // NSLog(@"Unable To Determine Key Map - Reverting to US Mapping\n");
    for (i = 0; i < (sizeof(USKeyCodes) / sizeof(int)); i += 2)
        keyTable[(unsigned short)USKeyCodes[i]] = (CGKeyCode) USKeyCodes[i+1];

    // This is the old SpecialKeyCodes keyboard mapping
    // Map the above key table into a static array so we can just look them up directly
    // NSLog(@"Loading %d XKeysym Special Keys\n", (sizeof(SpecialKeyCodes) / sizeof(int)));
    for (i = 0; i < (sizeof(SpecialKeyCodes) / sizeof(int)); i += 2)
        keyTable[(unsigned short)SpecialKeyCodes[i]] = (CGKeyCode) SpecialKeyCodes[i+1];
}

void KbdAddEvent(Bool down, KeySym keySym, rfbClientPtr cl) {
	rfbUndim();	
	
	if (alternateKeyboardHandler != nil)
		[(id)alternateKeyboardHandler handleKeyboard:(Bool) down forSym: (KeySym) keySym forClient: (rfbClientPtr) cl];
	else {
		CGKeyCode keyCode = keyTable[(unsigned short)keySym];
		CGCharCode keyChar = 0;
		UInt32 modsForKey = keyTableMods[keySym];
		bool doMods = (modsForKey != 0xFF && down && pressModsForKeys);

		if (keySym < 0xFF) // If it's an ASCII key we'll send the keyChar
			keyChar = (CGCharCode) keySym;

		//rfbLog("Key Char:%c Key Code:%d Key Sym:%d\n", keyChar, keyCode, keySym);
		if (keyCode == 0xFFFF)
			rfbLog("Warning: Unable to determine Key Code for X Key Sym %d (0x%x)\n", (int)keySym, (int)keySym);
		else {
			if (doMods) {
				modsForKey = modsForKey << 8;
				
				// Toggle the state of the appropriate keys
				if (!(cl->modiferKeys[keyTable[XK_Meta_L]]) != !(modsForKey & optionKey))
					CGEventCreateKeyboardEvent(NULL, keyTable[XK_Meta_L], (modsForKey & optionKey));
				
				if (!(cl->modiferKeys[keyTable[XK_Control_L]]) != !(modsForKey & controlKey))
					CGEventCreateKeyboardEvent(NULL, keyTable[XK_Control_L], (modsForKey & controlKey));
            
				if (!(cl->modiferKeys[keyTable[XK_Shift_L]]) != !(modsForKey & shiftKey))
					CGEventCreateKeyboardEvent(NULL, keyTable[XK_Shift_L], (modsForKey & shiftKey));
			}
        
			CGEventCreateKeyboardEvent(NULL, keyCode, down);
			
			if (doMods) {
				// Return keys to previous state
				if (!(cl->modiferKeys[keyTable[XK_Meta_L]]) != !(modsForKey & optionKey))
					CGEventCreateKeyboardEvent(NULL, keyTable[XK_Meta_L], cl->modiferKeys[keyTable[XK_Meta_L]]);

				if (!(cl->modiferKeys[keyTable[XK_Control_L]]) != !(modsForKey & controlKey))
					CGEventCreateKeyboardEvent(NULL, keyTable[XK_Control_L], cl->modiferKeys[keyTable[XK_Control_L]]);
				
				if (!(cl->modiferKeys[keyTable[XK_Shift_L]]) != !(modsForKey & shiftKey))
					CGEventCreateKeyboardEvent(NULL, keyTable[XK_Shift_L], cl->modiferKeys[keyTable[XK_Shift_L]]);
			}
        
			if (keyCode >= keyTable[XK_Alt_L] && keyCode <= keyTable[XK_Control_L]) {
				cl->modiferKeys[keyCode] = down; // Mark the key state for that client, we'll release down keys later
			}
		}
	}
}

void keyboardReleaseKeysForClient(rfbClientPtr cl) {
    int i = keyTable[XK_Alt_L];

    for (i = keyTable[XK_Alt_L]; i <= keyTable[XK_Control_L]; i++) {
        if (cl->modiferKeys[i]) {
            CGEventCreateKeyboardEvent(NULL, i, 0); // Release all modifier keys that were held down
                                                    //rfbLog("Released Key: %d", index);
        }
    }
}

void PtrAddEvent(int buttonMask, int x, int y, rfbClientPtr cl) {
    rfbUndim();

    if (buttonMask & rfbWheelMask) {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        NSUserDefaults *currentUserDefs = [[NSUserDefaults alloc] initWithUser:NSUserName()];

        // I would rather cache this data than look it up each time but I don't know how to get notification of a change
        // A - User changes his setting in SysPrefs
        // B - Running OSXvnc as root and user swiches

        mouseWheelDistance = 8 * [currentUserDefs floatForKey:@"com.apple.scrollwheel.scaling"];
        if (!mouseWheelDistance)
            mouseWheelDistance = 10;

        if (buttonMask & rfbWheelUpMask)
            CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitPixel,  1,  mouseWheelDistance);

        if (buttonMask & rfbWheelDownMask)
            CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitPixel,  1, -mouseWheelDistance);

        [pool release];
    }
    else {
        cl->clientCursorLocation.x = x;
        cl->clientCursorLocation.y = y;

        // Tricky here -- new events need to specify up, down and dragged, not just button state.
        //CGEventCreateMouseEvent(NULL, NX_OMOUSEDRAGGED, CGPointMake(x,y), kCGMouseButtonCenter)

        if (cl->swapMouseButtons23)
            CGPostMouseEvent(cl->clientCursorLocation, TRUE, 3,
                             (buttonMask & rfbButton1Mask) ? TRUE : FALSE,
                             (buttonMask & rfbButton3Mask) ? TRUE : FALSE,
                             (buttonMask & rfbButton2Mask) ? TRUE : FALSE);
        else
            CGPostMouseEvent(cl->clientCursorLocation, TRUE, 3,
                             (buttonMask & rfbButton1Mask) ? TRUE : FALSE,
                             (buttonMask & rfbButton2Mask) ? TRUE : FALSE,
                             (buttonMask & rfbButton3Mask) ? TRUE : FALSE);
    }
}
