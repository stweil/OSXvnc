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
#import <netdb.h>

#define PasswordProxy @"********"

@interface NSString (VNCExtensions)
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSString *string;
@end
@implementation NSString (VNCExtensions)
- (NSString *) string {
    return self;
}
@end
@interface NSTextView (VNCExtensions)
- (void) setStringValue: (NSString *) newString;
@end
@implementation NSTextView (VNCExtensions)
- (void) setStringValue: (NSString *) newString {
    self.string = newString;
}
@end

@interface NSFileManager (VNCExtensions)
- (BOOL) directoryExistsAtPath: (NSString *) path;
- (BOOL) createFullDirectoryAtPath:(NSString *)path attributes:(NSDictionary<NSFileAttributeKey, id> *)attributes;
- (BOOL) canWriteToFile: (NSString *) path;
@end

@implementation NSFileManager (VNCExtensions)

- (BOOL) directoryExistsAtPath: (NSString *) path {
    BOOL isDirectory = NO;

    return ([self fileExistsAtPath:path isDirectory: &isDirectory] && isDirectory);
}

- (BOOL) createFullDirectoryAtPath:(NSString *)path attributes:(NSDictionary *)attributes {
    return [self createDirectoryAtPath:path withIntermediateDirectories:YES attributes:attributes error:NULL];
}

- (BOOL) canWriteToFile: (NSString *) path {
    if ([[NSFileManager defaultManager] fileExistsAtPath:path])
        return [[NSFileManager defaultManager] isWritableFileAtPath:path];
    else {
        [[NSFileManager defaultManager] createFullDirectoryAtPath:path.stringByDeletingLastPathComponent attributes:nil];
        return [[NSFileManager defaultManager] isWritableFileAtPath:path.stringByDeletingLastPathComponent];
    }
}

@end


@implementation VNCController

static int shutdownSignal = 0;
static NSColor *successColor;
static NSColor *failureColor;

static void terminateOnSignal(int signal) {
    shutdownSignal = signal;
    NSLog(@"Trapped Signal %d -- Terminating", shutdownSignal);
    [NSApp terminate:NSApp];
}

static NSMutableString *hostNameString(void) {
    char hostName[256];
    gethostname(hostName, 256);

    NSMutableString *hostNameString = [NSMutableString stringWithUTF8String:hostName];
    if ([hostNameString hasSuffix:@".local"])
        [hostNameString deleteCharactersInRange:NSMakeRange(hostNameString.length - 6, 6)];

    return hostNameString;
}

static NSMutableArray *localIPAddresses(void) {
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
            [returnArray addObject:@(ipString)];
        }
    }

    freeifaddrs (ifp);
    return returnArray;
}

