//
//  VNCServer.m
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Mon Nov 17 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

#import "VNCServer.h"

@implementation VNCServer

- userSwitched: (NSNotification *) aNotification {
    rfbLog("User Switched Restarting - %@", [aNotification name]);

    rfbShutdown();

    exit(2);
}

@end
