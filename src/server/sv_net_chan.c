/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2006 Tim Angus

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
#include "server.h"

/*
==============
SV_Netchan_Encode

//first four bytes of the data are always:
long reliableAcknowledge;
==============
*/
static void
SV_Netchan_Encode(client_t *client, msg_t *msg, const qchar *commandString)
{
  long i;
  long index;
  byte key;
  byte *string;
  qint srdc;
  qint sbit;
  qbool soob;

  if (msg->cursize < SV_ENCODE_START)
  {
    return;
  }

  srdc = msg->readcount;
  sbit = msg->bit;
  soob = msg->oob;
        
  msg->bit = 0;
  msg->readcount = 0;
  msg->oob = qfalse;
        
  MSG_ReadLong(msg);

  msg->oob = soob;
  msg->bit = sbit;
  msg->readcount = srdc;
        
  string = (byte *)commandString;
  index = 0;

  //xor the client challenge with the netchan sequence number
  key = client->challenge ^ client->netchan.outgoingSequence;

  for(i = SV_ENCODE_START;i < msg->cursize;i++)
  {
    //modify the key with the last received and with this message acknowledged client command
    if (!string[index])
    {
      index = 0;
    }

    if (string[index] > 127 || string[index] == '%')
    {
      key ^= '.' << (i & 1);
    }
    else
    {
      key ^= string[index] << (i & 1);
    }

    index++;

    //encode the data with this key
    *(msg->data + i) = *(msg->data + i) ^ key;
  }
}

/*
==============
SV_Netchan_Decode

//first 12 bytes of the data are always:
long serverId;
long messageAcknowledge;
long reliableAcknowledge;
==============
*/
static void
SV_Netchan_Decode(client_t *client, msg_t *msg)
{
  qint serverId;
  qint messageAcknowledge;
  qint reliableAcknowledge;
  qint i;
  qint index;
  qint srdc;
  qint sbit;
  qbool soob;
  byte key;
  byte *string;

  srdc = msg->readcount;
  sbit = msg->bit;
  soob = msg->oob;
        
  msg->oob = 0;
        
  serverId = MSG_ReadLong(msg);
  messageAcknowledge = MSG_ReadLong(msg);
  reliableAcknowledge = MSG_ReadLong(msg);

  msg->oob = soob;
  msg->bit = sbit;
  msg->readcount = srdc;
        
  string = (byte *)client->reliableCommands[reliableAcknowledge & (MAX_RELIABLE_COMMANDS-1)];
  index = 0;
  //
  key = client->challenge ^ serverId ^ messageAcknowledge;

  for(i = msg->readcount + SV_DECODE_START;i < msg->cursize;i++)
  {
    //modify the key with the last sent and acknowledged server command
    if (!string[index])
    {
      index = 0;
    }

    if (string[index] > 127 || string[index] == '%')
    {
      key ^= '.' << (i & 1);
    }
    else
    {
      key ^= string[index] << (i & 1);
    }

    index++;

    //decode the data with this key
    *(msg->data + i) = *(msg->data + i) ^ key;
  }
}

/*
=================
SV_NetchanFreeQueue
=================
*/
void
SV_Netchan_ClearQueue(client_t *client)
{
  netchan_buffer_t *netbuf;
  netchan_buffer_t *next;

  for(netbuf = client->netchan_start_queue;netbuf;netbuf = next)
  {
    next = netbuf->next;
    Z_Free(netbuf);
  }

  client->netchan_start_queue = NULL;
  client->netchan_end_queue = &client->netchan_start_queue;
}

/*
=================
SV_Netchan_TransmitNextInQueue
=================
*/
void
SV_Netchan_TransmitNextInQueue(client_t *client)
{
  netchan_buffer_t *netbuf;

  while(!client->netchan.unsentFragments && client->netchan_start_queue)
  {
    Com_DPrintf("#462 Netchan_TransmitNextFragment: popping a queued message for transmit\n");
    netbuf = client->netchan_start_queue;
    SV_Netchan_Encode(client, &netbuf->msg, netbuf->lastClientCommandString);
    Netchan_Transmit(&client->netchan, netbuf->msg.cursize, netbuf->msg.data);

    //pop from queue
    client->netchan_start_queue = netbuf->next;

    if (!client->netchan_start_queue)
    {
      Com_DPrintf("#462 Netchan_TransmitNextFragment: emptied queue\n");
      client->netchan_end_queue = &client->netchan_start_queue;
    }
    else
    {
      Com_DPrintf("#462 Netchan_TransmitNextFragment: remaining queued message\n");
    }

    Z_Free(netbuf);
  }
}

/*
=================
SV_Netchan_TransmitNextFragment
=================
*/
qint
SV_Netchan_TransmitNextFragment(client_t *client)
{
  if (client->netchan.unsentFragments)
  {
    Netchan_TransmitNextFragment(&client->netchan);
    return SV_RateMsec(client);
  }
  else if (client->netchan_start_queue)
  {
    SV_Netchan_TransmitNextInQueue(client);
    return SV_RateMsec(client);
  }

  return -1;
}


/*
===============
SV_Netchan_Transmit
TTimo
https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=462
if there are some unsent fragments (which may happen if the snapshots
and the gamestate are fragmenting, and collide on send for instance)
then buffer them and make sure they get sent in correct order
================
*/

void
SV_Netchan_Transmit(client_t *client, msg_t *msg) //qint length, const byte *data ) {
{
  MSG_WriteByte(msg, svc_EOF);

  if (client->netchan.unsentFragments || client->netchan_start_queue)
  {
    netchan_buffer_t *netbuf;
    const size_t netSize = sizeof(netchan_buffer_t);
    const size_t cmdLen = strlen(client->lastClientCommandString) + 1;

    Com_DPrintf("#462 SV_Netchan_Transmit: unsent fragments, stacked\n");
    netbuf = (netchan_buffer_t *)Z_Malloc(netSize + msg->cursize + cmdLen);
    netbuf->msgBuffer = ((byte *)netbuf) + netSize;
    netbuf->lastClientCommandString = (qchar *)(((byte *)netbuf) + (netSize + msg->cursize));
    netbuf->lastClientCommandString[0] = '\0';

    //store the msg, we can't store it encoded, as the encoding depends on stuff we still have to finish sending
    MSG_Copy(&netbuf->msg, netbuf->msgBuffer, msg->cursize, msg);
    Q_strncpyz(netbuf->lastClientCommandString, client->lastClientCommandString, cmdLen);
    netbuf->next = NULL;

    //insert it in the queue, the message will be encoded and sent later
    *client->netchan_end_queue = netbuf;
    client->netchan_end_queue = &(*client->netchan_end_queue)->next;
  }
  else
  {
    SV_Netchan_Encode(client, msg, client->lastClientCommandString);
    Netchan_Transmit(&client->netchan, msg->cursize, msg->data);
  }
}

/*
=================
Netchan_SV_Process
=================
*/
qbool
SV_Netchan_Process(client_t *client, msg_t *msg)
{
  const qbool ret = Netchan_Process(&client->netchan, msg);

  if (!ret)
  {
    return qfalse;
  }

  SV_Netchan_Decode(client, msg);
  return qtrue;
}

