#ifndef _inc_blowfish_h
#define _inc_blowfish_h

#include <libkern/OSByteOrder.h>
// blowfish.h     interface file for blowfish.cpp
// _THE BLOWFISH ENCRYPTION ALGORITHM_
// by Bruce Schneier
// Revised code--3/20/94
// Converted to C++ class 5/96, Jim Conger

#define MAXKEYBYTES 	56		// 448 bits max
#define NPASS           16		// SBox passes

#ifndef DWORD
#define DWORD  		unsigned long
#endif
#define WORD  		unsigned short
#define BYTE  		unsigned char

class CBlowFish
{
// choose a byte order for your hardware
// not needed even more to change
#if		defined(__LITTLE_ENDIAN__)
//#define ORDER_DCBA	// chosing Intel in this case

//#ifdef ORDER_DCBA  	// DCBA - little endian - intel
	union aword {
	  DWORD dword;
	  BYTE byte [4];
	  struct {
	    unsigned int byte3:8;
	    unsigned int byte2:8;
	    unsigned int byte1:8;
	    unsigned int byte0:8;
	  } w;
	};
//#endif
#elif		defined(__BIG_ENDIAN__)
//#ifdef ORDER_ABCD  	// ABCD - big endian - motorola
	union aword {
	  DWORD dword;
	  BYTE byte [4];
	  struct {
	    unsigned int byte0:8;
	    unsigned int byte1:8;
	    unsigned int byte2:8;
	    unsigned int byte3:8;
	  } w;
	};
#else
#error Unknown endianess.
#endif

/*
#ifdef ORDER_BADC  	// BADC - vax
	union aword {
	  DWORD dword;
	  BYTE byte [4];
	  struct {
	    unsigned int byte1:8;
	    unsigned int byte0:8;
	    unsigned int byte3:8;
	    unsigned int byte2:8;
	  } w;
};
#endif
*/
private:
	DWORD 		* PArray ;
	DWORD		(* SBoxes)[256];
	void 		Blowfish_encipher (DWORD *xl, DWORD *xr) ;
	void 		Blowfish_decipher (DWORD *xl, DWORD *xr) ;

public:
			CBlowFish () ;
			~CBlowFish () ;
	void 		Initialize (BYTE key[], int keybytes) ;
	DWORD		GetOutputLength (DWORD lInputLong) ;
	DWORD		Encode (BYTE * pInput, BYTE * pOutput, DWORD lSize) ;
	void		Decode (BYTE * pInput, BYTE * pOutput, DWORD lSize) ;

} ;

#endif //_inc_blowfish_h
