//
//  VNCController.m
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Fri Aug 02 2002.
//  Copyright (c) 2002 Redstone Software Inc. All rights reserved.
//
/*
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
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

@implementation VNCController

static void rfbShutdownOnSignal(int signal) {
    NSLog(@"Trapped Signal %d -- Terminating", signal);
    [NSApp terminate:NSApp];
}

- init {
    [super init];

    displayNumber = 0;
    port = 5900;

    alwaysShared = FALSE;
    neverShared = FALSE;

    userStopped = FALSE;

    signal(SIGTERM, rfbShutdownOnSignal);
    signal(SIGINT, rfbShutdownOnSignal);
    signal(SIGHUP, rfbShutdownOnSignal);
    signal(SIGQUIT, rfbShutdownOnSignal);
    signal(SIGBUS, rfbShutdownOnSignal);
    signal(SIGSEGV, rfbShutdownOnSignal);
    
    return self;
}

- (void) awakeFromNib {
    // This should keep it in the bundle, a little less conspicuous
    passwordFile = [[[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@".osxvncauth"] retain];

    [[NSFileManager defaultManager] removeFileAtPath:passwordFile handler:nil];
    [displayNameField setStringValue:[[NSProcessInfo processInfo] hostName]];
    
    [self loadUserDefaults: self];
    [window setFrameUsingName:@"Server Panel"];
    [window setFrameAutosaveName:@"Server Panel"];

    if ([startServerOnLaunchCheckbox state])
        [self startServer: self];
    
    [self showWindow]; 

    [displayNumberField selectItemAtIndex:displayNumber];
    [portField setIntValue:port];
}

// This is sent when the server's screen params change, the server can't handle this right now so we'll restart
- (void)applicationDidChangeScreenParameters:(NSNotification *)aNotification {
    [statusMessageField setStringValue:@"Screen Resolution changed - restarting server."];

    [self stopServer: self];
    [self startServer: self];
}

- (void) loadUserDefaults: sender {
    NSString *portDefault = [[NSUserDefaults standardUserDefaults] stringForKey:@"portNumber"];
    NSData *vncauth = [[NSUserDefaults standardUserDefaults] dataForKey:@"vncauth"];
    int sharingMode = [[NSUserDefaults standardUserDefaults] integerForKey:@"sharingMode"];
    
    if (portDefault) {
        port = [portDefault intValue];
        [portField setStringValue:portDefault];
        [self changePort:self];
    }

    if ([vncauth length]) {
        [vncauth writeToFile:passwordFile atomically:YES];
        [passwordField setStringValue:@"XXXXXXXX"];
    }

    if ([[NSUserDefaults standardUserDefaults] stringForKey:@"desktopName"])
        [displayNameField setStringValue:[[NSUserDefaults standardUserDefaults] stringForKey:@"desktopName"]];

    [sharingMatrix selectCellAtRow:sharingMode column:0];
    [self changeSharing:self];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"swapButtons"])
        [swapMouseButtonsCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"swapButtons"]];
    
    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"startServerOnLaunch"])
        [startServerOnLaunchCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"startServerOnLaunch"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"dontDisconnectClients"])
        [dontDisconnectCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"dontDisconnectClients"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"allowDimming"])
        [allowDimmingCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"allowDimming"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"allowSleep"])
        [allowSleepCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"allowSleep"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"disableRemoteEvents"])
        [disableRemoteEventsCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"disableRemoteEvents"]];
}

- (void) saveUserDefaults: sender {
    [[NSUserDefaults standardUserDefaults] setInteger:port forKey:@"portNumber"];
    
    if ([[NSFileManager defaultManager] fileExistsAtPath:passwordFile])
        [[NSUserDefaults standardUserDefaults] setObject:[NSData dataWithContentsOfFile:passwordFile] forKey:@"vncauth"];
    else
        [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"vncauth"];
        
    if ([[displayNameField stringValue] length])
        [[NSUserDefaults standardUserDefaults] setObject:[displayNameField stringValue] forKey:@"desktopName"];

    [[NSUserDefaults standardUserDefaults] setInteger:[sharingMatrix selectedRow] forKey:@"sharingMode"];

    [[NSUserDefaults standardUserDefaults] setBool:[swapMouseButtonsCheckbox state] forKey:@"swapButtons"];

    [[NSUserDefaults standardUserDefaults] setBool:[startServerOnLaunchCheckbox state] forKey:@"startServerOnLaunch"];

    [[NSUserDefaults standardUserDefaults] setBool:[dontDisconnectCheckbox state] forKey:@"dontDisconnectClients"];

    [[NSUserDefaults standardUserDefaults] setBool:[allowDimmingCheckbox state] forKey:@"allowDimming"];

    [[NSUserDefaults standardUserDefaults] setBool:[allowSleepCheckbox state] forKey:@"allowSleep"];

    [[NSUserDefaults standardUserDefaults] setBool:[disableRemoteEventsCheckbox state] forKey:@"disableRemoteEvents"];

    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (void) startServer: sender {
    id argv;

    if (![window makeFirstResponder:window])
        [window endEditingFor:nil];

    if (argv = [self formCommandLine]) {
        NSString *executionPath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"OSXvnc-server"];
        NSString *outputPath = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"server.txt"];

        [[NSFileManager defaultManager] removeFileAtPath:outputPath handler:nil];
        [[NSFileManager defaultManager] createFileAtPath:outputPath contents:nil attributes:nil];
        controller = [[NSTask alloc] init];
        //serverOutput = [NSFileHandle fileHandleForUpdatingAtPath:outputPath];
        [controller setLaunchPath:executionPath];
        [controller setArguments:argv];
        //[controller setStandardOutput:[NSFileHandle fileHandleForUpdatingAtPath:outputPath]];
        [controller setStandardError:[NSFileHandle fileHandleForUpdatingAtPath:outputPath]];
        [controller launch];

        [[NSNotificationCenter defaultCenter] addObserver: self
                                                 selector: NSSelectorFromString(@"serverStopped:")
                                                     name: NSTaskDidTerminateNotification
                                                   object: controller];
        [statusMessageField setStringValue:@"Server Running"];
        [startServerButton setEnabled:FALSE];
        [stopServerButton setEnabled:TRUE];
        userStopped = FALSE;

        // Make it clear that changing options while the server is running is not going to do anything.
        [self disableEverything];
    }
}

- (void) stopServer: sender {
    if (controller != nil) {
        userStopped = TRUE;
        [controller terminate];
    }
    else {
        [statusMessageField setStringValue:@"The server is not running."];
    }
}

- (void) serverStopped: (NSNotification *) aNotification {
    [[NSNotificationCenter defaultCenter] removeObserver: self
                                                    name: NSTaskDidTerminateNotification
                                                  object: controller];

    [startServerButton setEnabled:TRUE];
    [stopServerButton setEnabled:FALSE];

    [self enableEverything];

    
    
    if (userStopped)
        [statusMessageField setStringValue:@"The server is stopped."];
    else if ([controller terminationStatus]) {
        /* GRRRR- it's not working
        NSString *outputPath = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"server.txt"];
        NSData *lastOutput;
        int offset;

        serverOutput = [NSFileHandle fileHandleForUpdatingAtPath:outputPath];
        [serverOutput seekToEndOfFile];
        offset = MAX(0, [serverOutput offsetInFile] - 200);
        [serverOutput seekToFileOffset:offset];
        lastOutput = [serverOutput readDataToEndOfFile];

        [serverOutput closeFile];
        */
        [statusMessageField setStringValue:[NSString stringWithFormat:@"The server has stopped running: (%d)\n", [controller terminationStatus]]];
    }
    else
        [statusMessageField setStringValue:@"The server has stopped running"];

    
    [controller release];
    [serverOutput release];
    controller = nil;
    serverOutput = nil;
}

