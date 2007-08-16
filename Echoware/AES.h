#pragma once

#include "OpenSSL/aes.h"

//wrapper class for AES encryption

class CAES
{
public:
	CAES(void);
	~CAES(void);
	
	void Decrypt(const unsigned char *in, int length, unsigned char *out);
	void Encrypt(const unsigned char *in, int length, unsigned char *out);

	int SetDecryptKey(const unsigned char *userKey, const int bits);
	int SetEncryptKey(const unsigned char *userKey, const int bits);

protected:
	void CryptData(const unsigned char *in, unsigned char *out, int length, const int enc); 

protected:	
	AES_KEY m_aesEncKey;
	AES_KEY m_aesDecKey;

public:
	unsigned char m_userDecKey[128];
	unsigned char m_userEncKey[128];
};
