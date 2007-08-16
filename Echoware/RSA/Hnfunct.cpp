/*
Functions for work with huge numbers
IgorSharov@rambler.ru
*/

#include "stdafx.h"
//#include <conio.h>	//Debug
#include <time.h>		//Debug
#include "Hnumber.h"
#include "operator.h"
#include "commonf.h"
#include "string.h"
#include "stdio.h"
#include "MyPrint.h"

#define buf_s 2222
extern time_t ModPower_Time;

HugeNumber zero("0"),one("1"),two("2"),three("3");

int BaseRazr()
//				Vozvrashaet chislo simvolov, neobxodimsx dlia predstavlenia odnogo razriada
/// Function returns number of the symbols necessary for representation of one rank
{
   int i,n;

	i=1;

	#ifdef HexBase
		for(n=0; i<base; n++) i=i*0x10;
	#else
		#ifdef DecBase
		for(n=0; i<base; n++) i=i*10;
		#else
		!!! Ne opredelen vid zapisi chisel !!!
		!!! 'HexBase' or 'DecBase' should be  determined !!!
		#endif
	#endif
   return n;
};

void GCD(HugeNumber &uu, HugeNumber &vv, HugeNumber &u)
//				Vizvrashaet naibolshij obshij delitel (parametr 'u')
/// Function returns the greatest common divider of two numbers (parameter 'u')
/// algorithm: see Knuth, vol2 page 361 (?)
{
	HugeNumber r,v;

	if (uu>vv)
	{
		u=uu;	v=vv;
	}
	else
	{
		u=vv;	v=uu;
	}

	for (;;)
	{
		// Step A1
		if (v==zero) return;
		// Step A2
		r=u % v; u=v; v=r;
	};
};

void LCM(HugeNumber &a, HugeNumber &b, HugeNumber &lcm)
/// The least common multiple of two nonnegative huge numbers
{
   HugeNumber tmp;

	GCD(a,b,lcm);
   tmp=a*b;
	lcm=tmp/lcm;
};

void Mult1(HugeNumber& u, word v, HugeNumber& w)
//				Arifmetika smeshannoj tochnosti
//				u Mnogimoe c mnogokratnoj tochnostij
//				v Mnogitel c odnokratnoj tochnostij
//				w Rezultat c mnogokratnoj tochnostij

/// Arithmetics of the mixed accuracy
/// u - multiplicand (repeated accuracy)
/// v - multiplier (unitary accuracy)
/// w - result (repeated accuracy)
{
	int j;
	int M;	//				Istinnaij razriadnist chisla
				/// The real word length of number
	dword t,k,dv;

	//				Shag M1. Nachalnaij ustanovka.
	/// Step M1. Initialization
	M=u.Razr();
	w=zero;
	w.Resize(M+1);
	dv=v;

	k=0;
	for (j=0; j<M; j++)
	{
				//				Shag M4. Umnogit i slogit
				/// Step M4. Multiply and add
				t=((dword)u.digit[j])*dv+((dword)w.digit[j])+k;
				w.digit[j]=word(t % base);
				//				!!! Zdes neobxodimo izpolzovat sdvig !!!
				/// !!! Here it is necessary to use shift !!!
				k=t/base;	/// Figure of carry
								//				Cifra perenosa
	};
	w.digit[M]=word(k);
};

void Div1(HugeNumber &u, word v, HugeNumber &w, word &r)
//				Arifmetika smeshannoj tochnosti
/// Arithmetics of the mixed accuracy
/// w=u/v; r=u%v
{
	size_t n;
	int i;
	dword t;

	n=u.Razr();
	w=zero;
	w.Resize( wmax(n,1) );

	r=0;
	for (i=n-1; i>=0; i--)
	{
		t=r;
		//					Nado zamenit umnogenie na sdvig
		// !!! It is necessary to replace multiplication t*base with shift !!!
		t=(t * base) + u.digit[i];
		w.digit[i]=word(t/v);
		r=word(t-w.digit[i]*v);
	};
};

void Div(HugeNumber& uu, HugeNumber& vv, HugeNumber& qq, HugeNumber& r)
//				Delenie uu na vv
//				Chastnoe - qq
//				Ostatok - r

