/*
IgorSharov@rambler.ru
*/
#include "stdafx.h"
//#include <conio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "operator.h"
#include "CommonF.h"
#include "HNumber.h"
#include "HNfunct.h"
#include "MyTypes.h"

static void Add1(HugeNumber &a, word m);
static void Convert(char *b, HugeNumber &w);
extern long int random(long int);

HugeNumber::HugeNumber()
{
	int i;

	for (i=0; i<MaxRazr; i++) digit[i]=0;
};

HugeNumber::HugeNumber(HugeNumber &x)
// Konstryktor kopirovanija
{
	int i;

   for (i=0; i<MaxRazr; i++) digit[i]=x.digit[i];
};

HugeNumber::HugeNumber(unsigned long num)
{
	char x[11];
	word i;
	HugeNumber a;

	sprintf(x, "%lu", num);	//ultoa(num,x,10);
	Convert(x,a);

	for (i=0; i<MaxRazr; i++) digit[i]=a.digit[i];
};

HugeNumber::HugeNumber(char *x)
{
   int i,n;

   n=BaseRazr();
   if (x[1]=='x')
   {
		// Chislo v shestnadsaterichnoj zapisi
      int len,j,k;
      char buf[11],*str;

      buf[n]=0;
      len=strlen(x);
      str= new char[len+n];
      strcpy(str,&x[2]);

      for (len=strlen(str); len%n!=0; len++)
      {
         for (j=len; j>0; j--)
         {
            str[j]=str[j-1];
         };
         str[0]='0';
      };

      j=0;
      for (i=len-1; i>=0; i=i-n)
      {
         for (k=0; k<n; k++)
            buf[k]=str[i-n+k+1];
         sscanf(buf,"%x",&digit[j++]);
      };
      delete str;
      for (;j<MaxRazr;j++) digit[j]=0;
   }
   else
   {
		// Chislo v desiatichnoj zapisi
   	HugeNumber a;
   	Convert(x,a);
   	for (i=0; i<MaxRazr; i++) digit[i]=a.digit[i];
   }
};

HugeNumber& HugeNumber::operator++()
{
	word j,k;
	dword p;

	k=0;

	// Pribavliaem 1 k mladshej cifre chisla
	p=(((dword)digit[0])+1L);
	digit[0]= word(p % base);

	// Opredeliaem cifry perenosa
	if (p>=base) k=1;

	for (j=1; j<MaxRazr; j++)
	{
		if (k==0) return *this;
		p=(((dword)digit[j])+k);
		digit[j]= word(p % base);
		if (p>=base)
			k=1;
		else
			k=0;
	};
	// Ne xvataet razriada - yvelichivay razraidnost chisla
	Resize(MaxRazr+1);
	digit[j]=k;
	return *this;
};

HugeNumber& HugeNumber::operator--()
{
	word j,tmp;
	size_t m;
	int k;

	if ((digit[0])==0)
	{
		digit[0]=base-1;
		k=-1;
	}
	else
	{
		digit[0]=digit[0]-1;
		return *this;
	}

	m=Razr();
	for (j=1; j<m; j++)
	{
		tmp=word((((unsigned long)digit[j])+k+base) % base);
		if (tmp==base-1)
			k=-1;
		else
			k=0;
		digit[j]=tmp;
		if (k==0) return *this;
	};
	return *this;
};

int HugeNumber::Razr() const
// Opredeliaet realnyjy razriadnoast chisla
// Razriadnost chisla nol ==1
{
	word i;

	for (i=MaxRazr-1; i>0; i--)
		if (digit[i]!=0) break;
	return i+1;
};

void HugeNumber::Resize(int k)
// Yvelichivaet kolichestvo rezriadov v pedstavlenii chisla do k
{
	if (k>MaxRazr)
	{
		//printf("Zaprosheno uvelichenie razriadnosti do %i",k);
		//printf("Request for digit capacity increasing to %i has been found.", k);
		//      getch();
	}
}

void HugeNumber::Random(int k)
// Zapolniaet mladshie k razriadov slychajnimi znachenijami
// Starshie m-k razriadov zapolniajytsia nyliami
{
	int i;

	Resize(k);
	for (i=0; i<k; i++) digit[i]=random(base);
	for (   ; i<MaxRazr; i++) digit[i]=0;
};

HugeNumber& HugeNumber::operator =( const HugeNumber & source )
{
	word m,i;

	if (digit!=source.digit)
	{	// Chisla zapisani po raznim adresam pamiati
		m = source.Razr();
		for (i=0; i<m   ; i++) digit[i]=source.digit[i];
		for (   ; i<MaxRazr; i++) digit[i]=0;
	}
	else; // Chisla zapisani po odnomy i tomy ge adresy => prisvaivanie vida a=a
	return *this;
};

