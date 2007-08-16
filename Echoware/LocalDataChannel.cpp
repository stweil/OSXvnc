
#include "StdAfx.h"
#include "LocalDataChannel.h"
#include "DataChannel.h"
#include "globals.h"

CLocalDataChannel::CLocalDataChannel(CDataChannel* pDataChannel)
:CDataChannelSocket(pDataChannel)
{
	m_bRFB=false;
}

CLocalDataChannel::~CLocalDataChannel(void)
{
	g_globals.m_logger.WriteFormated("~CLocalDataChannel: send buff=%d, rec buff=%d", m_pSendBuffer->Size(), m_pRecvBuffer->Size());
}

void crypt_data(const unsigned char *in, unsigned char *out, int length, const AES_KEY *key, const int enc) 
{
	unsigned long len = length;
	unsigned char tmp[AES_BLOCK_SIZE];

	while (len >= AES_BLOCK_SIZE) 
	{
		memset(tmp,0,AES_BLOCK_SIZE);
		memcpy(tmp,in,AES_BLOCK_SIZE);
		
		if (AES_ENCRYPT == enc)
		{
			AES_encrypt(tmp, out, key);
		}
		else
		{
			AES_decrypt(tmp, out, key);
		}

		len -= AES_BLOCK_SIZE;
		in += AES_BLOCK_SIZE;
		out += AES_BLOCK_SIZE;
	}

	if (len) 
	{
		memset(tmp,0,AES_BLOCK_SIZE);
		memcpy(tmp,in,AES_BLOCK_SIZE);
		if (AES_ENCRYPT == enc)
		{
			AES_encrypt(tmp, tmp, key);
		}
		else
		{
			AES_decrypt(tmp, tmp, key);
		}
		memcpy(out, tmp, AES_BLOCK_SIZE);
	}			
}

//send data to local server
//it is a notify message from CClientSocket
void CLocalDataChannel::OnSend(char* buff, int& len)
{		
	if (!m_pDataChannel->m_bEncryptDecrypt)
	{
		CDataChannelSocket::OnSend(buff, len);		
	}
	else
	{
		char temp[17];

		if (m_pSendBuffer->Peak(temp, 17)<17)
		{
			len=0;
			return;
		}

		int nLength, nMsgLength;

		sscanf(temp, "%d:%d", &nMsgLength, &nLength);

		if (nLength>len+17)
		{
			len=0;
			return;
		}

		unsigned int read=0;

		if ((read=m_pSendBuffer->Peak(buff, nLength+17))<(unsigned int)nLength+17)
		{
			len=0;
			return;
		}

		//nLength=read;

		//g_globals.m_logger.WriteFormated("CLocalDataChannel: Send on data channel %p, sock=%d ip=%s: len=%d", m_pDataChannel, m_sock, "127.0.0.1", nMsgLength);

		m_pSendBuffer->Drop(nLength+17);
			
		char* p=new char[nLength];
		memcpy(p, buff+17, nLength);

		//m_pDataChannel->m_aes.Decrypt((unsigned char*)p, nLength, (unsigned char*)buff);	
		/*CAES aes;
		aes.SetEncryptKey(m_pDataChannel->m_aes.m_userEncKey, 128);
		aes.SetDecryptKey(m_pDataChannel->m_aes.m_userDecKey, 128);
		
		aes.Decrypt((unsigned char*)p, nLength, (unsigned char*)buff);	*/

		AES_KEY aesKey;
		AES_set_decrypt_key((unsigned char*)m_pDataChannel->GetSessionKey(), 128, &aesKey);
		crypt_data((unsigned char*)p,
					   (unsigned char*)buff,
					   nLength,
					   &aesKey,
					   AES_DECRYPT );

		delete []p;				

		len=nMsgLength;			
	}
}

//there are received some data from local server and try to process it
//it is a notify message from CClientSocket
void CLocalDataChannel::OnReceive(char* buff, int len)
{
	if (len > 0)
		g_globals.m_logger.WriteFormated("CLocalDataChannel::OnReceive sock = %d len = %d", getSocket(), len);

	if (!m_pDataChannel->m_bEncryptDecrypt)
		CDataChannelSocket::OnReceive(buff, len);
	else
	{
		int nLength=len+16-len%16;
		char temp[18];
		memset(temp, 0, 18);
		sprintf(temp, "%d:%d", len, nLength);
		char *p=new char[nLength + 17];
		memcpy(p, temp, 17);
		AES_KEY aesKey;
		AES_set_encrypt_key((unsigned char*)m_pDataChannel->GetSessionKey(), 128, &aesKey);
		crypt_data((unsigned char*)buff, (unsigned char*)(p + 17), nLength, &aesKey, AES_ENCRYPT);
		m_pPairChannel->WriteData(p, nLength + 17);
		delete []p;
	}
	m_pPairChannel->Send();
}

bool CLocalDataChannel::StartSend()
{	
	int ret = Send();
	return (ret == 0);
}

