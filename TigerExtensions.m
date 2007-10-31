//
//  TigerExtensions.m
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Fri Jul 11 2003.
//  Copyright (c) 2003 RedstoneSoftware, Inc. All rights reserved.


#import "TigerExtensions.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#include "keysymdef.h"
#include "kbdptr.h"

#include "rfb.h"

#include "rfbserver.h"
#import "VNCServer.h"

#import "../RFBBundleProtocol.h"

static CGEventSourceRef vncSourceRef;
static CGEventTapLocation vncTapLocation;
static KeyboardLayoutRef unicodeLayout = NULL; //kKeyboardISO

#define unicodeHexInputKeyboardIdnetifier -1


unsigned char unicodeNumbersToKeyCodes[16] = { 29, 18, 19, 20, 21, 23, 22, 26, 28, 25, 0, 11, 8, 2, 14, 3 };

@implementation TigerExtensions

rfbserver *theServer;

static inline void sendKeyEvent(CGCharCode keyChar, CGKeyCode keyCode, Bool down, CGEventFlags modifierFlags) {
	if (vncTapLocation && vncSourceRef) {
		CGEventRef event = CGEventCreateKeyboardEvent(vncSourceRef, keyCode, down);
		// The value of this function escapes me (since you still need to specify the keyCode for it to work
		//CGEventKeyboardSetUnicodeString (event, 1, (const UniChar *) &keySym);
		CGEventSetFlags(event, modifierFlags);
		CGEventPost(vncTapLocation, event);
		CFRelease(event);
	}
	else {
		CGPostKeyboardEvent(keyChar, keyCode, down);
	}
}


void SyncSetKeyboardLayout (unsigned long keyboardScript, KeyboardLayoutRef newLayout) {
	KeyboardLayoutRef aKeyboardLayout;
	int identifier=0, targetIdentifier=0;

	if (GetScriptManagerVariable(smKeyScript) != keyboardScript) {
		KeyScript( keyboardScript | smKeyForceKeyScriptMask );
	}
	
	// Get the target identifier
	KLGetKeyboardLayoutProperty(newLayout, kKLIdentifier, (const void **)&targetIdentifier);

	// Async -- wait for it to change
	do {
		// Try setting it
		KLSetCurrentKeyboardLayout(newLayout);
		
		KLGetCurrentKeyboardLayout(&aKeyboardLayout);
		KLGetKeyboardLayoutProperty(aKeyboardLayout, kKLIdentifier, (const void **)&identifier);
	} while (identifier != targetIdentifier);
}

+ (void) rfbStartup: (rfbserver *) aServer {
    int argumentIndex;

    theServer = aServer;
	*(id *)(theServer->alternateKeyboardHandler) = self;
	
	NSLog(@"Keyboard Type %d", LMGetKbdType());
	
	switch ([[NSUserDefaults standardUserDefaults] integerForKey:@"EventTap"]) {
		case 3:
			NSLog(@"No Event Tap -- Using 10.3 API");
			vncTapLocation = nil;
			break;
		case 2:
			NSLog(@"Using HID Event Tap");
			vncTapLocation = kCGHIDEventTap;
			break;
		case 1:
			NSLog(@"Using Annotated Session Event Tap");
			vncTapLocation = kCGAnnotatedSessionEventTap;
			break;
		case 0:
		default:
			vncTapLocation = kCGSessionEventTap;
			break;
	}
	
	switch ([[NSUserDefaults standardUserDefaults] integerForKey:@"EventSource"]) {
		case 3:
			NSLog(@"No Event Source -- Using 10.3 API");
			vncTapLocation = nil;
			break;
		case 2:
			NSLog(@"Using HID Event Source");
			vncSourceRef = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
			break;
		case 1:
			NSLog(@"Using Combined Event Source");
			vncSourceRef = CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
			break;
		case 0:
		default:
			vncSourceRef = CGEventSourceCreate(kCGEventSourceStatePrivate);
			break;
	}
}

+ (void) rfbUsage {
    fprintf(stderr,
            "\nTiger BUNDLE OPTIONS (10.4+):\n"
			);
}

+ (void) rfbRunning {
	return;
}

+ (void) rfbPoll {
	//NSLog(@"Current Script: %d", GetScriptManagerVariable(smKeyScript));
    return;
}

+ (void) rfbReceivedClientMessage {
    return;
}

+ (void) rfbShutdown {
    NSLog(@"Unloading Tiger Extensions");
}