HugeNumber operator + ( HugeNumber &u, HugeNumber &v)
// Slogenie neotrisatelnix selix chisel
{
	word j;
	dword k; // Cifra perenosa
	word uk; // Chislo razriadov u
	word vk; // Chislo razriadov v
	word wk; // Chislo razriadov w
	word kw;
	dword p;
	HugeNumber w; // Zdes bydem nakaplivat rezyltat

	vk=v.Razr();
	uk=u.Razr();
	wk=wmax(uk,vk);
	kw=wmin(uk,vk);
	w.Resize(wk+1);	// Delaem nygnyiy razriadnost rezyltata

	k=0;
	// Symiryem razriadi po samomy korotkomy chisly
	for (j=0; j<kw; j++)
	{
		p=dword(u.digit[j])+dword(v.digit[j])+k;
		if (p>=base)
		{
			k=1;
			w.digit[j] = word(p & (base-1));
		}
		else
		{
			k=0;
			w.digit[j]= word(p);
		};
	};

	// Tiper nado prosymirovat ostavshiesia razriadi dlinnogo chisla i
	// razriad perenosa.
	if (k!=0)
	{	// Bil perenos pri symmirovanii po korotkomy chisly
		// Nado symmirivat ostavshiesia razriady
		if (uk>vk)
		{
			// Symmiryem, poka est cifra perenosa
			for (; j<uk; j++)
			{
				p=u.digit[j]+k;
				if (p>=base)
				{
					k=1;
      			w.digit[j] = word(p & (base-1));
				}
				else
				{
					k=0;
					w.digit[j]= word(p);
					break;
				};
			}
			// Kopiryem ostavshiesia razriady
			if (k==0) for (j++; j<uk; j++) w.digit[j]=u.digit[j];
		}
		else
		{
			// Symmiryem, poka est cifra perenosa
			for (; j<vk; j++)
			{
				p=v.digit[j]+k;
				w.digit[j]= word(p & (base-1));
				if (p>=base)
				{
					k=1;
				}
				else
				{
					k=0;
					break;
				};
			}
			// Kopiryem ostavshiesia razriady
			if (k==0) for (j++; j<vk; j++) w.digit[j]=v.digit[j];
		};
		if (k!=0)
		{	// Symmirovanie provedeno po vsem razriadam, no eshe est
			// cifra perenosa
			w.digit[j]=word(k);
		}
	}
	else
	{	// Nebilo perenosa pri symmirovanii po korotkomy chisly
		// Nado prosto skopirovat ostavshiesia razriady dlinnogo chisla
		if (vk>uk)
			for (; j<vk; j++)
				w.digit[j]=v.digit[j];
		else
			if (uk>vk)
				for (; j<uk; j++)
					w.digit[j]=word(u.digit[j]+k);
	};
	return w;
};

char* HugeNumber::ToStr(char *pointer, int size)
// Preobrasyet chislo mnogokratnoi tochnosti v striky
// pointer - byfer dlia stroki
// size    - razmer byfera
// returns: pointer esli preobrazovanie zaversheno normalno
//          NULL    esli byfer okazalsia mal
{
	int i,n;
	char buf[5];
	char format[111];

   n=BaseRazr();
	// n == chisly simvolov, neobxodimix dlia predstavlenia odnogo razriada
	if (size < this->Razr()*n+1) return NULL; // Byfer mal - stroka ne vlezet

	#ifdef HexBase
		sprintf(format,"%%%02iX",n);
	#else
		sprintf(format,"%%%02ii",n);
	#endif

	pointer[0]=0;
	for (i=this->Razr()-1; i>=0; i--)
	{
		sprintf(buf,format,this->digit[i]);
		strcat(pointer,buf);
	};

   // Ybrat neznachishai nyli
   for (;pointer[0]=='0';)
   {
      n=strlen(pointer);
		if (n==1) break;
      for (i=0; i<n; i++) pointer[i]=pointer[i+1];
   };
	return pointer;
};

char* HugeNumber::ToHexStr(char *pointer, int size)
// Preobrazyet chislo mnogokratnoj tochnosti v stroky
// pointer - byfer dlia stroki
// size    - razmer byfera
// returns: pointer esli preobrazovanie zaversheno normalno
//          NULL    esli byfer okazalsia mal
{
	int i,razr;
	char buf[5];
	char format[111];
	long l;

	l=1;
	for (razr=0; l<base; razr++) l=l*0x10;
	// razr == chisly simvolov, neobxodimix dlia predstavlenija odnogo razriada
	if (size < this->Razr()*razr+1) return NULL; // Malo pamiati.

   strcpy(buf,"%");
	sprintf(format,"%s%02iX",buf,razr);
	pointer[0]=0;
	for (i=this->Razr()-1; i>=0; i--)
	{
		sprintf(buf,format,this->digit[i]);
		strcat(pointer,buf);
	};

	return pointer;
};

