/*
 *  RFBBundleWrapper.cpp
 *  Echoware
 *
 *  Created by admin on 4/11/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "RFBBundleWrapper.h"
#include "globals.h"
//#include "ProxiesManager.h"
#import "Echoware.h"
#import "InterfaceDLLProxyInfo.h"
#import "User.h"
#include "EchoToOSX.h"

@interface NSString (EchoExtenstions2)

- (NSData *) nullTerminatedData;

@end

@implementation NSString (EchoExtenstions2)

- (NSData *) nullTerminatedData
{
	return [NSData dataWithBytes:[self lossyCString] length:[self cStringLength]+1];
}

@end

typedef void (*LoginNotificationCallBack)  (CFStringRef UserName, uid_t UID, gid_t GID);
typedef void (*LogoutNotificationCallBack) ();

struct LoginLogoutExtraInfoStructure
{
    LoginNotificationCallBack  loginFunctionToCall;
    LogoutNotificationCallBack logoutFunctionToCall;
};

static void LoginFunction(CFStringRef UserName, uid_t UID, gid_t GID)
{
	CRFBBundleWrapper::GetInstance()->UserDefaultsChecking();
}

static void LogoutFunction()
{
}

static void LoginLogoutProxyCallBackFunction(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info)
{
    #pragma unused (changedKeys)

    CFStringRef  consoleUserName;
    uid_t        consoleUserUID;
    gid_t        consoleUserGID;

    struct LoginLogoutExtraInfoStructure* loginLogoutCallbackInfo = (struct LoginLogoutExtraInfoStructure*) info;
    LoginNotificationCallBack loginFunctionToCall = loginLogoutCallbackInfo->loginFunctionToCall;
    LogoutNotificationCallBack logoutFunctionToCall = loginLogoutCallbackInfo->logoutFunctionToCall;

    consoleUserName = SCDynamicStoreCopyConsoleUser(store, &consoleUserUID, &consoleUserGID);

    if (consoleUserName != NULL) //Now if the username is non-null then we know that someone just logged in.  
								 //In this case call the login notification function.
    {
		if (loginFunctionToCall)
			loginFunctionToCall(consoleUserName, consoleUserUID, consoleUserGID);

        CFRelease(consoleUserName);
	}
    else						//Console username is null which means logout just occurred.
								//Thus call the logout function instead.
    {
		if (logoutFunctionToCall)
			logoutFunctionToCall();
    }
}

static int InstallLoginLogoutNotifiers(
			const CFStringRef StringDescribingYourApplication,
			const LoginNotificationCallBack YourLoginNotificationFunction,
			const LogoutNotificationCallBack YourLogoutNotificationFunction,
			CFRunLoopSourceRef* RunloopSourceReturned)
{
    CFStringRef SCDebuggingString = NULL;
    SCDynamicStoreContext DynamicStoreContext =
                                    { 0, NULL, NULL, NULL, NULL };
    SCDynamicStoreRef DynamicStoreCommunicationMechanism = NULL;
    CFStringRef KeyRepresentingConsoleUserNameChange = NULL;
    CFMutableArrayRef ArrayOfNotificationKeys;
    Boolean Result;
    struct LoginLogoutExtraInfoStructure* LoginLogoutCallBackFunctionInfo;

    // --- Setting return variables to known values --- */
    *RunloopSourceReturned = NULL;

    // --- Checking input arguments to ensure they are valid --- //
    //Checking the login and logout functions passed to make sure they
    //are valid (i.e. not null).
    if ((YourLoginNotificationFunction == NULL) ||
        (YourLogoutNotificationFunction == NULL))
    {
        return(-1); //Invalid arguments passed.  Returning error
    }

    /* The StringDescribingYourApplication is actually just a
     * System Configuration framework debugging string.
     * The string does not need to be unique or even present.  However, if
     * NULL is passed to this function
     * we will insert empty string for the debugging string.
     */
    if (StringDescribingYourApplication == NULL)
    {
        //Since passed null as the debugging string we assume an
        //'empty' string for the debugging string.
        SCDebuggingString = CFSTR("");
    }
    else
    {
        /* Since a actual string was passed we will use it as the debugging
         * string.  We create copy of the string passed in using
         * CFStringCreateCopy.
         * First Argument: Allocator to use.  We want
         *     default as usual so pass null.
         * Second Argument: The string to be duplicated.  In this case we
         *     use the string passed in.
         * Return Value: The new copy of the CFString in the
         *     second argument
         */
        SCDebuggingString = CFStringCreateCopy(NULL,
                    StringDescribingYourApplication);
    }

    if (SCDebuggingString == NULL)
    {
        //Since error creating debugging string we assume an
        //'empty' string for the debugging string.
        SCDebuggingString = CFSTR("");
    }

    // --- Creating Dynamic Store Context --- //

    /* Before we create the DynanicStoreCommunicationMechanism we will create
     * the context information.  The context information we will store
     * and will be passed to the LoginLogoutCallbackFunction is the login
     * and logout callbacks which it should call.  We put this information
     * in a SCDynamicStoreContext variable which will be added to the new
     * dynamic store.  First step is to allocate the strucutre to put in the
     * dynamicStoreContext.
     * Note that we can't deallocate this since it will be pointed to from
     * the dynamic store itself and used in subsiquent callbacks.
     */
     LoginLogoutCallBackFunctionInfo =
                    (LoginLogoutExtraInfoStructure*)malloc(sizeof(struct LoginLogoutExtraInfoStructure));

     if (LoginLogoutCallBackFunctionInfo == NULL)
     {
        //if were unable to allocate strucutre to hold login/logout callback
        //information then give up here.
        return(-3);
     }

    /* Now adding the callback information passed to this function to the
     * "LoginLogoutExtraInfoStructure".  This will then be added to the
     * dynamic store context.
     */

    LoginLogoutCallBackFunctionInfo->loginFunctionToCall =
                                   YourLoginNotificationFunction;


    LoginLogoutCallBackFunctionInfo->logoutFunctionToCall =
                                   YourLogoutNotificationFunction;


    /* Now adding the allocated structure to the dynamic store context.  This
     * context will be added to the dynamic store when it is created
     */

    DynamicStoreContext.info = (void*) LoginLogoutCallBackFunctionInfo;

    // --- Setting up notification with SystemConfiguration framework --- //

    /* The next step is to set up the notification with the
     * system configuration framework.  The first step is to create
     * a dynamic store.  The dynamic store is used to communicate with the
     * system configuration framework.  Create dynamic store with a
     * SCDynamicStoreCreate call.
     *
     * First Argument: Allocator to use.  As usual we want default
     *     allocator so pass null.
     * Second Argument: Before calling this must be a CFString to use as
     *     the debugging string in a debugging session.  This will be the
     *     same string passed into this function to describe the
     *     application or just an empty string.
     * Third Argument: This is the function which will be called whenever
     *     a key in the dynamic store changes.  We have a proxy function
     *     which will get called whenever the console username changes.
     *     The name change will happen when a new user logs in or out.
     *     Thus the proxy function gets called any login or logout.  The
     *     proxy function will in turn call the given Login/Logout
     *     functions passed to this function.
     * Forth Argument: The dynamic store context information to be used
     *        when creating the dynamic store.
     * Return Value: A mechanism used to comminicate with the dynamic
     * store and the system configuration framework.  This will be used
     * in later System Configuration calls.  Note this value must be
     * released later with CFRelease.
     */
    DynamicStoreCommunicationMechanism = SCDynamicStoreCreate
        (NULL, SCDebuggingString, LoginLogoutProxyCallBackFunction,
        &DynamicStoreContext);
        //After creating the dynamic store we are done with the
        //debugging string.  Thus, releasing now.
        CFRelease(SCDebuggingString);

    if (DynamicStoreCommunicationMechanism == NULL)
    {
        //if DynamicStoreCommunicationMechanism is null then
        //were unable to create communication mechanism.  Fail here.
        return(-2); //unable to create dynamic store.
    }

    /* Now we have a dynamic store communication mechanism we will need
     * to get the keys with which we want to get notified upon.  There is
     * actually only one key in this case which is the console username.
     * We get the console username key using the call
     * SCDynamicStoreKeyCreateConsoleUser.
     *
     * Return Value: A key representing we want to be notified of
     *     console user name changes.
     *        Note that the return value has to be released.
     */
    KeyRepresentingConsoleUserNameChange =
                    SCDynamicStoreKeyCreateConsoleUser(NULL);

    if (KeyRepresentingConsoleUserNameChange == NULL)
    //Note: if key is null we fail.
    {
        CFRelease(DynamicStoreCommunicationMechanism);
        //releasing allocated memory before giving up.
        return(-4);
    }

    // --- creating array of notification keys --- //
    /* Now that we have the console user key which is the key we will
     * be notifing upon we need to place the
     * key into an array.  This is because the System Configuration
     * framework expects all lists of keys to be in an array.  Thus we
     * will create an array for the occasion then place the key inside
     * of it.  We create the array with CFArrayCreateMutable.
     * First Argument: Allocator to use.  As usual we want default
     *     allocator so pass null.
     * Second Argument: The number of items which will be in the array.
     *     In this case we only have one key so the size will be one.
     * Third Argument: The type of retain/release methods to use with the
     *     data.  In this case we are using normal Core Foundation types
     *     which means default CFArray behavior will do.  Thus, as
     *     retain/release behavior we pick kCFTypeArrayCallBacks which
     *     represents the normal retain/release for an CFArray.
     * Return Value: An empty CFArray.  This is the array which will
     *     hold the notification keys.  Note the CFArray must be released.
     */
    ArrayOfNotificationKeys = CFArrayCreateMutable
                (NULL, (CFIndex)1, &kCFTypeArrayCallBacks);

        /* error creating CFArray of notification keys. */
    if (ArrayOfNotificationKeys == NULL)
    {
        CFRelease(DynamicStoreCommunicationMechanism);
        //releasing allocated memory before giving up.
        CFRelease(KeyRepresentingConsoleUserNameChange);
        return(-5);
    }

    /* Now that we have our empty array we need to add the notification
     * key to it.  We do this using CFArrayAppendValue
     * First Argument: The CFArray which will have an item appended to it.
     * Second Argument: Key which will be added to CFArray.  The key is
     * represented as CFString.
     * No return value
     */
    CFArrayAppendValue(ArrayOfNotificationKeys,
                    KeyRepresentingConsoleUserNameChange);

    /* Now that we have an array repesenting the values to notify upon
     * we will add this information to the dynamic store.  This way we
     * can create a CFRunloop source based upon the notification key
     * chosen.  In this case the notification key was the console
     * user change.  We add the notification keys to the
     * dynamic store with the call SCDynamicStoreSetNotificationKeys
     * First Argument: Dynamic store to add this information to.
     *     This is the dynamic store we obtained earlier.
     * Second Argument: The Array of notification keys.  In this case
     *     the single key representing any change in console user.
     * Third Argument: Any regex keys to be monitored.  In this case we
     *     already have all our keys we need from the array.  Thus this
     * value can be NULL since we don't need any other keys.
     * Return Value: A boolean representing if operation was successful
     * (true) or unsuccessful (false).
     */
     Result = SCDynamicStoreSetNotificationKeys(
     DynamicStoreCommunicationMechanism, ArrayOfNotificationKeys, NULL);

     //Done with notification keys array so release
     CFRelease(ArrayOfNotificationKeys);
     //done with notification key so release.
     CFRelease(KeyRepresentingConsoleUserNameChange);

     if (Result == FALSE) //unable to add keys to dynamic store.
     {
            //releasing allocated memory before giving up.
        CFRelease(DynamicStoreCommunicationMechanism);
        return(-6);
     }

     /* Now we are creating the CFRunloopSource which we will return on
      * this function.  This run loop source when inserted in your run loop
      * will cause your notification functions to get called via the
      * 'proxy' notification function.  We create the RunLoopSource
      * using the call SCDynamicStoreCreateRunLoopSource.
      *
      * First Argument: Allocator to use.   As usual we want default
      *     allocator so pass null.
      * Second Argument: The dynamic store which to base the run loop
      *     source upon.  We have already created a dynamic store which
      *     will notify on console user changes.  This will be the dynamic
      *     store the run loop source is based upon.
      * Third Argument: The 'order' of the CFRunloop source.  We want
      *     default behavior here too so pass zero.
      * Return value: the run loop source created.  Note that the
      *     return value must eventually be released.
      */
    *RunloopSourceReturned = SCDynamicStoreCreateRunLoopSource
                (NULL, DynamicStoreCommunicationMechanism, (CFIndex) 0);

	return 0;
}

