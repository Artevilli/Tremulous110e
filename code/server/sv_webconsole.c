/*
 ===========================================================================
 Copyright (C) 1998 Steve Yeager
 Copyright (C) 2006 Cheyenne Spring Barnes
 Copyright (C) 2008 Robert Beckebans <trebor_7@users.sourceforge.net>

 This file is part of XreaL source code.

 XreaL source code is free software; you can redistribute it
 and/or modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; either version 2 of the License,
 or (at your option) any later version.

 XreaL source code is distributed in the hope that it will be
 useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with XreaL source code; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ===========================================================================
 */

#include "server.h"
#if defined(USE_WEBCONSOLE)
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

qint sv_webconsoleSocket;
qbool sv_webconsoleConnected;
static cvar_t *sv_webconsolePassword;
static cvar_t *sv_webconsoleHost;
static cvar_t *sv_webconsolePort;
static cvar_t *sv_webconsoleServer;

/* Closes webconsole socket */
void
sv_webconsole_close(qint *sockfd)
{
  close(*sockfd);
}

/* Connects the websocket */
qbool
sv_webconsole_connect(qint *sockfd)
{
  qint port;
  const qchar *host;
  qchar message[32];
  struct sockaddr_in server;
  const struct hostent *he;

  //get webconsole vars
  sv_webconsolePassword = Cvar_GetAndDescribe("sv_webconsolePassword", "", CVAR_ARCHIVE, "Sets the password for the web console.");
  sv_webconsoleHost = Cvar_GetAndDescribe("sv_webconsoleHost", "127.0.0.1", CVAR_ARCHIVE, "Sets the host to use for the web console.");
  sv_webconsolePort = Cvar_GetAndDescribe("sv_webconsolePort", "5624", CVAR_ARCHIVE, "Sets the port to use for the web console.");
  sv_webconsoleServer = Cvar_GetAndDescribe("sv_webconsoleServer", "", CVAR_ARCHIVE, "Sets the server to use for the web console.");

  host = sv_webconsoleHost->string;
  port = sv_webconsolePort->integer;

  //create socket
  *sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (*sockfd == -1)
  {
    return qfalse;
  }

  //resolve host
  if ((he = gethostbyname(host)) == NULL)
  {
    return qfalse;
  }

  //build struct
  Com_Memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);
  server.sin_family = AF_INET;
  server.sin_port = htons(port);

  //attempt to connect the socket
  if (connect(*sockfd, (struct sockaddr *)&server, sizeof(server)))
  {
    return qfalse;
  }

  //non-blocking
  fcntl(*sockfd, F_SETFL, O_NONBLOCK);

  //send initial message
  Com_sprintf(message, sizeof(message), "server:%s:%s\n", sv_webconsoleServer->string, sv_webconsolePassword->string);

  if (send(*sockfd, message, strlen(message), MSG_NOSIGNAL) == -1)
  {
    close(*sockfd);
    return qfalse;
  }

  return qtrue;
}

/* Sends a message over webconsole socket.
 * Attempts to create a socket if necessary.
 */
void
sv_webconsole_send(qint *sockfd, qchar *message, qbool *connected)
{
  //attempt to connect socket if disconnected
  if (*connected == qfalse)
  {
    //first try to close it, then connect it
    close(*sockfd);
    *connected = sv_webconsole_connect(sockfd);
  }

  //if now connected
  if (*connected == qtrue)
  {
    //make sure message sends successfully
    if (send(*sockfd, message, strlen(message), MSG_NOSIGNAL) == -1)
    {
      *connected = qfalse;
      close(*sockfd);
    }
  }
}

/* Attempts to read a message from the webconsole
 * Returns NULL if nothing was successfully read
 */
const qchar *
sv_webconsole_read(qint *sockfd, qbool *connected)
{
  //attempt to connect socket if disconnected
  if (*connected == qfalse)
  {
    //first try to close it, then connect it
    close(*sockfd);
    *connected = sv_webconsole_connect(sockfd);
  }

  //if now connected
  if (*connected == qtrue)
  {
    //we can receive 1kb packets
    qint bufsize = 1024;
    qchar *receive = Z_Malloc(bufsize + 1);
    qint response;

    response = recv(*sockfd, receive, bufsize, MSG_NOSIGNAL);

    if (response == -1) //no response ready on non-blocking socket (hopefully)
    {
      return NULL;
    }
    else if (response == 0) //socket closed remotely
    {
      close(*sockfd);
      *connected = qfalse;
    }
    else
    {
      Com_Printf("Received(%d): %s\n", *connected == qtrue ? 1:0, receive);
      return receive;
    }
  }

  //case where the call failed
  return NULL;
}
#endif