HugeNumber operator - ( HugeNumber &u, HugeNumber &v)
// Bichitanie neotrisatelnix selix chisel w=u-v
// Vnimanie! u >= v !!!
//
{
	int j;
	int k;
	HugeNumber w;
	word a,b;

	w.Resize(u.Razr()); // Razriadnost rezyltata == razriadnosti ymenshaemogo
	k=0;
	for (j=0; j<u.Razr(); j++)
	{
		a=u.digit[j];
		if (j<v.Razr())
			b=v.digit[j];
		else
			b=0;

		w.digit[j]=(a-b+k+base) & (base-1);
		if (int(a+k)<int(b))
			k=-1;
		else
			k=0;
	};
	return w;
};


HugeNumber operator * ( HugeNumber &u, HugeNumber &v)
// u Mnogimoe
// v Mnogitel
// w Rezyltat
{
	word  i,j;
	word  M,N;		// Deistvitelnaia razriadnost chisel
	dword t;			// Rezultat ymnogenia 2-x chifr
	dword k;			// Cifra perenosa
	dword a,c;		// Cifri peremnogaemix chisel
	HugeNumber w;	// Promegytochnij rezyltat

	M=u.Razr();
	N=v.Razr();

	if (M==1)
	{
		w=v;	// ??? A nygen li etot operator ???
		Mult1(v,u.digit[0],w);
		return w;
	};

	if (N==1)
	{
		w=u;
		Mult1(u,v.digit[0],w);
		return w;
	};

	w.Resize(M+N+1);
	N++; M++;

	// Dlia provedenija vichislenij nado imet odin razriad v zapase
	v.Resize(N);
	u.Resize(M);

	k=0;
	for (j=0; j<N; j++)
	{
		 for (i=0; i<M; i++)
		 {
				// Step M4. Ymnogit i slogit
				a=u.digit[i];
				c=v.digit[j];
				t=a*c+((unsigned long)w.digit[i+j])+k;
				w.digit[i+j]=word(t & (base-1));
				// Zdes nado bspolsivat sdvig
				k = t / base;	// Cifra perenosa
				// Step M5. Cikl po i
		 };
		// Step M6. Cikl po j
	};

	return w;
};

HugeNumber operator / ( HugeNumber &u, HugeNumber &v)
{
	HugeNumber a,r;
	Div(u,v,a,r);
	return a;
};

HugeNumber operator % ( HugeNumber &u, HugeNumber &v)
// Ostatok ot delenija
{
	HugeNumber a,r;
	Div(u,v,a,r);
	return r;
};

static void Add1(HugeNumber &a, word m)
// Slogenie smeshannoj tochnosti po osnovanijy 'base'
// a=a+m
{
	dword k;	// Cifra perenosa
	int n;	// Chislo razriadov v 'a'
	int i;

	n=a.Razr();
	k=a.digit[0]+m;
	a.digit[0]=word(k % base);
	k=k / base;
	if (k==0) return;

	for (i=1; i<n; i++)
	{
		k=a.digit[i]+k;
		a.digit[i]=word(k % base);
		k=k / base;
		if (k==0) return;
	};
	if (k==0) return;
	if (i==a.Razr()) a.Resize(i+55);
	a.digit[i]=word(k);
};

static void Convert(char *b_string, HugeNumber &w)
// Prweobrazovanie iz stroki v chislo
{
	word n,m;
	word i;
	char *b;
	HugeNumber tmp;

	// Ydalit razdeliteli
	n=strlen(b_string);
	b=new char[n+1];	// Videlit pamiat dlia stroki po chisly simvolov + 1 byte for 
							// priznaka konza stroki
	// !!! Predidyshij operator bil zapisan s oshibkoj i vigladel tak: b=new char[n];
	// Odnako v Borland vsio rabotalo i oshibka vsplila tolko v VC++
	strcpy(b,b_string);

	for (;;)
	{
		for (i=0; i<n; i++) if (b[i]=='`') break;
		if (i==n) break;
		for (   ; i<n; i++) b[i]=b[i+1];
		n--;
	};

	m=strlen(b);
	w.Resize(m+55);
	n=w.Razr();

	for (i=0; i<n; i++) w.digit[i]=0;
	for (i=0; i<m; i++)
	{
		Mult1(w,10,tmp);
		Add1(tmp,b[i]-'0');
		w=tmp;
	};
	delete b;
};
