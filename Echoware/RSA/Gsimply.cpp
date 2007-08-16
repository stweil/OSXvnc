/*
Programs of the superbig random numbers generation.

IgorSharov@rambler.ru
*/
#include "stdafx.h"
//#include <conio.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "HNumber.h"
#include "HNfunct.h"
#include "commonf.h"
#include "operator.h"
#include "GSimply.h"
#include "MyPrint.h"
#include "Profiler.h"

// For measurement of an operating time of the program
extern time_t GeneratePrimeHugeNumber_Time;
extern time_t GeneratePrime_Time;
extern time_t DeleteN_Time;
extern time_t Razl_Time;
extern time_t Construction_Time;
extern time_t Test_Time;

extern int Test2(HugeNumber &x);

unsigned long *N=NULL;	// Decomposition of number p-1 on prime multipliers
int N_size=0; 				// Dimension of N
int N_size_max=0;
int N_razr=22;

#define razr_default DeltaIndex
#define MaxRepeat 555	// Amount of attempts of multiplier's selection 


int WasRandomized=0;
extern int Rabin(int t, HugeNumber &S, HugeNumber &m, HugeNumber &mm1);
extern void randomize();

void PutInList(unsigned long i)
{	//  To bring number 'i' in the list of prime multipliers
	if (N==NULL)
	{
		N_size_max=razr_default;
		N=(unsigned long*)malloc(N_size_max*sizeof(unsigned long));
		if (N==NULL)
		{
			printf("\nNot enough memory in PutInList"); exit(0);
		};
		N_size=0;
	}
	else
	{
		if (N_size==N_size_max)
		{
			N_size_max+=razr_default;
			N   =(unsigned long*)realloc(N,   N_size_max*sizeof(unsigned long));
			if (N==NULL)
			{
				printf("\nNot enough memory in PutInList"); exit(0);
			};
		}
		else
		{
			if (N_size>N_size_max)
			{
				printf("Internal error: N_size is great then N_size_max"); exit(0);
			};
		};
	};
	N[N_size++]=i;
};

void Razl(unsigned long y)
// Decomposition of number 'y' on prime multipliers
{
   time_t t1=clock();
	long i,n;
	char flag;	// The 'flag' will be equal 'Y' if even one divisor is found.
					// Otherwise the flag will be equal 'N'.

	for (flag='Y'; flag=='Y'; )
	{	// We shall leave from this cycle when we find out that the tested 
		// number has no any prime divider.
		flag='N';
		n=(long)(sqrt(y))+1;
		// We divide tested number by 2,3, ... y^0.5+1
		for (i=2; i<=n; i++)
		{
			if (y%i==0)
			{	// i divides y without the remainder
				// Whether is i a prime number?
				if (LongIsPrime(i))
				{	// Put number 'i' in the list of prime multipliers of 'y'					PutInList(i);
					y=y/i;
					flag='Y';
					break;
				};
			};
		};
	};
	// The number 'y' does not have any prime divisor.
	// Put number y in the list of prime multipliers
	if (y!=1) PutInList(y);
   Razl_Time+=clock()-t1;
};

int PrimeQ(HugeNumber &km1,HugeNumber &k)
// This function returns:
//   1 if the number k MAY BE simple
//   0 if the number is compound
{
	HugeNumber x;

	ModPower(x,three,km1,k);

	if (x==one)
		return 1;
	else
		return 0;
};

void DeleteN()
{
   time_t t1=clock();

	if (N!=NULL)
	{
		free(N); N=NULL;
		N_size=0;
	};
   DeleteN_Time+=clock()-t1;
};

