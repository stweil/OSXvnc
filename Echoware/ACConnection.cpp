#include "StdAfx.h"
#include "acconnection.h"
#include "ProxyConnection.h"
#include "DllProxyInfo.h"
#include "NetPacket.h"

#include <sys/cdefs.h>
#include <sys/utfconv.h>
#include <libkern/OSByteOrder.h>
//#include <CoreFoundation/CFByteOrder.h>

using namespace APISocket;

//time for send live message
#define SEND_LIVE_MESSAGE	10000

CACConnection::CACConnection(CProxyConnection* pProxyConnection) : CClientSocket()
{
	m_pProxyConnection = pProxyConnection;

	m_nEncryptionLevel = 1;		

	m_bConnected = false;
	m_bStopConnecting = false;

	m_dwConnectingStatus=0;

	m_pReadBuffer = new CBuffer();
	m_pSendBuffer = new CBuffer();

	m_dwLastIsAliveMsg = GetTickCount();
}

CACConnection::~CACConnection(void)
{	
	StopSend(THREAD_STOP_TIMEOUT);

	Close();

	delete m_pReadBuffer;
	delete m_pSendBuffer;
}

void CACConnection::StopConnect()
{
	m_bStopConnecting = true;
}

//connect the socket to szIP on port nPort
//first, try directly and if failed then try use proxy settings
bool CACConnection::Connect(const char* szIP, unsigned int nPort)
{
	bool res = false;
	if (CClientSocket::Connect(szIP, nPort) == 0)
		res = true;

	if (!res)
		res = g_globals.m_proxiesManager.ConnectViaProxy(this, szIP, nPort);
	
	return res;
}

