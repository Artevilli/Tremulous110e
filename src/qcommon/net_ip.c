/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#if (WINVER < 0x501)
#if defined(__MINGW32__)
//wspiapi.h isn't available on MinGW, so if it's
//present it's because the end user has added it
//and we should look for it in our tree
#include "wspiapi.h"
#else
#include <wspiapi.h>
#endif
#else //WINVER >= 0x501
#if 1 //Windows2000 compatibility
#include <ws2tcpip.h>
#include <wspiapi.h>
#else
#include <ws2spi.h>
#endif
#endif //WINVER >= 0x501

typedef qint socklen_t;
#ifdef ADDRESS_FAMILY
#define sa_family_t ADDRESS_FAMILY
#else
typedef unsigned short sa_family_t;
#endif

#undef EAGAIN
#undef EADDRNOTAVAIL
#undef EAFNOSUPPORT
#undef ECONNRESET

#define EAGAIN WSAEWOULDBLOCK
#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#define ECONNRESET WSAECONNRESET
typedef u_long ioctlarg_t;
#define socketError WSAGetLastError()

static WSADATA winsockdata;
static qbool winsockInitialized = qfalse;

#else //!_WIN32

#if (MAC_OS_X_VERSION_MIN_REQUIRED == 1020)
//needed for socklen_t on OSX 10.2
#define _BSD_SOCKLEN_T_
#endif

#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#if !defined(__sun) && !defined(__sgi)
#include <ifaddrs.h>
#endif

#if defined(__sun)
#include <sys/filio.h>
#endif

typedef qint SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#define ioctlsocket ioctl
typedef qint ioctlarg_t;
#define socketError errno

#endif

typedef union
{
  struct sockaddr_in v4;
  struct sockaddr_in6 v6;
  struct sockaddr_storage ss;
}
sockaddr_t;

#pragma pack(push,1)
typedef struct
socks5_request_s
{
  uint8_t version;
  uint8_t command;
  uint8_t reserved;
  uint8_t addrtype;
  union
  {
    struct
    {
      struct in_addr addr;
      uint16_t port;
    }
    v4;
    struct
    {
      struct in6_addr addr;
      uint16_t port;
    }
    v6;
    byte buffer[64];
  }
  u;
} socks5_request_t;

typedef union
socks5_udp_request_s
{
  struct
  {
    uint8_t reserved[2];
    uint8_t fragnum;
    uint8_t addrtype;
    union
    {
      struct
      {
        struct in_addr addr;
        uint16_t port;
        qchar data[2000];
      }
      v4;
      struct
      {
        struct in6_addr addr;
        uint16_t port;
        qchar data[2000];
      }
      v6;
    }
    u;
  }
  s;
  qchar buf[1];
}
socks5_udp_request_t;
#pragma pack(pop)

static qbool usingSocks = qfalse;
static qint networkingEnabled = 0;

//silences "net_restart already defined"
static qbool networkSet = qfalse;

static cvar_t *net_enabled;

static cvar_t *net_socksEnabled;
static cvar_t *net_socksServer;
static cvar_t *net_socksPort;
static cvar_t *net_socksUsername;
static cvar_t *net_socksPassword;

static cvar_t *net_ip;
static cvar_t *net_ip6;
static cvar_t *net_port;
static cvar_t *net_port6;
static cvar_t *net_mcast6addr;
static cvar_t *net_mcast6iface;
static cvar_t *net_dropsim;

static sockaddr_t socksRelayAddr;

static SOCKET ip_socket = INVALID_SOCKET;
static SOCKET ip6_socket = INVALID_SOCKET;
static SOCKET socks_socket = INVALID_SOCKET;
static SOCKET multicast6_socket = INVALID_SOCKET;

// Keep track of currently joined multicast group.
static struct ipv6_mreq curgroup;
// And the currently bound address.
static struct sockaddr_in6 boundto;

#if !defined(IF_NAMESIZE)
#define IF_NAMESIZE 16
#endif

// use an admin local address per default so that network admins can decide on how to handle quake3 traffic.
#define NET_MULTICAST_IP6 "ff04::696f:7175:616b:6533"

#define	MAX_IPS 32

typedef struct
{
  qchar ifname[IF_NAMESIZE];
	
  netadrtype_t type;
  sa_family_t family;
  sockaddr_t addr;
  sockaddr_t netmask;
}
nip_localaddr_t;

static nip_localaddr_t localIP[MAX_IPS];
static qint numIP;

static void
NET_Restart_f(void);


//=============================================================================


/*
====================
NET_ErrorString
====================
*/
static qchar *NET_ErrorString( void ) {
#ifdef _WIN32
	//FIXME: replace with FormatMessage?
	switch( socketError ) {
		case WSAEINTR: return "WSAEINTR";
		case WSAEBADF: return "WSAEBADF";
		case WSAEACCES: return "WSAEACCES";
		case WSAEDISCON: return "WSAEDISCON";
		case WSAEFAULT: return "WSAEFAULT";
		case WSAEINVAL: return "WSAEINVAL";
		case WSAEMFILE: return "WSAEMFILE";
		case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK";
		case WSAEINPROGRESS: return "WSAEINPROGRESS";
		case WSAEALREADY: return "WSAEALREADY";
		case WSAENOTSOCK: return "WSAENOTSOCK";
		case WSAEDESTADDRREQ: return "WSAEDESTADDRREQ";
		case WSAEMSGSIZE: return "WSAEMSGSIZE";
		case WSAEPROTOTYPE: return "WSAEPROTOTYPE";
		case WSAENOPROTOOPT: return "WSAENOPROTOOPT";
		case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
		case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
		case WSAEOPNOTSUPP: return "WSAEOPNOTSUPP";
		case WSAEPFNOSUPPORT: return "WSAEPFNOSUPPORT";
		case WSAEAFNOSUPPORT: return "WSAEAFNOSUPPORT";
		case WSAEADDRINUSE: return "WSAEADDRINUSE";
		case WSAEADDRNOTAVAIL: return "WSAEADDRNOTAVAIL";
		case WSAENETDOWN: return "WSAENETDOWN";
		case WSAENETUNREACH: return "WSAENETUNREACH";
		case WSAENETRESET: return "WSAENETRESET";
		case WSAECONNABORTED: return "WSWSAECONNABORTEDAEINTR";
		case WSAECONNRESET: return "WSAECONNRESET";
		case WSAENOBUFS: return "WSAENOBUFS";
		case WSAEISCONN: return "WSAEISCONN";
		case WSAENOTCONN: return "WSAENOTCONN";
		case WSAESHUTDOWN: return "WSAESHUTDOWN";
		case WSAETOOMANYREFS: return "WSAETOOMANYREFS";
		case WSAETIMEDOUT: return "WSAETIMEDOUT";
		case WSAECONNREFUSED: return "WSAECONNREFUSED";
		case WSAELOOP: return "WSAELOOP";
		case WSAENAMETOOLONG: return "WSAENAMETOOLONG";
		case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
		case WSASYSNOTREADY: return "WSASYSNOTREADY";
		case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
		case WSANOTINITIALISED: return "WSANOTINITIALISED";
		case WSAHOST_NOT_FOUND: return "WSAHOST_NOT_FOUND";
		case WSATRY_AGAIN: return "WSATRY_AGAIN";
		case WSANO_RECOVERY: return "WSANO_RECOVERY";
		case WSANO_DATA: return "WSANO_DATA";
		default: return "NO ERROR";
	}
#else
	return strerror(socketError);
#endif
}

