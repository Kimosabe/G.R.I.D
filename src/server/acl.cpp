#include "acl.h"

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