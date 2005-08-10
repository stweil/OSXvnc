//
//  VNCServer.h
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Mon Nov 17 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

// This object is arround to recieve NSNotfication events, it can then dispatch them into the regular C code

#import <Foundation/Foundation.h>

@interface VNCServer : NSObject {

}

- (void) userSwitched: (NSNotification *) aNotification;
- (void) clientConnected: (NSNotification *) aNotification;

@end
