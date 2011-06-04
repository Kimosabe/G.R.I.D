#ifndef KIMO_USERS_MANAGER_H_
#define KIMO_USERS_MANAGER_H_

#include <iostream>
#include <string>
#include <fstream>
#include <vector>

#include <msgpack.hpp>
#include <cryptopp\ripemd.h>

#include "acl.h"

typedef std::string String;
typedef std::fstream Fstream;

class UsersManager
{
    typedef CryptoPP::RIPEMD256 Hasher;

public:
    /*! Конструктор.
     *  
     *  \param users_path Путь к файлу с данными по пользователям.
     */
    UsersManager(const String& users_path);

    ~UsersManager();
    
    enum
    {
        MAX_PASSWORD_HASH_SIZE = Hasher::DIGESTSIZE,
        MAX_LOGIN_SIZE = Hasher::DIGESTSIZE,
    };
    
    bool isValid(const String& login, const String& password);
    int addUser(const String& login, const String& password);
    int removeUser(int id);
    int getId(const String& login);
	bool isAllowed(int id, const ACL::PRIVILEGE privilege);
	bool isDenied(int id, const ACL::PRIVILEGE privilege);
	void allow(int id, const ACL::PRIVILEGE privilege);
	void deny(int id, const ACL::PRIVILEGE privilege);

	int serialize(msgpack::sbuffer& buffer);
	int deserialize(msgpack::sbuffer& buffer);

    // Debug
    void printUsers(std::ostream& out);

private:
    UsersManager(const UsersManager&);
    void operator=(const UsersManager&);
    
    int loadUsers();
    int saveUsers();

    //! Структура данных о пользователе.
    struct User
    {
        User();
        User(const User& user);

        User& operator=(const User& user);
        inline bool operator<(const User&);

        int id;
		ACL acl;
        //char login[MAX_LOGIN_SIZE + 1];
        //byte password_hash[MAX_PASSWORD_HASH_SIZE];
        String login;
        String password_hash;

		// для сериализации
		MSGPACK_DEFINE(id, acl, login, password_hash);
    };
    typedef std::vector<User> Users;

    //! Зашифрованный файл с данными по пользователям.
    String m_users_path;
    //! Вектор со всеми данными по пользователям.
    Users m_users_storage;
    //! Класс, вычисляющий хэш.
    Hasher m_hasher;
};

#else

class UsersManager;

#endif // KIMO_USERS_MANAGER_H_
