#include "StdAfx.h"
#include "locallistener.h"
#include "DataChannel.h"

CLocalListener::CLocalListener(CDataChannel* pDataChannel)
{
	m_pDataChannel=pDataChannel;
	m_bAcceptConnection=false;
}

CLocalListener::~CLocalListener(void)
{
	Close();
	StopAccept(5000);
}

//notify message
//a local client is connected and can create a local data channel
void CLocalListener::OnNewClient(unsigned int sock)
{	
	APISocket::CSocket sock_obj(sock);	

	sock_obj.SetSockOption(true);

	m_pDataChannel->OnLocalDataChannel(sock_obj.Detach());

	m_bAcceptConnection=false;
}

void CLocalListener::OnLeaveClient(APISocket::CSocket*)
{
	m_bAcceptConnection=true;
}
