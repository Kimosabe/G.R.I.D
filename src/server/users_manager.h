#ifndef KIMO_USERS_MANAGER_H_
#define KIMO_USERS_MANAGER_H_

#include <iostream>
#include <string>
#include <fstream>
#include <vector>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/thread/mutex.hpp>
#include <msgpack.hpp>
#include <cryptopp\ripemd.h>

#include "acl.h"

using Kimo::ACL;

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
    
    bool isValid(const String& login, const String& password, bool password_already_hashed = false);
    int addUser(const String& login, const String& password, bool password_already_hashed = false);
    int removeUser(int id);
    int getId(const String& login);
	long newToken(int id);
	long getToken(int id);
	bool isAllowed(int id, const ACL::PRIVILEGE privilege);
	bool isDenied(int id, const ACL::PRIVILEGE privilege);
	void allow(int id, const ACL::PRIVILEGE privilege);
	void deny(int id, const ACL::PRIVILEGE privilege);

	int serialize(msgpack::sbuffer& buffer);
	int deserialize(msgpack::sbuffer& buffer);

	time_t getTokenLifetime();
	void setTokenLifetime(time_t lifetime);
	time_t getTokenTimestamp(int user_id);
	time_t getLastModified();
	boost::mutex& mutex();

    // Debug
    void printUsers(std::ostream& out);

private:
    UsersManager(const UsersManager&);
    void operator=(const UsersManager&);
    
    int loadUsers();
    int saveUsers();
	void hash(const String& src, String& dst);

    //! Структура данных о пользователе.
    struct User
    {
        User();
        User(const User& user);

        User& operator=(const User& user);
        inline bool operator<(const User&);

        int id;
		long token;
		time_t token_expire_time;
		ACL acl;
        //char login[MAX_LOGIN_SIZE + 1];
        //byte password_hash[MAX_PASSWORD_HASH_SIZE];
        String login;
        String password_hash;

		// для сериализации
		MSGPACK_DEFINE(id, token, token_expire_time, acl, login, password_hash);
    };
    typedef std::vector<User> Users;
	struct __Users
	{
		Users users;
		//! время последней омдификации
		time_t m_last_modified;
		MSGPACK_DEFINE(users, m_last_modified);
	};

    //! Зашифрованный файл с данными по пользователям.
    String m_users_path;
    //! Вектор со всеми данными по пользователям.
    __Users m_users_storage;
    //! Класс, вычисляющий хэш.
    Hasher m_hasher;
	//! Генератор случайных чисел
	boost::mt19937 m_engine;
	boost::uniform_int<long> m_uniform;
	//! Время жизни токена
	time_t m_token_lifetime;
	//! мьютекс о_О
	boost::mutex m_mutex;
};

#else

class UsersManager;

#endif // KIMO_USERS_MANAGER_H_