//try connect to echoServer
int CACConnection::Connect()
{
	int nRet=CONNECTION_SUCCESSFUL;

	if (m_bConnected)
	{
		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_CONNECTING, false);
		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_AUTHENTICATING, false);
		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_AUTHENTICATION_FAILED, false);
		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_ESTABLISHING_DATA_CHANNEL, false);
		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_SEARCHING_FOR_PARTNER, false);
		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_DISCONNECTED_FROM_PROXY, false);
		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_CONNECTED, true);
		return PROXY_ALREADY_CONNECTED;
	}
	
	//create the socket
	Create();

	g_globals.m_logger.Write("CACConnection: Enter connect");

	m_critSection.Lock();

	CDllProxyInfo* pProxyInfo = m_pProxyConnection->GetProxyInfo();

	if (pProxyInfo->GetConnectTimeout()!=-1)
	{
		g_globals.m_logger.WriteFormated("CACConnection: Timeout=%d", pProxyInfo->GetConnectTimeout());

		//set the connection timeout
		CClientSocket::SetTimeouts(pProxyInfo->GetConnectTimeout()*1000);
		SetBlockMode(false);
	}

	pProxyInfo->SetStatus(STATUS_DISCONNECTED_FROM_PROXY, false);

	g_globals.m_logger.WriteFormated("\tCACConnection: Try connect %s:%s %s", pProxyInfo->GetIP(), pProxyInfo->GetPort(), pProxyInfo->GetMyID());
	
	//connect
	bool a = Connect(pProxyInfo->GetIP(), atoi(pProxyInfo->GetPort()));
	if (a)
	{
		CClientSocket::SetTimeouts(pProxyInfo->GetConnectTimeout()*1000, pProxyInfo->GetSendTimeout()*1000, pProxyInfo->GetReceiveTimeout()*1000);
		unsigned int conn_timeout = (unsigned int)pProxyInfo->GetConnectTimeout() * 1000;
		m_pReadBuffer->Empty();
		m_pSendBuffer->Empty();
	
		m_dwConnectingStatus = STATUS_CONNECTING;
		m_bConnected = true;

		StartRoutine();
		SendMessage(MSG_PROXY_CONNECTED, 0, 0);
		if (errored)
			m_bStopConnecting = true;

		g_globals.m_logger.Write("\t\tCACConnection: connected");

		DWORD dwStartTime = GetTickCount();

		while (m_dwConnectingStatus != STATUS_CONNECTED && m_dwConnectingStatus != STATUS_AUTHENTICATION_FAILED)
		{
			if (m_bStopConnecting)
				break;

			Sleep(5000);

			if (GetTickCount() - dwStartTime > conn_timeout)
				break;
		}
		g_globals.m_logger.Write("\t\tCACConnection: connected2");

		switch(m_dwConnectingStatus)
		{
		case STATUS_CONNECTED:
			m_bConnected = true;
			nRet=CONNECTION_SUCCESSFUL;
			pProxyInfo->SetStatus(STATUS_CONNECTING, false);
			pProxyInfo->SetStatus(STATUS_AUTHENTICATING, false);
			pProxyInfo->SetStatus(STATUS_AUTHENTICATION_FAILED, false);
			pProxyInfo->SetStatus(STATUS_ESTABLISHING_DATA_CHANNEL, false);
			pProxyInfo->SetStatus(STATUS_SEARCHING_FOR_PARTNER, false);
			pProxyInfo->SetStatus(STATUS_DISCONNECTED_FROM_PROXY, false);
			pProxyInfo->SetStatus(STATUS_CONNECTED, true);
			pProxyInfo->SetReconnectProxy(true);
			break;
		case STATUS_AUTHENTICATION_FAILED:
			m_bConnected = true;
			nRet=AUTHENTICATION_FAILED;
			pProxyInfo->SetStatus(STATUS_CONNECTING, false);
			pProxyInfo->SetStatus(STATUS_AUTHENTICATING, false);
			pProxyInfo->SetStatus(STATUS_AUTHENTICATION_FAILED, true);
			pProxyInfo->SetStatus(STATUS_ESTABLISHING_DATA_CHANNEL, false);
			pProxyInfo->SetStatus(STATUS_SEARCHING_FOR_PARTNER, false);
			pProxyInfo->SetStatus(STATUS_DISCONNECTED_FROM_PROXY, false);
			pProxyInfo->SetStatus(STATUS_CONNECTED, false);
			pProxyInfo->SetReconnectProxy(false);
			break;
		case STATUS_AUTHENTICATING:
			m_bConnected = false;
			nRet=CONNECTION_TIMED_OUT;
			pProxyInfo->SetStatus(STATUS_CONNECTING, false);
			pProxyInfo->SetStatus(STATUS_AUTHENTICATING, false);
			pProxyInfo->SetStatus(STATUS_AUTHENTICATION_FAILED, false);
			pProxyInfo->SetStatus(STATUS_ESTABLISHING_DATA_CHANNEL, false);
			pProxyInfo->SetStatus(STATUS_SEARCHING_FOR_PARTNER, false);
			pProxyInfo->SetStatus(STATUS_DISCONNECTED_FROM_PROXY, true);
			pProxyInfo->SetStatus(STATUS_CONNECTED, false);
			break;
		case STATUS_CONNECTING:
			m_bConnected = false;
			nRet=CONNECTION_TIMED_OUT;
			pProxyInfo->SetStatus(STATUS_CONNECTING, false);
			pProxyInfo->SetStatus(STATUS_AUTHENTICATING, false);
			pProxyInfo->SetStatus(STATUS_AUTHENTICATION_FAILED, false);
			pProxyInfo->SetStatus(STATUS_ESTABLISHING_DATA_CHANNEL, false);
			pProxyInfo->SetStatus(STATUS_SEARCHING_FOR_PARTNER, false);
			pProxyInfo->SetStatus(STATUS_DISCONNECTED_FROM_PROXY, true);
			pProxyInfo->SetStatus(STATUS_CONNECTED, false);
			break;
		case STATUS_DISCONNECTED_FROM_PROXY:
			m_bConnected = false;
			nRet=ERROR_CONNECTING_TO_PROXY;
			pProxyInfo->SetStatus(STATUS_CONNECTING, false);
			pProxyInfo->SetStatus(STATUS_AUTHENTICATING, false);
			pProxyInfo->SetStatus(STATUS_AUTHENTICATION_FAILED, false);
			pProxyInfo->SetStatus(STATUS_ESTABLISHING_DATA_CHANNEL, false);
			pProxyInfo->SetStatus(STATUS_SEARCHING_FOR_PARTNER, false);
			pProxyInfo->SetStatus(STATUS_DISCONNECTED_FROM_PROXY, true);
			pProxyInfo->SetStatus(STATUS_CONNECTED, false);
			break;
		}
	}
	else
	{
		m_bConnected = false;
		pProxyInfo->SetStatus(STATUS_CONNECTED, false);
		nRet = NO_PROXY_SERVER_FOUND_TO_CONNECT;
		g_globals.m_logger.WriteFormated("\t\tCACConnection: not connected error: %d", CClientSocket::GetLastError());
	}
	
	g_globals.m_logger.Write("\tCACConnection: End Try connect");

	m_critSection.Unlock();

	g_globals.m_logger.Write("CACConnection: Exit connect");
	m_bStopConnecting = false;

	return nRet;
}

