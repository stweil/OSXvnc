////////////////////////////////////////////////////////////////////////////
// EchoWare is Copyright (C) 2004,2005 Echogent Systems, Inc. All rights reserved.

// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:

//  * Redistributions of source code must retain the above copyright notice, 
//    this list of conditions and the following disclaimer.
 
//  * Redistributions in binary form must reproduce the above copyright notice, 
//    this list of conditions and the following disclaimer in the documentation 
//    and/or other materials provided with the distribution.
// 
//  * Redistributions in any form must be accompanied by information on how 
//    to obtain complete source code for the echoware software and any 
//    accompanying software that uses the echoware software. The source code 
//    must either be included in the distribution or be available for no more 
//    than the cost of distribution plus a nominal fee, and must be freely 
//    redistributable under reasonable conditions. For an executable file, 
//   complete source code means the source code for all modules it contains. 
//    It does not include source code for modules or files that typically 
//    accompany the major components of the operating system on which the 
//    executable file runs. 

// THIS SOFTWARE IS PROVIDED BY ECHOGENT SYSTEMS, INC. ``AS IS'' AND ANY EXPRESS 
//OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
//OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, 
//ARE DISCLAIMED. IN NO EVENT SHALL ECHOGENT SYSTEMS, INC. BE LIABLE FOR ANY 
//DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
//(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
//LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND 
//ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
//THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//For more information: echoware@echogent.com
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
//#include "afx.h"
#include "iostream"
//#include "windows.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"


#include "stdafx.h"
#include "proxyconnect.h"
#include "Echoware.h"
/*#include "VPNProxyDllManager.h"*/

#include "md5.h"
#include "md4.h"
#include "ntlm.h"

#include "sys/socket.h"
#include "EchoToOSX.h"

#define printf

//typedef unsigned __int8		uint8_t;
//typedef unsigned __int16	uint16_t;
//typedef unsigned __int32	uint32_t;
//typedef unsigned __int64	uint64_t;
//typedef __int8				int8_t;
//typedef __int16				int16_t;
//typedef __int32				int32_t;
//typedef __int64				int64_t;


#define bzero( x, n) memset(x, 0, n)
#define bcopy( s, d, len) memcpy(d, s, len)

/*
** Function: hmac_md5
*/

void hmac_md5(
unsigned char* text, /* pointer to data stream */
int text_len, /* length of data stream */
unsigned char* key, /* pointer to authentication key */
int key_len, /* length of authentication key */
unsigned char digest[16] /* caller digest to be filled in */
)
{
        MD5_CTX context;
        unsigned char k_ipad[65];    /* inner padding -
                                      * key XORd with ipad
                                      */
        unsigned char k_opad[65];    /* outer padding -
                                      * key XORd with opad
                                      */
        unsigned char tk[16];
        int i;

		/* if key is longer than 64 bytes reset it to key=MD5(key) */
        if (key_len > 64) {

			MD5_CTX      tctx;
			MD5Init(&tctx);
			MD5Update(&tctx, key, key_len);
			MD5Final(tk, &tctx);

			key = tk;
			key_len = 16;
        }

        /*
         * the HMAC_MD5 transform looks like:
         *
         * MD5(K XOR opad, MD5(K XOR ipad, text))
         *
         * where K is an n byte key
         * ipad is the byte 0x36 repeated 64 times
         * opad is the byte 0x5c repeated 64 times
         * and text is the data being protected
         */

        /* start out by storing key in pads */
        bzero( k_ipad, sizeof k_ipad);
        bzero( k_opad, sizeof k_opad);
        bcopy( key, k_ipad, key_len);
        bcopy( key, k_opad, key_len);

        /* XOR key with ipad and opad values */
        for (i=0; i<64; i++) {
                k_ipad[i] ^= 0x36;
                k_opad[i] ^= 0x5c;
        }
        /*
         * perform inner MD5
         */
        MD5Init(&context);                   /* init context for 1st
                                              * pass */
        MD5Update(&context, k_ipad, 64);     /* start with inner pad */
        MD5Update(&context, text, text_len); /* then text of datagram */
        MD5Final(digest, &context);          /* finish up 1st pass */
        /*
         * perform outer MD5
         */
        MD5Init(&context);                   /* init context for 2nd
                                              * pass */
        MD5Update(&context, k_opad, 64);     /* start with outer pad */
        MD5Update(&context, digest, 16);     /* then results of 1st
                                              * hash */
        MD5Final(digest, &context);          /* finish up 2nd pass */
}



