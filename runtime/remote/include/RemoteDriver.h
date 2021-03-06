/*
 * This file is protected by Copyright. Please refer to the COPYRIGHT file
 * distributed with this source distribution.
 *
 * This file is part of OpenCPI <http://www.opencpi.org>
 *
 * OpenCPI is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * OpenCPI is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _REMOTE_DRIVER_H_
#define _REMOTE_DRIVER_H_
#include <string>
#include "ContainerManager.h"
namespace OCPI {
  namespace Remote {
    class Container;
    class Client;
    extern const char *remote;
    class Driver : public OCPI::Container::DriverBase<Driver, Container, remote>,
		   public OCPI::Util::Parent<Client> {
      static pthread_key_t s_threadKey;
    public:
      Driver() throw();
    private:
      ~Driver() throw();
      OCPI::Container::Container *
      probeContainer(const char *which, std::string &/*error*/,
		     const OCPI::API::PValue */*params*/);
      bool
      trySocket(std::set<std::string> &servers, OCPI::OS::Ether::Interface &ifc,
		OCPI::OS::Ether::Socket &s, OCPI::OS::Ether::Address &addr, bool discovery,
		const char **exclude, bool verbose, std::string &error);
      unsigned
      tryIface(std::set<std::string> &servers, OCPI::OS::Ether::Interface &ifc,
	       OCPI::OS::Ether::Address &devAddr, const char **exclude, bool discovery,
	       bool verbose, std::string &error);
      bool
      probeServer(const char *server, bool /*verbose*/, const char **exclude, char *containers,
		  bool discoveryOnly, std::string &error);
    public:
      unsigned
      search(const OCPI::API::PValue* props, const char **exclude, bool discoveryOnly);
      // This is the API hook for adding servers, which calls the driver method above
      static bool
      probeServer(const char *server, bool verbose, const char **exclude, bool discovery,
		  std::string &error);
    };
  }
}
#endif
