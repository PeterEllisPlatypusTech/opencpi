
/*
 *  Copyright (c) Mercury Federal Systems, Inc., Arlington VA., 2009-2010
 *
 *    Mercury Federal Systems, Incorporated
 *    1901 South Bell Street
 *    Suite 402
 *    Arlington, Virginia 22202
 *    United States of America
 *    Telephone 703-413-0781
 *    FAX 703-413-0784
 *
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


// -*- c++ -*-

#ifndef OCPIOSETHER_H__
#define OCPIOSETHER_H__

#include <stdint.h>
#include <memory.h>
#include <stdio.h>
#include <string>

#include "OcpiOsIovec.h"
#include "KernelDriver.h"
// Ethernet support

namespace OCPI {
  namespace OS {
    namespace Ether {
      typedef uint16_t Type;
      // Note this value includes the ethertype bytes
      const unsigned MaxPacketSize = (1518 - 4 - 12);
      // This is worth inlining since addresses are processed at runtime.
      extern bool haveDriver();
      class Address {
      public:
	static const unsigned s_size = 6;
      private:
	union {
	  uint8_t m_addr[s_size];
	  uint64_t m_addr64;
	};
	char m_pretty[s_size*3+1];
	bool m_error, m_broadcast;
      public:
	void setString(const char *x);
	void set(const void *x);
	inline explicit Address(const unsigned char *m = NULL) {
	  set(m);
	}
	inline explicit Address(const char *m) {
	  setString(m);
	}
	Address(const Address &addr) {
	  set(addr.addr());
	}
	inline const uint8_t *addr() const { return m_addr; }
	inline const uint64_t addr64() const { return m_addr64; }
	const char *pretty();
	inline bool error() const { return m_error; }
	inline bool broadcast() const { return m_broadcast; }
	inline bool operator==(const Address &rhs) {
	  return memcmp(addr(), rhs.addr(), s_size) == 0;
	}
	inline bool operator!=(const Address &rhs) {
	  return memcmp(addr(), rhs.addr(), s_size) != 0;
	}
	static Address s_broadcast;
	class Compare {
	public:
	  bool operator() (const Address &a, const Address &b) const {
	    return memcmp(a.addr(), b.addr(), s_size) < 0;
	  }
	};
      };
      struct Interface;
      struct Header {
	uint8_t destination[Address::s_size];
	uint8_t source[Address::s_size];
	Type type;
      };
      struct Packet {
	uint8_t destination[Address::s_size];
	uint8_t source[Address::s_size];
	uint8_t payload[MaxPacketSize];
      };
      // A socket that sends and receives L2 layer poackets.
      // Note that receives are done into internal buffers and it is assumed that
      // the packet buffer can be reused on the next receive call.
      // This socket is for communicating to a simple adress,
      // but since L2 sockets are only bound to an ethertype, we receive
      // from everyone.  Hence even though there is one of these sockets per destination,
      // there may only be one underlying socket, depending on the OS/implementation.
      class Socket {
	unsigned m_ifIndex;
	Address m_ifAddr;
	Type m_type;
	//	Address m_addr; // not a reference
	int m_fd;
	unsigned m_timeout;
	ocpi_role_t m_role;
	uint16_t m_endpoint;
      public:
	Socket(Interface &, ocpi_role_t role, Address *remote, uint16_t endpoint, std::string &error);
	~Socket();
	inline Address &ifAddr() { return m_ifAddr; }
	// These return false if error or timeout
	// Receive directly into packet buffer.
	bool receive(Packet &, unsigned &payLoadLength, unsigned timeoutms,
		     Address &addr, std::string &error);
	bool receive(uint8_t *buf, unsigned &offset, unsigned &length, unsigned timeoutms,
		     Address &addr, std::string &error);
	// Send, and fill out the addressing first (hence packet not const).
	// If no "addr", then use address in socket.
	// Return false if error or timeout
	bool send(Packet &, unsigned payloadLength, Address &addr, unsigned timeoutms, std::string &error);
	bool send(IOVec *, unsigned vecLen, Address &addr, unsigned timeoutms, std::string &error);
      };
      // The interface object has no dependencies on the scanner.
      struct Interface {
	Interface();
	Interface(const char *name, std::string &error);
	unsigned index;
	std::string name;
	Address addr;
	bool up, connected;
      };
      class IfScanner {
	uint64_t m_opaque[4];
      public:
	// Initialize the scanner
	// err is set if there is an error
	IfScanner(std::string &err);
	~IfScanner();
	// Get the basic information about the current interface, without "opening" it for I/O
	bool getNext(Interface &i, std::string &err, const char *only = NULL);
      };
    }
  }
}

#endif