#ifndef _PROXCONNECTION_H
#define _PROXCONNECTION_H

#if _MSC_VER > 1000
#pragma once
#endif 

#include "CritSection.h"

class CDllProxyInfo;
class CACConnection;
class CDataChannels;

//proxy connection class
class CProxyConnection
{
public:
	CProxyConnection(void);
	CProxyConnection(CDllProxyInfo* pProxyInfo);
	virtual ~CProxyConnection(void);

	int Connect();
	bool Disconnect();
	void StopConnecting();

	//sets the encryption level for this proxy connection
	void SetEncryptionLevel(int level);
	
	CDllProxyInfo* GetProxyInfo();

	//establish new data channel to partner
	int EstablishNewDataChannel(char* IDOfPartner);

	void OnError(int error);

	//the partner establish to me
	void OnRemotePartnerConnect(char* szDataChannelCode, char* IDOfPartner, char* szPeerPublicKey);	

protected:
	//the proxy info
	CDllProxyInfo* m_pProxyInfo;
	//the authenticate channel connection
	CACConnection* m_pACConnection;

	CCritSection m_critSection;
	int m_nEncryptionLevel;	

	//all data channels for this proxy connection(echoServer)
	CDataChannels* m_pDataChannels;
};

#endif