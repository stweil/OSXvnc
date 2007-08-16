/* EchoController */

#import <Cocoa/Cocoa.h>

class CServerListSynchronize;
class CCritSection;
class CMyDllProxyInfo;

@interface EchoController : NSObject
{
	NSTabView *mainTabView;

    IBOutlet NSView *echoServersView;

	@public IBOutlet NSTableView *echoTableView;

    IBOutlet NSButton *addButton;
    IBOutlet NSButton *editButton;
    IBOutlet NSButton *removeButton;
    IBOutlet NSWindow *addServerWindow;
	IBOutlet NSWindow *editServerWindow;

	IBOutlet NSTextFieldCell *echoServerField;
	IBOutlet NSTextFieldCell *usernameField;
	IBOutlet NSTextFieldCell *passwordField;
	IBOutlet NSTextFieldCell *echoServerField_edit;
	IBOutlet NSTextFieldCell *usernameField_edit;
	IBOutlet NSTextFieldCell *passwordField_edit;
	
	IBOutlet NSButton *advancedSettingsButton;
	IBOutlet NSWindow *advancedSettingsWindow;
	
	IBOutlet NSButton *proxyAuthenticationCheckbox;
    IBOutlet NSTextField *proxyAddress;
    IBOutlet NSTextField *proxyPort;
    IBOutlet NSTextField *proxyUsername;
    IBOutlet NSTextField *proxyUsernameLabel;
    IBOutlet NSTextField *proxyPassword;
    IBOutlet NSTextField *proxyPasswordLabel;

    IBOutlet NSButton *disableEchoCheckbox;
    IBOutlet NSButton *enableLoggingCheckbox;
	//IBOutlet NSButton *useAESCheckbox; YS: 21.12.2006 due to remove "128 bit encryption" button

	IBOutlet NSTextField *versionTextField;

	@public NSMutableArray *echoServers;
	@public NSMutableArray *echoInfoProxys;
	@public NSMutableArray *echoInfoProxysToRemove;
	
	int m_nEditIndex;
	CServerListSynchronize *m_ServerList;
	CCritSection *m_critSection;
}

- (void) loadGUI: sender;
- (void) setOffloadingPort: (id)sender;

- (void) loadServerList;
- (void) loadProxyFields;

- (void) reloadData;
- (void) selectRow: (int)row;
- (void) removeRow: (int)row removeProxy: (bool)rmProxy removeServer: (bool)rmServer;
- (CMyDllProxyInfo*) getDllProxyInfo: (int)row;
- (void) addInfo: (CMyDllProxyInfo*)info;
- (void) saveData;

- (NSString*) GetColumnValue: (int)row column: (NSString*)col;

- (void)windowWillClose: (NSNotification*)aNotification;

- (IBAction)addServer:(id)sender;
- (IBAction)advancedSettings:(id)sender;
- (IBAction)cancelAddServer:(id)sender;
- (IBAction)cancelAdvancedSettings:(id)sender;
- (IBAction)completeAddServer:(id)sender;
- (IBAction)completeAdvancedSettings:(id)sender;
- (IBAction)disableEcho:(id)sender;
- (IBAction)enableLogging:(id)sender;
- (IBAction)removeServer:(id)sender;
- (IBAction)requireProxyAuthentication:(id)sender;
- (IBAction)useEncryption:(id)sender;
- (IBAction)editServer:(id)sender;
- (IBAction)cancelEditServer:(id)sender;
- (IBAction)completeEditServer:(id)sender;
@end
