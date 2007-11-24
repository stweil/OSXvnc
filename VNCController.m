/*
 *  VNCController.m
 *  OSXvnc
 *
 *  Created by Jonathan Gillaspie on Fri Aug 02 2002.  osxvnc@redstonesoftware.com
 *  Copyright (c) 2002-2005 Redstone Software Inc. All rights reserved.
 *
 *  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#import "VNCController.h"

#import "OSXvnc-server/vncauth.h"
#import <signal.h>
#import <unistd.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <fcntl.h>
#import <sys/types.h>
#import <ifaddrs.h>
#include <netdb.h>


#define LocalizedString(X)      [[NSBundle mainBundle] localizedStringForKey:(X) value:nil table:nil]

#import "RFBBundleProtocol.h"

// So we can still build on Panther and below
#ifndef NSAppKitVersionNumber10_3
#define NSAppKitVersionNumber10_3 743
#endif
#ifndef NSAppKitVersionNumber10_4
#define NSAppKitVersionNumber10_4 824
#endif

@interface NSFileManager (VNCExtensions)

- (BOOL) directoryExistsAtPath: (NSString *) path;
- (BOOL) createFullDirectoryAtPath:(NSString *)path attributes:(NSDictionary *)attributes;
- (BOOL) canWriteToFile: (NSString *) path;

@end

@implementation NSFileManager (VNCExtensions)

- (BOOL) directoryExistsAtPath: (NSString *) path {
    BOOL isDirectory = NO;
	
    return ([self fileExistsAtPath:path isDirectory: &isDirectory] && isDirectory);
}

- (BOOL) createFullDirectoryAtPath:(NSString *)path attributes:(NSDictionary *)attributes {
    if ([self directoryExistsAtPath: path])
        return YES;
	
    if ([path length] && [self createFullDirectoryAtPath:[path stringByDeletingLastPathComponent] attributes:attributes])
        return [self createDirectoryAtPath:path attributes:attributes];
    
    return NO;
}

- (BOOL) canWriteToFile: (NSString *) path {
    if ([[NSFileManager defaultManager] fileExistsAtPath:path])
        return [[NSFileManager defaultManager] isWritableFileAtPath:path];
    else {
		[[NSFileManager defaultManager] createFullDirectoryAtPath:[path stringByDeletingLastPathComponent] attributes:nil];
        return [[NSFileManager defaultManager] isWritableFileAtPath:[path stringByDeletingLastPathComponent]];
	}
}

@end

@implementation VNCController

static void terminateOnSignal(int signal) {
    NSLog(@"Trapped Signal %d -- Terminating", signal);
    [NSApp terminate:NSApp];
}

NSMutableString *hostNameString() {
	char hostName[256];
	gethostname(hostName, 256);
	
	NSMutableString *hostNameString = [NSMutableString stringWithUTF8String:hostName];
	if ([hostNameString hasSuffix:@".local"])
		[hostNameString deleteCharactersInRange:NSMakeRange([hostNameString length]-6,6)];
	
	return hostNameString;
}

NSMutableArray *localIPAddresses() {
	NSMutableArray *returnArray = [NSMutableArray array];
	struct ifaddrs *ifa = NULL, *ifp = NULL;
	
	if (getifaddrs (&ifp) < 0) {
		return nil;
	}
	
	for (ifa = ifp; ifa; ifa = ifa->ifa_next) {
		char ipString[256];
		socklen_t salen;
		
		if (ifa->ifa_addr->sa_family == AF_INET)
            salen = sizeof (struct sockaddr_in);
		else if (ifa->ifa_addr->sa_family == AF_INET6)
            salen = sizeof (struct sockaddr_in6);
		else
            continue;
		
		if (getnameinfo (ifa->ifa_addr, salen, ipString, sizeof (ipString), NULL, 0, NI_NUMERICHOST) == 0) {
			[returnArray addObject:[NSString stringWithUTF8String:ipString]];
		}
	}
	
	freeifaddrs (ifp);	
	return returnArray;
}

- init {	
    [super init];

	// Transform the GUI into a "ForegroundApp" with Dock Icon and Menu
	// This is so the server can run without a UI
	// 10.3+ only
	ProcessSerialNumber psn = { 0, kCurrentProcess }; 
	OSStatus returnCode = TransformProcessType(& psn, kProcessTransformToForegroundApplication);
	//returnCode = SetFrontProcess(& psn );
	if( returnCode != 0) {
		NSLog(@"Could not bring the application to front. Error %d", returnCode);
	}
	
	// Use old preferences found in OSXvnc
	[[NSUserDefaults standardUserDefaults] addSuiteNamed:@"OSXvnc"];
	
    [[NSUserDefaults standardUserDefaults] registerDefaults: [NSDictionary dictionaryWithObjectsAndKeys:
		@"YES", @"startServerOnLaunch",
		@"NO", @"terminateOnFastUserSwitch",
		@"YES", @"serverKeepAlive",
		@"YES", @"allowDimming",
		@"YES", @"allowScreenSaver",
        @"", @"PasswordFile",
        @"", @"LogFile",
		[NSNumber numberWithInt:0], @"keyboardLayout",
		[NSNumber numberWithInt:3], @"keyboardEvents",
		[NSNumber numberWithBool:TRUE], @"allowRendezvous",
		@"/Library/StartupItems/OSXvnc", @"startupItemLocation",
		@"/System/Library/LaunchAgents/com.redstonesoftware.VineServer.plist", @"launchdItemLocation",
        nil]];
    
    alwaysShared = FALSE;
    neverShared = FALSE;
    userStopped = FALSE;

    signal(SIGHUP, SIG_IGN);
    signal(SIGABRT, terminateOnSignal);
    signal(SIGINT, terminateOnSignal);
    signal(SIGQUIT, terminateOnSignal);
    signal(SIGBUS, terminateOnSignal);
    signal(SIGSEGV, terminateOnSignal);
    signal(SIGTERM, terminateOnSignal);
    signal(SIGTSTP, terminateOnSignal);

	bundleArray = [[NSMutableArray alloc] init];
	[self loadDynamicBundles];
	
    return self;
}

- (void) bundlesPerformSelector: (SEL) performSel {
    NSEnumerator *bundleEnum = [bundleArray objectEnumerator];
    NSBundle *bundle = nil;
	
    while ((bundle = [bundleEnum nextObject])) {
		if ([[bundle principalClass] respondsToSelector:performSel])
			[[bundle principalClass] performSelector:performSel];
	}
}

- (void) loadDynamicBundles {
    NSBundle *osxvncBundle = [NSBundle mainBundle];
    NSString *execPath =[[NSProcessInfo processInfo] processName];
		
    NSLog(@"Main Bundle: %@", [osxvncBundle bundlePath]);
    if (!osxvncBundle) {
        // If We Launched Relative - make it absolute
        if (![execPath isAbsolutePath])
            execPath = [[[NSFileManager defaultManager] currentDirectoryPath] stringByAppendingPathComponent:execPath];
		
        execPath = [execPath stringByStandardizingPath];
        execPath = [execPath stringByResolvingSymlinksInPath];
		
        osxvncBundle = [NSBundle bundleWithPath:execPath];
        //resourcesPath = [[[resourcesPath stringByDeletingLastPathComponent] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"Resources"];
    }
	
    if (osxvncBundle) {
        NSArray *bundlePathArray = [NSBundle pathsForResourcesOfType:@"bundle" inDirectory:[osxvncBundle resourcePath]];
        NSEnumerator *bundleEnum = [bundlePathArray reverseObjectEnumerator];
        NSString *bundlePath = nil;
		
        while ((bundlePath = [bundleEnum nextObject])) {
            NSBundle *aBundle = [NSBundle bundleWithPath:bundlePath];
			
            NSLog(@"Loading Bundle %@", bundlePath);
			
            if ([aBundle load]) {
				[bundleArray addObject: aBundle];
            }
            else {
                NSLog(@"\t-Bundle Load Failed");
            }
        }
    }
    else {
        NSLog(@"No Bundles Loaded - Run %@ from inside %@.app", execPath, execPath);
    }
}

// Since this can block for a long time in certain DNS situations we will put this in a separate thread
- (void) updateHostInfo {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	[NSHost flushHostCache];

	NSHost *currentHost = [NSHost currentHost];
	NSMutableArray *commonHostNames = [[currentHost names] mutableCopy];
	//NSMutableArray *commonIPAddresses = [[currentHost addresses] mutableCopy];

	[self performSelectorOnMainThread:@selector(updateHostNames:) withObject:commonHostNames waitUntilDone:NO];
	//[self performSelectorOnMainThread:@selector(updateIPAddresses:) withObject:commonIPAddresses waitUntilDone:NO];
	
	waitingForHostInfo = FALSE;
	[pool release];
}

// Display Host Names
- (void) updateHostNames: (NSMutableArray *) commonHostNames {
	[commonHostNames removeObject:@"localhost"];

	if ([commonHostNames count] > 1) {
		[hostNamesLabel setStringValue:LocalizedString(@"Host Names:")];
		[hostNamesField setStringValue:[commonHostNames componentsJoinedByString:@"\n"]];	
	}
	else if ([commonHostNames count] == 1) {
		[hostNamesLabel setStringValue:LocalizedString(@"Host Name:")];
		[hostNamesField setStringValue:[commonHostNames componentsJoinedByString:@"\n"]];	
	}
	else {
		[hostNamesLabel setStringValue:LocalizedString(@"Host Name:")];
		[hostNamesField setStringValue:@""];
	}

}

// Display IP Info
- (void) updateIPAddresses: (NSMutableArray *) commonIPAddresses {
	NSCharacterSet *ipv6Chars = [NSCharacterSet characterSetWithCharactersInString:@"ABCDEFabcdef:"];
	NSEnumerator *ipEnum = nil;
	NSString *anIP = nil;
	
	// 10.1 didn't seem to give a value here let's try the base - that didn't work either, just duplicated it on 10.2+
	//[commonIPAddresses addObject:[[NSHost currentHost] address]];
	ipEnum = [commonIPAddresses reverseObjectEnumerator];
	
	while (anIP = [ipEnum nextObject]) {
		if ([anIP isEqualToString:@"127.0.0.1"] || 
			[anIP isEqualToString:@"fe80::1"] ||
			[anIP isEqualToString:@"::1"]) { // localhost entries
			[commonIPAddresses removeObject:anIP];
		}
		else if ([anIP rangeOfCharacterFromSet:ipv6Chars].location != NSNotFound) {
			[commonIPAddresses removeObject:anIP];
			// Nobody types these in
			//[commonIPAddresses addObject:[anIP stringByAppendingString:@" (IPv6)"]];
		}
	}
	
	if ([commonIPAddresses count] > 1) {
		[ipAddressesLabel setStringValue:LocalizedString(@"IP Addresses:")];
		[ipAddressesField setStringValue:[commonIPAddresses componentsJoinedByString:@"\n"]];
	}
	else if ([commonIPAddresses count] == 1) {
		[ipAddressesLabel setStringValue:LocalizedString(@"IP Address:")];
		[ipAddressesField setStringValue:[commonIPAddresses componentsJoinedByString:@"\n"]];
	}
	else {
		[ipAddressesLabel setStringValue:LocalizedString(@"IP Address:")];
		[ipAddressesField setStringValue:@""];
	}
}

- (NSWindow *) window {
	return window;
}

- (int) port {
	return port;
}

- (void) determinePasswordLocation {
    NSArray *passwordFiles = [NSArray arrayWithObjects:
        [[NSUserDefaults standardUserDefaults] stringForKey:@"PasswordFile"],
		@"~/.osxvncauth",
        [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@".osxvncauth"],
        @"/tmp/.osxvncauth",
        nil];
    NSEnumerator *passwordEnumerators = [passwordFiles objectEnumerator];
	
	[passwordFile release];
	passwordFile = nil;
    // Find first writable location for the password file
    while (passwordFile = [passwordEnumerators nextObject]) {
        passwordFile = [passwordFile stringByStandardizingPath];
        if ([passwordFile length] && [[NSFileManager defaultManager] canWriteToFile:passwordFile]) {
            [passwordFile retain];
            break;
        }
    }
}

- (void) determineLogLocation {
	NSArray *logFiles = [NSArray arrayWithObjects:
        [[NSUserDefaults standardUserDefaults] stringForKey:@"LogFile"],
		@"~/Library/Logs/VineServer.log",
        @"/var/log/VineServer.log",
        [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"VineServer.log"],
        @"/tmp/VineServer.log",
        nil];
    NSEnumerator *logEnumerators = [logFiles objectEnumerator];
	
	[logFile release];
	logFile = nil;
    // Find first writable location for the log file
    while (logFile = [logEnumerators nextObject]) {
        logFile = [logFile stringByStandardizingPath];
        if ([logFile length] && [[NSFileManager defaultManager] canWriteToFile:logFile]) {
            [logFile retain];
            break;
        }
    }	
}

- (void) awakeFromNib {
    id infoDictionary = [[NSBundle mainBundle] infoDictionary];
	
	[self determinePasswordLocation];
	[self determineLogLocation];
	
	if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_3) {
		[connectPort setStringValue:@""];
		[[connectPort cell] performSelector:@selector(setPlaceholderString:) withObject:@"5500"];
	}
	else 
		[connectPort setIntValue:5500];
	
	[window setInitialFirstResponder: displayNameField];
		
    [self loadUserDefaults: self];

    [window setTitle:[NSString stringWithFormat:@"%@ (%@)",
        [infoDictionary objectForKey:@"CFBundleName"],
        [infoDictionary objectForKey:@"CFBundleShortVersionString"]]];
    
    [window setFrameUsingName:@"Server Panel"];
    [window setFrameAutosaveName:@"Server Panel"];

    [optionsTabView selectTabViewItemAtIndex:0];

	if ([[NSFileManager defaultManager] fileExistsAtPath:[[NSUserDefaults standardUserDefaults] stringForKey:@"startupItemLocation"]] ||
		[[NSFileManager defaultManager] fileExistsAtPath:[[NSUserDefaults standardUserDefaults] stringForKey:@"launchdItemLocation"]])
		[disableStartupButton setEnabled:TRUE];
	else
		[disableStartupButton setEnabled:FALSE];
		
	[stopServerButton setKeyEquivalent:@""];
    [startServerButton setKeyEquivalent:@"\r"];

	// First we'll update with the quick-lookup information that doesn't seem to hang
	[self updateHostNames:[NSArray arrayWithObject:hostNameString()]];
	[self updateIPAddresses:localIPAddresses()];
	
	[self bundlesPerformSelector:@selector(loadGUI)];
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification {
	[window makeMainWindow];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {	

	if ([startServerOnLaunchCheckbox state])
        [self startServer: self];
}

- (void)applicationDidBecomeActive:(NSNotification *)aNotification {
	[self updateIPAddresses:localIPAddresses()];
    [window makeKeyAndOrderFront:self];
	// These can sometimes hang so we'll do it in another thread
	if (!waitingForHostInfo) {
		waitingForHostInfo = TRUE;
		[NSThread detachNewThreadSelector:@selector(updateHostInfo) toTarget:self withObject:nil];
	}	
}

// This is sent when the server's screen params change, the server can't handle this right now so we'll restart
- (void)applicationDidChangeScreenParameters:(NSNotification *)aNotification {
    [statusMessageField setStringValue:LocalizedString(@"Screen Resolution changed - Server Reinitialized")];
}

- (void)windowWillClose:(NSNotification *)aNotification {
    [NSApp addWindowsItem:window title:[window title] filename:NO];
}

- (int) scanForOpenPort: (int) tryPort {
    int listen_fd4=0;
    int value=1;
	struct sockaddr_in sin4;	
	// I'm going to only scan on IPv4 since our OSXvnc is going to register in both spaces
	//  struct sockaddr_in6 sin6;
	// 	int listen_fd6=0;

	bzero(&sin4, sizeof(sin4));
	sin4.sin_len = sizeof(sin4);
	sin4.sin_family = AF_INET;

    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"localhostOnly"])
		sin4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    else 
		sin4.sin_addr.s_addr = htonl(INADDR_ANY);
	/*
	bzero(&sin6, sizeof(sin6));
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_family = AF_INET6;
	 if ([[NSUserDefaults standardUserDefaults] boolForKey:@"localhostOnly"])
		 sin6.sin6_addr = in6addr_loopback;
	 else 
		 sin6.sin6_addr = in6addr_any;
	 */
    
	while (tryPort < 5910) {
		sin4.sin_port = htons(tryPort);
		//sin6.sin6_port = htons(tryPort);
		
		if ((listen_fd4 = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
			//NSLog(@"Socket Init failed %d\n", tryPort);
		}
		else if (fcntl(listen_fd4, F_SETFL, O_NONBLOCK) < 0) {
			//rfbLogPerror("fcntl O_NONBLOCK failed\n");
		}
		else if (setsockopt(listen_fd4, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
			//NSLog(@"setsockopt SO_REUSEADDR failed %d\n", tryPort);
		}
		else if (bind(listen_fd4, (struct sockaddr *) &sin4, sizeof(sin4)) < 0) {
			//NSLog(@"Failed to Bind Socket: Port %d may be in use by another VNC\n", tryPort);
		}
		else if (listen(listen_fd4, 5) < 0) {
			//NSLog(@"Listen failed %d\n", tryPort);
		}
		/*
		 else if ((listen_fd6 = socket(PF_INET6, SOCK_STREAM, 0)) < 0) {
			 // NSLog(@"Socket Init 6 failed %d\n", tryPort);
		 }
		 else if (fcntl(listen_fd6, F_SETFL, O_NONBLOCK) < 0) {
			 // rfbLogPerror("IPv6: fcntl O_NONBLOCK failed\n");
		 }
		 else if (setsockopt(listen_fd6, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
			 //NSLog(@"setsockopt 6 SO_REUSEADDR failed %d\n", tryPort);
		 }
		 else if (bind(listen_fd6, (struct sockaddr *) &sin6, sizeof(sin6)) < 0) {
			 //NSLog(@"Failed to Bind Socket: Port %d may be in use by another VNC\n", tryPort);
		 }
		 else if (listen(listen_fd6, 5) < 0) {
			 //NSLog(@"Listen failed %d\n", tryPort);
		 }
		 */
		else {
			close(listen_fd4);
			//close(listen_fd6);
			if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_3) {
				[portField setStringValue:@""];
				[[portField cell] performSelector:@selector(setPlaceholderString:) withObject:[NSString stringWithFormat:@"%d",tryPort]];
			}
			else 
				[portField setIntValue:tryPort];
			
			return tryPort;
		}
		close(listen_fd4);
		//close(listen_fd6);

		tryPort++;
	}
	
	[startupItemStatusMessageField setStringValue:LocalizedString(@"Unable to find open port 5900-5909")];
	
	return 0;
}