static void
NetadrToSockadr(const netadr_t *a, sockaddr_t *s)
{
  switch(a->type)
  {
    case
    NA_BROADCAST:
      s->v4.sin_family = AF_INET;
      s->v4.sin_port = a->port;
      s->v4.sin_addr.s_addr = INADDR_BROADCAST;
      break;

    case
    NA_IP:
      s->v4.sin_family = AF_INET;
      Com_Memcpy(&s->v4.sin_addr.s_addr, a->ipv._4, sizeof(s->v4.sin_addr.s_addr));
      s->v4.sin_port = a->port;
      break;

    case
    NA_IP6:
      s->v6.sin6_family = AF_INET6;
      Com_Memcpy(&s->v6.sin6_addr, a->ipv._6, sizeof(s->v6.sin6_addr));
      s->v6.sin6_port = a->port;
      s->v6.sin6_scope_id = a->scope_id;
      break;

    case
    NA_MULTICAST6:
      s->v6.sin6_family = AF_INET6;
      s->v6.sin6_addr = curgroup.ipv6mr_multiaddr;
      s->v6.sin6_port = a->port;
      break;

    default:
      s->v4.sin_family = AF_UNSPEC;
      s->v4.sin_port = 0;
      s->v4.sin_addr.s_addr = INADDR_ANY;
      break;
  }
}


static void SockadrToNetadr( const sockaddr_t *s, netadr_t *a ) {
	if (s->ss.ss_family == AF_INET) {
		a->type = NA_IP;
		Com_Memcpy(a->ipv._4, &s->v4.sin_addr.s_addr, sizeof(a->ipv._4));
		a->port = s->v4.sin_port;
	}
	else if(s->ss.ss_family == AF_INET6)
	{
		a->type = NA_IP6;
		Com_Memcpy(a->ipv._6, &s->v6.sin6_addr, sizeof(a->ipv._6));
		a->port = s->v6.sin6_port;
		a->scope_id = (uint32_t)s->v6.sin6_scope_id;
	}
}


static const struct addrinfo *SearchAddrInfo(struct addrinfo *hints, sa_family_t family)
{
	while(hints)
	{
		if(hints->ai_family == family)
			return hints;

		hints = hints->ai_next;
	}
	
	return NULL;
}

/*
=============
gai_error_str

wrapper over gai_strerror() to describe common error code(s)
because in-game console can't properly render non-ascii characters
on systems with locales other than US/UK
=============
*/
static const qchar *
gai_error_str(qint ecode)
{
  switch(ecode)
  {
    case
    EAI_NONAME:
      return "Unknown host.";

    default:
      return gai_strerror(ecode);
  }
}

/*
=============
Sys_StringToSockaddr
=============
*/
static qbool Sys_StringToSockaddr(const qchar *s, sockaddr_t *sadr, qint sadr_len, sa_family_t family, qint type)
{
	struct addrinfo hint;
	struct addrinfo *res = NULL;
	qint retval;
	
	Com_Memset(sadr, 0x0, sadr_len);
	Com_Memset(&hint, 0x0, sizeof(hint));

        hint.ai_family = family;
        hint.ai_socktype = SOCK_DGRAM;
	
	retval = getaddrinfo(s, NULL, &hint, &res);

	if (retval == 0)
	{
	        const struct addrinfo *search = NULL;

		if(family == AF_UNSPEC)
		{
			// Decide here and now which protocol family to use
			if((net_enabled->integer & NET_ENABLEV6) && (net_enabled->integer & NET_PRIOV6))
				search = SearchAddrInfo(res, AF_INET6);
			else
				search = SearchAddrInfo(res, AF_INET);
			
			if(!search)
			{
				if((net_enabled->integer & NET_ENABLEV6) &&
				   (net_enabled->integer & NET_PRIOV6) &&
				   (net_enabled->integer & NET_ENABLEV4))
					search = SearchAddrInfo(res, AF_INET);
				else if(net_enabled->integer & NET_ENABLEV6)
					search = SearchAddrInfo(res, AF_INET6);
			}
		}
		else
			search = SearchAddrInfo(res, family);

		if(search)
		{
			size_t addrlen = MIN(search->ai_addrlen, sadr_len);
				
			Com_Memcpy(sadr, search->ai_addr, addrlen);
			freeaddrinfo(res);
			
			return qtrue;
		}
		else
			Com_Printf("%s: Error resolving %s: No address of required type found.\n", __func__, s);
	}
	else
		Com_Printf("%s: Error resolving %s: %s\n", __func__, s, gai_error_str(retval));
	
	if (res)
		freeaddrinfo(res);
	
	return qfalse;
}

/*
=============
Sys_SockaddrToString
=============
*/
static void
Sys_SockaddrToString(qchar *dest, qint destlen, const sockaddr_t *input)
{
  socklen_t inputlen;

  if (input->ss.ss_family == AF_INET6)
  {
    inputlen = sizeof(struct sockaddr_in6);
  }
  else
  {
    inputlen = sizeof(struct sockaddr_in);
  }

  if (getnameinfo((const struct sockaddr *)input, inputlen, dest, destlen, NULL, 0, NI_NUMERICHOST) && destlen > 0)
  {
    *dest = '\0';
  }
}

/*
=============
Sys_StringToAdr
=============
*/
qbool Sys_StringToAdr( const qchar *s, netadr_t *a, netadrtype_t family ) {
	sockaddr_t sadr;
	sa_family_t fam;
	
	switch(family)
	{
		case NA_IP:
			fam = AF_INET;
		break;
		case NA_IP6:
			fam = AF_INET6;
		break;
		default:
			fam = AF_UNSPEC;
		break;
	}
	if( !Sys_StringToSockaddr(s, &sadr, sizeof(sadr), fam, SOCK_DGRAM ) ) {
		return qfalse;
	}
	
	SockadrToNetadr( &sadr, a );
	return qtrue;
}

/*
===================
NET_CompareBaseAdrMask

Compare without port, and up to the bit number given in netmask
===================
*/
qbool
NET_CompareBaseAdrMask(const netadr_t *a, const netadr_t *b, unsigned netmask)
{
  unsigned qchar cmpmask;
  unsigned qchar *addra;
  unsigned qchar *addrb;
  qint curbyte;

  if (a->type != b->type)
  {
    return qfalse;
  }

  switch(a->type)
  {
    case
    NA_LOOPBACK:
      return qtrue;

    case
    NA_IP:
      addra = (unsigned qchar *)(&a->ipv._4);
      addrb = (unsigned qchar *)(&b->ipv._4);

      if (netmask > 32)
      {
        netmask = 32;
      }

      break;

    case
    NA_IP6:
      addra = (unsigned qchar *)(&a->ipv._6);
      addrb = (unsigned qchar *)(&b->ipv._6);

      if (netmask > 128)
      {
        netmask = 128;
      }

      break;

    default:
      Com_Printf("%s: bad address type\n", __func__);
      return qfalse;
  }

  curbyte = netmask >> 3;

  if (curbyte && memcmp(addra, addrb, curbyte))
  {
    return qfalse;
  }

  netmask &= 0x07;

  if (netmask)
  {
    cmpmask = (1 << netmask) - 1;
    cmpmask <<= 8 - netmask;

    if ((addra[curbyte] & cmpmask) == (addrb[curbyte] & cmpmask))
    {
      return qtrue;
    }
  }
  else
  {
    return qtrue;
  }

  return qfalse;
}

