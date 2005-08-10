//
//  VNCServer.m
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Mon Nov 17 2003.
//  Copyright (c) 2003 Redstone Software, Inc. All rights reserved.
//

#import "VNCServer.h"

#import "rfb.h"

#include <unistd.h>

@implementation VNCServer

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

@end