- (int) runningPortNum {
	if (port)
		return port;
	else {
		if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_3) 
			return [[[portField cell] performSelector:@selector(placeholderString)] intValue];
		else 
			return [portField intValue];
	}
}

- (void) loadUserDefaults: sender {
    NSData *vncauth = [[NSUserDefaults standardUserDefaults] dataForKey:@"vncauth"];
    int sharingMode = [[NSUserDefaults standardUserDefaults] integerForKey:@"sharingMode"];
	
	port = [[NSUserDefaults standardUserDefaults] integerForKey:@"portNumber"];

	if (port) {
        if (port < 5900 || port > 5909)
            [displayNumberField selectItemWithTitle:@"--"];
        else
            [displayNumberField selectItemWithTitle:[NSString stringWithFormat:@"%d", port-5900]];
        [portField setIntValue:port];
    }
	else {
		[self scanForOpenPort:5900];
		[displayNumberField selectItemWithTitle:@"Auto"];
	}

    if (passwordFile && [vncauth length]) {
        [vncauth writeToFile:passwordFile atomically:YES];
        [passwordField setStringValue:@"********"];
    }

    if ([[NSUserDefaults standardUserDefaults] stringForKey:@"desktopName"])
        [displayNameField setStringValue:[[NSUserDefaults standardUserDefaults] stringForKey:@"desktopName"]];
    else {
		[displayNameField setStringValue:hostNameString()];
		//		if (NSAppKitVersionNumber > NSAppKitVersionNumber10_3) {
		//			[displayNameField setStringValue:
		//				[NSString stringWithFormat:@"%@ (%@)", NSUserName(), [[NSProcessInfo processInfo] hostName]]];
		//		}
		//		else
		//			[displayNameField setStringValue:[[NSProcessInfo processInfo] hostName]];
	}
    
    [sharingMatrix selectCellWithTag:sharingMode];
    [self changeSharing:self];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"swapButtons"])
        [swapMouseButtonsCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"swapButtons"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"startServerOnLaunch"])
        [startServerOnLaunchCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"startServerOnLaunch"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"terminateOnFastUserSwitch"])
        [terminateOnFastUserSwitch setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"terminateOnFastUserSwitch"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"serverKeepAlive"])
        [serverKeepAliveCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"serverKeepAlive"]];
    
    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"dontDisconnectClients"])
        [dontDisconnectCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"dontDisconnectClients"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"allowSleep"])
        [allowSleepCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"allowSleep"]];
	
    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"allowDimming"])
        [allowDimmingCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"allowDimming"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"allowScreenSaver"])
        [allowScreenSaverCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"allowScreenSaver"]];
	
	if ([[NSUserDefaults standardUserDefaults] objectForKey:@"protocolVersion"])
        [protocolVersion selectItemWithTitle:[[NSUserDefaults standardUserDefaults] stringForKey:@"protocolVersion"]];

	if ([[NSUserDefaults standardUserDefaults] objectForKey:@"otherArguments"])
        [otherArguments setStringValue:[[NSUserDefaults standardUserDefaults] stringForKey:@"otherArguments"]];
	
    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"allowKeyboardLoading"]) {
        [allowKeyboardLoading setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"allowKeyboardLoading"]];
        [allowPressModsForKeys setEnabled:[allowKeyboardLoading state]];
    }

	[keyboardLayout selectItemAtIndex:[keyboardLayout indexOfItemWithTag:[[NSUserDefaults standardUserDefaults] integerForKey:@"keyboardLayout"]]];
	[keyboardLayout selectItemAtIndex:[keyboardEvents indexOfItemWithTag:[[NSUserDefaults standardUserDefaults] integerForKey:@"keyboardEvents"]]];
	
    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"allowPressModsForKeys"]) 
        [allowPressModsForKeys setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"allowPressModsForKeys"]];        
    
    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"disableRemoteEvents"])
        [disableRemoteEventsCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"disableRemoteEvents"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"disableRichClipboard"])
        [disableRichClipboardCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"disableRichClipboard"]];
		
    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"showMouse"])
        [showMouseButton setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"showMouse"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"localhostOnly"])
        [limitToLocalConnections setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"localhostOnly"]];
	
    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"allowRendezvous"])
        [allowRendezvousCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"allowRendezvous"]];
}

