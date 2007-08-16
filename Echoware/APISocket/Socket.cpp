#include "apisocket.h"
#include <stdio.h>

#include "errno.h"
#include <sys/types.h>
#include <sys/socket.h>
#include "sys/fcntl.h"
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>

#include "../EchoToOSX.h"
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "globals.h"

APISocket::CSocket::CSocket(int sock/*=0*/)
{
	m_sock = sock;

	errored = false;
	m_nConnectTimeout = m_nSendTimeout = m_nRecvTimeout = m_nAcceptTimeout = -1;

	m_bBlockMode = false;
	m_hAsyncThread = 0;
	m_dwAsyncThread = 0;
	inAsyncNotificationLoop = false;
	StartAsync();
}

APISocket::CSocket::CSocket(const CSocket& other)
{
	m_sock = other.m_sock;

	errored = false;
	m_nConnectTimeout = other.m_nConnectTimeout;
	m_nSendTimeout = other.m_nSendTimeout;
	m_nRecvTimeout = other.m_nRecvTimeout;
	m_nAcceptTimeout = other.m_nAcceptTimeout;

	m_bBlockMode = other.m_bBlockMode;
	m_hAsyncThread = 0;
	m_dwAsyncThread = 0;
	inAsyncNotificationLoop = false;
	StartAsync();
}

APISocket::CSocket::~CSocket()
{
	StopAsync();
	
	Close();
	m_sock = 0;
}

void APISocket::CSocket::SetTimeouts(int nConnectTimeout/*=-1*/, int nSendTimeout/*=-1*/, int nRecvTimeout/*=-1*/, int nAcceptTimeout/*=-1*/)
{
	m_nConnectTimeout = nConnectTimeout;
	m_nSendTimeout = nSendTimeout;
	m_nRecvTimeout = nRecvTimeout;
	m_nAcceptTimeout = nAcceptTimeout;
}

void APISocket::CSocket::SetBlockMode(bool bBlock)
{
	m_bBlockMode = bBlock;
	NonBlock(!bBlock);
}

int APISocket::CSocket::GetConnectTimeout()
{
	return m_nConnectTimeout;
}

int APISocket::CSocket::GetSendTimeout()
{
	return m_nSendTimeout;
}

int APISocket::CSocket::GetRecvTimeout()
{
	return m_nRecvTimeout;
}

int APISocket::CSocket::GetAcceptTimeout()
{
	return m_nAcceptTimeout;
}

bool APISocket::CSocket::GetBlockMode()
{
	return m_bBlockMode;
}

bool APISocket::CSocket::Create(unsigned int nPort /*=0*/)
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);

	if (!sock)
		sock = socket(AF_INET, SOCK_STREAM, 0);

	if (!(sock < 0)) 
	{
		m_sock = sock;
		SetSockOption(true);
		return true;
	}
	
	return false;
}

unsigned int APISocket::CSocket::Bind(unsigned int nPort/*=0*/, bool bLocal/*=true*/)
{
	struct sockaddr_in self;
	memset(&self, 0, sizeof(self));

	self.sin_family = AF_INET;
	self.sin_port = htons(nPort);
	if (!bLocal)
		self.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		self.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	
	if (bind(m_sock, (struct sockaddr *)&self, sizeof(self)) != 0)
		return 0;

	if (!nPort)
		GetLocalPort(nPort);

	return nPort;
}

bool APISocket::CSocket::Listen(int backlog)
{
	return !listen(m_sock, backlog);
}

int APISocket::CSocket::Accept(int& sock)
{
	struct sockaddr_in client_addr;
	int addrlen=sizeof(client_addr);
  	
	sock = 0;

	if (!m_bBlockMode)
	{
		int rc = 0;
		if ((rc = WaitNonBlockCompletion(m_nAcceptTimeout, 0)) != 0)
			return rc;
	}
	
	sock = accept(m_sock,(struct sockaddr  *)&client_addr,(socklen_t *) &addrlen);
	
	if (sock == INVALID_SOCKET)
		return -1;

    return 0;    
}

int APISocket::CSocket::Connect(const char* szServer, unsigned int nPort)
{
	if (!m_sock)
		return -1;

	errored = false;
	StartAsync();

	struct sockaddr_in sockAddr;
	ZeroMemory(&sockAddr, sizeof(sockAddr));

	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(nPort);
	sockAddr.sin_addr.s_addr = inet_addr(szServer);

	if (sockAddr.sin_addr.s_addr == INADDR_NONE)
	{
		LPHOSTENT lphost;
		lphost = gethostbyname(szServer);
		if (lphost != NULL)
			sockAddr.sin_addr.s_addr = ((struct in_addr *)lphost->h_addr)->s_addr;
	}

	if (connect(m_sock, (struct sockaddr*)&sockAddr, sizeof(sockAddr)) != 0)
	{
		if (!m_bBlockMode)
		{
			while (!inAsyncNotificationLoop) Sleep(10000);
			return WaitNonBlockCompletion(m_nConnectTimeout, 1);
		}
	}

	while (!inAsyncNotificationLoop) Sleep(10000);

	return 0;
}

