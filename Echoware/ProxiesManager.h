#ifndef _PROXIESMANAGER_H
#define _PROXIESMANAGER_H

#if _MSC_VER > 1000
#pragma once
#endif 

#include "ProxyConnection.h"
#include <list>
#include "CritSection.h"
#include "ProxyConnect/ProxyConnect.h"

//a proxies manager class
class CProxiesManager
{
public:
	CProxiesManager(void);
	~CProxiesManager(void);

	//add a proxy to the proxies list
	void AddProxy(CDllProxyInfo* pProxyInfo);
	//remove a proxy from the proxies list
	void RemoveProxy(CDllProxyInfo* pProxyInfo);
	//remove all the proxies from the proxies list
	void RemoveAllProxies();

	//connect a proxy
	int ConnectProxy(CDllProxyInfo* pProxyInfo);
	//disconnect a proxy
	bool DisconnectProxy(CDllProxyInfo* pProxyInfo);
	//disconnect all the proxies
	bool DisconnectAllProxies();
	//stops connecting proxy
	void StopConnecting(CDllProxyInfo* pProxyInfo);

	//set a proxy encryption level
	void SetEncryptionLevel(int level, CDllProxyInfo* pProxyInfo);

	//set info for the local proxy
	void SetLocalProxyInfo(char* ip, char* port, char* username, char* password);

	//try to connect all proxies
	void AutoConnect();

	//try to Estabilish new data channel
	int EstablishNewDataChannel(CDllProxyInfo* pProxyInfo , char* IDOfPartner);

	//try to connect a socket using the proxy settings
	bool ConnectViaProxy(APISocket::CSocket* pSock, const char* szIp, unsigned int nPort);

	void ProxyError(CDllProxyInfo* pProxyInfo);

protected:
	//read the Internet Explorer settings about proxy
	int GetIEProxySettings(std::string& strProxyIP, unsigned int& nProxyPort);

protected:
	//find a proxy in the list by CDllProxyInfo
	const CProxyConnection* FindProxy(CDllProxyInfo* pProxyInfo);

	static unsigned long __stdcall ReconectProxiesThreadProc(LPVOID lpParameter);
	void* m_hReconectProxiesThread;
	unsigned long m_dwReconectProxiesThread;

protected:
	std::list<CProxyConnection*> m_listProxies;

	CDllProxyInfo* m_pLocalProxy;
	bool m_bLocalProxy;

	CCritSection m_critSection;

	CProxyConnect m_proxyConnect;
	
	bool shouldQuit;
	bool hasQuit;
	CDllProxyInfo *proxyError;
};

#endif