// Key_main.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
//#include <conio.h>
#include <stdlib.h>
#include <stdio.h>
//#include <conio.h>

#include "HNumber.h"
#include "GSimply.h"

extern void Keys(HugeNumber&,HugeNumber&,HugeNumber&);

int main(int argc, char* argv[])
{
   HugeNumber e,d,n;

   // Generate keys
   Keys(e,d,n);

   SaveKeys("EncodeKey.txt",d,n);	
	// Now "EncodeKey.txt" contains a key for encoding

   SaveKeys("DecodeKey.txt",e,n);
	// Now "DecodeKey.txt" contains a key for decoding
   
	printf("\nPress any key...");
//   getch();
   return 0;
	// File "Prot.txt" contains some debug information. 
	// We don't need this file for work.
}
