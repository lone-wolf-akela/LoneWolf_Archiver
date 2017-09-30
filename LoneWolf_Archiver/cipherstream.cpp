#include "stdafx.h"

#include "cipherstream.h"
#include "cipher.h"

void CipherStream::open(std::experimental::filesystem::path file, CipherStreamState state)
{
	_state = state;
	switch (state)
	{
	case Read_EncryptionUnknown:
		_memmapStream.open(file);
		if (std::strncmp(_memmapStream.getReadptr(), "_ARCHIVE", 8))
		{
			_state = Read_Encrypted;
			cipherInit();
		}
		else
		{
			_state = Read_NonEncrypted;
		}
		_memmapStream.setpos(0);
		break;

	case Read_Encrypted:
		_memmapStream.open(file);
		cipherInit();
		break;

	case Read_NonEncrypted:
		_memmapStream.open(file);
		break;

	case Write_Encrypted:
		throw NotImplementedError();
		break;
	case Write_NonEncrypted:
		throw NotImplementedError();
		break;
	default:
		throw OutOfRangeError();
	}
}

void CipherStream::close()
{
	_memmapStream.close();
}

void CipherStream::read(void* dst, size_t length)
{
	switch (_state)
	{
	case Read_Encrypted:
	{
		uint8_t *tmpDst = reinterpret_cast<uint8_t*>(dst);
		size_t begpos = _memmapStream.getpos();
		_memmapStream.read(dst, length);
		for (size_t i = 0; i < length; ++i)
		{
			tmpDst[i] +=
				*(reinterpret_cast<uint8_t*>(_cipherKey.get()) + (begpos + i) % _keySize);
		}
		return;
	}

	case Read_NonEncrypted:
		_memmapStream.read(dst, length);
		return;

	case Read_EncryptionUnknown:
	case Write_Encrypted:
	case Write_NonEncrypted:
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

void CipherStream::write(void* src, size_t length)
{
	throw NotImplementedError();
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
		throw NotImplementedError();
		break;
	case Write_NonEncrypted:
		throw NotImplementedError();
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
		throw NotImplementedError();
		break;
	case Write_NonEncrypted:
		throw NotImplementedError();
		break;

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
		throw NotImplementedError(); 
		break;
	case Write_NonEncrypted:
		throw NotImplementedError(); 
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

void CipherStream::cipherInit()
{
	_memmapStream.setpos(_memmapStream.getFileSize() - sizeof(_cipherBegBackPos));
	_memmapStream.read(&_cipherBegBackPos, sizeof(_cipherBegBackPos));
	_cipherBegPos = uint32_t(_memmapStream.getFileSize() - _cipherBegBackPos);
	_memmapStream.setpos(_cipherBegPos);
	_memmapStream.read(&_deadbe7a, sizeof(_deadbe7a));
	_memmapStream.read(&_keySize, sizeof(_keySize));
	_cipherKey = std::unique_ptr<uint32_t[]>(new uint32_t[_keySize / sizeof(uint32_t)]);
	_fileKey = std::unique_ptr<uint32_t[]>(new uint32_t[_keySize / sizeof(uint32_t)]);
	_memmapStream.read(_fileKey.get(), _keySize);
	cipher_magic();
	_memmapStream.setpos(0);
}

void CipherStream::cipher_magic()
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
