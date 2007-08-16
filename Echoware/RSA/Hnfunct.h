#ifndef _HNFUNCT__H
#define _HNFUNCT__H

#include "Hnumber.h"

extern HugeNumber zero,one,two,three;
extern int BaseRazr();
extern void GCD(HugeNumber &uu, HugeNumber &vv, HugeNumber &u);
extern void LCM(HugeNumber &uu, HugeNumber &vv, HugeNumber &u);
extern void Mult1(HugeNumber& u, word v, HugeNumber& w);
extern void Div1(HugeNumber &u, word v, HugeNumber &w, word &r);
extern void Div(HugeNumber& uu, HugeNumber& vv, HugeNumber& qq, HugeNumber& r);
extern double Double(HugeNumber &a);
extern unsigned long UnsignedLong(HugeNumber &a);
extern void ModPower(HugeNumber &Y, HugeNumber &x, HugeNumber &n, HugeNumber &f);
extern void Power(HugeNumber &Y, HugeNumber &x, HugeNumber &n);
extern int BytesInHN(HugeNumber &x);

#endif
