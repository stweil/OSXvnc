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

- userSwitched: (NSNotification *) aNotification {
    NSLog(@"User Switched Restarting - %@", [aNotification name]);

    sleep(10);
    rfbShutdown();

    exit(2);
}

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
