#if !defined(_NTLM_)
#define _NTLM_



#define MAX_HOSTLEN 32
#define MAX_DOMLEN 32
#define MAX_USERLEN 32
#define RESP_LEN 24
#define NONCE_LEN 8

#define MAX_NAME_LEN  64
#define MAX_MSG_LEN   2048

#define NTLM_NEG_UNICODE		0x00000001
#define NTLM_NEG_OEM			0x00000002
#define NTLM_REQ_TARGET			0x00000004
#define NTLM_NEG_NTLM			0x00000200
#define NTLM_NEG_DOMAIN			0x00001000
#define NTLM_NEG_LOCAL			0x00004000
#define NTLM_NEG_ASIGN			0x00008000
#define NTLM_TAR_DOMAIN			0x00010000
#define NTLM_NEG_NTLM2			0x00080000



#define NTLM_MSG_TYPE_1			0x00000001
#define NTLM_MSG_TYPE_2			0x00000002
#define NTLM_MSG_TYPE_3			0x00000003

int DoNTLMv1
(
	int sock, 
	const char* destIp, 
	unsigned int	destPort,
	unsigned char host[MAX_NAME_LEN],
	unsigned char domain[MAX_NAME_LEN],
	unsigned char user[MAX_NAME_LEN],
	unsigned char password[MAX_NAME_LEN]
);



int DoNTLMv2
(
	int sock, 
	const char* destIp, 
	unsigned int	destPort,
	unsigned char host[MAX_NAME_LEN],
	unsigned char domain[MAX_NAME_LEN],
	unsigned char user[MAX_NAME_LEN],
	unsigned char password[MAX_NAME_LEN]
);





#endif