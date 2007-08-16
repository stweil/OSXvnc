#include "StdAfx.h"
#include "datachannelsocket.h"
#include "DataChannel.h"

CDataChannelSocket::CDataChannelSocket(CDataChannel* pDataChannel)
{
	m_pDataChannel=pDataChannel;

	m_pRecvBuffer=new CBuffer();
	m_pSendBuffer=new CBuffer();

	m_nEncriptionLevel=1;

	m_pPairChannel=0;
}

CDataChannelSocket::~CDataChannelSocket(void)
{
	StopSend(THREAD_STOP_TIMEOUT);

	Close();	

	delete m_pRecvBuffer;
	m_pRecvBuffer=0;

	delete m_pSendBuffer;
	m_pSendBuffer=0;
}

void CDataChannelSocket::OnReceive(char* buff, int len)
{
	m_pPairChannel->WriteData(buff, len);
}

void CDataChannelSocket::OnSend(char* buff, int& len)
{
	len = m_pSendBuffer->Read(buff, len);
}

void CDataChannelSocket::OnError(int error)
{
	CClientSocket::OnError(error);
	m_pDataChannel->OnError(this, error);
}

//sets the pair data channel for this data channel
void CDataChannelSocket::SetPairChannel(CDataChannelSocket* pPairChannel)
{
	m_pPairChannel=pPairChannel;
}

//gets the pair data channel for this data channel
CDataChannelSocket* CDataChannelSocket::GetPairChannel()
{
	return m_pPairChannel;
}

void CDataChannelSocket::ReadData(char* buff, unsigned int& read_size)
{
	read_size = m_pRecvBuffer->Read(buff, read_size);
}

void CDataChannelSocket::WriteData(char* buff, unsigned int write_size)
{
	m_pSendBuffer->Write(buff, write_size);
}

//sets the encryption level for this data channel
void CDataChannelSocket::SetEncriptionLevel(int nLevel)
{
	m_nEncriptionLevel=nLevel;
}

//gets the encryption level for this data channel
int CDataChannelSocket::GetEncriptionLevel()
{
	return m_nEncriptionLevel;
}