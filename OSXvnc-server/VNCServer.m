//
//  VNCServer.m
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Mon Nov 17 2003.
//  Copyright (c) 2003 Redstone Software, Inc. All rights reserved.
//

#import "VNCServer.h"

#import "rfb.h"
#import "keysymdef.h"
#import "kbdptr.h"
#import "rfbserver.h"


#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
//#import <Security/AuthSession.h>


#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>



@implementation VNCServer

static VNCServer *sharedServer = nil;
// These correspond to the keycodes of the keys 0-9,A-F
static int unicodeNumbersToKeyCodes[16] = { 29, 18, 19, 20, 21, 23, 22, 26, 28, 25, 0, 11, 8, 2, 14, 3 };

+ sharedServer {
	if (!sharedServer) {
		sharedServer = [[self alloc] init];
	}
	return sharedServer;
}


- init {
	self = [super init];
	
	if (self) {		
		keyboardLoading = FALSE;
		
		useIP6 = TRUE;
		listenerFinished = FALSE;
		
		readyToStartup = TRUE;
		dynamicKeyboard = FALSE;
		
        [self loadKeyTable];
	}
	return self;
}

- (void) loadKeyTable {
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


	if (1) {
		// Find the right keycodes base on the loaded keyboard
		keyCodeShift = keyTable[XK_Shift_L];
		keyCodeOption = keyTable[XK_Meta_L];
		keyCodeControl = keyTable[XK_Control_L];
		keyCodeCommand = keyTable[XK_Alt_L];
	}		
	

}


// Here are the resources we can think about using for Int'l keyboard support

// http://developer.apple.com/documentation/Carbon/Reference/KeyboardLayoutServices/

// This will let us determine the current keyboard
// KLGetCurrentKeyboardLayout

// This will set the current keyboard
// KLSetCurrentKeyboardLayout

// This will get the Properties of a keyboard (like the key code to char tables) but...
// KLGetKeyboardLayoutProperty

// UCKeyTranslate - This is the opposite of what we need, if you give it a table and some keyCode (key input) it will tell you what Unicode char (or string) you get
// we need the opposite - we want to know what keys to hit to get a given key (or string)