//disconnect from echoServer
bool CACConnection::Disconnect()
{
	bool bRet = true;

	m_bStopConnecting = true;
	m_critSection.Lock();

	StopSend(THREAD_STOP_TIMEOUT);
	if (m_bConnected)
	{
		bRet = CClientSocket::Close();
		m_bConnected = !m_bConnected;
	}

	m_bStopConnecting = false;
	m_critSection.Unlock();

	return bRet;
}

//sets the encryption level
void CACConnection::SetEncryptionLevel(int level)
{
	m_critSection.Lock();

	m_nEncryptionLevel = level;

	m_critSection.Unlock();
}

//process messages received from echoserver
//there are received some data and try to process it
//it is a notify message from CClientSocket
void CACConnection::OnReceive(char* buff, int len)
{
	m_pReadBuffer->Write(buff, len);

	char szProxyEcho[sizeof(PROXY_ECHO)];
	if (m_pReadBuffer->Peak(szProxyEcho, sizeof(PROXY_ECHO))!=sizeof(PROXY_ECHO))		
	{
		if (!strcmp(szProxyEcho, PROXY_ECHO))
		{
			m_pReadBuffer->Read(szProxyEcho, sizeof(PROXY_ECHO));
			return;
		}
	}

	DWORD lenMsg=0;

	if (m_pReadBuffer->Peak(&lenMsg, sizeof(DWORD))!=sizeof(DWORD))
		return;
	lenMsg=OSSwapLittleToHostInt32(lenMsg);
	
	char* msg=new char[lenMsg];
	if (m_pReadBuffer->Peak(msg, lenMsg)!=lenMsg)
	{
		delete []msg;
		return;
	}

	m_pReadBuffer->Drop(lenMsg);

	NetPacketHeader netPacketHeader;	
	memcpy(&netPacketHeader, msg+sizeof(DWORD), sizeof(NetPacketHeader));
	netPacketHeader.len = OSSwapLittleToHostInt32(netPacketHeader.len);
	
	CProxyMsg* pProxyMsg=(CProxyMsg*)(msg+sizeof(DWORD)+sizeof(NetPacketHeader));
	pProxyMsg->messageid = OSSwapLittleToHostInt32(pProxyMsg->messageid);
	pProxyMsg->datalength = OSSwapLittleToHostInt32(pProxyMsg->datalength);	
	
	char* pData=0;

	if (pProxyMsg->datalength)
		pData=((char*)pProxyMsg)+sizeof(CProxyMsg);		

	if (pProxyMsg->messageid==MSG_PROXY_CONNECTED)
	{
		g_globals.m_logger.Write("CACConnection: Receive MSG_PROXY_CONNECTED");

		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_DISCONNECTED_FROM_PROXY, false);		

		m_rsaKeys.Generate();
		SendMessage(MSG_PROXY_PUBLICKEY, (char*)m_rsaKeys.m_pPublicKeyLE, m_rsaKeys.GetPublicKeyLength());
	}
	else if (pProxyMsg->messageid==MSG_PROXY_PUBLICKEY)
	{
		g_globals.m_logger.Write("CACConnection: Receive MSG_PROXY_PUBLICKEY");

		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_AUTHENTICATING, true);	

		m_dwConnectingStatus=STATUS_AUTHENTICATING;

		SendEncryptedPass(pData, pProxyMsg->datalength);
	}
	else if (pProxyMsg->messageid==MSG_PROXY_HANDSHAKEFAILED)
	{
		CClientSocket::Close();
		m_bConnected=false;

		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_AUTHENTICATION_FAILED, true);
		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_AUTHENTICATING, false);		

		g_globals.m_logger.Write("CACConnection: Receive MSG_PROXY_HANDSHAKEFAILED");

		m_dwConnectingStatus=STATUS_AUTHENTICATION_FAILED;
	}
	else if (pProxyMsg->messageid==MSG_PROXY_HANDSHAKECONFIRM)
	{
		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_DISCONNECTED_FROM_PROXY, false);		

		char szServerName[ID_STRING_SIZE];
		strcpy(szServerName, pData);

		if (strlen(szServerName))
			m_pProxyConnection->GetProxyInfo()->SetName(szServerName);
		else
			m_pProxyConnection->GetProxyInfo()->SetName((char*)m_pProxyConnection->GetProxyInfo()->GetIP());
		//1.926
		char szPublicIP[ID_STRING_SIZE];
		char szUserID[ID_STRING_SIZE];
		memset(szPublicIP, 0, ID_STRING_SIZE);
		memset(szUserID, 0, ID_STRING_SIZE);
		int nReceivedData = strlen(szServerName) + 1;
		if (nReceivedData < pProxyMsg->datalength) 
		{
			strcpy(szPublicIP, pData + nReceivedData);
			nReceivedData += strlen(szPublicIP) + 1;
		}
		if (nReceivedData < pProxyMsg->datalength) 
		{
			memcpy(szUserID, pData + nReceivedData, 255);
		}
		if (strlen(szUserID))
		{
//2007.08.09
			char szEmail[ID_STRING_SIZE];
			g_globals.ParseFormattedID(szUserID, ID_STRING_SIZE, szEmail);
			g_globals.m_logger.WriteFormated("CACConnection: Receive Client %s, Form %s", szUserID, szEmail);
			m_pProxyConnection->GetProxyInfo()->SetMyID(szEmail);
		}

		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_CONNECTED, true);
		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_AUTHENTICATING, false);
		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_CONNECTING, false);

		g_globals.m_logger.Write("CACConnection: Receive MSG_PROXY_HANDSHAKECONFIRM");

		m_dwConnectingStatus=STATUS_CONNECTED;
	}
	else if (pProxyMsg->messageid==MSG_PROXY_DISCONNECTED)
	{
		CClientSocket::Close();
		m_bConnected=false;

		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_DISCONNECTED_FROM_PROXY, true);
		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_CONNECTED, false);

		g_globals.m_logger.Write("CACConnection: Receive MSG_PROXY_DISCONNECTED");
	}
	else if (pProxyMsg->messageid==MSG_PROXY_DUPLICATE_LOGIN)
	{
		CClientSocket::Close();
		m_bConnected=false;

		m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_CONNECTED, false);

		m_dwConnectingStatus=STATUS_DISCONNECTED_FROM_PROXY;

		g_globals.m_logger.Write("CACConnection: Receive MSG_PROXY_DUPLICATE_LOGIN");
	}
	else if (pProxyMsg->messageid==MSG_PROXY_PARTNERFOUND)
	{	
		m_dwConnectingStatus=MSG_PROXY_PARTNERFOUND;
		g_globals.m_logger.Write("CACConnection: Receive MSG_PROXY_PARTNERFOUND");
	}
	else if (pProxyMsg->messageid==MSG_PROXY_PARTNERNOTFOUND)
	{
		m_dwConnectingStatus=MSG_PROXY_PARTNERNOTFOUND;
		g_globals.m_logger.Write("CACConnection: Receive MSG_PROXY_PARTNERNOTFOUND");
	}
	else if (pProxyMsg->messageid==MSG_CHANNEL_CODE)
	{
		/* initiator receives from echoserver */
		
		m_dwConnectingStatus=MSG_CHANNEL_CODE;
		g_globals.m_logger.WriteFormated("CACConnection: Receive MSG_CHANNEL_CODE %s", pData);
	}
	else if (pProxyMsg->messageid==MSG_PEER_KEY)
	{
		/* initiator receives from echoserver (initiatorchannelcode/dhpart2) */

		m_dwConnectingStatus=MSG_PEER_KEY;
		
		memcpy(m_szChannelCode, pData, CHANNEL_CODE_SIZE);
		m_szChannelCode[CHANNEL_CODE_SIZE]=0;
		
#warning the RSA key? is that char or int data?
		memcpy(m_szPeerPublicKey, pData+CHANNEL_CODE_SIZE, RSA_PUBLIC_KEY*sizeof(unsigned int));
		m_szPeerPublicKey[RSA_PUBLIC_KEY*sizeof(unsigned int)]=0;

		g_globals.m_logger.WriteFormated("CACConnection: Receive MSG_PEER_KEY, chanel code=%s", m_szChannelCode);
	}
	else if (pProxyMsg->messageid==MSG_CONNECT_TO_PEER)
	{
		/* responder receives from echoserver (respchannelcode/respID/dhPart1) */

		g_globals.m_logger.Write("CACConnection: Receive MSG_CONNECT_TO_PEER 1");

		m_dwConnectingStatus=MSG_CONNECT_TO_PEER;

		memcpy(m_szChannelCode, pData, CHANNEL_CODE_SIZE);
		m_szChannelCode[CHANNEL_CODE_SIZE]=0;

		memcpy(m_szPeerID, pData+CHANNEL_CODE_SIZE, ID_STRING_SIZE);
		m_szPeerID[ID_STRING_SIZE]=0;
#warning the RSA key? is that char or int data?
		memcpy(m_szPeerPublicKey, pData+ID_STRING_SIZE+CHANNEL_CODE_SIZE, RSA_PUBLIC_KEY*sizeof(unsigned int));
		m_szPeerPublicKey[RSA_PUBLIC_KEY*sizeof(unsigned int)]=0;		

		char *pBuff = NULL;
		DWORD dwTotalLength = 0;
		DWORD dwTemp = 0;

		/* responder sends MSG_PEER_KEY to echoserver 
			(initiatorId,dhpart2,respchannelcode) */
				
		dwTotalLength += CHANNEL_CODE_SIZE;//sizeof(m_szChannelCode);
		dwTotalLength += ID_STRING_SIZE;//sizeof(m_szPeerID);
		dwTotalLength += m_rsaKeys.GetPublicKeyLength();
		pBuff = new char[dwTotalLength];

		memset(pBuff,0,dwTotalLength);// Total Buffer size
		memcpy(pBuff,m_szPeerID,sizeof(m_szPeerID));
		dwTemp = ID_STRING_SIZE;//sizeof(m_szPeerID);
		memcpy(pBuff + dwTemp,m_szChannelCode, sizeof(m_szChannelCode));
		dwTemp += CHANNEL_CODE_SIZE;//sizeof(m_szChannelCode);
			
		/* set DHpart2 to all zero if no encryption */
		if (m_nEncryptionLevel==1)
			memcpy(pBuff+dwTemp, (char*)m_rsaKeys.m_pPublicKeyLE, m_rsaKeys.GetPublicKeyLength());			
		else
			memset(pBuff+dwTemp, 0, m_rsaKeys.GetPublicKeyLength());	
		
		
		g_globals.m_logger.WriteFormated("CACConnection: Receive MSG_CONNECT_TO_PEER 2, channel code=%s", m_szChannelCode);
		
		SendMessage(MSG_PEER_KEY,pBuff,dwTotalLength);	

		g_globals.m_logger.WriteFormated("CACConnection: Receive MSG_CONNECT_TO_PEER 3, channel code=%s", m_szChannelCode);

		delete []pBuff;

		m_pProxyConnection->OnRemotePartnerConnect(m_szChannelCode, m_szPeerID, m_szPeerPublicKey);

		g_globals.m_logger.WriteFormated("CACConnection: Receive MSG_CONNECT_TO_PEER end, channel code=%s", m_szChannelCode);
	}	
	else if (pProxyMsg->messageid==MSG_ISALIVE)
	{
		g_globals.m_logger.Write("CACConnection: Receive MSG_ISALIVE");
	}

	delete []msg;
}

