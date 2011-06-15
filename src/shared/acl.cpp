#include "acl.h"

namespace Kimo{

ACL::ACL() : m_acl(0)
{

}

ACL::ACL(const ACL &acl)
{
	m_acl = acl.m_acl;
}

ACL& ACL::operator =(const ACL &acl)
{
	m_acl = acl.m_acl;
	return *this;
}

ACL::~ACL()
{

}

void ACL::allow(const PRIVILEGE privilege)
{
	if (privilege == PRIV_MAXPRIV)
		return;

	m_acl |= privilege;
}

void ACL::deny(const PRIVILEGE privilege)
{
	if (privilege == PRIV_MAXPRIV)
		return;

	m_acl &= ~privilege;
}

bool ACL::isAllowed(const PRIVILEGE privilege)
{
	if (privilege == PRIV_MAXPRIV)
		return false;

	if (privilege == PRIV_ALLPRIV)
		return m_acl == PRIV_ALLPRIV;

	return (m_acl & privilege) != 0;
}

bool ACL::isDenied(const PRIVILEGE privilege)
{
	if (privilege == PRIV_MAXPRIV)
		return true;

	if (privilege == PRIV_ALLPRIV)
		return m_acl != PRIV_ALLPRIV;

	return (m_acl & privilege) == 0;
}

ACL::ACL_t ACL::acl()
{
	return m_acl;
}

bool ACL::isValidPrivilege(Kimo::ACL::ACL_t privilege)
{
	short c = 1;
	for (ACL_t i = 1; i <= ACL::PRIV_MAXPRIV - 1 && c <= sizeof(ACL_t)*8; i <<= 1, ++c)
		if (privilege == i)
			return true;
	if (privilege == ACL::PRIV_ALLPRIV)
		return true;
	return false;
}

Kimo::ACL::PRIVILEGE ACL::makePrivilege(Kimo::ACL::ACL_t privilege)
{
	switch(privilege)
	{
	case ACL::PRIV_ALLPRIV : return ACL::PRIV_ALLPRIV;
	case ACL::PRIV_LOGIN : return ACL::PRIV_LOGIN;
	case ACL::PRIV_PROCEXEC : return ACL::PRIV_PROCEXEC;
	case ACL::PRIV_PROCRD : return ACL::PRIV_PROCRD;
	case ACL::PRIV_PROCTERM : return ACL::PRIV_PROCTERM;
	case ACL::PRIV_USERRD : return ACL::PRIV_USERRD;
	case ACL::PRIV_USERWR : return ACL::PRIV_USERWR;
	default : throw new std::logic_error("privilege out of range");
	}
}

} // namespace Kimo