- (instancetype) init {
    self = [super init];

    // Transform the GUI into a "ForegroundApp" with Dock Icon and Menu
    // This is so the server can run without a UI
    // 10.3+ only
    // ProcessSerialNumber psn = { 0, kCurrentProcess };
    // OSStatus returnCode = TransformProcessType(& psn, kProcessTransformToForegroundApplication);
    // if (returnCode != 0) {
    //     NSLog(@"Could not transform process type. Error %d", returnCode);
    // }
    //
    // if (![[NSUserDefaults standardUserDefaults] boolForKey:@"autolaunch"])
    //     SetFrontProcess(& psn );
    hostName = [hostNameString() retain];

    successColor = [[NSColor colorWithDeviceRed:0.0 green:0.4 blue:0.0 alpha:1.0] retain];
    failureColor = [[NSColor colorWithDeviceRed:0.6 green:0.0 blue:0.0 alpha:1.0] retain];

    [[NSUserDefaults standardUserDefaults] registerDefaults: @{
                                                               @"PasswordFile": @"",
                                                               @"LogFile": @"",
                                                               @"desktopName": [NSString stringWithFormat:@"%@ (%@)", hostName, NSUserName()],

                                                               @"portNumberSystemServer": @"5900",
                                                               @"desktopNameSystemServer": hostName,

                                                               @"allowSleep": @"NO",
                                                               @"allowDimming": @"YES",
                                                               @"allowScreenSaver": @"YES",
                                                               @"swapButtons": @"YES",
                                                               @"keyboardLayout": @0,
                                                               @"keyboardEvents": @3,
                                                               @"eventSource": @2,

                                                               @"disableRemoteEvents": @"NO",
                                                               @"disableRichClipboard": @"NO",
                                                               @"allowRendezvous": @"YES",
                                                               @"dontDisconnectClients": @"NO",

                                                               @"startServerOnLaunch": @"NO",
                                                               @"terminateOnFastUserSwitch": @"NO",
                                                               @"serverKeepAlive": @"YES",

                                                               @"protocolVersion": @"Default",
                                                               @"otherArguments": @"",

                                                               @"localhostOnly": @"NO",
                                                               @"localhostOnlySystemServer": @"NO",

#if defined(WITH_EXTERNAL_IP)
                                                               @"externalIPURL": @"http://automation.whatismyip.com/n09230945.asp",
#endif

                                                               @"startupItemLocation": @"/Library/StartupItems/OSXvnc",
                                                               @"launchdItemLocation": @"/Library/LaunchAgents/com.redstonesoftware.VineServer.plist"
                                                               }];

    alwaysShared = FALSE;
    neverShared = FALSE;
    userStopped = FALSE;

    automaticReverseHost = [[NSBundle mainBundle].infoDictionary[@"ReverseHost"] copy];
    automaticReversePort  = [[NSBundle mainBundle].infoDictionary[@"ReversePort"] copy];

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

- (IBAction) terminateRequest: sender {
    if (clientList.count && !shutdownSignal)
        NSBeginAlertSheet(NSLocalizedString(@"Quit Vine Server", nil),
                          NSLocalizedString(@"Cancel", nil),
                          NSLocalizedString(@"Quit", nil),
                          nil, statusWindow, self, @selector(terminateSheetDidEnd:returnCode:contextInfo:), NULL, NULL,
                          NSLocalizedString(@"Disconnect %lu clients and quit Vine Server?", nil), (unsigned long)clientList.count);
    else
        [NSApp terminate: self];
}

- (void) terminateSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo {
    if (returnCode == NSAlertAlternateReturn) {
        [sheet orderOut:self];
        [NSApp terminate: self];
    }
}

- (BOOL) authenticationIsValid {
    return (automaticReverseHost.length || [[NSUserDefaults standardUserDefaults] dataForKey:@"vncauth"].length || [[NSUserDefaults standardUserDefaults] integerForKey:@"AuthenticationType"] > 1);
}

- (void) updateHostInfo {
    // These commands can sometimes take a little while, so we have a dedicated thread for them
    if (!waitingForHostInfo) {
        waitingForHostInfo = TRUE;
        [NSThread detachNewThreadSelector:@selector(dedicatedUpdateHostInfoThread) toTarget:self withObject:nil];
    }
}

// Since this can block for a long time in certain DNS situations we will put this in a separate thread
- (void) dedicatedUpdateHostInfoThread {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NS_DURING {
        // flushHostCache is no longer needed since OS X 10.6.
#if 0
        [NSHost flushHostCache];
#endif

        NSHost *currentHost = [NSHost currentHost];
        NSMutableArray *commonHostNames = [currentHost.names mutableCopy];
        NSMutableArray *commonIPAddresses = [currentHost.addresses mutableCopy];
        NSMutableArray *displayIPAddresses = [NSMutableArray array];

#if defined(WITH_EXTERNAL_IP)
        NSURL *externalIP = [NSURL URLWithString:[[NSUserDefaults standardUserDefaults] stringForKey:@"externalIPURL"]];
        NSData *externalIPData = [NSData dataWithContentsOfURL:externalIP];
        NSString *externalIPString = (externalIPData.length ? [NSString stringWithUTF8String: externalIPData.bytes] : @"" );
#endif

        NSEnumerator *ipEnum = nil;
        NSString *anIP = nil;
        BOOL anyConnections = TRUE; // Sadly it looks like the local IP's bypass the firewall anyhow

#if defined(WITH_EXTERNAL_IP)
        if (externalIPString.length && [commonIPAddresses indexOfObject:externalIPString] == NSNotFound)
            [commonIPAddresses insertObject:externalIPString atIndex:0];
#endif

        ipEnum = [commonIPAddresses objectEnumerator];
        while (anIP = [ipEnum nextObject]) {
#if defined(WITH_EXTERNAL_IP)
            BOOL isExternal = [externalIPString isEqualToString:anIP];
#else
            bool isExternal = false;
#endif
            NSMutableAttributedString *ipString = [[[NSMutableAttributedString alloc] initWithString: anIP] autorelease];

            if ([anIP hasPrefix:@"127.0.0.1"] || // localhost entries
                [anIP rangeOfString:@"::"].location != NSNotFound) {
                continue;
            }
            if (isExternal) {
                [ipString replaceCharactersInRange:NSMakeRange(ipString.length,0) withString:@"\tExternal"];
            } else {
                [ipString replaceCharactersInRange:NSMakeRange(ipString.length,0) withString:@"\tInternal"];
            }

            if (controller && !limitToLocalConnections.state) { // Colorize and add tooltip

                NSURL *testURL = [NSURL URLWithString:[NSString stringWithFormat:@"http://%@:%d",
                                                       anIP, self.runningPortNum]];
                NSData *testData = [NSData dataWithContentsOfURL:testURL];
                NSString *testString = (testData.length
                                        ? [NSString stringWithUTF8String: testData.bytes] : @"");

                if ([testString hasPrefix:@"RFB"]) {
                    [ipString replaceCharactersInRange:NSMakeRange(ipString.length,0) withString:@"\tNetwork is configured to allow connections to this IP"];
                    [ipString addAttribute:NSForegroundColorAttributeName value:successColor range:NSMakeRange(0,ipString.length)];
                    anyConnections = TRUE;
                }
                else {
                    [ipString replaceCharactersInRange:NSMakeRange(ipString.length,0) withString:@"\tNetwork is NOT configured to allow connections to this IP"];
                    [ipString addAttribute:NSForegroundColorAttributeName value:failureColor range:NSMakeRange(0,ipString.length)];
                }
            }
            else // We don't want to warn about the firewall if we don't actually do the detection
                anyConnections = TRUE;

            [displayIPAddresses addObject: ipString];
        }

        if (!anyConnections)
            [self performSelectorOnMainThread:@selector(addStatusMessage:) withObject: @"\n(It appears that your firewall is not permitting VNC connections)" waitUntilDone:NO];

        [self performSelectorOnMainThread:@selector(updateHostNames:) withObject: commonHostNames waitUntilDone:NO];
        [self performSelectorOnMainThread:@selector(updateIPAddresses:) withObject: displayIPAddresses waitUntilDone:NO];

        waitingForHostInfo = FALSE;
    }
    NS_HANDLER
    NSLog(@"Exception in updateHostInfo: %@", localException);
    NS_ENDHANDLER
    [pool release];
}

// Display Host Names
- (void) updateHostNames: (NSArray *) newHostNames {
    NSMutableArray *commonHostNames = [[newHostNames mutableCopy] autorelease];
    [commonHostNames removeObject:@"localhost"];

    if (commonHostNames.count > 1) {
        [hostNamesBox setTitle:NSLocalizedString(@"Host Names", nil)];
        hostNamesField.stringValue = [commonHostNames componentsJoinedByString:@"\n"];
    }
    else if (commonHostNames.count == 1) {
        [hostNamesBox setTitle:NSLocalizedString(@"Host Name", nil)];
        hostNamesField.stringValue = [commonHostNames componentsJoinedByString:@"\n"];
    }
    else {
        [hostNamesBox setTitle:NSLocalizedString(@"Host Name", nil)];
        hostNamesField.stringValue = @"";
    }
}

// Display IP Info
- (void) updateIPAddresses: (NSArray *) commonIPAddresses {
    [ipAddressesView renewRows:0 columns:2];

    id ipAddressEnum = [commonIPAddresses objectEnumerator];
    id ipAddress = nil;
    int i = 0;

    while (ipAddress = [ipAddressEnum nextObject]) {
        NSString *anIP = [ipAddress string];
        if ([anIP hasPrefix:@"127.0.0.1"] || // localhost entries
            [anIP rangeOfString:@"::"].location != NSNotFound) {
            ;//[commonIPAddresses removeObject:anIP];
        } else {
            NSRange endOfIP = [anIP rangeOfString:@"\t"];
            NSAttributedString *ipString = ipAddress;
            NSAttributedString *noteString = nil;
            NSString *tooltipString = @"";

            if (endOfIP.location != NSNotFound && [ipAddress isKindOfClass:[NSAttributedString class]]) {
                ipString = [ipAddress attributedSubstringFromRange: NSMakeRange(0,endOfIP.location)];
                noteString = [ipAddress attributedSubstringFromRange: NSMakeRange(endOfIP.location+1,[ipAddress length]-(endOfIP.location+1))];
                endOfIP = [noteString.string rangeOfString:@"\t"];
                if (endOfIP.location != NSNotFound) {
                    tooltipString = [noteString.string substringFromIndex:endOfIP.location+1];
                    noteString = [noteString attributedSubstringFromRange: NSMakeRange(0,endOfIP.location)];
                }
            }

            [ipAddressesView addRow];
            [ipAddressesView cellAtRow:i column:0].attributedStringValue = ipString;
            [ipAddressesView setToolTip:tooltipString forCell:[ipAddressesView cellAtRow:i column:0]];
            [ipAddressesView cellAtRow:i column:1].attributedStringValue = noteString;
            [ipAddressesView setToolTip:tooltipString forCell:[ipAddressesView cellAtRow:i column:1]];
            i++;
        }
    }
    [ipAddressesView sizeToCells];

    if (commonIPAddresses.count > 1) {
        [ipAddressesBox setTitle:NSLocalizedString(@"IP Addresses", nil)];
        //[ipAddressesField setStringValue:[commonIPAddresses componentsJoinedByString:@"\n"]];
    }
    else {
        [ipAddressesBox setTitle:NSLocalizedString(@"IP Address", nil)];
        //[ipAddressesField setStringValue:@""];
    }
}

- (void) addStatusMessage: message {
    if ([message isKindOfClass:[NSAttributedString class]])
        [statusMessageField.textStorage appendAttributedString:message];
    else if ([message isKindOfClass:[NSString class]])
        [statusMessageField.textStorage appendAttributedString:[[[NSAttributedString alloc] initWithString:message] autorelease]];
}

- (NSWindow *) window {
    return preferenceWindow;
}

- (void) determinePasswordLocation {
    NSArray *passwordFiles = @[[[NSUserDefaults standardUserDefaults] stringForKey:@"PasswordFile"],
                               @"~/.vinevncauth",
                               [[NSBundle mainBundle].bundlePath stringByAppendingPathComponent:@".vinevncauth"],
                               @"/tmp/.vinevncauth"];
    NSEnumerator *passwordEnumerators = [passwordFiles objectEnumerator];

    [passwordFile release];
    passwordFile = nil;
    // Find first writable location for the password file
    while (passwordFile = [passwordEnumerators nextObject]) {
        passwordFile = passwordFile.stringByStandardizingPath;
        if (passwordFile.length && [[NSFileManager defaultManager] canWriteToFile:passwordFile]) {
            [passwordFile retain];
            break;
        }
    }
}

- (void) determineLogLocation {
    NSArray *logFiles = @[[[NSUserDefaults standardUserDefaults] stringForKey:@"LogFile"],
                          @"~/Library/Logs/VineServer.log",
                          @"/var/log/VineServer.log",
                          @"/tmp/VineServer.log",
                          [[NSBundle mainBundle].bundlePath stringByAppendingPathComponent:@"VineServer.log"]];
    NSEnumerator *logEnumerators = [logFiles objectEnumerator];

    [logFile release];
    logFile = nil;
    // Find first writable location for the log file
    while (logFile = [logEnumerators nextObject]) {
        logFile = logFile.stringByStandardizingPath;
        if (logFile.length && [[NSFileManager defaultManager] canWriteToFile:logFile]) {
            [logFile retain];
            break;
        }
    }
}

- (void) awakeFromNib {
    id infoDictionary = [NSBundle mainBundle].infoDictionary;

    [self determinePasswordLocation];
    [self determineLogLocation];

    connectPort.stringValue = @"";
    [connectPort.cell performSelector:@selector(setPlaceholderString:) withObject:@"5500"];

    // Copy over old preferences found in OSXvnc
    NSDictionary *oldPrefs = [[NSUserDefaults standardUserDefaults] persistentDomainForName:@"OSXvnc"];

    if (!oldPrefs[@"Converted"]) {
        [[NSUserDefaults standardUserDefaults] registerDefaults:oldPrefs];
        [self loadUserDefaults: self];
        [self saveUserDefaults: self];
        oldPrefs = [oldPrefs mutableCopy];
        ((NSMutableDictionary *)oldPrefs)[@"Converted"] = [NSNumber numberWithBool:TRUE]; // Record that we've converted
        [[NSUserDefaults standardUserDefaults] setPersistentDomain:oldPrefs
                                                           forName:@"OSXvnc"]; // write it back
    }
    else
        [self loadUserDefaults: self];

    statusWindow.title = [NSString stringWithFormat:@"%@: %@",
                          infoDictionary[@"CFBundleName"],
                          displayNameField.stringValue];

    [optionsTabView selectTabViewItemAtIndex:0];

    systemServerIsConfigured = ([[NSFileManager defaultManager] fileExistsAtPath:[[NSUserDefaults standardUserDefaults] stringForKey:@"startupItemLocation"]] ||
                                [[NSFileManager defaultManager] fileExistsAtPath:[[NSUserDefaults standardUserDefaults] stringForKey:@"launchdItemLocation"]]);
    [self loadUIForSystemServer];

    stopServerButton.keyEquivalent = @"";
    startServerButton.keyEquivalent = @"\r";

    preferencesMessageTestField.stringValue = @"";

    // First we'll update with the quick-lookup information that doesn't hang
    [self updateHostNames:@[hostName]];
    [self updateIPAddresses: localIPAddresses()];

    [self updateHostInfo];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    if (startServerOnLaunchCheckbox.state)// && [self authenticationIsValid])
        [self startServer: self];
    if (!NSApp.hidden)
        [statusWindow makeMainWindow];
}

- (void)applicationDidBecomeActive:(NSNotification *)aNotification {
    if (!statusWindow.visible)
        [statusWindow makeKeyAndOrderFront:self];
    [self updateHostInfo];
}

// This is sent when the server's screen params change,
// the server can't handle this right now so we'll restart.
- (void)applicationDidChangeScreenParameters:(NSNotification *)aNotification {
    [self addStatusMessage:@"\n"];
    [self addStatusMessage:NSLocalizedString(@"Screen Resolution changed - Server Reinitialized", nil)];
}

- (void) updateUIForConnectionList: (NSArray *) connectionList {
    NSMutableString *statusMessage = [NSMutableString string];

    [clientList autorelease];
    clientList = [connectionList copy];

    NSUInteger activeConnectionsCount = clientList.count;

    if (!passwordField.stringValue.length)
        [statusMessage appendFormat:@"%@(%@)", NSLocalizedString(@"Server Running", nil),
         NSLocalizedString(@"No Authentication", nil)];
    else
        [statusMessage appendString: NSLocalizedString(@"Server Running", nil)];

    [statusMessage appendString:@"\n"];

    if (activeConnectionsCount == 0)
        [statusMessage appendString: NSLocalizedString(@"No Clients Connected", nil)];
    else if (activeConnectionsCount == 1) {
        [statusMessage appendFormat: @"%d ", 1];
        [statusMessage appendString: NSLocalizedString(@"Client Connected: ", nil)];
        [statusMessage appendString: [clientList[0] valueForKey:@"clientIP"]];
    }
    else if (activeConnectionsCount > 1) {
        [statusMessage appendFormat: @"%lu ", (unsigned long)activeConnectionsCount];
        [statusMessage appendString: NSLocalizedString(@"Clients Connected: ", nil)];
        [statusMessage appendString:
         [[clientList valueForKey:@"clientIP"] componentsJoinedByString:@", "]];
    }
    [statusMessageField setStringValue: statusMessage];

    if (activeConnectionsCount == 0) {
        [[NSApp performSelector:@selector(dockTile)]
         performSelector:@selector(setBadgeLabel:) withObject:@""];
    } else {
        [[NSApp performSelector:@selector(dockTile)]
         performSelector:@selector(setBadgeLabel:) withObject:[NSString stringWithFormat:@"%lu",
                                                               (unsigned long)activeConnectionsCount]];
    }
}

- (void)activeConnections: (NSNotification *) aNotification {
    [self updateUIForConnectionList: aNotification.userInfo[@"clientList"]];
}

- (int) scanForOpenPort: (int) tryPort {
    int listen_fd4=0;
    int value=1;
    struct sockaddr_in sin4;
    bzero(&sin4, sizeof(sin4));
    sin4.sin_len = sizeof(sin4);
    sin4.sin_family = AF_INET;
    sin4.sin_addr.s_addr = htonl(INADDR_ANY);

    // I'm going to only scan on IPv4 since our OSXvnc is going to register in both spaces
    //  struct sockaddr_in6 sin6;
    //  int listen_fd6=0;

    //  bzero(&sin6, sizeof(sin6));
    //  sin6.sin6_len = sizeof(sin6);
    //  sin6.sin6_family = AF_INET6;
    //  sin6.sin6_addr = in6addr_any;

    while (tryPort < 5910) {
        sin4.sin_port = htons(tryPort);
        //sin6.sin6_port = htons(tryPort);

        if ((listen_fd4 = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            //NSLog(@"Socket init failed %d", tryPort);
        }
        else if (fcntl(listen_fd4, F_SETFL, O_NONBLOCK) < 0) {
            //rfbLogPerror("fcntl O_NONBLOCK failed");
        }
        else if (setsockopt(listen_fd4, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
            //NSLog(@"setsockopt SO_REUSEADDR failed %d", tryPort);
        }
        else if (bind(listen_fd4, (struct sockaddr *) &sin4, sizeof(sin4)) < 0) {
            //NSLog(@"Failed to bind socket: port %d may be in use by another VNC", tryPort);
        }
        else if (listen(listen_fd4, 5) < 0) {
            //NSLog(@"Listen failed %d", tryPort);
        }
        /*
         else if ((listen_fd6 = socket(PF_INET6, SOCK_STREAM, 0)) < 0) {
         // NSLog(@"Socket init 6 failed %d", tryPort);
         }
         else if (fcntl(listen_fd6, F_SETFL, O_NONBLOCK) < 0) {
         // rfbLogPerror("IPv6: fcntl O_NONBLOCK failed");
         }
         else if (setsockopt(listen_fd6, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
         //NSLog(@"setsockopt 6 SO_REUSEADDR failed %d", tryPort);
         }
         else if (bind(listen_fd6, (struct sockaddr *) &sin6, sizeof(sin6)) < 0) {
         //NSLog(@"Failed to bind socket: port %d may be in use by another VNC", tryPort);
         }
         else if (listen(listen_fd6, 5) < 0) {
         //NSLog(@"Listen failed %d", tryPort);
         }
         */
        else {
            close(listen_fd4);
            //close(listen_fd6);

            return tryPort;
        }
        close(listen_fd4);
        //close(listen_fd6);

        tryPort++;
    }

    [statusMessageField setStringValue:NSLocalizedString(@"Unable to find open port 5900-5909", nil)];

    return 0;
}


- (int) runningPortNum {
    return portNumText.intValue;
}

- (void) loadUIForSystemServer {
    if (systemServerIsConfigured) {
        startupItemStatusMessageField.textColor = successColor;
        [startupItemStatusMessageField setStringValue:NSLocalizedString(@"Startup Item Configured (Started)", nil)];
    }
    else {
        startupItemStatusMessageField.textColor = failureColor;
        [startupItemStatusMessageField setStringValue:NSLocalizedString(@"Startup Item Disabled (Stopped)", nil)];
    }

    disableStartupButton.enabled = systemServerIsConfigured;
    systemServerMenu.state = (systemServerIsConfigured ? NSControlStateValueOn : NSControlStateValueOff);
    setStartupButton.title = (systemServerIsConfigured ? NSLocalizedString(@"Restart System Server", nil) : NSLocalizedString(@"Start System Server", nil));
}

- (void) loadAuthenticationUI {
    NSUInteger authType = [[NSUserDefaults standardUserDefaults] integerForKey:@"AuthenticationType"];

    if ([[NSUserDefaults standardUserDefaults] dataForKey:@"vncauth"].length) {
        [passwordField setStringValue:PasswordProxy];
        [authenticationType selectCellWithTag:1];
    }
    else if (authType == 2) {
        [authenticationType selectCellWithTag: 2];
    }

    limitToLocalConnections.state = [[NSUserDefaults standardUserDefaults] boolForKey:@"localhostOnly"];
}

- (void) loadSystemServerAuthenticationUI {
    if ([[NSUserDefaults standardUserDefaults] dataForKey:@"vncauthSystemServer"].length) {
        [systemServerPasswordField setStringValue:PasswordProxy];
        [systemServerAuthenticationType selectCellWithTag:1];
    }
    else { // if (authType == 2) {
        [systemServerAuthenticationType selectCellWithTag: 2];
    }

    systemServerLimitToLocalConnections.state = [[NSUserDefaults standardUserDefaults] boolForKey:@"localhostOnlySystemServer"];
}

- (void) loadUIForPort: (NSInteger) port {
    if (port) {
        if (port < 5900 || port > 5909)
            [displayNumberField selectItemWithTitle:@"--"];
        else
            [displayNumberField selectItemWithTitle:[NSString stringWithFormat:@"%d", (int)(port - 5900)]];
        portField.stringValue = [NSString stringWithFormat:@"%u", (unsigned)port];
        displayNumText.stringValue = displayNumberField.title;
        portNumText.stringValue = [NSString stringWithFormat:@"%u", (unsigned)port];
    } else {
        [displayNumberField selectItemWithTitle:@"Auto"];
        port = [self scanForOpenPort:5900];

        if (port) {
            portField.stringValue = @"";
            [portField.cell performSelector:@selector(setPlaceholderString:)
                                 withObject:[NSString stringWithFormat:@"%u", (unsigned)port]];
            displayNumText.intValue = (int)(port - 5900);
            portNumText.stringValue = [NSString stringWithFormat:@"%u", (unsigned)port];
        }
    }
}

- (void) loadUIForSystemServerPort: (NSInteger) port {
    if (port) {
        if (port < 5900 || port > 5909)
            [systemServerDisplayNumberField selectItemWithTitle:@"--"];
        else
            [systemServerDisplayNumberField selectItemWithTitle:[NSString stringWithFormat:@"%d",
                                                                 (int)(port - 5900)]];
        systemServerPortField.stringValue = [NSString stringWithFormat:@"%u", (unsigned)port];
    }
    else {
        [systemServerDisplayNumberField selectItemWithTitle:@"Auto"];
        port = [self scanForOpenPort:5900];

        if (port) {
            systemServerPortField.stringValue = @"";
            [systemServerPortField.cell performSelector:@selector(setPlaceholderString:)
                                             withObject:[NSString stringWithFormat:@"%u",
                                                         (unsigned)port]];
        }
    }
}

- (void) loadUserDefaults: sender {
    [self loadAuthenticationUI];
    [self loadSystemServerAuthenticationUI];

    [self loadUIForPort: [[NSUserDefaults standardUserDefaults] integerForKey:@"portNumber"]];
    [self loadUIForSystemServerPort: [[NSUserDefaults standardUserDefaults] integerForKey:@"portNumberSystemServer"]];

    displayNameField.stringValue = [[NSUserDefaults standardUserDefaults] stringForKey:@"desktopName"];
    systemServerDisplayNameField.stringValue = [[NSUserDefaults standardUserDefaults] stringForKey:@"desktopNameSystemServer"];

    allowSleepCheckbox.state = [[NSUserDefaults standardUserDefaults] boolForKey:@"allowSleep"];
    allowDimmingCheckbox.state = [[NSUserDefaults standardUserDefaults] boolForKey:@"allowDimming"];
    allowScreenSaverCheckbox.state = [[NSUserDefaults standardUserDefaults] boolForKey:@"allowScreenSaver"];
    swapMouseButtonsCheckbox.state = [[NSUserDefaults standardUserDefaults] boolForKey:@"swapButtons"];
    [keyboardLayout selectItemAtIndex:[keyboardLayout indexOfItemWithTag:[[NSUserDefaults standardUserDefaults] integerForKey:@"keyboardLayout"]]];
    [keyboardEvents selectItemAtIndex:[keyboardEvents indexOfItemWithTag:[[NSUserDefaults standardUserDefaults] integerForKey:@"keyboardEvents"]]];
    [eventSourcePopup selectItemAtIndex:[eventSourcePopup indexOfItemWithTag:[[NSUserDefaults standardUserDefaults] integerForKey:@"eventSource"]]];

    disableRemoteEventsCheckbox.state = [[NSUserDefaults standardUserDefaults] boolForKey:@"disableRemoteEvents"];
    disableRichClipboardCheckbox.state = [[NSUserDefaults standardUserDefaults] boolForKey:@"disableRichClipboard"];
    allowRendezvousCheckbox.state = [[NSUserDefaults standardUserDefaults] boolForKey:@"allowRendezvous"];

    [sharingMatrix selectCellWithTag: [[NSUserDefaults standardUserDefaults] integerForKey:@"sharingMode"]];
    dontDisconnectCheckbox.state = [[NSUserDefaults standardUserDefaults] boolForKey:@"dontDisconnectClients"];
    [self changeSharing:self];

    if ([[NSUserDefaults standardUserDefaults] floatForKey:@"protocolVersion"] > 0.0)
        [protocolVersion selectItemWithTitle:[[NSUserDefaults standardUserDefaults] stringForKey:@"protocolVersion"]];
    otherArguments.stringValue = [[NSUserDefaults standardUserDefaults] stringForKey:@"otherArguments"];

    startServerOnLaunchCheckbox.state = [[NSUserDefaults standardUserDefaults] boolForKey:@"startServerOnLaunch"];
    terminateOnFastUserSwitch.state = [[NSUserDefaults standardUserDefaults] boolForKey:@"terminateOnFastUserSwitch"];
    serverKeepAliveCheckbox.state = [[NSUserDefaults standardUserDefaults] boolForKey:@"serverKeepAlive"];
}

- (void) saveUserDefaults: sender {
    if (displayNameField.stringValue.length)
        [[NSUserDefaults standardUserDefaults] setObject:displayNameField.stringValue forKey:@"desktopName"];

    if (displayNumberField.selectedItem.tag == 0)
        [[NSUserDefaults standardUserDefaults] setInteger:0 forKey:@"portNumber"];
    else
        [[NSUserDefaults standardUserDefaults] setInteger:portField.intValue forKey:@"portNumber"];

    [[NSUserDefaults standardUserDefaults] setInteger:(authenticationType.selectedCell).tag forKey:@"AuthenticationType"];

    // System Server
    {
        if (systemServerDisplayNameField.stringValue.length)
            [[NSUserDefaults standardUserDefaults] setObject:systemServerDisplayNameField.stringValue forKey:@"desktopNameSystemServer"];

        if (systemServerDisplayNumberField.selectedItem.tag == 0)
            [[NSUserDefaults standardUserDefaults] setInteger:0 forKey:@"portNumberSystemServer"];
        else
            [[NSUserDefaults standardUserDefaults] setInteger:systemServerPortField.intValue forKey:@"portNumberSystemServer"];

        [[NSUserDefaults standardUserDefaults] setInteger:(systemServerAuthenticationType.selectedCell).tag forKey:@"AuthenticationTypeSystemServer"];
        [[NSUserDefaults standardUserDefaults] setBool:systemServerLimitToLocalConnections.state forKey:@"localhostOnlySystemServer"];
    }

    [[NSUserDefaults standardUserDefaults] setBool:swapMouseButtonsCheckbox.state forKey:@"swapButtons"];

    [[NSUserDefaults standardUserDefaults] setInteger:(sharingMatrix.selectedCell).tag forKey:@"sharingMode"];
    [[NSUserDefaults standardUserDefaults] setBool:dontDisconnectCheckbox.state forKey:@"dontDisconnectClients"];
    [[NSUserDefaults standardUserDefaults] setBool:disableRemoteEventsCheckbox.state forKey:@"disableRemoteEvents"];
    [[NSUserDefaults standardUserDefaults] setBool:disableRichClipboardCheckbox.state forKey:@"disableRichClipboard"];

    [[NSUserDefaults standardUserDefaults] setBool:limitToLocalConnections.state forKey:@"localhostOnly"];
    [[NSUserDefaults standardUserDefaults] setBool:allowRendezvousCheckbox.state forKey:@"allowRendezvous"];

    [[NSUserDefaults standardUserDefaults] setBool:startServerOnLaunchCheckbox.state forKey:@"startServerOnLaunch"];
    [[NSUserDefaults standardUserDefaults] setBool:terminateOnFastUserSwitch.state forKey:@"terminateOnFastUserSwitch"];
    [[NSUserDefaults standardUserDefaults] setBool:serverKeepAliveCheckbox.state forKey:@"serverKeepAlive"];

    [[NSUserDefaults standardUserDefaults] setBool:allowSleepCheckbox.state forKey:@"allowSleep"];
    [[NSUserDefaults standardUserDefaults] setBool:allowDimmingCheckbox.state forKey:@"allowDimming"];
    [[NSUserDefaults standardUserDefaults] setBool:allowScreenSaverCheckbox.state forKey:@"allowScreenSaver"];

    [[NSUserDefaults standardUserDefaults] setInteger:keyboardLayout.selectedItem.tag forKey:@"keyboardLayout"];
    [[NSUserDefaults standardUserDefaults] setInteger:keyboardEvents.selectedItem.tag forKey:@"keyboardEvents"];
    [[NSUserDefaults standardUserDefaults] setInteger:eventSourcePopup.selectedItem.tag forKey:@"eventSource"];

    if (protocolVersion.titleOfSelectedItem.floatValue > 0.0)
        [[NSUserDefaults standardUserDefaults] setFloat:protocolVersion.titleOfSelectedItem.floatValue forKey:@"protocolVersion"];
    else
        [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"protocolVersion"];

    [[NSUserDefaults standardUserDefaults] setObject:otherArguments.stringValue
                                              forKey:@"otherArguments"];

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
    if (![preferenceWindow makeFirstResponder:preferenceWindow]) {
        [preferenceWindow endEditingFor:nil];
    }
    if (![statusWindow makeFirstResponder:statusWindow]) {
        [statusWindow endEditingFor:nil];
    }
    if (displayNumberField.selectedItem.tag == 0) {
        [self loadUIForPort:0];  // To update the UI on the likely port that we will get
    }


    if (![self authenticationIsValid]) {
        [NSApp beginSheet: initialWindow modalForWindow:statusWindow modalDelegate:nil didEndSelector: NULL contextInfo: NULL];
        return;
    }
    if ([[NSUserDefaults standardUserDefaults] dataForKey:@"vncauth"] && ![[[NSUserDefaults standardUserDefaults] dataForKey:@"vncauth"] writeToFile: passwordFile atomically:NO]) {
        [self addStatusMessage:@"Unable to start - problem writing vnc password file"];
        return;
    }

    if ((argv = [self formCommandLineForSystemServer: NO])) {
        NSDictionary *infoDictionary = [NSBundle mainBundle].infoDictionary;

        NSString *executionPath = [[NSBundle mainBundle].bundlePath
                                   stringByAppendingPathComponent: @"Contents/MacOS/OSXvnc-server"];
        NSString *noteStartup = [NSString stringWithFormat:@"%@\tStarting %@ %@(%@)\n",
                                 [NSDate date], [NSProcessInfo processInfo].processName,
                                 [infoDictionary valueForKey:@"CFBundleShortVersion"],
                                 [infoDictionary valueForKey:@"CFBundleVersion"]];

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
        [serverOutput writeData:[noteStartup dataUsingEncoding: NSUTF8StringEncoding]];
        [serverOutput writeData:[[argv componentsJoinedByString:@" "] dataUsingEncoding: NSUTF8StringEncoding]];
        [serverOutput writeData:[@"\n\n" dataUsingEncoding: NSUTF8StringEncoding]];

        controller = [[NSTask alloc] init];
        controller.launchPath = executionPath;
        controller.arguments = argv;
        controller.standardOutput = serverOutput;
        controller.standardError = serverOutput;

        [[NSDistributedNotificationCenter defaultCenter] addObserver:self
                                                            selector:@selector(activeConnections:)
                                                                name:@"VNCConnections"
                                                              object:[NSString stringWithFormat:@"OSXvnc%d", self.runningPortNum]
                                                  suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];

        [controller launch];

        [lastLaunchTime release];
        lastLaunchTime = [[NSDate date] retain];

        [[NSNotificationCenter defaultCenter] addObserver: self
                                                 selector: @selector(serverStopped:)
                                                     name: NSTaskDidTerminateNotification
                                                   object: controller];

        if (!passwordField.stringValue.length)
            [statusMessageField setStringValue:[NSString stringWithFormat:@"%@ - %@", NSLocalizedString(@"Server Running", nil), NSLocalizedString(@"No Authentication", nil)]];
        else
            [statusMessageField setStringValue:NSLocalizedString(@"Server Running", nil)];
        //[startServerButton setEnabled:FALSE];
        [startServerButton setTitle:NSLocalizedString(@"Restart Server", nil)];
        [startServerMenuItem setTitle:NSLocalizedString(@"Restart Server", nil)];
        [stopServerButton setEnabled:TRUE];
        serverMenuItem.state = NSOnState;
        // We really don't want people to accidentally stop the server
        //[startServerButton setKeyEquivalent:@""];
        //[stopServerButton setKeyEquivalent:@"\r"];
        userStopped = FALSE;

        /* Only auto-connect the very first time ??; */
        if (automaticReverseHost.length) {
            [self addStatusMessage:[NSString stringWithFormat:@"\n%@: %@", NSLocalizedString(@"Initiating Reverse Connection To Host", nil), automaticReverseHost]];
            //  [automaticReverseHost release];
            //  automaticReverseHost = nil;
            //  [automaticReversePort release];
            //  automaticReversePort = nil;
        }

        // Give the server a second to launch
        [self performSelector:@selector(updateHostInfo) withObject:nil afterDelay:1.0];
    }
}

- (void) stopServer: sender {
    [self updateHostInfo];

    if (controller != nil) {
        userStopped = TRUE;
        [controller terminate];
    }
    else {
        [statusMessageField setStringValue:NSLocalizedString(@"The server is stopped.", nil)];
    }
}

- (void) serverStopped: (NSNotification *) aNotification {
    // If we don't get the notification soon enough, we may have already restarted
    if (controller.running) {
        return;
    }

    [[NSDistributedNotificationCenter defaultCenter] removeObserver:self
                                                               name:@"VNCConnections"
                                                             object:[NSString stringWithFormat:@"OSXvnc%d", self.runningPortNum]];

    [[NSNotificationCenter defaultCenter] removeObserver: self
                                                    name: NSTaskDidTerminateNotification
                                                  object: controller];

    [self updateUIForConnectionList:[NSArray array]];

    preferencesMessageTestField.stringValue = @"";
    [startServerButton setTitle:NSLocalizedString(@"Start Server", nil)];
    [startServerMenuItem setTitle:NSLocalizedString(@"Start Server", nil)];
    //[startServerButton setEnabled:TRUE];
    [stopServerButton setEnabled:FALSE];
    serverMenuItem.state = NSControlStateValueOff;
    //[stopServerButton setKeyEquivalent:@""];
    //[startServerButton setKeyEquivalent:@"\r"];

    if (userStopped)
        [statusMessageField setStringValue:NSLocalizedString(@"The server is stopped.", nil)];
    else if (controller.terminationStatus==250) {
        NSMutableString *messageString = [NSMutableString stringWithFormat: NSLocalizedString(@"Vine Server can't listen on the specified port (%d).", nil), self.runningPortNum];
        [messageString appendString:@"\n"];
        if (systemServerIsConfigured)
            [messageString appendString:NSLocalizedString(@"Probably because the VNC server is already running as a Startup Item.", nil)];
        else
            [messageString appendString:NSLocalizedString(@"Probably because another VNC is already using this port.", nil)];
        [statusMessageField setStringValue:messageString];
    }
    else if (controller.terminationStatus) {
        [statusMessageField setStringValue:[NSString stringWithFormat:NSLocalizedString(@"The server has stopped running. See Log (%d)\n", nil), controller.terminationStatus]];
    }
    else
        [statusMessageField setStringValue:NSLocalizedString(@"The server has stopped running.", nil)];

    if (!userStopped &&
        serverKeepAliveCheckbox.state &&
        controller.terminationStatus >= 0 &&
        controller.terminationStatus <= 64 &&
        -lastLaunchTime.timeIntervalSinceNow > 1.0)
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

- (NSMutableArray *) formCommandLineForSystemServer: (BOOL) isSystemServer {
    NSMutableArray *argv = [NSMutableArray array];

    if (isSystemServer) {
        [argv addObject:@"-rfbport"];
        if (systemServerDisplayNumberField.selectedItem.tag == 0)
            [argv addObject:@"0"];
        else
            [argv addObject:[NSString stringWithFormat:@"%d", systemServerPortField.intValue]];

        if (systemServerDisplayNameField.stringValue.length) {
            [argv addObject:@"-desktop"];
            [argv addObject:systemServerDisplayNameField.stringValue];
        }

        switch ((systemServerAuthenticationType.selectedCell).tag) {
            case 2:
                [argv addObject:@"-rfbnoauth"];
                break;
            case 1:
            default:
                if (passwordFile && [[NSFileManager defaultManager] fileExistsAtPath:passwordFile]) {
                    [argv addObject:@"-rfbauth"];
                    [argv addObject:passwordFile];
                }
                else {
                    [startupItemStatusMessageField setStringValue:NSLocalizedString(@"Error: Valid VNC password required to start server", nil)];
                    return nil;
                }
                break;
        }

        if (systemServerLimitToLocalConnections.state)
            [argv addObject:@"-localhost"];

        [argv addObject:@"-SystemServer"];
        [argv addObject:@"1"];
    }
    else {
        [argv addObject:@"-rfbport"];
        if (displayNumberField.selectedItem.tag == 0)
            [argv addObject:@"0"];
        else
            [argv addObject:[NSString stringWithFormat:@"%d", portField.intValue]];

        if (displayNameField.stringValue.length) {
            [argv addObject:@"-desktop"];
            [argv addObject:displayNameField.stringValue];
        }

        [argv addObject:@"-donotloadproxy"];

        switch ((authenticationType.selectedCell).tag) {
            case 2:
                [argv addObject:@"-rfbnoauth"];
                break;
            case 1:
            default:
                if (passwordFile && [[NSFileManager defaultManager] fileExistsAtPath:passwordFile]) {
                    [argv addObject:@"-rfbauth"];
                    [argv addObject:passwordFile];
                }
                else {
                    [self addStatusMessage:[NSString stringWithFormat:@"\n%@", NSLocalizedString(@"Valid VNC password required to start server", nil)]];
                    return nil;
                }
                break;
        }

        if (automaticReverseHost.length) {
            [argv addObject:@"-connectHost"];
            [argv addObject:automaticReverseHost];

            if (automaticReversePort.intValue) {
                [argv addObject:@"-connectPort"];
                [argv addObject: automaticReversePort];
            }
        }

        if (limitToLocalConnections.state)
            [argv addObject:@"-localhost"];
    }

    if (alwaysShared)
        [argv addObject:@"-alwaysshared"];
    if (neverShared)
        [argv addObject:@"-nevershared"];
    if (dontDisconnectCheckbox.state && !alwaysShared)
        [argv addObject:@"-dontdisconnect"];

    if (allowSleepCheckbox.state)
        [argv addObject:@"-allowsleep"];
    if (!allowDimmingCheckbox.state)
        [argv addObject:@"-nodimming"];
    if (!allowScreenSaverCheckbox.state)
        [argv addObject:@"-disableScreenSaver"];

    if (protocolVersion.titleOfSelectedItem.floatValue > 0.0) {
        [argv addObject:@"-protocol"];
        [argv addObject:protocolVersion.titleOfSelectedItem];
    }

    [argv addObject:@"-restartonuserswitch"];
    [argv addObject:(terminateOnFastUserSwitch.state ? @"Y" : @"N")];

    switch (keyboardLayout.selectedItem.tag) {
        case 2:
            [argv addObject:@"-UnicodeKeyboard"];
            [argv addObject:@"1"];
            [argv addObject:@"-keyboardLoading"];
            [argv addObject:@"N"];
            [argv addObject:@"-pressModsForKeys"];
            [argv addObject:@"Y"];
            break;
        case 1:
            [argv addObject:@"-UnicodeKeyboard"];
            [argv addObject:@"0"];
            [argv addObject:@"-keyboardLoading"];
            [argv addObject:@"Y"];
            [argv addObject:@"-pressModsForKeys"];
            [argv addObject:@"Y"];
            break;
        case 0:
        default:
            [argv addObject:@"-UnicodeKeyboard"];
            [argv addObject:@"0"];
            [argv addObject:@"-keyboardLoading"];
            [argv addObject:@"N"];
            [argv addObject:@"-pressModsForKeys"];
            [argv addObject:@"N"];
            break;
    }
    [argv addObject:@"-EventTap"];
    [argv addObject:[NSString stringWithFormat:@"%ld", keyboardEvents.selectedItem.tag]];
    [argv addObject:@"-EventSource"];
    [argv addObject:[NSString stringWithFormat:@"%ld", eventSourcePopup.selectedItem.tag]];


    if (swapMouseButtonsCheckbox.state)
        [argv addObject:@"-swapButtons"];
    if (disableRemoteEventsCheckbox.state)
        [argv addObject:@"-disableRemoteEvents"];
    if (disableRichClipboardCheckbox.state)
        [argv addObject:@"-disableRichClipboards"];

    [argv addObject:@"-rendezvous"];
    [argv addObject:(allowRendezvousCheckbox.state ? @"Y" : @"N")];

    if ([[NSUserDefaults standardUserDefaults] objectForKey:@"rfbDeferUpdateTime"]) {
        [argv addObject:@"-deferupdate"];
        [argv addObject:[[NSUserDefaults standardUserDefaults] stringForKey:@"rfbDeferUpdateTime"]];
    }

    if (otherArguments.stringValue.length)
        [argv addObjectsFromArray:[otherArguments.stringValue componentsSeparatedByString:@" "]];

    return argv;
}

- (void)controlTextDidChange:(NSNotification *)aNotification {
    if ([aNotification.object isKindOfClass:[NSControl class]]) {
        if ([aNotification.object target] && ([aNotification.object action] != NULL)) {
            [[aNotification.object target] performSelector: [aNotification.object action] withObject: aNotification.object];
        }
    }
}

- (IBAction) validateInitialAuthentication: sender {
    NSString *passwordString = initialPasswordText.stringValue;

    if (sender == initialPasswordText && passwordString.length) {
        [initialAuthenticationType selectCellWithTag:1];
        [initialDoneButton setEnabled: TRUE];
    }
    else if (sender == initialAuthenticationType) {
        long newAuth = (initialAuthenticationType.selectedCell).tag;
        if (newAuth == 1) {
            initialPasswordText.stringValue = @"";
            [initialDoneButton setEnabled: FALSE];
            [initialPasswordText.window makeFirstResponder: initialPasswordText];
        }
        // No Auth
        else { //if (newAuth == 2) {
            initialPasswordText.stringValue = @"";
            [initialPasswordText.window makeFirstResponder: nil];
            [initialDoneButton setEnabled: TRUE];
        }
    }
    else {
        [initialDoneButton setEnabled: FALSE];
    }
}

- (IBAction) setInitialAuthentication: sender {
    NSString *passwordString = initialPasswordText.stringValue;
    long newAuth = (initialAuthenticationType.selectedCell).tag;

    // VNC Password
    if (newAuth == 1) {
        [[NSUserDefaults standardUserDefaults] setObject:[NSData dataWithBytes:(const void *)vncEncryptPasswd(passwordString.UTF8String) length:8] forKey:@"vncauth"];
    }
    // No Auth
    else if (newAuth == 2) {
        [[NSFileManager defaultManager] removeItemAtPath:passwordFile error:NULL];
        [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"vncauth"];
        passwordField.stringValue = @"";
    }
    [[NSUserDefaults standardUserDefaults] setInteger:(initialAuthenticationType.selectedCell).tag forKey:@"AuthenticationType"];

    [self loadAuthenticationUI];

    [initialWindow orderOut:self];
    [NSApp endSheet: initialWindow];

    if (startServerOnLaunchCheckbox.state)
        [self startServer: self];
}

- (void) changeDisplayNumber: sender {
    [self loadUIForPort: displayNumberField.selectedItem.tag];

    if (sender != self) {
        [self saveUserDefaults: self];
        [self checkForRestart];
    }
}

- (void) changePort: sender {
    [self loadUIForPort: portField.intValue];

    if (sender != self) {
        [self saveUserDefaults: self];
        [self checkForRestart];
    }
}

- (void) changeSharing: sender {
    long selected = (sharingMatrix.selectedCell).tag;
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

- (IBAction) changeAuthenticationType: sender {
    long newAuth = (authenticationType.selectedCell).tag;

    if (newAuth != [[NSUserDefaults standardUserDefaults] integerForKey:@"AuthenticationType"]) {
        // VNC Password
        if (newAuth == 1) {
            passwordField.stringValue = @"";
            [preferenceWindow makeFirstResponder: passwordField];
        }
        // No Auth
        else if (newAuth == 2) {
            [[NSFileManager defaultManager] removeItemAtPath:passwordFile error:NULL];
            [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"vncauth"];
            passwordField.stringValue = @"";
            [preferenceWindow makeFirstResponder: nil];
        }

        if (sender != self) {
            [self saveUserDefaults: self];
            [self checkForRestart];
        }
    }
}

- (void) changePassword: sender {
    NSString *passwordString = [sender stringValue];

    if (passwordString.length && ![passwordString isEqualToString:PasswordProxy]) {
        [authenticationType selectCellWithTag:1];
        [[NSUserDefaults standardUserDefaults] setObject:[NSData dataWithBytes:(const void *)vncEncryptPasswd(passwordString.UTF8String) length:8] forKey:@"vncauth"];

        if (sender != self) {
            [self saveUserDefaults: self];
            [self checkForRestart];
        }
    }
}

- (IBAction) changeDisplayName: sender {
    if (![displayNameField.stringValue isEqualToString:[[NSUserDefaults standardUserDefaults] objectForKey:@"desktopName"]] && sender != self) {
        [self saveUserDefaults: self];
        statusWindow.title = [NSString stringWithFormat:@"%@: %@",
                              [NSBundle mainBundle].infoDictionary[@"CFBundleName"],
                              displayNameField.stringValue];
        [self checkForRestart];
    }
}

- (IBAction) optionChanged: sender {
    if (sender != self) {
        [self saveUserDefaults: sender];
        [self checkForRestart];
    }
}

- (void) changeSystemServerPort: sender {
    if (sender == systemServerPortField)
        [self loadUIForSystemServerPort: systemServerPortField.intValue];
    else
        [self loadUIForSystemServerPort: systemServerDisplayNumberField.selectedItem.tag];

    [self saveUserDefaults: self];
}

- (IBAction) changeSystemServerAuthentication: sender {
    NSString *passwordString = systemServerPasswordField.stringValue;
    long sysServerAuthType = (systemServerAuthenticationType.selectedCell).tag;

    if (sender == systemServerPasswordField && passwordString.length && ![passwordString isEqualToString:PasswordProxy]) {
        char *encPassword = vncEncryptPasswd(passwordString.UTF8String);

        [systemServerAuthenticationType selectCellWithTag:1];
        [[NSUserDefaults standardUserDefaults] setObject:[NSData dataWithBytes:(const void *)encPassword length:8] forKey:@"vncauthSystemServer"];
    }

    if (sender == systemServerAuthenticationType &&
        sysServerAuthType != [[NSUserDefaults standardUserDefaults] integerForKey:@"AuthenticationTypeSystemServer"]) {
        // VNC Password
        [[NSUserDefaults standardUserDefaults] setInteger:sysServerAuthType forKey:@"AuthenticationTypeSystemServer"];
        if (sysServerAuthType == 1) {
            systemServerPasswordField.stringValue = @"";
            [systemServerWindow makeFirstResponder: systemServerPasswordField];
        }
        // No Auth
        else if (sysServerAuthType == 2) {
            [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"vncauthSystemServer"];
            systemServerPasswordField.stringValue = @"";
            [systemServerWindow makeFirstResponder: nil];
        }
    }
}

// Bring up the Reverse Connection Window In A Sheet...
- (IBAction) reverseConnection: sender {
    reverseConnectionMessageField.stringValue = @"";
    [NSApp beginSheet:reverseConnectionWindow modalForWindow:statusWindow modalDelegate:self didEndSelector:NULL contextInfo:NULL];
}

- (IBAction) cancelConnectHost: sender {
    [NSApp endSheet: reverseConnectionWindow];
    [reverseConnectionWindow orderOut:self];
}

// This will issue a Distributed Notification to add a VNC client
- (IBAction) connectHost: sender {
    NSMutableDictionary *argumentsDict = [NSMutableDictionary dictionaryWithObjectsAndKeys:connectHost.stringValue,@"ConnectHost",connectPort.stringValue,@"ConnectPort",nil];

    if (!connectHost.stringValue.length) {
        [reverseConnectionMessageField setStringValue:NSLocalizedString(@"Please specify a Connect Host to establish a connection", nil)];
        return;
    }
    if (!connectPort.intValue) {
        argumentsDict[@"ConnectPort"] = @"5500";
    }

    if (!controller) {
        [self startServer: self];
        usleep(500000); // Give that server time to start
    }

    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:@"VNCConnectHost"
                                                                   object:[NSString stringWithFormat:@"OSXvnc%d", self.runningPortNum]
                                                                 userInfo:argumentsDict
                                                       deliverImmediately:YES];

    usleep(500000); // Give notification time to post

    if (kill(controller.processIdentifier, SIGCONT) == 0) {
        [self addStatusMessage: @"\n"];
        [self addStatusMessage: NSLocalizedString(@"Connection invitation sent to Connect Host", nil)];
    }
    else {
        [self addStatusMessage: @"\n"];
        [self addStatusMessage:[NSString stringWithFormat:NSLocalizedString(@"Error sending invitation: %s", nil), strerror(errno)]];
    }

    if (statusWindow.attachedSheet == reverseConnectionWindow) {
        [NSApp endSheet: reverseConnectionWindow];
        [reverseConnectionWindow orderOut:self];
    }
}

- (void) checkForRestart {
    if (controller) {
        if (![statusMessageField.textStorage.string hasSuffix: NSLocalizedString(@"Option Change Requires a Restart", nil)]) {
            [self addStatusMessage: @"\n"];
            [self addStatusMessage: NSLocalizedString(@"Option Change Requires a Restart", nil)];
        }

        [preferencesMessageTestField setStringValue:NSLocalizedString(@"Option Change Requires a Restart", nil)];
    }
}

- (void) applicationWillTerminate: (NSNotification *) notification {
    [self stopServer: self];
    [preferenceWindow endEditingFor: nil];
    [statusWindow endEditingFor: nil];

    [self saveUserDefaults:self];
}

- (BOOL) validateMenuItem: (NSMenuItem *) menuItem {
    // Disable the 'start server' menu item when the server is already started.
    // Disable the 'stop server' menu item when the server is not running.
    //    if ((menuItem == startServerMenuItem) && controller) {
    //        return FALSE;
    //    }
    if ((menuItem == stopServerMenuItem) && (!controller)) {
        return FALSE;
    }

    return TRUE;
}

- (IBAction) openFirewall:(id) sender {
    NSURL *aURL = [NSURL fileURLWithPath:@"/System/Library/PreferencePanes/Security.prefPane"];
    [[NSWorkspace sharedWorkspace] openURL: aURL];
}

- (IBAction) openLog:(id) sender {
    [[NSWorkspace sharedWorkspace] openFile:logFile];
}

- (IBAction) openGPL:(id) sender {
    NSURL *openPath = [[NSBundle mainBundle] URLForResource:@"Copying" withExtension:@"rtf"];

    [[NSWorkspace sharedWorkspace] openURL:openPath];
}

- (IBAction) openReleaseNotes:(id) sender {
    NSURL *openPath = [[NSBundle mainBundle] URLForResource:@"Vine Server Release Notes" withExtension:@"rtf"];

    [[NSWorkspace sharedWorkspace] openURL:openPath];
}

- (IBAction) openFile:(id) sender {
    NSURL *openPath = [[NSBundle mainBundle] URLForResource:[sender title] withExtension:@"rtf"];

    if (!openPath) {
        openPath = [[NSBundle mainBundle] URLForResource:[sender title] withExtension:@"pdf"];
    }
    if (!openPath) {
        openPath = [[NSBundle mainBundle] URLForResource:[sender title] withExtension:@"txt"];
    }
    if (!openPath) {
        openPath = [[NSBundle mainBundle] URLForResource:[sender title] withExtension:nil];
    }

    [[NSWorkspace sharedWorkspace] openURL:openPath];
}

- (BOOL) installLaunchd {
    BOOL success = TRUE;
    NSMutableDictionary *launchdDictionary = [NSMutableDictionary dictionary];
    NSString *launchdPath = [[NSUserDefaults standardUserDefaults] stringForKey:@"launchdItemLocation"];
    NSString *logLocation = @"/Library/Logs/VineServer.log";
    NSString *oldPasswordFile = passwordFile;
    NSData *vncauth = [[NSUserDefaults standardUserDefaults] dataForKey:@"vncauthSystemServer"];

    NSString *launchdResources = @"/Library/Application Support/VineServer";

    // If VineServer resources directory doesn't exist then create it
    if (![[NSFileManager defaultManager] fileExistsAtPath:launchdResources]) {
        success &= [myAuthorization executeCommand:@"/bin/mkdir"
                                          withArgs:@[@"-p", launchdResources]];
        success &= [myAuthorization executeCommand:@"/usr/sbin/chown"
                                          withArgs:@[@"-R", @"root:wheel", launchdResources]];
        if (!success) {
            [startupItemStatusMessageField setStringValue:NSLocalizedString(@"Error: Unable to setup VineServer folder", nil)];
            success = FALSE;
        }
    }

    if (vncauth.length) {
        passwordFile = [launchdResources stringByAppendingPathComponent: @".vinevncauth"];
        [vncauth writeToFile:@"/tmp/.vinevncauth" atomically:YES];
        if (![myAuthorization executeCommand:@"/bin/mv"
                                    withArgs:@[@"-f", @"/tmp/.vinevncauth", passwordFile]]) {
            [startupItemStatusMessageField setStringValue:NSLocalizedString(@"Error: Unable To Setup Password File", nil)];
            success = FALSE;
        }
    }
    NSMutableArray *argv = [self formCommandLineForSystemServer: YES];
    passwordFile = oldPasswordFile;

    if (success && argv) {
        NSMutableArray *copyArgsArray = [NSMutableArray array];
        NSString *executionPath = [[NSBundle mainBundle].bundlePath
                                   stringByAppendingPathComponent: @"Contents/MacOS/OSXvnc-server"];

        // Copy Server Executable
        [copyArgsArray removeAllObjects];
        [copyArgsArray addObject:@"-R"]; // Recursive
        [copyArgsArray addObject:@"-f"]; // Force Copy (overwrite existing)
        [copyArgsArray addObject:executionPath];
        [copyArgsArray addObject:launchdResources];

        if (![myAuthorization executeCommand:@"/bin/cp" withArgs:copyArgsArray]) {
            [startupItemStatusMessageField setStringValue:NSLocalizedString(@"Error: Unable to copy OSXvnc-server executable", nil)];
            return FALSE;
        }

        // Copy All Bundles
        NSEnumerator *bundleEnum = [[NSBundle pathsForResourcesOfType:@"bundle" inDirectory:[NSBundle mainBundle].resourcePath] objectEnumerator];
        NSString *bundlePath = nil;

        while (bundlePath = [bundleEnum nextObject]) {
            [copyArgsArray removeAllObjects];
            [copyArgsArray addObject:@"-R"]; // Recursive
            [copyArgsArray addObject:@"-f"]; // Force Copy (overwrite existing)
            [copyArgsArray addObject:bundlePath];
            [copyArgsArray addObject:[launchdResources stringByAppendingPathComponent:@"Resources"]];

            if (![myAuthorization executeCommand:@"/bin/cp" withArgs:copyArgsArray]) {
                startupItemStatusMessageField.stringValue = [NSString stringWithFormat:@"Error: Unable to copy bundle:%@", bundlePath.lastPathComponent];
                return FALSE;
            }
        }
        success &= [myAuthorization executeCommand:@"/bin/chmod"
                                          withArgs:@[@"-R", @"755", launchdResources]];

        // Configure PLIST
        launchdDictionary[@"Label"] = @"VineServer";
        [argv insertObject:[launchdResources stringByAppendingPathComponent:@"OSXvnc-server"] atIndex:0];
        launchdDictionary[@"ProgramArguments"] = argv;

        launchdDictionary[@"KeepAlive"] = [NSNumber numberWithBool:TRUE];
        launchdDictionary[@"RunAtLoad"] = [NSNumber numberWithBool:TRUE];
        //[launchdDictionary setObject:[NSNumber numberWithInt:1] forKey:@"ExitTimeOut"]; // Send a KILL signal after 1 second
        launchdDictionary[@"StandardOutPath"] = logLocation;
        launchdDictionary[@"StandardErrorPath"] = logLocation;
        launchdDictionary[@"LimitLoadToSessionType"] = @[@"Aqua",@"LoginWindow"];

        // Write to file
        NSString *tempPath = [@"/tmp" stringByAppendingPathComponent:launchdPath.lastPathComponent];
        [launchdDictionary writeToFile:tempPath atomically:NO];
        success &= [myAuthorization executeCommand:@"/usr/sbin/chown"
                                          withArgs:@[@"-R", @"root:wheel", tempPath]];
        success &= [myAuthorization executeCommand:@"/bin/chmod"
                                          withArgs:@[@"-R", @"744", tempPath]];
        // Install to launchdPath
        success &= [myAuthorization executeCommand:@"/bin/mv"
                                          withArgs:@[@"-f", tempPath, launchdPath]];

        //Setup Log File (for multiple user access)
        success &= [myAuthorization executeCommand:@"/usr/bin/touch"
                                          withArgs:@[logLocation]];
        success &= [myAuthorization executeCommand:@"/usr/sbin/chown"
                                          withArgs:@[@"-R", @"root:wheel", logLocation]];
        success &= [myAuthorization executeCommand:@"/bin/chmod"
                                          withArgs:@[@"-R", @"666", logLocation]];


        // Launch Using launchctl
        success &= [myAuthorization executeCommand:@"/bin/launchctl"
                                          withArgs:@[@"load", @"-S", @"Aqua", launchdPath]];

        if (!success) {
            [startupItemStatusMessageField setStringValue:NSLocalizedString(@"Error: Unable To Setup Vine Server using launchd", nil)];
        }
    }

    return success;
}

- (void) installAsService {
    if (!myAuthorization)
        myAuthorization = [[NSAuthorization alloc] init];

    if (!myAuthorization) {
        [startupItemStatusMessageField setStringValue:NSLocalizedString(@"Error: No Authorization", nil)];
        return;
    }

    // Remove Old SystemServer
    [self removeService: self];
    systemServerIsConfigured = [self installLaunchd];

    [self loadUIForSystemServer];

    [myAuthorization release];
    myAuthorization = nil;
}

- (IBAction) installAsService: sender {
    // No password, so double check
    if (!passwordField.stringValue.length) {
        NSBeginAlertSheet(NSLocalizedString(@"System Server", nil),
                          NSLocalizedString(@"Cancel", nil),
                          NSLocalizedString(@"Start Server", nil),
                          nil, systemServerWindow, self, @selector(serviceSheetDidEnd:returnCode:contextInfo:),
                          NULL, NULL, @"%@",
                          NSLocalizedString(@"No password has been specified for the System Server.  The System Server will automatically launch every time your machine is restarted.  Are you sure that you want to install a System Server with no password", nil));
    }
    else {
        [self installAsService];
    }
}

- (void) serviceSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo {
    if (returnCode==NSAlertDefaultReturn)
        return;
    else
        [self installAsService];
}

- (IBAction) removeService: sender {
    BOOL success = TRUE;
    NSString *startupPath = [[NSUserDefaults standardUserDefaults] stringForKey:@"startupItemLocation"];
    NSString *launchdPath = [[NSUserDefaults standardUserDefaults] stringForKey:@"launchdItemLocation"];

    if (!myAuthorization)
        myAuthorization = [[NSAuthorization alloc] init];

    if (!myAuthorization) {
        [startupItemStatusMessageField setStringValue:NSLocalizedString(@"Error: No Authorization", nil)];
        return;
    }

    if ([[NSFileManager defaultManager] fileExistsAtPath: startupPath]) {
        // Kill any running system servers, necessary since old OSXvnc scripts don't work on Leopard
        success &= [myAuthorization executeCommand:[NSString stringWithFormat:@"%@/OSXvnc/OSXvnc", [NSBundle mainBundle].resourcePath]
                                          withArgs:@[@"stop"]];
        success &= [myAuthorization executeCommand:@"/bin/rm"
                                          withArgs:@[@"-r", @"-f", startupPath]];
    }
    if ([[NSFileManager defaultManager] fileExistsAtPath:launchdPath]) {
        success &= [myAuthorization executeCommand:@"/bin/launchctl"
                                          withArgs:@[@"unload", @"-S", @"Aqua", launchdPath]];
        success &= [myAuthorization executeCommand:@"/bin/rm"
                                          withArgs:@[@"-r", @"-f", launchdPath]];
    }

    if (success) {
        systemServerIsConfigured = FALSE;
        [self loadUIForSystemServer];
    }
    else {
        [startupItemStatusMessageField setStringValue:NSLocalizedString(@"Error: Unabled to remove startup item", nil)];
    }

    if (sender != self) {
        [myAuthorization release];
        myAuthorization = nil;
    }
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