//send data to echoServer
//if the send buffer contain data, then this data is read it for send to echoServer
//if there are no data to send, then send IsAliveMessage if is time to do it
//it is a notify message from CClientSocket
void CACConnection::OnSend(char* buff, int& len)
{
	if (!m_bConnected)
	{
		len=0;
		return;
	}

	len=m_pSendBuffer->Read(buff, len);
}

void CACConnection::OnTimer()
{
	APISocket::CClientSocket::OnTimer();

	if (!m_pSendBuffer->Size() && m_bConnected &&
			GetTickCount() - m_dwLastIsAliveMsg >= SEND_LIVE_MESSAGE)
	{
		m_dwLastIsAliveMsg = GetTickCount();
		SendMessage(MSG_ISALIVE, 0, 0);
	}
}

//compose message packet to send to echoServer
//message : the message type
//data : the message data
//datalen : the message data length
void CACConnection::SendMessage(DWORD message, char *data, unsigned int datalen)
{
	char*	lpBuf = NULL;
	DWORD	nBytesSent = 0;

	CProxyMsg msg(message);

	NetPacketHeader header;
	
	header.len = msg.MakeMessage(&header, lpBuf, data, datalen);//create the message	

	DWORD dwToSend=OSSwapHostToLittleInt32(header.len+sizeof(DWORD));
	m_pSendBuffer->Write(&dwToSend, sizeof(DWORD));
	m_pSendBuffer->Write(lpBuf, header.len);
	Send();

	if (lpBuf)
		delete lpBuf;
}

