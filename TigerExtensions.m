//
//  TigerExtensions.m
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Fri Jul 11 2003.
//  Copyright (c) 2003 RedstoneSoftware, Inc. All rights reserved.

#import "JaguarExtensions.h"
#import "TigerExtensions.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#import <Security/AuthSession.h>

#include "keysymdef.h"
#include "kbdptr.h"

#include "rfb.h"

#include "rfbserver.h"
#import "VNCServer.h"

#import "../RFBBundleProtocol.h"

static BOOL readyToStartup = TRUE;
static BOOL dynamicKeyboard = FALSE;
static CGEventSourceRef vncSourceRef=NULL;
static CGEventTapLocation vncTapLocation=0;

static TISInputSourceRef unicodeInputSource=NULL;
static TISInputSourceRef currentInputSource=NULL;

// Current Modifiers reflect the "global" state of the modifier keys (not a particular VNC connection)
// This only matters for event taps.  Ideally we could detect their values but
// that seems to require an active event loop.
static CGEventFlags currentModifiers;

// These correspond to the keycodes of the keys 0-9,A-F
static unsigned char unicodeNumbersToKeyCodes[16] = { 29, 18, 19, 20, 21, 23, 22, 26, 28, 25, 0, 11, 8, 2, 14, 3 };

#ifndef NSAppKitVersionNumber10_3
#define NSAppKitVersionNumber10_3 743
#endif
#ifndef NSAppKitVersionNumber10_4
#define NSAppKitVersionNumber10_4 824
#endif

@implementation TigerExtensions

void SyncSetKeyboardLayout (TISInputSourceRef inputSource);

bool isConsoleSession();

// The Keycodes to various modifiers on the current keyboard
CGKeyCode keyCodeShift;
CGKeyCode keyCodeOption;
CGKeyCode keyCodeControl;
CGKeyCode keyCodeCommand;

int modifierDelay = 0;

rfbserver *theServer;

// This routine waits for the window server to register its per-session 
// services in our session.  This code was necessary in various pre-release 
// versions of Mac OS X 10.5, but it is not necessary on the final version. 
static void WaitForWindowServerSession(void) {
    CFDictionaryRef dict;
    int delay = 100000, maxDelay = 5000000;
	
	dict = CGSessionCopyCurrentDictionary();
	while (dict == NULL && maxDelay > 0) {
		usleep(delay);
		maxDelay -= delay;
        dict = CGSessionCopyCurrentDictionary();
	}
	if (maxDelay <= 0)
		NSLog(@"No CG session Available, max delay reached");
	if (dict != NULL)
		CFRelease(dict);
}

bool isConsoleSession() {
	BOOL returnValue = FALSE;
	CFDictionaryRef sessionInfoDict = CGSessionCopyCurrentDictionary();
		
	if (sessionInfoDict == NULL) 
		NSLog(@"Unable to get session dictionary.");
	else {
		CFBooleanRef userIsActive = CFDictionaryGetValue(sessionInfoDict, kCGSessionOnConsoleKey);
		returnValue = CFBooleanGetValue(userIsActive);
		CFRelease(sessionInfoDict);
	}
	
//	if (0) { 
//		// This one succeeds in "off-screen acounts" also
//		SecuritySessionId mySession;
//		SessionAttributeBits sessionInfo;
//		OSStatus error = SessionGetInfo(callerSecuritySession, &mySession, &sessionInfo);
//		
//		returnValue = (sessionInfo & sessionHasGraphicAccess);
//	}
//	else if (0) {
//		// There must be a better way but for now this seems to indicate if we are a console session or not
//		// at least for logged in users -- it always returns NO for the login window
//		CGEventSourceRef testRef = CGEventSourceCreate(kCGEventSourceStatePrivate);
//		int pollDelay = 0; // No poll at this time, just look once
//		
//		while (!testRef && pollDelay) {
//			usleep(100000);
//			pollDelay -= 100000;
//			testRef = CGEventSourceCreate(kCGEventSourceStatePrivate);
//		}
//	
//		if (testRef != NULL) {
//			returnValue = TRUE;
//			CFRelease(testRef);
//		}
//	}
	
	return returnValue;
}

+ (void) load {
	if (NSAppKitVersionNumber < NSAppKitVersionNumber10_4) {
		[NSException raise:@"Tiger (10.4) Required" format:@"Unable to load Tiger Bundle"];
	}
}

