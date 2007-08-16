/*
 *  MyDllProxyInfo.h
 *  Echoware
 *
 *  Created by Vasya Pupkin on 1/15/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#import "Echoware.h"
#import "InterfaceDLLProxyInfo.h"∂

class CMyDllProxyInfo
{
	public:
		enum STATUS {None, Connecting, Removing, Reconnecting};
		
	private:
		IDllProxyInfo *m_pDllProxyInfo;
		STATUS m_eStatus;
		
		int m_nPrevStatus;
		
	public:
		CMyDllProxyInfo(IDllProxyInfo *pProxyInfo);
		~CMyDllProxyInfo();
		
		IDllProxyInfo* getDllProxyInfo();
		void setDllProxyInfo(IDllProxyInfo *pProxyInfo);
		
		STATUS getStatus();
		void setStatus(STATUS status);
		void RemoveStatus();
		
		int getPrevStatus();
		void setPrevStatus(int status);
		
		bool isStatusChanged(int newStatus);
		
		void WaitForStatus(int status, int timeout = 0, int interval = 50000);
		char* getStatusString();
};