//compose MSG_PROXY_PASSWORD message packet
void CACConnection::SendEncryptedPass(char * pData,DWORD dwDataLength)
{
	char pEncPass[1024];
	
	m_rsaKeys.EncryptPassword(pData, dwDataLength, m_pProxyConnection->GetProxyInfo()->GetPassword(), pEncPass);

	char szEmail[ID_STRING_SIZE];
	if (g_globals.GetFormattedID(szEmail, ID_STRING_SIZE, m_pProxyConnection->GetProxyInfo()->GetMyID()) == -1)
		return;
	const int nTotalPkgSize = 1024 + ID_STRING_SIZE+ sizeof(DWORD);
	char szTotalData[nTotalPkgSize];
	memset(szTotalData, 0, nTotalPkgSize);
	memcpy(szTotalData, pEncPass,1024);
	memcpy(szTotalData + 1024, szEmail, ID_STRING_SIZE);

	SendMessage(MSG_PROXY_PASSWORD, szTotalData, nTotalPkgSize);	
}

//calback function, notify that there was an error on read or send data to echoserver on this channel
//it is a notify message from CClientSocket
void CACConnection::OnError(int error)
{
	CClientSocket::OnError(error);
	m_bStopConnecting = true;
	m_pProxyConnection->OnError(error);

	CDllProxyInfo* pProxyInfo=m_pProxyConnection->GetProxyInfo();
	pProxyInfo->SetStatus(STATUS_CONNECTED, false);
	pProxyInfo->SetStatus(STATUS_DISCONNECTED_FROM_PROXY, true);

	m_bConnected = false;
}