// This will use the KeyboardLayoutRef to produce a static table of lookups
// By iterating through all possible KeyCodes
- (void) loadKeyboard: (TISInputSourceRef) inputSource {
    int i, j;
    UCKeyboardLayout *uchrHandle = NULL;
    CFStringRef keyboardName;
    static UInt32 modifierKeyStates[] = {0, shiftKey, optionKey, controlKey, optionKey | shiftKey, optionKey | controlKey, controlKey | shiftKey, optionKey | shiftKey | controlKey};
	UInt32 modifierKeyState = 0;	
	NSArray *keyStates = [[NSUserDefaults standardUserDefaults] arrayForKey:@"KeyStates"];
	
    /* modifiers */
    //cmdKey                        = 1 << cmdKeyBit,
    //shiftKey                      = 1 << shiftKeyBit,
    //alphaLock                     = 1 << alphaLockBit,
    //optionKey                     = 1 << optionKeyBit,
    //controlKey                    = 1 << controlKeyBit,
    
    // KLGetKeyboardLayoutProperty is 10.2 only how do I access these resources in early versions?
    if (inputSource) {
        keyboardName = (CFStringRef) TISGetInputSourceProperty(inputSource, kTISPropertyLocalizedName);
        NSLog(@"Keyboard Detected: %@ - Loading Keys\n", keyboardName);
		uchrHandle = (UCKeyboardLayout *) CFDataGetBytePtr(TISGetInputSourceProperty(inputSource, kTISPropertyUnicodeKeyLayoutData));
    }
	
    // Initialize them all to 0xFFFF
    memset(keyTable, 0xFF, keyTableSize * sizeof(CGKeyCode));
    memset(keyTableMods, 0xFF, keyTableSize * sizeof(unsigned char));
	
    if (uchrHandle) {
        // Ok - we could get the LIST of Modifier Key States out of the Keyboard Layout
        // some of them are duplicates so we need to compare them, then we'll iterate through them in reverse order
        // UCKeyModifiersToTableNum = ; EventRecord
        // This layout gets a little harry
		
        UInt16 keyCode;
        UInt32 keyboardType = LMGetKbdType();
        UInt32 deadKeyState = 0;
        UniCharCount actualStringLength;
        UniChar unicodeChar[255];
		
        // Iterate Over Each Modifier Keyset
        for (i=0; i < (sizeof(modifierKeyStates) / sizeof(UInt32)); i++) {
            modifierKeyState = (modifierKeyStates[i] >> 8) & 0xFF;
            //NSLog(@"Loading Keys For Modifer State: %#04x", modifierKeyState);
            // Iterate Over Each Key Code
            for (keyCode = 0; keyCode < 255; keyCode++) {
				for (j=0; j < [keyStates count]; j++) {
					int keyActionState = [[keyStates objectAtIndex:j] intValue];
                    UInt32 deadKeyState = 0;
					OSStatus resultCode = UCKeyTranslate (uchrHandle,
														  keyCode,
														  keyActionState,
														  modifierKeyState,
														  keyboardType,
														  kUCKeyTranslateNoDeadKeysBit,
														  &deadKeyState,
														  255, // Only 1 key allowed due to VNC behavior
														  &actualStringLength,
														  unicodeChar);
					
					if (resultCode == noErr) {
						if (actualStringLength > 1) {
							NSLog(@"Multiple Characters For %d (%#04lx): %S",  keyCode, modifierKeyState, (int *) unicodeChar);
							//unicodeChar[0] = unicodeChar[actualStringLength-1];
						}
						else {
                            NSLog(@"Loaded %d (%04lx)",  keyCode, modifierKeyState);
							// We'll use the FIRST keyCode that we find for that UNICODE character
							if (keyTable[unicodeChar[0]] == 0xFFFF) {
								keyTable[unicodeChar[0]] = keyCode;
								keyTableMods[unicodeChar[0]] = modifierKeyState;
							}
						}
					}
					else {
						NSLog(@"Error Translating %d (%04lx): %s - %s",  keyCode, modifierKeyState, GetMacOSStatusErrorString(resultCode), GetMacOSStatusCommentString(resultCode));
					}
				}
			}
        }
    }
    else {
        // This is the old US only keyboard mapping
        // Map the above key table into a static array so we can just look them up directly
        NSLog(@"Unable To Determine Key Map - Reverting to US Mapping\n");
        for (i = 0; i < (sizeof(USKeyCodes) / sizeof(int)); i += 2)
            keyTable[(unsigned short)USKeyCodes[i]] = (CGKeyCode) USKeyCodes[i+1];
    }
	
    // This is the old SpecialKeyCodes keyboard mapping
    // Map the above key table into a static array so we can just look them up directly
    NSLog(@"Loading %ld XKeysym Special Keys\n", (sizeof(SpecialKeyCodes) / sizeof(int))/2);
    for (i = 0; i < (sizeof(SpecialKeyCodes) / sizeof(int)); i += 2) {
        keyTable[(unsigned short)SpecialKeyCodes[i]] = (CGKeyCode) SpecialKeyCodes[i+1];
	}

    keyCodeShift = keyTable[XK_Shift_L];
    keyCodeOption = keyTable[XK_Meta_L];
    keyCodeControl = keyTable[XK_Control_L];
    keyCodeCommand = keyTable[XK_Alt_L];
}

void SyncSetKeyboardLayout (TISInputSourceRef inputSource) {
	// http://developer.apple.com/library/mac/#documentation/TextFonts/Reference/TextInputSourcesReference/Reference/reference.html
	if (TISSelectInputSource(inputSource) != noErr) {
		NSLog(@"Error selecting input source:");
	}
}

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

