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
	
	IBOutlet NSWindow *statusWindow;
	IBOutlet NSWindow *preferenceWindow;
	IBOutlet NSWindow *reverseConnectionWindow;
	
	IBOutlet NSBox *hostNamesBox;
    IBOutlet NSTextField *hostNamesField;
    IBOutlet NSBox *ipAddressesBox;
    IBOutlet NSTextView *ipAddressesField;
	IBOutlet NSMatrix *ipAddressesView;
    IBOutlet NSTextField *displayNumText;
    IBOutlet NSTextField *portNumText;
	
	IBOutlet NSTextView *statusMessageField;
    IBOutlet NSButton *startServerButton;
    IBOutlet NSButton *stopServerButton;
	

	IBOutlet NSTabView *optionsTabView;
    IBOutlet NSTextField *preferencesMessageTestField;

    IBOutlet NSTextField *displayNameField;
    IBOutlet NSPopUpButton *displayNumberField;
    IBOutlet NSTextField *portField;
    IBOutlet NSTextField *passwordField;

	IBOutlet NSTextField *connectHost;
    IBOutlet NSTextField *connectPort;
	IBOutlet NSTextField *reverseConnectionMessageField;
	
    IBOutlet NSButton *allowSleepCheckbox;
    IBOutlet NSButton *allowDimmingCheckbox;
    IBOutlet NSButton *allowScreenSaverCheckbox;

    IBOutlet NSPopUpButton *protocolVersion;
    IBOutlet NSTextField *otherArguments;

    IBOutlet NSButton *allowKeyboardLoading;
    IBOutlet NSButton *allowPressModsForKeys;
	IBOutlet NSPopUpButton *keyboardLayout;
	IBOutlet NSPopUpButton *keyboardEvents;

    IBOutlet NSButton *showMouseButton;

    IBOutlet NSMatrix *sharingMatrix;
    IBOutlet NSButton *dontDisconnectCheckbox;
    IBOutlet NSButton *swapMouseButtonsCheckbox;
    IBOutlet NSButton *disableRemoteEventsCheckbox;
    IBOutlet NSButton *disableRichClipboardCheckbox;
    IBOutlet NSButton *limitToLocalConnections;
    IBOutlet NSButton *allowRendezvousCheckbox;

    IBOutlet NSButton *startServerOnLaunchCheckbox;
    IBOutlet NSButton *terminateOnFastUserSwitch;
    IBOutlet NSButton *serverKeepAliveCheckbox;

    IBOutlet NSButton *setStartupButton;
    IBOutlet NSButton *disableStartupButton;
    
    IBOutlet NSTextField *startupItemStatusMessageField;

    //int port;
	
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
	
	int activeConnectionsCount;
}

- init;

- (NSWindow *) window;
- (int) runningPortNum;

- (void) awakeFromNib;

- (void) loadDynamicBundles;

- (void) loadUserDefaults: sender;
- (void) saveUserDefaults: sender;

- (NSMutableArray *) formCommandLine;

- (IBAction) startServer: sender;
- (IBAction) stopServer: sender;
- (void) serverStopped: (NSNotification *) aNotification;

- (IBAction) changeDisplayNumber: sender;
- (IBAction) changePort: sender;
- (IBAction) changeSharing: sender;
- (IBAction) changePassword: sender;
- (IBAction) changeDisplayName: sender;
- (IBAction) optionChanged: sender;

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