//find partner on echoServer
//sends MSG_PROXY_FIND_PARTNER message and wait for echoServer answer
bool CACConnection::FindPartner(const char* szPartener)
{
	g_globals.m_logger.WriteFormated("CACConnection: FindPartner %s", szPartener);

	if (!m_bConnected)
	{
		g_globals.m_logger.WriteFormated("CACConnection: FindPartner %s end, not connected", szPartener);
		return false;
	}

	m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_SEARCHING_FOR_PARTNER, true);

	char sz[ID_STRING_SIZE];
	if (g_globals.GetFormattedID(sz, ID_STRING_SIZE, szPartener) == -1)
		return false;

	m_dwConnectingStatus=0;
	SendMessage(MSG_PROXY_FIND_PARTNER, (char*)sz, (unsigned int)ID_STRING_SIZE);	

	g_globals.m_logger.WriteFormated("CACConnection: FindPartner wait %d seconds", m_pProxyConnection->GetProxyInfo()->GetConnectTimeout());

	DWORD dwStartTime=GetTickCount();
	while (!m_dwConnectingStatus)
	{
		Sleep(10000);

		if (GetTickCount()-dwStartTime>(unsigned int)m_pProxyConnection->GetProxyInfo()->GetConnectTimeout()*1000)
			break;
	}	

	g_globals.m_logger.Write("CACConnection: FindPartner wait end");

	m_pProxyConnection->GetProxyInfo()->SetStatus(STATUS_SEARCHING_FOR_PARTNER, false);

	g_globals.m_logger.WriteFormated("CACConnection: FindPartner status=%d", m_dwConnectingStatus);

	return (bool)(m_dwConnectingStatus==MSG_PROXY_PARTNERFOUND);
}