- (void) rfbStartup: (rfbserver *) aServer {    
	[[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
															 @"NO", @"UnicodeKeyboard", // Load The Unicode Keyboard
															 @"NO", @"DynamicKeyboard", // Try to set the keyboard "as we need it", doesn't work well on Tiger
															 @"-1", @"UnicodeKeyboardIdentifier", // ID of the Unicode Keyboard resource to use (-1 is Apple's)
															 
															 @"2", @"EventSource", // Always private event source so we don't consolidate with existing keys (however HID for the EventTap always does anyhow)
															 @"3", @"EventTap", // Default Event Tap (3=HID for Console User and Session For OffScreen Users)
															 @"5000", @"ModifierDelay", // Delay when shifting modifier keys
															 @"NO", @"SystemServer", 
															 @"NO", @"keyboardLoading", // allows OSXvnc to look at the users selected keyboard and map keystrokes using it
															 @"YES", @"pressModsForKeys", // If OSXvnc finds the key you want it will temporarily toggle the modifier keys to produce it
															 [NSArray arrayWithObjects:[NSNumber numberWithInt:kUCKeyActionAutoKey /*kUCKeyActionDisplay*/], nil], @"KeyStates", // Key States to review to find KeyCodes
															 nil]];
	
    theServer = aServer;
	
	// System Server special behavior
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"SystemServer"]) {
		// We need quit if the user switches out (so we can relinquish the port)
		if (isConsoleSession()) {
			[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
																   selector:@selector(systemServerShouldQuit:)
																	   name: NSWorkspaceSessionDidResignActiveNotification
																	 object:nil];			
		}
		// We need to be able to "hold" if we aren't the console session
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
					NSLog(@"Received Result: %ld during event loop, Shutting Down", resultCode);
					//rfbShutdown();
					exit(0);
				}
			}
		}
	}
	
	modifierDelay = [[NSUserDefaults standardUserDefaults] integerForKey:@"ModifierDelay"];
	
	keyboardLoading = [[NSUserDefaults standardUserDefaults] boolForKey:@"keyboardLoading"];	
    if (keyboardLoading) {
		OSErr result;
		
        NSLog(@"Keyboard Loading - Enabled");
		
        pressModsForKeys = [[NSUserDefaults standardUserDefaults] boolForKey:@"pressModsForKeys"];
        if (pressModsForKeys)
            NSLog(@"Press Modifiers For Keys - Enabled");
        else
            NSLog(@"Press Modifiers For Keys - Disabled");
		
        
        loadedKeyboardRef = TISCopyCurrentKeyboardLayoutInputSource();
        if (loadedKeyboardRef)
			[self loadKeyboard:loadedKeyboardRef];
		else
			NSLog(@"Error (%u) unabled to load current keyboard layout", result);
    }
	
    if ([[[NSProcessInfo processInfo] arguments] indexOfObject:@"-ipv4"] != NSNotFound) {
		useIP6 = FALSE;
	}
	
	[self loadUnicodeKeyboard];
}

- (void) systemServerShouldQuit: (NSNotification *) aNotification {
    NSLog(@"User Switched Out, Stopping System Server - %@", [aNotification name]);
	//rfbShutdown();
	exit(0);
	return;
}
- (void) systemServerShouldContinue: (NSNotification *) aNotification {
    NSLog(@"User Switched In, Starting System Server - %@", [aNotification name]);
	readyToStartup = YES;
	return;
}

- (void) rfbRunning {    
	[self registerRendezvous];
	
	if (useIP6) {
		[NSThread detachNewThreadSelector:@selector(setupIPv6:) toTarget:self withObject:nil];
		// Wait for the IP6 to bind, if it binds later it confuses the IPv4 binding into allowing others on the port
		while (!listenerFinished)
			usleep(1000); 
	}
	
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
			// Combines only with other User Session Events
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

- (void) loadUnicodeKeyboard {
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"UnicodeKeyboard"] && unicodeInputSource == NULL) {
		// Need to figure out a way to lookup the Unicode Keyboard before this will work
		
		// OSStatus result = KLGetKeyboardLayoutWithIdentifier([[NSUserDefaults standardUserDefaults] integerForKey:@"UnicodeKeyboardIdentifier"], &unicodeLayout);
		//  *    Use TISCreateInputSourceList API to create a list of input
        //        *    sources that match specified properties, such as the
        //*    kTISPropertyInputSourceID property.

		// Unicode Keyboard Should load keys from definition
		pressModsForKeys = YES;
		[self loadKeyboard:unicodeInputSource];
	}	
}


