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

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include "rfb.h"

#import "rfbserver.h"
#import "VNCServer.h"

#import "../RFBBundleProtocol.h"

@implementation JaguarExtensions

static NSNetService *rfbService;
static NSNetService *vncService;
static BOOL keyboardLoading = FALSE;

static KeyboardLayoutRef loadedKeyboardRef;
static BOOL useIP6 = TRUE;
static BOOL listenerFinished = FALSE;

rfbserver *theServer;

+ (void) loadGUI {
	//	ProcessSerialNumber psn = { 0, kCurrentProcess }; 
	//	OSStatus returnCode = TransformProcessType(& psn, kProcessTransformToForegroundApplication);
	//	returnCode = SetFrontProcess(& psn );
	//	if( returnCode != 0) {
	//		NSLog(@"Could not bring the application to front. Error %d", returnCode);
	//	}
	//[[NSApplication sharedApplication] activateIgnoringOtherApps: YES];
}

+ (void) rfbStartup: (rfbserver *) aServer {
	[[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
		@"NO", @"keyboardLoading", // allows OSXvnc to look at the users selected keyboard and map keystrokes using it
		@"YES", @"pressModsForKeys", // If OSXvnc finds the key you want it will temporarily toggle the modifier keys to produce it
		[NSArray arrayWithObjects:[NSNumber numberWithInt:kUCKeyActionDisplay], [NSNumber numberWithInt:kUCKeyActionAutoKey], nil], @"KeyStates", // Key States to review to find KeyCodes
		//[NSNumber numberWithInt:kUCKeyActionDisplay], [NSNumber numberWithInt:kUCKeyActionDown], nil],
		nil]];

    theServer = aServer;
	
	keyboardLoading = [[NSUserDefaults standardUserDefaults] boolForKey:@"keyboardLoading"];	
    if (keyboardLoading) {
		OSErr result;
		
        NSLog(@"Keyboard Loading - Enabled");

        *(theServer->pressModsForKeys) = [[NSUserDefaults standardUserDefaults] boolForKey:@"pressModsForKeys"];
        if (*(theServer->pressModsForKeys))
            NSLog(@"Press Modifiers For Keys - Enabled");
        else
            NSLog(@"Press Modifiers For Keys - Disabled");

		result = KLGetCurrentKeyboardLayout(&loadedKeyboardRef);
        if (result == noErr)
			[self loadKeyboard:loadedKeyboardRef forServer: theServer];
		else
			NSLog(@"Error (%u) unabled to load current keyboard layout", result);
    }
	
    if ([[[NSProcessInfo processInfo] arguments] indexOfObject:@"-ipv4"] != NSNotFound) {
		useIP6 = FALSE;
	}
}

+ (void) rfbUsage {
    fprintf(stderr,
            "\nJAGUAR BUNDLE OPTIONS (10.2+):\n"
            "-keyboardLoading flag  This feature allows OSXvnc to look at the users selected keyboard and map keystrokes using it.\n"
            "                       Disabling this returns OSXvnc to standard (U.S. Keyboard) which will work better with Dead Keys.\n"
            "                       (default: no), 10.2+ ONLY\n"
            "-pressModsForKeys flag If OSXvnc finds the key you want it will temporarily toggle the modifier keys to produce it.\n"
            "                       This flag works well if you have different keyboards on the local and remote machines.\n"
            "                       Only works if -keyboardLoading is on\n"
            "                       (default: yes), 10.2+ ONLY\n"
	        "-bonjour flag       Allow OSXvnc to advertise VNC server using Bonjour discovery services.\n"
			"                       'VNC' will enable the service named VNC (For Eggplant & Chicken 2.02b)\n"
			"                       'Both' or '2' will enable the services named RFB and VNC\n"
			"                       (default: RFB:YES VNC:NO), 10.2+ ONLY\n"
	        "-ipv4                  Listen For Connections on IPv4 ONLY (Default: Off). 10.2+ ONLY\n"
	        "-ipv6                  Listen For Connections on IPv6 ONLY (Default: Off). 10.2+ ONLY\n"
			);
}

+ (void) rfbRunning {	
	[JaguarExtensions registerRendezvous];
	if (useIP6) {
		[NSThread detachNewThreadSelector:@selector(setupIPv6:) toTarget:self withObject:nil];
		// Wait for the IP6 to bind, if it binds later it confuses the IPv4 binding into allowing others on the port
		while (!listenerFinished)
			usleep(1000); 
	}
}

