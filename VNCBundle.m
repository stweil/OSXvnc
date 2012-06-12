//
//  VNCBundle.m
//  OSXvnc
//
//  Created by Jonathan Gillaspie on 3/6/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "VNCBundle.h"

#import "keysymdef.h"
#import "kbdptr.h"


@implementation VNCBundle

+ (void) initialize {
	[[NSUserDefaults standardUserDefaults] registerDefaults:
	 [NSDictionary dictionaryWithObjectsAndKeys:
	  [NSArray arrayWithObjects:[NSNumber numberWithInt:kUCKeyActionAutoKey],[NSNumber numberWithInt:kUCKeyActionDisplay],[NSNumber numberWithInt:kUCKeyActionDown], [NSNumber numberWithInt:kUCKeyActionUp], nil], @"KeyStates", // Key States to review to find KeyCodes
	  nil]];
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
+ (void) loadKeyboard: (TISInputSourceRef) inputSource forServer: (rfbserver *) theServer{
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
		uchrHandle = (CFDataRef) TISGetInputSourceProperty(inputSource, kTISPropertyUnicodeKeyLayoutData);
    }
	
    // Initialize them all to 0xFFFF
    memset(theServer->keyTable, 0xFF, keyTableSize * sizeof(CGKeyCode));
    memset(theServer->keyTableMods, 0xFF, keyTableSize * sizeof(unsigned char));
	
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
							NSLog(@"Multiple Characters For %d (%#04x): %S",  keyCode, modifierKeyState, unicodeChar);
							//unicodeChar[0] = unicodeChar[actualStringLength-1];
						}
						else {
							// We'll use the FIRST keyCode that we find for that UNICODE character
							if (theServer->keyTable[unicodeChar[0]] == 0xFFFF) {
								theServer->keyTable[unicodeChar[0]] = keyCode;
								theServer->keyTableMods[unicodeChar[0]] = modifierKeyState;
							}
						}
					}
					else {
						NSLog(@"Error Translating %d (%#04x): %d",  keyCode, modifierKeyState, resultCode);
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
            theServer->keyTable[(unsigned short)USKeyCodes[i]] = (CGKeyCode) USKeyCodes[i+1];
    }
	
    // This is the old SpecialKeyCodes keyboard mapping
    // Map the above key table into a static array so we can just look them up directly
    NSLog(@"Loading %d XKeysym Special Keys\n", (sizeof(SpecialKeyCodes) / sizeof(int))/2);
    for (i = 0; i < (sizeof(SpecialKeyCodes) / sizeof(int)); i += 2) {
        theServer->keyTable[(unsigned short)SpecialKeyCodes[i]] = (CGKeyCode) SpecialKeyCodes[i+1];
	}
}

@end