- (void) userSwitchedIn: (NSNotification *) aNotification {
    NSLog(@"User Switched In, Using HID Tap - %@", [aNotification name]);
	vncTapLocation = kCGHIDEventTap;
	return;
}
- (void) userSwitchedOut: (NSNotification *) aNotification {
    NSLog(@"User Switched Out, Using Session Tap - %@", [aNotification name]);
	vncTapLocation = kCGSessionEventTap;	
	return;
}

- (void) rfbConnect {
	if (unicodeInputSource != NULL && !dynamicKeyboard) {
		currentInputSource = TISCopyCurrentKeyboardInputSource();
		// Switch to Unicode Keyboard
		SyncSetKeyboardLayout(unicodeInputSource);
	}
}

- (void) rfbDisconnect {    
	if (unicodeInputSource != NULL && !dynamicKeyboard) {
		// Switch to Old Keyboard
		SyncSetKeyboardLayout(currentInputSource);
	}
}

- (void) rfbUsage {    
    fprintf(stderr,
            "-keyboardLoading flag  This feature allows OSXvnc to look at the users selected keyboard and map keystrokes using it.\n"
            "                       Disabling this returns OSXvnc to standard (U.S. Keyboard) which will work better with Dead Keys.\n"
            "                       (default: no)\n"
            "-pressModsForKeys flag If OSXvnc finds the key you want it will temporarily toggle the modifier keys to produce it.\n"
            "                       This flag works well if you have different keyboards on the local and remote machines.\n"
            "                       Only works if -keyboardLoading is on\n"
            "                       (default: yes)\n"
	        "-bonjour flag       Allow OSXvnc to advertise VNC server using Bonjour discovery services.\n"
			"                       'VNC' will enable the service named VNC (For Eggplant & Chicken 2.02b)\n"
			"                       'Both' or '2' will enable the services named RFB and VNC\n"
			"                       (default: RFB:YES VNC:NO)\n"
	        "-ipv4                  Listen For Connections on IPv4 ONLY (Default: Off)\n"
	        "-ipv6                  Listen For Connections on IPv6 ONLY (Default: Off)\n"
			);
}