/*
===================
NET_CompareBaseAdr

Compares without the port
===================
*/
qbool
NET_CompareBaseAdr(const netadr_t *a, const netadr_t *b)
{
  return NET_CompareBaseAdrMask(a, b, ~0U);
}

const qchar *
NET_AdrToString(const netadr_t *a)
{
  static qchar s[NET_ADDRSTRMAXLEN];

  switch(a->type)
  {
    case
    NA_LOOPBACK:
      strcpy(s, "loopback");
      break;

    case
    NA_BOT:
      strcpy(s, "bot");
      break;

    case
    NA_IP:
      Com_sprintf(s, sizeof(s), "%u.%u.%u.%u", a->ipv._4[0], a->ipv._4[1], a->ipv._4[2], a->ipv._4[3]);
      break;

    case
    NA_IP6:
      sockaddr_t sadr;

      NetadrToSockadr(a, &sadr);
      Sys_SockaddrToString(s, sizeof(s), &sadr);
      break;

    case
    NA_BAD: //invalid, unknown or non-applicable address type
      //Com_Printf("NET_AdrToString: Address type: 0.0.0.0 or ::\n");
      strcpy(s, "invalid");
      break;

    default:
      Com_Printf("NET_AdrToString: Unknown address type: %i\n", a->type);
      strcpy(s, "unknown");
      break;
  }

  return s;
}

const qchar *
NET_AdrToStringwPort(const netadr_t *a)
{
  static qchar s[NET_ADDRSTRMAXLEN_EXT];

  switch(a->type)
  {
    case
    NA_LOOPBACK:
      strcpy(s, "loopback");
      break;

    case
    NA_BOT:
      strcpy(s, "bot");
      break;

    case
    NA_IP:
      Com_sprintf(s, sizeof(s), "%s:%hu", NET_AdrToString(a), ntohs(a->port));
      break;

    case
    NA_IP6:
      Com_sprintf(s, sizeof(s), "[%s]:%hu", NET_AdrToString(a), ntohs(a->port));
      break;

    case
    NA_BAD: //invalid, unknown or non-applicable address type
      //Com_Printf("NET_AdrToString: Address type: 0.0.0.0 or ::\n");
      strcpy(s, "invalid");
      break;

    default:
      Com_Printf("NET_AdrToString: Unknown address type: %i\n", a->type);
      strcpy(s, "unknown");
      break;
  }

  return s;
}


qbool
NET_CompareAdr(const netadr_t *a, const netadr_t *b)
{
  if (!NET_CompareBaseAdr(a, b))
  {
    return qfalse;
  }

  switch(a->type)
  {
    case
    NA_IP:

    case
    NA_IP6:
      if (a->port == b->port)
      {
        return qtrue;
      }

      break;

    default:
      return qfalse;
  }

  return qfalse;
}


qbool
NET_IsLocalAddress(const netadr_t *adr)
{
  return adr->type == NA_LOOPBACK;
}

//=============================================================================

/*
==================
Sys_GetPacket

Never called by the game logic, just the system event queing
==================
*/
static qbool
Sys_GetPacket(netadr_t *net_from, msg_t *net_message, const fd_set *fdr) {
	qint 	ret;
	sockaddr_t from;
	socklen_t	fromlen;
	qint		err;
	
	if(ip_socket != INVALID_SOCKET && FD_ISSET(ip_socket, fdr))
	{
		fromlen = sizeof(from);
		ret = recvfrom( ip_socket, (void *)net_message->data, net_message->maxsize, 0, (struct sockaddr *) &from, &fromlen );
		
		if (ret == SOCKET_ERROR)
		{
			err = socketError;

			if( err != EAGAIN && err != ECONNRESET )
				Com_Printf( "Sys_GetPacket: %s\n", NET_ErrorString() );
		}
		else
		{

			Com_Memset( &from.v4.sin_zero, 0, sizeof(from.v4.sin_zero) );
		
			if ( usingSocks && memcmp( &from, &socksRelayAddr, fromlen ) == 0 ) {
				if ( ret < 10 || net_message->data[0] != 0 || net_message->data[1] != 0 || net_message->data[2] != 0 || net_message->data[3] != 1 ) {
					return qfalse;
				}
				net_from->type = NA_IP;
				net_from->ipv._4[0] = net_message->data[4];
				net_from->ipv._4[1] = net_message->data[5];
				net_from->ipv._4[2] = net_message->data[6];
				net_from->ipv._4[3] = net_message->data[7];
				net_from->port = *(uint16_t *)&net_message->data[8];
				net_message->readcount = 10;
			}
			else {
			        net_from->type = NA_BAD;
				SockadrToNetadr(&from, net_from);
				net_message->readcount = 0;
			}
		
			if( ret >= net_message->maxsize ) {
				Com_Printf( "Oversize packet from %s\n", NET_AdrToString(net_from) );
				return qfalse;
			}
			
			net_message->cursize = ret;
			return qtrue;
		}
	}
	
	if(ip6_socket != INVALID_SOCKET && FD_ISSET(ip6_socket, fdr))
	{
		fromlen = sizeof(from);
		ret = recvfrom(ip6_socket, (void *)net_message->data, net_message->maxsize, 0, (struct sockaddr *) &from, &fromlen);
		
		if (ret == SOCKET_ERROR)
		{
			err = socketError;

			if( err != EAGAIN && err != ECONNRESET )
				Com_Printf( "Sys_GetPacket: %s\n", NET_ErrorString() );
		}
		else
		{
		        net_from->type = NA_BAD;
			SockadrToNetadr(&from, net_from);
			net_message->readcount = 0;
		
			if(ret >= net_message->maxsize)
			{
				Com_Printf( "Oversize packet from %s\n", NET_AdrToString(net_from) );
				return qfalse;
			}
			
			net_message->cursize = ret;
			return qtrue;
		}
	}

	if(multicast6_socket != INVALID_SOCKET && multicast6_socket != ip6_socket && FD_ISSET(multicast6_socket, fdr))
	{
		fromlen = sizeof(from);
		ret = recvfrom(multicast6_socket, (void *)net_message->data, net_message->maxsize, 0, (struct sockaddr *) &from, &fromlen);
		
		if (ret == SOCKET_ERROR)
		{
			err = socketError;

			if( err != EAGAIN && err != ECONNRESET )
				Com_Printf( "Sys_GetPacket: %s\n", NET_ErrorString() );
		}
		else
		{
			SockadrToNetadr(&from, net_from);
			net_message->readcount = 0;
		
			if(ret >= net_message->maxsize)
			{
				Com_Printf( "Oversize packet from %s\n", NET_AdrToString(net_from) );
				return qfalse;
			}
			
			net_message->cursize = ret;
			return qtrue;
		}
	}
	
	
	return qfalse;
}

