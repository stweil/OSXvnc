//
//  VNCBundle.h
//  OSXvnc
//
//  Created by Jonathan Gillaspie on 3/6/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import <Carbon/Carbon.h>

#import "rfbserver.h"

@interface VNCBundle : NSObject {

}

+ (void) loadKeyboard: (TISInputSourceRef) keyboardLayoutRef forServer: (rfbserver *) theServer;

@end