- (void) rfbPoll { 	
	// Check if keyboardLayoutRef has changed
    if (keyboardLoading) {
        TISInputSourceRef currentKeyboardLayoutRef = TISCopyCurrentKeyboardLayoutInputSource();
        
        if (currentKeyboardLayoutRef != loadedKeyboardRef) {
            loadedKeyboardRef = currentKeyboardLayoutRef;
            [self loadKeyboard: loadedKeyboardRef];
        }
    }
	
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

- (void) rfbShutdown {
	[rfbService stop];
	[vncService stop];
}

// Mouse handling code

- (void) handleMouseButtons:(int) buttonMask atPoint:(NSPoint) aPoint forClient: (rfbClientPtr) cl {
    rfbUndim();
    
    if (buttonMask & rfbWheelMask) {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        NSUserDefaults *currentUserDefs = [[NSUserDefaults alloc] initWithUser:NSUserName()];
        CGEventRef scrollEvent;
        int mouseWheelDistance;
 
        // I would rather cache this data than look it up each time but I don't know how to get notification of a change
        // A - User changes his setting in SysPrefs
        // B - Running OSXvnc as root and user swiches
        
        mouseWheelDistance = 8 * [currentUserDefs floatForKey:@"com.apple.scrollwheel.scaling"];
        if (!mouseWheelDistance)
            mouseWheelDistance = 10;
        
        if (buttonMask & rfbWheelUpMask)
            scrollEvent = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitPixel,  1,  mouseWheelDistance);
        
        if (buttonMask & rfbWheelDownMask)
            scrollEvent = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitPixel,  1, -mouseWheelDistance);
        
        CGEventPost(vncTapLocation, scrollEvent);
        [pool release];
    }
    else {
        cl->clientCursorLocation.x = aPoint.x;
        cl->clientCursorLocation.y = aPoint.y;
        
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

// Keyboard handling code
- (void) handleKeyboard:(Bool) down forSym: (KeySym) keySym forClient: (rfbClientPtr) cl {
	CGKeyCode keyCode = keyTable[(unsigned short)keySym];
	CGEventFlags modifiersToSend = 0;
	
    rfbUndim();	
	
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
			if (pressModsForKeys) {
				if (keyTableMods[keySym] != 0xFF) {					
					// Setup the state of the appropriate keys based on the value in the KeyTableMods
					CGEventFlags oldModifiers = currentModifiers;
					CGEventFlags modifiersToSend = kCGEventFlagMaskNonCoalesced;
					
					if ((keyTableMods[keySym] << 8) & shiftKey)
						modifiersToSend |= kCGEventFlagMaskShift;
					if ((keyTableMods[keySym] << 8) & optionKey)
						modifiersToSend |= kCGEventFlagMaskAlternate;
					if ((keyTableMods[keySym] << 8) & controlKey)
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

- (void) releaseModifiersForClient: (rfbClientPtr) cl {
    [self setKeyModifiers: 0];
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
        CGEventRef event = CGEventCreateKeyboardEvent(NULL, keyCode, down);
        CGEventPost(vncTapLocation, event);
		//CGPostKeyboardEvent(0, keyCode, down);
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

- (void) setupIPv6: argument {
    int listen_fd6=0, client_fd=0;
	int value=1;  // Need to pass a ptr to this
	struct sockaddr_in6 sin6, peer6;
	unsigned int len6=sizeof(sin6);
	
	bzero(&sin6, sizeof(sin6));
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(theServer->rfbPort);
	if (theServer->rfbLocalhostOnly)
		sin6.sin6_addr = in6addr_loopback;
	else
		sin6.sin6_addr = in6addr_any;
	
	if ((listen_fd6 = socket(PF_INET6, SOCK_STREAM, 0)) < 0) {
		NSLog(@"IPv6: Unable to open socket");
	}
	/*
	 else if (fcntl(listen_fd6, F_SETFL, O_NONBLOCK) < 0) {
	 NSLog(@"IPv6: fcntl O_NONBLOCK failed\n");
	 }
	 */
	else if (setsockopt(listen_fd6, IPPROTO_IPV6, IPV6_V6ONLY, &value, sizeof(value)) < 0) {
		NSLog(@"IPv6: setsockopt IPV6_V6ONLY failed\n");
	}
	else if (setsockopt(listen_fd6, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
		NSLog(@"IPv6: setsockopt SO_REUSEADDR failed\n");
	}
	else if (bind(listen_fd6, (struct sockaddr *) &sin6, len6) < 0) {
		NSLog(@"IPv6: Failed to Bind Socket: Port %d may be in use by another VNC\n", theServer->rfbPort);
	}
	else if (listen(listen_fd6, 5) < 0) {
		NSLog(@"IPv6: Listen failed\n");
	}
	else {
		NSLog(@"IPv6: Started Listener Thread on port %d\n", theServer->rfbPort);
		listenerFinished = TRUE;
		
	    while ((client_fd = accept(listen_fd6, (struct sockaddr *) &peer6, &len6)) !=-1) {
			NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
			
			[[NSNotificationCenter defaultCenter] postNotification:
			 [NSNotification notificationWithName:@"NewRFBClient" object:[NSNumber numberWithInt:client_fd]]];
			
			// We have to trigger a signal so the event loop will pickup (if no clients are connected)
			pthread_cond_signal(&(theServer->listenerGotNewClient));
			
			[pool release];
		}
		
		NSLog(@"IPv6: Accept failed %d\n", errno);
	}
	listenerFinished = TRUE;
	
	return;
}

- (void) registerRendezvous {
    NSAutoreleasePool *tempPool = [[NSAutoreleasePool alloc] init];
	BOOL loadRendezvousVNC = NO;
	BOOL loadRendezvousRFB = YES;
	int argumentIndex = [[[NSProcessInfo processInfo] arguments] indexOfObject:@"-rendezvous"];
	RendezvousDelegate *rendezvousDelegate = [[RendezvousDelegate alloc] init];
	
    if (argumentIndex == NSNotFound) {
		argumentIndex = [[[NSProcessInfo processInfo] arguments] indexOfObject:@"-bonjour"];
	}
	
    if (argumentIndex != NSNotFound) {
        NSString *value = nil;
        
        if ([[[NSProcessInfo processInfo] arguments] count] > argumentIndex + 1)
            value = [[[NSProcessInfo processInfo] arguments] objectAtIndex:argumentIndex+1];
        
        if ([value hasPrefix:@"n"] || [value hasPrefix:@"N"] || [value hasPrefix:@"0"]) {
            loadRendezvousVNC = NO; loadRendezvousRFB = NO;
		}
		else if ([value hasPrefix:@"y"] || [value hasPrefix:@"Y"] || [value hasPrefix:@"1"] || [value hasPrefix:@"rfb"]) {
			loadRendezvousVNC = NO; loadRendezvousRFB = YES;
		}
		else if ([value hasPrefix:@"b"] || [value hasPrefix:@"B"] || [value hasPrefix:@"2"]) {
			loadRendezvousVNC = YES; loadRendezvousRFB = YES; 
		}
		else if ([value hasPrefix:@"vnc"]) {
			loadRendezvousVNC = YES; loadRendezvousRFB = NO;
		}
    }
	
	// Register For Rendezvous
    if (loadRendezvousRFB) {
		rfbService = [[NSNetService alloc] initWithDomain:@""
                                                     type:@"_rfb._tcp." 
                                                     name:[NSString stringWithUTF8String:theServer->desktopName]
                                                     port:(int) theServer->rfbPort];
		[rfbService setDelegate:rendezvousDelegate];		
		[rfbService publish];
	}
	//	else
	//		NSLog(@"Bonjour (_rfb._tcp) - Disabled");
	
	if (loadRendezvousVNC) {
		vncService = [[NSNetService alloc] initWithDomain:@""
                                                     type:@"_vnc._tcp." 
                                                     name:[NSString stringWithUTF8String:theServer->desktopName]
                                                     port:(int) theServer->rfbPort];
		[vncService setDelegate:rendezvousDelegate];		
		
		[vncService publish];
	}
	//	else
	//		NSLog(@"Bonjour (_vnc._tcp) - Disabled");
    
    [tempPool release];
}

- (void) rfbReceivedClientMessage {
    return;
}

- (void) userSwitched: (NSNotification *) aNotification {
    NSLog(@"User Switched Restarting - %@", [aNotification name]);
	
    sleep(10);
    rfbShutdown();
	
    exit(2);
}

- (void) clientConnected: (NSNotification *) aNotification {
    NSLog(@"New IPv6 Client Notification - %@", [aNotification name]);
	rfbStartClientWithFD([[aNotification object] intValue]);
}

- (void) connectHost: (NSNotification *) aNotification {
	NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
	
	char *reverseHost = (char *)[[[aNotification userInfo] objectForKey:@"ConnectHost"] UTF8String];
	int reversePort = [[[aNotification userInfo] objectForKey:@"ConnectPort"] intValue];
	
    NSLog(@"Connecting VNC Client %s(%d)",reverseHost,reversePort);
	connectReverseClient(reverseHost,reversePort);
	
	[pool release];	
}

@end




@implementation RendezvousDelegate

// Sent when the service is about to publish

- (void)netServiceWillPublish:(NSNetService *)netService {
	NSLog(@"Registering Bonjour Service(%@) - %@", [netService type], [netService name]);
}

// Sent if publication fails
- (void)netService:(NSNetService *)netService didNotPublish:(NSDictionary *)errorDict {
    NSLog(@"An error occurred with service %@.%@.%@, error code = %@",		  
		  [netService name], [netService type], [netService domain], [errorDict objectForKey:NSNetServicesErrorCode]);
}

// Sent when the service stops
- (void)netServiceDidStop:(NSNetService *)netService {	
	NSLog(@"Disabling Bonjour Service - %@", [netService name]);
    // You may want to do something here, such as updating a user interfac
}


@end
