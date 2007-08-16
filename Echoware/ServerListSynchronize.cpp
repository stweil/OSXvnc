/*
 *  ServerListSynchronize.cpp
 *  Echoware
 *
 *  Created by admin on 2/21/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "ServerListSynchronize.h"
#include "EchoToOSX.h"
#include "ProxiesManager.h"
#include "MyDllProxyInfo.h"
#include "globals.h"

CServerListSynchronize::CServerListSynchronize()
{
	m_fInitialized = false;
	echo = nil;
}

CServerListSynchronize::~CServerListSynchronize()
{
	if (echo)
	{
		int count = [echo->echoInfoProxys count];
		while (count > 0)
		{
			count--;
			CMyDllProxyInfo *pMyProxyInfo = (CMyDllProxyInfo*)[[echo->echoInfoProxys objectAtIndex:count] pointerValue];
			delete pMyProxyInfo;
		}
	}

	m_hManageThread_Connect = 0;
	m_dwManageThread_Connect = 0;
	m_hManageThread_Remove = 0;
	m_dwManageThread_Remove = 0;
	m_hManageThread_Update = 0;
	m_dwManageThread_Update = 0;
	
	echo = nil;
}

void CServerListSynchronize::Init()
{
	if (!m_fInitialized)
	{
		m_hManageThread_Connect = 0;
		m_dwManageThread_Connect = 0;
		m_hManageThread_Remove = 0;
		m_dwManageThread_Remove = 0;
		m_hManageThread_Update = 0;
		m_dwManageThread_Update = 0;
		
		echo = nil;

		m_fTerminated_Connect = false;
		m_fTerminated_Remove = false;
		m_fTerminated_Update = false;
		
		m_fTerminated = false;

		m_fHasQuit_Connect = false;
		m_fHasQuit_Remove = false;
		m_fHasQuit_Update = false;

		m_fStatusChanged = false;
		m_fInitialized = true;
	}
}

void CServerListSynchronize::Start(EchoController *echo)
{
	m_fTerminated_Connect = false;
	m_fTerminated_Remove = false;
	m_fTerminated_Update = false;

	m_fTerminated = false;

	m_fHasQuit_Connect = false;
	m_fHasQuit_Remove = false;
	m_fHasQuit_Update = false;
	
	this->echo = echo;

	m_hManageThread_Connect = CreateThread(0, 0, ManageThreadProc_Connect, this, 0, &m_dwManageThread_Connect);
	m_hManageThread_Remove = CreateThread(0, 0, ManageThreadProc_Remove, this, 0, &m_dwManageThread_Remove);
	m_hManageThread_Update = CreateThread(0, 0, ManageThreadProc_Update, this, 0, &m_dwManageThread_Update);
}

void CServerListSynchronize::Terminate()
{
	m_fHasQuit_Update = false;
	m_fHasQuit_Connect = false;
	m_fHasQuit_Remove = false;

	m_fTerminated_Update = true;
	m_fTerminated_Connect = true;
	m_fTerminated_Remove = true;

	unsigned long timeout = 5000;

	ShutdownThread(m_hManageThread_Update, timeout, &m_fTerminated_Update, &m_fHasQuit_Update);
	ShutdownThread(m_hManageThread_Connect, timeout, &m_fTerminated_Connect, &m_fHasQuit_Connect);
	ShutdownThread(m_hManageThread_Remove, timeout, &m_fTerminated_Remove, &m_fHasQuit_Remove);

	m_fTerminated = true;
}

bool CServerListSynchronize::getStatusChanged()
{
	return m_fStatusChanged;
}

void CServerListSynchronize::setStatusChanged(bool value)
{
	m_fStatusChanged = value;
}

unsigned long CServerListSynchronize::ManageThreadProc_Connect(void* lpParameter)
{
	CServerListSynchronize* sls = (CServerListSynchronize*)lpParameter;

	while (true)
	{
		sls->m_critSection.Lock();
		
		if (sls->m_fTerminated_Connect)
		{
			sls->m_critSection.Unlock();
			break;
		}

		if (sls->echo)
		{
			int count = [sls->echo->echoInfoProxys count];
			int ind = 0;
			while (ind < count)
			{
				CMyDllProxyInfo *pMyProxyInfo = [sls->echo getDllProxyInfo: ind];
				if (pMyProxyInfo == NULL)
				{
					ind++;
					continue;
				}
				
				IDllProxyInfo *echoProxyInfo = pMyProxyInfo->getDllProxyInfo();

				switch(pMyProxyInfo->getStatus())
				{
					case CMyDllProxyInfo::Connecting:
					{
						NSLog(@"Connecting...");
						
						int ind = [sls->echo->echoTableView selectedRow];
						[sls->echo reloadData];
						[sls->echo selectRow: ind];

						echoProxyInfo->SetReconnectProxy(false);
						int connectResult = ConnectProxy(echoProxyInfo);
						switch (connectResult)
						{
						case 0:
							echoProxyInfo->SetReconnectProxy(true);
							NSLog(@"Status: Connected");
							break;
						case 1:
							NSLog(@"Status: No Server Avail");
							break;
						case 2:
							NSLog(@"Status: Auth Failed");
							break;
						case 3:
							echoProxyInfo->SetReconnectProxy(true);
							NSLog(@"Status: Already Active");
							break;
						case 4:
							echoProxyInfo->SetReconnectProxy(true);
							NSLog(@"Status: Timed Out");
							break;
						default:
							NSLog(@"Status: Unknown Return Code");
							break;
						}
						if (pMyProxyInfo->getStatus() == CMyDllProxyInfo::Connecting)
							pMyProxyInfo->RemoveStatus();
						pMyProxyInfo->setPrevStatus(echoProxyInfo->GetStatus());

						ind = [sls->echo->echoTableView selectedRow];
						[sls->echo reloadData];
						[sls->echo selectRow: ind];

						NSLog(@"Connected...");
					}
					break;

					case CMyDllProxyInfo::Reconnecting:
					{
						NSLog(@"Reconnecting...");

						int ind = [sls->echo->echoTableView selectedRow];
						[sls->echo reloadData];
						[sls->echo selectRow: ind];

						echoProxyInfo->SetReconnectProxy(false);
						int status = echoProxyInfo->GetStatus();
						if (!(status & STATUS_DISCONNECTED_FROM_PROXY) && status)
						{
							bool disconnected = DisconnectProxy(echoProxyInfo);
							pMyProxyInfo->WaitForStatus(STATUS_DISCONNECTED_FROM_PROXY, 500000);
							if (disconnected)
								NSLog(@"Disconnected...");
							else
								NSLog(@"Not disconnected...");
						}

						int connectResult = ConnectProxy(echoProxyInfo);
						switch (connectResult)
						{
						case 0:
							echoProxyInfo->SetReconnectProxy(true);
							NSLog(@"Status: Connected");
							break;
						case 1:
							NSLog(@"Status: No Server Avail");
							break;
						case 2:
							NSLog(@"Status: Auth Failed");
							break;
						case 3:
							echoProxyInfo->SetReconnectProxy(true);
							NSLog(@"Status: Already Active");
							break;
						case 4:
							echoProxyInfo->SetReconnectProxy(true);
							NSLog(@"Status: Timed Out");
							break;
						default:
							NSLog(@"Status: Unknown Return Code");
							break;
						}
						if (pMyProxyInfo->getStatus() == CMyDllProxyInfo::Reconnecting)
							pMyProxyInfo->RemoveStatus();
						pMyProxyInfo->setPrevStatus(echoProxyInfo->GetStatus());

						ind = [sls->echo->echoTableView selectedRow];
						[sls->echo reloadData];
						[sls->echo selectRow: ind];
						
						NSLog(@"Reconnected...");
					}
					break;
				}
				ind++;
			}
		}
		
		sls->m_critSection.Unlock();
		Sleep(1000000);
	}
	
	sls->m_fHasQuit_Connect = true;
	return 0;
}

unsigned long CServerListSynchronize::ManageThreadProc_Remove(void* lpParameter)
{
	CServerListSynchronize* sls = (CServerListSynchronize*)lpParameter;

	while (true)
	{
		sls->m_critSectionRemove.Lock();
		
		if (sls->m_fTerminated_Remove)
		{
			sls->m_critSectionRemove.Unlock();
			break;
		}

		if (sls->echo)
		{
			int count = [sls->echo->echoInfoProxysToRemove count];
			while (count > 0)
			{
				count--;
				CMyDllProxyInfo *pMyProxyInfo = (CMyDllProxyInfo*)[[sls->echo->echoInfoProxysToRemove objectAtIndex:count] pointerValue];
				IDllProxyInfo *echoProxyInfo = pMyProxyInfo->getDllProxyInfo();

				switch(pMyProxyInfo->getStatus())
				{
					case CMyDllProxyInfo::Removing:
					{
						NSLog(@"Removing...");

						echoProxyInfo->SetReconnectProxy(false);
						StopConnecting(echoProxyInfo);
						int status = echoProxyInfo->GetStatus();
						if (!(status & STATUS_DISCONNECTED_FROM_PROXY) && status)
						{
							bool disconnected = DisconnectProxy(echoProxyInfo);
							pMyProxyInfo->WaitForStatus(STATUS_DISCONNECTED_FROM_PROXY, 500000);
							if (disconnected)
								NSLog(@"Disconnected...");
							else
								NSLog(@"Not disconnected...");
						}

						[sls->echo->echoInfoProxysToRemove removeObjectAtIndex: count];
						DeleteProxyInfoClassObject(echoProxyInfo);
						pMyProxyInfo->RemoveStatus();
						delete pMyProxyInfo;

						NSLog(@"Removed...");
					}
					break;
				}
			}
		}
		
		sls->m_critSectionRemove.Unlock();
		Sleep(1000000);
	}
	
	sls->m_fHasQuit_Remove = true;
	return 0;
}

unsigned long CServerListSynchronize::ManageThreadProc_Update(void* lpParameter)
{
	CServerListSynchronize* sls = (CServerListSynchronize*)lpParameter;

	while (true)
	{
		sls->m_critSection.Lock();
		
		if (sls->m_fTerminated_Update)
		{
			sls->m_critSection.Unlock();
			break;
		}

		bool statusChanged = false;
		if (sls->echo)
		{
			int count = [sls->echo->echoInfoProxys count];
			while (count > 0)
			{
				count--;
				CMyDllProxyInfo *pMyProxyInfo = [sls->echo getDllProxyInfo: count];
				if (pMyProxyInfo == NULL)
					continue;
				
				IDllProxyInfo *echoProxyInfo = pMyProxyInfo->getDllProxyInfo();
				
				bool status_changed = pMyProxyInfo->isStatusChanged(echoProxyInfo->GetStatus());
				if (!statusChanged && status_changed)
				{
					statusChanged = true;
					pMyProxyInfo->setPrevStatus(echoProxyInfo->GetStatus());
					break;
				}
			}
		}
		sls->setStatusChanged(statusChanged);

		if (statusChanged)
		{
			NSLog(@"Status was changed");
			int index = [sls->echo->echoTableView selectedRow];
			[sls->echo reloadData];
			[sls->echo selectRow: index];
		}

		sls->m_critSection.Unlock();
		Sleep(2000000);
	}

	sls->m_fHasQuit_Update = true;
	return 0;
}