+ (void) rfbStartup: (rfbserver *) aServer {
	[[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
		@"NO", @"UnicodeKeyboard", // Load The Unicode Keyboard
		@"NO", @"DynamicKeyboard", // Try to set the keyboard "as we need it", doesn't work well on Tiger
		@"-1", @"UnicodeKeyboardIdentifier", // ID of the Unicode Keyboard resource to use (-1 is Apple's)

		@"2", @"EventSource", // Always private event source so we don't consolidate with existing keys (however HID for the EventTap always does anyhow)
		@"3", @"EventTap", // Default Event Tap (3=HID for Console User and Session For OffScreen Users)
		@"5000", @"ModifierDelay", // Delay when shifting modifier keys
		@"NO", @"SystemServer", 
		nil]];

    theServer = aServer;
	*(id *)(theServer->alternateKeyboardHandler) = [[self alloc] init];

	// 10.5 System Server special behavior
	if (floor(NSAppKitVersionNumber) > floor(NSAppKitVersionNumber10_4) &&
		[[NSUserDefaults standardUserDefaults] boolForKey:@"SystemServer"]) {
		// On 10.5 we need quit if the user switches out (so we can relinquish the port)
		if (isConsoleSession()) {
			[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
																   selector:@selector(systemServerShouldQuit:)
																	   name: NSWorkspaceSessionDidResignActiveNotification
																	 object:nil];			
		}
		// On 10.5 we need to be able to "hold" if we aren't the console session
		else {
			[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
																   selector:@selector(systemServerShouldContinue:)
																	   name: NSWorkspaceSessionDidBecomeActiveNotification
																	 object:nil];
			readyToStartup = NO;
			// Run Loop
			NSLog(@"System Server for non-console session, pausing until we receive console access");
			while (!readyToStartup) {
				OSStatus resultCode = RunCurrentEventLoop(kEventDurationSecond); //EventTimeout
				if (resultCode != eventLoopTimedOutErr) {
					NSLog(@"Received Result: %d during event loop, Shutting Down", resultCode);
					//rfbShutdown();
					exit(0);
				}
			}
		}
	}

	modifierDelay = [[NSUserDefaults standardUserDefaults] integerForKey:@"ModifierDelay"];

	[self loadUnicodeKeyboard];
}

+ (void) systemServerShouldQuit: (NSNotification *) aNotification {
    NSLog(@"User Switched Out, Stopping System Server - %@", [aNotification name]);
	//rfbShutdown();
	exit(0);
	return;
}
+ (void) systemServerShouldContinue: (NSNotification *) aNotification {
    NSLog(@"User Switched In, Starting System Server - %@", [aNotification name]);
	readyToStartup = YES;
	return;
}

+ (void) rfbRunning {	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"DynamicKeyboard"])
		dynamicKeyboard = TRUE;
	
	// Event Source represents which existing event states should be combined with the incoming events
	switch ([[NSUserDefaults standardUserDefaults] integerForKey:@"EventSource"]) {
		case 2:
			// Doesn't combine with any other sources
			NSLog(@"Using Private Event Source");
			vncSourceRef = CGEventSourceCreate(kCGEventSourceStatePrivate);
			break;
		case 1:
			// Combines with only with other User Session
			NSLog(@"Using Combined Event Source, WARNING: Doesn't work if we FUS off-screen (10.5.1)");
			vncSourceRef = CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
			break;
		case 0:
			// Combines with Physical Keyboard
			NSLog(@"Using HID Event Source, WARNING: Doesn't allow keys if we FUS off-screen (10.5.7)");
			vncSourceRef = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
			break;
		case 3: // I am pretty sure works similar to the HID
		default:
			vncSourceRef = NULL;
			break;
	}
	if (!vncSourceRef)
		NSLog(@"No Event Source -- Using 10.3 API");	
	
	// Event Taps represent at what level of the input manager the events will be interpretted
	switch ([[NSUserDefaults standardUserDefaults] integerForKey:@"EventTap"]) {
		case 3: {
			if (isConsoleSession()) {
				NSLog(@"Using Smart Event Tap -- HID for console user");
				vncTapLocation = kCGHIDEventTap;
			}
			else {
				NSLog(@"Using Smart Event Tap -- Session for off-screen user");
				vncTapLocation = kCGSessionEventTap;
			}
			
			[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
																   selector:@selector(userSwitchedIn:)
																	   name: NSWorkspaceSessionDidBecomeActiveNotification
																	 object:nil];
			[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
																   selector:@selector(userSwitchedOut:)
																	   name: NSWorkspaceSessionDidResignActiveNotification
																	 object:nil];			
			break;
		}
		case 2:
			NSLog(@"Using Annotated Session Event Tap");
			vncTapLocation = kCGAnnotatedSessionEventTap;
			break;
		case 1:
			// At this level will can passed in modifiers
			// it will ignore physical keyboard state
			// it will NOT impact physical keyboard state 			
			NSLog(@"Using Session Event Tap");
			vncTapLocation = kCGSessionEventTap;
			break;
		case 0:
		default:
			// At this level will ignore passed in modifiers
			// it will combine with the physical keyboard state (CapsLock, etc)
			// it WILL impact physical keyboard state 
			NSLog(@"Using HID Event Tap");
			vncTapLocation = kCGHIDEventTap;
			break;
	}
}