- (void) saveUserDefaults: sender {
	if ([displayNumberField indexOfSelectedItem] == 0)
		[[NSUserDefaults standardUserDefaults] setInteger:0 forKey:@"portNumber"];
	else
		[[NSUserDefaults standardUserDefaults] setInteger:[portField intValue] forKey:@"portNumber"];

    if (passwordFile && [[NSFileManager defaultManager] fileExistsAtPath:passwordFile])
        [[NSUserDefaults standardUserDefaults] setObject:[NSData dataWithContentsOfFile:passwordFile] forKey:@"vncauth"];
    else
        [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"vncauth"];

    if ([[displayNameField stringValue] length])
        [[NSUserDefaults standardUserDefaults] setObject:[displayNameField stringValue] forKey:@"desktopName"];

    [[NSUserDefaults standardUserDefaults] setBool:[swapMouseButtonsCheckbox state] forKey:@"swapButtons"];

    [[NSUserDefaults standardUserDefaults] setInteger:[[sharingMatrix selectedCell] tag] forKey:@"sharingMode"];
    [[NSUserDefaults standardUserDefaults] setBool:[dontDisconnectCheckbox state] forKey:@"dontDisconnectClients"];
    [[NSUserDefaults standardUserDefaults] setBool:[disableRemoteEventsCheckbox state] forKey:@"disableRemoteEvents"];
    [[NSUserDefaults standardUserDefaults] setBool:[disableRichClipboardCheckbox state] forKey:@"disableRichClipboard"];
	
    [[NSUserDefaults standardUserDefaults] setBool:[limitToLocalConnections state] forKey:@"localhostOnly"];
    [[NSUserDefaults standardUserDefaults] setBool:[allowRendezvousCheckbox state] forKey:@"allowRendezvous"];
	
    [[NSUserDefaults standardUserDefaults] setBool:[startServerOnLaunchCheckbox state] forKey:@"startServerOnLaunch"];
    [[NSUserDefaults standardUserDefaults] setBool:[terminateOnFastUserSwitch state] forKey:@"terminateOnFastUserSwitch"];
    [[NSUserDefaults standardUserDefaults] setBool:[serverKeepAliveCheckbox state] forKey:@"serverKeepAlive"];

    [[NSUserDefaults standardUserDefaults] setBool:[allowSleepCheckbox state] forKey:@"allowSleep"];
    [[NSUserDefaults standardUserDefaults] setBool:[allowDimmingCheckbox state] forKey:@"allowDimming"];
    [[NSUserDefaults standardUserDefaults] setBool:[allowScreenSaverCheckbox state] forKey:@"allowScreenSaver"];

	[[NSUserDefaults standardUserDefaults] setInteger:[[keyboardLayout selectedItem] tag] forKey:@"keyboardLayout"];
	[[NSUserDefaults standardUserDefaults] setInteger:[[keyboardEvents selectedItem] tag] forKey:@"keyboardEvents"];
	
	if ([[protocolVersion titleOfSelectedItem] floatValue] > 0.0)
		[[NSUserDefaults standardUserDefaults] setFloat:[[protocolVersion titleOfSelectedItem] floatValue] forKey:@"protocolVersion"];
	else
        [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"protocolVersion"];

	if ([[otherArguments stringValue] length])
		[[NSUserDefaults standardUserDefaults] setObject:[otherArguments stringValue] forKey:@"otherArguments"];
	else
        [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"otherArguments"];
	
    [[NSUserDefaults standardUserDefaults] setBool:[allowKeyboardLoading state] forKey:@"allowKeyboardLoading"];
    [[NSUserDefaults standardUserDefaults] setBool:[allowPressModsForKeys state] forKey:@"allowPressModsForKeys"];
    
    [[NSUserDefaults standardUserDefaults] setBool:[showMouseButton state] forKey:@"showMouse"];

    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (void) startServer: sender {
    NSArray *argv;
	
    if (controller) {
        // Set to relaunch and then try to shut-down
        relaunchServer = TRUE;
        [self stopServer: self];
        return;
    }

    if (![window makeFirstResponder:window])
        [window endEditingFor:nil];

	if ([displayNumberField indexOfSelectedItem] == 0) {
		[self scanForOpenPort:5900]; // To update the UI on the likely port that we will get
	}

    if (argv = [self formCommandLine]) {
        NSDictionary *infoDictionary = [[NSBundle mainBundle] infoDictionary];

        NSString *executionPath = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"OSXvnc-server"];
        NSString *noteStartup = [NSString stringWithFormat:@"%@\tStarting %@ Version %@\n", [NSDate date], [[NSProcessInfo processInfo] processName], [infoDictionary valueForKey:@"CFBundleVersion"]];

		[self determineLogLocation];
        if (![[NSFileManager defaultManager] fileExistsAtPath:logFile]) {
            [[NSFileManager defaultManager] createFileAtPath:logFile contents:nil attributes:nil];
        }
        else { // Clear it
            serverOutput = [NSFileHandle fileHandleForUpdatingAtPath:logFile];
            [serverOutput truncateFileAtOffset:0];
            [serverOutput closeFile];
        }
        serverOutput = [[NSFileHandle fileHandleForUpdatingAtPath:logFile] retain];
        [serverOutput writeData:[noteStartup dataUsingEncoding:NSASCIIStringEncoding]];
        [serverOutput writeData:[[argv componentsJoinedByString:@" "] dataUsingEncoding:NSASCIIStringEncoding]];
        [serverOutput writeData:[@"\n\n" dataUsingEncoding:NSASCIIStringEncoding]];

        controller = [[NSTask alloc] init];
        [controller setLaunchPath:executionPath];
        [controller setArguments:argv];
        [controller setStandardOutput:serverOutput];
        [controller setStandardError:serverOutput];
        [controller launch];
        		
        [lastLaunchTime release];
        lastLaunchTime = [[NSDate date] retain];
        
        [[NSNotificationCenter defaultCenter] addObserver: self
                                                 selector: NSSelectorFromString(@"serverStopped:")
                                                     name: NSTaskDidTerminateNotification
                                                   object: controller];
		
		if (![[passwordField stringValue] length])
			[statusMessageField setStringValue:[NSString stringWithFormat:@"%@ - %@", LocalizedString(@"Server Running"), LocalizedString(@"No Authentication")]];
		else
			[statusMessageField setStringValue:LocalizedString(@"Server Running")];
        //[startServerButton setEnabled:FALSE];
        [stopServerButton setEnabled:TRUE];
		// We really don't want people to accidentally stop the server
        //[startServerButton setKeyEquivalent:@""];
        //[stopServerButton setKeyEquivalent:@"\r"];
        userStopped = FALSE;
    }
}