//=============================================================================

/*
==================
Sys_SendPacket
==================
*/
void Sys_SendPacket( qint length, const void *data, const netadr_t *to ) {
	qint ret = SOCKET_ERROR;
	sockaddr_t addr;

	switch(to->type)
	{
          case
          NA_BROADCAST:

          case
          NA_IP:

          case
          NA_IP6:

          case
          NA_MULTICAST6:
            break;

          default:
            Com_Error(ERR_FATAL, "Sys_SendPacket: bad address type %i", to->type);
            return;
	}

	if( (ip_socket == INVALID_SOCKET && to->type == NA_IP) ||
	        (ip_socket == INVALID_SOCKET && to->type == NA_BROADCAST) ||
		(ip6_socket == INVALID_SOCKET && to->type == NA_IP6) ||
		(ip6_socket == INVALID_SOCKET && to->type == NA_MULTICAST6) )
		return;

	if(to->type == NA_MULTICAST6 && (net_enabled->integer & NET_DISABLEMCAST))
		return;

	NetadrToSockadr( to, &addr );

	if ( usingSocks && to->type == NA_IP ) {
		socks5_udp_request_t cmd;

		if ( length <= sizeof( cmd.s.u.v4.data ) ) {
			cmd.s.reserved[0] = 0;
			cmd.s.reserved[1] = 0;
			cmd.s.fragnum = 0;  // not fragmented
			cmd.s.addrtype = 1; // address type: IPV4
			cmd.s.u.v4.addr.s_addr = addr.v4.sin_addr.s_addr;
			cmd.s.u.v4.port = addr.v4.sin_port;
			memcpy( cmd.s.u.v4.data, data, length );
			ret = sendto( ip_socket, cmd.buf, length + 10, 0, ( struct sockaddr * ) &socksRelayAddr.v4, sizeof( socksRelayAddr.v4 ) );
		}
	}
	else {
		if(addr.ss.ss_family == AF_INET)
			ret = sendto( ip_socket, data, length, 0, (struct sockaddr *) &addr, sizeof(struct sockaddr_in) );
		else if(addr.ss.ss_family == AF_INET6)
			ret = sendto( ip6_socket, data, length, 0, (struct sockaddr *) &addr, sizeof(struct sockaddr_in6) );
	}
	if( ret == SOCKET_ERROR ) {
		qint err = socketError;

		// wouldblock is silent
		if( err == EAGAIN ) {
			return;
		}

		// some PPP links do not allow broadcasts and return an error
		if( ( err == EADDRNOTAVAIL ) && ( ( to->type == NA_BROADCAST ) ) ) {
			return;
		}
		//removing floody msg
		//Com_Printf( "Sys_SendPacket: %s\n", NET_ErrorString() );
	}
}


//=============================================================================

/*
==================
Sys_IsLANAddress

LAN clients will have their rate var ignored
==================
*/
qbool Sys_IsLANAddress( const netadr_t *adr ) {
	qint		index, run, addrsize;
	qbool differed;
	const byte *comparemask;
	const byte *compareip;
	const byte *compareadr;

	if( adr->type == NA_LOOPBACK ) {
		return qtrue;
	}

	if( adr->type == NA_IP )
	{
		// RFC1918:
		// 10.0.0.0        -   10.255.255.255  (10/8 prefix)
		// 172.16.0.0      -   172.31.255.255  (172.16/12 prefix)
		// 192.168.0.0     -   192.168.255.255 (192.168/16 prefix)
		if(adr->ipv._4[0] == 10)
			return qtrue;
		if(adr->ipv._4[0] == 172 && (adr->ipv._4[1]&0xf0) == 16)
			return qtrue;
		if(adr->ipv._4[0] == 192 && adr->ipv._4[1] == 168)
			return qtrue;

		if(adr->ipv._4[0] == 127)
			return qtrue;
	}
	else if(adr->type == NA_IP6)
	{
		if(adr->ipv._6[0] == 0xfe && (adr->ipv._6[1] & 0xc0) == 0x80)
			return qtrue;
		if((adr->ipv._6[0] & 0xfe) == 0xfc)
			return qtrue;
	}
	
	// Now compare against the networks this computer is member of.
	for(index = 0; index < numIP; index++)
	{
		if(localIP[index].type == adr->type)
		{
			if(adr->type == NA_IP)
			{
				compareip = (byte *) &((struct sockaddr_in *) &localIP[index].addr)->sin_addr.s_addr;
				comparemask = (byte *) &((struct sockaddr_in *) &localIP[index].netmask)->sin_addr.s_addr;
				compareadr = adr->ipv._4;
				
				addrsize = sizeof(adr->ipv._4);
			}
			else
			{
			        //TODO? should scope_id be checked here?

				compareip = (byte *) &((struct sockaddr_in6 *) &localIP[index].addr)->sin6_addr;
				comparemask = (byte *) &((struct sockaddr_in6 *) &localIP[index].netmask)->sin6_addr;
				compareadr = adr->ipv._6;
				
				addrsize = sizeof(adr->ipv._6);
			}

			differed = qfalse;
			for(run = 0; run < addrsize; run++)
			{
				if((compareip[run] & comparemask[run]) != (compareadr[run] & comparemask[run]))
				{
					differed = qtrue;
					break;
				}
			}
			
			if(!differed)
				return qtrue;

		}
	}
	
	return qfalse;
}

/*
==================
Sys_ShowIP
==================
*/
void Sys_ShowIP(void) {
	qint i;
	qchar addrbuf[NET_ADDRSTRMAXLEN];

	for(i = 0; i < numIP; i++)
	{
		Sys_SockaddrToString(addrbuf, sizeof(addrbuf), &localIP[i].addr);

		if(localIP[i].type == NA_IP)
			Com_Printf( "IP: %s\n", addrbuf);
		else if(localIP[i].type == NA_IP6)
			Com_Printf( "IP6: %s\n", addrbuf);
	}
}


//=============================================================================


