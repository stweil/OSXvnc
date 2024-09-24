//
//  VNCServer.h
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Mon Nov 17 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

// This object is around to receive NSNotfication events, it can then dispatch them into the regular C code

#import <Foundation/Foundation.h>
#import <Carbon/Carbon.h>

#import "rfb.h"
#import "rfbserver.h"
#import <X11/keysym.h>
#import "kbdptr.h"


@interface VNCServer : NSObject {
	NSNetService *rfbService;
	NSNetService *vncService;
	BOOL keyboardLoading;

	TISInputSourceRef loadedKeyboardRef;
	BOOL useIP6;
	BOOL listenerFinished;

	rfbserver *theServer;

	BOOL readyToStartup;
	BOOL dynamicKeyboard;
    // This flag will try to change the modifier key state to the required set for the unicode key that came in
    BOOL pressModsForKeys;
	CGEventSourceRef vncSourceRef;
	CGEventTapLocation vncTapLocation;

	TISInputSourceRef unicodeInputSource;
	TISInputSourceRef currentInputSource;

	// Current Modifiers reflect the "global" state of the modifier keys (not a particular VNC connection)
	// This only matters for event taps.  Ideally we could detect their values but
	// that seems to require an active event loop.
	CGEventFlags currentModifiers;

    CGKeyCode keyTable[keyTableSize];
    unsigned char keyTableMods[keyTableSize]; // 8 Bits for Modifier Keys

    // The Keycodes to various modifiers on the current keyboard
    CGKeyCode keyCodeShift;
    CGKeyCode keyCodeOption;
    CGKeyCode keyCodeControl;
    CGKeyCode keyCodeCommand;

    NSInteger modifierDelay;
}

+ (instancetype)sharedServer;

- (void) loadKeyboard: (TISInputSourceRef) keyboardLayoutRef;
- (void) registerRendezvous;

- (void) setupIPv6: argument;


- (void) rfbStartup: (rfbserver *) aServer;
- (void) rfbUsage;
- (void) rfbRunning;

- (void) rfbConnect;
- (void) rfbDisconnect;

- (void) rfbPoll;
- (void) rfbReceivedClientMessage;
- (void) rfbShutdown;

- (void) systemServerShouldQuit: (NSNotification *) aNotification;
- (void) systemServerShouldContinue: (NSNotification *) aNotification;

- (void) loadUnicodeKeyboard;

- (void) handleMouseButtons:(int) buttonMask atPoint:(NSPoint) aPoint forClient: (rfbClientPtr) cl;
- (void) handleKeyboard:(Bool) down forSym: (KeySym) keySym forClient: (rfbClientPtr) cl;
- (void) releaseModifiersForClient: (rfbClientPtr) cl;

- (void) setKeyModifiers: (CGEventFlags) modifierFlags;
- (BOOL) checkModiferState;
- (void) sendKeyEvent: (CGKeyCode) keyCode down: (BOOL) down modifiers: (CGEventFlags) modifiersToSend;

- (void) userSwitched: (NSNotification *) aNotification;
- (void) clientConnected: (NSNotification *) aNotification;
- (void) connectHost: (NSNotification *) aNotification;


@end

@interface RendezvousDelegate : NSObject <NSNetServiceDelegate> {
}

@end
