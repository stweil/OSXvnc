#ifndef _RSAKEYS_H
#define _RSAKEYS_H

#if _MSC_VER > 1000
#pragma once
#endif 

#include "rsa/HNumber.h"

#define RSA_PUBLIC_KEY	80
#define RSA_PRIVATE_KEY	40
#define size_of sizeof(word)

class CRSAKeys
{
public:
	CRSAKeys();
	virtual ~CRSAKeys();

	//Generate the Deffie Hellman key pair
	void Generate();

	const char* GetPublicKey() const;
	const char* GetPrivateKey() const;	

	int GetPublicKeyLength() const;
	int GetPrivateKeyLength() const;	

	void EncryptPassword(char* pData, DWORD dwDataLength, const char* strPass, char* output);
	
	void GenerateSessionKey(char* szPeerPublicKey, char* szSessionKey);	

	char m_pPublicKeyLE[RSA_PUBLIC_KEY * size_of];

protected:
	//Generate the random private key
	void GenerateRandomPrivateKey(char* pKey);
	//Generate the public key for the private key
	void GeneratePublicKey();
	
	//"Right to Left Binary Algorithm" from http://en.wikipedia.org/wiki/Modular_exponentiation
	//r=(b^e)(mod m)
	void ModPower(HugeNumber& b, HugeNumber& e, HugeNumber& m, HugeNumber& r);

protected:
	char m_pPublicKey[RSA_PUBLIC_KEY * size_of];
	char m_pPrivateKey[RSA_PRIVATE_KEY + 1];
};

#endif