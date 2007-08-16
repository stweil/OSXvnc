/*
 *  User.h
 *  Echoware
 *
 *  Created by admin on 4/11/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#import <Cocoa/Cocoa.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "DirService.h"

class CUser
{
	private:
		NSString* m_name;
		NSString* m_realName;
		int m_gid;
		int m_uid;
		bool m_admin;
		int* m_groups;
		
		void initWithUsername(NSString* username, CDirService* dirService);
	public:
		CUser(NSString *username);
		CUser(NSString *username, NSString *password);
		virtual ~CUser();

		NSString* name() { return m_name; }
		NSString* realName() { return m_realName; }
		int gid() { return m_gid; }
		int uid() { return m_uid; }
		bool admin() { return m_admin; }
		int* groups() { return m_groups; }
		
		NSUserDefaults *userDefaults(NSString *domain);
		
		static CFStringRef CopyCurrentConsoleUsername();
		static NSString* CurrentConsoleUsername();
		static bool ConsoleUserIsLoggedIn();
};
