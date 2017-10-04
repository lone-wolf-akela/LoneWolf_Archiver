#include "stdafx.h"

#include "cipherstream.h"
#include "cipher.h"

void CipherStream::open(boost::filesystem::path file, CipherStreamState state)
{
	_state = state;
	switch (state)
	{
	case Read_EncryptionUnknown:
	{
		_memmapStream.open(file);
		if (strncmp(_memmapStream.getReadptr(), "_ARCHIVE", 8))
		{
			_state = Read_Encrypted;
			_cipherInit();
		}
		else
		{
			_state = Read_NonEncrypted;
		}
		_memmapStream.setpos(0);
	}
	break;

	case Read_Encrypted:
		_memmapStream.open(file);
		_cipherInit();
		break;

	case Read_NonEncrypted:
		_memmapStream.open(file);
		break;

	case Write_Encrypted:
	{
		_filestream.open(file, std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);
		if (!_filestream.is_open())
		{
			throw FileIoError("Error happened when openning file to write.");
		}

		std::random_device rd;
		std::mt19937 mt(rd());
		std::uniform_int_distribution<uint32_t> dis;

		_keySize = 256;
		_cipherKey = std::unique_ptr<uint32_t[]>(new uint32_t[_keySize / sizeof(uint32_t)]);
		_fileKey = std::unique_ptr<uint32_t[]>(new uint32_t[_keySize / sizeof(uint32_t)]);
		for (size_t i = 0; i < _keySize / sizeof(uint32_t); ++i)
		{
			_fileKey[i] = dis(mt);
		}
		//do not do magic now. wait until we got _cipherBegPos
		//_cipher_magic();
	}
	break;

	case Write_NonEncrypted:
	{
		_filestream.open(file, std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);
		if (!_filestream.is_open())
		{
			throw FileIoError("Error happened when openning file to write.");
		}
	}
	break;

	default:
		throw OutOfRangeError();
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
	case Read_Encrypted:
	{
		uint8_t *tmpDst = reinterpret_cast<uint8_t*>(dst);
		size_t begpos = _memmapStream.getpos();
		size_t lengthRead = _memmapStream.read(dst, length);
		for (size_t i = 0; i < lengthRead; ++i)
		{
			tmpDst[i] +=
				*(reinterpret_cast<uint8_t*>(_cipherKey.get()) + (begpos + i) % _keySize);
		}
		return lengthRead;
	}

	case Read_NonEncrypted:
		return _memmapStream.read(dst, length);

	case Write_NonEncrypted:
	{
		_filestream.read(reinterpret_cast<char*>(dst), length);
		size_t lengthRead = size_t(_filestream.gcount());
		if(_filestream.eof())
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

	case Write_Encrypted:
	{
		uint8_t *uint8Dst = reinterpret_cast<uint8_t*>(dst);
		char *charDst = reinterpret_cast<char*>(dst);

		size_t begpos = getpos();

		_filestream.read(charDst, length);
		size_t lengthRead = size_t(_filestream.gcount());
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
			uint8Dst[i] +=
				*(reinterpret_cast<uint8_t*>(_cipherKey.get()) + (begpos + i) % _keySize);
		}

		return lengthRead;
	}

	case Read_EncryptionUnknown:	
	default:
		throw OutOfRangeError();
	}
}

std::unique_ptr<readDataProxy> CipherStream::readProxy(size_t length)
{
	switch (_state)
	{
	case Read_Encrypted:
	{
		std::unique_ptr<readDataProxy> proxy = std::make_unique<readDataProxy>(true);
		char *tmpData = new char[length];
		read(tmpData, length);
		proxy->data = tmpData;
		return proxy;
	}
	case Read_NonEncrypted:
		return _memmapStream.readProxy(length);

	case Read_EncryptionUnknown:
	case Write_Encrypted:
	case Write_NonEncrypted:
	default:
		throw OutOfRangeError();
	}
}

void CipherStream::write(const void* src, size_t length)
{
	switch (_state)
	{
	case Write_Encrypted:
	{
		const uint8_t *uint8Src = reinterpret_cast<const uint8_t*>(src);
		size_t begpos = getpos();

		std::unique_ptr<uint8_t[]> buffer(new uint8_t[length]);
		for (size_t i = 0; i < length; ++i)
		{
			buffer[i] = *(uint8Src + i) -
				*(reinterpret_cast<uint8_t*>(_cipherKey.get()) + (begpos + i) % _keySize);
		}
		_filestream.write(reinterpret_cast<char*>(buffer.get()), length);
		_filestream.seekg(_filestream.tellp());
		if (!_filestream.good())
		{
			throw FileIoError("Filestream failed.");
		}
	}
	break;

	case Write_NonEncrypted: 
	{
		_filestream.write(reinterpret_cast<const char*>(src), length);
		_filestream.seekg(_filestream.tellp());
		if (!_filestream.good())
		{
			throw FileIoError("Filestream failed.");
		}
	}
	break;

	case Read_EncryptionUnknown:
	case Read_NonEncrypted:
	case Read_Encrypted:
	default: 
		throw OutOfRangeError();
	}
}