+ (void) loadUnicodeKeyboard {
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"UnicodeKeyboard"] && unicodeInputSource == NULL) {
		// Need to figure out a way to lookup the Unicode Keyboard before this will work
		
		// OSStatus result = KLGetKeyboardLayoutWithIdentifier([[NSUserDefaults standardUserDefaults] integerForKey:@"UnicodeKeyboardIdentifier"], &unicodeLayout);

		// Unicode Keyboard Should load keys from definition
		(*(theServer->pressModsForKeys) = YES);
		[self loadKeyboard:unicodeInputSource forServer:theServer];
	}	
}


+ (void) userSwitchedIn: (NSNotification *) aNotification {
    NSLog(@"User Switched In, Using HID Tap - %@", [aNotification name]);
	vncTapLocation = kCGHIDEventTap;
	return;
}
+ (void) userSwitchedOut: (NSNotification *) aNotification {
    NSLog(@"User Switched Out, Using Session Tap - %@", [aNotification name]);
	vncTapLocation = kCGSessionEventTap;	
	return;
}

+ (void) rfbConnect {
	if (unicodeInputSource != NULL && !dynamicKeyboard) {
		currentInputSource = TISCopyCurrentKeyboardInputSource();
		// Switch to Unicode Keyboard
		SyncSetKeyboardLayout(unicodeInputSource);
	}
}

+ (void) rfbDisconnect {
	if (unicodeInputSource != NULL && !dynamicKeyboard) {
		// Switch to Old Keyboard
		SyncSetKeyboardLayout(currentInputSource);
	}
}

+ (void) rfbUsage {
    fprintf(stderr,
            "\nTiger BUNDLE OPTIONS (10.4+):\n"
			);
}

+ (void) rfbPoll {
	if (0 && vncTapLocation == kCGHIDEventTap) {
		CGEventFlags newModifiers = 0;
		
		if (CGEventSourceKeyState(kCGEventSourceStateHIDSystemState,keyCodeShift))
			newModifiers |= kCGEventFlagMaskShift;
		if (CGEventSourceKeyState(kCGEventSourceStateHIDSystemState,keyCodeOption))
			newModifiers |= kCGEventFlagMaskAlternate;
		if (CGEventSourceKeyState(kCGEventSourceStateHIDSystemState,keyCodeControl))
			newModifiers |= kCGEventFlagMaskControl;
		if (CGEventSourceKeyState(kCGEventSourceStateHIDSystemState,keyCodeCommand))
			newModifiers |= kCGEventFlagMaskControl;
		
		currentModifiers = newModifiers;
	}
	
    return;
}

+ (void) rfbReceivedClientMessage {
    return;
}

+ (void) rfbShutdown {
    NSLog(@"Unloading Tiger Extensions");
}

