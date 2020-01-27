#include <random>
#include <array>

#include "../helper/helper.h"

#include "filestream.h"
#include "cipher.h"


namespace stream
{
	void FileStream::open(const std::filesystem::path& file,
		State state,
		uint_fast32_t encryption_key_seed)
	{
		assert(encryption_key_seed == 0 || state == State::Write_Encrypted);

		// check if it is encrypted
		if(State::Read_EncryptionUnknown == state)
		{
			std::fstream t_fstream(file, std::ios::binary | std::ios::in);
			if (!t_fstream)
			{
				throw FileIoError("Error happened when openning file: " +
					file.string() + " to read.");
			}
			constexpr std::array _ARCHIVE = { '_','A','R','C','H','I','V','E' };
			std::array<char, 8> buffer{};
			t_fstream.read(buffer.data(), buffer.size());
			if (_ARCHIVE != buffer)
			{
				state = State::Read_Encrypted;
			}
			else
			{
				state = State::Read_NonEncrypted;
			}
		}

		_state_is_encrypted = state == State::Read_Encrypted || state == State::Write_Encrypted;
		_state_is_read = state == State::Read_Encrypted || state == State::Read_NonEncrypted;

		if(_state_is_read)
		{
			// only get file size when reading
			_filesize = file_size(file);
			
			_filestream.open(file, std::ios::binary | std::ios::in);
			if (!_filestream)
			{
				throw FileIoError("Error happened when openning file: " +
					file.string() + " to read.");
			}

			if(_state_is_encrypted)
			{
				_cipherInit();
			}
		}
		else
		{
			_filestream.open(file, std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);
			if (!_filestream)
			{
				throw FileIoError("Error happened when openning file: " +
					file.string() + " to write.");
			}
			
			if(_state_is_encrypted)
			{
				std::random_device rd;
				std::mt19937 mt(encryption_key_seed == 0 ? rd() : encryption_key_seed);
				const std::uniform_int_distribution<uint16_t> dis;

				_keySize = 256;
				_cipherKey.resize(_keySize);
				_fileKey.resize(_keySize);

				_fileKey[0] = 0x15;
				_fileKey[1] = 0x80;
				_fileKey[2] = 0xD6;
				_fileKey[3] = 0xA0;

				std::generate(_fileKey.begin() + 4, _fileKey.end(),
					[&mt, &dis] {return uint8_t(dis(mt)); });

				//do not do magic now. wait until we got _cipherBegPos
				//_cipher_magic();
			}
		}
		
	}

	void FileStream::close()
	{
		_filestream.close();
	}

	size_t FileStream::read(void* dst, size_t length)
	{
		if(_state_is_encrypted)
		{
			const size_t begpos = getpos();

			_filestream.read(static_cast<char*>(dst), length);
			const auto lengthRead = size_t(_filestream.gcount());
			if (_filestream.eof())
			{
				_filestream.clear();
			}
			_filestream.seekp(_filestream.tellg());
			if (!_filestream.good())
			{
				throw FileIoError("Filestream failed.");
			}
			for (size_t i = 0; i < lengthRead; ++i)
			{
				static_cast<uint8_t*>(dst)[i] += _cipherKey[(begpos + i) % _keySize];
			}
			return lengthRead;
		}
		else
		{
			_filestream.read(static_cast<char*>(dst), length);
			const auto lengthRead = size_t(_filestream.gcount());
			if (_filestream.eof())
			{
				_filestream.clear();
			}
			_filestream.seekp(_filestream.tellg());
			if (!_filestream.good())
			{
				throw FileIoError("Filestream failed.");
			}
			return lengthRead;
		}
	}

	void FileStream::write(const void* src, size_t length)
	{
		assert(!_state_is_read);
		if(_state_is_encrypted)
		{
			auto* uint8Src = static_cast<const uint8_t*>(src);
			const size_t begpos = getpos();

			std::vector<uint8_t> buffer(length);
			for (size_t i = 0; i < length; ++i)
			{
				buffer[i] = uint8Src[i] - _cipherKey[(begpos + i) % _keySize];
			}

			_filestream.write(pointer_cast<const char, uint8_t>(buffer.data()), length);
			_filestream.seekg(_filestream.tellp());
			if (!_filestream.good())
			{
				throw FileIoError("Filestream failed.");
			}
		}
		else
		{
			_filestream.write(static_cast<const char*>(src), length);
			_filestream.seekg(_filestream.tellp());
			if (!_filestream.good())
			{
				throw FileIoError("Filestream failed.");
			}
		}
	}

