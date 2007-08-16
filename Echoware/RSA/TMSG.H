#ifndef __TMsg
#define __TMsg
// Class for encoding/decoding operations. Any message for 
// coding should be represented as an object of this class.
class TMsg
{
	public:
		unsigned int   size;
		unsigned char *value;

		TMsg();
		~TMsg();
		void UCharToMsg(const unsigned char *source, const unsigned int slen);
		void AddBytes(unsigned char *bytes, unsigned int len);
		void FreeMem();
		void Resize(const unsigned int NewSize);
};
#endif