static CRFBBundleWrapper* instance = NULL;

CRFBBundleWrapper::CRFBBundleWrapper()
{
	rfb_port = 0;
	is_rfb = false;
}

CRFBBundleWrapper::~CRFBBundleWrapper()
{
	rfb_port = 0;
	is_rfb = false;
}

bool CRFBBundleWrapper::canWriteToFile(NSString *path)
{
    if ([[NSFileManager defaultManager] fileExistsAtPath:path])
        return [[NSFileManager defaultManager] isWritableFileAtPath:path];
    else
        return [[NSFileManager defaultManager] isWritableFileAtPath:[path stringByDeletingLastPathComponent]];
}

void CRFBBundleWrapper::loadProxyFields(NSUserDefaults* suDefaults)
{
	static id proxyStringsDictionary = nil;
	
	NSString *proxyAddrString = [suDefaults stringForKey:@"EchoProxyAddr"];
	NSString *proxyPortString = [suDefaults stringForKey:@"EchoProxyPort"];
	NSString *proxyUserString = [suDefaults stringForKey:@"EchoProxyUser"];
	NSString *proxyPassString = [suDefaults stringForKey:@"EchoProxyPass"];
	
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

void CRFBBundleWrapper::loadServerList(NSUserDefaults* suDefaults)
{
	g_globals.m_proxiesManager.DisconnectAllProxies();
	g_globals.m_proxiesManager.RemoveAllProxies();

	NSEnumerator *echoEnum = [[suDefaults objectForKey:@"EchoServers"] objectEnumerator];
	NSMutableDictionary *echoDict = nil;

	while (echoDict = [[[echoEnum nextObject] mutableCopy] autorelease])
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
		
		g_globals.m_logger.Write("");
		g_globals.m_logger.WriteFormated("IP: %s", proxyInfo->GetIpPort());
		g_globals.m_logger.WriteFormated("User: %s", proxyInfo->GetMyID());
		
		SetEncryptionLevel(1, proxyInfo);
		
		proxyInfo->SetReconnectProxy(false);
		int connectResult = ConnectProxy(proxyInfo);
		switch (connectResult)
		{
			case 0:
				g_globals.m_logger.Write("Status: Connected");
				break;
			case 1:
				g_globals.m_logger.Write("Status: No Server Avail");
				break;
			case 2:
				g_globals.m_logger.Write("Status: Auth Failed");
				break;
			case 3:
				g_globals.m_logger.Write("Status: Already Active");
				break;
			case 4:
				g_globals.m_logger.Write("Status: Timed Out");
				break;
			default:
				g_globals.m_logger.Write("Status: Unknown Return Code");
				break;
		}
		proxyInfo->SetReconnectProxy(true);
	}
}

