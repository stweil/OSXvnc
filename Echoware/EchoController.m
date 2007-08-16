
#import "EchoController.h"
#import "Echoware.h"
#import "InterfaceDLLProxyInfo.h"
#import "unistd.h"

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "User.h"
#include "ProxiesManager.h"
#include "MyDllProxyInfo.h"
#include "globals.h"
#include "ServerListSynchronize.h"
#include "CritSection.h"

static EchoController *sharedEchoController;

@interface NSString (EchoExtenstions)

- (NSData *) nullTerminatedData;

@end

@implementation NSString (EchoExtenstions)

- (NSData *) nullTerminatedData
{
	return [NSData dataWithBytes:[self lossyCString] length:[self cStringLength]+1];
}

@end

@implementation EchoController (RFBBundle)

+ (void) loadGUI
{
	sharedEchoController = [[self alloc] init];
}

+ (void) rfbUsage
{
}

#include "rfbserver.h"
#include "RFBBundleWrapper.h"

+ (void) rfbStartup: theServer
{
	CRFBBundleWrapper::GetInstance()->rfb_port = ((rfbserver*)theServer)->rfbPort;
	CRFBBundleWrapper::GetInstance()->is_rfb = true;
	CRFBBundleWrapper::GetInstance()->UserDefaultsChecking();
	CRFBBundleWrapper::GetInstance()->startRunLoop();
}

+ (void) rfbRunning
{
}

+ (void) rfbPoll
{
}

+ (void) rfbReceivedClientMessage
{
}

+ (void) rfbShutdown
{
	CRFBBundleWrapper::FreeInstance();
}

@end

@implementation EchoController

- init
{
	self = [super init];
	
	if (self)
	{
		echoInfoProxys = [[NSMutableArray alloc] init];
		echoInfoProxysToRemove = [[NSMutableArray alloc] init];
		m_nEditIndex = -1;

		m_critSection = new CCritSection();
		m_ServerList = new CServerListSynchronize();

		[self loadGUI: self];

		NSLog(@"Starting Threads...");
		m_ServerList->Init();
		m_ServerList->Start(self);
		NSLog(@"Starting Threads done...");
	}
}

- (void)windowWillClose: (NSNotification*)aNotification;
{
	NSLog(@"Terminating Threads...");
	if (m_ServerList)
		m_ServerList->Terminate();
	NSLog(@"Terminating Threads done...");
	
	NSLog(@"Disconnecting All Connections...");
	bool disconnected = DisconnectAllProxies();
	NSLog(@"Disconnecting All Connections done... %d", disconnected);
	if (m_ServerList)
		delete m_ServerList;
	m_ServerList = NULL;
	
	if (m_critSection)
		delete m_critSection;
	m_critSection = NULL;
	
	[echoInfoProxys removeAllObjects];
	[echoInfoProxysToRemove removeAllObjects];
	
	[echoInfoProxys release];
	[echoInfoProxysToRemove release];
	
	[echoServers removeAllObjects];
	[echoServers release];

	m_nEditIndex = -1;
}

- (void) loadGUI: sender
{
	NSEnumerator *viewEnum = [[[[[NSApp delegate] window] contentView] subviews] objectEnumerator];
	
	while (mainTabView = [viewEnum nextObject])
		if ([mainTabView isKindOfClass:[NSTabView class]])
			break;

	if (mainTabView)
	{
		if ([NSBundle loadNibNamed:@"EchoServers" owner:self])
			NSLog(@"EchoServer Gui loaded");
		else
			NSLog(@"EchoServer Gui loading Error %d", errno);
			
		NSRect rect = [[mainTabView window] frame];
		rect.size.width += 80;
		[[mainTabView window] setFrame: rect display: YES];
	}
	else
	{ // Sometimes we aren't able to get the main window, let's try again
		[self performSelector:@selector(loadGUI:) withObject:self afterDelay:1.0];
	}
}

- (BOOL) canWriteToFile: (NSString *) path
{
    if ([[NSFileManager defaultManager] fileExistsAtPath:path])
        return [[NSFileManager defaultManager] isWritableFileAtPath:path];
    else
        return [[NSFileManager defaultManager] isWritableFileAtPath:[path stringByDeletingLastPathComponent]];
}

- (void) setOffloadingPort: (id)sender
{
	SetPortForOffLoadingData((int) [[NSApp delegate] runningPortNum]);
}

