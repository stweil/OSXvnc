/*
IgorSharov@rambler.ru
*/
#include "stdafx.h"
#include <Time.h>
#include "HNumber.h"
#include "HNFunct.h"
#include "Operator.h"
#include "MyPrint.h"
#include "CommonF.h"
#include "MyTypes.h"

#define MaxInt (int)(2147483647)

extern time_t Misha_Time;
extern long int random(long int);

void NextPrime(int &intA)
{
   for (;;)
   {
      intA+=2;
      if (LongIsPrime(intA)) break;
   };
};

int Rabin(int t, HugeNumber &S, HugeNumber &m, HugeNumber &mm1)
// Test Rabina. Ispityemoe chislo imeet vid mm1=m-1=(2^t)*s
// returns:
//   0 if x - sostavnoe chislo
//  -1 esli ne ydjotsia ystanovit, chto x- sostavnoe
{
   HugeNumber s,x,b;
   word wi;
   int i,flag;

   Protocol("Rabin's test is working");
   // s dolgno bit nechiotnim
   for (s=S;;)
   {
      Div1(s,2,x,wi);
      if (wi!=0) break;
      s=x; t++;
      if (t==MaxInt)
      {
         Protocol("Rabin: t is too big!");
         return -1;
      };
   };

   if (t==0) return -1;

   i=m.Razr()-1;
   if (i>0)
      b.Random(i);
   else
      b.digit[0]=2+random(UnsignedLong(m)-2);

   ModPower(x,b,s,m);
   // x==(b^s) mod m
   if (x!=one && x!=mm1)
      flag=1;
   else
      flag=0;

   for (i=1; i<=t; i++)
   {
      x=x*x;
      x=x%m;
      if (flag==1 && x==one) return 0;
      if (x!=one && x!=mm1)
         flag=1;
      else
         flag=0;
   };
   if (x!=one)
      return 0;
   else
      return -1;
};

int Test1(HugeNumber &a, HugeNumber &n, HugeNumber &nm1, HugeNumber &degree)
// Function returns
// -1 - n is prime
// +1 - We need aditionl checks for n
//  0 - n is not prime
{
   HugeNumber b;

   Protocol("Main test is working.");
   //Protocol("n=",n);
   // Now: nm1=(2^s)*k, n=nm1+1, k<2^s, degree=(n-1)/2

   ModPower(b,a,degree,n);
   if (b==nm1) return -1;
   if (b==one) return +1;
   return 0;
};

int Test2(HugeNumber &x)
// Returns:
//   0 if x - sjstavnoe chislo
//  -1 if ne ydajotsia ystanovit, chto x ne prostoe chislo
{
   HugeNumber tmp;
   word wi;
   int i;

   Div1(x,2,tmp,wi);
   if (wi==0) return 0;
   for (i=3; i<base; i+=2)
   {
      if (LongIsPrime(i))
      {
         Div1(x,i,tmp,wi);
         if (wi==0) return 0;
      };
   };
   return -1;
};

void MakePrime(int intS, HugeNumber &n)
// Generator prostix chisel na osnove teoremi, kotoryiy mne dal Misha
// intS  - stepen dvojki
{
   int intA,i,j,BitsPerDigit,counter=0;
   HugeNumber k,tmp,degree,c,nm1,kMax,HNintA,HNtmp2;
   word wi;
   time_t t1=clock();

   #ifdef HexBase
      BitsPerDigit=BaseRazr()*4;
   #else
  //    Can't work in unhexdecimal system!
   #endif

   //ProtocolF("s=",(float)intS);
	HNtmp2=intS;
   Power(kMax,two,HNtmp2);
   kMax=kMax-one;
   k.Random(intS/BitsPerDigit);  // Slychiajnoe k < 2^s
   for (;;)
   {
      k=k+one;
      for(;k>kMax;) k.Random(intS/BitsPerDigit);  // Slychiajnoe k < 2^s
      intA=3;
      // Vibirajy osnovanie modylnoj eksponenti
      // a and k dolgni bit vzaimno prosty
      for (;;)
      {
         Div1(k,intA,tmp,wi);
         if (wi!=0) break;
         NextPrime(intA);
         if (intA>=base)
         {
            intA=3;
            k.Random(intS/12);  // Slychiajnoe k < 2^s
         };
      };

	  HNtmp2=intS-1;
      Power(tmp,two,HNtmp2);  // tmp == 2^(s-1)
		// Zdes ymnogenie na stepen dvojki mogno zamenit sdvigom.
      degree=k*tmp;           // degree ==(n-1)/2
                              // Note: n will be defined a bit later
      //Protocol("degree=",degree);
		// Zdes toge mogno primenit sdvig
      Mult1(tmp,2,c);
      // Now: c=2^s
      nm1=c*k;
      // Now: nm1=(2^s)*k
      n=nm1+one;                // Here n is defined
      //Protocol("n=",n);

		// Nachnajy sikl proverok chisla
      for (wi=1;wi==1;)
      {
			// Test na malii mnogiteli
         i=Test2(n);
         if (i==0) break;

			// Test Rabina
         i=Rabin(intS,k,n,nm1);
         if (i==0) break;

			// Vipolnit osnovnoj test na prostoty
         for (j=0;j<5;j++)
         {
			HNintA=intA;	
			 i=Test1(HNintA, n, nm1, degree); counter++;
            if (i==0)
            {
               Protocol("n is not prime!");
               break;
            };
            if (i==-1)
            {
               Protocol("n is prime!");
               break;   // n is prime!
            };
            if (i==1)
            {
               Protocol("We need aditional checks for n");
               NextPrime(intA);
					// Ja xochy chtiby vsegda intA<base
               if (intA>=base) i=0;
            };
            if (i!=1) break;
         };
			// Esli osnovnoj test ne smog opredelit prostoty za pjat
			// popitok, to bery drugoe chislo
         if (j==5) break;
         wi=i;
      };
      if (i==-1) break;
   };
   //ProtocolF("GeneratePrime: Digits in n=",(float)BytesInHN(n));
   //ProtocolF("GeneratePrime: counter=",(float)counter);
   Misha_Time+=clock()-t1;
   //ProtocolF("GeneratePrime: Misha_Time=",(float)Misha_Time/CLK_TCK);
};