//try to connect to szPartner on echoServer
//sends MSG_CONNECT_TO_PEER message and wait for echoServer answer
bool CACConnection::ConnectToPeer(const char* szPartener, char* szChannelCode, char* szPeerPublicKey)
{
	g_globals.m_logger.WriteFormated("CACConnection: Try connect to peer %s, channel code ", szPartener, szChannelCode);

	char *pBuff = NULL;
	DWORD dwTotalLength = 0;
	DWORD dwTemp = 0;

	dwTotalLength += ID_STRING_SIZE;//(DWORD)strlen(szPartener)+1;
	dwTotalLength += m_rsaKeys.GetPublicKeyLength();

	pBuff = new char[dwTotalLength];
	memset(pBuff, 0, dwTotalLength);
	g_globals.GetFormattedID(pBuff, ID_STRING_SIZE, szPartener);
	dwTemp = ID_STRING_SIZE;//(DWORD)strlen(szPartener)+1;
		
	/* set DHpart1 to all zero if no encryption */
	if (m_nEncryptionLevel==1)
		memcpy(pBuff+dwTemp, m_rsaKeys.GetPublicKey(), m_rsaKeys.GetPublicKeyLength());
	else
		memset(pBuff+dwTemp, 0, m_rsaKeys.GetPublicKeyLength());	

	m_dwConnectingStatus=0;

	m_szPeerPublicKey[0]=m_szChannelCode[0]=0;

	SendMessage(MSG_CONNECT_TO_PEER, pBuff, dwTotalLength);														

	delete [] pBuff;

	DWORD dwStartTime=GetTickCount();
	while (m_dwConnectingStatus!=MSG_PEER_KEY && m_dwConnectingStatus!=MSG_PROXY_PARTNERNOTFOUND)
	{
		Sleep(10000);

		if (GetTickCount()-dwStartTime>(unsigned int)m_pProxyConnection->GetProxyInfo()->GetConnectTimeout()*1000)
			break;
	}

	if (m_dwConnectingStatus==MSG_PEER_KEY)
	{
		strcpy(szChannelCode, m_szChannelCode);
		memcpy(szPeerPublicKey, m_szPeerPublicKey, RSA_PUBLIC_KEY*sizeof(unsigned int));
	}
	
	if (m_dwConnectingStatus==MSG_PEER_KEY)
		g_globals.m_logger.WriteFormated("CACConnection: Success connect to peer %s, channel code ", szPartener, szChannelCode);
	else
		g_globals.m_logger.WriteFormated("CACConnection: Fail connect to peer %s, channel code ", szPartener, szChannelCode);

	return m_dwConnectingStatus==MSG_PEER_KEY;
}