+ (void) setupIPv6: argument {
    int listen_fd6=0, client_fd=0;
	int value=1;  // Need to pass a ptr to this
	struct sockaddr_in6 sin6, peer6;
	unsigned int len6=sizeof(sin6);
	
	bzero(&sin6, sizeof(sin6));
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(theServer->rfbPort);
	if (theServer->rfbLocalhostOnly)
		sin6.sin6_addr = in6addr_loopback;
	else
		sin6.sin6_addr = in6addr_any;
	
	if ((listen_fd6 = socket(PF_INET6, SOCK_STREAM, 0)) < 0) {
		NSLog(@"IPv6: Unable to open socket");
	}
	/*
	    else if (fcntl(listen_fd6, F_SETFL, O_NONBLOCK) < 0) {
			NSLog(@"IPv6: fcntl O_NONBLOCK failed\n");
		}
	 */
	else if (setsockopt(listen_fd6, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
		NSLog(@"IPv6: setsockopt SO_REUSEADDR failed\n");
	}
	else if (bind(listen_fd6, (struct sockaddr *) &sin6, len6) < 0) {
		NSLog(@"IPv6: Failed to Bind Socket: Port %d may be in use by another VNC\n", theServer->rfbPort);
	}
	else if (listen(listen_fd6, 5) < 0) {
		NSLog(@"IPv6: Listen failed\n");
	}
	else {
		NSLog(@"IPv6: Started Listener Thread on port %d\n", theServer->rfbPort);
		listenerFinished = TRUE;
		
	    while ((client_fd = accept(listen_fd6, (struct sockaddr *) &peer6, &len6)) !=-1) {
			NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
			
			[[NSNotificationCenter defaultCenter] postNotification:
				[NSNotification notificationWithName:@"NewRFBClient" object:[NSNumber numberWithInt:client_fd]]];
			
			// We have to trigger a signal so the event loop will pickup (if no clients are connected)
			pthread_cond_signal(&(theServer->listenerGotNewClient));
			
			[pool release];
		}
		
		NSLog(@"IPv6: Accept failed %d\n", errno);
	}
	listenerFinished = TRUE;
	
	return;
}

+ (void) registerRendezvous {
	BOOL loadRendezvousVNC = NO;
	BOOL loadRendezvousRFB = YES;
	int argumentIndex = [[[NSProcessInfo processInfo] arguments] indexOfObject:@"-rendezvous"];
	RendezvousDelegate *rendezvousDelegate = [[RendezvousDelegate alloc] init];
	
    if (argumentIndex == NSNotFound) {
		argumentIndex = [[[NSProcessInfo processInfo] arguments] indexOfObject:@"-bonjour"];
	}
	
    if (argumentIndex != NSNotFound) {
        NSString *value = nil;
        
        if ([[[NSProcessInfo processInfo] arguments] count] > argumentIndex + 1)
            value = [[[NSProcessInfo processInfo] arguments] objectAtIndex:argumentIndex+1];
        
        if ([value hasPrefix:@"n"] || [value hasPrefix:@"N"] || [value hasPrefix:@"0"]) {
            loadRendezvousVNC = NO; loadRendezvousRFB = NO;
		}
		else if ([value hasPrefix:@"y"] || [value hasPrefix:@"Y"] || [value hasPrefix:@"1"] || [value hasPrefix:@"rfb"]) {
			loadRendezvousVNC = NO; loadRendezvousRFB = YES;
		}
		else if ([value hasPrefix:@"b"] || [value hasPrefix:@"B"] || [value hasPrefix:@"2"]) {
			loadRendezvousVNC = YES; loadRendezvousRFB = YES; 
		}
		else if ([value hasPrefix:@"vnc"]) {
			loadRendezvousVNC = YES; loadRendezvousRFB = NO;
		}
    }
	
	// Register For Rendezvous
    if (loadRendezvousRFB) {
		rfbService = [[NSNetService alloc] initWithDomain:@""
												   type:@"_rfb._tcp." 
												   name:[NSString stringWithCString:theServer->desktopName]
												   port:(int) theServer->rfbPort];
		[rfbService setDelegate:rendezvousDelegate];		
		[rfbService publish];
	}
//	else
//		NSLog(@"Bonjour (_rfb._tcp) - Disabled");

	if (loadRendezvousVNC) {
		vncService = [[NSNetService alloc] initWithDomain:@""
												  type:@"_vnc._tcp." 
												  name:[NSString stringWithCString:theServer->desktopName]
												  port:(int) theServer->rfbPort];
		[vncService setDelegate:rendezvousDelegate];		
		
		[vncService publish];
	}
//	else
//		NSLog(@"Bonjour (_vnc._tcp) - Disabled");
}

+ (void) rfbPoll {
    // Check if keyboardLayoutRef has changed
    if (keyboardLoading) {
        KeyboardLayoutRef currentKeyboardLayoutRef;
		
        if (KLGetCurrentKeyboardLayout(&currentKeyboardLayoutRef) == noErr) {
            if (currentKeyboardLayoutRef != loadedKeyboardRef) {
                loadedKeyboardRef = currentKeyboardLayoutRef;
				[self loadKeyboard: loadedKeyboardRef forServer:theServer];
            }
        }
    }
}

+ (void) rfbReceivedClientMessage {
    return;
}

+ (void) rfbShutdown {
    NSLog(@"Unloading Jaguar Extensions");
	[rfbService stop];
	[vncService stop];
}

@end

@implementation RendezvousDelegate

// Sent when the service is about to publish

- (void)netServiceWillPublish:(NSNetService *)netService {
	NSLog(@"Registering Bonjour Service(%@) - %@", [netService type], [netService name]);
}

// Sent if publication fails
- (void)netService:(NSNetService *)netService didNotPublish:(NSDictionary *)errorDict {
    NSLog(@"An error occurred with service %@.%@.%@, error code = %@",		  
		  [netService name], [netService type], [netService domain], [errorDict objectForKey:NSNetServicesErrorCode]);
}

// Sent when the service stops
- (void)netServiceDidStop:(NSNetService *)netService {	
	NSLog(@"Disabling Bonjour Service - %@", [netService name]);
    // You may want to do something here, such as updating a user interfac
}


@end

#include <netdb.h>

@implementation NSProcessInfo (VNCExtension)

- (CGDirectDisplayID) CGMainDisplayID {
	return CGMainDisplayID();
}

- (struct hostent *) getHostByName:(char *) host {
	return gethostbyname(host);
}

@end

