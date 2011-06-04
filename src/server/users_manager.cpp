#include <iostream>

#include "users_manager.h"

using std::cout;
using std::endl;

UsersManager::UsersManager(const String& users_path)
: m_users_path(users_path)
{
// TODO: Добавить загрузку данных из файла.
    loadUsers();
}

UsersManager::~UsersManager()
{
}

bool UsersManager::isValid(const String& login, const String& password)
{
    Users::iterator itr;
    byte buffer[MAX_PASSWORD_HASH_SIZE + 1];

    m_hasher.CalculateDigest(buffer, (const byte*)password.c_str(), password.length());
    buffer[m_hasher.DigestSize()] = '\0';
    String password_hash = (const char*)buffer;

    for (itr = m_users_storage.begin(); itr != m_users_storage.end(); ++itr)
        if (itr->login == login && itr->password_hash == password_hash)
            return true;

    return false;
}

// TODO: Добавить проверку на выход за границу int'а
int UsersManager::addUser(const String& login, const String& password)
{
    // Если пользователь существует, не можем добавить
    if (getId(login) >= 0)
        return -1;

    // Логин слишком длинный
    if (login.size() > MAX_LOGIN_SIZE)
        return -2;

    User user;
    byte buffer[MAX_PASSWORD_HASH_SIZE + 1];
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

    // Вычисляем хэш пароля
    m_hasher.CalculateDigest(buffer, (const byte*)password.c_str(), password.length());
    buffer[m_hasher.DigestSize()] = '\0';
    user.password_hash = (const char*)buffer;

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
	/*
    file.setf(Fstream::left);
    file.seekp(0);
    User user;
    char buffer[MAX_LOGIN_SIZE + 1];

    m_users_storage.clear();

    //file >> user.id;
    file.read((char*)(&user.id), sizeof(user.id));

    file.read(buffer, MAX_LOGIN_SIZE);
    buffer[MAX_LOGIN_SIZE] = '\0';
    user.login = buffer;

    file.read(buffer, MAX_PASSWORD_HASH_SIZE);
    buffer[MAX_PASSWORD_HASH_SIZE] = '\0';
    user.password_hash = buffer;

    while (!file.fail())
    {
        m_users_storage.push_back(user);

        //file >> user.id;
        file.read((char*)(&user.id), sizeof(user.id));

        file.read(buffer, MAX_LOGIN_SIZE);
        buffer[MAX_LOGIN_SIZE] = '\0';
        user.login = buffer;

        file.read(buffer, MAX_PASSWORD_HASH_SIZE);
        buffer[MAX_PASSWORD_HASH_SIZE] = '\0';
        user.password_hash = buffer;
    }*/

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
		deserialize(buffer);

    return 0;
}

int UsersManager::saveUsers()
{
    Fstream file(m_users_path.c_str(), Fstream::out | Fstream::binary | Fstream::trunc);
	/*
    Users::iterator itr;
    int width = file.width();
    file.setf(Fstream::left);
    file.fill('\0');

    for(itr = m_users_storage.begin(); itr != m_users_storage.end(); ++itr)
    {

        file.width(width);
        file.write((char*)(&itr->id), sizeof(itr->id));
        //file << itr->id;

        file.width(MAX_LOGIN_SIZE);
        file << itr->login;

        file.width(MAX_PASSWORD_HASH_SIZE);
        file << itr->password_hash;
    }*/

	msgpack::sbuffer buffer;
	serialize(buffer);
	file.write(buffer.data(), buffer.size());

    return 0;
}

bool UsersManager::User::operator <(const User& user)
{
    return id < user.id;
}

UsersManager::User::User()
: id(-1), login(), password_hash()
{
}

UsersManager::User::User(const User& user)
: id(user.id), login(user.login), password_hash(user.password_hash), acl(user.acl)
{
}

UsersManager::User& UsersManager::User::operator=(const User& user)
{
    id = user.id;
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
