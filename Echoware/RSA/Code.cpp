#include "stdafx.h"
//#include <mem.h>
#include <string.h>

#include "HNumber.h"
#include "GSimply.h"
#include "Operator.h"
#include "HNFunct.h"

int Encode(unsigned char *msg, int msgLen, HugeNumber &FirstHalf, HugeNumber &SecondHalf)
// function returns:
// 1 - OK
// 0 - failure
{
   int i,n,k,j;
   //char buf[11];
   HugeNumber x,y;
   unsigned long CheckSum;

   // Count check sum
   CheckSum=msgLen;
   for (i=0; i<msgLen; i++) CheckSum+=msg[i];

   k=0;
   n=msgLen*2;
   for (i=0; i<n; i++)
   {
      j=msg[i/2];
      if (i%2==0)
         x.digit[k]=(x.digit[k]<<4)+(j>>4);
      else
         x.digit[k]=(x.digit[k]<<4)+(j&0xF);
      if (i%3==2) k++;
   };
   if (i%3==1) x.digit[k]=x.digit[k]<<8;
   if (i%3==2) x.digit[k]=x.digit[k]<<4;
   if (i%3!=0) k++;
   x.digit[k++]=msgLen/base;
   x.digit[k++]=msgLen%base;
   x.digit[k++]=CheckSum%base;

   if (SecondHalf<=x) return 0;   // Message is too long for encoding
   
   ModPower(y,x,FirstHalf,SecondHalf);
   y.ToStr((char*)msg,2222);
   return 1;
};

int Decode(unsigned char *msg, int &msgLen, HugeNumber &FirstHalf, HugeNumber &SecondHalf)
// function returns:
// 1 - OK
// 0 - failure
{
   int i,n,k,j;
   //char buf[11];
   HugeNumber x,y;
   unsigned long CheckSum;
   unsigned long testCheckSum;

   y=(char*)msg;
   if (SecondHalf<=y) return 0;   // Message is too long for decoding

   ModPower(x,y,FirstHalf,SecondHalf);

   j=k=0;
   memset(msg,0,2222);
   n=x.Razr();
   // Restore message's length in bytes
   msgLen=x.digit[n-3];
   msgLen=msgLen*base+x.digit[n-2];
   // Restore message's chech sum
   testCheckSum=x.digit[n-1];

   n=msgLen*2;
   for (i=0; i<n; i++)
   {
      if (i%3==0)
         j=(j<<4)+((x.digit[k]>>8)&0xF);
      else if (i%3==1)
         j=(j<<4)+((x.digit[k]>>4)&0xF);
      else
      {
         j=(j<<4)+(x.digit[k]&0xF);
         k++;
      };

      if (i%2==1)
      {
         msg[i/2]=j;
         j=0;
      };
   };

   // Count check sum
   CheckSum=msgLen;
   for (i=0; i<msgLen; i++) CheckSum+=msg[i];

   if (testCheckSum!=CheckSum%base) return 0;
   return 1;
};


