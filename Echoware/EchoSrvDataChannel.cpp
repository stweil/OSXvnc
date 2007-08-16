#include "StdAfx.h"
#include "echosrvdatachannel.h"
#include "localdatachannel.h"
#include "NetPacket.h"
#include "DataChannel.h"

CEchoSrvDataChannel::CEchoSrvDataChannel(CDataChannel* pDataChannel)
:CDataChannelSocket(pDataChannel)
{
	m_bOffLoadingDataChannel = false;
	ResetOffLoadingTimer();
	ResetRetryTimer();
	m_nRetryCounter = 0;
	m_fStartRetry = false;
}

CEchoSrvDataChannel::~CEchoSrvDataChannel(void)
{
	g_globals.m_logger.WriteFormated("CEchoSrvDataChannel: send buff=%d, rec buff=%d", m_pSendBuffer->Size(), m_pRecvBuffer->Size());
}

bool CEchoSrvDataChannel::Connect(const char* szIP, unsigned int nPort)
{
	if (CDataChannelSocket::Connect(szIP, nPort)==0)
		return true;

	return g_globals.m_proxiesManager.ConnectViaProxy(this, szIP, nPort);
}

//connects tis data channel to echoServer and authenticating this connection
int CEchoSrvDataChannel::Connect(const char* szServer, unsigned int nPort, const char* szChannelCode, const char* szMyID)
{	
	if (!Connect(szServer, nPort))
		return -1;

	char *pBuff = NULL;
	DWORD dwTotalLength = 0;
	DWORD dwTemp = 0;
	
	dwTotalLength = ID_STRING_SIZE;
	dwTotalLength += CHANNEL_CODE_SIZE;//(DWORD)strlen(szChannelCode)+1;
	pBuff = new char[dwTotalLength];
	g_globals.GetFormattedID(pBuff, dwTotalLength, szMyID);
	dwTemp = ID_STRING_SIZE;
	memcpy(pBuff+dwTemp, szChannelCode, strlen(szChannelCode)+1);

	SendMessage(MSG_DATA_CHANNEL_CONNECT,pBuff,dwTotalLength);

	if(pBuff)
		delete pBuff;
		
	m_dwOffLoadingTime = GetTickCount();

	return 0;
}

//message composer
void CEchoSrvDataChannel::SendMessage(DWORD message, char *data, unsigned int datalen)
{
	char*	lpBuf = NULL;
	DWORD	nBytesSent = 0;

	CProxyMsg msg(message );

	NetPacketHeader header;
	
	header.len = msg.MakeMessage(&header, lpBuf, data, datalen); //create the message	
		
	DWORD dwToSend = OSSwapHostToLittleInt32(header.len + sizeof(DWORD));
	m_pSendBuffer->Write(&dwToSend, sizeof(DWORD));
	m_pSendBuffer->Write(lpBuf, header.len);
	Send();

	if (lpBuf)
		delete lpBuf;
}

void CEchoSrvDataChannel::OnReceive(char* buff, int len)
{
	CDataChannelSocket::OnReceive(buff, len);
	if (m_bOffLoadingDataChannel)
		m_pDataChannel->ConnectLocalServer();
	m_pPairChannel->Send();
}

void CEchoSrvDataChannel::OnSend(char* buff, int& len)
{
	CDataChannelSocket::OnSend(buff, len);
	if (len > 0)
		g_globals.m_logger.WriteFormated("CEchoSrvDataChannel::OnSend sock = %d len = %d", getSocket(), len);
}

void CEchoSrvDataChannel::OnError(int error)
{
	CDataChannelSocket::OnError(error);
}

void CEchoSrvDataChannel::ResetOffLoadingTimer()
{
	m_dwOffLoadingTime = GetTickCount();
}

void CEchoSrvDataChannel::ResetRetryTimer()
{
	m_dwRetryTime = GetTickCount();
}

void CEchoSrvDataChannel::OnTimer()
{
	APISocket::CClientSocket::OnTimer();

	if (m_bOffLoadingDataChannel && GetTickCount() - m_dwOffLoadingTime >= CONNECTION_TO_OFFLOAD_TIMER_VALUE)
	{
		m_pDataChannel->ConnectLocalServer();
		ResetOffLoadingTimer();
	}
	
	if (m_fStartRetry && m_bOffLoadingDataChannel && GetTickCount() - m_dwRetryTime >= RECONNECTION_TO_OFFLOAD_TIMER_VALUE)
	{
		g_globals.m_logger.WriteFormated("CEchoSrvDataChannel::OnTimer Start retrying the localdatachannel");
		m_pDataChannel->m_pLocalDataChannel->StartAsync();
		m_pDataChannel->m_pLocalDataChannel->StartSend();
		ResetRetryTimer();
		m_nRetryCounter++;
		m_fStartRetry = false;
		g_globals.m_logger.WriteFormated("CEchoSrvDataChannel::OnTimer End retrying the localdatachannel");
	}
}
