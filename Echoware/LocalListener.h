#ifndef _LOCALLISTENER_H
#define _LOCALLISTENER_H

#if _MSC_VER > 1000
#pragma once
#endif 

#include "apisocket.h"

class CDataChannel;

//local listener for clients
class CLocalListener :
	public APISocket::CServerSocket
{
public:
	CLocalListener(CDataChannel* pDataChannel);
	virtual ~CLocalListener(void);

	//notifications
	virtual void OnNewClient(unsigned int sock);
	virtual void OnLeaveClient(APISocket::CSocket* pClient);

protected:
	

protected:
	bool m_bAcceptConnection;
	CDataChannel* m_pDataChannel;
};

#endif