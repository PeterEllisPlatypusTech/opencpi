/*
 *  This file is part of OpenCPI (www.opencpi.org).
 *     ____                   __________   ____
 *    / __ \____  ___  ____  / ____/ __ \ /  _/ ____  _________ _
 *   / / / / __ \/ _ \/ __ \/ /   / /_/ / / /  / __ \/ ___/ __ `/
 *  / /_/ / /_/ /  __/ / / / /___/ ____/_/ / _/ /_/ / /  / /_/ /
 *  \____/ .___/\___/_/ /_/\____/_/    /___/(_)____/_/   \__, /
 *      /_/                                             /____/
 *
 *  OpenCPI is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenCPI is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with OpenCPI.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <ezxml.h>
#include <OcpiOsAssert.h>
#include <OcpiOsMisc.h>
#include <OcpiUtilHash.h>
#include <OcpiUtilAutoMutex.h>
#include <OcpiUtilEzxml.h>
#include <OcpiUtilMisc.h>
#include <OcpiPValue.h>
#include "XferServices.h"
#include "XferFactory.h"
#include "XferManager.h"

namespace OX = OCPI::Util::EzXml;
namespace OU = OCPI::Util;
namespace OS = OCPI::OS;
namespace OD = OCPI::Driver;
namespace DataTransfer {

// An XferFactory keeps track of which templates exist.
XferServices &XferFactory::
getTemplate(EndPoint &source, EndPoint &target) {
  TemplatePair pair(&source, &target);
  OU::SelfAutoMutex guard(this);
  TemplateMapIter ti = m_templates.find(pair);
  XferServices *temp = 
    ti == m_templates.end() ? 
    (m_templates[pair] = &createXferServices(source, target)) :
    ti->second;
  temp->addRef();
  return *temp;
}
void XferFactory::
removeTemplate(XferServices &xfs) {
  TemplatePair pair(&xfs.m_from, &xfs.m_to);
  OU::SelfAutoMutex guard(this);
  TemplateMapIter ti = m_templates.find(pair);
  assert(ti != m_templates.end());
  m_templates.erase(ti);
}


MailBox 
XferFactory::
getNextMailBox()
{
  static MailBox mailbox = 1;
  static bool mb_once = false;
  if ( ! mb_once ) {
    const char* env = getenv("OCPI_TRANSFER_MAILBOX");
    if( !env || (env[0] == 0)) {
      ocpiDebug("Set ""OCPI_TRANSFER_MAILBOX"" environment variable to control mailbox");
    }
    else {
      mailbox = (MailBox)atoi(env);
    }
    mb_once = true;
  }
#if 0
  for (; mailbox < m_locations.size() && m_locations[mailbox]; mailbox++)
    if (mailbox == MAX_SYSTEM_SMBS || mailbox > getMaxMailBox())
      throw OU::Error("Mailboxes for endpoints for protocol %s are exhausted (all %u are used)",
		      getProtocol(), getMaxMailBox());
#endif
  ocpiDebug("Transfer factory %p returning mailbox %u", this, mailbox);
  return mailbox++;
}



MailBox 
XferFactory::
getMaxMailBox()
{
  static bool mmb_once = false;
  static MailBox max_mb = MAX_SYSTEM_SMBS;
  if ( ! mmb_once ) {
    const char* env = getenv("OCPI_MAX_MAILBOX");
    if( !env || (env[0] == 0)) {
      ocpiDebug("Set ""OCPI_MAX_MAILBOX"" environment variable to control max mailbox");
    }
    else {
      max_mb = (MailBox)atoi(env);
    }
    mmb_once = true;
  }
  return max_mb;
}

ezxml_t 
XferFactory::
getNode( ezxml_t tn, const char* name )
{
  ezxml_t node = tn;
  while ( node ) {
    if ( node->name ) ocpiDebug("node %s", node->name );
    if ( node->name && (strcmp( node->name, name) == 0 ) ) {
      return node;
    }
    node = ezxml_next( node );
  }
  if ( tn ) {
    if(tn->child)if((node=getNode(tn->child,name)))return node;
    if(tn->sibling)if((node=getNode(tn->sibling,name)))return node;	 
  }
  return NULL;
}

// These defaults are pre-configuration
FactoryConfig::
FactoryConfig(size_t smbSize, size_t retryCount)
  : m_SMBSize(3*1024*1024), m_retryCount(128)
{
  if (smbSize)
    m_SMBSize = smbSize;
  if (retryCount)
    m_retryCount = retryCount;
}

// Parse and default from parent
void FactoryConfig::
parse(FactoryConfig *parent, ezxml_t x) {
  if (parent)
    *this = *parent;
  m_xml = x;
  if (x) {
    const char *err;
    // Note we are not writing defaults here because they are set
    // in the constructor, and they need to be set even when there is no xml
    if ((err = OX::checkAttrs(x, "load", "SMBSize", "TxRetryCount", NULL)) ||
	(err = OX::getNumber(x, "SMBSize", &m_SMBSize, NULL, 0, false)) ||
	(err = OX::getNumber(x, "TxRetryCount", &m_retryCount, NULL, 0, false)))
      throw std::string(err); // FIXME configuration api error exception class
  }
}

bool 
XferFactory::
supportsEndPoints(
		  std::string& end_point1,
		  std::string& end_point2 )
{

  ocpiDebug("In  XferFactory::supportsEndPoints, (%s) (%s)",
         end_point1.c_str(), end_point2.c_str() );

  size_t len = strlen( getProtocol() );
  if ( end_point1.length() && end_point2.length() ) {

    if ( (strncmp( end_point1.c_str(), getProtocol(), strlen(getProtocol())) == 0 ) &&
         strncmp( end_point2.c_str(), getProtocol(), strlen(getProtocol())) == 0 ) {
      if ( (end_point1[len] != ':') ||  (end_point2[len] != ':') ) {
        return false;
      }
      return true;
    }
  }
  else if ( end_point1.length() ) {
    if ( (strncmp( end_point1.c_str(), getProtocol(), strlen(getProtocol())) == 0) ) {
      if ( (len<end_point1.length()) && (end_point1[len] != ':')  ) {
        return false;
      }
      return true;
    }
  }
  else if ( end_point2.length() ) {
    if ( (strncmp( end_point2.c_str(), getProtocol(), strlen(getProtocol())) == 0 ) ) {
      if ( (len<end_point2.length()) && (end_point2[len] != ':')  ) {
        return false;
      }
      return true;
    }
  }

  return false;
}

XferFactory::
XferFactory(const char *name)
  : OD::DriverType<XferManager,XferFactory>(name, *this)
{
}

// Destructor
XferFactory::~XferFactory()
{
  this->lock();
  
  // These are already children, so they get removec automatically
  //  for (DataTransfer::TemplateMapIter tmi = m_templates.begin(); tmi != m_templates.end(); tmi++)
  //    delete tmi->second;
}

// This default implementation is just to parse generic properties,
// defaulting from our parent
void XferFactory::
configure(ezxml_t x) {
  // parse generic attributes and default from parent
  parse(&XferManager::getFactoryManager(), x);
  // base class does device config if present
  OD::Driver::configure(x); 
}

// Internal method
EndPoint &XferFactory::
addEndPoint(const char *endPoint, const char *other, bool local, size_t size) {
  std::string info;
  if (endPoint) {
    const char *colon = strchr(endPoint, ':');
    if (colon && colon[1]) {
      colon++;
      const char *semi = strrchr(colon, ';');
      info.assign(colon, semi ? semi - colon : strlen(colon));
    } else
      endPoint = NULL;
  }
  EndPoint &ep =
    createEndPoint(info.empty() ? NULL : info.c_str(), endPoint, other, local, size, NULL);
  ep.setName();
  ocpiInfo("Dataplane endpoint %p created: %s", &ep, ep.m_name.c_str());
  ocpiAssert(m_endPoints.find(ep.m_uuid) == m_endPoints.end());
  m_endPoints[ep.m_uuid] = &ep;
  ep.addRef();
  return ep;
}
void XferFactory::
removeEndPoint(EndPoint &ep) {
#if 0
  //  if (ep.local)
  {
    assert(m_locations[ep.mailBox]);
    m_locations[ep.mailBox] = NULL; // note this might already be done.??
  }
#endif
  ocpiCheck(m_endPoints.erase(ep.m_uuid) == 1);
}

EndPoint* XferFactory::
findEndPoint(const char *end_point) {
  OU::Uuid uuid;
  EndPoint::getUuid(end_point, uuid);
  OU::SelfAutoMutex guard (this); 
  EndPoints::iterator i = m_endPoints.find(uuid);
  if (i != m_endPoints.end())
    return i->second;
  return NULL;
}
// Get, and possible create, the endpoint.  The "local" argument is not involved in the lookup,
// only in the creation.
EndPoint &XferFactory::
getEndPoint(const char *endPoint, bool local, bool cantExist, size_t size)
{ 
  assert(endPoint); // find out if anyone uses NULL
  const char *semi = strrchr(endPoint, ';');
  if (!semi || !semi[0]) // not a complete endpoint, a true allocation
    return addEndPoint(endPoint, NULL, local, size);
  OU::Uuid uuid;
  EndPoint::getUuid(endPoint, uuid);
  OU::SelfAutoMutex guard (this); 
  EndPointsIter i = m_endPoints.find(uuid);
  if (i != m_endPoints.end())
    if (cantExist)
      throw OU::Error("Local explicit endpoint already exists: '%s'", endPoint);
    else {
      i->second->addRef(); // FIXME:: this likely happens too often and thus will leak.
#if 0
      OCPI::OS::dumpStack(std::cerr);
#endif
      return *i->second;
    }
  return addEndPoint(endPoint, NULL, local, size);
}

#if 0
// The default is that the remote one doesn't matter to this allocation
std::string XferFactory::
allocateCompatibleEndpoint(const OU::PValue*params,
			   const char *,
			   MailBox mailBox, MailBox maxMailBoxes) {
  return allocateEndpoint(params, mailBox, maxMailBoxes);
}
#endif

// Return a mailbox number for a new endpoint, given an "other" endpoint that we might
// want to avoid.
MailBox XferFactory::
setNewMailBox(const char *other) {
  uint16_t mailBox = 0, myMax = getMaxMailBox();
  if (other) {
    uint16_t maxMb, otherMailBox;
    size_t size;
    EndPoint::parseEndPointString(other, &otherMailBox, &maxMb, &size);
    assert(otherMailBox);
    if (maxMb && maxMb != myMax)
      throw OU::Error("Remote end point has different number of mailbox slots (%u vs. our %u)",
		      maxMb, myMax);
    OU::SelfAutoMutex guard (this); 
    // Find an unused slot that is different from the remote one
    // mailbox might be zero so this will find the first free slot in any case
    MailBox n = 0;
#if 0
    for (n = 1; n < m_locations.size(); n++)
      if (n != mailBox && !m_locations[n])  { // if the index is different and unused..
	mailBox = n;
	break;
      }
#endif
    if (!mailBox) {
      if (n == MAX_SYSTEM_SMBS || n > myMax)
	throw OU::Error("Mailboxes for protocol %s are exhausted (all %u are used)",
			getProtocol(), myMax);
      mailBox = getNextMailBox();
    }
  } else
    mailBox = getNextMailBox();
  //  m_locations.resize(myMax + 1, NULL); // 1 origin
  //  assert(!m_locations[mailBox]);
  //  m_locations[mailBox] = &ep;
  return mailBox;

}

// The caller (transport session) doesn't have one, and wants one of its own,
// even though this means there will be multiple "local" endpoints in the same
// process
EndPoint &XferFactory::
addCompatibleLocalEndPoint(const char *remote) {
  if (!strchr(remote, ':'))
    remote = NULL;
  OU::SelfAutoMutex guard (this); 
#if 0 // we dont share local endpoints among containers (yet)
  if (!remote && !maxMb)
    for (EndPoints::iterator i = m_endPoints.begin(); i != m_endPoints.end(); i++)
      if (*i && (*i)->local)
	return *i;
#endif
  return addEndPoint(NULL, remote, true, 0);
}

void Device::
configure(ezxml_t x) {
  OD::Device::configure(x); // give the base class a chance to do generic configuration
  parse(&driverBase(), x);
}

}