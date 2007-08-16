#include "StdAfx.h"
#include "aes.h"

CAES::CAES(void)
{
}

CAES::~CAES(void)
{
}

void CAES::Decrypt(const unsigned char *in, int length, unsigned char *out)
{	
	CryptData(in, out, length, AES_DECRYPT);
}

void CAES::Encrypt(const unsigned char *in, int length, unsigned char *out)
{
	CryptData(in, out, length, AES_ENCRYPT);
}

int CAES::SetDecryptKey(const unsigned char *userKey, const int bits)
{
	//return AES_set_decrypt_key(userKey, bits, &m_aesDecKey);

	memcpy(m_userDecKey, userKey, bits);
	return 0;
}

int CAES::SetEncryptKey(const unsigned char *userKey, const int bits)
{
	//return AES_set_encrypt_key(userKey, bits, &m_aesEncKey);
	memcpy(m_userEncKey, userKey, bits);
	return 0;
}

void CAES::CryptData(const unsigned char *in, unsigned char *out, int length, const int enc) 
{
	AES_KEY aesKey;
	if (AES_ENCRYPT == enc)
		AES_set_encrypt_key(m_userEncKey, 128, &aesKey);
	else 
		AES_set_decrypt_key(m_userDecKey, 128, &aesKey);

	unsigned long len = length;
	unsigned char tmp[AES_BLOCK_SIZE];

	while (len >= AES_BLOCK_SIZE) 
	{
		memset(tmp,0,AES_BLOCK_SIZE);
		memcpy(tmp,in,AES_BLOCK_SIZE);
		
		if (AES_ENCRYPT == enc)
		{
			AES_encrypt(tmp, out, &aesKey);
		}
		else
		{
			AES_decrypt(tmp, out, &aesKey);
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
			AES_encrypt(tmp, tmp, &aesKey);
		}
		else
		{
			AES_decrypt(tmp, tmp, &aesKey);
		}
		memcpy(out, tmp, AES_BLOCK_SIZE);
	}			
}