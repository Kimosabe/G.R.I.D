#include <cryptopp/files.h>
#include <cryptopp/modes.h>
#include <cryptopp/aes.h>
#include <cryptopp/ripemd.h>
#include <cryptopp/pwdbased.h>

#include "kimo_crypt.h"

typedef CryptoPP::AES Cipher;
typedef CryptoPP::RIPEMD160 Hasher;
typedef CryptoPP::PKCS12_PBKDF<Hasher> PasswordToKey;
typedef CryptoPP::CBC_Mode<Cipher> Mode;

void encrypt(String& dst, const char* src, const size_t len, const String& passwd)
{
	byte key[Cipher::DEFAULT_KEYLENGTH];
	PasswordToKey o_O;
	o_O.DeriveKey(key, Cipher::DEFAULT_KEYLENGTH, 0, (const byte*)passwd.data(), passwd.size(), 0, 0, 10, 0.0);

	byte iv[Cipher::BLOCKSIZE];
	memset(iv, 0, Cipher::BLOCKSIZE);

	Mode::Encryption e(key, Cipher::DEFAULT_KEYLENGTH, iv);

	CryptoPP::StreamTransformationFilter encryption(e, new CryptoPP::StringSink(dst));
	encryption.Put((const byte*)src, len);
	encryption.MessageEnd();
}

void decrypt(String& dst, const char* src, const size_t len, const String& passwd)
{
	byte key[Cipher::DEFAULT_KEYLENGTH];
	PasswordToKey o_O;
	o_O.DeriveKey(key, Cipher::DEFAULT_KEYLENGTH, 0, (const byte*)passwd.data(), passwd.size(), 0, 0, 10, 0.0);

	byte iv[Cipher::BLOCKSIZE];
	memset(iv, 0, Cipher::BLOCKSIZE);

	Mode::Decryption d(key, Cipher::DEFAULT_KEYLENGTH, iv);

	CryptoPP::StreamTransformationFilter decryption(d, new CryptoPP::StringSink(dst));
	decryption.Put((const byte*)src, len);
	decryption.MessageEnd();
}

int encrypt_from_file(String& dst, const char* path, const String& passwd)
{
	String contents;
	try
	{
		CryptoPP::FileSource(path, true, new CryptoPP::StringSink(contents));
		encrypt(dst, contents.c_str(), contents.size(), passwd);
	}
	catch(CryptoPP::FileStore::OpenErr& /*ex*/)
	{
		return -1;
	}

	return 0;
}

int decrypt_from_file(String& dst, const char* path, const String& passwd)
{
	String contents;
	try
	{
		CryptoPP::FileSource(path, true, new CryptoPP::StringSink(contents));
		decrypt(dst, contents.c_str(), contents.size(), passwd);
	}
	catch(CryptoPP::FileStore::OpenErr& /*ex*/)
	{
		return -1;
	}

	return 0;
}

void encrypt_to_file(const char* path, const char* src, const size_t len, const String& passwd)
{
	byte key[Cipher::DEFAULT_KEYLENGTH];
	PasswordToKey o_O;
	o_O.DeriveKey(key, Cipher::DEFAULT_KEYLENGTH, 0, (const byte*)passwd.data(), passwd.size(), 0, 0, 10, 0.0);

	byte iv[Cipher::BLOCKSIZE];
	memset(iv, 0, Cipher::BLOCKSIZE);

	Mode::Encryption e(key, Cipher::DEFAULT_KEYLENGTH, iv);

	CryptoPP::StreamTransformationFilter encryption(e, new CryptoPP::FileSink(path));
	encryption.Put((const byte*)src, len);
	encryption.MessageEnd();
}

void decrypt_to_file(const char* path, const char* src, const size_t len, const String& passwd)
{
	byte key[Cipher::DEFAULT_KEYLENGTH];
	PasswordToKey o_O;
	o_O.DeriveKey(key, Cipher::DEFAULT_KEYLENGTH, 0, (const byte*)passwd.data(), passwd.size(), 0, 0, 10, 0.0);

	byte iv[Cipher::BLOCKSIZE];
	memset(iv, 0, Cipher::BLOCKSIZE);

	Mode::Decryption d(key, Cipher::DEFAULT_KEYLENGTH, iv);

	CryptoPP::StreamTransformationFilter decryption(d, new CryptoPP::FileSink(path));
	decryption.Put((const byte*)src, len);
	decryption.MessageEnd();
}
