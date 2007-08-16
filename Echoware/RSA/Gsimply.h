#ifndef __GSymply
#define __GSymply

extern int PrimeQ(HugeNumber &km1, HugeNumber &k);
extern void GeneratePrimeHugeNumber(HugeNumber &x);
extern int SaveKeys(char *FileName, HugeNumber &x, HugeNumber &y);
extern int RestoreKey(char *FileName, HugeNumber &x, HugeNumber &y);

#endif