- (NSArray *) formCommandLine {
    NSMutableArray *argv = [NSMutableArray array];

    if (![[portField stringValue] length]) {
        [statusMessageField setStringValue:@"Need a valid Port or Display Number"];
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

    if ([swapMouseButtonsCheckbox state])
        [argv addObject:@"-swapButtons"];
    if ([disableRemoteEventsCheckbox state])
        [argv addObject:@"-disableRemoteEvents"];

    if ([[NSFileManager defaultManager] fileExistsAtPath:passwordFile]) {
        [argv addObject:@"-rfbauth"];
        [argv addObject:passwordFile];
    }
    
    return argv;
}

- (void) changeDisplayNumber: sender {
    displayNumber = [displayNumberField indexOfSelectedItem];

    if (displayNumber < 10) {
        port = displayNumber + 5900;
        [portField setIntValue:port];
    }

    [self saveUserDefaults: self];
}

- (void) changePort: sender {
    port = [portField intValue];
    displayNumber = port - 5900;
    if (displayNumber < 0 || displayNumber > 9) {
        displayNumber = 10;
    }
    [displayNumberField selectItemAtIndex:displayNumber];

    if (sender != self)
        [self saveUserDefaults: self];
}

- (void) changeSharing: sender {
    int selected = [sharingMatrix selectedRow];
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

    if (sender != self)
        [self saveUserDefaults: self];
}

- (void) changePassword: sender {
    [[NSFileManager defaultManager] removeFileAtPath:passwordFile handler:nil];

    if ([[passwordField stringValue] length]) {
        if (vncEncryptAndStorePasswd((char *)[[passwordField stringValue] cString], (char *)[passwordFile cString]) != 0) {
            [statusMessageField setStringValue:@"Problem - Unable to store password."];
            [passwordField setStringValue:nil];
        }
    }

    [self saveUserDefaults: self];
}

- (void) disableEverything {
    [displayNumberField setEnabled:FALSE];
    [portField setEnabled:FALSE];
    [passwordField setEnabled:FALSE];
    [displayNameField setEnabled:FALSE];
    [sharingMatrix setEnabled:FALSE];
    [dontDisconnectCheckbox setEnabled:FALSE];
    [allowDimmingCheckbox setEnabled:FALSE];
    [allowSleepCheckbox setEnabled:FALSE];
    [disableRemoteEventsCheckbox setEnabled:FALSE];
    [swapMouseButtonsCheckbox setEnabled:FALSE];
}

- (void) enableEverything {
    [displayNumberField setEnabled:TRUE];
    [portField setEnabled:TRUE];
    [passwordField setEnabled:TRUE];
    [displayNameField setEnabled:TRUE];
    [sharingMatrix setEnabled:TRUE];
    if (!alwaysShared) {
        [dontDisconnectCheckbox setEnabled:TRUE];
    }
    [allowDimmingCheckbox setEnabled:TRUE];
    [allowSleepCheckbox setEnabled:TRUE];
    [disableRemoteEventsCheckbox setEnabled:TRUE];
    [swapMouseButtonsCheckbox setEnabled:TRUE];
}

- (void) applicationWillTerminate: (NSNotification *) notification {
    [self stopServer: self];
    [window endEditingFor: nil];

    [self saveUserDefaults:self];
}

- (void) showWindow {
    [hideOrShowWindowMenuItem setTitle:@"Hide Window"];
    [window makeKeyAndOrderFront: self];
}

- (void) hideWindow {
    [hideOrShowWindowMenuItem setTitle:@"Show Window"];
    [window orderOut: self];
}

- (void) hideOrShowWindow: sender {
    // Make the window visible.
    if (![window isVisible])
        [self showWindow];
    else
        [self hideWindow];
}

// Make the window not visible, but don't actually close it.
- (BOOL) windowShouldClose: sender {
    [self hideWindow];
    return FALSE;
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

- (IBAction) openFile:(id) sender {
    NSString *openPath = [[NSBundle mainBundle] pathForResource:[sender title] ofType:@"rtf"];

    if (!openPath) {
        openPath = [[NSBundle mainBundle] pathForResource:[sender title] ofType:@"pdf"];
    }
    if (!openPath) {
        openPath = [[NSBundle mainBundle] pathForResource:[sender title] ofType:nil];
    }
    if (!openPath) {
        openPath = [[NSBundle mainBundle] pathForResource:[sender title] ofType:@"txt"];
    }

    [[NSWorkspace sharedWorkspace] openFile:openPath];
}

- (void) dealloc {
    [passwordFile release];
}

@end