- (void) stopServer: sender {
    if (controller != nil) {
        userStopped = TRUE;
        [controller terminate];
    }
    else {
        [statusMessageField setStringValue:LocalizedString(@"The server is not running.")];
    }
}

- (void) serverStopped: (NSNotification *) aNotification {
    [[NSNotificationCenter defaultCenter] removeObserver: self
                                                    name: NSTaskDidTerminateNotification
                                                  object: controller];

    // If we don't get the notification soon enough, we may have already restarted
    if ([controller isRunning]) {
        return;
    }

    [startServerButton setTitle:LocalizedString(@"Start Server")];
    //[startServerButton setEnabled:TRUE];
    [stopServerButton setEnabled:FALSE];
    //[stopServerButton setKeyEquivalent:@""];
    //[startServerButton setKeyEquivalent:@"\r"];

    if (userStopped)
        [statusMessageField setStringValue:LocalizedString(@"The server is stopped.")];
    else if ([controller terminationStatus]==250) {
		NSMutableString *messageString = [NSMutableString stringWithFormat:@"%@ can't listen on the specified port (%d).\n", [[NSProcessInfo processInfo] processName], port];
		if ([disableStartupButton isEnabled])
			[messageString appendString:LocalizedString(@"Probably because the OSXvnc server is already running as a Startup Item.")];
		else
			[messageString appendString:LocalizedString(@"Probably because another VNC is already using this port.")];
		[statusMessageField setStringValue:messageString];
    }
    else if ([controller terminationStatus]) {
        [statusMessageField setStringValue:[NSString stringWithFormat:LocalizedString(@"The server has stopped running. See Log (%d)\n"), [controller terminationStatus]]];
    }
    else
        [statusMessageField setStringValue:LocalizedString(@"The server has stopped running")];

    if (!userStopped && 
        [serverKeepAliveCheckbox state] &&
        [controller terminationStatus] >= 0 && 
		[controller terminationStatus] <= 64 &&
        -[lastLaunchTime timeIntervalSinceNow] > 1.0)
        relaunchServer = YES;
    
    [controller release];
    controller = nil;
    [serverOutput closeFile];
    [serverOutput release];
    serverOutput = nil;

    // If it crashes in less than a second it probably can't launch
    if (relaunchServer) {
        relaunchServer = NO;
        [self startServer:self];
    }
}