bool APISocket::CSocket::Close()
{
	bool ret = false;
	Shutdown(SD_BOTH);
	if (m_sock)
		ret = !closesocket(m_sock);
	m_sock = 0;
	return ret;
}

bool APISocket::CSocket::Shutdown(int how)
{
	if (m_sock)
		return !shutdown(m_sock, how);
	else
		return false;
}

int APISocket::CSocket::Send(const char* buff, int& nLen, int nFlag/*=0*/)
{
	nLen=send(m_sock, buff, nLen, nFlag);

	if (nLen<=0)
	{
		if (nLen==SOCKET_ERROR)
		{
			nLen=0;
			int err;
			uint errlen = sizeof(err);
			getsockopt(m_sock, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
			return (err == WSAEWOULDBLOCK)?(0):(-1);
		}

		return -1;
	}

	return 0;
}

int APISocket::CSocket::Receive(char* buff, int& nLength, int nFlag/*=0*/)
{
	nLength=recv(m_sock, buff, nLength, nFlag);

	if (nLength<=0)
	{
		if (nLength==SOCKET_ERROR)
		{
			nLength=0;
			int err;
			uint errlen = sizeof(err);
			getsockopt(m_sock, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
			return (err == WSAEWOULDBLOCK)?(0):(-1);
		}
		return -1;
	}

	return 0;
}

//gets the TCP_NODELAY socket option
//return true for success and false for error to get the TCP_NODELAY socket option
bool APISocket::CSocket::GetSockOption(bool& bTcpNodelay)
{
	int on;
	uint len=sizeof(on);
	bool ret=!getsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&on, &len);
	bTcpNodelay=(on!=0);
	return ret;
}

//sets the TCP_NODELAY socket option
//return true for success and false for error to set the TCP_NODELAY socket option
bool APISocket::CSocket::SetSockOption(bool bTcpNodelay)
{
	int on=(int)bTcpNodelay;
	return !setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on));
}

int APISocket::CSocket::GetLastError()
{
	return WSAGetLastError();
}

bool APISocket::CSocket::NonBlock(bool bNonBlock)
{
	if (bNonBlock)
		return (fcntl(m_sock, F_SETFL, O_NONBLOCK) != -1);
	else
		return (fcntl(m_sock, F_SETFL, fcntl(m_sock, F_GETFL, 0) & ~O_NONBLOCK) != -1);
}

int APISocket::CSocket::WaitNonBlockCompletion(int nTimeout, int nType)
{
	fd_set fd;
	struct timeval time;

	time.tv_sec = nTimeout/1000;
	time.tv_usec = (nTimeout-time.tv_sec*1000)*1000;

	FD_ZERO(&fd);
	FD_SET(m_sock, &fd);
	
	int rc=0;

	if (nType==0)
		rc = select(m_sock+1, &fd, NULL, NULL, &time);
	else if (nType==1)
		rc = select(m_sock+1, NULL, &fd, NULL, &time);
	
	if (rc==-1)
		return -1;// error
	else if (rc==0)
		return 1;//timeout
		
	if (FD_ISSET(m_sock, &fd))
		return 0;
	else
		return -1;
}

bool APISocket::CSocket::GetLocalPort(unsigned int& nPort)
{
	struct sockaddr_in self;
	memset(&self, 0, sizeof(self));

	uint len=sizeof(self);

	if (getsockname( m_sock, (struct sockaddr*)&self, &len ))
		return false;

	nPort=ntohs(self.sin_port);

	return true;
}

bool APISocket::CSocket::GetRemotePort(unsigned int& nPort)
{
	struct sockaddr_in self;
	memset(&self, 0, sizeof(self));

	int len=sizeof(self);

	if (getpeername( m_sock, (struct sockaddr*)&self, (socklen_t *) &len ))
		return false;

	nPort=ntohs(self.sin_port);

	return true;
}

bool APISocket::CSocket::GetRemoteIP(char* szRemoteIP)
{
	struct sockaddr_in self;
	memset(&self, 0, sizeof(self));

	uint len=sizeof(self);

	if (getpeername( m_sock, (struct sockaddr*)&self, &len ))
		return false;
	
	sprintf(szRemoteIP, "%s", inet_ntoa(self.sin_addr));	

	return true;
}

