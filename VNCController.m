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

#import "OSXvnc-server/libvncauth/vncauth.h"
#import <signal.h>

#define LocalizedString(X)      [[NSBundle mainBundle] localizedStringForKey:(X) value:nil table:nil]

@implementation VNCController

static void terminateOnSignal(int signal) {
    NSLog(@"Trapped Signal %d -- Terminating", signal);
    [NSApp terminate:NSApp];
}

- init {
    [super init];

    [[NSUserDefaults standardUserDefaults] registerDefaults: [NSDictionary dictionaryWithObjectsAndKeys:
        @"", @"PasswordFile",
        @"", @"LogFile",
		[NSNumber numberWithBool:TRUE], @"allowRendezvous",
        nil]];
    
    port = 5900;

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
        
    return self;
}

- (BOOL) canWriteToFile: (NSString *) path {
    if ([[NSFileManager defaultManager] fileExistsAtPath:path])
        return [[NSFileManager defaultManager] isWritableFileAtPath:path];
    else
        return [[NSFileManager defaultManager] isWritableFileAtPath:[path stringByDeletingLastPathComponent]];
}

- (void) awakeFromNib {
    id infoDictionary = [[NSBundle mainBundle] infoDictionary];
    NSArray *passwordFiles = [NSArray arrayWithObjects:
        [[NSUserDefaults standardUserDefaults] stringForKey:@"PasswordFile"],
        [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@".osxvncauth"],
		@"~/.osxvncauth",
        @"/tmp/.osxvncauth",
        nil];
    NSEnumerator *passwordEnumerators = [passwordFiles objectEnumerator];
    NSArray *logFiles = [NSArray arrayWithObjects:
        [[NSUserDefaults standardUserDefaults] stringForKey:@"LogFile"],
        @"/var/log/OSXvnc-server.log",
		@"~/Library/Logs/OSXvnc-server.log",
        [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"OSXvnc-server.log"],
        @"/tmp/OSXvnc-server.log",
        nil];
    NSEnumerator *logEnumerators = [logFiles objectEnumerator];

    // Find first writable location for the password file
    while (passwordFile = [passwordEnumerators nextObject]) {
        passwordFile = [passwordFile stringByStandardizingPath];
        if ([passwordFile length] && [self canWriteToFile:passwordFile]) {
            [passwordFile retain];
            break;
        }
    }

    // Find first writable location for the log file
    while (logFile = [logEnumerators nextObject]) {
        logFile = [logFile stringByStandardizingPath];
        if ([logFile length] && [self canWriteToFile:logFile]) {
            [logFile retain];
            break;
        }
    }
    
    [displayNameField setStringValue:[[NSProcessInfo processInfo] hostName]];

    [self loadUserDefaults: self];

    [window setTitle:[NSString stringWithFormat:@"%@ (%@)",
        [infoDictionary objectForKey:@"CFBundleName"],
        [infoDictionary objectForKey:@"CFBundleShortVersionString"]]];
    
    [window setFrameUsingName:@"Server Panel"];
    [window setFrameAutosaveName:@"Server Panel"];

    [portField setIntValue:port];

    [optionsTabView selectTabViewItemAtIndex:0];

    [disableStartupButton setEnabled:[[NSFileManager defaultManager] fileExistsAtPath:@"/Library/StartupItems/OSXvnc"]];
    
    // Display Host Names
    {
        NSMutableArray *commonHostNames = [[[NSHost currentHost] names] mutableCopy];
        
        [commonHostNames removeObject:@"localhost"];
        
        if ([commonHostNames count] > 1) {
            [hostNamesLabel setStringValue:LocalizedString(@"Host Names:")];
        }
        [hostNamesField setStringValue:[commonHostNames componentsJoinedByString:@"\n"]];        
    }
    // Display IP Info
    {
        NSCharacterSet *ipv6Chars = [NSCharacterSet characterSetWithCharactersInString:@"ABCDEFabcdef:"];
        NSMutableArray *commonIPAddresses = [[[NSHost currentHost] addresses] mutableCopy];
        NSEnumerator *ipEnum = nil;
        NSString *anIP = nil;

        // 10.1 didn't seem to give a value here let's try the base - that didn't work either, just duplicated it on 10.2+
        //[commonIPAddresses addObject:[[NSHost currentHost] address]];
        ipEnum = [commonIPAddresses reverseObjectEnumerator];
        
        while (anIP = [ipEnum nextObject]) {
            if ([anIP isEqualToString:@"127.0.0.1"] || [anIP rangeOfCharacterFromSet:ipv6Chars].location != NSNotFound)
                [commonIPAddresses removeObject:anIP];
        }
        
        if ([commonIPAddresses count] > 1) {
            [ipAddressesLabel setStringValue:LocalizedString(@"IP Addresses:")];
        }
        [ipAddressesField setStringValue:[commonIPAddresses componentsJoinedByString:@"\n"]];        
    }
        
    if ([startServerOnLaunchCheckbox state])
        [self startServer: self];
}