static const char base64digits[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define BAD	-1
static const char base64val[] = {
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD, 62, BAD,BAD,BAD, 63,
     52, 53, 54, 55,  56, 57, 58, 59,  60, 61,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,  0,  1,  2,   3,  4,  5,  6,   7,  8,  9, 10,  11, 12, 13, 14,
     15, 16, 17, 18,  19, 20, 21, 22,  23, 24, 25,BAD, BAD,BAD,BAD,BAD,
    BAD, 26, 27, 28,  29, 30, 31, 32,  33, 34, 35, 36,  37, 38, 39, 40,
     41, 42, 43, 44,  45, 46, 47, 48,  49, 50, 51,BAD, BAD,BAD,BAD,BAD
};
#define DECODE64(c)  (isascii(c) ? base64val[c] : BAD)



#ifndef MAX
#define MAX( x, y )	( ( (x)>(y) ) ? (x) : (y) )
#endif

void base64(unsigned char *out, const unsigned char *in, int len)
{
  while (len >= 3) 
  {
    *out++ = base64digits[in[0] >> 2];
    *out++ = base64digits[((in[0] << 4) & 0x30) | (in[1] >> 4)];
    *out++ = base64digits[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
    *out++ = base64digits[in[2] & 0x3f];
    len -= 3;
    in += 3;
 }

  if (len > 0) 
  {
    unsigned char fragment;

    *out++ = base64digits[in[0] >> 2];
    fragment = (in[0] << 4) & 0x30;
    if (len > 1)
      fragment |= in[1] >> 4;
    *out++ = base64digits[fragment];
    *out++ = (len < 2) ? '=' : base64digits[(in[1] << 2) & 0x3c];
    *out++ = '=';
  }
  *out = '\0';
}

int unbase64(char *out, const char *in, int maxlen)
{
    int len = 0;
    register unsigned char digit1, digit2, digit3, digit4;
    if (in[0] == '+' && in[1] == ' ')
		in += 2;
    if (*in == '\r')
		return(0);
    do 
	{
		digit1 = in[0];
		if (DECODE64(digit1) == BAD)
			return(-1);
		digit2 = in[1];
		if (DECODE64(digit2) == BAD)
			return(-1);
		digit3 = in[2];
		if (digit3 != '=' && DECODE64(digit3) == BAD)
			return(-1);
		digit4 = in[3];
		if (digit4 != '=' && DECODE64(digit4) == BAD)
			return(-1);
		in += 4;
		++len;
		if (maxlen && len > maxlen)
			return(-1);
		*out++ = (DECODE64(digit1) << 2) | (DECODE64(digit2) >> 4);
		if (digit3 != '=')
		{
			++len;
			if (maxlen && len > maxlen)
				return(-1);
			*out++ = ((DECODE64(digit2) << 4) & 0xf0) | (DECODE64(digit3) >> 2);
			if (digit4 != '=')
			{
				++len;
			if (maxlen && len > maxlen)
				return(-1);
			*out++ = ((DECODE64(digit3) << 6) & 0xc0) | DECODE64(digit4);
			}
		}
    } 
	while (*in && *in != '\r' && digit4 != '=');
    return (len);
}



#define MSG_BUF_SIZE 4096
#define NAME_BUF_LEN 256
#define MAXSIZE 65536
#define	SIZE1	8

typedef struct 
{
	unsigned short	length;
	unsigned short	space;
	unsigned long	offset;
} securitybuffer;


typedef struct 
{
	unsigned char	digest[SIZE1];			//8 bytes 	
	unsigned char	serverchallenge[SIZE1];		//8 bytes 

	unsigned long	signature;				//4 bytes
	unsigned long	reserved;				//4 bytes
	unsigned long 	timestamp;				//4 bytes 
	unsigned char	clientchallenge[SIZE1];	//8 bytes
	unsigned long	unknown;				//4 bytes
	//unsigned long	data;					//data	
	unsigned char	data[1];				//data	
} ntlmv2_blob;



typedef struct 
{
	unsigned char	signature[SIZE1];		//8 bytes
	unsigned long	messagetype;			//4 bytes
	unsigned long	flags;					//4 bytes
	securitybuffer	domain;					//8 bytes
	securitybuffer	workstation;			//8 bytes
} ntlmv2_type1;


typedef struct 
{
	unsigned char	signature[SIZE1];		//8 bytes
	unsigned long	messagetype;			//4 bytes
	securitybuffer	targetname;				//8 bytes
	unsigned long	flags;					//4 bytes
	unsigned char	challenge[SIZE1];		//8 bytes
	unsigned long	context1;				//4 bytes
	unsigned long	context2;				//4 bytes	
	securitybuffer	targetinfo;				//8 bytes
	unsigned char	data[1];				//data
} ntlmv2_type2;


typedef struct 
{
	unsigned char	signature[SIZE1];
	unsigned long	messagetype;
	securitybuffer	LM_response;
	securitybuffer	NTLM_response;
	securitybuffer	domain;
	securitybuffer	user;
	securitybuffer	workstation;
	unsigned char	sessionkey[SIZE1];
	unsigned long	flags;
	unsigned char	data[1];
} ntlmv2_type3;





//
// Global variable for NTLMv2
//
char gpassword[NAME_BUF_LEN];
char gusername[NAME_BUF_LEN];
char gdomain[NAME_BUF_LEN];
char gworkstation[NAME_BUF_LEN];

char ntlmv2_type1_buffer[MSG_BUF_SIZE];
char ntlmv2_type3_buffer[MSG_BUF_SIZE];
char ntlmv2_type2_buffer[MSG_BUF_SIZE];


unsigned char challenge[8];
unsigned char unicodepassword[NAME_BUF_LEN*2];

unsigned char lmv2digest[16];

unsigned char *blob = NULL;
int bloblen;

char *type2_targetinfo;
int type2_targetinfo_len=0;

unsigned long type2_flags;
unsigned long unicodeFlag = 0;


//
// Global functions for NTLMv2
//
int ntlmv2_build_type1();
int ntlmv2_parse_type2(unsigned char *buf);
int ntlmv2_build_type3();
int ntlmv2_prepare_type3();



int ntlmv2_build_type1() 
{
	ntlmv2_type1 type1;
	memset(&type1, 0, sizeof(type1));
	type1.signature[0] = 'N';
	type1.signature[1] = 'T';
	type1.signature[2] = 'L';
	type1.signature[3] = 'M';
	type1.signature[4] = 'S';
	type1.signature[5] = 'S';
	type1.signature[6] = 'P';
	type1.signature[7] = '\0';

	type1.messagetype = NTLM_MSG_TYPE_1;
	type1.flags = NTLM_NEG_UNICODE|NTLM_NEG_OEM|NTLM_REQ_TARGET|
				  NTLM_NEG_NTLM|NTLM_NEG_ASIGN|NTLM_NEG_NTLM2;

	base64((unsigned char*)ntlmv2_type1_buffer, (unsigned char *)&type1, sizeof(type1));

	return 0;
}





int ntlmv2_parse_type2(unsigned char *buf) 
{
	int len;
	char response[4096];
	ntlmv2_type2 *type2=NULL;

	if ((strncmp((char*)buf, "HTTP/1.1 407", strlen("HTTP/1.1 407"))==0) ||
		(strncmp((char*)buf, "HTTP/1.0 407", strlen("HTTP/1.0 407"))==0))
	{
		
	}
	else
	{
		return -1;
	}

	if (strstr((char*)buf, "Proxy-Authenticate: NTLM") == NULL)
	{
		return -1;
	}

	char* ptr = strstr((char*)buf, "Proxy-Authenticate: NTLM ");
	ptr += strlen("Proxy-Authenticate: NTLM ");
	
	//
	// Get 64 chars response and decode it
	//
	memset(response, 0, sizeof(response));
	memcpy(response, ptr, 64);

	len = unbase64((char*)ntlmv2_type2_buffer, (char*)response, MSG_BUF_SIZE);
	if (len <= 0) 
	{		
		return -1;
	}

	type2 = (ntlmv2_type2*)ntlmv2_type2_buffer;

	if (strcmp((char*)type2->signature, "NTLMSSP") != 0) 
	{	
		return -1;
	}

	if (type2->messagetype != NTLM_MSG_TYPE_2) 
	{		
		return -1;
	}

	
/*
	if (!(type2->flags & NTLM_NEG_NTLM && type2->flags & NTLM_NEG_NTLM2)) 
	{
		if(theApp.bEnablelogging)
			LogVPNProxy("Dll : ntlmv2_parse_type2: not support NEG_NTLM/NEG_NTLM2, return -1");		
		return -1;
	}
*/

	if (type2->flags & NTLM_NEG_UNICODE)
		unicodeFlag = 1;
	else
		unicodeFlag = 0;

	//
	// Save type2 flag to use later
	//
	type2_flags = type2->flags;

	//
	// Get the Challenge (8 bytes)
	//
	memcpy(challenge, type2->challenge, 8);
	
	//
	// Target Information 
	//
	type2_targetinfo = &ntlmv2_type2_buffer[type2->targetinfo.offset];
	type2_targetinfo_len = type2->targetinfo.length;

	return ntlmv2_prepare_type3();
}



int ntlmv2_prepare_type3() 
{
	int i, j;
	int passlen = 0;
	int userdomainlen=0;
	unsigned char passdigest[16];
	unsigned char userdomaindigest[16];	
	unsigned char respdigest[16];
	unsigned char lmv2data[16];
	unsigned char *userdomain;
	ntlmv2_blob *blob_ptr;
	MD4_CTX passcontext;
	
	//
	// The NTLM password hash is obtained.
	// this is the MD4 digest of the Unicode mixed-case password
	//
	memset(unicodepassword, 0, NAME_BUF_LEN*2);
	for (i = 0; i < strlen(gpassword); i++) 
	{
		if (unicodeFlag) 
		{
			unicodepassword[i*2] = gpassword[i];
			passlen++;
			passlen++;
		} 
		else 
		{
			unicodepassword[i] = gpassword[i];
			passlen++;
		}
	}

	MD4Init(&passcontext);
	MD4Update(&passcontext, unicodepassword, passlen);
	MD4Final(passdigest, &passcontext);

	//
	// The Unicode uppercase username is concatenated with the Unicode uppercase 
	// authentication target (domain or server name). 
	// The HMAC-MD5 message authentication code algorithm is applied to this value 
	// using the 16-byte NTLM hash as the key. 
	// This results in a 16-byte value - the NTLMv2 hash. 
	//
	userdomainlen = (strlen(gusername) + strlen(gdomain)) *2;
	userdomain = (unsigned char*)malloc(userdomainlen);
	if (!userdomain) 
	{
		return -1;
	}

	memset(userdomain, 0, userdomainlen);
	userdomainlen = 0;
	for (i = 0; i < strlen(gusername); i++) 
	{
		if (unicodeFlag) 
		{
			userdomain[i*2] = toupper(gusername[i]);
			userdomainlen++;
			userdomainlen++;
		} 
		else 
		{
			userdomain[i] = toupper(gusername[i]);
			userdomainlen++;
		}
	}

	for (j = 0; j < strlen(gdomain); j++) 
	{
		if (unicodeFlag) 
		{
			userdomain[i*2 + j*2] = toupper(gdomain[j]);
			userdomainlen++;
			userdomainlen++;
		} 
		else 
		{
			userdomain[i + j] = toupper(gdomain[j]);
			userdomainlen++;
		}
	}

	//
	// Apply HMAC-MD5, using NTLM hash as the key
	//
	hmac_md5(userdomain, userdomainlen, passdigest, 16, userdomaindigest);

	free(userdomain);

	//
	// A block of data known as the "blob" is constructed
	//
	bloblen = sizeof(ntlmv2_blob) + type2_targetinfo_len;
	blob = (unsigned char *)malloc(bloblen);
	if (!blob) 
	{
		return -1;
	}

	memset(blob, 0, bloblen);

	blob_ptr = (ntlmv2_blob*)blob;

	//
	// The challenge from the Type 2 message is concatenated with the blob. 
	//
	memcpy(blob_ptr->serverchallenge, challenge, 8);

	//
	// Client Challenge
	//
	for (i = 0; i < 8; i++)
		blob_ptr->clientchallenge[i] = (unsigned char)((256.0*rand())/(RAND_MAX + 1.0)) ;

	memcpy(&blob_ptr->data, type2_targetinfo, type2_targetinfo_len);

	blob_ptr->signature = 0x00000101;

	//
	// The HMAC-MD5 message authentication code algorithm is applied to 
	// this value using the 16-byte NTLMv2 hash (userdomaindigest) as the key. 
	// This results in a 16-byte output value. 
	// This value is concatenated with the blob to form the NTLMv2 response. 
	//

	//
	// Apply HMAC-MD5
	//
	hmac_md5(blob+8, bloblen-8, userdomaindigest, 16, respdigest);

	memcpy(blob_ptr->digest, respdigest, 16);

	//
	// LM2 response
	//
	memcpy(lmv2data, blob_ptr->serverchallenge, 8);
	memcpy(lmv2data+8, blob_ptr->clientchallenge, 8);
			
	//
	// Apply HMAC-MD5
	//
	hmac_md5(lmv2data, 16, userdomaindigest, 16, lmv2digest);
	
	return 0;
}



int ntlmv2_build_type3() 
{	
	int totalLen=0;
	int size=1, index=0;
	ntlmv2_type3 *type3;
	unsigned char *ptr;

	if (unicodeFlag)
		size = 2;

	totalLen = sizeof(ntlmv2_type3) + (16 + bloblen + size*(strlen(gdomain) + strlen(gusername) + strlen(gworkstation)));
	type3 = (ntlmv2_type3 *)malloc(totalLen);
	if (!type3) 
	{
		return -1;
	}
	
	memset(type3, 0, totalLen);
	ptr = (unsigned char*)type3;

	type3->signature[0] = 'N';
	type3->signature[1] = 'T';
	type3->signature[2] = 'L';
	type3->signature[3] = 'M';
	type3->signature[4] = 'S';
	type3->signature[5] = 'S';
	type3->signature[6] = 'P';
	type3->signature[7] = '\0';

	type3->messagetype = NTLM_MSG_TYPE_3;

	//
	// LM/LMv2 Resp
	//
	type3->LM_response.length = 16;
	type3->LM_response.space = 16;
	type3->LM_response.offset = sizeof(ntlmv2_type3);
	memcpy(&ptr[type3->LM_response.offset], lmv2digest, 16);

	//
	// NTLM/NTLMv2 Resp
	//
	type3->NTLM_response.length = bloblen;
	type3->NTLM_response.space = bloblen;
	type3->NTLM_response.offset = type3->LM_response.offset + type3->LM_response.space;
	memcpy(&ptr[type3->NTLM_response.offset], blob, bloblen);

	//
	// Domain
	//
	type3->domain.length = size*strlen(gdomain);
	type3->domain.space = size*strlen(gdomain);
	type3->domain.offset = type3->NTLM_response.offset + type3->NTLM_response.space;
	for (index = 0; index < strlen(gdomain); index++)
		ptr[type3->domain.offset + index*size] = gdomain[index];

	//
	// User
	//
	type3->user.length = size*strlen(gusername);
	type3->user.space = size*strlen(gusername);
	type3->user.offset = type3->domain.offset + type3->domain.space;
	for (index = 0; index < strlen(gusername); index++)
		ptr[type3->user.offset + index*size] = gusername[index];

	//
	// Workstation
	//
	type3->workstation.length = size*strlen(gworkstation);
	type3->workstation.space = size*strlen(gworkstation);
	type3->workstation.offset = type3->user.offset + type3->user.space;
	for (index = 0; index < strlen(gworkstation); index++)
		ptr[type3->workstation.offset + index*size] = gworkstation[index];

	//
	// Flags
	//
	type3->flags = type2_flags & ~NTLM_TAR_DOMAIN;

	//
	// Encode
	//
	base64((unsigned char*)ntlmv2_type3_buffer, (unsigned char*)type3, totalLen);

	if (type3 != NULL)
		free(type3);

	if (blob != NULL)
		free(blob);

	return 0;
}



int ConnectViaHttpProxy
(
	const char* destIp, 
	UINT	destPort, 
	char*	proxyIp, 
	UINT proxyPort, 
	char* username,
	char* password
);


#define MAX_BUFFER_SIZE			65536
#define MAX_NAME_LEN  64


#define LOCAL_PROXY_CONNECT_FAIL		-10
#define LOCAL_PROXY_AUTH_FAIL			-11
#define LOCAL_PROXY_RESP_FAIL			-12
#define LOCAL_PROXY_RECV_FAIL			-13
#define byte unsigned char




int DoNTLMv2
(
	int fd,
	const char* destIp, 
	UINT	destPort,
	unsigned char host[MAX_NAME_LEN],
	unsigned char domain[MAX_NAME_LEN],
	unsigned char user[MAX_NAME_LEN],
	unsigned char password[MAX_NAME_LEN]
)

{
	char buf[MAX_BUFFER_SIZE];
	char logstr[512];
	char*bufPtr=NULL;
	int numRead, numWrite;

	memset(buf, 0 , sizeof(buf));
	memset(ntlmv2_type1_buffer, 0 , sizeof(ntlmv2_type1_buffer));
	memset(ntlmv2_type3_buffer, 0 , sizeof(ntlmv2_type3_buffer));

	strcpy((char*)gworkstation, (char*)host);
	strcpy((char*)gdomain, (char*)domain);
	strcpy((char*)gusername, (char*)user);
	strcpy((char*)gpassword, (char*)password);

	

	//
	// Build msg1
	//
	if (ntlmv2_build_type1()<0)
	{
		
		//closesocket(fd);
		return -1;
	}

	printf("ntlmv2_type1_buffer=%s\n", ntlmv2_type1_buffer);

	//
	// Send msg1
	//
	sprintf(buf, "CONNECT %s:%hu HTTP/1.0\r\nProxy-Authorization: NTLM %s\r\n", destIp, destPort, ntlmv2_type1_buffer);
	sprintf( buf, "%sProxy-Connection: Keep-Alive\r\n\r\n", buf );
	
	numWrite = send(fd, buf, strlen(buf), 0);
	if (numWrite <=0)
    {
		//closesocket(fd);
		return -1;
    }

	sprintf(logstr, "Dll : DoNTLMv2: Sent type1msg buf(Actual:%d/Sent:%d)=%s", strlen(buf), numWrite, buf);
	printf(logstr);

	memset(buf, 0, sizeof(buf));

	if ((numRead = recv(fd, buf, MAX_BUFFER_SIZE, 0)) <= 0)
	{
		printf("Failed to receive response from this server\n");
		//closesocket(fd);
		return -1;
	}
	else
	{
		
	}

	//
	// Analyze response
	//
	if (ntlmv2_parse_type2((unsigned char*)buf) !=0)
	{
		printf("Wrong Type2\n");
		//closesocket(fd);
		return -1;
	}

	//
	// Build msg3
	//
	ntlmv2_build_type3();
	printf("ntlmv2_type3_buffer=%s\n", ntlmv2_type3_buffer);

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "CONNECT %s:%hu HTTP/1.0\r\nProxy-Authorization: NTLM %s\r\n", destIp, destPort, ntlmv2_type3_buffer);
	sprintf( buf, "%sProxy-Connection: Keep-Alive\r\n\r\n", buf );

	//
	// Send msg3
	//	
	numWrite = send(fd, buf, strlen(buf), 0);
	if (numWrite <=0)
    {
		//closesocket(fd);
		return -1;
    }

	sprintf(logstr, "Dll : DoNTLMv2: Sent type3msg buf(Actual:%d/Sent:%d)=%s", strlen(buf), numWrite, buf);
	printf(logstr);

	memset(buf, 0, sizeof(buf));
	if ((numRead = recv(fd, buf, MAX_BUFFER_SIZE, 0)) <= 0)
	{
		printf("Failed to receive response from this server\n");
		//closesocket(fd);
		return -1;
	}
	else
	{
		if (strncmp(buf, "HTTP/1.0 200", 12)==0 || strncmp(buf, "HTTP/1.1 200", 12)==0)
		{
			printf("Dll : DoNTLMv2: Successfully connected.\n");
			return fd;
		}
		else if ((strncmp((char*)buf, "HTTP/1.1 407", strlen("HTTP/1.1 407"))==0) ||
				 (strncmp((char*)buf, "HTTP/1.0 407", strlen("HTTP/1.0 407"))==0))
		{
			//closesocket(fd);
			return LOCAL_PROXY_AUTH_FAIL;
		}
		else
		{
			//closesocket(fd);
			return LOCAL_PROXY_AUTH_FAIL;
		}
	}

	return -1;
}


