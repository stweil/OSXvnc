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


// Run a panel to authenticate for root privs.... but how...

// try CoreTechnologies/securityservices/authorizationservices is a directory in /Developer/Documentation says Doug

#import "VNCController.h"

#import "OSXvnc-server/libvncauth/vncauth.h"
#import <signal.h>

#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>

@implementation VNCController

static void rfbShutdownOnSignal(int signal) {
    NSLog(@"Trapped Signal %d -- Terminating", signal);
    [NSApp terminate:NSApp];
}

- init {
    [super init];

    port = 5900;

    alwaysShared = FALSE;
    neverShared = FALSE;

    userStopped = FALSE;

    signal(SIGHUP, SIG_IGN);
    signal(SIGINT, rfbShutdownOnSignal);
    signal(SIGQUIT, rfbShutdownOnSignal);
    signal(SIGBUS, rfbShutdownOnSignal);
    signal(SIGSEGV, rfbShutdownOnSignal);
    signal(SIGTERM, rfbShutdownOnSignal);
    signal(SIGTSTP, rfbShutdownOnSignal);

    return self;
}

- (void) awakeFromNib {
    // This should keep it in the bundle, a little less conspicuous
    if ([[NSFileManager defaultManager] isWritableFileAtPath:[[NSBundle mainBundle] bundlePath]])
        passwordFile = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@".osxvncauth"];
    else
        passwordFile = @"/tmp/.osxvncauth";
    [passwordFile retain];

    if ([[NSFileManager defaultManager] isWritableFileAtPath:@"/var/log"])
        logFile = [@"/var/log" stringByAppendingPathComponent:@"OSXvnc-server.log"];
    else if ([[NSFileManager defaultManager] isWritableFileAtPath:[[NSBundle mainBundle] bundlePath]])
        logFile = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"OSXvnc-server.log"];
    else
        logFile = @"/tmp/OSXvnc-server.log";
    [logFile retain];

    //[[NSFileManager defaultManager] removeFileAtPath:passwordFile handler:nil];
    [displayNameField setStringValue:[[NSProcessInfo processInfo] hostName]];

    [self loadUserDefaults: self];
    [window setFrameUsingName:@"Server Panel"];
    [window setFrameAutosaveName:@"Server Panel"];

    if ([startServerOnLaunchCheckbox state])
        [self startServer: self];

    [portField setIntValue:port];
}

