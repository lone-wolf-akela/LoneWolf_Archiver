#pragma once
#include "stdafx.h"

#include "memmapfilestream.h"

enum CipherStreamState
{
	Read_EncryptionUnknown, Read_Encrypted, Read_NonEncrypted, Write_Encrypted, Write_NonEncrypted
};

class CipherStream : public FileStream
{
public:
	CipherStream(void) = default;
	CipherStream(boost::filesystem::path file, CipherStreamState state)
	{
		open(file, state);
	}
	void open(boost::filesystem::path file, CipherStreamState state);
	void close(void);

	size_t read(void *dst, size_t length) override;
	std::unique_ptr<readDataProxy> readProxy(size_t length) override;
	void write(const void *src, size_t length) override;

	void thread_read(size_t pos, void *dst, size_t length) override;
	std::unique_ptr<readDataProxy> thread_readProxy(size_t pos, size_t length) override;

	void setpos(size_t pos) override;
	size_t getpos(void) override;
	void movepos(signed_size_t diff) override;
	
	void writeKey(void);
	void writeEncryptionEnd(void);

	CipherStreamState getState(void) const;
private:
	void _cipherInit(void);	
	void _cipher_magic(void);

	CipherStreamState _state;
	MemMapFileStream _memmapStream;
	boost::filesystem::fstream _filestream;

	uint32_t _cipherBegBackPos;
	uint32_t _cipherBegPos;
	uint32_t _deadbe7a;
	std::unique_ptr<uint32_t[]> _cipherKey;
	std::unique_ptr<uint32_t[]> _fileKey;
	uint16_t _keySize;
};
