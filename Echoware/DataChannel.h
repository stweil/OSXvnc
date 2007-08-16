#ifndef _DATACHANNEL_H
#define _DATACHANNEL_H

#if _MSC_VER > 1000
#pragma once
#endif 

class CDataChannels;
class CLocalDataChannel;
class CEchoSrvDataChannel;

class CLocalDCTimer;

#include "LocalListener.h"
#include "AES.h"

//class for pair Local connection<->echoServer connection 
class CDataChannel
{
public:
	CDataChannel(CDataChannels* pDataChannels, const char* szChannelCode,
		const char* szSessionKey, bool bEncryptDecrypt);
	virtual ~CDataChannel(void);

	//connect to echoServer
	bool ConnectEchoServer();

	//connect to local server
	bool ConnectLocalServer();

	//connect to local server
	bool ConnectLocalServer(unsigned int nPort);

	//listen for local client
	bool Listen(unsigned int& nPort);
	void StopListen();

	void OnError(APISocket::CSocket* pSocket, unsigned int err);

	//sets the encryption level for this data channel
	void SetEncriptionLevel(int nLevel);
	//gets the encryption level for this data channel
	int GetEncriptionLevel();

	//a local connection
	void OnLocalDataChannel(unsigned int sock, bool bOffLoadingDataChannel=false);
	
	const char* GetSessionKey(){return m_szSessionKey;}
	CLocalDataChannel* m_pLocalDataChannel;

protected:
	//the data channels manager
	CDataChannels* m_pDataChannels;

	CLocalListener m_localListener;
	
	CEchoSrvDataChannel* m_pEchoServerDataChannel;

	char* m_szChannelCode;	
	char* m_szSessionKey;
	
public:
	CAES m_aes;
	bool m_bEncryptDecrypt;	

	CCritSection m_crtLocalDCSection;	
	bool m_bLocalDC;	
};


#endif