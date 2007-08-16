// General purpose functions
#include "stdafx.h"
//#include <conio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "MyTypes.h"

extern time_t MakePrimeL_Time;
extern time_t LongIsPrime_Time;

void randomize()
// Initialization of the random-number generator.
// It is added for compatibility with Borland C ++ Builder
{
   /* Seed the random-number generator with current time so that
     the numbers will be different every time we run.
    */
   srand( (unsigned)time( NULL ) );
};

long int random(long int ULimit)
// Generates a random number in the range 0..ULimit
// It is added for compatibility with Borland C ++ Builderr
{
	return (long int)(double(rand())/RAND_MAX*ULimit);
};

word wmin(word a, word b)
{
	if (a<b)
		return a;
	else
		return b;
};

word wmax(word a, word b)
{
	if (a>b)
		return a;
	else
		return b;
};

int LongIsPrime(unsigned long a)
{
   time_t t1=clock();
	unsigned long b,i;

	if (a%2==0)
   {
      LongIsPrime_Time+=clock()-t1;
      return 0;
   }
	b=(unsigned long)(sqrt(a))+1;
	for (i=3; i<b; i+=2)
	{
		if (a%i==0)
      {
         LongIsPrime_Time+=clock()-t1;
         return 0; // The tested number is compound
      }
	};

   LongIsPrime_Time+=clock()-t1;
	return 1; // The tested number is prime
};

unsigned long MakePrimeL()
// Generation of a random prime number
{
   time_t t1=clock();
	unsigned long a;

   a=random(5000); // MaxRazr>8200*(lg3/lg16)
	for (;;)
	{
		if (LongIsPrime(a)) break;
      a++;
	};
   MakePrimeL_Time+=clock()-t1;
	return a;
};
/*
void Error(char *msg)
{
	printf("\n%s\n",msg);
	abort();
};
*/
