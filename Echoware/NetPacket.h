#ifndef _NETPACKET_H
#define _NETPACKET_H

#if _MSC_VER > 1000
#pragma once
#endif 

enum
{
    PROXY_MSG_BASE = WM_USER + 1301,

    MSG_PROXY_CONNECTED = PROXY_MSG_BASE + 1, // 2326
    MSG_PROXY_PUBLICKEY,	// 2327
    MSG_PROXY_PASSWORD,		// 2328
    MSG_PROXY_INIT_HANDSHAKE,	// 2329
    MSG_PROXY_INIT_HANDSHAKEREPLY,	//2330
    MSG_PROXY_HANDSHAKECONFIRM,	// 2331
    MSG_PROXY_HANDSHAKECOMPLETE,	//2332
	MSG_PROXY_HANDSHAKEFAILED,	//2333
	MSG_PROXY_DISCONNECTED,	// 2334
	MSG_PROXY_FIND_PARTNER,	// 2335
	MSG_PROXY_PARTNERNOTFOUND,	//2336
	MSG_PROXY_PARTNERFOUND,	// 2337
	MSG_PROXY_VPN_SUCCESSFUL,	// 2339//2338
	MSG_PROXY_VPN_DISCONNECTED,	// 2340//2339
	MSG_PROXY_DUPLICATE_LOGIN,	// 2341//2340
	MSG_ISALIVE,	// 2342//2341

	MSG_CONNECT_TO_PEER, // 2343//2342
	MSG_CHANNEL_CODE, // 2344//2343
	MSG_PEER_KEY, // 2345//2344
	MSG_DATA_CHANNEL_CONNECT,	// 2346//2345

	PROXY_MSG_LAST
};

#define EF_NET_PACKET_ID "EF1.0"
#define PROXY_ECHO "KaboodleProxy_Protocol Version 1.0_Server Version 1.0"

enum {PACKET_MESSAGE=1,PACKET_OTHER=2};

struct NetPacketHeader
{
	char id[6];//EF packet and version identifier
	char type; //type of packet (usually message)	
	long len;
	NetPacketHeader()
	{
		strcpy( id, EF_NET_PACKET_ID );
		type = PACKET_MESSAGE;
	}
};

class CProxyMsg
{
public:
	UINT	messageid;//message or event id
	BYTE	usertype;//user type
	long	datalength;//length of the data following this header
	//bytearray data;//the actual data
	CProxyMsg() {}

	CProxyMsg(DWORD messageid,long datalength=0,BYTE usertype=0)
	{
		this->messageid=messageid;
		this->datalength=datalength;
		this->usertype=usertype;
	}
	int size()
	{
		return sizeof(CProxyMsg);
	}
	int MakeMessage(NetPacketHeader* pheader,char*& destbuffer,void* datapart=NULL,long datalength=0)
	{
	/*
		int s1 = sizeof(pheader->id);
		int s2 = sizeof(pheader->type);
		int s3 = sizeof(pheader->len);
		
		int s4 = sizeof(this->messageid);
		int s5 = sizeof(this->datalength);
		int s6 = sizeof(this->usertype);
		
		int len = s1 + s2 + s3 + s4 + s5 + s6;
		
		destbuffer = new char[len];
		
		pheader->len = len;
		memcpy(destbuffer, pheader->id, s1);
		memcpy(destbuffer + s1, &pheader->type, s2);
		
		DWORD tmp = OSSwapHostToLittleInt32(pheader->len);
		memcpy(destbuffer + s1 + s2, &tmp, s3);
		
		this->datalength=datalength;
		tmp = OSSwapHostToLittleInt32(this->messageid);
		memcpy(destbuffer + s1 + s2 + s3, &tmp, s4);
		memcpy(destbuffer + s1 + s2 + s3 + s4, &this->usertype, s5);
		tmp = OSSwapHostToLittleInt32(this->datalength);
		memcpy(destbuffer + s1 + s2 + s3 + s4 + s5, &tmp, s6);

		if (datapart) //copy data
			memcpy(destbuffer + len, datapart, datalength);

		return len;
*/
		int headersize=sizeof(NetPacketHeader);
		int mysize=sizeof(CProxyMsg);
		destbuffer=new char[headersize+mysize+datalength];
		memset(destbuffer, 0, headersize+mysize+datalength);
		
		pheader->len=OSSwapHostToLittleInt32(headersize+mysize+datalength);
		
		memcpy(destbuffer, &pheader->id, sizeof(pheader->id));
		
		WORD tmp1 = pheader->type;
		memcpy(destbuffer + sizeof(pheader->id), &tmp1, sizeof(tmp1));
		
		DWORD tmp2 = pheader->len;
		memcpy(destbuffer + sizeof(pheader->id) + sizeof(tmp1), &tmp2, sizeof(tmp2));
		
//		memcpy(destbuffer,pheader,headersize); //copy packet header
		pheader->len=headersize+mysize+datalength;

		// Swap to LE
		this->messageid=OSSwapHostToLittleInt32(messageid);
		this->datalength=OSSwapHostToLittleInt32(datalength);
		
		DWORD tmp = this->messageid;
		memcpy(destbuffer + headersize, &tmp, sizeof(tmp));
		
		tmp = this->usertype;
		tmp = OSSwapHostToLittleInt32(tmp);
		memcpy(destbuffer + headersize + sizeof(tmp), &tmp, sizeof(tmp));
		
		tmp = this->datalength;
		memcpy(destbuffer + headersize + sizeof(tmp) + sizeof(tmp), &tmp, sizeof(tmp));

		//memcpy(destbuffer+headersize,this,mysize); //copy message descriptor
		
		// Reverse
		this->messageid=OSSwapLittleToHostInt32(messageid);
		this->datalength=datalength;

		if (datapart) //copy data
			memcpy(destbuffer+headersize+mysize,datapart,datalength);
		return headersize+mysize+datalength;
	}
};
#endif