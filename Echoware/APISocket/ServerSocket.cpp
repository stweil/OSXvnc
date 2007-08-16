#include "stdafx.h"
#include "APISocket.h"

APISocket::CServerSocket::CServerSocket(unsigned int nPort/*=0*/)
	:APISocket::CSocket()
{
	m_nPort=nPort;

	m_hAcceptThread=0;
	m_dwAcceptThread=0;
}

APISocket::CServerSocket::~CServerSocket(void)
{
}

bool APISocket::CServerSocket::Create()
{	
	if (!CSocket::Create(m_nPort))
		return false;

	if (!(m_nPort=Bind(m_nPort)))
		return false;
		
	return true;
}

bool APISocket::CServerSocket::Listen(int backlog)
{
	return CSocket::Listen();
}

unsigned int APISocket::CServerSocket::GetListenPort()
{
	return m_nPort;
}

bool APISocket::CServerSocket::StartAccept()
{	
	if (m_hAcceptThread)
		return true;

	shouldQuit = 0;
	hasQuit = 0;
	if (!(m_hAcceptThread=CreateThread(0, 0, AcceptThreadProc, this, 0, &m_dwAcceptThread)))
		return false;
	
	return true;
}

bool APISocket::CServerSocket::StopAccept(unsigned long dwWaitTimeout)
{
	bool termResult = ShutdownThread(m_hAcceptThread, dwWaitTimeout, &shouldQuit, &hasQuit);
	m_hAcceptThread = 0;
	m_dwAcceptThread = 0;
	return termResult;
}

void APISocket::CServerSocket::OnNewClient(int sock)
{	
}

void APISocket::CServerSocket::OnLeaveClient(CSocket* pClient)
{
}

unsigned long __stdcall APISocket::CServerSocket::AcceptThreadProc(void* lpParameter)
{
	CServerSocket* pServerSock=	(CServerSocket*) lpParameter;	

	int sock;

	while (true)
	{
		if (pServerSock->shouldQuit)
			break;

		int rc=pServerSock->Accept(sock);
		if (rc==0)
			pServerSock->OnNewClient(sock);
		else if (rc==-1)
		{
			if (pServerSock->shouldQuit)
				break;
			
			pServerSock->OnError(pServerSock->GetLastError());	
			Sleep(10000);
		}
		else if (rc==1)
			Sleep(1000);
	}
	pServerSock->hasQuit=1;
	return 0;
}