int APISocket::CSocket::Attach(int sock)
{
	int old=m_sock;

	m_sock=sock;

	return old;
}

int APISocket::CSocket::Detach()
{
	int old=m_sock;

	m_sock=0;

	return old;
}

void APISocket::CSocket::OnClose()
{
}

void APISocket::CSocket::OnError(int error)
{
	errored = true;
	g_globals.m_logger.WriteFormated("CSocket::OnError sock = %d err = %d", m_sock, error);
}

void APISocket::CSocket::OnReceive(char* buff, int len)
{
}

void APISocket::CSocket::OnSend(char* buff, int &len)
{
	len = 0;
}

void APISocket::CSocket::OnWrite()
{
	errored = false;
	int r = Send();
}

void APISocket::CSocket::OnRead()
{
	errored = false;
	int len_to_read = ReadableByteCount();
	if (len_to_read <= 0) return;
	if (len_to_read > 65000) /////////////
		len_to_read = 65000; ////////////
	char *buff = new char[len_to_read];
	int len_readed = len_to_read;
	int r = Receive(buff, len_readed);
	if (len_readed <= 0)
	{
		delete [] buff;
		return;
	}
	
	OnReceive(buff, len_readed);
	delete[] buff;
}

int APISocket::CSocket::ReadableByteCount()
{
	if (!m_sock)
		return 0;

	int	bytesAvailable = 0;

	if (ioctl(m_sock, FIONREAD, &bytesAvailable) == -1)
	{
		if (errno == EINVAL)
			bytesAvailable = -1;
		else
			bytesAvailable = 0;
	}
	
	return bytesAvailable;
}

int APISocket::CSocket::Send()
{
	if (!m_sock || errored)
		return 1;
	char *buff = new char[MAX_BUFFER];
	int len = MAX_BUFFER;
	OnSend(buff, len);
	int ret = 0;
	if (len > 0)
		ret = Send(buff, len);
	delete [] buff;
	return ret;
}

int APISocket::CSocket::getSocket()
{
	return m_sock;
}

bool APISocket::CSocket::StartAsync()
{
	if (m_hAsyncThread)
		return true;

	inAsyncNotificationLoop = false;
	shouldAsyncQuit = 0;
	hasAsyncQuit = 0;
	if (!(m_hAsyncThread = CreateThread(0, 0, AsyncThreadProc, this, 0, &m_dwAsyncThread)))
		return false;
	return true;
}

bool APISocket::CSocket::StopAsync()
{
	if (!m_hAsyncThread)
		return true;

	shouldAsyncQuit = true;
	m_csAsync.Lock();
	m_csAsync.Unlock();
	bool stopResult = ShutdownThread(m_hAsyncThread, THREAD_STOP_TIMEOUT, &shouldAsyncQuit, &hasAsyncQuit);
	m_hAsyncThread = 0;
	m_dwAsyncThread = 0;
	return stopResult;
}

void APISocket::CSocket::AsyncNotifications()
{
	if (!m_sock) return;

	fd_set read_fds, write_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_SET(m_sock, &read_fds);
	FD_SET(m_sock, &write_fds);

	int res = select(m_sock + 1, &read_fds, &write_fds, NULL, NULL);
	if (res > 0)
	{
		if (FD_ISSET(m_sock, &read_fds))
		{
			if (ReadableByteCount())
			{
				inAsyncNotificationLoop = true;
				OnRead();
			}
			else
			{
				inAsyncNotificationLoop = true;
				OnError(GetLastError());
				shouldAsyncQuit = true;
				return;
			}
		}

		if (FD_ISSET(m_sock, &write_fds))
		{
			inAsyncNotificationLoop = true;
			OnWrite();
		}
	}
	inAsyncNotificationLoop = true;
}

void APISocket::CSocket::AsyncNotificationLoop()
{
	while (!shouldAsyncQuit)
	{
		AsyncNotifications();
		Sleep(10000);
	}
	inAsyncNotificationLoop = false;
	m_hAsyncThread = 0;
	m_dwAsyncThread = 0;
	hasAsyncQuit = 1;
}

unsigned long APISocket::CSocket::AsyncThreadProc(void* lpParameter)
{
	APISocket::CSocket *socket = (APISocket::CSocket*)lpParameter;
	socket->m_csAsync.Lock();
	socket->AsyncNotificationLoop();
	socket->m_csAsync.Unlock();
	Sleep(50000);
	return 0;
}
