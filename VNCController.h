//
//  VNCController.h
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Fri Aug 02 2002.
//  Copyright (c) 2002 Redstone Software, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import "NSAuthorization.h"

@interface VNCController : NSObject {
    IBOutlet NSMenuItem *startServerMenuItem;
    IBOutlet NSMenuItem *stopServerMenuItem;

    IBOutlet NSWindow *window;
    IBOutlet NSTabView *optionsTabView;

    IBOutlet NSPopUpButton *displayNumberField;
    IBOutlet NSTextField *portField;
    IBOutlet NSTextField *passwordField;
    IBOutlet NSTextField *displayNameField;

	IBOutlet NSTextField *connectHost;
    IBOutlet NSTextField *connectPort;
	
    IBOutlet NSTextField *hostNamesField;
    IBOutlet NSTextField *hostNamesLabel;
    IBOutlet NSTextField *ipAddressesLabel;
    IBOutlet NSTextField *ipAddressesField;
    
    IBOutlet NSButton *allowSleepCheckbox;
    IBOutlet NSButton *allowDimmingCheckbox;
    IBOutlet NSButton *allowScreenSaverCheckbox;

    IBOutlet NSPopUpButton *protocolVersion;
    IBOutlet NSTextField *otherArguments;

    IBOutlet NSButton *allowKeyboardLoading;
    IBOutlet NSButton *allowPressModsForKeys;

    NSButton *showMouseButton;

    IBOutlet NSMatrix *sharingMatrix;
    IBOutlet NSButton *dontDisconnectCheckbox;
    IBOutlet NSButton *swapMouseButtonsCheckbox;
    IBOutlet NSButton *disableRemoteEventsCheckbox;
    IBOutlet NSButton *limitToLocalConnections;
    IBOutlet NSButton *allowRendezvousCheckbox;

    IBOutlet NSButton *startServerOnLaunchCheckbox;
    IBOutlet NSButton *terminateOnFastUserSwitch;
    IBOutlet NSButton *serverKeepAliveCheckbox;

    IBOutlet NSButton *setStartupButton;
    IBOutlet NSButton *disableStartupButton;
    
    IBOutlet NSTextField *statusMessageField;
    IBOutlet NSButton *startServerButton;
    IBOutlet NSButton *stopServerButton;
    IBOutlet NSTextField *startupItemStatusMessageField;

    int port;

    BOOL alwaysShared;
    BOOL neverShared;
    BOOL userStopped;
    BOOL relaunchServer;

    NSTask *controller;
    NSFileHandle *serverOutput;
    
    NSString *passwordFile;
    NSString *logFile;
    
    NSAuthorization *myAuthorization;
    
    NSDate *lastLaunchTime;
}

- init;

- (NSWindow *) window;
- (int) runningPortNum;

- (void) awakeFromNib;

- (void) loadDynamicBundles;

- (void) loadUserDefaults: sender;
- (void) saveUserDefaults: sender;

- (NSArray *) formCommandLine;

- (IBAction) startServer: sender;
- (IBAction) stopServer: sender;
- (void) serverStopped: (NSNotification *) aNotification;

- (IBAction) changeDisplayNumber: sender;
- (IBAction) changePort: sender;
- (IBAction) changeSharing: sender;
- (IBAction) changePassword: sender;
- (IBAction) changeDisplayName: sender;
- (IBAction) optionChanged: sender;

- (IBAction) connectHost: sender;

- (void) checkForRestart;

- (void) applicationWillTerminate: (NSNotification *) notification;

- (BOOL) validateMenuItem: (NSMenuItem *) menuItem ;

- (IBAction) openLog:(id) sender;
- (IBAction) openGPL:(id) sender;
- (IBAction) openReleaseNotes:(id) sender;
- (IBAction) openFile:(id) sender;

- (IBAction) installAsService: sender;
- (IBAction) removeService: sender;

@end
