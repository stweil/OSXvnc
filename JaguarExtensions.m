//
//  JaguarExtensions.m
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Fri Jul 11 2003.
//  Copyright (c) 2003 RedstoneSoftware, Inc. All rights reserved.


#import "JaguarExtensions.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#include "keysymdef.h"
#include "kbdptr.h"

@implementation JaguarExtensions

static NSNetService *service;
static BOOL keyboardLoading;

static KeyboardLayoutRef loadedKeyboardRef;

void loadKeyboard(KeyboardLayoutRef keyboardLayoutRef);

rfbserver *theServer;

+ (void) rfbStartup: (rfbserver *) aServer {
    int argumentIndex;

    theServer = aServer;
	
    keyboardLoading = NO;
    argumentIndex = [[[NSProcessInfo processInfo] arguments] indexOfObject:@"-keyboardLoading"];
    if (argumentIndex != NSNotFound) {
        NSString *value = nil;
        
        if ([[[NSProcessInfo processInfo] arguments] count] > argumentIndex + 1)
            value = [[[NSProcessInfo processInfo] arguments] objectAtIndex:argumentIndex+1];
        
        if ([value hasPrefix:@"n"] || [value hasPrefix:@"N"] || [value hasPrefix:@"0"])
            keyboardLoading = NO;
        else
            keyboardLoading = YES;
    }

    if (keyboardLoading) {
        NSLog(@"Keyboard Loading - Enabled");

        *(theServer->pressModsForKeys) = TRUE;
        argumentIndex = [[[NSProcessInfo processInfo] arguments] indexOfObject:@"-pressModsForKeys"];
        if (argumentIndex != NSNotFound) {
            NSString *value = nil;

            if ([[[NSProcessInfo processInfo] arguments] count] > argumentIndex + 1)
                value = [[[NSProcessInfo processInfo] arguments] objectAtIndex:argumentIndex+1];

            if ([value hasPrefix:@"n"] || [value hasPrefix:@"N"] || [value hasPrefix:@"0"])
                *(theServer->pressModsForKeys) = FALSE;
        }
        if (*(theServer->pressModsForKeys))
            NSLog(@"Press Modifiers For Keys - Enabled");
        else
            NSLog(@"Press Modifiers For Keys - Disabled");

        if (KLGetCurrentKeyboardLayout(&loadedKeyboardRef) == noErr) {
            loadKeyboard(loadedKeyboardRef);
        }
    }
    else {
        NSLog(@"Keyboard Loading - Disabled");
        NSLog(@"Press Modifiers For Character - Disabled");
    }
}

