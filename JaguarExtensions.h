//
//  JaguarExtensions.h
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Fri Jul 11 2003.
//  Copyright (c) 2003 RedstoneSoftware. All rights reserved.
//


#import <Foundation/Foundation.h>
#import <Carbon/Carbon.h>

#import "RFBBundleProtocol.h"

@interface JaguarExtensions : NSObject <RFBBundleProtocol> {
}

// Actually these are for 10.1 (Puma) and Higher but not sure if we need to bundle them
void loadKeyboard(KeyboardLayoutRef keyboardLayoutRef);

+ (void) registerRendezvous;

@end

@interface RendezvousDelegate : NSObject

@end
