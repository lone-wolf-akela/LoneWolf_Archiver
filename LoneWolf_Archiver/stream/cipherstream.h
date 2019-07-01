#pragma once
#include <cstdint>
#include <fstream>

#include "memmapfilestream.h"

namespace stream
{
	enum CipherStreamState
	{
		Read_EncryptionUnknown,
		Read_Encrypted,
		Read_NonEncrypted,
		Write_Encrypted,
		Write_NonEncrypted
	};

	class CipherStream : public FileStream
	{
	public:
		CipherStream(void) = default;
		CipherStream(const std::filesystem::path& file, CipherStreamState state)
		{
			open(file, state);
		}
		void open(const std::filesystem::path& file, CipherStreamState state);
		void close(void);

		size_t read(void* dst, size_t length) override;
		std::tuple<OptionalOwnerBuffer, size_t>
			optionalOwnerRead(size_t length) override;
		void write(const void* src, size_t length) override;

		//these two do not move the read ptr
		size_t read(size_t pos, void* dst, size_t length) override;
		std::tuple<OptionalOwnerBuffer, size_t>
			optionalOwnerRead(size_t pos, size_t length) override;

		void setpos(size_t pos) override;
		size_t getpos(void) override;
		void movepos(ptrdiff_t diff) override;

		void writeKey(void);
		void writeEncryptionEnd(void);

		CipherStreamState getState(void) const;
	private:
		void _cipherInit(void);
		void _cipher_magic(void);

		CipherStreamState _state = Read_EncryptionUnknown;
		MemMapFileStream _memmapStream;
		std::fstream _filestream;

		uint32_t _cipherBegBackPos = 0;
		uint32_t _cipherBegPos = 0;
		uint32_t _deadbe7a = 0;
		std::unique_ptr<uint32_t[]> _cipherKey;
		std::unique_ptr<uint32_t[]> _fileKey;
		uint16_t _keySize = 0;
	};
}