// Knuth vol2 page291 (?)
// qq= uu / vv; r=uu mod vv
/// !!! To not delete calls of debugging procedure 'Protocol' in comments !!!
{
	word d;
	size_t Nrazr;  //				Nomer razriada, ispolzuemogo pri poluchenii
                  //				predstavlenia chisla v dopolnitelnom kode.
						/// Number of the rank used at reception of representation 
						/// of number in an additional code.
	HugeNumber u,v,w,z,x,f;
	int M;         //				Razriadnost chastnogo
						/// Rank of quotient.

	int N;         /// Razriadnost ostatka (delitelia)
						/// Rank of remainder (divisor)

	int n;         /// n==N-1
	int i,j;       
	int c;         /// Control value on step D4
	unsigned long q;
   int x_razr,f_razr,z_razr;
	word tmp;

	// Obnuliaem chastnoe i ostatok
	qq=zero;	/// quotient
	r=zero;	/// remainder

	// D0.			Opredeliaem resriadnost ...
	/// We determine the rank of ...
	//					...delitelia
	/// ...divisor
	N=vv.Razr(); n=N-1;
	if ((N==1) && (vv.digit[0]==0))
   {
      printf("divisor ==0 !");
//      getch();
   }

	//					...delimogo
	/// ...dividend
	M=uu.Razr();
	if (M==1 && uu.digit[0]==0) return ;

	if (M<N)
	{	//				Rasriadnost delimogo < rasriadnosti delitelia
		/// The rank of dividend < the rank of divisor..
		r=uu;
		return;
	};

	if (N==1)
	{  //				Rasriadnost delitelia == 1 - primeniaj bolee prostoj algoritm.
		/// The rank of divisor == 1 - we use more simple algorithm.
		if (M==1)
		{
			word a=uu.digit[0];
			word b=vv.digit[0];

			qq.digit[0]=a/b;
			r.digit[0]=a-qq.digit[0]*b;
		}
		else
			Div1(uu,vv.digit[0],qq,r.digit[0]);
		return;
	};

	// D1. Normalize
	d=base/(vv.digit[n]+1);
	if (d>1)
	{
		Mult1(uu,d,u);
		Mult1(vv,d,v);
		//Protocol("u=",u);
		//Protocol("v=",v);
	}
	else
	{	//				A nugni li eti operatori? Da, nugni.
		/// Whether or not this operators are necessary???  Yes, they are necessary.
		v=vv;
		u=uu;
	};
   M++;

	u.Resize(M);
	x.Resize(N+2);
	qq.Resize(M-N+1);
	z.Resize(N+1);
	for (j=M-1; j>=N; j--)
	{
		// D3
		if (u.digit[j]==v.digit[n])
			q=base-1;
		else
		{
			q=u.digit[j];
			q=(q*base+u.digit[j-1])/v.digit[n];
		}

		while ( v.digit[n-1]*q > (( u.digit[j]*dword(base) + u.digit[j-1] - q*v.digit[n] )*dword(base) + u.digit[j-2])) q--;

		// D4.		Umnogit i vichest. u[j..j+N]=u[j..j+N]-v[0..N]*q
		/// Multiply and subtract. 
		Mult1(v,word(q),w); // w=v*q
		//Protocol("w=",w);
		
		for (i=0; i<=N;     i++) x.digit[N-i]=u.digit[j-i];	// x[]=u[j..j+N]
      x_razr=x.Razr();
		for (   ; i<x_razr; i++) x.digit[i]=0;
		//Protocol("x=",x);
		
		if (x < w)
			c=-1;
		else
			c=0;
		//				c=-1 pri x<w, c=0 pri x=w i pri x>w
		/// c=-1 if x<w, c=0 if x=w and if x>w

		if (c<0)
		{
			Nrazr=x.Razr();
			x.digit[Nrazr]=1;
		};

		f=x-w;	//				Operisija prisvaivanija moget umenshit razriadnost
					///  Giving operation can reduce rank
		//Protocol("f=",f);	
      f_razr=f.Razr();
		for (i=0; i<f_razr; i++) z.digit[i]=f.digit[i];
      z_razr=z.Razr();
		for (   ; i<z_razr; i++) z.digit[i]=0;
		//Protocol("z=",z);	

		for (i=0; i<=N; i++)
		{
			// u[j..j+N]=u[j..j+N]-v[0..N]*q
			if (N-i>z_razr-1)
				u.digit[j-i]=0;
			else
				u.digit[j-i]=z.digit[N-i];
		};
		//Protocol("u=",u);	

		//D5.			Proverit ostatok
		/// Check up the remainder.
		qq.digit[j-N]=word(q);
		//Protocol("qq=",qq);	

		if (c<0)
		{
			//D6.		Kompensirujushee slogenie
			///Compensating addition
			qq.digit[j-N]-=1;
			//Protocol("qq=",qq);

			//			z zapisano v dopolnitelnom kode -> pri slogenii bydet vixod za
			//			'razriadnyjy setky'
			/// z it is written down in additional code - > addition there 
			/// will be an output for a 'grid '
			x=z+v;

			//			Ynichtogajy cifry, kotoraja pri slogenii vishla za granicy
			//			'razriadnoj setki'
			/// I destroy figure which at addition has left abroad of the grid
			//Protocol("1) x=",x);
			x.digit[Nrazr]=0;
			//Protocol("1.1) x=",x);

			//			Pri kompensiryjyshem slogenii pojavliaetsia perenos v starshij
			//			razriad -  etim perenosom sledyet prenebrech
			/// At compensating addition there is a carry to the senior 
			/// rank - this carry should be neglected.
			//Protocol("2) x=",x);
			for (i=0; i<=N; i++) u.digit[j-i]=x.digit[N-i];
			//Protocol("3) u=",u);
		};
	};
	//				Teper qq est isxodnoe chastnoe
	/// Now qq is a required quotient.
	// D8.		Denormalizacija (poluchaem isxodnij ostatok)
	/// Unstandardization (the required quotient is received)
	//Protocol("u=",u);

	Div1(u,d,r,tmp); // r[]=u[M..0]/d
	//Protocol("r=",r);
};

