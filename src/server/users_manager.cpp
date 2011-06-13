#include <iostream>
#include <ctime>

#include "users_manager.h"
#include <CryptoPP/hex.h>

using std::cout;
using std::endl;

UsersManager::UsersManager(const String& users_path)
: m_users_path(users_path), m_engine(time(NULL)), m_uniform(0, 0x7FFFFFFF),
  m_token_lifetime(3600)
{
	if (loadUsers() < 0 || m_users_storage.empty())
	{
		String root("root");
		String password("vv007");
		hash(password, password);
		
		int id = addUser(root, password, true);
		allow(id, Kimo::ACL::PRIV_ALLPRIV);
	}
	else
	{
		cout << m_users_storage.size() << endl;
	}
}

UsersManager::~UsersManager()
{
}

bool UsersManager::isValid(const String& login, const String& password, bool password_already_hashed)
{
    Users::iterator itr;
	if (password_already_hashed)
	{
		for (itr = m_users_storage.begin(); itr != m_users_storage.end(); ++itr)
			if (itr->login == login && itr->password_hash == password)
				return true;	
	}
	else
	{
		String password_hash;
		hash(password, password_hash);

		for (itr = m_users_storage.begin(); itr != m_users_storage.end(); ++itr)
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

    // Логин слишком длинный
    if (login.size() > MAX_LOGIN_SIZE)
        return -2;

    User user;
    int i, id = -1;

    // Ищем незанятую ячейку в хранилище пользователей.
    Users::iterator itr;
    for (i = 0, itr = m_users_storage.begin(); itr != m_users_storage.end(); ++itr, ++i)
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
        id = m_users_storage.size();
        user.id = id;
        m_users_storage.push_back(user);
    }
    else
    {
        user.id = id;
        *itr = user;
    }

    saveUsers();

    return id;
}

int UsersManager::removeUser(int id)
{
    // не верный идентификатор пользователя
    if (id < 0)
        return -1;

    Users::iterator itr;
    for (itr = m_users_storage.begin(); itr != m_users_storage.end(); ++itr)
        if (itr->id == id)
        {
            itr->login.clear();
            itr->password_hash.clear();
            itr->id = -1;

            saveUsers();

            return 0;
        }

    // пользователь не найден
    return -2;
}

int UsersManager::getId(const String& login)
{
    Users::iterator itr;
    for (itr = m_users_storage.begin(); itr != m_users_storage.end(); ++itr)
        if (itr->login == login)
            return itr->id;

    // Пользователь не найден
    return -1;
}

void UsersManager::printUsers(std::ostream& out)
{
    Users::iterator itr;
    for (itr = m_users_storage.begin(); itr != m_users_storage.end(); ++itr)
        out << "id: " << itr->id << std::endl
            << "login: " << itr->login << std::endl
            << "password: " << itr->password_hash << std::endl
			<< "acl: " << std::hex << (int)itr->acl.acl() << std::dec << std::endl;
}

int UsersManager::loadUsers()
{
    Fstream file(m_users_path.c_str(), Fstream::in | Fstream::binary);

	msgpack::sbuffer buffer;
	char c;
	bool data_was_read = false;
	file.read(&c, 1);
	while(file)
	{
		data_was_read = true;
		buffer.write(&c, 1);
		file.read(&c, 1);
	}

	// XXX: ужасное решение с присваиванием в каждом витке цикла
	if (data_was_read)
		return deserialize(buffer);

    return -1;
}

int UsersManager::saveUsers()
{
	msgpack::sbuffer buffer;
	if (serialize(buffer) < 0)
		return -1;

	Fstream file(m_users_path.c_str(), Fstream::out | Fstream::binary | Fstream::trunc);
	file.write(buffer.data(), buffer.size());

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
	if (id < 0 || id >= m_users_storage.size() || m_users_storage[id].id < 0)
		return false;

	return m_users_storage[id].acl.isAllowed(privilege);
}

bool UsersManager::isDenied(int id, const ACL::PRIVILEGE privilege)
{
	if (id < 0 || id >= m_users_storage.size() || m_users_storage[id].id < 0)
		return true;

	return m_users_storage[id].acl.isDenied(privilege);
}

void UsersManager::allow(int id, const ACL::PRIVILEGE privilege)
{
	// TODO: добавить возвращаемый результат найден/не найден
	if (id < 0 || id >= m_users_storage.size() || m_users_storage[id].id < 0)
		return;

	m_users_storage[id].acl.allow(privilege);

	saveUsers();
}

void UsersManager::deny(int id, const ACL::PRIVILEGE privilege)
{
	// TODO: добавить возвращаемый результат найден/не найден
	if (id < 0 || id >= m_users_storage.size() || m_users_storage[id].id < 0)
		return;

	m_users_storage[id].acl.deny(privilege);

	saveUsers();
}

int UsersManager::serialize(msgpack::sbuffer& buffer)
{
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

		Users users;
		obj.convert(&users);

		m_users_storage = users;
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

	return 0;
}

long UsersManager::newToken(int id)
{
	if (id < 0 || m_users_storage.size() <= id)
		return -1;

	m_users_storage[id].token = m_uniform(m_engine);
	m_users_storage[id].token_expire_time = time(NULL) + m_token_lifetime;

	return m_users_storage[id].token;
}

long UsersManager::getToken(int id)
{
	if (id < 0 || m_users_storage.size() <= id)
		return -1;

	if (m_users_storage[id].token_expire_time < time(NULL))
		return -2;

	return m_users_storage[id].token;
}

time_t UsersManager::getTokenLifetime()
{
	return m_token_lifetime;
}

void UsersManager::setTokenLifetime(time_t lifetime)
{
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
