#ifndef _ECHOSRVDATACHANNELS_H
#define _ECHOSRVDATACHANNELS_H

#if _MSC_VER > 1000
#pragma once
#endif 

#include "DataChannelSocket.h"

class CDataChannel;

//data channel to echoServer
class CEchoSrvDataChannel :
	public CDataChannelSocket
{
public:
	CEchoSrvDataChannel(CDataChannel* pDataChannel);
	virtual ~CEchoSrvDataChannel(void);

	//connect to echoServer
	int Connect(const char* szServer, unsigned int nPort, const char* szChannelCode, const char* szMyID);	

	//notification : there are data to read
	//[in] len : length of data
	//[in] buff: data 
	virtual void OnReceive(char* buff, int len);

	//notification : can send data
	//[in] len : length of data
	//[in] buff: data 
	virtual void OnSend(char* buff, int& len);
	
	virtual void OnError(int error);
	
	virtual void OnTimer();
	
	void SetOffLoadingDataChannel(bool value) { m_bOffLoadingDataChannel = value; }
	void ResetOffLoadingTimer();
	void ResetRetryTimer();
	int m_nRetryCounter;
	bool m_bOffLoadingDataChannel;
	bool m_fStartRetry;

protected:
	//construct message to send to echoServer
	void SendMessage(DWORD message, char *data, unsigned int datalen);
	//connect the socket
	bool Connect(const char* szIP, unsigned int nPort);
	
protected:
	
	DWORD m_dwOffLoadingTime;
	DWORD m_dwRetryTime;
};

#endif