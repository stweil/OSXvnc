#include "stdafx.h"
#include "APISocket.h"

#include "EchoToOSX.h"

APISocket::CClientSocket::CClientSocket(int sock /*=0*/)
	:APISocket::CSocket(sock)
{
	m_hRoutineThread=0;
	m_dwRoutineThread=0;
}

APISocket::CClientSocket::~CClientSocket(void)
{
}

bool APISocket::CClientSocket::StartSend()
{
	bool res = StartRoutine();
	int ret = Send();
	
	return res && (ret == 0);
}

bool APISocket::CClientSocket::StopSend(unsigned long dwWaitTimeout)
{
	return StopRoutine(dwWaitTimeout);
}

bool APISocket::CClientSocket::StartRoutine()
{
	if (m_hRoutineThread)
		return true;

	routineShouldQuit=0;
	routineHasQuit=0;
	if (!(m_hRoutineThread=CreateThread(0, 0, RoutineThreadProc, this, 0, &m_dwRoutineThread)))
		return false;
	return true;
}

bool APISocket::CClientSocket::StopRoutine(unsigned long dwWaitTimeout)
{
	if (!m_hRoutineThread)
		return true;

	bool termResult = ShutdownThread(m_hRoutineThread, dwWaitTimeout, &routineShouldQuit, &routineHasQuit);
	m_hRoutineThread=0;
	m_dwRoutineThread=0;
	return termResult;
}

void APISocket::CClientSocket::OnTimer()
{
}

unsigned long __stdcall APISocket::CClientSocket::RoutineThreadProc(void* lpParameter)
{
	APISocket::CClientSocket* pClient =	(APISocket::CClientSocket*) lpParameter;
	while (true)
	{
		if (pClient->routineShouldQuit)
			break;
		pClient->OnTimer();
		Sleep(500000);
	}
	pClient->routineHasQuit = 1;
	return 0;
}