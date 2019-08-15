#include <random>
#include <concepts>

#include "../helper/helper.h"

#include "cipherstream.h"
#include "cipher.h"


namespace stream
{
	void CipherStream::open(const std::filesystem::path& file, CipherStreamState state)
	{
		_state = state;
		switch (state)
		{
		case CipherStreamState::Read_EncryptionUnknown:
		{
			_memmapStream.open(file);
			if (0 != strncmp(reinterpret_cast<const char*>(_memmapStream.getReadptr()),
				"_ARCHIVE", 8))
			{
				_state = CipherStreamState::Read_Encrypted;
				_cipherInit();
			}
			else
			{
				_state = CipherStreamState::Read_NonEncrypted;
			}
			_memmapStream.setpos(0);
		}
		break;
		case CipherStreamState::Read_Encrypted:
		{
			_memmapStream.open(file);
			_cipherInit();
		}
		break;
		case CipherStreamState::Read_NonEncrypted:
		{
			_memmapStream.open(file);
		}
		break;
		case CipherStreamState::Write_Encrypted:
		{
			_filestream.open(file, std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);
			if (!_filestream)
			{
				throw FileIoError("Error happened when openning file: " +
					file.string() + " to write.");
			}

			std::random_device rd;
			std::mt19937 mt(rd());
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
		break;
		default: // Write_NonEncrypted:
		{
			_filestream.open(file, std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);
			if (!_filestream)
			{
				throw FileIoError("Error happened when openning file: " +
					file.string() + " to write.");
			}
		}
		break;
		}
	}

	void CipherStream::close()
	{
		_memmapStream.close();
		_filestream.close();
	}

	size_t CipherStream::read(void* dst, size_t length)
	{
		switch (_state)
		{
		case CipherStreamState::Read_Encrypted:
		{
			const size_t l = read(_memmapStream.getpos(), dst, length);
			_memmapStream.movepos(l);
			return l;
		}
		case CipherStreamState::Read_NonEncrypted:
		{
			return _memmapStream.read(dst, length);
		}
		case CipherStreamState::Write_NonEncrypted:
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
		case CipherStreamState::Write_Encrypted:
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
		default: // Read_EncryptionUnknown:
			throw OutOfRangeError();
		}
	}

	std::tuple<OptionalOwnerBuffer, size_t>
		CipherStream::optionalOwnerRead(size_t length)
	{
		switch (_state)
		{
		case CipherStreamState::Read_Encrypted:
		{
			auto r = optionalOwnerRead(_memmapStream.getpos(), length);
			_memmapStream.movepos(std::get<1>(r));
			return r;
		}
		case CipherStreamState::Read_NonEncrypted:
		{
			return _memmapStream.optionalOwnerRead(length);
		}
		default: // Read_EncryptionUnknown:
		// Write_Encrypted:
		// Write_NonEncrypted:
			throw OutOfRangeError();
		}
	}

	void CipherStream::write(const void* src, size_t length)
	{
		switch (_state)
		{
		case CipherStreamState::Write_Encrypted:
		{
			auto* uint8Src = reinterpret_cast<const uint8_t*>(src);
			const size_t begpos = getpos();

			const std::unique_ptr<uint8_t[]> buffer(new uint8_t[length]);
			for (size_t i = 0; i < length; ++i)
			{
				buffer[i] = uint8Src[i] - _cipherKey[(begpos + i) % _keySize];
			}

			_filestream.write(reinterpret_cast<const char*>(buffer.get()), length);
			_filestream.seekg(_filestream.tellp());
			if (!_filestream.good())
			{
				throw FileIoError("Filestream failed.");
			}
		}
		break;
		case CipherStreamState::Write_NonEncrypted:
		{
			_filestream.write(static_cast<const char*>(src), length);
			_filestream.seekg(_filestream.tellp());
			if (!_filestream.good())
			{
				throw FileIoError("Filestream failed.");
			}
		}
		break;
		default: // Read_EncryptionUnknown:
		// Read_NonEncrypted:
		// Read_Encrypted:
			throw OutOfRangeError();
		}
	}

	size_t CipherStream::read(size_t pos, void* dst, size_t length)
	{
		switch (_state)
		{
		case CipherStreamState::Read_Encrypted:
		{
			const auto uint8Dst = static_cast<uint8_t*>(dst);
			const size_t l = _memmapStream.read(pos, dst, length);
			for (size_t i = 0; i < l; ++i)
			{
				uint8Dst[i] += _cipherKey[(pos + i) % _keySize];
			}
			return l;
		}
		case CipherStreamState::Read_NonEncrypted:
		{
			return _memmapStream.read(pos, dst, length);
		}
		default: // Read_EncryptionUnknown:
		// Write_Encrypted:
		// Write_NonEncrypted:
			throw OutOfRangeError();
		}
	}

	std::tuple<OptionalOwnerBuffer, size_t>
		CipherStream::optionalOwnerRead(size_t pos, size_t length)
	{
		switch (_state)
		{
		case CipherStreamState::Read_Encrypted:
		{
			std::vector<std::byte> buffer(length);
			size_t l = read(pos, buffer.data(), length);
			buffer.resize(l);
			return { OptionalOwnerBuffer(std::move(buffer)), l };
		}
		case CipherStreamState::Read_NonEncrypted:
		{
			return _memmapStream.optionalOwnerRead(pos, length);
		}
		default: // Read_EncryptionUnknown:
		// Write_Encrypted:
		// Write_NonEncrypted:
			throw OutOfRangeError();
		}
	}

	void CipherStream::setpos(size_t pos)
	{
		switch (_state)
		{
		case CipherStreamState::Read_Encrypted:
		case CipherStreamState::Read_NonEncrypted:
		{
			_memmapStream.setpos(pos);
		}
		break;
		case CipherStreamState::Write_Encrypted:
		case CipherStreamState::Write_NonEncrypted:
		{
			_filestream.seekp(pos);
			_filestream.seekg(pos);
			if (!_filestream.good())
			{
				throw FileIoError("Filestream failed.");
			}
		}
		break;
		default: // Read_EncryptionUnknown:
			throw OutOfRangeError();
		}
	}

	size_t CipherStream::getpos()
	{
		switch (_state)
		{
		case CipherStreamState::Read_Encrypted:
		case CipherStreamState::Read_NonEncrypted:
		{
			return _memmapStream.getpos();
		}
		case CipherStreamState::Write_Encrypted:
		case CipherStreamState::Write_NonEncrypted:
		{
			return size_t(_filestream.tellg());
		}
		default: // Read_EncryptionUnknown:
			throw OutOfRangeError();
		}
	}

	void CipherStream::movepos(ptrdiff_t diff)
	{
		switch (_state)
		{
		case CipherStreamState::Read_Encrypted:
		case CipherStreamState::Read_NonEncrypted:
			_memmapStream.movepos(diff);
			break;

		case CipherStreamState::Write_Encrypted:
		case CipherStreamState::Write_NonEncrypted:
			setpos(getpos() + diff);
			break;

		default: // Read_EncryptionUnknown:
			throw OutOfRangeError();
		}
	}

	CipherStreamState CipherStream::getState() const
	{
		return _state;
	}

	void CipherStream::_cipherInit()
	{
		_memmapStream.setpos(size_t(_memmapStream.getFileSize() - sizeof(_cipherBegBackPos)));
		_memmapStream.read(&_cipherBegBackPos, sizeof(_cipherBegBackPos));

		_cipherBegPos = chkcast<uint32_t>(_memmapStream.getFileSize() - _cipherBegBackPos);

		_memmapStream.setpos(_cipherBegPos);
		_memmapStream.read(&_deadbe7a, sizeof(_deadbe7a));
		_memmapStream.read(&_keySize, sizeof(_keySize));
		_cipherKey.resize(_keySize);
		_fileKey.resize(_keySize);
		_memmapStream.read(_fileKey.data(), _keySize);
		if (_fileKey[0] == 0x15 &&
			_fileKey[1] == 0x80 &&
			_fileKey[2] == 0xD6 &&
			_fileKey[3] == 0xA0)
		{
			throw FatalError("Fatal error.");
		}
		_cipher_magic();
		_memmapStream.setpos(0);
	}

	void CipherStream::writeKey()
	{
		_deadbe7a = 0xdeadbe7a;

		_cipherBegPos = chkcast<uint32_t>(getpos());

		_filestream.write(reinterpret_cast<const char*>(&_deadbe7a), sizeof(_deadbe7a));
		_filestream.write(reinterpret_cast<const char*>(&_keySize), sizeof(_keySize));
		_filestream.write(reinterpret_cast<const char*>(_fileKey.data()), _keySize);
		_filestream.seekg(_filestream.tellp());
		if (!_filestream.good())
		{
			throw FileIoError("Filestream failed.");
		}
		//re-magic with new _cipherBegPos!
		_cipher_magic();
	}

	void CipherStream::writeEncryptionEnd()
	{
		_filestream.seekp(0, std::ios::end);
		const uintmax_t filesize = _filestream.tellp();

		_cipherBegBackPos = chkcast<uint32_t>(filesize + sizeof(_cipherBegBackPos) - _cipherBegPos);

		_filestream.write(reinterpret_cast<const char*>(&_cipherBegBackPos), sizeof(_cipherBegBackPos));

		_filestream.seekg(_filestream.tellp());
		if (!_filestream.good())
		{
			throw FileIoError("Filestream failed.");
		}
	}
	template <std::Integral Tv, std::Integral Tb>
		static Tv ROTL(Tv val, Tb bits)
	{
		return (val << bits) | (val >> (32 - bits));
	}
	void CipherStream::_cipher_magic()
	{
		for (decltype(_keySize) i = 0; i < _keySize; i += 4)
		{
			auto currentKey = *reinterpret_cast<uint32_t*>(&_fileKey[i]);
			for (auto byte = 0; byte < 4; byte++)
			{
				uint32_t tempVal = ROTL(currentKey + _cipherBegPos, 8);
				const auto tempBytes = reinterpret_cast<uint8_t*>(&tempVal);
				for (uint32_t j = 0; j < 4; j++)
				{
					currentKey = cipherConst[uint8_t(currentKey ^ tempBytes[j])] ^ (currentKey >> 8);
				}
				_cipherKey[byte + i] = uint8_t(currentKey);
			}
		}
	}
}