+ (void) rfbUsage {
    fprintf(stderr,
            "-keyboardLoading flag  This BETA feature allows OSXvnc to look at the users selected keyboard and map keystrokes using it.\n"
            "                       Disabling this returns OSXvnc to 1.2 (U.S. Keyboard) which may work better with Dead Keys.\n"
            "                       (default: no), 10.2+ ONLY\n"
            "-pressModsForKeys flag If OSXvnc finds the key you want it will temporarily toggle the modifier keys to produce it.\n"
            "                       This flag works well if you have different keyboards on the local and remote machines.\n"
            "                       Only works if -keyboardLoading is on\n"
            "                       (default: no), 10.2+ ONLY\n"
	        "-rendezvous flag       Allow OSXvnc to advertise VNC server using Rendezvous discovery services.\n"
            "                       (default: yes), 10.2+ ONLY\n");
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
void loadKeyboard(KeyboardLayoutRef keyboardLayoutRef) {
    int i;
    int keysLoaded = 0;
    UCKeyboardLayout *uchrHandle = NULL;
    const void *kchrHandle = NULL;
    CFStringRef keyboardName;
    KeyboardLayoutKind layoutKind;
    static UInt32 modifierKeyStates[] = {0, shiftKey, optionKey, controlKey, optionKey | shiftKey};
    /* modifiers */
    //cmdKey                        = 1 << cmdKeyBit,
    //shiftKey                      = 1 << shiftKeyBit,
    //alphaLock                     = 1 << alphaLockBit,
    //optionKey                     = 1 << optionKeyBit,
    //controlKey                    = 1 << controlKeyBit,

    
    // KLGetKeyboardLayoutProperty is 10.2 only how do I access these resources in early versions?
    if (keyboardLayoutRef) {
        KLGetKeyboardLayoutProperty(keyboardLayoutRef, kKLName, (const void **) &keyboardName);
        KLGetKeyboardLayoutProperty(keyboardLayoutRef, kKLKind, (const void **) &layoutKind);
        NSLog(@"Keyboard Detected: %@ (Type:%d) - Loading Keys\n", keyboardName, layoutKind);
        if (layoutKind == kKLKCHRuchrKind || layoutKind == kKLuchrKind)
            KLGetKeyboardLayoutProperty(keyboardLayoutRef, kKLuchrData, (const void **) &uchrHandle);
        else
            KLGetKeyboardLayoutProperty(keyboardLayoutRef, kKLKCHRData, (const void **) &kchrHandle);
    }

    // Initialize them all to 0xFFFF
    memset(theServer->keyTable, 0xFF, keyTableSize * sizeof(CGKeyCode));
    // Zero out mods
    bzero(theServer->keyTableMods, keyTableSize * sizeof(unsigned char));
    
    if (uchrHandle) {
        // Ok - we could get the LIST of Modifier Key States out of the Keyboard Layout
        // some of them are duplicates so we need to compare them, then we'll iterate through them in reverse order
        // UCKeyModifiersToTableNum = ; EventRecord
        // This layout gets a little harry

        UInt16 keyCode;
        UInt32 modifierKeyState = 0;
        UInt32 keyboardType = LMGetKbdType();
        UInt32 deadKeyState = 0;
        UniCharCount actualStringLength;
        UniChar unicodeChar;

        // Iterate Ove Each Modifier Key
        for (i=0; i < (sizeof(modifierKeyStates) / sizeof(UInt32)); i++) {
            modifierKeyState = (modifierKeyStates[i] >> 8) & 0xFF;
            NSLog(@"Loading Keys For Modifer State: %#04x", modifierKeyState);
            // Iterate Over Each Key Code
            for (keyCode = 0; keyCode < 127; keyCode++) {
                OSStatus resultCode = UCKeyTranslate (uchrHandle,
                                                      keyCode,
                                                      kUCKeyActionDown,
                                                      modifierKeyState,
                                                      keyboardType,
                                                      kUCKeyTranslateNoDeadKeysBit,
                                                      &deadKeyState,
                                                      1, // Only 1 key allowed due to VNC behavior
                                                      &actualStringLength,
                                                      &unicodeChar);

                if (resultCode == kUCOutputBufferTooSmall) {
                    NSLog(@"Unable To Convert KeyCode, Multiple Characters For: %d (%#04x)",  keyCode, modifierKeyState);
                }
                else if (resultCode == noErr) {
                    // We'll use the FIRST keyCode that we find for that UNICODE character
                    if (theServer->keyTable[unicodeChar] == 0xFFFF) {
                        theServer->keyTable[unicodeChar] = keyCode;
                        theServer->keyTableMods[unicodeChar] = modifierKeyState;
                        keysLoaded++;
                    }
                }
            }
            NSLog(@"Loaded %d Keys", keysLoaded);
            keysLoaded = 0;
        }
    }
    else if (kchrHandle) {
        UInt32 modifierKeyState = 0;
        UInt16 keyCode;
        UInt32 state=0;
        UInt32 kchrCharacters;

        // Ok - we need to get the LIST of Modifier Key States out of the Keyboard Layout
        // some of them are duplicates so we need to compare them, then we'll iterate through them in reverse order
        //UCKeyModifiersToTableNum = ;
        for (i=0; i < (sizeof(modifierKeyStates) / sizeof(UInt32)); i++) {
            modifierKeyState = (modifierKeyStates[i] >> 8) & 0xFF;
            NSLog(@"Loading Keys For Modifer State:%#04x", modifierKeyState);

            // Iterate Over Each Key Code
            for (keyCode = 0; keyCode < 127; keyCode++) {
                // We pass the modifierKeys as the top 8 bits of keycode
                kchrCharacters = KeyTranslate(kchrHandle, (modifierKeyState<<8 | keyCode), &state);

                if (kchrCharacters & 0xFFFF0000) {
                    NSLog(@"Unable To Convert KeyCode, Multiple Characters (%#04x) (%#04x) For: %d (%#04x)",
                          kchrCharacters>>16 & 0xFFFF, kchrCharacters & 0xFFFF, keyCode, modifierKeyState);
                }
                else {
                    // We'll use the FIRST keyCode that we find for that UNICODE character
                    if (theServer->keyTable[kchrCharacters & 0xFFFF] == 0xFFFF) {
                        //NSLog(@"KeyCode:%d UniCode:%d", keyCode, kchrCharacters & 0xFFFF);
                        theServer->keyTable[kchrCharacters & 0xFFFF] = keyCode;
                        theServer->keyTableMods[kchrCharacters & 0xFFFF] = modifierKeyState;
                        keysLoaded++;
                    }
                }
            }
            NSLog(@"Loaded %d Keys", keysLoaded);
            keysLoaded = 0;
        }
    }
    else {
        // This is the old US only keyboard mapping
        // Map the above key table into a static array so we can just look them up directly
        NSLog(@"Unable To Determine Key Map - Reverting to US Mapping\n");
        for (i = 0; i < (sizeof(USKeyCodes) / sizeof(int)); i += 2)
            theServer->keyTable[(unsigned short)USKeyCodes[i]] = (CGKeyCode) USKeyCodes[i+1];
    }

    // This is the old SpecialKeyCodes keyboard mapping
    // Map the above key table into a static array so we can just look them up directly
    NSLog(@"Loading %d XKeysym Special Keys\n", (sizeof(SpecialKeyCodes) / sizeof(int))/2);
    for (i = 0; i < (sizeof(SpecialKeyCodes) / sizeof(int)); i += 2)
        theServer->keyTable[(unsigned short)SpecialKeyCodes[i]] = (CGKeyCode) SpecialKeyCodes[i+1];   
}

+ (void) rfbRunning {	
	[JaguarExtensions registerRendezvous];
}

+ (void) registerRendezvous {
	BOOL loadRendezvous = YES;
	int argumentIndex = [[[NSProcessInfo processInfo] arguments] indexOfObject:@"-rendezvous"];
	
    if (argumentIndex != NSNotFound) {
        NSString *value = nil;
        
        if ([[[NSProcessInfo processInfo] arguments] count] > argumentIndex + 1)
            value = [[[NSProcessInfo processInfo] arguments] objectAtIndex:argumentIndex+1];
        
        if ([value hasPrefix:@"n"] || [value hasPrefix:@"N"] || [value hasPrefix:@"0"])
            loadRendezvous = NO;
        else
            loadRendezvous = YES;
    }
	
	// Register For Rendezvous
    if (loadRendezvous) {
		 service = [[NSNetService alloc] initWithDomain:@""
												   type:@"_rfb._tcp." 
												   name:[NSString stringWithCString:theServer->desktopName]
												   port:(int) theServer->rfbPort];
		[service setDelegate:[[RendezvousDelegate alloc] init]];		

		if (![service publish])
			NSLog(@"An error occurred publishing the Rendezvous Net Service");
	}
	else
		NSLog(@"Rendezvous - Disabled");
}

+ (void) rfbPoll {
    // Check if keyboardLayoutRef has changed
    if (keyboardLoading) {
        KeyboardLayoutRef currentKeyboardLayoutRef;
		
        if (KLGetCurrentKeyboardLayout(&currentKeyboardLayoutRef) == noErr) {
            if (currentKeyboardLayoutRef != loadedKeyboardRef) {
                loadedKeyboardRef = currentKeyboardLayoutRef;
                loadKeyboard(loadedKeyboardRef);
            }
        }
    }
}

+ (void) rfbReceivedClientMessage {
    return;
}

+ (void) rfbShutdown {
    NSLog(@"Unloading Jaguar Extensions");
	if (service)
		[service stop];
}

@end

@implementation RendezvousDelegate

// Sent when the service is about to publish

- (void)netServiceWillPublish:(NSNetService *)netService {
	NSLog(@"Registering Rendezvous Service - %@", [netService name]);
}

// Sent if publication fails
- (void)netService:(NSNetService *)netService didNotPublish:(NSDictionary *)errorDict {
    NSLog(@"An error occurred with service %@.%@.%@, error code = %@",		  
		  [netService name], [netService type], [netService domain], [errorDict objectForKey:NSNetServicesErrorCode]);
}

// Sent when the service stops
- (void)netServiceDidStop:(NSNetService *)netService {	
	NSLog(@"Disabling Rendezvous Service - %@", [netService name]);
    // You may want to do something here, such as updating a user interfac
}


@end