- (void) awakeFromNib
{
	if (InitializeProxyDll())
	{
		NSTabViewItem *newTab = [[NSTabViewItem alloc] initWithIdentifier:@"Echo"];
		
		//SetLoggingOptions(TRUE, "/tmp/EchoServer.log");
		//YS: choose log file, where we can write
		NSArray *logFiles = [NSArray arrayWithObjects:
			[[NSUserDefaults standardUserDefaults] stringForKey:@"LogFile"],
			@"/var/log/OSXvnc-server.log",
			@"~/Library/Logs/OSXvnc-server.log",
			[[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"OSXvnc-server.log"],
			@"/tmp/OSXvnc-server.log",
			nil];
		NSEnumerator *logEnumerators = [logFiles objectEnumerator];
		// Find first writable location for the log file
		NSString *logFile = nil;
		while (logFile = [logEnumerators nextObject])
		{
			logFile = [logFile stringByStandardizingPath];
			if ([logFile length] && [self canWriteToFile:logFile])
			{
				[logFile retain];
				break;
			}
		}
		//
		
		//converto from NSString to char*
		int len_logFile = [logFile length];
		char pLogFile[len_logFile + 1];
		strncpy(pLogFile, [logFile cString], len_logFile);
		pLogFile[len_logFile] = '\0';
		//
		
		bool enableLog = [[NSUserDefaults standardUserDefaults] boolForKey:@"EnableLogging"];
		[enableLoggingCheckbox setState: enableLog];
		SetLoggingOptions(enableLog, pLogFile);
		
		SetPortForOffLoadingData((int) [[NSApp delegate] runningPortNum]);

		[disableEchoCheckbox setState: [[NSUserDefaults standardUserDefaults] boolForKey:@"EchoDisabled"]];
		[self disableEcho:self];
		//YS: 21.12.2006 due to remove "128 bit encryption" button
		//[useAESCheckbox setState: [[NSUserDefaults standardUserDefaults] boolForKey:@"EchoAESEncryption"]];
		
		[self loadProxyFields];
		
		[newTab setLabel:@"echoServer"];
		[mainTabView addTabViewItem:newTab];
		[newTab setView:echoServersView];
		
		NSString *first = [NSString stringWithCString: GetDllVersion()];
		NSString *second = [NSString stringWithFormat: @"EchoWare version %@", first];
		[versionTextField setStringValue: second];
		NSLog(second);
		
		[self loadServerList];
		[self saveData];
		
		[echoTableView setTarget:self];
		[echoTableView setDoubleAction:@selector(editServer:)];
	}
}

- (void) loadProxyFields
{
	// The EchoDLL expects us to manage memory on these strings that are passed in so we'll make some NSData objects
	static id proxyStringsDictionary = nil;
	
	NSString *proxyAddrString = [[NSUserDefaults standardUserDefaults] stringForKey:@"EchoProxyAddr"];
	NSString *proxyPortString = [[NSUserDefaults standardUserDefaults] stringForKey:@"EchoProxyPort"];
	NSString *proxyUserString = [[NSUserDefaults standardUserDefaults] stringForKey:@"EchoProxyUser"];
	NSString *proxyPassString = [[NSUserDefaults standardUserDefaults] stringForKey:@"EchoProxyPass"];
	
	/* Load To GUI */
	if (proxyAddrString)
		[proxyAddress setStringValue: proxyAddrString];
	if ([proxyPortString intValue])
		[proxyPort setIntValue: [proxyPortString intValue]];

	if (proxyUserString)
	{
		[proxyUsername setStringValue: proxyUserString];
		if (proxyPassString)
			[proxyPassword setStringValue: proxyPassString];
		else
			proxyPassString = @"";

		[proxyAuthenticationCheckbox setState: TRUE];
	}
	else
	{
		proxyUserString = @"";
		proxyPassString = @"";
	}
	
	[self requireProxyAuthentication:self];
	
	// Load to EchoWare
	if ([proxyAddrString length] && [proxyPortString intValue])
	{
		[proxyStringsDictionary release];
		proxyStringsDictionary = [[NSMutableDictionary alloc] init];
		
		[proxyStringsDictionary setObject:[proxyAddrString nullTerminatedData] forKey:@"EchoProxyAddr"];
		[proxyStringsDictionary setObject:[proxyPortString nullTerminatedData] forKey:@"EchoProxyPort"];
		if (proxyUserString)
			[proxyStringsDictionary setObject:[proxyUserString nullTerminatedData] forKey:@"EchoProxyUser"];
		else
			[proxyStringsDictionary setObject:[@"" nullTerminatedData] forKey:@"EchoProxyUser"];
			
		if (proxyPassString)
			[proxyStringsDictionary setObject:[proxyPassString nullTerminatedData] forKey:@"EchoProxyPass"];
		else
			[proxyStringsDictionary setObject:[@"" nullTerminatedData] forKey:@"EchoProxyPass"];
		
		SetLocalProxyInfo((char *) [[proxyStringsDictionary objectForKey:@"EchoProxyAddr"] bytes], 
						  (char *) [[proxyStringsDictionary objectForKey:@"EchoProxyPort"] bytes], 
						  (char *) [[proxyStringsDictionary objectForKey:@"EchoProxyUser"] bytes], 
						  (char *) [[proxyStringsDictionary objectForKey:@"EchoProxyPass"] bytes]);	
	}
}

- (void) createEchoServerFromDictionary: (NSMutableDictionary *) echoDict
{
	IDllProxyInfo* proxyInfo = (IDllProxyInfo*)CreateProxyInfoClassObject();
	
	[echoDict setObject:[[[echoDict objectForKey:@"IPAddress"] nullTerminatedData] retain] forKey:@"IPAddress_cStringData"];
	proxyInfo->SetIP((const char *)[[echoDict objectForKey:@"IPAddress_cStringData"] bytes]);

	if ([echoDict objectForKey:@"Port"])
	{
		[echoDict setObject:[[[echoDict objectForKey:@"Port"] nullTerminatedData] retain] forKey:@"Port_cStringData"];
		proxyInfo->SetPort((const char *)[[echoDict objectForKey:@"Port_cStringData"] bytes]);
	}
	else
		proxyInfo->SetPort("1328");
	
	if ([echoDict objectForKey:@"User"])
	{
		[echoDict setObject:[[[echoDict objectForKey:@"User"] nullTerminatedData] retain] forKey:@"User_cStringData"];
		proxyInfo->SetMyID((char *)[[echoDict objectForKey:@"User_cStringData"] bytes]);
	}
	if ([echoDict objectForKey:@"Pass"])
	{
		[echoDict setObject:[[[echoDict objectForKey:@"Pass"] nullTerminatedData] retain] forKey:@"Pass_cStringData"];
		proxyInfo->SetPassword((const char *)[[echoDict objectForKey:@"Pass_cStringData"] bytes]);
	}
		
	//YS: 21.12.2006 due to remove "128 bit encryption" button
	//old code: SetEncryptionLevel([useAESCheckbox state], proxyInfo);
	SetEncryptionLevel(1, proxyInfo);//nnn
	
	CMyDllProxyInfo *pMyProxyInfo = new CMyDllProxyInfo(proxyInfo);
	pMyProxyInfo->setStatus(CMyDllProxyInfo::Connecting);
	
	[self addInfo: pMyProxyInfo];
	[echoTableView displayRect:[echoTableView rectOfRow:[echoInfoProxys count]]];
	[echoTableView setNeedsDisplay:YES];
	[echoTableView displayIfNeeded];
}

- (void) loadServerList
{
	NSUserDefaults *uDef = [NSUserDefaults standardUserDefaults];
	NSEnumerator *echoEnum = [[uDef objectForKey:@"EchoServers"] objectEnumerator];
	NSMutableDictionary *anEchoServer = nil;
	
	[echoServers release];
	echoServers = [[NSMutableArray alloc] init];
	
	while (anEchoServer = [[[echoEnum nextObject] mutableCopy] autorelease])
	{
		[echoServers addObject: anEchoServer];
		[self createEchoServerFromDictionary: anEchoServer];
	}
	[uDef setObject:echoServers forKey: @"EchoServers"];
}

#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>

-(NSString*) GetComputerName
{
	CFStringRef name = NULL;
	NSString *computerName = NULL;
	name = SCDynamicStoreCopyComputerName(NULL, NULL);
	if (name != NULL)
		computerName = [NSString stringWithString:(NSString*)name];
	CFRelease(name);
	return computerName;
}

- (IBAction)addServer:(id)sender
{
//YS: 21.12.2006 due to set by default all edits on add server screen
{
	NSString *server_default = @"demo.echovnc.com";
	NSString *password_default = @"demo2007";
	NSString *username_default = [self GetComputerName];
	if (username_default == NULL)
		username_default = @"VineServerDemo";
		
	[echoServerField setStringValue: server_default];
	[usernameField setStringValue: username_default];
	[passwordField setStringValue: password_default];
}	
	[NSApp beginSheet:addServerWindow
	   modalForWindow:[echoServersView window] 
		modalDelegate:self 
	   didEndSelector:NULL 
		  contextInfo:NULL];
}

- (IBAction)cancelAddServer:(id)sender
{
	[NSApp endSheet:addServerWindow returnCode:NSAlertAlternateReturn];
	[addServerWindow orderOut:self];
}

- (IBAction)completeAddServer:(id)sender
{
	NSMutableDictionary *newEchoServer = [[NSMutableDictionary alloc] init];
	
	[addServerWindow makeFirstResponder:[addServerWindow nextResponder]];
	
	NSArray *serverAndPort = [[echoServerField stringValue] componentsSeparatedByString:@":"];
	
	if ([[serverAndPort objectAtIndex:0] length])
	{
		[newEchoServer setObject:[serverAndPort objectAtIndex:0] forKey:@"IPAddress"];
		if ([serverAndPort count] > 1)
			[newEchoServer setObject:[serverAndPort objectAtIndex:1] forKey:@"Port"];
		if ([[usernameField stringValue] length])
		{
			NSString * tmp = [usernameField stringValue];
			tmp = [tmp stringByAppendingString: @":vnc"];
			[newEchoServer setObject:tmp forKey:@"User"];
		}
		if ([[passwordField stringValue] length])
			[newEchoServer setObject:[passwordField stringValue] forKey:@"Pass"];

		[echoServers addObject:newEchoServer];
	}
	
	[NSApp endSheet:addServerWindow returnCode:NSAlertDefaultReturn];
	[addServerWindow orderOut:self];

	[self createEchoServerFromDictionary: newEchoServer];
	[self reloadData];
	[self selectRow: [echoServers count] - 1];
	[self saveData];
}

- (IBAction)removeServer:(id)sender
{
	int index = [echoTableView selectedRow];
	if (index >= 0)
	{
		CMyDllProxyInfo *pMyProxyInfo = [self getDllProxyInfo: index];
		if (pMyProxyInfo == NULL)
			return;
		if (pMyProxyInfo->getStatus() == CMyDllProxyInfo::Removing)
			return;

		[self removeRow: index removeProxy: true removeServer: true];
		[self reloadData];
		if (index >= [echoServers count])
			index = [echoServers count] - 1;
		[self selectRow: index];
		[self saveData];

		pMyProxyInfo->setStatus(CMyDllProxyInfo::Removing);
		[echoInfoProxysToRemove addObject: [NSValue valueWithPointer: pMyProxyInfo]];
	}
}

- (IBAction)advancedSettings:(id)sender
{
	[NSApp beginSheet:advancedSettingsWindow
	  modalForWindow:[echoServersView window] 
	   modalDelegate:self 
	  didEndSelector:NULL 
		 contextInfo:NULL];
}

- (IBAction)cancelAdvancedSettings:(id)sender
{
	[NSApp endSheet:advancedSettingsWindow returnCode:NSAlertAlternateReturn];
	[advancedSettingsWindow orderOut:self];
	[self loadProxyFields];
}

- (IBAction)completeAdvancedSettings:(id)sender
{
	[[NSUserDefaults standardUserDefaults] setObject:[proxyAddress stringValue] forKey:@"EchoProxyAddr"];
	[[NSUserDefaults standardUserDefaults] setInteger:[proxyPort intValue]  forKey:@"EchoProxyPort"];
	
	if ([proxyAuthenticationCheckbox state])
	{
		[[NSUserDefaults standardUserDefaults] setObject:[proxyUsername stringValue] forKey:@"EchoProxyUser"];
		[[NSUserDefaults standardUserDefaults] setObject:[proxyPassword stringValue] forKey:@"EchoProxyPass"];
	}
	else
	{
		[[NSUserDefaults standardUserDefaults] removeObjectForKey:@"EchoProxyUser"];
		[[NSUserDefaults standardUserDefaults] removeObjectForKey:@"EchoProxyPass"];
	}

	[[NSUserDefaults standardUserDefaults] synchronize];
	[self loadProxyFields];
	
	[NSApp endSheet:advancedSettingsWindow returnCode:NSAlertDefaultReturn];
	[advancedSettingsWindow orderOut:self];
}

- (IBAction)requireProxyAuthentication:(id)sender
{
	BOOL enabled = [proxyAuthenticationCheckbox state];
	
	[proxyUsername setEnabled:enabled];
	[proxyPassword setEnabled:enabled];
	[proxyUsernameLabel setTextColor:(enabled ? [NSColor blackColor] : [NSColor grayColor])];
	[proxyPasswordLabel setTextColor:(enabled ? [NSColor blackColor] : [NSColor grayColor])];
}

- (IBAction)disableEcho:(id)sender
{
	BOOL disabled = [disableEchoCheckbox state];
	
	[addButton setEnabled:!disabled];
	[advancedSettingsButton setEnabled:!disabled];
	[echoTableView setEnabled:!disabled];
	[removeButton setEnabled:!disabled];
	
	//YS: 21.12.2006 due to remove "128 bit encryption" button
	//[useAESCheckbox setEnabled:!disabled];
	
	[[NSUserDefaults standardUserDefaults] setBool:disabled forKey:@"EchoDisabled"];
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)enableLogging:(id)sender
{
	bool enabled = [enableLoggingCheckbox state];
	
	EnableLogging(enabled);

	[[NSUserDefaults standardUserDefaults] setBool: enabled forKey: @"EnableLogging"];
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)useEncryption:(id)sender
{
	id proxyEnum = [echoInfoProxys objectEnumerator];
	NSValue *aProxyValue = nil;
	
	while (aProxyValue = [proxyEnum nextObject])
	{
		//YS: 21.12.2006 due to remove "128 bit encryption" button
		//old code: SetEncryptionLevel([useAESCheckbox state], [aProxyValue pointerValue]);
		SetEncryptionLevel(1, [aProxyValue pointerValue]);
	}
	
	//YS: 21.12.2006 due to remove "128 bit encryption" button
	//[[NSUserDefaults standardUserDefaults] setBool:[useAESCheckbox state] forKey:@"EchoAESEncryption"];
	//[[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)editServer:(id)sender
{
	m_nEditIndex = [echoTableView selectedRow];
	if (m_nEditIndex >= 0)
	{
		CMyDllProxyInfo *pMyProxyInfo = [self getDllProxyInfo: m_nEditIndex];
		if (pMyProxyInfo == NULL)
			return;
		IDllProxyInfo *echoProxyInfo = pMyProxyInfo->getDllProxyInfo();
		if (pMyProxyInfo->getStatus() == CMyDllProxyInfo::Removing
		 || pMyProxyInfo->getStatus() == CMyDllProxyInfo::Connecting
		 || pMyProxyInfo->getStatus() == CMyDllProxyInfo::Reconnecting)
		 	return;
			
		int nLen = strlen(echoProxyInfo->GetMyID());
		char* sTmp = new char[nLen + 1];
		memset(sTmp, 0, nLen + 1);
		memcpy(sTmp, echoProxyInfo->GetMyID(), nLen);
		char* p = strstr(sTmp, ":");
		if (p != NULL)
			*p = 0;
		NSString *username = [NSString stringWithCString:sTmp];
		delete sTmp;
		NSString *password =[NSString stringWithCString: echoProxyInfo->GetPassword()];
		NSString *server = [NSString stringWithCString: echoProxyInfo->GetIpPort()];
		
		[echoServerField_edit setStringValue: server];
		[usernameField_edit setStringValue: username];
		[passwordField_edit setStringValue: password];
		
		[NSApp beginSheet:editServerWindow
			modalForWindow:[echoServersView window] 
			modalDelegate:self 
			didEndSelector:NULL 
			contextInfo:NULL];
	}
}

- (IBAction)cancelEditServer:(id)sender
{
	[NSApp endSheet:editServerWindow returnCode:NSAlertAlternateReturn];
	[editServerWindow orderOut:self];
}

- (IBAction)completeEditServer:(id)sender
{
	[editServerWindow makeFirstResponder:[editServerWindow nextResponder]];
	[NSApp endSheet:editServerWindow returnCode:NSAlertDefaultReturn];
	[editServerWindow orderOut:self];

	if (m_nEditIndex >= 0)
	{
		CMyDllProxyInfo *pMyProxyInfo = [self getDllProxyInfo: m_nEditIndex];
		if (pMyProxyInfo == NULL) return;
		IDllProxyInfo *echoProxyInfo = pMyProxyInfo->getDllProxyInfo();

		NSString *username = [usernameField_edit stringValue];
		username = [username stringByAppendingString: @":vnc"];
		NSString *password = [passwordField_edit stringValue];

		int len_username = [username length];
		int len_password = [password length]; 
		char pUsername[len_username + 1];
		char pPassword[len_password + 1];
		strncpy(pUsername, [username cString], len_username);
		strncpy(pPassword, [password cString], len_password);
		pUsername[len_username] = 0;
		pPassword[len_password] = 0;

		echoProxyInfo->SetMyID(pUsername);
		echoProxyInfo->SetPassword(pPassword);

		NSMutableDictionary *echoDict = [[[echoServers objectAtIndex: m_nEditIndex] mutableCopy] autorelease];

		[echoDict setObject: username forKey: @"User"];
		[echoDict setObject: password forKey: @"Pass"];
		[echoDict setObject: [[username nullTerminatedData] retain] forKey:@"User_cStringData"];
		[echoDict setObject: [[password nullTerminatedData] retain] forKey:@"Pass_cStringData"];

		[echoServers replaceObjectAtIndex: m_nEditIndex withObject: echoDict];

		pMyProxyInfo->setStatus(CMyDllProxyInfo::Reconnecting);
		[self reloadData];
		[self selectRow: m_nEditIndex];
		[self saveData];
	}
}

- (void) reloadData
{
	m_critSection->Lock();
	[echoTableView reloadData];
	m_critSection->Unlock();
}

- (void) selectRow: (int)row;
{
	m_critSection->Lock();
	[echoTableView selectRow: row byExtendingSelection: false];
	m_critSection->Unlock();
}

- (void) removeRow: (int)row removeProxy: (bool)rmProxy removeServer: (bool)rmServer;
{
	m_critSection->Lock();
	if (rmProxy)
		[echoInfoProxys removeObjectAtIndex: row];
	if (rmServer)
		[echoServers removeObjectAtIndex: row];
	m_critSection->Unlock();
}

- (CMyDllProxyInfo*) getDllProxyInfo: (int)row
{
	CMyDllProxyInfo *res = NULL;
	m_critSection->Lock();
	if (row >= 0 && row < [echoInfoProxys count])
		res = (CMyDllProxyInfo*)[[echoInfoProxys objectAtIndex: row] pointerValue];
	m_critSection->Unlock();
	return res;
}

- (void) addInfo: (CMyDllProxyInfo*)info
{
	m_critSection->Lock();
	[echoInfoProxys	addObject: [NSValue valueWithPointer: info]];
	m_critSection->Unlock();
}

- (void) saveData
{
	m_critSection->Lock();
	
	NSUserDefaults *suDefaults = [NSUserDefaults standardUserDefaults];
	[suDefaults setObject:echoServers forKey:@"EchoServers"];
	[suDefaults synchronize];

	m_critSection->Unlock();
}

- (NSString*) GetColumnValue: (int)row column: (NSString*)col
{
	NSString* res = @"";
	if (col && row >= 0)
	{
		NSDictionary *echoServer = [echoServers objectAtIndex: row];
		CMyDllProxyInfo *pMyProxyInfo = (CMyDllProxyInfo *)[[echoInfoProxys objectAtIndex: row] pointerValue];
		IDllProxyInfo *proxyInfo = pMyProxyInfo->getDllProxyInfo();

		if ([col isEqualToString:@"echoServer"])
		{
			if (proxyInfo->GetName() != nil)
				res = [NSString stringWithCString:proxyInfo->GetName() encoding:NSUTF8StringEncoding];
			else
				res = @"<Unknown>";
		}
		else if ([col isEqualToString:@"IP"])
		{
			if ([echoServer objectForKey:@"Port"])
				res = [NSString stringWithFormat:@"%@:%@", [echoServer objectForKey:@"IPAddress"], [echoServer objectForKey:@"Port"]];
			else
				res = [NSString stringWithFormat:@"%@", [echoServer objectForKey:@"IPAddress"]];	
		}
		else if ([col isEqualToString:@"Status"])
		{
			res = [NSString stringWithCString: pMyProxyInfo->getStatusString()];
		}
		else if ([col isEqualToString:@"User"])
		{
			int nLen = strlen(proxyInfo->GetMyID());
			char* sTmp = new char[nLen + 1];
			memset(sTmp, 0, nLen + 1);
			memcpy(sTmp, proxyInfo->GetMyID(), nLen);
			char* p = strstr(sTmp, ":");
			if (p != NULL)
				*p = 0;
			res = [NSString stringWithCString:sTmp encoding:NSUTF8StringEncoding];
			delete sTmp;
		}
	} 
	return res;
}
@end

@implementation EchoController (TableDataSource)

- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
	return [echoServers count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
	return [self GetColumnValue: row column: [tableColumn identifier]];
}

@end
