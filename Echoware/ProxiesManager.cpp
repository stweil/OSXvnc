#include "StdAfx.h"
#include "proxiesmanager.h"
#include "DllProxyInfo.h"
//#include <algorithm>

CProxiesManager::CProxiesManager(void)
{
	shouldQuit=0; // Mac OSX
	hasQuit=0; // Mac OSX

	m_bLocalProxy=false;
	m_pLocalProxy=0;
	
	m_hReconectProxiesThread=CreateThread(0, 0, ReconectProxiesThreadProc, this, 0, &m_dwReconectProxiesThread);
}

CProxiesManager::~CProxiesManager(void)
{
	ShutdownThread(m_hReconectProxiesThread, THREAD_STOP_TIMEOUT, &shouldQuit, &hasQuit);
	RemoveAllProxies();
}

//add a proxy to the proxies list
void CProxiesManager::AddProxy(CDllProxyInfo* pProxyInfo)
{
	m_critSection.Lock();

	CProxyConnection* pProxy=new CProxyConnection(pProxyInfo);
	
	m_listProxies.push_back(pProxy);

	m_critSection.Unlock();
}

//remove a proxy from the proxies list
void CProxiesManager::RemoveProxy(CDllProxyInfo* pProxyInfo)
{
	m_critSection.Lock();	

	for(std::list<CProxyConnection*>::iterator it=m_listProxies.begin(); it!=m_listProxies.end(); it++)
		if (((CProxyConnection*)*it)->GetProxyInfo()==pProxyInfo)
		{
			CProxyConnection* pProxy=(CProxyConnection*)*it;
			delete pProxy;
			pProxy=0;

			std::list<CProxyConnection*>::iterator itend=it;
			itend++;
			m_listProxies.erase(it, itend);
			break;
		}		

	m_critSection.Unlock();
}

//remove all the proxies from the proxies list
void CProxiesManager::RemoveAllProxies()
{
	m_critSection.Lock();

	for(std::list<CProxyConnection*>::iterator it=m_listProxies.begin(); it!=m_listProxies.end(); it++)
	{
		CProxyConnection* pProxy=(CProxyConnection*)*it;
		delete pProxy;
	}

	m_listProxies.clear();

	m_critSection.Unlock();
}

//connect a proxy
int CProxiesManager::ConnectProxy(CDllProxyInfo* pProxyInfo)
{
	int nRet=1;
	
	m_critSection.Lock();

	CProxyConnection* pProxyConnection=(CProxyConnection*)FindProxy(pProxyInfo);

	if (pProxyConnection)
		nRet=pProxyConnection->Connect();

	m_critSection.Unlock();

	return nRet;
}

//disconnect a proxy
bool CProxiesManager::DisconnectProxy(CDllProxyInfo* pProxyInfo)
{
	bool bRet=true;

	m_critSection.Lock();

	CProxyConnection* pProxyConnection=(CProxyConnection*)FindProxy(pProxyInfo);

	if (pProxyConnection)
		bRet=pProxyConnection->Disconnect();

	m_critSection.Unlock();

	return bRet;
}

//stop connecting a proxy
void CProxiesManager::StopConnecting(CDllProxyInfo* pProxyInfo)
{
	CProxyConnection* pProxyConnection=(CProxyConnection*)FindProxy(pProxyInfo);

	if (pProxyConnection)
		pProxyConnection->StopConnecting();
}

//disconnect all the proxies
bool CProxiesManager::DisconnectAllProxies()
{
	bool bRet=true;

	m_critSection.Lock();

	for(std::list<CProxyConnection*>::iterator it=m_listProxies.begin(); it!=m_listProxies.end(); it++)		
	{
		bRet= bRet && ((CProxyConnection*)*it)->Disconnect();
	}

	m_critSection.Unlock();

	return bRet;
}

//set a proxy encryption level
void CProxiesManager::SetEncryptionLevel(int level, CDllProxyInfo* pProxyInfo)
{
	m_critSection.Lock();
	
	CProxyConnection* pProxyConnection=(CProxyConnection*)FindProxy(pProxyInfo);

	if (pProxyConnection)
		pProxyConnection->SetEncryptionLevel(level);

	m_critSection.Unlock();
}

