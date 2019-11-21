﻿#pragma once
#include <cstdint>
#include <fstream>
#include <vector>

#include "memmapfilestream.h"

namespace stream
{
	enum class CipherStreamState
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
		CipherStream() = default;
		CipherStream(const std::filesystem::path& file,
			CipherStreamState state,
			uint_fast32_t encryption_key_seed = 0)
		{
			open(file, state, encryption_key_seed);
		}
		void open(const std::filesystem::path& file,
			CipherStreamState state,
			uint_fast32_t encryption_key_seed = 0);
		void close();

		size_t read(void* dst, size_t length) override;
		std::tuple<OptionalOwnerBuffer, size_t>
			optionalOwnerRead(size_t length) override;
		void write(const void* src, size_t length) override;

		//these two do not move the read ptr
		size_t read(size_t pos, void* dst, size_t length) override;
		std::tuple<OptionalOwnerBuffer, size_t>
			optionalOwnerRead(size_t pos, size_t length) override;

		void setpos(size_t pos) override;
		size_t getpos() override;
		void movepos(ptrdiff_t diff) override;

		void writeKey();
		void writeEncryptionEnd();

		[[nodiscard]] CipherStreamState getState() const;
	private:
		void _cipherInit();
		void _cipher_magic();

		CipherStreamState _state = CipherStreamState::Read_EncryptionUnknown;
		MemMapFileStream _memmapStream = {};
		std::fstream _filestream = {};

		uint32_t _cipherBegBackPos = 0;
		uint32_t _cipherBegPos = 0;
		uint32_t _deadbe7a = 0;
		std::vector<uint8_t> _cipherKey = {};
		std::vector<uint8_t> _fileKey = {};
		uint16_t _keySize = 0;
	};
}