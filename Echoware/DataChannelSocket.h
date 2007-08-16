#ifndef _DATACHANNELSOCKET_H
#define _DATACHANNELSOCKET_H

#if _MSC_VER > 1000
#pragma once
#endif 

#include "apisocket.h"

class CDataChannel;
class CBuffer;

//data channel socket class
//a base class for CLocalDataChannel class and CEchoSrvDataChannel class
class CDataChannelSocket :
	public APISocket::CClientSocket
{
public:
	CDataChannelSocket(CDataChannel* pDataChannel);
	virtual ~CDataChannelSocket(void);

	//notification : there are data to read
	//[in] len : length of data
	//[in] buff: data 
	virtual void OnReceive(char* buff, int len);

	//notification : can send data
	//[in] len : length of data
	//[in] buff: data 
	virtual void OnSend(char* buff, int& len);

	//notification : receive error
	//[out] error
	virtual void OnError(int error);

	//sets the pair data channel for this data channel
	void SetPairChannel(CDataChannelSocket* pPairChannel);
	//gets the pair data channel for this data channel
	CDataChannelSocket* GetPairChannel();

	void ReadData(char* buff, unsigned int& read_size);
	void WriteData(char* buff, unsigned int write_size);
	
	//sets the encryption level for this data channel
	void SetEncriptionLevel(int nLevel);
	//gets the encryption level for this data channel
	int GetEncriptionLevel();

protected:
	//the parent data channel
	CDataChannel* m_pDataChannel;

	//buffers for recv/send on socket
	CBuffer* m_pRecvBuffer;
	CBuffer* m_pSendBuffer;

	//the pair data channel for this data channel
	CDataChannelSocket* m_pPairChannel;

	int m_nEncriptionLevel;
};

#endif