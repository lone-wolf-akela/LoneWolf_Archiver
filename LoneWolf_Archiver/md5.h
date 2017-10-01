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
类MD5封装了与MD5相关的操作,其有4个构造函数:
MD5();                                                      //默认构造函数
MD5(const void *input, size_t length);                      //输入内存地址与长度信息的构造函数
MD5(const string &str);                                     //输入字符串的构造函数
MD5(ifstream &in);                                          //输入流的构造函数
3个Update函数:
void update(const void *input, size_t length);		        //往MD5对象内添加内存块
void update(const string &str);                             //添加字符串
void update(ifstream &in);                                  //添加流

const byte* digest();                                       //计算MD5码,并返回指向它的指针
string toString();                                          //计算MD5,并返回其对应的字符串
void reset();                                               //重置
*/