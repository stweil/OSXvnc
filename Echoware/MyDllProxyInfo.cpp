/*
 *  MyDllProxyInfo.cpp
 *  Echoware
 *
 *  Created by Vasya Pupkin on 1/15/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "MyDllProxyInfo.h"
#include "unistd.h"
#include "EchoToOSX.h"

CMyDllProxyInfo::CMyDllProxyInfo(IDllProxyInfo *pProxyInfo)
{
	m_pDllProxyInfo = pProxyInfo;
	m_eStatus = CMyDllProxyInfo::None;
	m_nPrevStatus = 0;
	if (m_pDllProxyInfo)
		m_nPrevStatus = m_pDllProxyInfo->GetStatus();
}

CMyDllProxyInfo::~CMyDllProxyInfo()
{
	m_pDllProxyInfo = 0;
	m_eStatus = CMyDllProxyInfo::None;
	m_nPrevStatus = 0;
}

IDllProxyInfo* CMyDllProxyInfo::getDllProxyInfo()
{
	return m_pDllProxyInfo;
}

void CMyDllProxyInfo::setDllProxyInfo(IDllProxyInfo *pProxyInfo)
{
	m_pDllProxyInfo = pProxyInfo;
}

CMyDllProxyInfo::STATUS CMyDllProxyInfo::getStatus()
{
	return m_eStatus;
}

void CMyDllProxyInfo::setStatus(CMyDllProxyInfo::STATUS status)
{
	m_eStatus = status;
}

void CMyDllProxyInfo::RemoveStatus()
{
	setStatus(CMyDllProxyInfo::None);
}

int CMyDllProxyInfo::getPrevStatus()
{
	return m_nPrevStatus;
}

void CMyDllProxyInfo::setPrevStatus(int status)
{
	m_nPrevStatus = status;
}

bool CMyDllProxyInfo::isStatusChanged(int newStatus)
{
	return (m_nPrevStatus != newStatus);
}

void CMyDllProxyInfo::WaitForStatus(int status, int timeout, int interval)
{
	if (m_pDllProxyInfo)
	{
		int t = 0;
		while (!(m_pDllProxyInfo->GetStatus() & status))
		{
			Sleep(interval);
			t += interval;
			if (timeout > 0 && t >= timeout)
				break;
		}
	}
}

char* CMyDllProxyInfo::getStatusString()
{
	if (m_pDllProxyInfo)
	{
		switch(m_eStatus)
		{
			case Connecting:
			case Reconnecting:
				return "Connecting...";
			case Removing:
				return "Removing...";
		}
		
		int status = (int)m_pDllProxyInfo->GetStatus();

		if (status & STATUS_SEARCHING_FOR_PARTNER)
			return "Partner Search Initiated";
		else if (status & STATUS_ESTABLISHING_DATA_CHANNEL)
			return "Establishing Data Channel...";
		else if (status & STATUS_AUTHENTICATION_FAILED)
			return "Authentication Failed";
		else if (status & STATUS_AUTHENTICATING)
			return "Authenticating...";
		else if (status & STATUS_DISCONNECTED_FROM_PROXY || !status)
			return "No Proxy Connection";
		else if (status & STATUS_CONNECTED)
			return "Proxy Channel Connected";
	}
	return "";
}