/*
====================
NET_IPSocket
====================
*/
static SOCKET NET_IPSocket( const qchar *net_interface, qint port, qint *err ) {
	SOCKET				newsocket;
	struct sockaddr_in	address;
	ioctlarg_t			_true = 1;
	qint					i = 1;

	*err = 0;

	if( net_interface ) {
		Com_Printf( "Opening IP socket: %s:%i\n", net_interface, port );
	}
	else {
		Com_Printf( "Opening IP socket: 0.0.0.0:%i\n", port );
	}

	if( ( newsocket = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP ) ) == INVALID_SOCKET ) {
		*err = socketError;
		Com_Printf( "WARNING: NET_IPSocket: socket: %s\n", NET_ErrorString() );
		return newsocket;
	}
	// make it non-blocking
	if( ioctlsocket( newsocket, FIONBIO, &_true ) == SOCKET_ERROR ) {
		Com_Printf( "WARNING: NET_IPSocket: ioctl FIONBIO: %s\n", NET_ErrorString() );
		*err = socketError;
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	// make it broadcast capable
	if( setsockopt( newsocket, SOL_SOCKET, SO_BROADCAST, (qchar *) &i, sizeof(i) ) == SOCKET_ERROR ) {
		Com_Printf( "WARNING: NET_IPSocket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString() );
	}

	if( !net_interface || !net_interface[0]) {
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
	}
	else
	{
		if(!Sys_StringToSockaddr( net_interface, (sockaddr_t *)&address, sizeof(address), AF_INET, SOCK_DGRAM))
		{
			closesocket(newsocket);
			return INVALID_SOCKET;
		}
	}

	if( port == PORT_ANY ) {
		address.sin_port = 0;
	}
	else {
		address.sin_port = htons( (short)port );
	}

	if( bind( newsocket, (void *)&address, sizeof(address) ) == SOCKET_ERROR ) {
		Com_Printf( "WARNING: NET_IPSocket: bind: %s\n", NET_ErrorString() );
		*err = socketError;
		closesocket( newsocket );
		return INVALID_SOCKET;
	}

	return newsocket;
}

/*
====================
NET_IP6Socket
====================
*/
static SOCKET NET_IP6Socket( const qchar *net_interface, qint port, struct sockaddr_in6 *bindto, qint *err ) {
	SOCKET				newsocket;
	struct sockaddr_in6	address;
	ioctlarg_t			_true = 1;

	*err = 0;

	if( net_interface )
	{
		// Print the name in brackets if there is a colon:
		if(Q_CountChar(net_interface, ':'))
			Com_Printf( "Opening IP6 socket: [%s]:%i\n", net_interface, port );
		else
			Com_Printf( "Opening IP6 socket: %s:%i\n", net_interface, port );
	}
	else
		Com_Printf( "Opening IP6 socket: [::]:%i\n", port );

	if( ( newsocket = socket( PF_INET6, SOCK_DGRAM, IPPROTO_UDP ) ) == INVALID_SOCKET ) {
		*err = socketError;
		Com_Printf( "WARNING: NET_IP6Socket: socket: %s\n", NET_ErrorString() );
		return newsocket;
	}

	// make it non-blocking
	if( ioctlsocket( newsocket, FIONBIO, &_true ) == SOCKET_ERROR ) {
		Com_Printf( "WARNING: NET_IP6Socket: ioctl FIONBIO: %s\n", NET_ErrorString() );
		*err = socketError;
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

#ifdef IPV6_V6ONLY
	{
		qint i;

		// ipv4 addresses should not be allowed to connect via this socket.
		if(setsockopt(newsocket, IPPROTO_IPV6, IPV6_V6ONLY, (qchar *) &i, sizeof(i)) == SOCKET_ERROR)
		{
			// win32 systems don't seem to support this anyways.
			Com_DPrintf("WARNING: NET_IP6Socket: setsockopt IPV6_V6ONLY: %s\n", NET_ErrorString());
		}
	}
#endif

	if( !net_interface || !net_interface[0]) {
		address.sin6_family = AF_INET6;
		address.sin6_addr = in6addr_any;
	}
	else
	{
		if(!Sys_StringToSockaddr( net_interface, (sockaddr_t *)&address, sizeof(address), AF_INET6, SOCK_DGRAM))
		{
			closesocket(newsocket);
			return INVALID_SOCKET;
		}
	}

	if( port == PORT_ANY ) {
		address.sin6_port = 0;
	}
	else {
		address.sin6_port = htons( (short)port );
	}

	if( bind( newsocket, (void *)&address, sizeof(address) ) == SOCKET_ERROR ) {
		Com_Printf( "WARNING: NET_IP6Socket: bind: %s\n", NET_ErrorString() );
		*err = socketError;
		closesocket( newsocket );
		return INVALID_SOCKET;
	}
	
	if(bindto)
		*bindto = address;

	return newsocket;
}

/*
====================
NET_SetMulticast
Set the current multicast group
====================
*/
static void NET_SetMulticast6(void)
{
	struct sockaddr_in6 addr;

	if(!*net_mcast6addr->string || !Sys_StringToSockaddr(net_mcast6addr->string, (sockaddr_t *) &addr, sizeof(addr), AF_INET6, SOCK_DGRAM))
	{
		Com_Printf("WARNING: NET_JoinMulticast6: Incorrect multicast address given, "
			   "please set cvar %s to a sane value.\n", net_mcast6addr->name);
		
		Cvar_SetIntegerValue(net_enabled->name, net_enabled->integer | NET_DISABLEMCAST);
		
		return;
	}
	
	Com_Memcpy(&curgroup.ipv6mr_multiaddr, &addr.sin6_addr, sizeof(curgroup.ipv6mr_multiaddr));

	if(*net_mcast6iface->string)
	{
#ifdef _WIN32
		curgroup.ipv6mr_interface = net_mcast6iface->integer;
#else
		curgroup.ipv6mr_interface = if_nametoindex(net_mcast6iface->string);
#endif
	}
	else
		curgroup.ipv6mr_interface = 0;
}

/*
====================
NET_JoinMulticast
Join an ipv6 multicast group
====================
*/
void NET_JoinMulticast6(void)
{
	qint err;
	
	if(ip6_socket == INVALID_SOCKET || multicast6_socket != INVALID_SOCKET || (net_enabled->integer & NET_DISABLEMCAST))
		return;
	
	if(IN6_IS_ADDR_MULTICAST(&boundto.sin6_addr) || IN6_IS_ADDR_UNSPECIFIED(&boundto.sin6_addr))
	{
		// The way the socket was bound does not prohibit receiving multi-cast packets. So we don't need to open a new one.
		multicast6_socket = ip6_socket;
	}
	else
	{
		if((multicast6_socket = NET_IP6Socket(net_mcast6addr->string, ntohs(boundto.sin6_port), NULL, &err)) == INVALID_SOCKET)
		{
			// If the OS does not support binding to multicast addresses, like WinXP, at least try with the normal file descriptor.
			multicast6_socket = ip6_socket;
		}
	}
	
	if(curgroup.ipv6mr_interface)
	{
		if (setsockopt(multicast6_socket, IPPROTO_IPV6, IPV6_MULTICAST_IF,
			       (qchar *) &curgroup.ipv6mr_interface, sizeof(curgroup.ipv6mr_interface)) < 0)
		{
        	        Com_Printf("NET_JoinMulticast6: Couldn't set scope on multicast socket: %s\n", NET_ErrorString());

        	        if(multicast6_socket != ip6_socket)
        	        {
        	        	closesocket(multicast6_socket);
        	        	multicast6_socket = INVALID_SOCKET;
        	        	return;
			}
		}
        }

        if (setsockopt(multicast6_socket, IPPROTO_IPV6, IPV6_JOIN_GROUP, (qchar *) &curgroup, sizeof(curgroup)))
        {
        	Com_Printf("NET_JoinMulticast6: Couldn't join multicast group: %s\n", NET_ErrorString());
        	
       	        if(multicast6_socket != ip6_socket)
       	        {
       	        	closesocket(multicast6_socket);
       	        	multicast6_socket = INVALID_SOCKET;
       	        	return;
		}
	}
}

void NET_LeaveMulticast6()
{
	if(multicast6_socket != INVALID_SOCKET)
	{
		if(multicast6_socket != ip6_socket)
			closesocket(multicast6_socket);
		else
			setsockopt(multicast6_socket, IPPROTO_IPV6, IPV6_LEAVE_GROUP, (qchar *) &curgroup, sizeof(curgroup));

		multicast6_socket = INVALID_SOCKET;
	}
}

/*
====================
NET_OpenSocks
====================
*/
static void NET_OpenSocks( qint port ) {
	struct sockaddr_in	address;
	qint					len;
	unsigned qchar		buf[4 + 255 * 2];
	socks5_request_t cmd;

	usingSocks = qfalse;

	Com_Printf( "Opening connection to SOCKS server.\n" );

	if ( ( socks_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ) == INVALID_SOCKET ) {
		Com_Printf( "WARNING: NET_OpenSocks: socket: %s\n", NET_ErrorString() );
		return;
	}

        if (!Sys_StringToSockaddr(net_socksServer->string, (sockaddr_t *)&address, sizeof(address), AF_INET, SOCK_STREAM))
        {
          Com_Printf("WARNING: %s failed\n", __func__);
          return;
	}

	address.sin_port = htons( net_socksPort->integer );

	if ( connect( socks_socket, (struct sockaddr *)&address, sizeof( struct sockaddr_in ) ) == SOCKET_ERROR ) {
		Com_Printf( "%s: connect: %s\n", __func__, NET_ErrorString() );
		return;
	}

	buf[0] = 5; //SOCKS version

	if ( *net_socksUsername->string || *net_socksPassword->string ) {
		// rfc1929 - send socks authentication handshake
		buf[1] = 2; // method count
		buf[2] = 0; // method id #00: no authentication
		buf[3] = 2; // method id #02: username/password
		len = 4;
	}
	else
	{
		buf[1] = 1; //method count
		buf[2] = 0; //method id #00: no authentication
		len = 3;
	}

	if ( send( socks_socket, (void *)buf, len, 0 ) == SOCKET_ERROR ) {
		Com_Printf( "%s: send: %s\n", __func__, NET_ErrorString() );
		return;
	}

	// get the response
	len = recv( socks_socket, (void *)buf, 32, 0 );
	if ( len == SOCKET_ERROR ) {
		Com_Printf( "%s: recv: %s\n", __func__, NET_ErrorString() );
		return;
	}
	if ( len != 2 || buf[0] != 5 ) {
		Com_Printf( "%s: bad auth.method response\n", __func__ );
		return;
	}
	switch( buf[1] ) {
	case 0:	// no authentication
	case 2: // username/password authentication
		break;
	default:
		Com_Printf( "%s: unsupported auth.method\n", __func__ );
		return;
	}

	// do username/password authentication if needed
	if ( buf[1] == 2 ) {
		qint		ulen;
		qint		plen;

		// build the request
		ulen = strlen( net_socksUsername->string );
		plen = strlen( net_socksPassword->string );

                if (ulen > 255)
                {
                  ulen = 255;
                }

                if (plen > 255)
                {
                  plen = 255;
                }

		buf[0] = 1;		// username/password authentication version
		buf[1] = ulen;
		if ( ulen ) {
			Com_Memcpy( &buf[2], net_socksUsername->string, ulen );
		}
		buf[2 + ulen] = plen;
		if ( plen ) {
			Com_Memcpy( &buf[3 + ulen], net_socksPassword->string, plen );
		}

		// send it
		if ( send( socks_socket, (void *)buf, 3 + ulen + plen, 0 ) == SOCKET_ERROR ) {
			Com_Printf( "%s: send: %s\n", __func__, NET_ErrorString() );
			return;
		}

		// get the response
		len = recv( socks_socket, (void *)buf, 64, 0 );
		if ( len == SOCKET_ERROR ) {
			Com_Printf( "%s: recv: %s\n", __func__, NET_ErrorString() );
			return;
		}
		if ( len != 2 || buf[0] != 1 ) {
			Com_Printf( "%s: bad auth response\n", __func__ );
			return;
		}
	}

	// send the UDP associate request
	cmd.version = 5;  // SOCKS version
	cmd.command = 3;  // UDP associate
	cmd.reserved = 0; // reserved
	cmd.addrtype = 1; // address type: IPV4
	cmd.u.v4.addr.s_addr = INADDR_ANY;
	cmd.u.v4.port = htons( port );
	if ( send( socks_socket, (void *)&cmd, 10, 0 ) == SOCKET_ERROR ) {
		Com_Printf( "%s: send: %s\n", __func__, NET_ErrorString() );
		return;
	}

	// get the response
	len = recv( socks_socket, (void *)&cmd, sizeof( cmd ), 0 );
	if ( len == SOCKET_ERROR ) {
		Com_Printf( "%s: recv: %s\n", __func__, NET_ErrorString() );
		return;
	}
	if ( len < 10 || cmd.version != 5 ) {
		Com_Printf( "%s: bad response\n", __func__ );
		return;
	}
	// check completion code
	if ( cmd.command != 0 ) {
		Com_Printf( "%s: request denied: %i\n", __func__, cmd.command );
		return;
	}
	if ( cmd.addrtype != 1 ) {
		Com_Printf( "%s: relay address is not IPV4: %i\n", __func__, cmd.addrtype );
		return;
	}

	Com_Memset(&socksRelayAddr, 0, sizeof(socksRelayAddr));

	socksRelayAddr.v4.sin_family = AF_INET;
	socksRelayAddr.v4.sin_addr.s_addr = cmd.u.v4.addr.s_addr;
	socksRelayAddr.v4.sin_port = cmd.u.v4.port;

	usingSocks = qtrue;
}


/*
=====================
NET_AddLocalAddress
=====================
*/
static void NET_AddLocalAddress(const qchar *ifname, const struct sockaddr *addr, const struct sockaddr *netmask)
{
	qint addrlen;
	sa_family_t family;
	
	// only add addresses that have all required info.
	if(!addr || !netmask || !ifname)
		return;
	
	family = addr->sa_family;

	if(numIP < MAX_IPS)
	{
		if(family == AF_INET)
		{
			addrlen = sizeof(struct sockaddr_in);
			localIP[numIP].type = NA_IP;
		}
		else if(family == AF_INET6)
		{
			addrlen = sizeof(struct sockaddr_in6);
			localIP[numIP].type = NA_IP6;
		}
		else
			return;
		
		Q_strncpyz(localIP[numIP].ifname, ifname, sizeof(localIP[numIP].ifname));
	
		localIP[numIP].family = family;

		Com_Memcpy(&localIP[numIP].addr, addr, addrlen);
		Com_Memcpy(&localIP[numIP].netmask, netmask, addrlen);
		
		numIP++;
	}
}

#if defined(__linux__) || defined(MACOSX) || defined(__BSD__)
static void NET_GetLocalAddress(void)
{
        qchar hostname[256];
	struct ifaddrs *ifap, *search;

        if (gethostname(hostname, sizeof(hostname)))
        {
          return;
        }

        Com_Printf("Hostname: %s\n", hostname);

        numIP = 0;

	if(getifaddrs(&ifap))
		Com_Printf("NET_GetLocalAddress: Unable to get list of network interfaces: %s\n", NET_ErrorString());
	else
	{
		for(search = ifap; search; search = search->ifa_next)
		{
			// Only add interfaces that are up.
			if(ifap->ifa_flags & IFF_UP)
				NET_AddLocalAddress(search->ifa_name, search->ifa_addr, search->ifa_netmask);
		}
	
		freeifaddrs(ifap);
		
		Sys_ShowIP();
	}
}
#else
static void NET_GetLocalAddress( void ) {
	qchar hostname[256];
	struct addrinfo		hint;
	struct addrinfo 	*res = NULL;
	struct addrinfo 	*search;
	struct sockaddr_in mask4;
	struct sockaddr_in6 mask6;

        numIP = 0;

	if(gethostname( hostname, sizeof(hostname) ) == SOCKET_ERROR)
		return;

	Com_Printf( "Hostname: %s\n", hostname );
	
	Com_Memset(&hint, 0, sizeof(hint));
	
	hint.ai_family = AF_UNSPEC;
	hint.ai_socktype = SOCK_DGRAM;
	
	if(getaddrinfo(hostname, NULL, &hint, &res))
 		return;

	/* On operating systems where it's more difficult to find out the configured interfaces, we'll just assume a
	 * netmask with all bits set. */
	
	Com_Memset(&mask4, 0, sizeof(mask4));
	Com_Memset(&mask6, 0, sizeof(mask6));
	mask4.sin_family = AF_INET;
	Com_Memset(&mask4.sin_addr.s_addr, 0xFF, sizeof(mask4.sin_addr.s_addr));
	mask6.sin6_family = AF_INET6;
	Com_Memset(&mask6.sin6_addr, 0xFF, sizeof(mask6.sin6_addr));

	// add all IPs from returned list.
	for(search = res; search; search = search->ai_next)
	{
		if(search->ai_family == AF_INET)
			NET_AddLocalAddress("", search->ai_addr, (struct sockaddr *) &mask4);
		else if(search->ai_family == AF_INET6)
			NET_AddLocalAddress("", search->ai_addr, (struct sockaddr *) &mask6);
	}
	
	Sys_ShowIP();
}
#endif

/*
====================
NET_OpenIP
====================
*/
static void NET_OpenIP( void ) {
	qint		i;
	qint		err;
	qint		port;
	qint		port6;

	net_ip = Cvar_GetAndDescribe("net_ip", "0.0.0.0", CVAR_LATCH, "Specifies network interface address client should use for outgoing UDP connections using IPv4.");
	net_ip6 = Cvar_GetAndDescribe("net_ip6", "::", CVAR_LATCH, "Specifies network interface address client should use for outgoing UDP connections using IPv6.");
	net_port = Cvar_GetAndDescribe("net_port", va(NULL, "%i", PORT_SERVER), CVAR_LATCH, "The network port to use (IPv4).");
	Cvar_CheckRange(net_port, "0", "65535", CV_INTEGER);
	net_port6 = Cvar_GetAndDescribe("net_port6", va(NULL, "%i", PORT_SERVER), CVAR_LATCH, "The network port to use (IPv6).");
	Cvar_CheckRange(net_port6, "0", "65535", CV_INTEGER);
	
	port = net_port->integer;
	port6 = net_port6->integer;

	NET_GetLocalAddress();

	// automatically scan for a valid port, so multiple
	// dedicated servers can be started without requiring
	// a different net_port for each one

	if(net_enabled->integer & NET_ENABLEV6)
	{
		for( i = 0 ; i < 10 ; i++ )
		{
			ip6_socket = NET_IP6Socket(net_ip6->string, port6 + i, &boundto, &err);
			if (ip6_socket != INVALID_SOCKET)
			{
				Cvar_SetIntegerValue( "net_port6", port6 + i );
				break;
			}
			else
			{
				if(err == EAFNOSUPPORT)
					break;
			}
		}
		if(ip6_socket == INVALID_SOCKET)
			Com_Printf( "WARNING: Couldn't bind to a v6 ip address.\n");
	}

	if(net_enabled->integer & NET_ENABLEV4)
	{
		for( i = 0 ; i < 10 ; i++ ) {
			ip_socket = NET_IPSocket( net_ip->string, port + i, &err );
			if (ip_socket != INVALID_SOCKET) {
				Cvar_SetIntegerValue( "net_port", port + i );

				if (net_socksEnabled->integer)
					NET_OpenSocks( port + i );

				break;
			}
			else
			{
				if(err == EAFNOSUPPORT)
					break;
			}
		}
		
		if(ip_socket == INVALID_SOCKET)
			Com_Printf( "WARNING: Couldn't bind to a v4 ip address.\n");
	}
}


//===================================================================


/*
====================
NET_GetCvars
====================
*/
static qbool NET_GetCvars( void ) {
	qbool	modified;

	modified = qfalse;

	if( net_enabled && net_enabled->modified ) {
		modified = qtrue;
	}
	
#ifdef DEDICATED
	// I want server owners to explicitly turn on ipv6 support.
	net_enabled = Cvar_Get( "net_enabled", "1", CVAR_LATCH | CVAR_ARCHIVE );
#else
	/* End users have it enabled so they can connect to ipv6-only hosts, but ipv4 will be
	 * used if available due to ping */
	net_enabled = Cvar_Get( "net_enabled", "3", CVAR_LATCH | CVAR_ARCHIVE );
#endif

	// Some cvars for configuring multicast options which facilitates scanning for servers on local subnets.
	if( net_mcast6addr && net_mcast6addr->modified ) {
		modified = qtrue;
	}
	net_mcast6addr = Cvar_GetAndDescribe("net_mcast6addr", NET_MULTICAST_IP6, CVAR_LATCH | CVAR_ARCHIVE, "Multicast address to use for scanning for IPv6 servers on the local network.");

	if( net_mcast6iface && net_mcast6iface->modified ) {
		modified = qtrue;
	}
#if defined(_WIN32)
	net_mcast6iface = Cvar_GetAndDescribe("net_mcast6iface", "0", CVAR_LATCH | CVAR_ARCHIVE, "Outgoing interface to use for scan.");
#else
	net_mcast6iface = Cvar_GetAndDescribe("net_mcast6iface", "", CVAR_LATCH | CVAR_ARCHIVE, "Outgoing interface to use for scan.");
#endif
	if( net_socksEnabled && net_socksEnabled->modified ) {
		modified = qtrue;
	}
	net_socksEnabled = Cvar_GetAndDescribe("net_socksEnabled", "0", CVAR_LATCH | CVAR_ARCHIVE, "Toggle the use of network socks 5 protocol enabling firewall access (can only be set at initialization time from the OS command line).");
	Cvar_CheckRange(net_socksEnabled, "0", "1", CV_INTEGER);

	if( net_socksServer && net_socksServer->modified ) {
		modified = qtrue;
	}
	net_socksServer = Cvar_GetAndDescribe("net_socksServer", "", CVAR_LATCH | CVAR_ARCHIVE, "Set the address (name or IP number) of the SOCKS server (firewall machine), NOT a Q3ATEST server (can only be set at initialization time from the OS command line).");

	if( net_socksPort && net_socksPort->modified ) {
		modified = qtrue;
	}
	net_socksPort = Cvar_GetAndDescribe("net_socksPort", "1080", CVAR_LATCH | CVAR_ARCHIVE, "Set proxy and/or firewall port, default is 1080 (can only be set at initialization time from the OS command line).");
	Cvar_CheckRange(net_socksPort, "0", "65535", CV_INTEGER);

	if( net_socksUsername && net_socksUsername->modified ) {
		modified = qtrue;
	}
	net_socksUsername = Cvar_GetAndDescribe("net_socksUsername", "", CVAR_LATCH | CVAR_ARCHIVE, "Variable holds username for socks firewall. Supports authentication and username/password authentication method (RFC-1929). It does NOT support GSS-API method (RFC-1961) authentication (can only be set at initialization time from the OS command line).");

	if( net_socksPassword && net_socksPassword->modified ) {
		modified = qtrue;
	}
	net_socksPassword = Cvar_GetAndDescribe("net_socksPassword", "", CVAR_LATCH | CVAR_ARCHIVE, "Variable holds password for socks firewall access. Supports no authentication and username/password authentication method (RFC-1929). It does NOT support GSS-API method (RFC-1961) authentication (can only be set at initialization time from the OS command line).");
	net_dropsim = Cvar_GetAndDescribe("net_dropsim", "", CVAR_TEMP, "Simulated packet drops.");


	return modified;
}


/*
====================
NET_Config
====================
*/
void NET_Config( qbool enableNetworking ) {
	qbool	modified;
	qbool	stop;
	qbool	start;

	// get any latched changes to cvars
	modified = NET_GetCvars();

	if( !net_enabled->integer ) {
		enableNetworking = qfalse;
	}

	// if enable state is the same and no cvars were modified, we have nothing to do
	if( enableNetworking == networkingEnabled && !modified ) {
		return;
	}

	if( enableNetworking == networkingEnabled ) {
		if( enableNetworking ) {
			stop = qtrue;
			start = qtrue;
		}
		else {
			stop = qfalse;
			start = qfalse;
		}
	}
	else {
		if( enableNetworking ) {
			stop = qfalse;
			start = qtrue;
		}
		else {
			stop = qtrue;
			start = qfalse;
		}
		networkingEnabled = enableNetworking;
	}

	if( stop ) {
		if ( ip_socket != INVALID_SOCKET ) {
			closesocket( ip_socket );
			ip_socket = INVALID_SOCKET;
		}

		if(multicast6_socket != INVALID_SOCKET)
		{
			if(multicast6_socket != ip6_socket)
				closesocket(multicast6_socket);
				
			multicast6_socket = INVALID_SOCKET;
		}

		if ( ip6_socket != INVALID_SOCKET ) {
			closesocket( ip6_socket );
			ip6_socket = INVALID_SOCKET;
		}

		if ( socks_socket != INVALID_SOCKET ) {
			closesocket( socks_socket );
			socks_socket = INVALID_SOCKET;
		}
		
	}

	if( start )
	{
		if (net_enabled->integer)
		{
			NET_OpenIP();
			NET_SetMulticast6();
		}
	}
}


/*
====================
NET_Init
====================
*/
void NET_Init( void ) {
#ifdef _WIN32
	qint		r;

	r = WSAStartup( MAKEWORD( 2, 0 ), &winsockdata );
	if( r ) {
		Com_Printf(S_COLOR_YELLOW "WARNING: Winsock initialization failed, returned %d\n", r);
		return;
	}

	winsockInitialized = qtrue;
	Com_DPrintf("Winsock Initialized\n");
#endif

	// this is really just to get the cvars registered
	NET_GetCvars();

	NET_Config( qtrue );

        if (!networkSet)
        {
          Cmd_AddCommand("net_restart", NET_Restart_f);
          networkSet = qtrue;
        }
}


/*
====================
NET_Shutdown
====================
*/
void NET_Shutdown( void ) {
	if ( !networkingEnabled ) {
		return;
	}

	NET_Config( qfalse );

#ifdef _WIN32
	WSACleanup();
	winsockInitialized = qfalse;
#endif

        if (networkSet)
        {
          Cmd_RemoveCommand("net_restart");
          networkSet = qfalse;
        }
}

/*
====================
NET_Event

Called from NET_Sleep which uses select() to determine which sockets have seen action.
====================
*/
static void
NET_Event(const fd_set *fdr)
{
  byte bufData[MAX_MSGLEN_BUF];
  netadr_t from;
  msg_t netmsg;

  while(1)
  {
    MSG_Init(&netmsg, bufData, MAX_MSGLEN);

    if (Sys_GetPacket(&from, &netmsg, fdr))
    {
      if (net_dropsim->value > 0.0f && net_dropsim->value <= 100.0f)
      {
        //com_dropsim->value percent of incoming packets dropped
        if (rand() < (qint)(((double)RAND_MAX) / 100.0f * (double)net_dropsim->value))
        {
          continue; //drop this packet
        }
      }

#if !defined(DEDICATED)
      Com_RunAndTimeServerPacket(&from, &netmsg);
#else
      if (com_sv_running->integer || com_dedicated->integer)
      {
        Com_RunAndTimeServerPacket(&from, &netmsg);
      }
      else
      {
        CL_PacketEvent(&from, &netmsg);
      }
#endif
    }
    else
    {
      break;
    }
  }
}

/*
====================
NET_Sleep

Sleeps msec or until something happens on the network

Returns qfalse on network event or qtrue in all other cases
====================
*/
qbool
NET_Sleep(qint timeout)
{
  struct timeval tv;
  fd_set fdr;
  qint retval;
  SOCKET highestfd = INVALID_SOCKET;

  if (timeout < 0)
  {
    timeout = 0;
  }

  FD_ZERO(&fdr);

  if (ip_socket != INVALID_SOCKET)
  {
    FD_SET(ip_socket, &fdr);
    highestfd = ip_socket;
  }

  if (ip6_socket != INVALID_SOCKET)
  {
    FD_SET(ip6_socket, &fdr);

    if (highestfd == INVALID_SOCKET || ip6_socket > highestfd)
    {
      highestfd = ip6_socket;
    }
  }

  if (highestfd == INVALID_SOCKET)
  {
#if defined(_WIN32)
    // windowsain't happy when select is called without valid FDs
    Sleep(timeout / 1000); //SleepEX(timeout / 1000, 0);
    return qtrue;
#else
    struct timespec req;
    req.tv_sec = timeout / 1000000;
    req.tv_nsec = ( timeout % 1000000 ) * 1000;
    nanosleep(&req, NULL);
    return qtrue;
#endif
  }

  tv.tv_sec = timeout / 1000000;
  tv.tv_usec = timeout - tv.tv_sec * 1000000;

  retval = select(highestfd + 1, &fdr, NULL, NULL, &tv);

  if (retval > 0)
  {
    NET_Event(&fdr);
    return qfalse;
  }

  if (retval == SOCKET_ERROR)
  {
#if !defined(_WIN32)
    if (socketError != EINTR)
    {
#endif
      Com_Printf(S_COLOR_YELLOW "Warning: select() syscall failed: %s\n", NET_ErrorString());
#if !defined(_WIN32)
    }
#endif
  }

  return qtrue;
}


/*
====================
NET_Restart_f
====================
*/
static void NET_Restart_f( void ) {
	NET_Config( networkingEnabled );
}