//set info for the local proxy
void CProxiesManager::SetLocalProxyInfo(char* ip, char* port, char* username, char* password)
{
	m_critSection.Lock();

	m_bLocalProxy=true;

	if (!m_pLocalProxy)
		m_pLocalProxy=new CDllProxyInfo;

	m_pLocalProxy->SetIP(ip);
	m_pLocalProxy->SetPort(port);
	m_pLocalProxy->SetName(username);
	m_pLocalProxy->SetPassword(password);

	m_critSection.Unlock();
}

//try to connect all proxies
void CProxiesManager::AutoConnect()
{
	m_critSection.Lock();

	for(std::list<CProxyConnection*>::iterator it=m_listProxies.begin(); it!=m_listProxies.end(); it++)		
	{
		((CProxyConnection*)*it)->Connect();		
	}

	m_critSection.Unlock();
}

//try to Estabilish new data channel
int CProxiesManager::EstablishNewDataChannel(CDllProxyInfo* pProxyInfo , char* IDOfPartner)
{
	int nRet=0;

	m_critSection.Lock();

	CProxyConnection* pProxyConnection=(CProxyConnection*)FindProxy(pProxyInfo);

	if (pProxyConnection)
		nRet=pProxyConnection->EstablishNewDataChannel(IDOfPartner);
	
	m_critSection.Unlock();

	return nRet;
}

//find a proxy in the list by CDllProxyInfo
const CProxyConnection* CProxiesManager::FindProxy(CDllProxyInfo* pProxyInfo)
{
	for(std::list<CProxyConnection*>::iterator it=m_listProxies.begin(); it!=m_listProxies.end(); it++)
		if (((CProxyConnection*)*it)->GetProxyInfo()==pProxyInfo)		
			return (CProxyConnection*)*it;
			
	return 0;
}

//try to connect a socket using the proxy settings
bool CProxiesManager::ConnectViaProxy(APISocket::CSocket* pSock, const char* szIp, unsigned int nPort)
{
	if (m_bLocalProxy)
		return m_proxyConnect.ConnectViaProxy(*pSock, szIp, nPort, (char*)m_pLocalProxy->GetIP(), 
			atoi(m_pLocalProxy->GetPort()), (char*)m_pLocalProxy->GetName(), (char*)m_pLocalProxy->GetPassword())>0;

	std::string strProxyIP;
	unsigned int nProxyPort;

	if (GetIEProxySettings(strProxyIP, nProxyPort))
		return m_proxyConnect.ConnectViaProxy(*pSock, szIp, nPort, (char*)strProxyIP.c_str(), 
			nProxyPort, "", "")>0;

	return false;
}

//read the Internet Explorer settings about proxy
int CProxiesManager::GetIEProxySettings(std::string& strProxyIP, unsigned int& nProxyPort)
{
	#warning Must Implement GetIEProxySettings
	return 0;
}

#define WM_PROXY_ERROR		(WM_USER+50)

void CProxiesManager::ProxyError(CDllProxyInfo* pProxyInfo)
{
	m_critSection.Lock();

	proxyError = pProxyInfo;

	m_critSection.Unlock();
}

unsigned long __stdcall CProxiesManager::ReconectProxiesThreadProc(LPVOID lpParameter)
{
	CProxiesManager* pThis = (CProxiesManager*)lpParameter;

	while (!pThis->shouldQuit)
	{
		if (pThis->proxyError)
		{
			pThis->DisconnectProxy(pThis->proxyError);
			pThis->proxyError = NULL;
			Sleep(1000000);
		}
		else
		{
			bool sleep15 = false;
			pThis->m_critSection.Lock();
			if (!pThis->m_listProxies.empty())
			{
				sleep15 = true;
				for (std::list<CProxyConnection*>::iterator itProxy = pThis->m_listProxies.begin(); itProxy != pThis->m_listProxies.end(); itProxy++)
				{
					CProxyConnection* pProxyConn = (CProxyConnection*)*itProxy;
					CDllProxyInfo* pProxyInfo = pProxyConn->GetProxyInfo();
					int dwStatus = pProxyInfo->GetStatus();
					bool bReconnect = pProxyInfo->GetReconnectProxy();

					if ((dwStatus & STATUS_DISCONNECTED_FROM_PROXY || (dwStatus == 0)) && bReconnect)
					{
						g_globals.m_logger.Write("=>Try reconnect proxy");

						pProxyConn->Connect();

						g_globals.m_logger.Write("<=Reconnect proxy");
					}
				}
			}
			pThis->m_critSection.Unlock();
			if (sleep15)
				Sleep(15000000);
			else
				Sleep(1000000);
		}
	}

	pThis->hasQuit = 1;
	return 0;
}