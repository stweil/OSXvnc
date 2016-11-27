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

    IBOutlet NSMenuItem *serverMenuItem;

    IBOutlet NSWindow *statusWindow;
    IBOutlet NSWindow *preferenceWindow;
    IBOutlet NSWindow *reverseConnectionWindow;

    // System Server UI
    IBOutlet NSMenuItem *systemServerMenu;
    IBOutlet NSWindow *systemServerWindow;

    IBOutlet NSTextField *systemServerDisplayNameField;
    IBOutlet NSPopUpButton *systemServerDisplayNumberField;
    IBOutlet NSTextField *systemServerPortField;

    IBOutlet NSMatrix *systemServerAuthenticationType;
    IBOutlet NSTextField *systemServerPasswordField;
    IBOutlet NSButton *systemServerLimitToLocalConnections;

    IBOutlet NSButton *disableStartupButton;
    IBOutlet NSButton *setStartupButton;

    IBOutlet NSTextField *startupItemStatusMessageField;

    // Control Panel
    IBOutlet NSBox *hostNamesBox;
    IBOutlet NSTextField *hostNamesField;
    IBOutlet NSBox *ipAddressesBox;
    IBOutlet NSMatrix *ipAddressesView;
    IBOutlet NSTextField *displayNumText;
    IBOutlet NSTextField *portNumText;

    IBOutlet NSTextView *statusMessageField;
    IBOutlet NSButton *startServerButton;
    IBOutlet NSButton *stopServerButton;

    // Preferences Interface
    IBOutlet NSTabView *optionsTabView;
    IBOutlet NSTextField *preferencesMessageTestField;

    // Connection
    IBOutlet NSTextField *displayNameField;
    IBOutlet NSPopUpButton *displayNumberField;
    IBOutlet NSTextField *portField;

    IBOutlet NSMatrix *authenticationType;
    IBOutlet NSTextField *passwordField;

    IBOutlet NSButton *limitToLocalConnections;

    // Device
    IBOutlet NSButton *allowSleepCheckbox;
    IBOutlet NSButton *allowDimmingCheckbox;
    IBOutlet NSButton *allowScreenSaverCheckbox;

    IBOutlet NSPopUpButton *keyboardLayout;

    IBOutlet NSButton *swapMouseButtonsCheckbox;

    // Advanced
    IBOutlet NSPopUpButton *protocolVersion;
    IBOutlet NSTextField *otherArguments;

    IBOutlet NSButton *openGLCheckbox;
    IBOutlet NSPopUpButton *keyboardEvents;
    IBOutlet NSPopUpButton *eventSourcePopup;

    // Sharing
    IBOutlet NSMatrix *sharingMatrix;
    IBOutlet NSButton *dontDisconnectCheckbox;
    IBOutlet NSButton *disableRemoteEventsCheckbox;
    IBOutlet NSButton *disableRichClipboardCheckbox;
    IBOutlet NSButton *allowRendezvousCheckbox;

    // Startup
    IBOutlet NSButton *startServerOnLaunchCheckbox;
    IBOutlet NSButton *terminateOnFastUserSwitch;
    IBOutlet NSButton *serverKeepAliveCheckbox;


    // Reverse Connections UI
    IBOutlet NSTextField *connectHost;
    IBOutlet NSTextField *connectPort;
    IBOutlet NSTextField *reverseConnectionMessageField;

    // Initial Settings UI
    IBOutlet NSWindow *initialWindow;
    IBOutlet NSTextField *initialPasswordText;
    IBOutlet NSMatrix *initialAuthenticationType;
    IBOutlet NSButton *initialDoneButton;

    //int port;
    BOOL systemServerIsConfigured;

    BOOL alwaysShared;
    BOOL neverShared;
    BOOL userStopped;
    BOOL relaunchServer;
    BOOL doNotLoadProxy;

    BOOL waitingForHostInfo;

    NSTask *controller;
    NSFileHandle *serverOutput;

    NSString *passwordFile;
    NSString *logFile;

    NSAuthorization *myAuthorization;

    NSDate *lastLaunchTime;
    NSMutableArray *bundleArray;

    NSArray *clientList;

    NSString *hostName;

    NSString *automaticReverseHost;
    NSString *automaticReversePort;
}

- (instancetype) init;

@property (NS_NONATOMIC_IOSONLY, readonly, strong) NSWindow *window;
@property (NS_NONATOMIC_IOSONLY, readonly) int runningPortNum;

- (void) awakeFromNib;

- (void) loadUIForSystemServer;
- (void) loadUIForPort: (NSInteger) port;

- (void) loadUserDefaults: sender;
- (void) saveUserDefaults: sender;

- (IBAction) validateInitialAuthentication: sender;
- (IBAction) setInitialAuthentication: sender;

- (NSMutableArray *) formCommandLineForSystemServer: (BOOL) isSystemServer;

- (IBAction) startServer: sender;
- (IBAction) stopServer: sender;
- (void) serverStopped: (NSNotification *) aNotification;

- (IBAction) changeDisplayNumber: sender;
- (IBAction) changePort: sender;
- (IBAction) changeSharing: sender;
- (IBAction) changeAuthenticationType: sender;
- (IBAction) changePassword: sender;
- (IBAction) changeDisplayName: sender;
- (IBAction) optionChanged: sender;

- (IBAction) changeSystemServerPort: sender;
- (IBAction) changeSystemServerAuthentication: sender;

- (IBAction) reverseConnection: sender;
- (IBAction) cancelConnectHost: sender;
- (IBAction) connectHost: sender;

- (void) checkForRestart;

- (void) applicationWillTerminate: (NSNotification *) notification;

- (BOOL) validateMenuItem: (NSMenuItem *) menuItem ;

// Menu Items
- (IBAction) openFirewall:(id) sender;
- (IBAction) openLog:(id) sender;
- (IBAction) openGPL:(id) sender;
- (IBAction) openReleaseNotes:(id) sender;
- (IBAction) openFile:(id) sender;

- (IBAction) installAsService: sender;
- (IBAction) removeService: sender;

- (IBAction) terminateRequest: sender;

@end
