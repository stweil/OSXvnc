// General purpose functions
#ifndef __CommonF_H
#define __CommonF_H

#include <stdio.h>
#include "MyTypes.h"
#include "HNumber.h"

extern void Error(const char *msg);
extern word wmin(word a, word b);
extern word wmax(word a, word b);
extern void Close();
extern int LongIsPrime(unsigned long a);
extern unsigned long MakePrimeL();
extern void Error(char *msg);

#endif