void CRFBBundleWrapper::loadLoggingOptions(NSUserDefaults* suDefaults)
{
	NSString *bundle_path = [[NSBundle mainBundle] bundlePath];

	NSArray *logFiles = [NSArray arrayWithObjects:
		@"/var/log/OSXvnc-server.log",
		@"~/Library/Logs/OSXvnc-server.log",
		@"/tmp/OSXvnc-server.log",
		[bundle_path stringByAppendingPathComponent:@"OSXvnc-server.log"],
		[suDefaults stringForKey:@"LogFile"],
		nil];
	NSEnumerator *logEnumerators = [logFiles objectEnumerator];
	// Find first writable location for the log file
	NSString *logFile = nil;
	while (logFile = [logEnumerators nextObject])
	{
		logFile = [logFile stringByStandardizingPath];
		if ([logFile length] && canWriteToFile(logFile))
		{
			[logFile retain];
			break;
		}
	}
	NSLog(logFile);
	//
	
	//converto from NSString to char*
	int len_logFile = [logFile length];
	char pLogFile[len_logFile + 1];
	strncpy(pLogFile, [logFile cString], len_logFile);
	pLogFile[len_logFile] = '\0';
	//

	bool enableLog = [suDefaults boolForKey:@"EnableLogging"];
	SetLoggingOptions(enableLog, pLogFile);
}