// This is sent when the server's screen params change, the server can't handle this right now so we'll restart
- (void)applicationDidChangeScreenParameters:(NSNotification *)aNotification {
    [statusMessageField setStringValue:@"Screen Resolution changed - Server Reinitializing"];
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

    if ([vncauth length]) {
        [vncauth writeToFile:passwordFile atomically:YES];
        [passwordField setStringValue:@"********"];
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

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"showMouse"])
        [showMouseButton setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"showMouse"]];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"localhostOnly"])
        [limitToLocalConnections setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"localhostOnly"]];
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

    [[NSUserDefaults standardUserDefaults] setBool:[showMouseButton state] forKey:@"showMouse"];

    [[NSUserDefaults standardUserDefaults] setBool:[limitToLocalConnections state] forKey:@"localhostOnly"];

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
        NSString *noteStartup = [NSString stringWithFormat:@"%@\tStarting OSXvnc Version %@\n\n", [NSDate date], [infoDictionary valueForKey:@"CFBundleShortVersionString"]];

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

        controller = [[NSTask alloc] init];
        [controller setLaunchPath:executionPath];
        [controller setArguments:argv];
        [controller setStandardOutput:serverOutput];
        [controller setStandardError:serverOutput];
        [controller launch];

        [[NSNotificationCenter defaultCenter] addObserver: self
                                                 selector: NSSelectorFromString(@"serverStopped:")
                                                     name: NSTaskDidTerminateNotification
                                                   object: controller];
        [statusMessageField setStringValue:@"Server Running"];
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
        [statusMessageField setStringValue:@"The server is not running."];
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

    [startServerButton setTitle:@"Start Server"];
    [startServerButton setEnabled:TRUE];
    [stopServerButton setEnabled:FALSE];
    [stopServerButton setKeyEquivalent:@""];
    [startServerButton setKeyEquivalent:@"\r"];

    if (userStopped)
        [statusMessageField setStringValue:@"The server is stopped."];
    else if ([controller terminationStatus]) {
        [statusMessageField setStringValue:[NSString stringWithFormat:@"The server has stopped running. See Log (%d)\n", [controller terminationStatus]]];
    }
    else
        [statusMessageField setStringValue:@"The server has stopped running"];

    [controller release];
    controller = nil;
    [serverOutput closeFile];
    [serverOutput release];
    serverOutput = nil;

    if (relaunchServer) {
        relaunchServer = NO;
        [self startServer:self];
    }
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
    if ([showMouseButton state])
        [argv addObject:@"-rfbLocalBuffer"];

    if ([swapMouseButtonsCheckbox state])
        [argv addObject:@"-swapButtons"];
    if ([disableRemoteEventsCheckbox state])
        [argv addObject:@"-disableRemoteEvents"];
    if ([limitToLocalConnections state])
        [argv addObject:@"-localhost"];

    if ([[NSFileManager defaultManager] fileExistsAtPath:passwordFile]) {
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
                [statusMessageField setStringValue:@"Problem - Unable to store password."];
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

- (void) checkForRestart {
    if (controller) {
        [statusMessageField setStringValue:@"Server Running -\n   Option Change Requires a Restart"];

        [startServerButton setTitle:@"Restart Server"];
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
        openPath = [[NSBundle mainBundle] pathForResource:[sender title] ofType:nil];
    }
    if (!openPath) {
        openPath = [[NSBundle mainBundle] pathForResource:[sender title] ofType:@"txt"];
    }

    [[NSWorkspace sharedWorkspace] openFile:openPath];
}

- (IBAction) installAsService: sender {
    OSStatus myStatus;
    AuthorizationFlags myFlags = kAuthorizationFlagDefaults;
    AuthorizationRef myAuthorizationRef;
    AuthorizationItem myItems = {kAuthorizationRightExecute, 0, NULL, 0};
    AuthorizationRights myRights = {1, &myItems};
    
    myStatus = AuthorizationCreate(NULL,
                                   kAuthorizationEmptyEnvironment,
                                   myFlags,
                                   &myAuthorizationRef);

    if (myStatus != errAuthorizationSuccess)
        return;

    myFlags = kAuthorizationFlagDefaults |
        kAuthorizationFlagInteractionAllowed |
        kAuthorizationFlagPreAuthorize |
        kAuthorizationFlagExtendRights;
    // This will pre-authorize the authentication
    myStatus = AuthorizationCopyRights(myAuthorizationRef, &myRights, NULL, myFlags, NULL);
    
    if (myStatus == errAuthorizationSuccess) {
        // If StartupItems doesn't exist then create it
        if (![[NSFileManager defaultManager] fileExistsAtPath:@"/Library/StartupItems"]) {
            char *mkdirArguments[] = { "-p", "/Library/StartupItems", NULL };

            AuthorizationExecuteWithPrivileges(myAuthorizationRef, "/bin/mkdir", kAuthorizationFlagDefaults, mkdirArguments, NULL);
        }
        if (![[NSFileManager defaultManager] fileExistsAtPath:@"/Library/StartupItems/OSXvnc"]) {
            NSMutableArray *copyArgsArray = [NSMutableArray array];
            char **copyArguments = NULL;
            int i;
            
            [copyArgsArray addObject:@"-R"];
            [copyArgsArray addObject:[[NSBundle mainBundle] pathForResource:@"OSXvnc" ofType:nil]];
            [copyArgsArray addObject:@"/Library/StartupItems"];

            copyArguments = malloc(sizeof(char *) * ([copyArgsArray count]+1));
            for (i=0;i<[copyArgsArray count];i++) {
                copyArguments[i] = (char *) [[copyArgsArray objectAtIndex:i] lossyCString];
            }
            copyArguments[i] = NULL;

            myStatus = AuthorizationExecuteWithPrivileges(myAuthorizationRef, "/bin/cp", kAuthorizationFlagDefaults, copyArguments, NULL);
            free(copyArguments);
        }

        // First we will modify the script file
        if (myStatus == errAuthorizationSuccess) {
            NSMutableString *startupScript = [NSMutableString stringWithContentsOfFile:@"/Library/StartupItems/OSXvnc/OSXvnc"];
            char *copyArguments[] = {"-f", "/tmp/OSXvnc", "/Library/StartupItems/OSXvnc/OSXvnc", NULL};
            NSRange argsRange;
            
            // Modify the file

            // Replace the VNCARGS line
            argsRange = [startupScript lineRangeForRange:[startupScript rangeOfString:@"VNCARGS"]];
            if (argsRange.location != NSNotFound) {
                NSMutableString *replaceString = [NSString stringWithFormat:@"VNCARGS=\"%@\"\n",[[self formCommandLine] componentsJoinedByString:@" "]];

                [startupScript replaceCharactersInRange:argsRange withString:replaceString];

                [startupScript writeToFile:@"/tmp/OSXvnc" atomically:YES];
                myStatus = AuthorizationExecuteWithPrivileges(myAuthorizationRef, "/bin/mv", kAuthorizationFlagDefaults, copyArguments, NULL);
            }
        }
    }
    
    AuthorizationFree (myAuthorizationRef, kAuthorizationFlagDefaults);
}

- (void) dealloc {
    [passwordFile release];
    [logFile release];

    [super dealloc];
}

@end