void CipherStream::thread_read(size_t pos, void* dst, size_t length)
{
	switch (_state)
	{
	case Read_Encrypted:
	{
		uint8_t *tmpDst = reinterpret_cast<uint8_t*>(dst);
		_memmapStream.thread_read(pos, dst, length);
		for (size_t i = 0; i < length; ++i)
		{
			tmpDst[i] +=
				*(reinterpret_cast<uint8_t*>(_cipherKey.get()) + (pos + i) % _keySize);
		}
		return;
	}

	case Read_NonEncrypted:
		_memmapStream.thread_read(pos, dst, length);
		return;

	case Read_EncryptionUnknown:
	case Write_Encrypted:
	case Write_NonEncrypted:
	default:
		throw OutOfRangeError();
	}
}

std::unique_ptr<readDataProxy> CipherStream::thread_readProxy(size_t pos, size_t length)
{
	switch (_state)
	{
	case Read_Encrypted:
	{
		std::unique_ptr<readDataProxy> proxy = std::make_unique<readDataProxy>(true);
		char *tmpData = new char[length];
		thread_read(pos, tmpData, length);
		proxy->data = tmpData;
		return proxy;
	}
	case Read_NonEncrypted:
		return _memmapStream.thread_readProxy(pos, length);

	case Read_EncryptionUnknown:
	case Write_Encrypted:
	case Write_NonEncrypted:
	default:
		throw OutOfRangeError();
	}
}

void CipherStream::setpos(size_t pos)
{
	switch (_state)
	{
	case Read_Encrypted:
	case Read_NonEncrypted:
		_memmapStream.setpos(pos);
		break;

	case Write_Encrypted:
	case Write_NonEncrypted:
		_filestream.seekp(pos);
		_filestream.seekg(pos);	
		if(!_filestream.good())
		{
			throw FileIoError("Filestream failed.");
		}
		break;

	case Read_EncryptionUnknown:
	default:
		throw OutOfRangeError();
	}
}

size_t CipherStream::getpos()
{
	switch (_state)
	{
	case Read_Encrypted:
	case Read_NonEncrypted:
		return _memmapStream.getpos();

	case Write_Encrypted:
	case Write_NonEncrypted:
		return size_t(_filestream.tellg());

	case Read_EncryptionUnknown:
	default:
		throw OutOfRangeError();
	}
}

void CipherStream::movepos(signed_size_t diff)
{
	switch(_state)
	{
	case Read_Encrypted:
	case Read_NonEncrypted:
		_memmapStream.movepos(diff);
		break;

	case Write_Encrypted: 
	case Write_NonEncrypted:
		setpos(getpos() + diff);
		break;

	case Read_EncryptionUnknown:
	default: 
		throw OutOfRangeError();
	}
}

CipherStreamState CipherStream::getState()
{
	return _state;
}

void CipherStream::_cipherInit()
{
	_memmapStream.setpos(size_t(_memmapStream.getFileSize() - sizeof(_cipherBegBackPos)));
	_memmapStream.read(&_cipherBegBackPos, sizeof(_cipherBegBackPos));
	_cipherBegPos = uint32_t(_memmapStream.getFileSize() - _cipherBegBackPos);
	_memmapStream.setpos(_cipherBegPos);
	_memmapStream.read(&_deadbe7a, sizeof(_deadbe7a));
	_memmapStream.read(&_keySize, sizeof(_keySize));
	_cipherKey = std::unique_ptr<uint32_t[]>(new uint32_t[_keySize / sizeof(uint32_t)]);
	_fileKey = std::unique_ptr<uint32_t[]>(new uint32_t[_keySize / sizeof(uint32_t)]);
	_memmapStream.read(_fileKey.get(), _keySize);
	_cipher_magic();
	_memmapStream.setpos(0);
}

void CipherStream::writeKey()
{
	_deadbe7a = 0xdeadbe7a;
	_cipherBegPos = uint32_t(getpos());
	_filestream.write(reinterpret_cast<char*>(&_deadbe7a), sizeof(_deadbe7a));
	_filestream.write(reinterpret_cast<char*>(&_keySize), sizeof(_keySize));
	_filestream.write(reinterpret_cast<char*>(_fileKey.get()), _keySize);
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
	uintmax_t filesize = _filestream.tellp();
	_cipherBegBackPos = uint32_t(filesize + sizeof(_cipherBegBackPos) - _cipherBegPos);
	_filestream.write(reinterpret_cast<char*>(&_cipherBegBackPos), sizeof(_cipherBegBackPos));

	_filestream.seekg(_filestream.tellp());
	if (!_filestream.good())
	{
		throw FileIoError("Filestream failed.");
	}
}

void CipherStream::_cipher_magic()
{
#define ROTL(val, bits) (((val) << (bits)) | ((val) >> (32-(bits))))
	uint32_t currentKey;

	for (uint32_t i = 0; i < _keySize; i += 4)
	{
		currentKey = _fileKey[i / 4];
		for (int byte = 0; byte < 4; byte++)
		{
			uint32_t tempVal = ROTL(currentKey + _cipherBegPos, 8);
			uint8_t *tempBytes = reinterpret_cast<uint8_t*>(&tempVal);
			for (uint32_t j = 0; j < 4; j++)
			{
				currentKey = cipherConst[uint8_t(currentKey ^ tempBytes[j])]
					^ (currentKey >> 8);
			}
			*(reinterpret_cast<uint8_t*>(_cipherKey.get()) + byte + i) = uint8_t(currentKey);
		}
	}
#undef ROTL
}
