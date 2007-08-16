/*
IgorSharov@rambler.ru
Bolshie chisla so znakom
v2.0
*/

#include "stdafx.h"
#include "MyNum.h"
#include "HNFunct.h"
#include "Operator.h"

MyNum::MyNum()
{
	sign=+1;
	value=zero;
};

void Assign1(MyNum &x)
{
	x.sign=+1;
	x.value=one;
};

void Assign0(MyNum &x)
{
	x.sign=+1;
	x.value=zero;
};

void ConvertHugeNumberToMyNum(MyNum &a, HugeNumber &b)
{
	a.sign=+1;
	a.value=b;
};

void Assign(MyNum &x, MyNum &y)
{
	x.sign=y.sign;
	x.value=y.value;
};

int IsZero(MyNum &a)
{
	if (a.value==zero)
	{
		a.sign=+1;		// na vsiakij slychaj
		return -1;
	}
	else
		return 0;
};

int CompareAbs(MyNum &a, MyNum &b)
{
	if (a.value>b.value)
		return +1;
	else if (a.value<b.value)
		return -1;
	else
		return 0;
};

void Subtr(MyNum &c, MyNum &a, MyNum &b)
{
	if (a.sign*b.sign>0)
	{	// Chisla odnogo znaka
		if (CompareAbs(a,b)>0)
		{	// |a|>|b|
			c.sign=a.sign;
			c.value=a.value-b.value;
		}
		else
		{
			c.sign=-b.sign;
			c.value=b.value-a.value;
		};
	}
	else	// Chisla protivopolognix znakov
	if (a.sign>b.sign)
	{	// a>0 && b<0
		c.sign=+1;
		c.value=a.value+b.value;
	}
	else
	{	// a<0 && b>0
		c.sign=-1;
		c.value=a.value+b.value;
	};
};

void Div(MyNum &c, MyNum &a, MyNum &b)
{
	c.sign=a.sign*b.sign;
	c.value=a.value/b.value;
};

void Mult(MyNum &c, MyNum &a, MyNum &b)
{
	c.sign=a.sign*b.sign;
	c.value=a.value*b.value;
};

