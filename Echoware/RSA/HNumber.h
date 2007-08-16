#ifndef __HugeNumber_H
#define __HugeNumber_H

#include <stdlib.h>
#include "MyTypes.h"

class HugeNumber
{
	public:
		word digit[MaxRazr];				// Znachenija razriadov
		HugeNumber();						
		HugeNumber(char *);				
		HugeNumber(unsigned long);	
		HugeNumber(const HugeNumber&);		
		HugeNumber(HugeNumber&);		

		void Random(int k);				// Generiryet slychiajnoe chislo iz k razriadov
		int Razr() const;					// Opredeliaet razriadnost chisla
		void Resize(int k); 				// Yvelichivaet chislo razriadov do k
		char *ToStr(char*,int);			// Preobrazovivaet chislo v tekstovij vid
		char *ToHexStr(char *,int);

		HugeNumber& operator = (const HugeNumber &);
		friend HugeNumber operator + ( HugeNumber &, HugeNumber &);
		friend HugeNumber operator - ( HugeNumber &u, HugeNumber &v);
		friend HugeNumber operator * ( HugeNumber &u, HugeNumber &v);
		friend HugeNumber operator / ( HugeNumber &u, HugeNumber &v);
		friend HugeNumber operator % ( HugeNumber &u, HugeNumber &v);
		HugeNumber& operator++();
		HugeNumber& operator--();
};

HugeNumber operator + ( HugeNumber &, HugeNumber &);
HugeNumber operator - ( HugeNumber &u, HugeNumber &v);
HugeNumber operator * ( HugeNumber &u, HugeNumber &v);
HugeNumber operator / ( HugeNumber &u, HugeNumber &v);
HugeNumber operator % ( HugeNumber &u, HugeNumber &v);
#endif
