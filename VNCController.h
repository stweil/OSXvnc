//
//  VNCController.h
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Fri Aug 02 2002.
//  Copyright (c) 2002 Redstone Software, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface VNCController : NSObject {
    IBOutlet NSMenuItem *startServerMenuItem;
    IBOutlet NSMenuItem *stopServerMenuItem;

    IBOutlet NSWindow *window;
    IBOutlet NSTabView *optionsTabView;

    IBOutlet NSPopUpButton *displayNumberField;
    IBOutlet NSTextField *portField;
    IBOutlet NSTextField *passwordField;
    IBOutlet NSTextField *displayNameField;
    IBOutlet NSButton *allowDimmingCheckbox;
    IBOutlet NSButton *allowSleepCheckbox;

    IBOutlet NSButton *allowKeyboardLoading;
    IBOutlet NSButton *allowPressModsForKeys;

    NSButton *showMouseButton;

    IBOutlet NSMatrix *sharingMatrix;
    IBOutlet NSButton *dontDisconnectCheckbox;
    IBOutlet NSButton *swapMouseButtonsCheckbox;
    IBOutlet NSButton *disableRemoteEventsCheckbox;
    IBOutlet NSButton *limitToLocalConnections;

    IBOutlet NSButton *startServerOnLaunchCheckbox;
    IBOutlet NSButton *serverKeepAliveCheckbox;

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
}

- init;
- (void) awakeFromNib;

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

- (void) checkForRestart;

- (void) applicationWillTerminate: (NSNotification *) notification;

- (BOOL) validateMenuItem: (NSMenuItem *) menuItem ;

- (IBAction) openLog:(id) sender;
- (IBAction) openFile:(id) sender;

- (IBAction) installAsService: sender;


@end
