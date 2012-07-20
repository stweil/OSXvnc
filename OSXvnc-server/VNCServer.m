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

- (void) connectHost: (NSNotification *) aNotification {
	NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
	
	char *reverseHost = (char *)[[[aNotification userInfo] objectForKey:@"ConnectHost"] UTF8String];
	int reversePort = [[[aNotification userInfo] objectForKey:@"ConnectPort"] intValue];
	
    NSLog(@"Connecting VNC Client %s(%d)",reverseHost,reversePort);
	connectReverseClient(reverseHost,reversePort);

	[pool release];	
}

@end
