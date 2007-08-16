#include "StdAfx.h"
#include "proxyconnection.h"
#include "dllproxyinfo.h"
#include "ACConnection.h"
#include "DataChannels.h"
#include "DataChannel.h"

CProxyConnection::CProxyConnection(void)
{
}

CProxyConnection::CProxyConnection(CDllProxyInfo* pProxyInfo)
{
	m_pProxyInfo=pProxyInfo;
	m_pACConnection=new CACConnection(this);

	m_pDataChannels=new CDataChannels(pProxyInfo);
	m_nEncryptionLevel=1;
}

CProxyConnection::~CProxyConnection(void)
{
	if (m_pProxyInfo)
		delete m_pProxyInfo;

	delete m_pACConnection;

	delete m_pDataChannels;
}

CDllProxyInfo* CProxyConnection::GetProxyInfo()
{
	return m_pProxyInfo;
}

int CProxyConnection::Connect()
{
	int nRet;

	m_critSection.Lock();

	nRet=m_pACConnection->Connect();

	m_critSection.Unlock();

	return nRet;
}

bool CProxyConnection::Disconnect()
{
	bool bRet;

	m_critSection.Lock();

	m_pDataChannels->RemoveAllDataChannels();
	bRet=m_pACConnection->Disconnect();	

	m_critSection.Unlock();

	return bRet;
}

void CProxyConnection::StopConnecting()
{
	m_pACConnection->StopConnect();	
}

void CProxyConnection::SetEncryptionLevel(int level)
{
	m_critSection.Lock();

	m_nEncryptionLevel=level;
	m_pACConnection->SetEncryptionLevel(level);

	m_critSection.Unlock();
}

int CProxyConnection::EstablishNewDataChannel(char* IDOfPartner)
{
	g_globals.m_logger.WriteFormated("CProxyConnection: EstablishNewDataChannel to partner %s", IDOfPartner);
	g_globals.m_logger.Write("CProxyConnection: Find partner");

	if (!m_pACConnection->FindPartner(IDOfPartner))
	{
		g_globals.m_logger.Write("CProxyConnection: Failed find partner");
		return 0;
	}

	g_globals.m_logger.Write("CProxyConnection: Success find partner");

	g_globals.m_logger.WriteFormated("CProxyConnection: Connect to peer %s", IDOfPartner);
	
	char szChannelCode[12];
	char szPeerPublicKey[RSA_PUBLIC_KEY*sizeof(unsigned int)+1];

	if (!m_pACConnection->ConnectToPeer(IDOfPartner, szChannelCode, szPeerPublicKey))
	{
		g_globals.m_logger.WriteFormated("CProxyConnection: Failed connect to peer %s", IDOfPartner);
		return 0;
	}

	g_globals.m_logger.WriteFormated("CProxyConnection: Success connect to peer %s", IDOfPartner);

	bool bEncDec=false;	
	if (m_nEncryptionLevel && strcmp(szPeerPublicKey, "")!=0)
		bEncDec=true;
	//****
	//bEncDec=false;	
	//*****
	
	char szSessionKey[1024];

	CDataChannel* pDataChannel=new CDataChannel(m_pDataChannels, szChannelCode, 
								m_pACConnection->GenerateSessionKey(szSessionKey, szPeerPublicKey), bEncDec);
	m_pDataChannels->AddDataChannel(pDataChannel);

	unsigned int nPort=0;
	if (!pDataChannel->Listen(nPort))
		return 0;

	return nPort;
}

void CProxyConnection::OnError(int error)
{	
	g_globals.m_proxiesManager.ProxyError(m_pProxyInfo);
}

void CProxyConnection::OnRemotePartnerConnect(char* szDataChannelCode, char* IDOfPartner, char* szPeerPublicKey)
{
	g_globals.m_logger.Write("=>OnRemotePartnerConnect");

	bool bEncDec=false;	
	if (m_nEncryptionLevel && strcmp(szPeerPublicKey, "") != 0)
		bEncDec=true;	
	//*****
	//bEncDec=false;
	//*****

	char szSessionKey[1024];
	CDataChannel* pDataChannel=new CDataChannel(m_pDataChannels, szDataChannelCode, 
					m_pACConnection->GenerateSessionKey(szSessionKey, szPeerPublicKey), bEncDec);
	

	m_pDataChannels->AddDataChannel(pDataChannel);

	if (pDataChannel->ConnectEchoServer())
	{
		g_globals.m_logger.Write("OnRemotePartnerConnect success");				
	}
	else
	{
		g_globals.m_logger.Write("OnRemotePartnerConnect fail");
		m_pDataChannels->RemoveDataChannel(pDataChannel);
	}

	//pDataChannel->ConnectLocalServer(g_globals.GetPortForOffLoadingData());
	g_globals.m_logger.Write("<=OnRemotePartnerConnect");
}
