#ifndef __MyNum
#define __MyNum

#define SizeOfMyNum 4

#ifndef __TByte
#define __TByte
typedef unsigned char byte;
#endif

#ifndef __TWord
#define __TWord
typedef unsigned int word;
#endif

#define MyNumLen	7

#include "HNumber.h"

struct MyNum
{
	int        sign;	// Znak chisla
	HugeNumber value; // Chislo bez znaka

	MyNum();
};

extern void Assign1(MyNum &x);
extern void Assign0(MyNum &x);
extern void Assign(MyNum &x, MyNum &y);
extern void ConvertHugeNumberToMyNum(MyNum &a, HugeNumber &b);
extern int IsZero(MyNum &a);
extern int CompareAbs(MyNum &a, MyNum &b);
extern void Subtr(MyNum &c, MyNum &a, MyNum &b);
extern void Div(MyNum &c, MyNum &a, MyNum &b);
extern void Mult(MyNum &c, MyNum &a, MyNum &b);
extern void Mod2(MyNum &c, MyNum &a);
extern void Div2(MyNum &c, MyNum &a);

#endif