int GeneratePrime(HugeNumber &p)
//	Generates prime number 'p' on the basis of prime numbers N [*]
//	If function can not generate simple number, it returns 0.
//	Function returns 1 if the prime number is generated.
// Knuth vol2 page 420 (?)
{
	HugeNumber x,pm1,y,tmp,z;
	int i,j,intTmp;
	unsigned long b;
   time_t t1=clock();
   time_t t2;

   Protocol("Let's try to construct huge prime from small prime's set.");
   t2=clock();
   y=N[0];
	for (i=1; i<N_razr; i++)
   {
      Mult1(y,N[i],tmp); y=tmp;
   };
   // N[] - set of small prime numbers
   // y == N[0]*N[1]*...*N[N_razr-1]
	x=two;
	for (i=0;i<MaxRepeat;i++)
	{
      tmp=x*y;
		p=tmp+one;
      if (Test2(p)==-1)
      {
         if (Rabin(0,tmp,p,tmp)==-1)
         {
      		if (PrimeQ(tmp,p))
            {
               break;
            };
         };
      };
		x=x+one;
	};
   Construction_Time+=clock()-t2;
   if (i>=MaxRepeat)
   {
      GeneratePrime_Time+=clock()-t1;
      Protocol("Attempt failure");
      return 0; // Very big multiplier is not necessary for me
   };

   ProtocolF("The number looks like prime and it's length is: ",(float)BytesInHN(p));
	// 'p' looks like a prime number
   t2=clock();
	pm1=p-one;

	// We search for decomposition of number pm1 on prime multipliers
	Razl(UnsignedLong(x));

	// Check of decomposition of number pm1 on prime multipliers
	// This check isn't necessary, but it is a good way to check my 
	// previous calculatings.
   y=N[0];
	for (i=1; i<N_size; i++)
   {
      Mult1(y,N[i],tmp); y=tmp;
   };

	if (y!=pm1)
	{
		Protocol("\nError detected while the number was compiling.");
//      getch();
		exit(0);
	};

   // Sorting of N[]
   for (j=0;j!=1;)
   {
      j=1;
      for (i=1; i<N_size; i++)
      {
         if (N[i-1]<N[i])
         {
            j=0;
            intTmp=N[i];
            N[i]=N[i-1];
            N[i-1]=intTmp;
         };
      };
   };

	// Let's check up performance of condition x**(pm1/N[i]) mod p != 1
	b=0;
	for (i=0; i<N_size; i++)
	{
		if (b!=N[i])	// To not examine repeating multipliers
		{
			b=N[i];
         tmp=b;
         tmp=pm1/tmp;
			ModPower(tmp,three,tmp,p);
			if (tmp==one)
			{
            GeneratePrime_Time+=clock()-t1;
            Test_Time+=clock()-t2;
				// Number is compound
				DeleteN();
            Protocol("Number is not prime.");
				return 0;
			};
		};
	};

	DeleteN();
   GeneratePrime_Time+=clock()-t1;
   Test_Time+=clock()-t2;
   Protocol("Number is prime.");
	return 1;
};

void GeneratePrimeHugeNumber(HugeNumber &x)
{
	int j;
   time_t t1=clock();

	if (!WasRandomized)
	{
		WasRandomized=-1;
		randomize();
	};
	for (;;)
	{
		DeleteN();
		for (j=0; j<N_razr; j++) PutInList(MakePrimeL());
      Protocol("Small prime numbers set is built.");
		if (GeneratePrime(x)) break;
	};
	DeleteN();
   GeneratePrimeHugeNumber_Time+=clock()-t1;
};

int SaveKeys(char *FileName, HugeNumber &x, HugeNumber &y)
// function returns:
// 1 - OK
// 0 - failure
{
	FILE *f;
	char buf[2222];
   unsigned long CheckSum=0;
   int i,n;

   f=fopen(FileName,"wt");
   if (f==NULL) return 0;

   n=x.Razr();
   for (i=0; i<n; i++) CheckSum+=x.digit[i];
   fprintf(f,"%i\n0x%s\n",n,x.ToStr(buf,2222));

   n=y.Razr();
   for (i=0; i<n; i++) CheckSum+=y.digit[i];
   fprintf(f,"%i\n0x%s\n",n,y.ToStr(buf,2222));

   fprintf(f,"%u\n",CheckSum);
   fclose(f);
   return 1;
};

int RestoreKey(char *FileName, HugeNumber &x, HugeNumber &y)
// function returns:
// 1 - OK
// 0 - failure
{
	int i,n;
	FILE *f;
	char buf[2222];
   unsigned long testCheckSum=0;
   unsigned long CheckSum=0;

   f=fopen(FileName,"rt");
   if (f==NULL) return 0;

   //=== Reading the first part of the key ===
   fscanf(f,"%i\n",&n);
   // Here should be n*2+1>2222
   // I'm sure but some check won't hurt the program.
   if (n*2+1>2222)
   {
      fclose(f);
      return 0;
   };

   fscanf(f,"%s\n",buf);

   x=buf;
   for (i=0; i<n; i++) CheckSum+=x.digit[i];
   // See comments above
   if (n!=x.Razr())
   {
      fclose(f);
      return 0;
   };

   //=== Reading the second part of the key ===
   fscanf(f,"%i\n",&n);
   if (n*2+1>2222)
   {
      fclose(f);
      return 0;
   };

   fscanf(f,"%s\n",buf);
   y=buf;

   for (i=0; i<n; i++) CheckSum+=y.digit[i];
   if (n!=y.Razr())
   {
      fclose(f);
      return 0;
   };

   fscanf(f,"%u\n",&testCheckSum);
   fclose(f);
   if (testCheckSum!=CheckSum) return 0;
   return 1;
};

