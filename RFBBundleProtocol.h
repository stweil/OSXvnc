//
//  RFBBundleProtocol.h
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Fri Aug 22 2003.
//  Copyright (c) 2003 RedstoneSoftware. All rights reserved.
//

#import "rfbserver.h"

@protocol RFBBundleProtocol

+ performSelector:(SEL) aSelector;

+ performSelector:(SEL) aSelector withObject: (id) anObject;

+ (void) rfbStartup: (rfbserver *) theServer;
    /* This method is called during startup and should display with NSLog some information about the Bundle */
    /* You can use NSProcessInfo to get access to the startup arguments */

+ (void) rfbUsage;
    /* Print stuff to stderr if you want to add usage information */

+ (void) rfbPoll;
    /* This method is called during each check the VNC system does for screen updates, etc. */

+ (void) rfbReceivedClientMessage;
    /* This method is called immediately as each event is received */

+ (void) rfbShutdown;
    /* This method is called when we go to shutdown the server */

@end