	size_t FileStream::peek(size_t pos, void* dst, size_t length)
	{
		assert(_state_is_read);
		const auto old_pos = _filestream.tellg();
		_filestream.seekg(pos);
		const auto l = read(dst, length);
		_filestream.seekg(old_pos);
		return l;
	}

	void FileStream::setpos(size_t pos)
	{
		_filestream.seekp(pos);
		_filestream.seekg(pos);
		if (!_filestream.good())
		{
			throw FileIoError("Filestream failed.");
		}
	}

	size_t FileStream::getpos()
	{
		return size_t(_filestream.tellg());
	}

	void FileStream::movepos(ptrdiff_t diff)
	{
		setpos(getpos() + diff);
	}

	void FileStream::_cipherInit()
	{
		setpos(_filesize - sizeof(_cipherBegBackPos));
		read(&_cipherBegBackPos, sizeof(_cipherBegBackPos));

		_cipherBegPos = chkcast<uint32_t>(_filesize - _cipherBegBackPos);

		setpos(_cipherBegPos);
		read(&_deadbe7a, sizeof(_deadbe7a));
		read(&_keySize, sizeof(_keySize));
		_cipherKey.resize(_keySize);
		_fileKey.resize(_keySize);
		read(_fileKey.data(), _keySize);
		if (_fileKey[0] == 0x15 &&
			_fileKey[1] == 0x80 &&
			_fileKey[2] == 0xD6 &&
			_fileKey[3] == 0xA0)
		{
			throw FatalError("Fatal error.");
		}
		_cipher_magic();
		setpos(0);
	}

	void FileStream::writeKey()
	{
		_deadbe7a = 0xdeadbe7a;

		_cipherBegPos = chkcast<uint32_t>(getpos());

		_filestream.write(pointer_cast<const char, uint32_t>(&_deadbe7a), sizeof(_deadbe7a));
		_filestream.write(pointer_cast<const char, uint16_t>(&_keySize), sizeof(_keySize));
		_filestream.write(pointer_cast<const char, uint8_t>(_fileKey.data()), _keySize);
		_filestream.seekg(_filestream.tellp());
		if (!_filestream.good())
		{
			throw FileIoError("Filestream failed.");
		}
		//re-magic with new _cipherBegPos!
		_cipher_magic();
	}

	void FileStream::writeEncryptionEnd()
	{
		_filestream.seekp(0, std::ios::end);
		const uintmax_t filesize = _filestream.tellp();

		_cipherBegBackPos = chkcast<uint32_t>(filesize + sizeof(_cipherBegBackPos) - _cipherBegPos);

		_filestream.write(pointer_cast<const char, uint32_t>(&_cipherBegBackPos), sizeof(_cipherBegBackPos));

		_filestream.seekg(_filestream.tellp());
		if (!_filestream.good())
		{
			throw FileIoError("Filestream failed.");
		}
	}
	template <std::integral Tv, std::integral Tb>
	static Tv ROTL(Tv val, Tb bits)
	{
		return (val << bits) | (val >> (32 - bits));
	}
	void FileStream::_cipher_magic()
	{
		for (decltype(_keySize) i = 0; i < _keySize; i += 4)
		{
			auto currentKey = *pointer_cast<uint32_t, uint8_t>(&_fileKey[i]);
			for (size_t byte = 0; byte < 4; byte++)
			{
				uint32_t tempVal = ROTL(currentKey + _cipherBegPos, 8);
				const auto tempBytes = pointer_cast<uint8_t, uint32_t>(&tempVal);
				for (uint32_t j = 0; j < 4; j++)
				{
					currentKey = cipherConst[uint8_t(currentKey ^ tempBytes[j])] ^ (currentKey >> 8);
				}
				_cipherKey[byte + i] = uint8_t(currentKey);
			}
		}
	}
}
