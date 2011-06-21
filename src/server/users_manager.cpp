#include <iostream>
#include <ctime>

#include "users_manager.h"
#include <CryptoPP/hex.h>
#include <boost/thread/locks.hpp>
#include "kimo_crypt.h"

using std::cout;
using std::endl;

UsersManager::UsersManager(const String& users_path, const std::string& passwd)
: m_users_path(users_path), m_engine((boost::uint32_t)time(NULL)), m_uniform(0, 0x7FFFFFFF),
  m_token_lifetime(3600), m_passwd(passwd)
{
	if (loadUsers() < 0 || m_users_storage.users.empty())
	{
		String root("root");
		String password("");
		hash(password, password);
		
		int id = addUser(root, password, true);
		allow(id, Kimo::ACL::PRIV_ALLPRIV);
	}
	else
	{
		cout << m_users_storage.users.size() << endl;
	}
}

UsersManager::~UsersManager()
{
}

bool UsersManager::isValid(const String& login, const String& password, bool password_already_hashed)
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
    Users::iterator itr;
	if (password_already_hashed)
	{
		for (itr = m_users_storage.users.begin(); itr != m_users_storage.users.end(); ++itr)
			if (itr->login == login && itr->password_hash == password)
				return true;	
	}
	else
	{
		String password_hash;
		hash(password, password_hash);

		for (itr = m_users_storage.users.begin(); itr != m_users_storage.users.end(); ++itr)
			if (itr->login == login && itr->password_hash == password_hash)
				return true;
	}

    return false;
}

// TODO: Добавить проверку на выход за границу int'а
int UsersManager::addUser(const String& login, const String& password, bool password_already_hashed)
{
    // Если пользователь существует, не можем добавить
    if (getId(login) >= 0)
        return -1;
	int id = -1;
	{
	boost::lock_guard<boost::mutex> lock(m_mutex);
    // Логин слишком длинный
    if (login.size() > MAX_LOGIN_SIZE)
        return -2;

    User user;
    int i;

    // Ищем незанятую ячейку в хранилище пользователей.
    Users::iterator itr;
    for (i = 0, itr = m_users_storage.users.begin(); itr != m_users_storage.users.end(); ++itr, ++i)
        if (itr->id < 0)
        {
            id = i;
            break;
        }

    user.login = login;

	if (password_already_hashed)
	{
		user.password_hash = password;
	}
	else
	{
		// Вычисляем хэш пароля
		hash(password, user.password_hash);
	}

    // Если все ячейки заняты, прийдется добавить новую.
    if (id < 0)
    {
        id = m_users_storage.users.size();
        user.id = id;
        m_users_storage.users.push_back(user);
    }
    else
    {
        user.id = id;
        *itr = user;
    }

	m_users_storage.m_last_modified = time(NULL);

	}
    saveUsers();

    return id;
}

int UsersManager::removeUser(int id)
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
    // не верный идентификатор пользователя
    if (id < 0)
        return -1;

    Users::iterator itr;
    for (itr = m_users_storage.users.begin(); itr != m_users_storage.users.end(); ++itr)
        if (itr->id == id)
        {
            itr->login.clear();
            itr->password_hash.clear();
            itr->id = -1;

			m_users_storage.m_last_modified = time(NULL);

			m_mutex.unlock(); // XXX
            saveUsers();
			m_mutex.lock();

            return 0;
        }

    // пользователь не найден
    return -2;
}

int UsersManager::getId(const String& login)
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
    Users::iterator itr;
    for (itr = m_users_storage.users.begin(); itr != m_users_storage.users.end(); ++itr)
        if (itr->login == login)
            return itr->id;

    // Пользователь не найден
    return -1;
}

void UsersManager::printUsers(std::ostream& out)
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
    Users::iterator itr;
    for (itr = m_users_storage.users.begin(); itr != m_users_storage.users.end(); ++itr)
        out << "id: " << itr->id << std::endl
            << "login: " << itr->login << std::endl
            << "password: " << itr->password_hash << std::endl
			<< "acl: " << std::hex << (int)itr->acl.acl() << std::dec << std::endl;
}

int UsersManager::loadUsers()
{
	msgpack::sbuffer buffer;
	std::string temp;
	if (decrypt_from_file(temp, m_users_path.c_str(), m_passwd) < 0)
		return -1;
	buffer.write(temp.data(), temp.size());
	return deserialize(buffer);
}

int UsersManager::saveUsers()
{
	msgpack::sbuffer buffer;
	if (serialize(buffer) < 0)
		return -1;

	encrypt_to_file(m_users_path.c_str(), buffer.data(), buffer.size(), m_passwd);

    return 0;
}

bool UsersManager::User::operator <(const User& user)
{
    return id < user.id;
}

UsersManager::User::User()
: id(-1), token(-1), token_expire_time(-1),
  acl(), login(), password_hash()
{
}

UsersManager::User::User(const User& user)
: id(user.id), token(user.token), token_expire_time(user.token_expire_time),
  acl(user.acl), login(user.login), password_hash(user.password_hash)
{
}

