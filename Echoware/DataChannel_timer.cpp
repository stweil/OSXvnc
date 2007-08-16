#include "StdAfx.h"
#include "DataChannel.h"
#include "DataChannels.h"
#include "LocalDataChannel.h"
#include "EchoSrvDataChannel.h"
#include "DllProxyInfo.h"

#define RSA_PUBLIC_KEY	80

#pragma warning(disable : 4355)
CDataChannel::CDataChannel(CDataChannels* pDataChannels, const char* szChannelCode, 
						   const char* szSessionKey, bool bEncryptDecrypt) : 
	 m_localListener(this), m_bEncryptDecrypt(bEncryptDecrypt)
{	
	m_pLocalDCTimer=new CLocalDCTimer(this);
	m_bLocalDC=false;

	m_pDataChannels=pDataChannels;
	m_pLocalDataChannel=new CLocalDataChannel(this);
	m_pEchoServerDataChannel=new CEchoSrvDataChannel(this);

	const CDllProxyInfo* pProxyInfo=pDataChannels->GetProxyInfo();

	m_pLocalDataChannel->SetTimeouts(pProxyInfo->GetConnectTimeout()*1000,
									pProxyInfo->GetSendTimeout()*1000,
									pProxyInfo->GetReceiveTimeout()*1000);
	m_pEchoServerDataChannel->SetTimeouts(pProxyInfo->GetConnectTimeout()*1000,
									pProxyInfo->GetSendTimeout()*1000,
									pProxyInfo->GetReceiveTimeout()*1000);

	m_szChannelCode=new char[12];
	strcpy(m_szChannelCode, szChannelCode);

	m_szSessionKey=new char[1024];
	strcpy(m_szSessionKey, szSessionKey);

	m_aes.SetEncryptKey((const unsigned char*)m_szSessionKey, 128); 
	
	m_aes.SetDecryptKey((const unsigned char*)m_szSessionKey, 128); 	

	g_globals.m_logger.WriteFormated("CDataChannel: New data channel: code=%s , encrypt=%c", szChannelCode, (m_bEncryptDecrypt)?('Y'):('N'));
}
#pragma warning(default : 4355)

CDataChannel::~CDataChannel(void)
{
	if (m_pLocalDCTimer)
	{
		delete m_pLocalDCTimer;
		m_pLocalDCTimer=0;
	}

	StopListen();

	if (m_pEchoServerDataChannel)
	{		
		m_pEchoServerDataChannel->Shutdown(SD_BOTH);
		m_pEchoServerDataChannel->Close();

		m_pEchoServerDataChannel->StopReceive(THREAD_STOP_TIMEOUT);
		m_pEchoServerDataChannel->StopSend(THREAD_STOP_TIMEOUT);		
	}

	if (m_pLocalDataChannel)
	{	
		m_pLocalDataChannel->Shutdown(SD_BOTH);	
		m_pLocalDataChannel->Close();		

		m_pLocalDataChannel->StopReceive(THREAD_STOP_TIMEOUT);
		m_pLocalDataChannel->StopSend(THREAD_STOP_TIMEOUT);		
	}	

	if (m_pEchoServerDataChannel)
	{
		delete m_pEchoServerDataChannel;
		m_pEchoServerDataChannel=0;
	}	

	if (m_pLocalDataChannel)
	{
		delete m_pLocalDataChannel;
		m_pLocalDataChannel=0;
	}	

	delete []m_szChannelCode;
	delete []m_szSessionKey;	
}

bool CDataChannel::ConnectEchoServer()
{
	g_globals.m_logger.Write("=>ConnectEchoServer");
	if (!m_pEchoServerDataChannel->Create() ||
		m_pEchoServerDataChannel->Connect(m_pDataChannels->GetProxyInfo()->GetIP(),
										atoi(m_pDataChannels->GetProxyInfo()->GetPort()),
										m_szChannelCode, 
										m_pDataChannels->GetProxyInfo()->GetMyID())!=0)
	{
		OnError(m_pEchoServerDataChannel, 1);

		g_globals.m_logger.WriteFormated("CDataChannel: Error connect data channel %p to channel code %s", this, m_szChannelCode);	

		return false;
	}

	m_pLocalDataChannel->SetPairChannel(m_pEchoServerDataChannel);
	m_pEchoServerDataChannel->SetPairChannel(m_pLocalDataChannel);	

	m_pEchoServerDataChannel->SetOffLoadingDataChannel();
	m_pEchoServerDataChannel->StartReceive();
	m_pEchoServerDataChannel->StartSend();

	//ConnectLocalServer();

	m_pLocalDCTimer->SetTimer(CONNECTION_TO_OFFLOAD_TIMER_VALUE);	

	g_globals.m_logger.Write("<=ConnectEchoServer");

	return true;
}