// Keyboard handling code
+ handleKeyboard:(Bool) down forSym: (KeySym) keySym forClient: (rfbClientPtr) cl {
	CGKeyCode keyCode = theServer->keyTable[(unsigned short)keySym];
	CGCharCode keyChar = 0;
	UInt32 modsForKey = theServer->keyTableMods[keySym] << 8;

	if (down) {
		//NSLog(@"%d, %d", keyCode, keySym);
		//NSLog(@"%C", keySym);
	}
	
	// If we can't locate the keycode then we will use the special OPTION+4 HEX coding that is available on the Unicode HexInput Keyboard
	if (keyCode == 0xFFFF && down) {		
		CGKeyCode keyCodeMeta = 58; // KeyCode for the Option key with the Unicode Hex input keyboard
		unsigned short mask=0xF000;
		int rightShift;

		unsigned long currentKeyboardScript;
		KeyboardLayoutRef currentKeyboardLayout;
		KLGetCurrentKeyboardLayout(&currentKeyboardLayout);
		
		if (unicodeLayout == NULL) {
			// Have to change it once on startup to get the unicodeLayout
			unsigned long startingKeyScript = (unsigned long) GetScriptManagerVariable(smKeyScript);
			KeyScript( smUnicodeScript | smKeyForceKeyScriptMask );
			OSStatus result = KLGetKeyboardLayoutWithIdentifier(unicodeHexInputKeyboardIdnetifier, &unicodeLayout);
			NSLog(@"Loading Unicode Keyboard Layout: %d", result);
			KeyScript( startingKeyScript | smKeyForceKeyScriptMask );
		}
		
		// Switch to Unicode Keyboard
		SyncSetKeyboardLayout(smUnicodeScript,unicodeLayout);
		
		// Hold Down Option
		//sendKeyEvent(keyChar, keyCodeMeta, 1, kCGEventFlagMaskNonCoalesced);
		for (rightShift = 12; rightShift >= 0; rightShift-=4) {
			short unidigit = (keySym & mask) >> rightShift;
			
			//CGEventSetIntegerValueField( event , kCGKeyboardEventKeyboardType, unicodeHexInputKeyboardIdnetifier);
			sendKeyEvent(keyChar, unicodeNumbersToKeyCodes[unidigit], 1, kCGEventFlagMaskAlternate | kCGEventFlagMaskNonCoalesced);
			sendKeyEvent(keyChar, unicodeNumbersToKeyCodes[unidigit], 0, kCGEventFlagMaskAlternate | kCGEventFlagMaskNonCoalesced);
						
			mask >>= 4;
		}
		//sendKeyEvent(keyChar, keyCodeMeta, 0, kCGEventFlagMaskNonCoalesced);

		// Switch to Old Keyboard
		SyncSetKeyboardLayout(currentKeyboardScript, currentKeyboardLayout);
	}
	else {
		if (down && *(theServer->pressModsForKeys)) {
			CGKeyCode keyCodeShift = theServer->keyTable[XK_Shift_L];
			CGKeyCode keyCodeMeta = theServer->keyTable[XK_Meta_L];
			CGKeyCode keyCodeControl = theServer->keyTable[XK_Control_L];
		
//            // Toggle the state of the appropriate keys
//            if (!(cl->modiferKeys[keyCodeShift]) != !(modsForKey & shiftKey)) {
//				sendKeyEvent(keyChar, keyCodeShift, (modsForKey & shiftKey));
//			}
//            if (!(cl->modiferKeys[keyCodeMeta]) != !(modsForKey & optionKey)) {
//				sendKeyEvent(keyChar, keyCodeMeta, (modsForKey & optionKey));
//            }
//            if (!(cl->modiferKeys[keyCodeControl]) != !(modsForKey & controlKey)) {
//				sendKeyEvent(keyChar, keyCodeControl, (modsForKey & controlKey));
//            }
//
//			sendKeyEvent(keyChar, keyCode, down);
//			
//            // Return keys to previous state
//            if (!(cl->modiferKeys[keyCodeShift]) != !(modsForKey & optionKey)) {
//				sendKeyEvent(keyChar, keyCodeShift, cl->modiferKeys[keyCodeShift]);
//            }			
//            if (!(cl->modiferKeys[keyCodeMeta]) != !(modsForKey & optionKey)) {
//				sendKeyEvent(keyChar, keyCodeMeta, cl->modiferKeys[keyCodeMeta]);
//            }
//            if (!(cl->modiferKeys[keyCodeControl]) != !(modsForKey & controlKey)) {
//				sendKeyEvent(keyChar, keyCodeControl, cl->modiferKeys[keyCodeControl]);
//            }			
		}
        else {
			sendKeyEvent(keyChar, keyCode, down, 
						 (cl->modiferKeys[theServer->keyTable[XK_Shift_L]] ? kCGEventFlagMaskShift : 0) |
						 (cl->modiferKeys[theServer->keyTable[XK_Control_L]] ? kCGEventFlagMaskControl : 0) |
						 (cl->modiferKeys[theServer->keyTable[XK_Meta_L]] ? kCGEventFlagMaskAlternate : 0) |
						 (cl->modiferKeys[theServer->keyTable[XK_Alt_L]] ? kCGEventFlagMaskCommand : 0) |
						 kCGEventFlagMaskNonCoalesced);
		}
        
		 // Mark the key state for that client, we'll release down keys later
        if (keyCode >= theServer->keyTable[XK_Alt_L] && keyCode <= theServer->keyTable[XK_Control_L]) {
            cl->modiferKeys[keyCode] = down;
        }
	}
}

@end

