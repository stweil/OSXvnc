/*
IgorSharov@rambler.ru
*/
#include "stdafx.h"
#include <time.h>
//#include <conio.h>
#include <stdlib.h>
#include <stdio.h>

#include "MyTypes.h"
#include "Function.h"
#include "CommonF.h"
#include "HNfunct.h"
#include "Operator.h"
#include "MyPrint.h"
#include "MyNum.h"
#include "GSimply.h"

#define CLK_TCK   CLOCKS_PER_SEC

extern int N_razr;
extern int WasRandomized;
extern void MakePrime(int intS, HugeNumber &n);
extern void randomize();
extern long int random(long int);

time_t MakePrimeL_Time=0;
time_t LongIsPrime_Time=0;
time_t Misha_Time=0;
time_t Construction_Time=0;
time_t Razl_Time=0;
time_t DeleteN_Time=0;
time_t Test_Time=0;
time_t GeneratePrimeHugeNumber_Time=0;
time_t GeneratePrime_Time=0;

void clrscr()
// Dobavlena dlia sovmestimosti s Borland C++ Builder
// k sogalenijy ja ne obnarygil etoj fynkcii v Visual C++
// Eta funkcija dolgna ochishat ekran i ystanavlivat kyrsor
// v levij verxnij ygol ekrana.
{
};

void Euklid(MyNum &u, MyNum &v, MyNum &nod, MyNum &e)
// Obobshionnij algorint Evklida
// Knuth vol2 page367 (?)
{
	MyNum u1,u2,u3;
	MyNum v1,v2,v3;
	MyNum t1,t2,t3;
	MyNum q,tmp;

	// X1. Nachialnaja ystanovka
	Assign1(u1);
	Assign0(u2);
	Assign(u3,u);

	Assign0(v1);
	Assign1(v2);
	Assign(v3,v);

	// X2. v3==0 ?
	while (!IsZero(v3))
	{
		// X3. Razdelit, Vichest
		Div(q,u3,v3);
		Mult(tmp,v1,q);
		Subtr(t1,u1,tmp);
		Mult(tmp,v2,q);
		Subtr(t2,u2,tmp);
		Mult(tmp,v3,q);
		Subtr(t3,u3,tmp);

		Assign(u1,v1);
		Assign(u2,v2);
		Assign(u3,v3);
		Assign(v1,t1);
		Assign(v2,t2);
		Assign(v3,t3);
	};
	Assign(nod,u3);
	Assign(e,u2);
};

void CreateKeys(HugeNumber &D, HugeNumber &e, HugeNumber &N)
// Vichslenie pari klychej (D,N) (e,N)
{
	HugeNumber P,Q,p1q1,i,N100;
	HugeNumber a,b,P1,Q1;
	MyNum sP,sQ,sa,si,sD,se,sp1q1;	// Bolshie chisla so znakom
	long k;

	if (!WasRandomized)
	{
		randomize();
		WasRandomized=-1;
	};

	for (k=0;;k++)
	{
      Protocol("Calculating P ...");
      MakePrime(100, P);
      Protocol("Done. P=",P);
      Protocol("Calculating Q ...");
      MakePrime(100, Q);
      Protocol("Done. Q=",Q);
		if (P!=Q) break;
	};
	N=P*Q;
   Protocol("N=",N);

	P1=P-one;
	Q1=Q-one;
	p1q1=P1*Q1;

   Protocol("Calculating D and e ...");
// MakePrime(2, P); - tak delat nelzia!
// Etot metod ne rabotaet dlia malix stepenej dvojki
   for (k=random(55555); !LongIsPrime(k);k++);
   D=k;
	ConvertHugeNumberToMyNum(sp1q1,p1q1);
	ConvertHugeNumberToMyNum(sD,D);
	for (k=1;;k++)
	{
		Euklid(sp1q1,sD,si,se);
		if (si.value==one && se.sign>0) break;
		sD.value=sD.value+one;
	};
   Protocol("Done.");
	D=sD.value;
	e=se.value;
	// Vichislena para klychej (D,N) (e,N)
   Protocol("D=",D);
   Protocol("e=",e);
   ProtocolF("Amount iteration for calculation D and E is ",float(k));
};

void Keys(HugeNumber &d, HugeNumber &e, HugeNumber &nn)
{
	HugeNumber a,c;
	HugeNumber b("12345");
	unsigned long t_start;

	clrscr();
	randomize();
	Protocol("Keys generation process is started.");
	t_start=clock();
	CreateKeys(d,e,nn);
//	ProtocolF("Time of calculation (sec)= ",float((clock()-t_start)/CLK_TCK));
	ProtocolF("length (bytes) of d=",(float)BytesInHN(d));
	ProtocolF("length (bytes) of e=",(float)BytesInHN(e));
	ProtocolF("length (bytes) of n=",(float)BytesInHN(nn));
	ProtocolF("Your key's length is ",(float)BytesInHN(nn));
	if (nn>b)
	{
	 /*
	 As a matter in fact we don't need this step
	 but this is a good way to test our previous calculations.
	 */
		t_start=clock();
		Protocol("Begin of test encoding...");
		// (d,nn) - klych dlia zashifrovivanija
		ModPower(a,b,d,nn);
		Protocol("Done. Test decoding is started...");
		// (e,nn) - klych dlia rasshifrovivanija
		ModPower(c,a,e,nn);
		Protocol("Done.");
		if (c!=b)
		{
			Protocol("Error it test calculation.\n");
			abort();
		};
//	 ProtocolF("Time of test = ",(float)(clock()-t_start)/CLK_TCK);
	};

	Protocol("Keys generation process is done.");
};