- (NSMutableArray *) formCommandLine {
    NSMutableArray *argv = [NSMutableArray array];

	/* Now Using "AUTO Detect", the CLI is slightly better in that it loads the Jag Bundle and can also detect IPv6 ports
	 if (!port) {
		 [statusMessageField setStringValue:LocalizedString(@"Need a valid Port or Display Number")];
		 return nil;
	 }
	 */
	
    [argv addObject:@"-rfbport"];
    [argv addObject:[NSString stringWithFormat:@"%d", port]];
    if ([[displayNameField stringValue] length]) {
        [argv addObject:@"-desktop"];
        [argv addObject:[displayNameField stringValue]];
    }

    if (alwaysShared)
        [argv addObject:@"-alwaysshared"];
    if (neverShared)
        [argv addObject:@"-nevershared"];
    if ([dontDisconnectCheckbox state] && !alwaysShared)
        [argv addObject:@"-dontdisconnect"];

	if ([allowSleepCheckbox state])
        [argv addObject:@"-allowsleep"];
	if (![allowDimmingCheckbox state])
        [argv addObject:@"-nodimming"];
	if (![allowScreenSaverCheckbox state])
        [argv addObject:@"-disableScreenSaver"];

	if ([[protocolVersion titleOfSelectedItem] floatValue] > 0.0) {
		[argv addObject:@"-protocol"];
		[argv addObject:[protocolVersion titleOfSelectedItem]];
	}
		
    [argv addObject:@"-restartonuserswitch"];
    [argv addObject:([terminateOnFastUserSwitch state] ? @"Y" : @"N")];

    if ([showMouseButton state])
        [argv addObject:@"-rfbLocalBuffer"];

	switch ([[keyboardLayout selectedItem] tag]) {
		case 0:
			[argv addObject:@"-UnicodeKeyboard"];
			[argv addObject:@"1"];
			[argv addObject:@"-keyboardLoading"];
			[argv addObject:@"N"];
			[argv addObject:@"-pressModsForKeys"];
			[argv addObject:@"N"];
			break;
		case 1:
			[argv addObject:@"-UnicodeKeyboard"];
			[argv addObject:@"0"];
			[argv addObject:@"-keyboardLoading"];
			[argv addObject:@"Y"];
			[argv addObject:@"-pressModsForKeys"];
			[argv addObject:@"Y"];
			break;
		case 2:
			[argv addObject:@"-UnicodeKeyboard"];
			[argv addObject:@"0"];
			[argv addObject:@"-keyboardLoading"];
			[argv addObject:@"N"];
			[argv addObject:@"-pressModsForKeys"];
			[argv addObject:@"N"];
			break;
		default:
			[argv addObject:@"-keyboardLoading"];
			[argv addObject:([allowKeyboardLoading state] ? @"Y" : @"N")];
			[argv addObject:@"-pressModsForKeys"];
			[argv addObject:([allowPressModsForKeys state] ? @"Y" : @"N")];
			break;
	}
	[argv addObject:@"-EventTap"];
	[argv addObject:[NSString stringWithFormat:@"%d", [[keyboardEvents selectedItem] tag]]];

    if ([swapMouseButtonsCheckbox state])
        [argv addObject:@"-swapButtons"];
    if ([disableRemoteEventsCheckbox state])
        [argv addObject:@"-disableRemoteEvents"];
    if ([disableRichClipboardCheckbox state])
        [argv addObject:@"-disableRichClipboards"];
		
    if ([limitToLocalConnections state])
        [argv addObject:@"-localhost"];
	
	[argv addObject:@"-rendezvous"];
    [argv addObject:([allowRendezvousCheckbox state] ? @"Y" : @"N")];
	
    if (passwordFile && [[NSFileManager defaultManager] fileExistsAtPath:passwordFile]) {
        [argv addObject:@"-rfbauth"];
        [argv addObject:passwordFile];
    }

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"rfbDeferUpdateTime"]) {
        [argv addObject:@"-deferupdate"];
        [argv addObject:[[NSUserDefaults standardUserDefaults] stringForKey:@"rfbDeferUpdateTime"]];
    }

	if (doNotLoadProxy)
		[argv addObject:@"-donotloadproxy"];

	if ([[otherArguments stringValue] length])
		[argv addObjectsFromArray:[[otherArguments stringValue] componentsSeparatedByString:@" "]];
	
    return argv;
}


