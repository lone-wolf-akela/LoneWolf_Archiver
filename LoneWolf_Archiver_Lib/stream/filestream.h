#if !defined(LONEWOLF_ARCHIVER_LIB_STREAM_FILESTREAM_H)
#define LONEWOLF_ARCHIVER_LIB_STREAM_FILESTREAM_H

#include <cstddef>

#include <vector>
#include <filesystem>
#include <fstream>

#include "../exceptions/exceptions.h"

namespace stream
{
	enum class State
	{
		Read_EncryptionUnknown,
		Read_Encrypted,
		Read_NonEncrypted,
		Write_Encrypted,
		Write_NonEncrypted
	};
	
	class FileStream
	{
	public:
		FileStream() = default;
		FileStream(const FileStream&) = delete;
		FileStream& operator=(const FileStream&) = delete;
		FileStream(FileStream&&) = delete;
		FileStream& operator=(FileStream&&) = delete;
		virtual ~FileStream() = default;

		FileStream(const std::filesystem::path& file,
			State state,
			uint_fast32_t encryption_key_seed = 0)
		{
			open(file, state, encryption_key_seed);
		}
		void open(const std::filesystem::path& file,
			State state,
			uint_fast32_t encryption_key_seed = 0);
		void close();
		
		size_t read(void* dst, size_t length);
		void write(const void* src, size_t length);

		//this do not move the read ptr
		size_t peek(size_t pos, void* dst, size_t length);

		void setpos(size_t pos);
		[[nodiscard]] size_t getpos();
		void movepos(ptrdiff_t diff);

		void writeKey();
		void writeEncryptionEnd();

	private:
		void _cipherInit();
		void _cipher_magic();

		bool _state_is_read = false;
		bool _state_is_encrypted = false;
		
		std::fstream _filestream = {};
		std::uintmax_t _filesize = 0;
		
		uint32_t _cipherBegBackPos = 0;
		uint32_t _cipherBegPos = 0;
		uint32_t _deadbe7a = 0;
		std::vector<uint8_t> _cipherKey = {};
		std::vector<uint8_t> _fileKey = {};
		uint16_t _keySize = 0;
	};
}

#endif
