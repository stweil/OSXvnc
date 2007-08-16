/*
 *  User.cpp
 *  Echoware
 *
 *  Created by admin on 4/11/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "User.h"
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>

CUser::CUser(NSString *username)
{
	CDirService dirService;
	initWithUsername(username, &dirService);
}

CUser::CUser(NSString *username, NSString *password)
{
	CDirService dirService;
	
	if (dirService.authenticateUser([username cStringUsingEncoding:NSUTF8StringEncoding],
									[password cStringUsingEncoding:NSUTF8StringEncoding]))
	{
		initWithUsername(username, &dirService);
	}
}

CUser::~CUser()
{
	[m_name release];
	[m_realName release];
	if (m_groups != NULL) free(m_groups);
	m_groups = NULL;
}

NSUserDefaults* CUser::userDefaults(NSString *domain)
{
//	uid_t old_uid = geteuid();
//	[NSUserDefaults resetStandardUserDefaults];
//	seteuid(m_uid);

	NSUserDefaults *suDefaults = [NSUserDefaults standardUserDefaults];
	NSDictionary *persistDomain = [suDefaults persistentDomainForName: domain];
	[suDefaults registerDefaults: persistDomain];

//	[NSUserDefaults resetStandardUserDefaults];
//	seteuid(tmp_uid);
//	old_uid = -1;
	
	return suDefaults;
}

void CUser::initWithUsername(NSString* username, CDirService* dirService)
{
		NSDictionary* userInfo = dirService->attributeDataForNodeOfType(kDSStdRecordTypeUsers,
																  [username cStringUsingEncoding:NSUTF8StringEncoding],
																  kDSNAttrRecordName,
																  kDS1AttrUniqueID,
																  kDS1AttrPrimaryGroupID,
																  kDS1AttrDistinguishedName,
																  NULL,
																  NULL);
		NSDictionary* adminInfo = dirService->attributeDataForNodeOfType(kDSStdRecordTypeGroups,
																  "admin",
																  kDSNAttrRecordName,
																  kDSNAttrGroupMembership,
																  NULL,
																  NULL,
																  NULL,
																  NULL);
		m_name     = [[[userInfo objectForKey:[NSString stringWithCString:kDSNAttrRecordName encoding:NSUTF8StringEncoding]] objectAtIndex:0] retain];		
		m_realName = [[[userInfo objectForKey:[NSString stringWithCString:kDS1AttrDistinguishedName encoding:NSUTF8StringEncoding]] objectAtIndex:0] retain];
		m_uid      = [[[userInfo objectForKey:[NSString stringWithCString:kDS1AttrUniqueID encoding:NSUTF8StringEncoding]] objectAtIndex:0] intValue];	
		m_gid      = [[[userInfo objectForKey:[NSString stringWithCString:kDS1AttrPrimaryGroupID encoding:NSUTF8StringEncoding]] objectAtIndex:0] intValue];			
		
		NSArray* adminMembers = [adminInfo objectForKey:[NSString stringWithCString:kDSNAttrGroupMembership encoding:NSUTF8StringEncoding]];		
		m_admin = [adminMembers containsObject:m_realName] || [adminMembers containsObject:m_name];
		
		if (m_name != nil)
		{	
			m_groups = NULL;
			
			int groupsTemp[256];
			int groupsize = 256;
			getgrouplist([m_name cStringUsingEncoding:NSUTF8StringEncoding], m_gid, groupsTemp, &groupsize);
			if (groupsize > 1)
			{
				m_groups = (int*)malloc( sizeof(int) * groupsize );
				m_groups[groupsize - 1] = -1;
				int i = 0;
				for (i = 1; i < groupsize; i++)
					m_groups[i-1] = groupsTemp[i];
			}
		}
}

CFStringRef CUser::CopyCurrentConsoleUsername()
{
	CFStringRef consoleUserName;
	uid_t uid;
	gid_t gid;
	consoleUserName = SCDynamicStoreCopyConsoleUser(NULL, &uid, &gid);
	return consoleUserName;
}

NSString* CUser::CurrentConsoleUsername()
{
	NSString *res = NULL;
	CFStringRef userName = CopyCurrentConsoleUsername();
	if (userName != NULL)
	{
		res = (NSString*)userName;
	}
	return res;
}

bool CUser::ConsoleUserIsLoggedIn()
{
	bool result = false;
	CFStringRef userName = CopyCurrentConsoleUsername();
	if (userName != NULL)
	{
		CFRelease(userName);
		result = true;
	}
	return result;
}