- (void) changeDisplayNumber: sender {
	if ([displayNumberField indexOfSelectedItem] == 0) {
		[self scanForOpenPort:5900]; // Even if we don't launch now we'll update with the likely port number
		port = 0;
	}
	else  if (port != [[[displayNumberField selectedItem] title] intValue] + 5900) {
        if ([displayNumberField indexOfSelectedItem] < 10) {
            port = [[[displayNumberField selectedItem] title] intValue] + 5900;
            [portField setIntValue:port];
        }
	}

	if (sender != self) {
		[self saveUserDefaults: self];
		[self checkForRestart];
	}
}

- (void) changePort: sender {
   // if (port != [portField intValue]) {
	port = [portField intValue];
	if (!port) {
		[displayNumberField selectItemWithTitle:@"Auto"];
		[self scanForOpenPort:5900];// Even if we don't launch now we'll update with the likely port number
	}
	else if (port < 5900 || port > 5909)
		[displayNumberField selectItemWithTitle:@"--"];
	else
		[displayNumberField selectItemWithTitle:[NSString stringWithFormat:@"%d", port-5900]];

	if (sender != self) {
		[self saveUserDefaults: self];
		[self checkForRestart];
	}
}

- (void) changeSharing: sender {
    int selected = [[sharingMatrix selectedCell] tag];
    if (selected == 1) {
        // Always shared.
        alwaysShared = TRUE;
        neverShared = FALSE;
        [dontDisconnectCheckbox setEnabled:NO];
    } else if (selected == 2) {
        // Never shared.
        alwaysShared = FALSE;
        neverShared = TRUE;
        [dontDisconnectCheckbox setEnabled:YES];
    } else {
        // Not always or never shared.
        alwaysShared = FALSE;
        neverShared = FALSE;
        [dontDisconnectCheckbox setEnabled:YES];
    }

    if (sender != self) {
        [self saveUserDefaults: self];
        [self checkForRestart];
    }
}

- (void) changePassword: sender {
    if ((![[passwordField stringValue] isEqualToString:@"********"] && [[passwordField stringValue] length]) ||
        (![[passwordField stringValue] length] && [[NSUserDefaults standardUserDefaults] objectForKey:@"vncauth"])) {
        [[NSFileManager defaultManager] removeFileAtPath:passwordFile handler:nil];

        if ([[passwordField stringValue] length]) {
            if (vncEncryptAndStorePasswd((char *)[[passwordField stringValue] cString], (char *)[passwordFile cString]) != 0) {
                [statusMessageField setStringValue:[NSString stringWithFormat:LocalizedString(@"Problem - Unable to store password to %@"), passwordFile]];
                [passwordField setStringValue:nil];
            }
            else
                [passwordField setStringValue:@"********"];
        }

        if (sender != self) {
            [self saveUserDefaults: self];
            [self checkForRestart];
        }
    }
}

- (IBAction) changeDisplayName: sender {
    if (![[displayNameField stringValue] isEqualToString:[[NSUserDefaults standardUserDefaults] objectForKey:@"desktopName"]] && sender != self) {
        [self saveUserDefaults: self];
        [self checkForRestart];
    }
}

- (IBAction) optionChanged: sender {
    if (sender != self) {
        [self saveUserDefaults: sender];
        [self checkForRestart];
    }
}

// This will issue a Distributed Notification to add a VNC client
- (IBAction) connectHost: sender {
	NSMutableDictionary *argumentsDict = [NSMutableDictionary dictionaryWithObjectsAndKeys:[connectHost stringValue],@"ConnectHost",[connectPort stringValue],@"ConnectPort",nil];
	
	if (![[connectHost stringValue] length]) {
		[statusMessageField setStringValue:LocalizedString(@"Please specify a Connect Host to establish a connection")];
		return;
	}
	if (![connectPort intValue]) {
		[argumentsDict setObject:@"5500" forKey:@"ConnectPort"];
	}
		
	if (!controller) {
		[self startServer: self];
		usleep(500000); // Give that server time to start
	}
	
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:@"VNCConnectHost"
																   object:[NSString stringWithFormat:@"OSXvnc%d",[self runningPortNum]]
																 userInfo:argumentsDict
													   deliverImmediately:YES];

	usleep(500000); // Give notification time to post
	
	if (kill([controller processIdentifier], SIGCONT) == 0)
		[statusMessageField setStringValue:LocalizedString(@"Connection invitation sent to Connect Host")];
	else
		[statusMessageField setStringValue:[NSString stringWithFormat:LocalizedString(@"Error sending invitation: %s"), strerror(errno)]];
}

- (void) checkForRestart {
    if (controller) {
        [statusMessageField setStringValue:LocalizedString(@"Server Running -\n   Option Change Requires a Restart")];
        [startupItemStatusMessageField setStringValue:@""];

        [startServerButton setTitle:LocalizedString(@"Restart Server")];
        //[startServerButton setEnabled:TRUE];
        //[stopServerButton setKeyEquivalent:@""];
        //[startServerButton setKeyEquivalent:@"\r"];
    }
}

- (void) applicationWillTerminate: (NSNotification *) notification {
    [self stopServer: self];
    [window endEditingFor: nil];

    [self saveUserDefaults:self];
}

- (BOOL) validateMenuItem: (NSMenuItem *) menuItem {
    // Disable the 'start server' menu item when the server is already started.
    // Disable the 'stop server' menu item when the server is not running.
    if ((menuItem == startServerMenuItem) && controller) {
        return FALSE;
    }
    else if ((menuItem == stopServerMenuItem) && (!controller)) {
        return FALSE;
    }

    return TRUE;
}

- (IBAction) openLog:(id) sender {
    [[NSWorkspace sharedWorkspace] openFile:logFile];
}

- (IBAction) openGPL:(id) sender {
    NSString *openPath = [[NSBundle mainBundle] pathForResource:@"Copying" ofType:@"rtf"];
	
    [[NSWorkspace sharedWorkspace] openFile:openPath];
}

- (IBAction) openReleaseNotes:(id) sender {
    NSString *openPath = [[NSBundle mainBundle] pathForResource:@"Release Notes" ofType:@"rtf"];
	
    [[NSWorkspace sharedWorkspace] openFile:openPath];
}

- (IBAction) openFile:(id) sender {
    NSString *openPath = [[NSBundle mainBundle] pathForResource:[sender title] ofType:@"rtf"];

    if (!openPath) {
        openPath = [[NSBundle mainBundle] pathForResource:[sender title] ofType:@"pdf"];
    }
    if (!openPath) {
        openPath = [[NSBundle mainBundle] pathForResource:[sender title] ofType:@"txt"];
    }
    if (!openPath) {
        openPath = [[NSBundle mainBundle] pathForResource:[sender title] ofType:nil];
    }
	
    [[NSWorkspace sharedWorkspace] openFile:openPath];
}