UsersManager::User& UsersManager::User::operator=(const User& user)
{
    id = user.id;
	token = user.token;
	token_expire_time = user.token_expire_time;
    login = user.login;
    password_hash = user.password_hash;
	acl = user.acl;

    return *this;
}

bool UsersManager::isAllowed(int id, const ACL::PRIVILEGE privilege)
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
	if (id < 0 || id >= m_users_storage.users.size() || m_users_storage.users[id].id < 0)
		return false;

	return m_users_storage.users[id].acl.isAllowed(privilege);
}

bool UsersManager::isDenied(int id, const ACL::PRIVILEGE privilege)
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
	if (id < 0 || id >= m_users_storage.users.size() || m_users_storage.users[id].id < 0)
		return true;
	

	return m_users_storage.users[id].acl.isDenied(privilege);
}

void UsersManager::allow(int id, const ACL::PRIVILEGE privilege)
{
	{
	boost::lock_guard<boost::mutex> lock(m_mutex);
	// TODO: добавить возвращаемый результат найден/не найден
	if (id < 0 || id >= m_users_storage.users.size() || m_users_storage.users[id].id < 0)
		return;
	
	m_users_storage.users[id].acl.allow(privilege);
	}

	m_users_storage.m_last_modified = time(NULL);

	saveUsers();
}

void UsersManager::deny(int id, const ACL::PRIVILEGE privilege)
{
	{
	boost::lock_guard<boost::mutex> lock(m_mutex);
	// TODO: добавить возвращаемый результат найден/не найден
	if (id < 0 || id >= m_users_storage.users.size() || m_users_storage.users[id].id < 0)
		return;
	
	m_users_storage.users[id].acl.deny(privilege);
	}

	m_users_storage.m_last_modified = time(NULL);

	saveUsers();
}

int UsersManager::serialize(msgpack::sbuffer& buffer)
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
	msgpack::pack(buffer, m_users_storage);

	return 0;
}

int UsersManager::deserialize(msgpack::sbuffer& buffer)
{
	msgpack::unpacked unpacked;
	msgpack::object obj;

	try
	{
		msgpack::unpack(&unpacked, buffer.data(), buffer.size());
		obj = unpacked.get();

		__Users users;
		obj.convert(&users);

		boost::lock_guard<boost::mutex> lock(m_mutex);
		m_users_storage = users;
		//m_users_storage.m_last_modified = time(NULL);
	}
	catch(msgpack::unpack_error& e)
	{
		cout << e.what() << endl;
		return -1;
	}
	catch(std::exception& e)
	{
		cout << e.what() << endl;
		return -1;
	}

	UsersManager::saveUsers();

	return 0;
}

long UsersManager::newToken(int id)
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
	if (id < 0 || m_users_storage.users.size() <= id)
		return -1;

	m_users_storage.users[id].token = m_uniform(m_engine);
	m_users_storage.users[id].token_expire_time = time(NULL) + m_token_lifetime;

	m_users_storage.m_last_modified = time(NULL);

	return m_users_storage.users[id].token;
}

long UsersManager::getToken(int id)
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
	if (id < 0 || m_users_storage.users.size() <= id)
		return -1;

	if (m_users_storage.users[id].token_expire_time < time(NULL))
		return -2;

	return m_users_storage.users[id].token;
}

time_t UsersManager::getTokenLifetime()
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
	return m_token_lifetime;
}

void UsersManager::setTokenLifetime(time_t lifetime)
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
	m_token_lifetime = lifetime;
}

void UsersManager::hash(const String& src, String& dst)
{
	byte buffer[MAX_PASSWORD_HASH_SIZE];
	CryptoPP::HexEncoder encoder;

	m_hasher.CalculateDigest(buffer, (const byte*)src.c_str(), src.length());
	
	dst.clear();
	encoder.Attach( new CryptoPP::StringSink( dst ) );
	encoder.Put( buffer, MAX_PASSWORD_HASH_SIZE );
	encoder.MessageEnd();
}

boost::mutex& UsersManager::mutex()
{
	return m_mutex;
}

time_t UsersManager::getTokenTimestamp(int user_id)
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
	if (user_id < 0 || m_users_storage.users.size() <= user_id)
		return -1;

	return m_users_storage.users[user_id].token_expire_time;
}

time_t UsersManager::getLastModified()
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
	return m_users_storage.m_last_modified;
}

void UsersManager::setLastModified(time_t timestamp)
{
	m_users_storage.m_last_modified = timestamp;
}

int UsersManager::getId(long token)
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
    Users::iterator itr;
    for (itr = m_users_storage.users.begin(); itr != m_users_storage.users.end(); ++itr)
		if (itr->token == token)
            return itr->id;

    // Пользователь не найден
    return -1;
}

std::string UsersManager::getLogin(int id)
{
	boost::lock_guard<boost::mutex> lock(m_mutex);
	if (id < 0 || m_users_storage.users.size() <= id)
		return 0;

	return m_users_storage.users[id].login;
}
