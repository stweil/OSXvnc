#include "stdafx.h"
#include "RSAKeys.h"
#include <time.h>
#include "RSA/HNumber.h"
#include "RSA/Hnfunct.h"
#include "blowfish.h"
#include "rsa/operator.h"

#define DFLT_GENERATOR	    "2"
#define DFLT_MODULUS "7212610147295474909544523785043492409969382148186765460082500085393519556525921455588705423020751421"  

CRSAKeys::CRSAKeys()
{	
}

CRSAKeys::~CRSAKeys()
{
}

const char* CRSAKeys::GetPublicKey() const
{
	return m_pPublicKey;
}

const char* CRSAKeys::GetPrivateKey() const
{
	return m_pPrivateKey;
}

int CRSAKeys::GetPublicKeyLength() const
{
	return RSA_PUBLIC_KEY * size_of;
}

int CRSAKeys::GetPrivateKeyLength() const
{
	return RSA_PRIVATE_KEY;
}

void CRSAKeys::GenerateRandomPrivateKey(char* pKey)
{
	unsigned long temp[5] ;
	memset(temp, 0x00, 16);
	unsigned long randomNo;
	
	memset(pKey, 0x00, RSA_PRIVATE_KEY + 1);
	for(int index = 0; index < 5; index++) 
	{
		srand( (unsigned)time( NULL ) + index);
		randomNo = rand() * rand();
		temp[index] = randomNo;
	}
	sprintf(pKey, "%08lx%08lx%08lx%08lx%08lx", temp[0], temp[1], temp[2], temp[3], temp[4]);
	pKey[RSA_PRIVATE_KEY] ='\0';
}

void CRSAKeys::GeneratePublicKey()
{	
	//code for Public key generation
	HugeNumber Y,x,n,f;
	x=HugeNumber(DFLT_GENERATOR);
	f=HugeNumber(DFLT_MODULUS);
	n=HugeNumber(m_pPrivateKey);
	//::ModPower(Y,x,n,f);//execute the modpower function Get the public key as HugeNumber
	ModPower(x,n,f,Y);

	memset(m_pPublicKey, 0, RSA_PUBLIC_KEY * size_of);
	memcpy(m_pPublicKey, Y.digit, RSA_PUBLIC_KEY * size_of); // copy the huge number digit buffer to char *	
	
	memset(m_pPublicKeyLE, 0, RSA_PUBLIC_KEY * size_of);
	// Mac OS X must swap these for a big endian machine
	if (1)
	{
		for (int i = 0; i < RSA_PUBLIC_KEY; i++)
		{
			word value = OSSwapHostToLittleInt32(Y.digit[i]);
			memcpy(m_pPublicKeyLE + i * size_of, &value, size_of);
		}
	}
}

void CRSAKeys::GenerateSessionKey(char* szPeerPublicKey, char* szSessionKey)
{
	HugeNumber Y,n,f,h1;
	word *tmp = (word*)szPeerPublicKey;//convert form liitle to host
	for (int i = 0; i < RSA_PUBLIC_KEY; i++)
	{
		word value = OSSwapLittleToHostInt32(tmp[i]);
		h1.digit[i] = value;
	}
//	memcpy(h1.digit, szPeerPublicKey, RSA_PUBLIC_KEY * size_of);
	memset(szSessionKey, 0, 1024);
	
	f=HugeNumber(DFLT_MODULUS);
	n=HugeNumber(m_pPrivateKey);
	//::ModPower(Y,h1,n,f);
	ModPower(h1,n,f,Y);
	szSessionKey=Y.ToHexStr(szSessionKey,1024 );	
}

void CRSAKeys::Generate()
{
	GenerateRandomPrivateKey(m_pPrivateKey);
	// Generate public key (deffie hellman)
	GeneratePublicKey();
}

void CRSAKeys::EncryptPassword(char* pData, DWORD dwDataLength, const char* strPass, char* output)
{
	HugeNumber Y,n,f,h1;
	if (1)
	{
		word *publicKeyPointer = (word *)pData;
		for (int i = 0; i < dwDataLength / size_of; i++)
		{
			h1.digit[i] = OSSwapLittleToHostInt32(publicKeyPointer[i]);
		}
	}
	else
		memcpy(h1.digit, pData, dwDataLength);	

	char* pSessionKey = new char[1024];
	memset(pSessionKey, 0, 1024);
	
	f=HugeNumber(DFLT_MODULUS);
	//::ModPower(Y,h1,n,f);
	
	n=HugeNumber(m_pPrivateKey);
	ModPower(h1,n,f,Y);
	pSessionKey = Y.ToHexStr(pSessionKey, 1024);
		
	//const char* data = strPass;
	char* data = new char[dwDataLength];
	memset(data, 0, dwDataLength);
	memcpy(data, strPass, min(strlen(strPass), dwDataLength));

	//translate data to HOST endian
	word *dwData = (word*)data;
	for (int i = 0; i < dwDataLength / size_of; i++)
	{
		dwData[i] = OSSwapLittleToHostInt32(dwData[i]);
	}
		
	CBlowFish blowfish;
	blowfish.Initialize((BYTE*)pSessionKey, strlen(pSessionKey));
	blowfish.Encode((BYTE*)data, (BYTE*)output, dwDataLength);
	
	//SS: added change to big endian
	word *publicKeyPointer = (word*)output;
	for (int i = 0; i < dwDataLength / size_of; i++)
	{
		publicKeyPointer[i] = OSSwapHostToLittleInt32(publicKeyPointer[i]);
	}
	delete []pSessionKey;
	delete []data;
}

//"Right to Left Binary Algorithm" from http://en.wikipedia.org/wiki/Modular_exponentiation
//r=(b^e)(mod m)
void CRSAKeys::ModPower(HugeNumber& b, HugeNumber& e, HugeNumber& m, HugeNumber& r)
{
	r="1";

	HugeNumber zero("0");
	HugeNumber two("2");

	while (e > zero)
	{
		//if (e & 1 > 0)
		//if ((e % two) > zero)
		if (e.digit[0] % 2 != 0)
		{
			r=r * b;
			r=r % m;
		}

		e = e / two;
		b = (b * b);
		b = (b % m);
	}
}