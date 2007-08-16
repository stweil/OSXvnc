#ifndef __MyPrint
#define __MyPrint

#include <stdio.h>

#include "MyNum.h"

extern FILE *Prot;
extern void Protocol(char *Msg);
extern void Protocol(char *Msg, MyNum &a);
extern void Protocol(char *Msg, HugeNumber &a);
extern void ProtocolF(const char *Msg, float a);
extern void PrintCode(const unsigned char *Msg, const long MsgLen);
extern void PrintChar(unsigned char *ch, const long len);

#endif