- (void)applicationDidBecomeActive:(NSNotification *)aNotification {
    [window makeKeyAndOrderFront:self];
}

// This is sent when the server's screen params change, the server can't handle this right now so we'll restart
- (void)applicationDidChangeScreenParameters:(NSNotification *)aNotification {
    [statusMessageField setStringValue:LocalizedString(@"Screen Resolution changed - Server Reinitialized")];
}

- (void)windowWillClose:(NSNotification *)aNotification {
    [NSApp addWindowsItem:window title:[window title] filename:NO];
}

- (void) loadUserDefaults: sender {
    NSString *portDefault = [[NSUserDefaults standardUserDefaults] stringForKey:@"portNumber"];
    NSData *vncauth = [[NSUserDefaults standardUserDefaults] dataForKey:@"vncauth"];
    int sharingMode = [[NSUserDefaults standardUserDefaults] integerForKey:@"sharingMode"];

    if (portDefault) {
        port = [portDefault intValue];
        [portField setIntValue:port];
        if (port < 5900 || port > 5909)
            [displayNumberField selectItemAtIndex:10];
        else
            [displayNumberField selectItemAtIndex:port-5900];
    }

    if (passwordFile && [vncauth length]) {
        [vncauth writeToFile:passwordFile atomically:YES];
        [passwordField setStringValue:@"********"];
    }

    if ([[NSUserDefaults standardUserDefaults] stringForKey:@"desktopName"])
        [displayNameField setStringValue:[[NSUserDefaults standardUserDefaults] stringForKey:@"desktopName"]];
    else
        [displayNameField setStringValue:[[NSProcessInfo processInfo] hostName]];
    
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

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"allowDimming"])
        [allowDimmingCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"allowDimming"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"allowSleep"])
        [allowSleepCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"allowSleep"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"allowKeyboardLoading"]) {
        [allowKeyboardLoading setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"allowKeyboardLoading"]];
        [allowPressModsForKeys setEnabled:[allowKeyboardLoading state]];
    }

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"allowPressModsForKeys"]) 
        [allowPressModsForKeys setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"allowPressModsForKeys"]];        
    
    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"disableRemoteEvents"])
        [disableRemoteEventsCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"disableRemoteEvents"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"showMouse"])
        [showMouseButton setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"showMouse"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"localhostOnly"])
        [limitToLocalConnections setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"localhostOnly"]];
	
    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"allowRendezvous"])
        [allowRendezvousCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"allowRendezvous"]];
}