bool CRFBBundleWrapper::UserDefaultsChecking()
{
	BOOL result = FALSE;

	NSArray *args = [[NSProcessInfo processInfo] arguments];
	int argumentIndex = [args indexOfObject:@"-donotloadproxy"];
	if (argumentIndex != NSNotFound)
		return result;

	NSString *username = CUser::CurrentConsoleUsername();
	if (!username)
		return result;

	CUser user(username);
	[username release];

	uid_t old_uid = geteuid();
	[NSUserDefaults resetStandardUserDefaults];
	seteuid(user.uid());
	
	NSUserDefaults *suDefaults = user.userDefaults(@"OSXvnc");
	
	loadLoggingOptions(suDefaults);

	g_globals.m_logger.Write("Load echo servers.");
	if (InitializeProxyDll())
	{
		SetPortForOffLoadingData(rfb_port);

		loadProxyFields(suDefaults);
		loadServerList(suDefaults);
	}
	else
	{
		g_globals.m_logger.Write("Proxy DLL can not be initialized.");
	}
	result = TRUE;

	[NSUserDefaults resetStandardUserDefaults];
	seteuid(old_uid);

	return result;
}

int CRFBBundleWrapper::doInstall()
{
	if (is_rfb)
	{
		CFRunLoopSourceRef testRunloopSource = NULL;

		int result = InstallLoginLogoutNotifiers(
				CFSTR("This is my crazy program"), LoginFunction,
				LogoutFunction, &testRunloopSource);

		if (result != 0)
			return 1; //give up since could not install notifier.

		CFRunLoopAddSource(CFRunLoopGetCurrent(), testRunloopSource, kCFRunLoopDefaultMode);
		CFRelease(testRunloopSource);
		CFRunLoopRun();
	}

    return 0;
}

CRFBBundleWrapper* CRFBBundleWrapper::GetInstance()
{
	if (!instance)
		instance = new CRFBBundleWrapper();
	return instance;
}

void CRFBBundleWrapper::FreeInstance()
{
	if (instance)
		delete instance;
	instance = NULL;
}

unsigned long CRFBBundleWrapper::RunLoopThreadProc(void* lpParameter)
{
	CRFBBundleWrapper *rfb = (CRFBBundleWrapper*)lpParameter;
	if (rfb)
		rfb->doInstall();
	return 0;
}

void CRFBBundleWrapper::startRunLoop()
{
	void* m_hThread;
	unsigned long m_dwThread;

	m_hThread = CreateThread(0, 0, RunLoopThreadProc, this, 0, &m_dwThread);
}
