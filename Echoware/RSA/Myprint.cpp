#include "stdafx.h"
#include <stdio.h>
#include "HNumber.h"
#include "MyNum.h"

FILE *Prot=NULL;

void Protocol(char *Msg)
{
   Prot=fopen("Prot.txt","at");
	printf("%s\n",Msg);
	fprintf(Prot,"%s\n",Msg);
	fclose(Prot);
};

void Protocol(const char *Msg, MyNum &a)
{
	char Buf[222];
	HugeNumber hn;

   Prot=fopen("Prot.txt","at");
	hn=a.value;
	hn.ToStr(Buf,222);

	printf("%s ",Msg);
	fprintf(Prot,"%s ",Msg);
	if (a.sign<0)
	{
		printf("-");
		fprintf(Prot,"-");
	};
	printf("%s\n",Buf);
	fprintf(Prot,"%s\n",Buf);
	fclose(Prot);
};

void ProtocolF(const char *Msg, float a)
{
   Prot=fopen("Prot.txt","at");

	printf("%s %g (dec)\n",Msg,a);
	fprintf(Prot,"%s %g (dec)\n",Msg,a);
	fclose(Prot);
};

void Protocol(char *Msg, HugeNumber &a)
{
	char Buf[222];

   Prot=fopen("Prot.txt","at");

	a.ToStr(Buf,222);
	printf("%s %s (hex)\n",Msg,Buf);
	fprintf(Prot,"%s %s (hex)\n",Msg,Buf);
	fclose(Prot);
};

void PrintCode(const unsigned char *Msg, const long MsgLen)
// Vivod zakodirovannogo soovshenija v 'krasivom' vide
{
	const int MaxLen=24;
	int i,j;

	j=0;
	for (i=0; i<int(MsgLen); i++)
	{
		if (j%2 == 0 && j!=0) printf(" ");
		if (j>=MaxLen)
		{
			j=0;
			printf("\n");
		};
		printf("%02X",Msg[i]);
		j++;
	};
	printf("\n");
};

void PrintChar(unsigned char *ch, const long len)
{
	int i;

	for (i=0; i<len; i++) printf("%02X ",ch[i]);
	printf("\n");
};