- (void) installStartupItem {
	// In the future we may not always overwrite (look at Version # or something)
    BOOL overwrite = TRUE;
    NSMutableString *startupScript = nil;
    NSRange lineRange;
	NSString *startupPath = [[NSUserDefaults standardUserDefaults] stringForKey:@"startupItemLocation"];
	NSString *startupResourcePath = [startupPath stringByAppendingPathComponent:@"Resources"];

    // If StartupItems directory doesn't exist then create it
    if (![[NSFileManager defaultManager] fileExistsAtPath:@"/Library/StartupItems"]) {
		BOOL success = TRUE;
		
		success &= [myAuthorization executeCommand:@"/bin/mkdir" 
										  withArgs:[NSArray arrayWithObjects:@"-p", @"/Library/StartupItems", nil]];
        success &= [myAuthorization executeCommand:@"/usr/sbin/chown" 
										  withArgs:[NSArray arrayWithObjects:@"-R", @"root:wheel", @"/Library/StartupItems", nil]];

        if (!success) {
            [startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unable to setup StartupItems folder")];
			return;
        }
    }
        
    // If we are overwriting or if the OSXvnc folder doesn't exist
    if (overwrite || ![[NSFileManager defaultManager] fileExistsAtPath:startupPath]) {
        NSMutableArray *copyArgsArray = [NSMutableArray array];
        NSString *sourceFolder = [[NSBundle mainBundle] pathForResource:@"OSXvnc" ofType:nil];
        
        [copyArgsArray addObject:@"-R"]; // Recursive
        [copyArgsArray addObject:@"-f"]; // Force Copy (overwrite existing)
        [copyArgsArray addObject:[[NSBundle mainBundle] pathForResource:@"OSXvnc" ofType:nil]];
        [copyArgsArray addObject:@"/Library/StartupItems"];
        
        if (![myAuthorization executeCommand:@"/bin/cp" withArgs:copyArgsArray]) {
            [startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unable to copy OSXvnc folder")];
            return;
        }
		
		// Copy Server Executable
		[copyArgsArray removeAllObjects];
		[copyArgsArray addObject:@"-R"]; // Recursive
        [copyArgsArray addObject:@"-f"]; // Force Copy (overwrite existing)
        [copyArgsArray addObject:[[[[[NSProcessInfo processInfo] arguments] objectAtIndex:0] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"OSXvnc-server"]];
        [copyArgsArray addObject:startupPath];
		
        if (![myAuthorization executeCommand:@"/bin/cp" withArgs:copyArgsArray]) {
            [startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unable to copy OSXvnc-server executable")];
            return;
        }
		
		// Copy All Bundles
		{
			NSEnumerator *bundleEnum = [[NSBundle pathsForResourcesOfType:@"bundle" inDirectory:[[NSBundle mainBundle] resourcePath]] objectEnumerator];
			NSString *bundlePath = nil;
			
			while (bundlePath = [bundleEnum nextObject]) {
				[copyArgsArray removeAllObjects];
				[copyArgsArray addObject:@"-R"]; // Recursive
				[copyArgsArray addObject:@"-f"]; // Force Copy (overwrite existing)
				[copyArgsArray addObject:bundlePath];
				//[copyArgsArray addObject:[[NSBundle mainBundle] pathForResource:@"JaguarBundle" ofType:@"bundle"]];
				[copyArgsArray addObject:startupResourcePath];

		        if (![myAuthorization executeCommand:@"/bin/cp" withArgs:copyArgsArray]) {
					[startupItemStatusMessageField setStringValue:[NSString stringWithFormat:@"Error: Unable to copy bundle:%@", [bundlePath lastPathComponent]]];
					return;
				}
			}
		}
		
        startupScript = [NSMutableString stringWithContentsOfFile:[sourceFolder stringByAppendingPathComponent:@"OSXvnc"]];
    }
    else {
        // Would be nice to always use this but there is a timing issue with the AuthorizationExecuteWithPrivileges command
        startupScript = [NSMutableString stringWithContentsOfFile:@"/Library/StartupItems/OSXvnc/OSXvnc"];
    }
    
    // Now we will modify the script file
    if (![startupScript length]) {
        [startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unable To Read in OSXvnc script File")];
        return;
    }
    
    // Replace the VNCPATH line
    lineRange = [startupScript lineRangeForRange:[startupScript rangeOfString:@"VNCPATH="]];
    if (lineRange.location != NSNotFound) {
        NSMutableString *replaceString = [NSString stringWithFormat:@"VNCPATH=\"%@\"\n", startupPath];        
        [startupScript replaceCharactersInRange:lineRange withString:replaceString];
    }
	
	// Replace the VNCARGS line
    lineRange = [startupScript lineRangeForRange:[startupScript rangeOfString:@"VNCARGS="]];
    if (lineRange.location != NSNotFound) {
		NSData *vncauth = [[NSUserDefaults standardUserDefaults] dataForKey:@"vncauth"];
        NSMutableString *replaceString = nil;
		NSString *oldPasswordFile = passwordFile;
		NSString *oldDesktopName = [displayNameField stringValue];
			
		if ([vncauth length]) {
			NSArray *mvArguments = [NSArray arrayWithObjects:@"-f", @"/tmp/.osxvncauth", @"/Library/StartupItems/OSXvnc/.osxvncauth", nil];

			[vncauth writeToFile:@"/tmp/.osxvncauth" atomically:YES];
			if (![myAuthorization executeCommand:@"/bin/mv" withArgs:mvArguments]) {
				[startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unable To Setup Password File")];
				return;
			}
			passwordFile = @"/Library/StartupItems/OSXvnc/.osxvncauth";
		}
		
		// Coerce the CommandLine string with slight modifications
		if (floor(NSAppKitVersionNumber) > floor(NSAppKitVersionNumber10_1)) {
			NSMutableString *newDesktopName = [[oldDesktopName mutableCopy] autorelease];
			[newDesktopName replaceOccurrencesOfString:@" " withString:@"_" options:nil range:NSMakeRange(0,[oldDesktopName length])];
			[displayNameField setStringValue:newDesktopName];
		}
		doNotLoadProxy = YES;
		replaceString = [NSString stringWithFormat:@"VNCARGS=\"%@\"\n",[[self formCommandLine] componentsJoinedByString:@" "]];
		doNotLoadProxy = NO;
        [startupScript replaceCharactersInRange:lineRange withString:replaceString];

		[displayNameField setStringValue:oldDesktopName];
		passwordFile = oldPasswordFile;
	}
    if ([startupScript writeToFile:@"/tmp/OSXvnc.script" atomically:YES]) {
		BOOL success = TRUE;
		
		success &= [myAuthorization executeCommand:@"/bin/mv" 
										  withArgs:[NSArray arrayWithObjects:@"-f", @"/tmp/OSXvnc.script", @"/Library/StartupItems/OSXvnc/OSXvnc", nil]];
        success &= [myAuthorization executeCommand:@"/usr/sbin/chown" 
										  withArgs:[NSArray arrayWithObjects:@"-R", @"root:wheel", startupPath, nil]];
        success &= [myAuthorization executeCommand:@"/bin/chmod" 
										  withArgs:[NSArray arrayWithObjects:@"-R", @"744", startupPath, nil]];
		success &= [myAuthorization executeCommand:@"/bin/chmod"
										  withArgs:[NSArray arrayWithObjects:@"755", startupPath, startupResourcePath, nil]];
		
		if (!success) {
			[startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unable To Replace OSXvnc Script File")];
			return;
		}

		// For 10.4 and above we need to execute through the System Starter for it to follow the console properly
		// Doesn't work
		// SystemStarter needs to actually be run as root (and calling sudo requests a password again, so this doesn't work 
		if (0 && floor(NSAppKitVersionNumber) > floor(NSAppKitVersionNumber10_3)) {
			[myAuthorization executeCommand:@"/usr/bin/su"
								   withArgs:[NSArray arrayWithObjects:@"-l", @"root", @"-c", @"/sbin/SystemStarter", @"-d", @"start", @"VNC", nil]];
		}
		else
			[myAuthorization executeCommand:@"/Library/StartupItems/OSXvnc/OSXvnc" 
								   withArgs:[NSArray arrayWithObjects:@"restart", nil]];
    }
    else {
        [startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unable To Write out Temporary Script File")];
        return;
    }
    
	if (![[passwordField stringValue] length])
		[startupItemStatusMessageField setStringValue:[NSString stringWithFormat:@"%@ - %@", LocalizedString(@"Startup Item Configured (Started)"), LocalizedString(@"No Authentication")]];
	else
		[startupItemStatusMessageField setStringValue:LocalizedString(@"Startup Item Configured (Started)")];
}

- (void) installLaunchd {
	NSMutableDictionary *launchdDictionary = [NSMutableDictionary dictionary];
	NSMutableArray *argv = [self formCommandLine];
	BOOL success = TRUE;
	NSString *launchdPath = [[NSUserDefaults standardUserDefaults] stringForKey:@"launchdItemLocation"];

	if (argv) {
		// Configure PLIST
		[argv insertObject:[[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"OSXvnc-server"] atIndex:0];
		[launchdDictionary setObject:argv forKey:@"ProgramArguments"];

		[launchdDictionary setObject:[NSNumber numberWithBool:TRUE] forKey:@"KeepAlive"];
		[launchdDictionary setObject:[NSNumber numberWithBool:TRUE] forKey:@"RunAtLoad"];
		[launchdDictionary setObject:@"VineServer" forKey:@"Label"];
		[launchdDictionary setObject:[NSArray arrayWithObjects:@"Aqua",@"LoginWindow",nil] forKey:@"LimitLoadToSessionType"];
		[launchdDictionary setObject:@"/var/log/VineServer.log" forKey:@"StandardOutputPath"];
		[launchdDictionary setObject:@"/var/log/VineServer.log" forKey:@"StandardErrorPath"];
			
		// Write to file
		NSString *tempPath = [@"/tmp" stringByAppendingPathComponent:[launchdPath lastPathComponent]];
		[launchdDictionary writeToFile:tempPath atomically:NO];
		success &= [myAuthorization executeCommand:@"/usr/sbin/chown" 
										  withArgs:[NSArray arrayWithObjects:@"-R", @"root:wheel", tempPath, nil]];
        success &= [myAuthorization executeCommand:@"/bin/chmod" 
										  withArgs:[NSArray arrayWithObjects:@"-R", @"744", tempPath, nil]];
		
		// Install to /System/LaunchAgents/com.redstonesoftware.VineServer.plist
		success &= [myAuthorization executeCommand:@"/bin/mv" 
										  withArgs:[NSArray arrayWithObjects:@"-f", tempPath, launchdPath, nil]];
		
		// Launch Using launchctl -S Aqua /System/LaunchAgents/com.redstonesoftware.VineServer.plist
		
		if (floor(NSAppKitVersionNumber) <= floor(NSAppKitVersionNumber10_4)) {
			success &= [myAuthorization executeCommand:@"/bin/launchctl" 
											  withArgs:[NSArray arrayWithObjects:@"load", launchdPath, nil]];
		}
		else {
			success &= [myAuthorization executeCommand:@"/bin/launchctl" 
											  withArgs:[NSArray arrayWithObjects:@"load", @"-S", @"Aqua", launchdPath, nil]];
		}
	}
	if (!success) {
		[startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unable To Setup Vine Server using LaunchD")];
		return;
	}
}

- (IBAction) installAsService: sender {
	// No password, so double check
	if (![[passwordField stringValue] length]) {
		int result=NSRunAlertPanel(LocalizedString(@"System Server"),LocalizedString(@"No password has been specified for the System Server.  The System Server will automatic launch every time your machine is restarted.  Are you sure that you want to install a System Server with no password"),LocalizedString(@"Cancel"),LocalizedString(@"Start Server"),nil);
		
		if (result==NSAlertDefaultReturn)
			return;
	}
	
    if (!myAuthorization)
        myAuthorization = [[NSAuthorization alloc] init];
    
    if (!myAuthorization) {
        [startupItemStatusMessageField setStringValue:LocalizedString(@"Error: No Authorization")];
        return;
    }
	
	if (floor(NSAppKitVersionNumber) <= floor(NSAppKitVersionNumber10_3))
		[self installStartupItem];
	else
		[self installLaunchd];	
	
    [disableStartupButton setEnabled:YES];

	[myAuthorization release];
	myAuthorization = nil;
}

- (IBAction) removeService: sender {
	BOOL success = TRUE;
	NSString *startupPath = [[NSUserDefaults standardUserDefaults] stringForKey:@"startupItemLocation"];
	NSString *launchdPath = [[NSUserDefaults standardUserDefaults] stringForKey:@"launchdItemLocation"];

    if (!myAuthorization)
        myAuthorization = [[NSAuthorization alloc] init];
    
    if (!myAuthorization) {
        [startupItemStatusMessageField setStringValue:LocalizedString(@"Error: No Authorization")];
        return;
    }

	if ([[NSFileManager defaultManager] fileExistsAtPath:startupPath]) {
		success &= [myAuthorization executeCommand:@"/Library/StartupItems/OSXvnc/OSXvnc" 
										  withArgs:[NSArray arrayWithObjects:@"stop", nil]];
		success &= [myAuthorization executeCommand:@"/bin/rm" 
										  withArgs:[NSArray arrayWithObjects:@"-r", @"-f", startupPath, nil]];
	}
	if ([[NSFileManager defaultManager] fileExistsAtPath:launchdPath]) {
		if (floor(NSAppKitVersionNumber) <= floor(NSAppKitVersionNumber10_4)) {
			success &= [myAuthorization executeCommand:@"/bin/launchctl" 
											  withArgs:[NSArray arrayWithObjects:@"unload", launchdPath, nil]];
		}
		else 
			success &= [myAuthorization executeCommand:@"/bin/launchctl" 
											  withArgs:[NSArray arrayWithObjects:@"unload", @"-S", @"Aqua", launchdPath, nil]];
		success &= [myAuthorization executeCommand:@"/bin/rm" 
										  withArgs:[NSArray arrayWithObjects:@"-r", @"-f", launchdPath, nil]];
	}
	
    if (success) {
        [startupItemStatusMessageField setStringValue:LocalizedString(@"Startup Item Disabled (Stopped)")];
        [disableStartupButton setEnabled:NO];
    }
    else {
        [startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unabled to remove startup item")];
    }

	[myAuthorization release];
	myAuthorization = nil;
}

- (void) dealloc {
    [passwordFile release];
    [logFile release];
	[myAuthorization release];
	myAuthorization = nil;
	[bundleArray release];
	bundleArray = nil;

    [super dealloc];
}

@end
