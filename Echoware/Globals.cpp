#include "stdafx.h"
#include "Globals.h"

//#include <Winsock2.h>

CGlobals g_globals;

CGlobals::CGlobals()
{
	m_bIsDllInitialized=false;
	m_nPortForOffLoadingData=0;
}

CGlobals::~CGlobals()
{
	if (g_globals.GetDllInitialized())
	{
		g_globals.m_logger.Write("ReleaseSockets");
		g_globals.ReleaseSockets();
	}
}

bool CGlobals::InitSockets(unsigned char nHighVersion, unsigned char nLowVersion)
{	
// No Socket System Init Needed on OSX
//	WORD wVersionRequested;
//	WSADATA wsaData;
//	int err;
//	 
//	wVersionRequested = MAKEWORD(nLowVersion, nHighVersion);
//	 
//	err = WSAStartup( wVersionRequested, &wsaData );
//	if (err!=0)
//		return false;	
//	 
//	if (LOBYTE(wsaData.wVersion)!=nLowVersion || HIBYTE(wsaData.wVersion)!=nHighVersion) 
//	{		
//		WSACleanup();
//		return false; 
//	}
	return true;
}

bool CGlobals::ReleaseSockets()
{
// No Socket System Init Needed on OSX
//	if (WSACleanup()==0)
//		return true;
//
//	if (WSAGetLastError()==WSAEINPROGRESS)
//		return false;
	return true;
}

void CGlobals::SetDllInitialized(bool bIsDllInitialized)
{
	m_critSection.Lock();

	m_bIsDllInitialized=bIsDllInitialized;

	m_critSection.Unlock();
}

bool CGlobals::GetDllInitialized()
{
	bool bRet;

	m_critSection.Lock();

	bRet=m_bIsDllInitialized;

	m_critSection.Unlock();

	return bRet;
}

void CGlobals::SetPortForOffLoadingData(int nPortForOffLoadingData)
{
	m_critSection.Lock();

	m_nPortForOffLoadingData=nPortForOffLoadingData;

	m_critSection.Unlock();
}

int CGlobals::GetPortForOffLoadingData()
{
	int nRet;

	m_critSection.Lock();

	nRet=m_nPortForOffLoadingData;

	m_critSection.Unlock();

	return nRet;
}
//-----------------------------------------------------------------------------------------

int CGlobals::GetFormattedID(char *szBuffer, int len, const char* szPartner)
{
	if (len < ID_STRING_SIZE)
		return -1;
	memset(szBuffer, 0, len);
	char *pColon;
	pColon = strrchr( szPartner, ':');
	if (pColon == NULL)
	{
		strcpy(szBuffer + CLIENT_ID_OFFSET, szPartner);
		return 0;
	}
	else
	{
		strncpy(szBuffer + CLIENT_ID_OFFSET, szPartner, (int)(pColon - szPartner));
		strcpy(szBuffer + APP_ID_OFFSET, szPartner + (int)(pColon - szPartner + 1));
		return 1;
	}
}
//-----------------------------------------------------------------------------------------

int CGlobals::ParseFormattedID(char *szBuffer, int len, char* szPartner)
{
	memset(szPartner, 0, len);
	if (szBuffer + APP_ID_OFFSET != NULL)
	{
		sprintf(szPartner, "%s:%s", szBuffer, szBuffer + APP_ID_OFFSET);
	}
	else
		sprintf(szPartner, "%s", szBuffer);
	return ID_STRING_SIZE;
}
//-----------------------------------------------------------------------------------------
