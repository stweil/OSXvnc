#ifndef _ACCONNECTION_H
#define _ACCONNECTION_H

#if _MSC_VER > 1000
#pragma once
#endif 

#include "APISocket.h"
#include "CritSection.h"
#include "RSAKeys.h"

class CProxyConnection;
class CBuffer;

/*
Authetication channel class 
It is used for client authentication and identification on the echoServer
*/

class CACConnection :
	public APISocket::CClientSocket
{
public:
	CACConnection(CProxyConnection* pProxyConnection);
	virtual ~CACConnection(void);

	//connect to echoServer
	int Connect();

	//disconnect from echoServer
	bool Disconnect();
	
	void StopConnect();

	//sets the encryption level
	void SetEncryptionLevel(int level);	

	//find partner on echoServer
	bool FindPartner(const char* szPartener);

	//connect to peer on echoServer
	bool ConnectToPeer(const char* szPartener);
	bool ConnectToPeer(const char* szPartener, char* szChannelCode, char* szPeerPublicKey);

	//generate a session key from a public key
	char* GenerateSessionKey(char* szSessionKey, char* szPeerPublicKey)
	{
		m_rsaKeys.GenerateSessionKey(szPeerPublicKey, szSessionKey);
		return szSessionKey;
	}

protected:
	//overwrited notification functions
	void OnReceive(char* buff, int len);
	void OnSend(char* buff, int& len);
	virtual void OnError(int error);
	
	virtual void OnTimer();

	//construct message to send to echoServer
	void SendMessage(DWORD message, char *data, unsigned int datalen);
	//construct mesage MSG_PROXY_PASSWORD
	void SendEncryptedPass(char * pData,DWORD dwDataLength);	

	//connect to szIP:nPort
	bool Connect(const char* szIP, unsigned int nPort);

protected:
	CCritSection m_critSection;
	int m_nEncryptionLevel;

	//parent proxy connection
	CProxyConnection* m_pProxyConnection;

	bool m_bStopConnecting;
	bool m_bConnected;
	int m_dwConnectingStatus;

	//buffers for recv/send on socket
	CBuffer* m_pReadBuffer;
	CBuffer* m_pSendBuffer;

	CRSAKeys m_rsaKeys;
	char m_pRemotePublicKey[RSA_PUBLIC_KEY];

private:
	char m_szChannelCode[12];
	char m_szPeerID[256];
	char m_szPeerPublicKey[RSA_PUBLIC_KEY*sizeof(unsigned int)+1];

	DWORD m_dwLastIsAliveMsg;
};

#endif