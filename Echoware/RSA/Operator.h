#ifndef __Operator_H
#define __Operator_H

#include "HNumber.h"

extern int operator !=( const HugeNumber & test1, const HugeNumber & test2 );
extern int operator == ( const HugeNumber & test1, const HugeNumber & test2 );
extern int operator < ( const HugeNumber & test1, const HugeNumber & test2 );
extern int operator > ( const HugeNumber & test1, const HugeNumber & test2 );
extern int operator <=( const HugeNumber & test1, const HugeNumber & test2 );

#endif