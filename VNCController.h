//
//  VNCController.h
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Fri Aug 02 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface VNCController : NSObject {
    IBOutlet NSWindow *window;
    IBOutlet NSMenuItem *startServerMenuItem;
    IBOutlet NSMenuItem *stopServerMenuItem;

    IBOutlet NSPopUpButton *displayNumberField;
    IBOutlet NSTextField *portField;
    IBOutlet NSTextField *passwordField;
    IBOutlet NSTextField *displayNameField;
    IBOutlet NSButton *allowDimmingCheckbox;
    IBOutlet NSButton *allowSleepCheckbox;
    IBOutlet NSButton *startServerOnLaunchCheckbox;

    IBOutlet NSMenuItem *hideOrShowWindowMenuItem;
    IBOutlet NSMatrix *sharingMatrix;
    IBOutlet NSButton *dontDisconnectCheckbox;
    IBOutlet NSButton *swapMouseButtonsCheckbox;
    IBOutlet NSButton *disableRemoteEventsCheckbox;
    
    IBOutlet NSTextField *statusMessageField;
    IBOutlet NSButton *startServerButton;
    IBOutlet NSButton *stopServerButton;

    int displayNumber;
    int port;

    BOOL alwaysShared;
    BOOL neverShared;
    BOOL userStopped;

    NSTask *controller;
    NSFileHandle *serverOutput;

    NSString *passwordFile;
}

- init;
- (void) awakeFromNib;

- (void) loadUserDefaults: sender;
- (void) saveUserDefaults: sender;

- (NSArray *) formCommandLine;

- (void) startServer: sender;
- (void) stopServer: sender;
- (void) serverStopped: (NSNotification *) aNotification;

- (void) changeDisplayNumber: sender;
- (void) changePort: sender;
- (void) changeSharing: sender;
- (void) changePassword: sender;

- (void) disableEverything;
- (void) enableEverything;
- (void) showWindow;
- (void) hideWindow;
- (void) hideOrShowWindow: sender;

- (BOOL) windowShouldClose: sender;

- (void) applicationWillTerminate: (NSNotification *) notification;

- (BOOL) validateMenuItem: (NSMenuItem *) menuItem ;


@end
