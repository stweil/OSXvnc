#ifndef _LOCALDATACHANNELS_H
#define _LOCALDATACHANNELS_H

#if _MSC_VER > 1000
#pragma once
#endif 

#include "datachannelsocket.h"

#define RECONNECT

class CDataChannel;

//local data channel class
class CLocalDataChannel :
	public CDataChannelSocket
{
public:
	CLocalDataChannel(CDataChannel* pDataChannel);
	virtual ~CLocalDataChannel(void);

	//notification : there are data to read
	//[in] len : length of data
	//[in] buff: data 
	virtual void OnReceive(char* buff, int len);

	//notification : can send data
	//[in] len : length of data
	//[in] buff: data 
	virtual void OnSend(char* buff, int& len);
	
	bool StartSend();
protected:
	bool m_bRFB;
};

#endif.30