/*
IgorSharov@rambler.ru
*/
#include "stdafx.h"

#include "HNumber.h"

int operator !=( const HugeNumber &test1, const HugeNumber &test2 )
// 1 esli test1!=test2, inache 0
{
	word i,r1,r2;

	r1=test1.Razr(); r2=test2.Razr();
	if (r1 != r2) return 1;

	for (i=0; i<r1; i++)
	{
		if (test1.digit[i] != test2.digit[i]) return 1;
	};
	return 0;
};

int operator == ( const HugeNumber &test1, const HugeNumber &test2 )
// 1 esli test1==test2, inache 0
{
	word i,r1,r2;

	r1=test1.Razr(); r2=test2.Razr();
	if (r1 != r2) return 0;

	for (i=0; i<r1; i++)
	{
		if (test1.digit[i] != test2.digit[i]) return 0;
	};
	return 1;
};

int operator < ( const HugeNumber &test1, const HugeNumber &test2 )
// 1 elsi test1<test2, inache 0
{
	size_t r1,r2;
	int i;

	r1=test1.Razr(); r2=test2.Razr();
	if (r1 < r2) return 1;
	if (r1 > r2) return 0;

	// V chislax odinakovoe kolichestvo razriadov
	for (i=r1-1; i>=0; i--)
	{
		if (test1.digit[i] < test2.digit[i]) return 1;
		if (test1.digit[i] > test2.digit[i]) return 0;
	};
	return 0;
};

int operator > ( const HugeNumber &test1, const HugeNumber &test2 )
// 1 esli test1<test2, inache 0
{
	size_t r1,r2;
	int i;

	r1=test1.Razr(); r2=test2.Razr();
	if (r1 > r2) return 1;
	if (r1 < r2) return 0;

	// V chislax odinakovoe kolichestvo razriadov
	for (i=r1-1; i>=0; i--)
	{
		if (test1.digit[i] > test2.digit[i]) return 1;
		if (test1.digit[i] < test2.digit[i]) return 0;
	};
	return 0;
};

int operator <=( const HugeNumber &test1, const HugeNumber &test2 )
// 1 esli test1<=test2, inache 0
{
	size_t r1,r2;
	int i;

	r1=test1.Razr(); r2=test2.Razr();
	if (r1 < r2) return 1;
	if (r1 > r2) return 0;

	// V chislax odinakovoe kolichestvo razriadov
	for (i=r1-1; i>=0; i--)
	{
		if (test1.digit[i] <= test2.digit[i]) return 1;
		if (test1.digit[i] >  test2.digit[i]) return 0;
	};
	return 0;
};