double Double(HugeNumber &a)
{
	double x;
	int i;
	int n;

	n=a.Razr();
	x=0;
	for (i=n-1; i>=0; i--) x=(x*(base)+a.digit[i]);
	return x;
};

unsigned long UnsignedLong(HugeNumber &a)
{
	unsigned long x;
	int i,n;

	n=a.Razr();
	x=0;
	for (i=n-1; i>=0; i--) x=x*base+a.digit[i];
	return x;
};

void ModPower(HugeNumber &Y, HugeNumber &x, HugeNumber &n, HugeNumber &f)
//				Binarnij metod vozvedenija v stepen po moduliy f
/// Binary method of exponentation on the module f
/// Knuth, page 483 (?)
/// Y=(x^n) mod f
{
	HugeNumber Nbig,Z;
	HugeNumber i;
	HugeNumber tmp;

	// A1.			Nachlnaja ystanovka
	/// Initialisation
	Nbig=n;	//Assign(Nbig,n);
	Y=one;	//Assign1(Y);
	Z=x; 		//Assign(Z,x);

	for (;;)
	{
		// A2.		Razdelit Nbig popolam
		/// Divide Nbig half-and-half
		Div(Nbig,two,tmp,i);	//Mod2(i,Nbig);
									//Div2(Nbig,Nbig);
		Nbig=tmp;
		//Print("A2. Nbig=",Nbig);

		//				Bilo li staroe znachenie Nbig chiotno?
		/// Whether there was old value Nbig even?
		if (i!=zero)				//if (!IsZero(i))
		{
			// A3.	Umnogit Y na Z
			/// Multiply Y on Z
         tmp=Y*Z;
			Y=tmp%f;				//Mult(tmp,Y,Z);
									//Mod(Y,tmp,f);

			//Protocol("A3. Y=",Y);
			// A4. Nbig = 0 ?
			if (Nbig==zero) return;	//if (IsZero(Nbig)) return;
		};

		// A5.		Vozvesti Z v kvadrat
		/// Raise Z to the second power
      tmp=Z*Z;
		Z=tmp%f;		//Mult(tmp,Z,Z);
						//Mod(Z,tmp,f);
		//Protocol("A5. Z=",Z);
	};
};

void Power(HugeNumber &Y, HugeNumber &x, HugeNumber &n)
// Binary method of exponentation
// Knuth, page 483 (?)
// Y=x^n
{
	HugeNumber Nbig,Z;
	HugeNumber i;
	HugeNumber tmp;

	// A1. Initialisation
	Nbig=n;
	Y=one;
	Z=x;

	for (;;)
	{
		// A2. Divide Nbig half-and-half
		Div(Nbig,two,tmp,i);

		Nbig=tmp;
		//Protocol("A2. Nbig=",Nbig);
		// Whether there was old value Nbig even?
		if (i!=zero)
		{
			// A3. Multiply Y on Z
         Y=Y*Z;
			//Protocol("A3. Y=",Y);
			// A4. Nbig = 0 ?
			if (Nbig==zero) return;
		};

		// A5. Vizvesti Z v kvadrat.
		// Raise Z to the second power
      Z=Z*Z;
		//Protocol("A5. Z=",Z);
	};
};

int BytesInHN(HugeNumber &x)
//				Vozvrashaet chislo baitov v x
/// Returns number of bytes in x
{
   char *buf;
   int n;

   n=BaseRazr()*x.Razr()+1;
   buf=new char[n];
   x.ToStr(buf,n);
   n=strlen(buf);
   delete buf;
   return n;
};