bool CDataChannel::ConnectLocalServer()
{
	m_pDataChannels->LocalConnectDataChannel(this);

	return true;
}

//try to connect to the local server on the port nPort
bool CDataChannel::ConnectLocalServer(unsigned int nPort)
{
	g_globals.m_logger.Write("=>CDataChannel::ConnectLocalServer");
	m_crtLocalDCSection.Lock();	

	if (m_bLocalDC)
	{
		m_crtLocalDCSection.Unlock();
		return true;
	}

	if (m_pLocalDCTimer)
	{
		delete m_pLocalDCTimer;
		m_pLocalDCTimer=0;
	}

	g_globals.m_logger.Write("=>Try connect local server");

	APISocket::CClientSocket client;
	if (!client.Create())
	{
		m_crtLocalDCSection.Unlock();
		g_globals.m_logger.Write("<=Try connect local server - error create");
		return false;
	}

	client.SetBlockMode(false);
	const CDllProxyInfo* pProxyInfo=m_pDataChannels->GetProxyInfo();
	client.SetTimeouts(pProxyInfo->GetConnectTimeout(), pProxyInfo->GetSendTimeout(), pProxyInfo->GetReceiveTimeout());
	if (client.Connect("127.0.0.1", nPort)!=0)
	{
		m_crtLocalDCSection.Unlock();
		g_globals.m_logger.Write("<=Try connect local server - error connect");
		return false;
	}

	m_bLocalDC=true;

	OnLocalDataChannel(client.Detach(), true);

	g_globals.m_logger.Write("<=Try connect local server - success");

	m_crtLocalDCSection.Unlock();	

	return true;
}

//start to listen for local connections
bool CDataChannel::Listen(unsigned int& nPort)
{	
	g_globals.m_logger.Write("CDataChannel: Enter Listen for local client");	
	if (!m_localListener.Create())
		return false;

	if (!m_localListener.Listen())
		return false;

	nPort=m_localListener.GetListenPort();

	if (!m_localListener.StartAccept())
		return false;

	g_globals.m_logger.Write("CDataChannel: Listen for local client started");	

	return true;
}

//stop listen for local connections
void CDataChannel::StopListen()
{
	m_localListener.Close();
	m_localListener.StopAccept(5000);
}

//notify that there was an error on sockets operations
void CDataChannel::OnError(APISocket::CSocket* pSocket, unsigned int err)
{	
	g_globals.m_logger.WriteFormated("CDataChannel: Start Remove data channel %p", this); 
	if (m_pDataChannels)
		m_pDataChannels->RemoveDataChannel(this);		
	g_globals.m_logger.WriteFormated("CDataChannel: End Remove data channel %p", this); 
}

//sets the encryption level
void CDataChannel::SetEncriptionLevel(int nLevel)
{
	m_pEchoServerDataChannel->SetEncriptionLevel(nLevel);
}

//gets the encryption level
int CDataChannel::GetEncriptionLevel()
{
	return m_pEchoServerDataChannel->GetEncriptionLevel();
}

//notify that we have a local connection
//there are 2 types of local connections :  OffLoadingDataChannel or not
//try to create the data channel to echoServer
void CDataChannel::OnLocalDataChannel(unsigned int sock, bool bOffLoadingDataChannel/*=false*/)
{
	m_pLocalDataChannel->Attach(sock);
	m_pLocalDataChannel->m_bOffLoadingDataChannel=bOffLoadingDataChannel;

	if (!bOffLoadingDataChannel)
	{	
		g_globals.m_logger.WriteFormated("CDataChannel: Try connect data channel %p to channel code %s", this, m_szChannelCode);	

		if (!m_pEchoServerDataChannel->Create() ||
			m_pEchoServerDataChannel->Connect(m_pDataChannels->GetProxyInfo()->GetIP(),
											atoi(m_pDataChannels->GetProxyInfo()->GetPort()),
											m_szChannelCode, 
											m_pDataChannels->GetProxyInfo()->GetMyID())!=0)
		{
			OnError(m_pEchoServerDataChannel, 1);

			g_globals.m_logger.WriteFormated("CDataChannel: Error connect data channel %p to channel code %s", this, m_szChannelCode);	

			return;
		}

		g_globals.m_logger.WriteFormated("CDataChannel: Success connect data channel %p to channel code %s", this, m_szChannelCode);			

		m_pLocalDataChannel->SetPairChannel(m_pEchoServerDataChannel);
		m_pEchoServerDataChannel->SetPairChannel(m_pLocalDataChannel);	

		m_pEchoServerDataChannel->StartReceive();
		m_pEchoServerDataChannel->StartSend();	
	}
	
	
	m_pLocalDataChannel->StartReceive();
	m_pLocalDataChannel->StartSend();	
}