// Keyboard handling code
- (void) handleKeyboard:(Bool) down forSym: (KeySym) keySym forClient: (rfbClientPtr) cl {
	CGKeyCode keyCode = theServer->keyTable[(unsigned short)keySym];
	CGEventFlags modifiersToSend = 0;

	if (1) {
		// Find the right keycodes base on the loaded keyboard
		keyCodeShift = theServer->keyTable[XK_Shift_L];
		keyCodeOption = theServer->keyTable[XK_Meta_L];
		keyCodeControl = theServer->keyTable[XK_Control_L];
		keyCodeCommand = theServer->keyTable[XK_Alt_L];
	}		

	// If we can't locate the keycode then we will use the special OPTION+4 HEX coding that is available on the Unicode HexInput Keyboard
	if (keyCode == 0xFFFF) {
		if (down && unicodeInputSource != NULL) {
			CGKeyCode keyCodeMeta = 58; // KeyCode for the Option key with the Unicode Hex input keyboard
			unsigned short mask=0xF000;
			int rightShift;
			CGEventFlags oldModifiers = currentModifiers;
			
			// Switch to Unicode Keyboard
			if (dynamicKeyboard) {
				currentInputSource = TISCopyCurrentKeyboardInputSource();
				SyncSetKeyboardLayout(unicodeInputSource);
			}
			
			modifiersToSend = kCGEventFlagMaskAlternate | kCGEventFlagMaskNonCoalesced;
			
			[self setKeyModifiers: modifiersToSend];
			for (rightShift = 12; rightShift >= 0; rightShift-=4) {
				short unidigit = (keySym & mask) >> rightShift;
				
				[self sendKeyEvent:unicodeNumbersToKeyCodes[unidigit] down:1 modifiers:modifiersToSend];
				[self sendKeyEvent:unicodeNumbersToKeyCodes[unidigit] down:0 modifiers:modifiersToSend];
				
				mask >>= 4;
			}
			[self setKeyModifiers: oldModifiers];
			
			// Switch to Old Keyboard
			if (dynamicKeyboard)
				SyncSetKeyboardLayout(currentInputSource);
		}
	}
	else {
		BOOL isModifierKey = (XK_Shift_L <= keySym && keySym <= XK_Hyper_R);
		
		if (isModifierKey) {
			// Mark the key state for the client, we'll release down keys later
			cl->modiferKeys[keyCode] = down;
			
			// Record them in our "currentModifiers"
			switch (keySym) {
				case XK_Shift_L:
				case XK_Shift_R:
					if (down)
						currentModifiers |= kCGEventFlagMaskShift;
					else
						currentModifiers &= ~kCGEventFlagMaskShift;
					break;	
				case XK_Control_L:
				case XK_Control_R:
					if (down)
						currentModifiers |= kCGEventFlagMaskControl;
					else
						currentModifiers &= ~kCGEventFlagMaskControl;
					break;	
				case XK_Meta_L:
				case XK_Meta_R:
					if (down)
						currentModifiers |= kCGEventFlagMaskAlternate;
					else
						currentModifiers &= ~kCGEventFlagMaskAlternate;
					break;	
				case XK_Alt_L:
				case XK_Alt_R:
					if (down)
						currentModifiers |= kCGEventFlagMaskCommand;
					else
						currentModifiers &= ~kCGEventFlagMaskCommand;
					break;
			}					
			
			[self sendKeyEvent:keyCode down:down modifiers:currentModifiers];
		}
		else {
			if (*(theServer->pressModsForKeys)) {
				if (theServer->keyTableMods[keySym] != 0xFF) {					
					// Setup the state of the appropriate keys based on the value in the KeyTableMods
					CGEventFlags oldModifiers = currentModifiers;
					CGEventFlags modifiersToSend = kCGEventFlagMaskNonCoalesced;
					
					if ((theServer->keyTableMods[keySym] << 8) & shiftKey)
						modifiersToSend |= kCGEventFlagMaskShift;
					if ((theServer->keyTableMods[keySym] << 8) & optionKey)
						modifiersToSend |= kCGEventFlagMaskAlternate;
					if ((theServer->keyTableMods[keySym] << 8) & controlKey)
						modifiersToSend |= kCGEventFlagMaskControl;
					
					// Treat command key separately (not as part of the generation string)
					modifiersToSend |= (currentModifiers & kCGEventFlagMaskCommand);
					
					[self setKeyModifiers: modifiersToSend];
					[self sendKeyEvent:keyCode down:down modifiers:modifiersToSend];
					// Back to current depressed state
					[self setKeyModifiers: oldModifiers];
				}
				else {
					// Not Modified (special keys, other modifiers)
					[self sendKeyEvent:keyCode down:down modifiers:currentModifiers];
				}
			}
			else {
				CGEventFlags oldModifiers = currentModifiers;
				CGEventFlags modifiersToSend = kCGEventFlagMaskNonCoalesced | 
					(cl->modiferKeys[keyCodeShift] ? kCGEventFlagMaskShift : 0) |
					(cl->modiferKeys[keyCodeControl] ? kCGEventFlagMaskControl : 0) |
					(cl->modiferKeys[keyCodeOption] ? kCGEventFlagMaskAlternate : 0) |
					(cl->modiferKeys[keyCodeCommand] ? kCGEventFlagMaskCommand : 0);
				
				[self setKeyModifiers: modifiersToSend];
				[self sendKeyEvent:keyCode down:down modifiers:modifiersToSend];
				[self setKeyModifiers: oldModifiers];
			}
		}
	}
}

