#ifndef MD5_H
#define MD5_H

#include <string>
#include <boost/filesystem/fstream.hpp>

/* Type define */
typedef unsigned char byte;
typedef unsigned int uint32;

/* MD5 declaration. */
class MD5 
{
public:
	MD5();
	MD5(const void *input, size_t length);
	explicit MD5(const std::string &str);
	explicit MD5(boost::filesystem::ifstream &in);
	void update(const void *input, size_t length);
	void update(const std::string &str);
	void update(boost::filesystem::ifstream &in);
	const byte* digest();
	std::string toString();
	void reset();
private:
	void update(const byte *input, size_t length);
	void final();
	void transform(const byte block[64]);
	static void encode(const uint32 *input, byte *output, size_t length);
	static void decode(const byte *input, uint32 *output, size_t length);
	static std::string bytesToHexString(const byte *input, size_t length);

	/* class uncopyable */									
	MD5(const MD5&) = delete;
	MD5& operator=(const MD5&) = delete;
private:
	uint32 _state[4];	/* state (ABCD) */
	uint32 _count[2];	/* number of bits, modulo 2^64 (low-order word first) */
	byte _buffer[64];	/* input buffer */
	byte _digest[16];	/* message digest */
	bool _finished;		/* calculate finished ? */

	static const byte PADDING[64];	/* padding for calculate */
	static const char HEX[16];
	static const size_t BUFFER_SIZE = 1024;
};

#endif/*MD5_H*/


/*
��MD5��װ����MD5��صĲ���,����4�����캯��:
MD5();                                                      //Ĭ�Ϲ��캯��
MD5(const void *input, size_t length);                      //�����ڴ��ַ�볤����Ϣ�Ĺ��캯��
MD5(const string &str);                                     //�����ַ����Ĺ��캯��
MD5(ifstream &in);                                          //�������Ĺ��캯��
3��Update����:
void update(const void *input, size_t length);		        //��MD5����������ڴ��
void update(const string &str);                             //����ַ���
void update(ifstream &in);                                  //�����

const byte* digest();                                       //����MD5��,������ָ������ָ��
string toString();                                          //����MD5,���������Ӧ���ַ���
void reset();                                               //����
*/