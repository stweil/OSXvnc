/*
 *  RFBBundleWrapper.h
 *  Echoware
 *
 *  Created by admin on 4/11/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#import <Cocoa/Cocoa.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

class CRFBBundleWrapper
{
	private:
	public:
		bool is_rfb;
		int rfb_port;

		CRFBBundleWrapper();
		virtual ~CRFBBundleWrapper();

		bool canWriteToFile(NSString *path);
		void loadProxyFields(NSUserDefaults* suDefaults);
		void loadServerList(NSUserDefaults* suDefaults);
		void loadLoggingOptions(NSUserDefaults* suDefaults);
		bool UserDefaultsChecking();
		int doInstall();
		void startRunLoop();
		
		static CRFBBundleWrapper* GetInstance();
		static void FreeInstance();
		static unsigned long RunLoopThreadProc(void* lpParameter);
};