- (void) saveUserDefaults: sender {
    [[NSUserDefaults standardUserDefaults] setInteger:port forKey:@"portNumber"];

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
    [[NSUserDefaults standardUserDefaults] setBool:[limitToLocalConnections state] forKey:@"localhostOnly"];
    [[NSUserDefaults standardUserDefaults] setBool:[allowRendezvousCheckbox state] forKey:@"allowRendezvous"];
	
    [[NSUserDefaults standardUserDefaults] setBool:[startServerOnLaunchCheckbox state] forKey:@"startServerOnLaunch"];
    [[NSUserDefaults standardUserDefaults] setBool:[startServerOnLaunchCheckbox state] forKey:@"terminateOnFastUserSwitch"];
    [[NSUserDefaults standardUserDefaults] setBool:[serverKeepAliveCheckbox state] forKey:@"serverKeepAlive"];

    [[NSUserDefaults standardUserDefaults] setBool:[allowDimmingCheckbox state] forKey:@"allowDimming"];
    [[NSUserDefaults standardUserDefaults] setBool:[allowSleepCheckbox state] forKey:@"allowSleep"];

    [[NSUserDefaults standardUserDefaults] setBool:[allowKeyboardLoading state] forKey:@"allowKeyboardLoading"];
    [[NSUserDefaults standardUserDefaults] setBool:[allowPressModsForKeys state] forKey:@"allowPressModsForKeys"];
    
    [[NSUserDefaults standardUserDefaults] setBool:[showMouseButton state] forKey:@"showMouse"];

    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (void) startServer: sender {
    id argv;

    if (controller) {
        // Set to relaunch and then try to shut-down
        relaunchServer = TRUE;
        [self stopServer: self];
        return;
    }

    if (![window makeFirstResponder:window])
        [window endEditingFor:nil];

    if (argv = [self formCommandLine]) {
        NSDictionary *infoDictionary = [[NSBundle mainBundle] infoDictionary];

        NSString *executionPath = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"OSXvnc-server"];
        NSString *noteStartup = [NSString stringWithFormat:@"%@\tStarting OSXvnc Version %@\n", [NSDate date], [infoDictionary valueForKey:@"CFBundleShortVersionString"]];

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
        [statusMessageField setStringValue:LocalizedString(@"Server Running")];
        [startServerButton setEnabled:FALSE];
        [stopServerButton setEnabled:TRUE];
        [startServerButton setKeyEquivalent:@""];
        [stopServerButton setKeyEquivalent:@"\r"];
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
    [startServerButton setEnabled:TRUE];
    [stopServerButton setEnabled:FALSE];
    [stopServerButton setKeyEquivalent:@""];
    [startServerButton setKeyEquivalent:@"\r"];

    if (userStopped)
        [statusMessageField setStringValue:LocalizedString(@"The server is stopped.")];
    else if ([controller terminationStatus]==250) {
		NSMutableString *messageString = [NSMutableString stringWithFormat:@"OSXvnc can't listen on the specified port (%d).\n", port];
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

- (NSArray *) formCommandLine {
    NSMutableArray *argv = [NSMutableArray array];

    if (![[portField stringValue] length]) {
        [statusMessageField setStringValue:LocalizedString(@"Need a valid Port or Display Number")];
        return nil;
    }

    [argv addObject:@"-rfbport"];
    [argv addObject:[portField stringValue]];
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
    if (![allowDimmingCheckbox state])
        [argv addObject:@"-nodimming"];
    if ([allowSleepCheckbox state])
        [argv addObject:@"-allowsleep"];

    [argv addObject:@"-restartonuserswitch"];
    [argv addObject:([terminateOnFastUserSwitch state] ? @"Y" : @"N")];

    if ([showMouseButton state])
        [argv addObject:@"-rfbLocalBuffer"];

    [argv addObject:@"-keyboardLoading"];
    [argv addObject:([allowKeyboardLoading state] ? @"Y" : @"N")];
    [argv addObject:@"-pressModsForKeys"];
    [argv addObject:([allowPressModsForKeys state] ? @"Y" : @"N")];

    if ([swapMouseButtonsCheckbox state])
        [argv addObject:@"-swapButtons"];
    if ([disableRemoteEventsCheckbox state])
        [argv addObject:@"-disableRemoteEvents"];
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

    return argv;
}


- (void) changeDisplayNumber: sender {
    if (port != [displayNumberField indexOfSelectedItem] + 5900) {
        if ([displayNumberField indexOfSelectedItem] < 10) {
            port = [displayNumberField indexOfSelectedItem] + 5900;
            [portField setIntValue:port];
        }

        if (sender != self) {
            [self saveUserDefaults: self];
            [self checkForRestart];
        }
    }
}

- (void) changePort: sender {
    if (port != [portField intValue]) {
        port = [portField intValue];
        if (port < 5900 || port > 5909)
            [displayNumberField selectItemAtIndex:10];
        else
            [displayNumberField selectItemAtIndex:port-5900];

        if (sender != self) {
            [self saveUserDefaults: self];
            [self checkForRestart];
        }
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
    [allowPressModsForKeys setEnabled:[allowKeyboardLoading state]];
    if (sender != self) {
        [self saveUserDefaults: sender];
        [self checkForRestart];
    }
}

- (void) checkForRestart {
    if (controller) {
        [statusMessageField setStringValue:LocalizedString(@"Server Running -\n   Option Change Requires a Restart")];
        [startupItemStatusMessageField setStringValue:@""];

        [startServerButton setTitle:LocalizedString(@"Restart Server")];
        [startServerButton setEnabled:TRUE];
        [stopServerButton setKeyEquivalent:@""];
        [startServerButton setKeyEquivalent:@"\r"];
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

- (IBAction) installAsService: sender {
    // In the future we may not always overwrite (look at Version # or something)
    BOOL overwrite = TRUE;
    NSMutableString *startupScript = nil;
    NSRange lineRange;

    if (!myAuthorization)
        myAuthorization = [[NSAuthorization alloc] init];
    
    if (!myAuthorization) {
        [startupItemStatusMessageField setStringValue:LocalizedString(@"Error: No Authorization")];
        return;
    }
    
    // If StartupItems directory doesn't exist then create it
    if (![[NSFileManager defaultManager] fileExistsAtPath:@"/Library/StartupItems"]) {
		BOOL success = TRUE;
		
		success &= [myAuthorization executeCommand:@"/bin/mkdir" 
										  withArgs:[NSArray arrayWithObjects:@"-p", @"/Library/StartupItems", nil]];
        success &= [myAuthorization executeCommand:@"/usr/sbin/chown" 
										  withArgs:[NSArray arrayWithObjects:@"-R", @"root", @"/Library/StartupItems", nil]];
        if (!success) {
            [startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unable to setup StartupItems folder")];
            return;
        }
    }
        
    // If we are overwriting or if the OSXvnc folder doesn't exist
    if (overwrite || ![[NSFileManager defaultManager] fileExistsAtPath:@"/Library/StartupItems/OSXvnc"]) {
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
		
		[copyArgsArray removeAllObjects];
		[copyArgsArray addObject:@"-R"]; // Recursive
        [copyArgsArray addObject:@"-f"]; // Force Copy (overwrite existing)
        [copyArgsArray addObject:[[[[[NSProcessInfo processInfo] arguments] objectAtIndex:0] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"OSXvnc-server"]];
        //[copyArgsArray addObject:[[NSBundle mainBundle] pathForResource:@"OSXvnc-server" ofType:nil]];
        [copyArgsArray addObject:@"/Library/StartupItems/OSXvnc"];
		
        if (![myAuthorization executeCommand:@"/bin/cp" withArgs:copyArgsArray]) {
            [startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unable to copy OSXvnc-server executable")];
            return;
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
        NSMutableString *replaceString = [NSString stringWithFormat:@"VNCPATH=\"/Library/StartupItems/OSXvnc\"\n"];        
        [startupScript replaceCharactersInRange:lineRange withString:replaceString];
    }
	
	// Replace the VNCARGS line
    lineRange = [startupScript lineRangeForRange:[startupScript rangeOfString:@"VNCARGS="]];
    if (lineRange.location != NSNotFound) {
		NSData *vncauth = [[NSUserDefaults standardUserDefaults] dataForKey:@"vncauth"];
        NSMutableString *replaceString = [NSString stringWithFormat:@"VNCARGS=\"%@\"\n",[[self formCommandLine] componentsJoinedByString:@" "]];
			
		if ([vncauth length]) {
			NSString *oldPasswordFile = passwordFile;
			NSArray *mvArguments = [NSArray arrayWithObjects:@"-f", @"/tmp/.osxvncauth", @"/Library/StartupItems/OSXvnc/.osxvncauth", nil];

			[vncauth writeToFile:@"/tmp/.osxvncauth" atomically:YES];
			if (![myAuthorization executeCommand:@"/bin/mv" withArgs:mvArguments]) {
				[startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unable To Setup Password File")];
				return;
			}
			passwordFile = @"/Library/StartupItems/OSXvnc/.osxvncauth";
			replaceString = [NSString stringWithFormat:@"VNCARGS=\"%@\"\n",[[self formCommandLine] componentsJoinedByString:@" "]];
			passwordFile = oldPasswordFile;
		}
		
        [startupScript replaceCharactersInRange:lineRange withString:replaceString];
    }
    if ([startupScript writeToFile:@"/tmp/OSXvnc.script" atomically:YES]) {
		BOOL success = TRUE;
        NSArray *mvArguments = [NSArray arrayWithObjects:@"-f", @"/tmp/OSXvnc.script", @"/Library/StartupItems/OSXvnc/OSXvnc", nil];
		
		success &= [myAuthorization executeCommand:@"/bin/mv" withArgs:mvArguments];
        success &= [myAuthorization executeCommand:@"/usr/sbin/chown" 
										  withArgs:[NSArray arrayWithObjects:@"-R", @"root", @"/Library/StartupItems/OSXvnc", nil]];
		success &= [myAuthorization executeCommand:@"/usr/bin/chgrp" 
										  withArgs:[NSArray arrayWithObjects:@"0", @"/Library/StartupItems", nil]];
        success &= [myAuthorization executeCommand:@"/usr/bin/chgrp" 
										  withArgs:[NSArray arrayWithObjects:@"-R", @"0", @"/Library/StartupItems/OSXvnc", nil]];
        success &= [myAuthorization executeCommand:@"/bin/chmod" 
										  withArgs:[NSArray arrayWithObjects:@"-R", @"744", @"/Library/StartupItems/OSXvnc", nil]];
		success &= [myAuthorization executeCommand:@"/bin/chmod" 
										  withArgs:[NSArray arrayWithObjects:@"755", @"/Library/StartupItems/OSXvnc", nil]];
		
		if (!success) {
			[startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unable To Replace OSXvnc Script File")];
			return;
		}

		[myAuthorization executeCommand:@"/Library/StartupItems/OSXvnc/OSXvnc" 
							   withArgs:[NSArray arrayWithObjects:@"restart", nil]];

		/* SystemStarter needs to actually be run as root (and calling sudo requests a password again, so this doesn't work 
		[myAuthorization executeCommand:@"/usr/bin/sudo"
							   withArgs:[NSArray arrayWithObjects:@"SystemStarter",@"start", @"VNC", nil]];
		 */
    }
    else {
        [startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unable To Write out Temporary Script File")];
        return;
    }
    
    [startupItemStatusMessageField setStringValue:LocalizedString(@"Startup Item Configured (Started)")];
    [disableStartupButton setEnabled:YES];
}

- (IBAction) removeService: sender {
	BOOL success = TRUE;

    if (!myAuthorization)
        myAuthorization = [[NSAuthorization alloc] init];
    
    if (!myAuthorization) {
        [startupItemStatusMessageField setStringValue:LocalizedString(@"Error: No Authorization")];
        return;
    }

	success &= [myAuthorization executeCommand:@"/Library/StartupItems/OSXvnc/OSXvnc" 
									  withArgs:[NSArray arrayWithObjects:@"stop", nil]];
	success &= [myAuthorization executeCommand:@"/bin/rm" 
									  withArgs:[NSArray arrayWithObjects:@"-r", @"-f", @"/Library/StartupItems/OSXvnc", nil]];
    if (success) {
        [startupItemStatusMessageField setStringValue:LocalizedString(@"Startup Item Disabled (Stopped)")];
        [disableStartupButton setEnabled:NO];
    }
    else {
        [startupItemStatusMessageField setStringValue:LocalizedString(@"Error: Unabled to remove startup item")];
    }    
}

- (void) dealloc {
    [passwordFile release];
    [logFile release];

    [super dealloc];
}

@end