- (void) setKeyModifiers: (CGEventFlags) modifierFlags {
	// If it's a session tap (and we have an event source) then we can specify our own modifiers as part of the event (nothing to do here)
	// Otherwise we will have to explicitly twiddle them at the HID level based on their current state
	if (vncTapLocation == kCGHIDEventTap || !vncSourceRef) {
		CGEventRef event = nil;
		
		// Toggle the state of the appropriate keys
		if ((currentModifiers & kCGEventFlagMaskCommand) != (modifierFlags & kCGEventFlagMaskCommand)) {
			[self sendKeyEvent:keyCodeCommand down:((modifierFlags & kCGEventFlagMaskCommand) != 0) modifiers:0];
		}
		if ((currentModifiers & kCGEventFlagMaskShift) != (modifierFlags & kCGEventFlagMaskShift)) {
			[self sendKeyEvent:keyCodeShift down:((modifierFlags & kCGEventFlagMaskShift) != 0) modifiers:0];
		}
		if ((currentModifiers & kCGEventFlagMaskAlternate) != (modifierFlags & kCGEventFlagMaskAlternate)) {
			[self sendKeyEvent:keyCodeOption down:((modifierFlags & kCGEventFlagMaskAlternate) != 0) modifiers:0];
		}
		if ((currentModifiers & kCGEventFlagMaskControl) != (modifierFlags & kCGEventFlagMaskControl)) {
			[self sendKeyEvent:keyCodeControl down:((modifierFlags & kCGEventFlagMaskControl) != 0) modifiers:0];
		}
		
		if (modifierDelay)
			usleep(modifierDelay);		
	}
	currentModifiers = modifierFlags;
}

- (BOOL) checkModiferState {
	CGEventFlags actualFlags = CGEventSourceFlagsState(CGEventSourceGetSourceStateID(vncSourceRef));
	BOOL match = YES;
	
	if ((actualFlags & kCGEventFlagMaskCommand) != (currentModifiers & kCGEventFlagMaskCommand) ||
		(actualFlags & kCGEventFlagMaskShift) != (currentModifiers & kCGEventFlagMaskShift) ||
		(actualFlags & kCGEventFlagMaskAlternate) != (currentModifiers & kCGEventFlagMaskAlternate) ||
		(actualFlags & kCGEventFlagMaskControl) != (currentModifiers & kCGEventFlagMaskControl)) {
		match = NO;
		//NSLog(@"Actual(%0x) did not match Recorded (%0x)", actualFlags & 0xffffffff, currentModifiers & 0xffffffff);
	}
	return match;
}

- (void) sendKeyEvent: (CGKeyCode) keyCode down: (BOOL) down modifiers: (CGEventFlags) modifiersToSend {
	if (!vncSourceRef) {
		CGPostKeyboardEvent(0, keyCode, down);
	}
	else {
		CGEventRef event = CGEventCreateKeyboardEvent(vncSourceRef, keyCode, down);
		
		// The value of this function escapes me (since you still need to specify the keyCode for it to work
		// CGEventKeyboardSetUnicodeString (event, 1, (const UniChar *) &keySym);
		
		// If it's a session tap then we can specify our own modifiers as part of the event
		if (vncTapLocation != kCGHIDEventTap)
			CGEventSetFlags(event, modifiersToSend);
		
		CGEventPost(vncTapLocation, event);
		
		if (vncTapLocation == kCGHIDEventTap) {
			int maxWait = 250000; // 1/4 second
			
			// NEED TO WAIT UNTIL MODIFIER FLAGS REFLECT THE EXPECTED STATE
			while ([self checkModiferState] == NO && maxWait > 0) {
				maxWait -= 10000;
				usleep(10000);
			}
		}
		
		CFRelease(event);
	}
}


void SyncSetKeyboardLayout (TISInputSourceRef inputSource) {
	// http://developer.apple.com/library/mac/#documentation/TextFonts/Reference/TextInputSourcesReference/Reference/reference.html
	if (TISSelectInputSource(inputSource) != noErr) {
		NSLog(@"Error selecting input source:");
	}
}

//- (void) sendKeyEvent: (NSDictionary *) eventData {
//	NSDictionary *eventDictionary = [NSDictionary dictionaryWithObjectsAndKeys:
//									 [NSNumber numberWithInt: keyCode], @"keyCode",
//									 [NSNumber numberWithBool:down], @"down",
//									 [NSNumber numberWithLong: modifiersToSend], @"modifierFlags",
//									 nil];
//	[self performSelectorOnMainThread:@selector(sendKeyEvent:) withObject:eventDictionary waitUntilDone:YES];	
//	CGKeyCode keyCode = [[eventData objectForKey:@"keyCode"] intValue];
//	BOOL down = [[eventData objectForKey:@"down"] boolValue];
//	CGEventFlags localModifierFlags = [[eventData objectForKey:@"modifierFlags"] longValue];
//}	


@end
