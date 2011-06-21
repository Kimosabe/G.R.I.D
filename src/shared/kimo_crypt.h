#ifndef KIMO_CRYPT_H
#define KIMO_CRYPT_H

#include <string>

typedef std::string String;

void encrypt(String& dst, const char* src, const size_t len, const String& passwd);
void decrypt(String& dst, const char* src, const size_t len, const String& passwd);
int encrypt_from_file(String& dst, const char* path, const String& passwd);
int decrypt_from_file(String& dst, const char* path, const String& passwd);
void encrypt_to_file(const char* path, const char* src, const size_t len, const String& passwd);
void decrypt_to_file(const char* path, const char* src, const size_t len, const String& passwd);

#endif//KIMO_CRYPT_H