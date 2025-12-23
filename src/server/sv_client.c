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
//sv_client.c -- server code for dealing with clients

#include "server.h"

//Chey: putting these here so gcc knows what it is
static const void
SV_CloseDownload(client_t *cl);

#if !defined(STATELESS_CHALLENGES_VERSION_ONE)
//
//Server-side Stateless Challenges
//backported from https://github.com/JACoders/OpenJK/pull/832
//

#define TS_SHIFT 14 //~16 seconds to reply to the challenge

/*
=================
SV_CreateChallenge

Create an unforgeable, temporal challenge for the given client address
=================
*/
static qint
SV_CreateChallenge(qint timestamp, const netadr_t *from)
{
  qint challenge;

  //Create an unforgeable, temporal challenge for this client using HMAC(secretKey, clientParams + timestamp)
  //Use first 4 bytes of the HMAC digest as a qint (client only deals with numeric challenges)
  //The most-significant bit stores whether the timestamp is odd or even. This lets later verification code handle the
  //case where the engine timestamp is incremented between the time this challenge is sent and the client replies.
  challenge = Com_MD5Addr(from, timestamp);
  challenge &= 0x7FFFFFFF;
  challenge |= (unsigned)(timestamp & 0x1) << 31;

  return challenge;
}

/*
=================
SV_CreateChallenge

Verify a challenge received by the client matches the expected challenge
=================
*/
static qbool
SV_VerifyChallenge(qint receivedChallenge, const netadr_t *from)
{
  qint currentTimestamp = svs.time >> TS_SHIFT;
  qint currentPeriod = currentTimestamp & 0x1;

  //Use the current timestamp for verification if the client period matches the client challenge's period.
  //Otherwise, use the previous timestamp in case the current timestamp incremented in the time between the
  //client being sent a challenge and the client's reply that's being verified now.
  qint challengePeriod = ((unsigned)receivedChallenge >> 31) & 0x1;
  qint challengeTimestamp = currentTimestamp - (currentPeriod ^ challengePeriod);

  qint expectedChallenge = SV_CreateChallenge(challengeTimestamp, from);

  return (receivedChallenge == expectedChallenge) ? qtrue:qfalse;
}

/*
=================
SV_InitChallenger
=================
*/
void
SV_InitChallenger(void)
{
  Com_MD5Init();
}
#endif

/*
=================
SV_GetChallenge

A "getchallenge" OOB command has been received
Returns a challenge number that can be used
in a subsequent connectResponse command.
We do this to prevent denial of service attacks that
flood the server with invalid connection IPs.  With a
challenge, they must give a valid IP address.

If we are authorizing, a challenge request will cause a packet
to be sent to the authorize server.

When an authorizeip is returned, a challenge response will be
sent to that ip.
=================
*/
const void
SV_GetChallenge(const netadr_t *from)
{
#if defined(INCLUDE_LEGACY_CHALLENGE)
  if (sv_legacyChallenge->integer)
  {
    qint i;
    qint oldest;
    qint oldestTime;
    qint oldestClientTime;
    qint clientChallenge;
    challenge_t *challenge;
    qbool wasfound;

#if !defined(DEDICATED)
    return;
#endif

    oldest = 0;
    oldestTime = 0x7fffffff;
    oldestClientTime = 0x7fffffff;
    wasfound = qfalse;
    clientChallenge = Q_atoi(Cmd_Argv(1));

    //see if we already have a challenge for this ip
    for(i = 0;i < MAX_CHALLENGES;i++)
    {
      challenge = &svs.challenges[i];

      if (!challenge->connected && NET_CompareAdr(from, &challenge->adr))
      {
        wasfound = qtrue;

        if (challenge->time < oldestClientTime)
        {
          oldestClientTime = challenge->time;
        }
      }

      if (wasfound && i >= MAX_CHALLENGES_MULTI)
      {
        i = MAX_CHALLENGES;
        break;
      }

      if (challenge->time < oldestTime)
      {
        oldestTime = challenge->time;
        oldest = i;
      }
    }

    if (i == MAX_CHALLENGES)
    {
      //this is the first time this client has asked for a challenge
      challenge = &svs.challenges[oldest];
      challenge->clientChallenge = clientChallenge;
      challenge->adr = *from;
      challenge->pingTime = -1;
      challenge->firstTime = svs.time;
      challenge->firstPing = 0;
      challenge->time = svs.time;
      challenge->connected = qfalse;
      i = oldest;
    }

    //always generate a new challenge number, so the client cannot circumvent sv_maxping
    challenge->challenge = ((rand() << 16) ^ rand()) ^ svs.time;
    challenge->wasrefused = qfalse;
    challenge->time = svs.time;

    //send the challengeResponse
    //if (svs.time - challenge->firstTime > AUTHORIZE_TIMEOUT)
    //{
      challenge->pingTime = svs.time;
      NET_OutOfBandPrint(NS_SERVER, from, "challengeResponse %i %i", challenge->challenge, PROTOCOL_VERSION);

      //return;
    //}
  }
  else
  {
#endif
#if !defined(DEDICATED)
    return;
#endif

    //create a unique challenge for this client without storing state on server
#if defined(STATELESS_CHALLENGES_VERSION_ONE)
    const qint challenge = SV_CreateChallenge(from);
#else
    const qint challenge = SV_CreateChallenge(svs.time >> TS_SHIFT, from);
#endif

    if (Cmd_Argc() < 2)
    {
      //legacy client query so do not send not needed info
      NET_OutOfBandPrint(NS_SERVER, from, "challengeResponse %i", challenge);
    }
    else
    {
      //grab the client's challenge to echo back if given
      NET_OutOfBandPrint(NS_SERVER, from, "challengeResponse %i %i", challenge, PROTOCOL_VERSION);
    }
#if defined(INCLUDE_LEGACY_CHALLENGE)
  }
#endif
}

/*
==================
SV_isValidGUID
==================
*/
static const qbool
SV_isValidGUID(const netadr_t *from, const qchar *userinfo)
{
  qint i;
  client_t *cl;
  qchar guid[MAX_GUID_LENGTH + 1] = {0};

  Q_strncpyz(guid, Info_ValueForKey(userinfo, "cl_guid"), sizeof(guid));

  //Chey: FIXME: this is still a bit of a problem... ideally i want stock 1.1 to have a special flag and have the server actually detect that it is stock 1.1 rather than just blindly accepting blank guid values, but this will do for now
  if (sv_guidCheckAllowStock->integer)
  {
    if (guid[0] == '\0')
    {
      return qtrue;
    }
  }

  //dont allow empty, unknown, or no guid
  if (strlen(guid) < MAX_GUID_LENGTH)
  {
    NET_OutOfBandPrint(NS_SERVER, from, "print\nBAD GUID: Invalid qkey.\n");
    Com_DPrintf("Client rejected for bad sized qkey\n");
    return qfalse;
  }

  //check guid format
  for(i = 0;i < MAX_GUID_LENGTH;i++)
  {
    if (guid[i] < 48 || (guid[i] > 57 && guid[i] < 65) || guid[i] > 70)
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nBAD GUID: Invalid qkey.\n");
      Com_DPrintf("Client rejected for bad qkey\n");
    }
  }

  //dont check duplicate guid in developer mode
  if (!sv_cheats->integer)
  {
    //check duplicate guid with validated clients
    for(i = 0;i < sv.maxclients;i++)
    {
      cl = &svs.clients[i];

      //dont check for bots with an empty GUID or players who are not fully connected
      //otherwise it could check the reserved client slot which already contains
      //the same client information with the client that is trying to connect and
      //encounters latency or enters the password at the same time
      if (cl->state != CS_ACTIVE || cl->netchan.remoteAddress.type == NA_BOT)
      {
        continue;
      }

      const qchar *guid2 = Info_ValueForKey(cl->userinfo, "cl_guid");

      if (!Q_strncmp(guid, guid2, MAX_GUID_LENGTH + 1))
      {
        NET_OutOfBandPrint(NS_SERVER, from, "print\nBAD GUID: Duplicate qkey.\n");
        Com_DPrintf("Client rejected for duplicate qkey\n");
        return qfalse;
      }
    }
  }

  return qtrue;
}

#if defined(INCLUDE_LEGACY_CHALLENGE)
/*
==================
SV_CheckChallence
==================
*/
const qbool
SV_CheckChallenge(const netadr_t *from)
{
  qint i;

  if (strlen(Cmd_Argv(1)) > 128)
  {
    SV_WriteAttackLog(va(NULL, "SVC_Info: challenge length exceeded from %s dropping request\n", NET_AdrToString(from)));
    return qfalse;
  }

  const qint challenge = Q_atoi(Cmd_Argv(1));

  if (!NET_IsLocalAddress(from))
  {
    for(i = 0;i < MAX_CHALLENGES;i++)
    {
      if (NET_CompareAdr(from, &svs.challenges[i].adr))
      {
        if (challenge == svs.challenges[i].challenge && !svs.challenges[i].connected)
        {
          break; //good
        }
      }
    }

    if (i == MAX_CHALLENGES)
    {
      return qfalse;
    }
  }

  return qtrue;
}
#endif

/*
==================
SV_GetStateName
==================
*/
static const qchar *
SV_GetStateName(const clientState_t state)
{
  switch(state)
  {
    case
    CS_FREE:
      return "CS_FREE";

    case
    CS_ZOMBIE:
      return "CS_ZOMBIE";

    case
    CS_CONNECTED:
      return "CS_CONNECTED";

    case
    CS_PRIMED:
      return "CS_PRIMED";

    case
    CS_ACTIVE:
      return "CS_ACTIVE";

    default:
      return "CS_UNKNOWN";
  }
}

/*
==================
SV_SetClientState
==================
*/
const void
SV_SetClientState(client_t *cl, const clientState_t newState)
{
  if (cl->state == newState)
  {
    return;
  }

#if 0 //!defined(_DEBUG)
  if (!com_developer->integer)
  {
    return;
  }
#endif

  if (com_developer->integer)
  {
    if (cl->name[0] != '\0')
    {
      Com_Printf("%s is going from %s to %s\n", cl->name, SV_GetStateName(cl->state), SV_GetStateName(newState));
    }
    else
    {
      Com_Printf("%d is going from %s to %s\n", ARRAY_INDEX(svs.clients, cl), SV_GetStateName(cl->state), SV_GetStateName(newState));
    }
  }

  cl->state = newState;
}

/*
==================
SV_FilterNameStringed
==================
*/
static const void
SV_FilterNameStringed(const qchar *userinfo)
{
  const qchar *nameValue;
  qchar *nameInInfo;

  if (!*userinfo || !userinfo)
  {
    return;
  }

  nameValue = Info_ValueForKey(userinfo, "name");

  if ((!*nameValue || !nameValue) || Q_stricmpn(nameValue, "@@@", 3))
  {
    return;
  }

  nameInInfo = (qchar *)(Q_stristr(userinfo, "\\name\\@@@"));

  if (!nameInInfo)
  {
    return;
  }

  nameInInfo += 6;
  *nameInInfo = ' ';
  *(nameInInfo + 1) = ' ';
  *(nameInInfo + 2) = ' ';
}
  

/*
==================
SV_DirectConnect

A "connect" OOB command has been received
==================
*/
const void
SV_DirectConnect(const netadr_t *from)
{
#if defined(INCLUDE_LEGACY_CHALLENGE)
  if (sv_legacyChallenge->integer)
  {
    qchar userinfo[MAX_INFO_STRING];
    qint i;
    qint n;
    client_t *cl;
    client_t *newcl;
    //sharedEntity_t *ent;
    qint clientNum;
    qint version;
    qint qport;
    qint challenge;
    const qchar *password;
    qint startIndex;
    intptr_t denied;
    qint count;
    const unsigned now = Sys_Milliseconds();
    const qchar *ip;
    const qchar *v;
    const qchar *info;

    Com_DPrintf("SVC_DirectConnect()\n");

    for(i = 0, n = 0;i < sv.maxclients;i++)
    {
      const netadr_t *addr = &svs.clients[i].netchan.remoteAddress;

      if (NET_CompareBaseAdr(addr, from) && !Sys_IsLANAddress(from) && addr->type != NA_BOT)
      {
        if (svs.clients[i].state >= CS_CONNECTED && !svs.clients[i].justConnected)
        {
          if (++n >= sv_maxclientsPerIP->integer)
          {
            NET_OutOfBandPrint(NS_SERVER, from, "print\ntoo many connections\n");
            return;
          }
        }
      }
    }

    info = Cmd_Argv(1);
    v = Info_ValueForKey(info, "challenge");

    //verify challenge in the first place
    info = Cmd_Argv(1);
    v = Info_ValueForKey(info, "challenge");

    if (*v == '\0')
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nmissing challenge in userinfo\n");
      return;
    }

    challenge = Q_atoi(v);
    Q_strncpyz(userinfo, info, sizeof(userinfo));
    v = Info_ValueForKey(userinfo, "protocol");

    if (*v == '\0')
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nmissing protocol in userinfo\n");
      return;
    }

    version = Q_atoi(Info_ValueForKey(userinfo, "protocol"));

    if (version != PROTOCOL_VERSION)
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nServer uses protocol version %i\n", PROTOCOL_VERSION);
      Com_DPrintf("    rejected connect from version %i\n", version);
      return;
    }

    v = Info_ValueForKey(userinfo, "qport");

    if (*v == '\0')
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nmissing qport in userinfo\n");
      return;
    }

    qport = Q_atoi(Info_ValueForKey(userinfo, "qport"));

    //we don't need these keys after connection, release some space in userinfo
    Info_RemoveKey(userinfo, "challenge");
    Info_RemoveKey(userinfo, "qport");
    Info_RemoveKey(userinfo, "protocol");
    Info_RemoveKey(userinfo, "client");

    //don't let "ip" overflow userinfo string
    if (NET_IsLocalAddress(from))
    {
      ip = "localhost";
    }
    else
    {
      ip = NET_AdrToString(from);
    }

    if (!Info_SetValueForKey(userinfo, "ip", ip))
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nUserinfo string length exceeded. Try removing setu cvars from your config.\n");
      return;
    }

    //run userinfo filter
    v = SV_RunFilters(userinfo, from);

    if (*v != '\0')
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\n%s\n", v);
      Com_DPrintf("Engine rejected a connection: %s.\n", v);
      return;
    }

    //restore burst capacity
    SVC_RateRestoreBurstAddress(from, 10, 1000, now);

    //quick reject
    newcl = NULL;

    for(i = 0;i < sv.maxclients;i++)
    {
      cl = &svs.clients[i];

      if (NET_CompareBaseAdr(from, &cl->netchan.remoteAddress))
      {
        const qint elapsed = svs.time - cl->lastConnectTime;

        if (elapsed < (sv_reconnectlimit->integer * 1000) && elapsed >= 0)
        {
          const qint remains = ((sv_reconnectlimit->integer * 1000) - elapsed + 999) / 1000;

          if (com_developer->integer)
          {
            Com_Printf("%s: reconnect rejected: too soon\n", NET_AdrToString(from));
          }

          NET_OutOfBandPrint(NS_SERVER, from, "print\nreconnecting, please wait %i second%s\n", remains, remains != 1 ? "s":"");
          return;
        }

        newcl = cl; //we may reuse this slot
        break;
      }
    }

    //see if the challenge is valid (LAN clients don't need to challenge)
    if (!NET_IsLocalAddress(from))
    {
      qint ping;
      challenge_t *challengeptr;

      for(i = 0;i < MAX_CHALLENGES;i++)
      {
        if (NET_CompareAdr(from, &svs.challenges[i].adr))
        {
          if (challenge == svs.challenges[i].challenge)
          {
            break;
          }
        }
      }

      if (i == MAX_CHALLENGES)
      {
        NET_OutOfBandPrint(NS_SERVER, from, "print\nNo or bad challenge for address\n");
        return;
      }

      challengeptr = &svs.challenges[i];

      if (challengeptr->wasrefused)
      {
        //Return silently, so that error messages written by the server keep being displayed.
        return;
      }

      if (!svs.challenges[i].firstPing)
      {
        ping = svs.time - challengeptr->pingTime;
        challengeptr->firstPing = ping;
      }
      else
      {
        ping = challengeptr->firstPing;
      }

      !challengeptr->connected ? Com_Printf("Client %i connecting with %i challenge ping\n", i, ping):Com_DPrintf("Client %i connecting again with %i challenge ping\n", i, ping);

      challengeptr->connected = qtrue;

      //never reject a LAN client based on ping
      if (!Sys_IsLANAddress(from))
      {
        if (sv_minPing->value > 0 && ping < sv_minPing->value)
        {
          //don't let them keep trying until they get a big delay
          NET_OutOfBandPrint(NS_SERVER, from, "print\nServer is for high pings only\n");
          Com_DPrintf("Client %i rejected on a too low ping\n", i);

          //reset the address otherwise their ping will keep increasing
          //with each connect message and they'd eventually be able to connect
          svs.challenges[i].adr.port = 0;
          challengeptr->wasrefused = qtrue;
          return;
        }

        if (sv_maxPing->value > 0 && ping > sv_maxPing->value)
        {
          NET_OutOfBandPrint(NS_SERVER, from, "print\nServer is for low pings only\n");
          Com_DPrintf("Client %i rejected on a too high ping\n", i);
          challengeptr->wasrefused = qtrue;
          return;
        }
      }
    }

    //q3fill protection
    if (!Sys_IsLANAddress(from))
    {
      qint connectingip = 0;

      for(i = 0;i < sv.maxclients;i++)
      {
        if (svs.clients[i].netchan.remoteAddress.type != NA_BOT && svs.clients[i].state == CS_CONNECTED && NET_CompareBaseAdr(&svs.clients[i].netchan.remoteAddress, from))
        {
          connectingip++;
        }
      }

      if (connectingip >= 3)
      {
        NET_OutOfBandPrint(NS_SERVER, from, "print\nwait\n");
      }
    }

    //if there is already a slot for this ip, reuse it
    for(i = 0;i < sv.maxclients;i++)
    {
      cl = &svs.clients[i];

      if (cl->state == CS_FREE)
      {
        continue;
      }

      //both qport and netport should match for a reconnecting client
      if (NET_CompareBaseAdr(from, &cl->netchan.remoteAddress) && cl->netchan.qport == qport)
      {
        Com_Printf("%s:reconnect\n", NET_AdrToString(from));
        newcl = cl;

        if (newcl->state >= CS_CONNECTED)
        {
          //call QVM disconnect function before calling connect again
          //fixes issues such as disappearing CTF flags in unpatched mods
#if defined(USE_JAVA)
          Java_G_ClientDisconnect(svs.clients, newcl);
#else
          VM_Call(sv.gvm, GAME_CLIENT_DISCONNECT, ARRAY_INDEX(svs.clients, newcl));
#endif

          //don't leak memory or file handles due to e.g. downloads in progress
          SV_FreeClient(newcl);
        }

        //this doesn't work because it nukes the players userinfo

//     //disconnect the client from the game first so any flags the
//     //player might have are dropped
//#if defined(USE_JAVA)
//     Java_G_ClientDisconnect(svs.clients, newcl);
//#else
//     VM_Call(sv.gvm, GAME_CLIENT_DISCONNECT, ARRAY_INDEX(svs.clients, newcl));
//#endif
        //
        goto gotnewcl1;
      }
    }

    //check guid after we ensure client doesn't already use a slot
    if (sv_guidCheck->integer)
    {
      if (!SV_isValidGUID(from, userinfo))
      {
        return;
      }
    }

    //find a client slot
    //if "sv_privateClients" is set > 0, then that number
    //of client slots will be reserved for connections that
    //have "password" set to the value of "sv_privatePassword"
    //Info requests will report the maxclients as if the private
    //slots didn't exist, to prevent people from trying to connect
    //to a full server.
    //This is to allow us to reserve a couple slots here on our
    //servers so we can play without having to kick people.

    //check for privateClient password
    password = Info_ValueForKey(userinfo, "password");

    if (*password && !strcmp(password, sv_privatePassword->string))
    {
      startIndex = sv_democlients->integer;
    }
    else
    {
      //skip past the reserved slots
      startIndex = sv_privateClients->integer + sv_democlients->integer;
    }

    if (newcl && newcl >= svs.clients + startIndex && newcl->state == CS_FREE)
    {
      Com_Printf("%s: reuse slot %i\n", NET_AdrToString(from), ARRAY_INDEX(svs.clients, newcl));
      goto gotnewcl1;
    }

    //select least used free slot
    n = 0;
    newcl = NULL;

    for(i = startIndex;i < sv.maxclients;i++)
    {
      cl = &svs.clients[i];

      if (cl->state == CS_FREE && (newcl == NULL || svs.time - cl->lastDisconnectTime > n))
      {
        n = svs.time - cl->lastDisconnectTime;
        newcl = cl;
      }
    }

    if (!newcl)
    {
      cl = NULL;

      //find a bot
      for(i = startIndex;i < sv.maxclients;i++)
      {
        if (svs.clients[i].netchan.remoteAddress.type == NA_BOT)
        {
          cl = &svs.clients[i];
          break;
        }

        if (svs.clients[i].gentity->r.svFlags & SVF_BOT)
        {
          cl = &svs.clients[i];
          break;
        }
      }

      if (!cl)
      {
        if (NET_IsLocalAddress(from))
        {
          Com_Error(ERR_FATAL, "server is full on local connect");
          return;
        }
        else
        {
          NET_OutOfBandPrint(NS_SERVER, from, "print\nServer is full\n");
          Com_DPrintf("Rejected a connection.\n");
          return;
        }
      }

      //found bot so remove for room
      SV_DropClient(cl, "bot removal");
      newcl = cl;
    }

    //we got a newcl, so reset the reliableSequence and reliableAcknowledge
    cl->reliableAcknowledge = 0;
    cl->reliableSequence = 0;

gotnewcl1:	
    //build a new connection
    //accept the new client
    //this is the only place a client_t is ever initialized
    Com_Memset(newcl, 0, sizeof(*newcl));
    clientNum = ARRAY_INDEX(svs.clients, newcl);
#if 0 //skip this until cs primed
    ent = SV_GentityNum(clientNum);
    newcl->gentity = ent;
    ent->r.svFlags = 0;
#endif

    //save the challenge
    newcl->challenge = challenge;

    //save the address
    Netchan_Setup(NS_SERVER, &newcl->netchan , from, qport);

    //init the netchan queue
    SV_Netchan_ClearQueue(newcl);

    //save the userinfo
    Q_strncpyz(newcl->userinfo, userinfo, sizeof(newcl->userinfo));

    //filter name while connecting to server
    SV_FilterNameStringed(newcl->userinfo);

    SV_UserinfoChanged(newcl, qtrue, qfalse); //update userinfo, do not run filter

    //get the game a chance to reject this connection or modify the userinfo
#if defined(USE_JAVA)
    denied = Java_G_ClientConnect(clientNum, qtrue, qfalse); //firstTime = qtrue

    if (denied)
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\n%s\n", denied);
      Com_DPrintf("Game rejected a connection: %s.\n", denied);
      return;
    }
#else
    denied = VM_Call(sv.gvm, GAME_CLIENT_CONNECT, clientNum, qtrue, qfalse);	//firstTime = qtrue

    if (denied)
    {
      //we can't just use VM_ArgPtr, because that is only valid inside a VM_Call
      const qchar *str = (const qchar *)GVM_ArgPtr(denied);

      NET_OutOfBandPrint(NS_SERVER, from, "print\n%s\n", str);
      Com_DPrintf("Game rejected a connection: %s.\n", str);
      return;
    }
#endif

    //clear out firstPing now that client is connected
    svs.challenges[i].firstPing = 0;

    //send the connect packet to the client
    NET_OutOfBandPrint(NS_SERVER, from, "connectResponse");

    SV_SetClientState(newcl, CS_CONNECTED);

    //newcl->nextSnapshotTime = svs.time;
    newcl->lastSnapshotTime = svs.time - 9999; //generate a snapshot immediately
    newcl->lastPacketTime = svs.time;
    newcl->lastConnectTime = svs.time;
    newcl->lastDisconnectTime = svs.time;

    SVC_RateRestoreToxicAddress(&newcl->netchan.remoteAddress, 10, 1000, now);
    newcl->justConnected = qtrue;
	  
    //when we receive the first packet from the client, we will
    //notice that it is from a different serverid and that the
    //gamestate message was not just sent, forcing a retransmit
    newcl->gamestateMessageNum = newcl->messageAcknowledge -1; //force gamestate retransmit
    newcl->lastUserinfoChange = 0;
    newcl->lastUserinfoCount = 0;

    //if this was the first client on the server, or the last client
    //the server can hold, send a heartbeat to the master.
    count = 0;

    for(i = 0;i < sv.maxclients;i++)
    {
      cl = &svs.clients[i];

      if (cl->state >= CS_CONNECTED)
      {
        count++;
      }
    }

    if (count == 1 || count == sv.maxclients)
    {
      SV_Heartbeat_f();
    }
  }
  else
  {
#endif
    qchar userinfo[MAX_INFO_STRING];
    qint i;
    qint n;
    client_t *cl;
    client_t *newcl;
    //sharedEntity_t *ent;
    qint clientNum;
    qint version;
    qint qport;
    qint challenge;
    const qchar *password;
    qint startIndex;
    intptr_t denied;
    qint count;
    const unsigned now = Sys_Milliseconds();
    const qchar *ip;
    const qchar *v;
    const qchar *info;

    Com_DPrintf("SVC_DirectConnect()\n");

    for(i = 0, n = 0;i < sv.maxclients;i++)
    {
      const netadr_t *addr = &svs.clients[i].netchan.remoteAddress;

      if (NET_CompareBaseAdr(addr, from) && !Sys_IsLANAddress(from) && addr->type != NA_BOT)
      {
        if (svs.clients[i].state >= CS_CONNECTED && !svs.clients[i].justConnected)
        {
          if (++n >= sv_maxclientsPerIP->integer)
          {
            NET_OutOfBandPrint(NS_SERVER, from, "print\ntoo many connections\n");
            return;
          }
        }
      }
    }

    //verify challenge in first place
    info = Cmd_Argv(1);
    v = Info_ValueForKey(info, "challenge");

    if (*v == '\0')
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nmissing challenge in userinfo\n");
      return;
    }

    challenge = Q_atoi(v);

    //see if the challenge is valid (localhost clients don't need to challenge)
    if (!NET_IsLocalAddress(from))
    {
      //verify the received challenge against the expected challenge
#if defined(STATELESS_CHALLENGES_VERSION_ONE)
      if (!SV_VerifyChallenge(challenge, from))
      {
        NET_OutOfBandPrint(NS_SERVER, from, "print\nincorrect challenge for your address\n");
        return;
      }
#else
      if (!SV_VerifyChallenge(challenge, from))
      {
        NET_OutOfBandPrint(NS_SERVER, from, "print\nincorrect challenge, please reconnect\n");
        return;
      }
#endif
    }

    Q_strncpyz(userinfo, info, sizeof(userinfo));
    v = Info_ValueForKey(userinfo, "protocol");

    if (*v == '\0')
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nmissing protocol in userinfo\n");
      return;
    }

    version = Q_atoi(Info_ValueForKey(userinfo, "protocol"));

    if (version != PROTOCOL_VERSION)
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nServer uses protocol version %i\n", PROTOCOL_VERSION);
      Com_DPrintf("    rejected connect from version %i\n", version);

      return;
    }

    v = Info_ValueForKey(userinfo, "qport");

    if (*v == '\0')
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nmissing qport in userinfo\n");
      return;
    }

    qport = Q_atoi(Info_ValueForKey(userinfo, "qport"));

    //we don't need these keys after connection, release some space in userinfo
    Info_RemoveKey(userinfo, "challenge");
    Info_RemoveKey(userinfo, "qport");
    Info_RemoveKey(userinfo, "protocol");
    Info_RemoveKey(userinfo, "client");

    //don't let "ip" overflow userinfo string
    if (NET_IsLocalAddress(from))
    {
      ip = "localhost";
    }
    else
    {
      ip = (qchar *)NET_AdrToString(from);
    }

    if (!Info_SetValueForKey(userinfo, "ip", ip))
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nUserinfo string length exceeded. Try removing setu cvars from your config.\n");
      return;
    }

    //run userinfo filter
    v = SV_RunFilters(userinfo, from);

    if (*v != '\0')
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\n%s\n", v);
      Com_DPrintf("Engine rejected a connection: %s.\n", v);
      return;
    }

    //restore burst capacity
    SVC_RateRestoreBurstAddress(from, 10, 1000, now);

    //quick reject
    newcl = NULL;

    for(i = 0;i < sv.maxclients;i++)
    {
      cl = &svs.clients[i];

      if (NET_CompareBaseAdr(from, &cl->netchan.remoteAddress))
      {
        const qint elapsed = svs.time - cl->lastConnectTime;

        if (elapsed < (sv_reconnectlimit->integer * 1000) && elapsed >= 0)
        {
          const qint remains = ((sv_reconnectlimit->integer * 1000) - elapsed + 999) / 1000;

          if (com_developer->integer)
          {
            Com_Printf("%s: reconnect rejected: too soon\n", NET_AdrToString(from));
          }

          NET_OutOfBandPrint(NS_SERVER, from, "print\nreconnecting, please wait %i second%s\n", remains, remains != 1 ? "s":"");
          return;
        }

        newcl = cl; //we may reuse this slot
        break;
      }
    }

    //q3fill protection
    if (!Sys_IsLANAddress(from))
    {
      qint connectingip = 0;

      for(i = 0;i < sv.maxclients;i++)
      {
        if (svs.clients[i].netchan.remoteAddress.type != NA_BOT && svs.clients[i].state == CS_CONNECTED && NET_CompareBaseAdr(&svs.clients[i].netchan.remoteAddress, from))
        {
          connectingip++;
        }
      }

      if (connectingip >= 3)
      {
        NET_OutOfBandPrint(NS_SERVER, from, "print\nwait\n");
      }
    }

    //if there is already a slot for this ip, reuse it
    for(i = 0;i < sv.maxclients;i++)
    {
      cl = &svs.clients[i];

      if (cl->state == CS_FREE)
      {
        continue;
      }

      //both qport and netport should match for a reconnecting client
      if (NET_CompareBaseAdr(from, &cl->netchan.remoteAddress) && cl->netchan.qport == qport)
      {
        Com_Printf("%s:reconnect\n", NET_AdrToString(from));
        newcl = cl;

        if (newcl->state >= CS_CONNECTED)
        {
          //call QVM disconnect function before calling connect again
          //fixes issues such as disappearing CTF flags in unpatched mods
#if defined(USE_JAVA)
          Java_G_ClientDisconnect(svs.clients, newcl);
#else
          VM_Call(sv.gvm, GAME_CLIENT_DISCONNECT, ARRAY_INDEX(svs.clients, newcl));
#endif

          //don't leak memory or file handles due to e.g. downloads in progress
          SV_FreeClient(newcl);
        }

        //this doesn't work because it nukes the players userinfo

//     //disconnect the client from the game first so any flags the
//     //player might have are dropped
//#if defined(USE_JAVA)
//     Java_G_ClientDisconnect(svs.clients, newcl);
//#else
//     VM_Call(sv.gvm, GAME_CLIENT_DISCONNECT, ARRAY_INDEX(svs.clients, newcl));
//#endif
        //
        goto gotnewcl2;
      }
    }

    //check guid after we ensure client doesn't already use a slot
    if (sv_guidCheck->integer)
    {
      if (!SV_isValidGUID(from, userinfo))
      {
        return;
      }
    }

    //find a client slot
    //if "sv_privateClients" is set > 0, then that number
    //of client slots will be reserved for connections that
    //have "password" set to the value of "sv_privatePassword"
    //Info requests will report the maxclients as if the private
    //slots didn't exist, to prevent people from trying to connect
    //to a full server.
    //This is to allow us to reserve a couple slots here on our
    //servers so we can play without having to kick people.

    //check for privateClient password
    password = Info_ValueForKey(userinfo, "password");

    if (*password && !strcmp(password, sv_privatePassword->string))
    {
      startIndex = sv_democlients->integer;
    }
    else
    {
      //skip past the reserved slots
      startIndex = sv_privateClients->integer + sv_democlients->integer;
    }

    if (newcl && newcl >= svs.clients + startIndex && newcl->state == CS_FREE)
    {
      Com_Printf("%s: reuse slot %i\n", NET_AdrToString(from), ARRAY_INDEX(svs.clients, newcl));
      goto gotnewcl2;
    }

    //select least used free slot
    n = 0;
    newcl = NULL;

    for(i = startIndex;i < sv.maxclients;i++)
    {
      cl = &svs.clients[i];

      if (cl->state == CS_FREE && (newcl == NULL || svs.time - cl->lastDisconnectTime > n))
      {
        n = svs.time - cl->lastDisconnectTime;
        newcl = cl;
      }
    }

    if (!newcl)
    {
      cl = NULL;

      //find a bot
      for(i = startIndex;i < sv.maxclients;i++)
      {
        if (svs.clients[i].netchan.remoteAddress.type == NA_BOT)
        {
          cl = &svs.clients[i];
          break;
        }

        if (svs.clients[i].gentity->r.svFlags & SVF_BOT)
        {
          cl = &svs.clients[i];
          break;
        }
      }

      if (!cl)
      {
        if (NET_IsLocalAddress(from))
        {
          Com_Error(ERR_FATAL, "server is full on local connect");
          return;
        }
        else
        {
          NET_OutOfBandPrint(NS_SERVER, from, "print\nServer is full\n");
          Com_DPrintf("Rejected a connection.\n");
          return;
        }
      }

      //found bot so remove for room
      SV_DropClient(cl, "bot removal");
      newcl = cl;
    }

    //we got a newcl, so reset the reliableSequence and reliableAcknowledge
    cl->reliableAcknowledge = 0;
    cl->reliableSequence = 0;

gotnewcl2:	
    //build a new connection
    //accept the new client
    //this is the only place a client_t is ever initialized
    Com_Memset(newcl, 0, sizeof(*newcl));
    clientNum = ARRAY_INDEX(svs.clients, newcl);
#if 0 //skip this until cs primed
    ent = SV_GentityNum(clientNum);
    newcl->gentity = ent;
    ent->r.svFlags = 0;
#endif

    //save the challenge
    newcl->challenge = challenge;

    //save the address
    Netchan_Setup(NS_SERVER, &newcl->netchan , from, qport);

    //init the netchan queue
    SV_Netchan_ClearQueue(newcl);

    //save the userinfo
    Q_strncpyz(newcl->userinfo, userinfo, sizeof(newcl->userinfo));

    //filter name while connecting to server
    SV_FilterNameStringed(newcl->userinfo);

    SV_UserinfoChanged(newcl, qtrue, qfalse); //update userinfo, do not run filter

    //get the game a chance to reject this connection or modify the userinfo
#if defined(USE_JAVA)
    denied = Java_G_ClientConnect(clientNum, qtrue, qfalse); //firstTime = qtrue

    if (denied)
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\n%s\n", denied);
      Com_DPrintf("Game rejected a connection: %s.\n", denied);
      return;
    }
#else
    denied = VM_Call(sv.gvm, GAME_CLIENT_CONNECT, clientNum, qtrue, qfalse);	//firstTime = qtrue

    if (denied)
    {
      //we can't just use VM_ArgPtr, because that is only valid inside a VM_Call
      const qchar *str = (const qchar *)GVM_ArgPtr(denied);

      NET_OutOfBandPrint(NS_SERVER, from, "print\n%s\n", str);
      Com_DPrintf("Game rejected a connection: %s.\n", str);
      return;
    }
#endif

    //clear out firstPing now that client is connected
#if defined(INCLUDE_LEGACY_CHALLENGE)
    svs.challenges[i].firstPing = 0;
#endif
    //send the connect packet to the client
    NET_OutOfBandPrint(NS_SERVER, from, "connectResponse");

    SV_SetClientState(newcl, CS_CONNECTED);

    //newcl->nextSnapshotTime = svs.time;
    newcl->lastSnapshotTime = svs.time - 9999; //generate a snapshot immediately
    newcl->lastPacketTime = svs.time;
    newcl->lastConnectTime = svs.time;
    newcl->lastDisconnectTime = svs.time;

    SVC_RateRestoreToxicAddress(&newcl->netchan.remoteAddress, 10, 1000, now);
    newcl->justConnected = qtrue;
	  
    //when we receive the first packet from the client, we will
    //notice that it is from a different serverid and that the
    //gamestate message was not just sent, forcing a retransmit
    newcl->gamestateMessageNum = newcl->messageAcknowledge - 1; //force gamestate retransmit
    newcl->lastUserinfoChange = 0;
    newcl->lastUserinfoCount = 0;

    //if this was the first client on the server, or the last client
    //the server can hold, send a heartbeat to the master.
    count = 0;

    for(i = 0;i < sv.maxclients;i++)
    {
      cl = &svs.clients[i];

      if (cl->state >= CS_CONNECTED)
      {
        count++;
      }
    }

    if (count == 1 || count == sv.maxclients)
    {
      SV_Heartbeat_f();
    }
#if defined(INCLUDE_LEGACY_CHALLENGE)
  }
#endif
}

/*
=====================
SV_FreeClient

Destructor for data allocated in a client structure
=====================
*/
const void
SV_FreeClient(client_t *client)
{
  SV_Netchan_ClearQueue(client);
  SV_CloseDownload(client);
}

/*
=====================
SV_DropClient

Called when the player is totally leaving the server, either willingly
or unwillingly.  This is NOT called if the entire server is quiting
or crashing -- SV_FinalMessage() will handle that
=====================
*/
const void
SV_DropClient(client_t *drop, const qchar *reason)
{
#if defined(INCLUDE_LEGACY_CHALLENGE)
  if (sv_legacyChallenge->integer)
  {
    qchar name[sizeof(drop->name)];
    qint i;
    challenge_t *challenge;
    qbool isBot;

    if (drop->state == CS_ZOMBIE)
    {
      return; //already dropped
    }

    isBot = drop->netchan.remoteAddress.type == NA_BOT;

    //setting the connected state of the challenge to false is totally unnecessary and only leads to all sorts of anomalies
    //here is an explanation of why commenting this out will not break anything
    //when a player connects and reaches the point in direct connect where the challenge connected
    //state is set to true, the client may be dropped due to high ping, being banned by the qvm, a server full, or other reasons,
    //in these cases, the client structure is never created for the connection and the player is never dropped using
    //drop client, drop client happens to be the only place where a challenge's
    //connected state is set to false, other than in get challenge, where a new challenge is allocated
    //so when the player is dropped for high ping and other reasons stated above, and tries to connect from scratch
    //using the get challenge packet, a new challenge structure is created, the old one does not get reused
    //there is no more harm done in not reusing a challenge if a player happens to connect all the way because connecting all the way
    //happens much less frequently, the challenge does not contain any important information that is needed when the client tries to
    //reconnect, now what commenting this secion out improves, the challenge fields such as challenge, time, pingtime, firsttime,
    //will all be computed from scratch, just like how it should be, if the challenge structure is extended in various patches
    //the properties of the challenge won't get inherited by subsequent connections by the same client, this is much cleaner

    Q_strncpyz(name, drop->name, sizeof(name)); //for further dprintf because drop name will be nuked in sv setuserinfo

    //see if we already have a challenge for this ip
    challenge = &svs.challenges[0];

    for(i = 0;i < MAX_CHALLENGES;i++, challenge++)
    {
      if (NET_CompareAdr(&drop->netchan.remoteAddress, &challenge->adr))
      {
        Com_Memset(challenge, 0, sizeof(*challenge));
        //challenge->connected = qfalse;
        break;
      }
    }

    //kill any download
    //SV_CloseDownload(drop);

    //free all allocated data on the client structure
    SV_FreeClient(drop);

    /*
    reset the reliable sequence to the correctly acknowledged command
    this prevents sv_addservercommand() from making another recursive call to sv_dropclient()
    if the client lacks sufficient space for another reliable command
    it also guarantees that the client receives both the print and disconnect commands
    */
    drop->reliableSequence = drop->reliableAcknowledge;
    /*
    setting the gamestate message number to -1 ensures that sv_addservercommand()
    will not call sv_dropclient() again, even though it is unlikely the client
    will receive many server commands during the drop
    */
    drop->gamestateMessageNum = -1;

    //tell everyone why they got dropped
    if (reason)
    {
      SV_SendServerCommand(NULL, "print \"%s" S_COLOR_WHITE " %s\n\"", drop->name, reason);
    }

    //call the prog function for removing a client
    //this will remove the body, among other things
#if defined(USE_JAVA)
    Java_G_ClientDisconnect(ARRAY_INDEX(svs.clients, drop));
#else
    VM_Call(sv.gvm, GAME_CLIENT_DISCONNECT, ARRAY_INDEX(svs.clients, drop));
#endif

    //add the disconnect command
    if (reason)
    {
      SV_SendServerCommand(drop, "disconnect \"%s\"", reason);
    }

    if (isBot)
    {
      SV_BotFreeClient(ARRAY_INDEX(svs.clients, drop));
    }

    //nuke user info
    SV_SetUserinfo(ARRAY_INDEX(svs.clients, drop), "");

    drop->justConnected = qfalse;
    drop->lastDisconnectTime = svs.time;

    if (isBot)
    {
      drop->state = CS_FREE; //Chey: bots dont go zombie since theres no connection
    }
    else
    {
      Q_strncpyz(drop->name, name, sizeof(name));
      SV_SetClientState(drop, CS_ZOMBIE); //become free in a few seconds
    }

    if (!reason)
    {
      return;
    }

    /*
    if this was the last client on the server, send a heartbeat
    to the master so it is known the server is empty
    send a heartbeat now so the master will get up to date info
    */
    for(i = 0;i < sv.maxclients;i++)
    {
      if (svs.clients[i].state >= CS_CONNECTED)
      {
        break;
      }
    }

    if (i == sv.maxclients)
    {
      SV_Heartbeat_f();
    }
  }
  else
  {
#endif
    qchar name[sizeof(drop->name)];
    qint i;
    qbool isBot;

    if (drop->state == CS_ZOMBIE)
    {
      return; //already dropped
    }

    isBot = drop->netchan.remoteAddress.type == NA_BOT;

    //setting the connected state of the challenge to false is totally unnecessary and only leads to all sorts of anomalies
    //here is an explanation of why commenting this out will not break anything
    //when a player connects and reaches the point in direct connect where the challenge connected
    //state is set to true, the client may be dropped due to high ping, being banned by the qvm, a server full, or other reasons,
    //in these cases, the client structure is never created for the connection and the player is never dropped using
    //drop client, drop client happens to be the only place where a challenge's
    //connected state is set to false, other than in get challenge, where a new challenge is allocated
    //so when the player is dropped for high ping and other reasons stated above, and tries to connect from scratch
    //using the get challenge packet, a new challenge structure is created, the old one does not get reused
    //there is no more harm done in not reusing a challenge if a player happens to connect all the way because connecting all the way
    //happens much less frequently, the challenge does not contain any important information that is needed when the client tries to
    //reconnect, now what commenting this secion out improves, the challenge fields such as challenge, time, pingtime, firsttime,
    //will all be computed from scratch, just like how it should be, if the challenge structure is extended in various patches
    //the properties of the challenge won't get inherited by subsequent connections by the same client, this is much cleaner

    Q_strncpyz(name, drop->name, sizeof(name)); //for further dprintf because drop name will be nuked in sv setuserinfo

    //kill any download
    //SV_CloseDownload(drop);

    //free all allocated data on the client structure
    SV_FreeClient(drop);

    /*
    reset the reliable sequence to the correctly acknowledged command
    this prevents sv_addservercommand() from making another recursive call to sv_dropclient()
    if the client lacks sufficient space for another reliable command
    it also guarantees that the client receives both the print and disconnect commands
    */
    drop->reliableSequence = drop->reliableAcknowledge;
    /*
    setting the gamestate message number to -1 ensures that sv_addservercommand()
    will not call sv_dropclient() again, even though it is unlikely the client
    will receive many server commands during the drop
    */
    drop->gamestateMessageNum = -1;

    //tell everyone why they got dropped
    if (reason)
    {
      SV_SendServerCommand(NULL, "print \"%s" S_COLOR_WHITE " %s\n\"", drop->name, reason);
    }

    //call the prog function for removing a client
    //this will remove the body, among other things
#if defined(USE_JAVA)
    Java_G_ClientDisconnect(ARRAY_INDEX(svs.clients, drop));
#else
    VM_Call(sv.gvm, GAME_CLIENT_DISCONNECT, ARRAY_INDEX(svs.clients, drop));
#endif

    //add the disconnect command
    if (reason)
    {
      SV_SendServerCommand(drop, "disconnect \"%s\"", reason);
    }

    if (isBot)
    {
      SV_BotFreeClient(ARRAY_INDEX(svs.clients, drop));
    }

    //nuke user info
    SV_SetUserinfo(ARRAY_INDEX(svs.clients, drop), "");

    drop->justConnected = qfalse;
    drop->lastDisconnectTime = svs.time;

    if (isBot)
    {
      drop->state = CS_FREE; //Chey: bots dont go zombie since theres no connection
    }
    else
    {
      Q_strncpyz(drop->name, name, sizeof(name));
      SV_SetClientState(drop, CS_ZOMBIE); //become free in a few seconds
    }

    if (!reason)
    {
      return;
    }

    /*
    if this was the last client on the server, send a heartbeat
    to the master so it is known the server is empty
    send a heartbeat now so the master will get up to date info
    */
    for(i = 0;i < sv.maxclients;i++)
    {
      if (svs.clients[i].state >= CS_CONNECTED)
      {
        break;
      }
    }

    if (i == sv.maxclients)
    {
      SV_Heartbeat_f();
    }
#if defined(INCLUDE_LEGACY_CHALLENGE)
  }
#endif
}

/*
================
SV_RemainingGameState

estimates free space available for additional systeminfo keys
================
*/
const qint
SV_RemainingGameState(void)
{
  qint len;
  qint start;
  qint i;
  entityState_t nullstate;
  const svEntity_t *svEnt;
  msg_t msg;
  byte msgBuffer[MAX_MSGLEN_BUF];

  MSG_Init(&msg, msgBuffer, MAX_MSGLEN);

  MSG_WriteLong(&msg, 7); //last client command

  for(i = 0;i < 256;i++) //simulate dummy client commands
  {
    MSG_WriteByte(&msg, i & 127);
  }

  //send the gamestate
  MSG_WriteByte(&msg, svc_gamestate);
  MSG_WriteLong(&msg, 7); //client->reliableSequence

  //write the configstrings
  for(start = 0;start < MAX_CONFIGSTRINGS;start++)
  {
    if (start == CS_SERVERINFO)
    {
      MSG_WriteByte(&msg, svc_configstring);
      MSG_WriteShort(&msg, start);
      MSG_WriteBigString(&msg, Cvar_InfoString(CVAR_SERVERINFO, NULL));
      continue;
    }

    if (start == CS_SYSTEMINFO)
    {
      MSG_WriteByte(&msg, svc_configstring);
      MSG_WriteShort(&msg, start);
      MSG_WriteBigString(&msg, Cvar_InfoString_Big(CVAR_SYSTEMINFO, NULL));
      continue;
    }

    if (sv.configstrings[start][0])
    {
      MSG_WriteByte(&msg, svc_configstring);
      MSG_WriteShort(&msg, start);
      MSG_WriteBigString(&msg, sv.configstrings[start]);
    }
  }

  //write the baselines
  Com_Memset(&nullstate, 0, sizeof(nullstate));

  for(start = 0;start < MAX_GENTITIES;start++)
  {
    if (!sv.baselineUsed[start])
    {
      continue;
    }

    svEnt = &sv.svEntities[start];
    MSG_WriteByte(&msg, svc_baseline);
    MSG_WriteDeltaEntity(&msg, &nullstate, &svEnt->baseline, qtrue);
  }

  MSG_WriteByte(&msg, svc_EOF);
  MSG_WriteLong(&msg, 7); //client num

  //write the checksum feed
  MSG_WriteLong(&msg, sv.checksumFeed);

  //finalize packet
  MSG_WriteByte(&msg, svc_EOF);

  len = PAD(msg.bit, 8) / 8;

  //reserve some space for potential userinfo expansion
  len += 512;

  return MAX_MSGLEN - len;
}

/*
================
SV_SendClientGameState

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each new map load.

It will be resent if the client acknowledges a later message but has
the wrong gamestate.
================
*/
const void
SV_SendClientGameState(client_t *client)
{
  qint start;
  entityState_t nullstate;
  const svEntity_t *svEnt;
  msg_t msg;
  byte msgBuffer[MAX_MSGLEN_BUF];
#if !defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
  qbool csUpdated;
#endif

  if (sv_forceSendFragments->integer)
  {
    while(client->state && client->netchan.unsentFragments)
    {
      Com_DPrintf("sv sendclientgamestate writing old fragments for %s\n", client->name);
      Netchan_TransmitNextFragment(&client->netchan);
    }
  }

  Com_DPrintf("SV_SendClientGameState() for %s\n", client->name);

#if defined(UDP_DOWNLOAD_NO_DOUBLE_LOAD)
  if (client->state < CS_PRIMED)
  {
    //clear old cs change log to avoid unnecessary retransmits
    Com_Memset(client->csUpdated, 0, sizeof(client->csUpdated));
  }
#endif

  //Chey: this is to prevent clients from being able to go from CS_ACTIVE to CS_PRIMED by sending a fake package
  if (client->state == CS_CONNECTED)
  {
    SV_SetClientState(client, CS_PRIMED);
  }

  client->downloading = qfalse;
  client->pureAuthentic = qfalse;
  client->gotCP = qfalse;

  //to start generating delta for packet entities
  client->gentity = SV_GentityNum(ARRAY_INDEX(svs.clients, client));

  //when we receive the first packet from the client, we will
  //notice that it is from a different serverid and that the
  //gamestate message was not just sent, forcing a retransmit
  client->gamestateMessageNum = client->netchan.outgoingSequence;

  //only accept usercmds from current server time
  Com_Memset(&client->lastUsercmd, 0x0, sizeof(client->lastUsercmd));
  client->lastUsercmd.serverTime = sv.time - 1;

  MSG_Init(&msg, msgBuffer, MAX_MSGLEN);

  //NOTE, MRE: all server->client messages now acknowledge
  //let the client know which reliable clientCommands we have received
  MSG_WriteLong(&msg, client->lastClientCommand);

  //send any server commands waiting to be sent first.
  //we have to do this cause we send the client->reliableSequence
  //with a gamestate and it sets the clc.serverCommandSequence at
  //the client side
  SV_UpdateServerCommandsToClient(client, &msg);

  //send the gamestate
  MSG_WriteByte(&msg, svc_gamestate);
  MSG_WriteLong(&msg, client->reliableSequence);

  //write the configstrings
#if !defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
  csUpdated = qfalse;
#endif
  for(start = 0;start < MAX_CONFIGSTRINGS;start++)
  {
    if (*sv.configstrings[start] != '\0')
    {
      MSG_WriteByte(&msg, svc_configstring);
      MSG_WriteShort(&msg, start);

      if (start == CS_SYSTEMINFO && sv.pure != sv_pure->integer)
      {
        //make sure we send latched sv.pure, not forced cvar value
        qchar systemInfo[BIG_INFO_STRING];

        Q_strncpyz(systemInfo, sv.configstrings[start], sizeof(systemInfo));
        Info_SetValueForKey_s(systemInfo, sizeof(systemInfo), "sv_pure", va(NULL, "%i", sv.pure));
        MSG_WriteBigString(&msg, systemInfo);
      }
      else
      {
        MSG_WriteBigString(&msg, sv.configstrings[start]);
      }
    }

#if !defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
    if (client->csUpdated[start])
    {
      csUpdated = qtrue;
    }

    client->csUpdated[start] = qfalse;
#endif
  }

#if !defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
  if (client->gamestateAck == GSA_INIT)
  {
    //initial submission, accept any messageAcknowledge with matching serverId
    client->gamestateAck = GSA_SENT_ONCE;
  }
  else
  {
    if (client->gamestateAck == GSA_SENT_ONCE && !csUpdated)
    {
      //if no configstrings are being updated since last submission then assume that we're (re)sending identical gamestate
    }
    else
    {
      //expect exact messageAcknowledge
      client->gamestateAck = GSA_SENT_MANY;
    }
  }
#endif

#if defined(GAMESTATE_OVERFLOW_FIX)
  //update client->baseline_cutoff
  SV_CalculateMaxBaselines(client, msg);
#endif

  //write the baselines
  Com_Memset(&nullstate, 0, sizeof(nullstate));

  for(start = 0;start < MAX_GENTITIES;start++)
  {
    if (!sv.baselineUsed[start])
    {
      continue;
    }

#if defined(GAMESTATE_OVERFLOW_FIX)
    if (start > client->maxEntityBaseline)
    {
      continue;
    }
#endif

    svEnt = &sv.svEntities[start];
    MSG_WriteByte(&msg, svc_baseline);
    MSG_WriteDeltaEntity(&msg, &nullstate, &svEnt->baseline, qtrue);
  }

  MSG_WriteByte(&msg, svc_EOF);
  MSG_WriteLong(&msg, ARRAY_INDEX(svs.clients, client));

  //write the checksum feed
  MSG_WriteLong(&msg, sv.checksumFeed);

  //it is important to handle gamestate overflow
  //but at this stage client cant process any reliable commands
  //so at least try to inform him in console and release connection slot
  if (msg.overflowed)
  {
    if (client->netchan.remoteAddress.type == NA_LOOPBACK)
    {
      Com_Error(ERR_DROP, "gamestate overflow");
    }
    else
    {
      NET_OutOfBandPrint(NS_SERVER, &client->netchan.remoteAddress, "print\n" S_COLOR_RED "SERVER_ERROR: gamestate overflow\n");
      SV_DropClient(client, "gamestate overflow");
    }

    return;
  }

  //deliver this to the client
  SV_SendMessageToClient(&msg, client);
}

/*
==================
SV_ClientEnterWorld
==================
*/
const void
SV_ClientEnterWorld(client_t *client)
{
  qint clientNum;
  sharedEntity_t *ent;

  SV_SetClientState(client, CS_ACTIVE);
#if !defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
  client->gamestateAck = GSA_ACKED;
#endif

  client->oldServerTime = 0;

  //resend all configstrings using the cs commands since these are
  //no longer sent when the client is CS_PRIMED
  SV_UpdateConfigstrings(client);

  //set up the entity for the client
  clientNum = ARRAY_INDEX(svs.clients, client);
  ent = SV_GentityNum(clientNum);
  ent->s.number = clientNum;
  client->gentity = ent;

  client->lastUserinfoChange = 0;
  client->lastUserinfoCount = 0;

  client->deltaMessage = client->netchan.outgoingSequence - (PACKET_BACKUP + 1); //force delta reset
  //client->nextSnapshotTime = svs.time;	//generate a snapshot immediately
  client->lastSnapshotTime = svs.time - 9999; //generate a snapshot immediately

  //call the game begin function
#if defined(USE_JAVA)
  Java_G_ClientBegin(clientNum);
#else
  VM_Call(sv.gvm, GAME_CLIENT_BEGIN, clientNum);
#endif
}

/*
============================================================

CLIENT COMMAND EXECUTION

============================================================
*/
#if defined(UDP_DOWNLOAD_OPTIMIZE)
//size of chunks to read from source pk3
//serverside only, does not affect outgoing messages
#define DOWNLOAD_READ_CHUNK_SIZE 16384

//rate control constants
#define DOWNLOAD_MAX_RATE 5000 //in KB/s
#define DOWNLOAD_MIN_RATE 250 //in KB/s (burst rate, overall speed may be slower due to transmit window)
#define DOWNLOAD_RETRANSMIT_RATE_DECREASE(oldRate) (oldRate * 0.8) //on retransmit
#define DOWNLOAD_RATE_INCREASE(oldRate, blockSize) (oldRate + blockSize / 200.0) //on block acknowledge

//max bytes per packet, should match FRAGMENT_SIZE
#define DOWNLOAD_FRAGMENT_SIZE 1300

//assumed size of packet for rate limiting purposes (account for a bit of overhead)
#define DOWNLOAD_RATE_PACKET_SIZE (DOWNLOAD_FRAGMENT_SIZE + 100)

//maximum full-sized packets allowed per block
//roughly MAX_MSGLEN / DOWNLOAD_FRAGMENT_SIZE
#define DOWNLOAD_MAX_PACKETS_PER_BLOCK 12

#define DOWNLOAD_MAX_PACKETS_PER_MS (DOWNLOAD_MAX_RATE / DOWNLOAD_FRAGMENT_SIZE + 2)

//for certain purposes, don't treat client download rate as higher than global rate limit
#define DOWNLOAD_CLIENT_RATE(cl) (sv_dlRate->integer > 0 && sv_dlRate->integer < cl->downloadCurrentRate ? sv_dlRate->integer:cl->downloadCurrentRate)

//max unacknowledged bytes to send (should be enough to accommodate client ping)
#define DOWNLOAD_WINDOW_BYTES(cl) (DOWNLOAD_CLIENT_RATE(cl) * 200)
#endif

/*
==================
SV_CloseDownload

clear/free any download vars
==================
*/
static const ID_INLINE void
SV_CloseDownload(client_t *cl)
{
  unsigned i;

  //EOF
  if (cl->download != FS_INVALID_HANDLE)
  {
    FS_FCloseFile(cl->download);
    cl->download = FS_INVALID_HANDLE;
  }

  *cl->downloadName = '\0';

  //Free the temporary buffer space
  for(i = 0;i < MAX_DOWNLOAD_WINDOW;i++)
  {
    if (cl->downloadBlocks[i])
    {
      Z_Free(cl->downloadBlocks[i]);
      cl->downloadBlocks[i] = NULL;
    }
  }

#if defined(UDP_DOWNLOAD_OPTIMIZE)
  if (cl->downloadSrcChunk)
  {
    Z_Free(cl->downloadSrcChunk);
    cl->downloadSrcChunk = NULL;
  }
#endif
}

/*
==================
SV_StopDownload_f

Abort a download if in progress
==================
*/
static const ID_INLINE void
SV_StopDownload_f(client_t *cl)
{
  if (cl->state == CS_ACTIVE)
  {
    return;
  }

  if (*cl->downloadName)
  {
    Com_DPrintf("clientDownload: %d : file \"%s\" aborted\n", ARRAY_INDEX(svs.clients, cl), cl->downloadName);
  }

  SV_CloseDownload(cl);
}

/*
==================
SV_DoneDownload_f

Downloads are finished
==================
*/
static const ID_INLINE void
SV_DoneDownload_f(client_t *cl)
{
  if (cl->state == CS_ACTIVE)
  {
    return;
  }

  if (*cl->downloadName)
  {
    return;
  }

  Com_DPrintf("clientDownload: %s Done\n", cl->name);

#if defined(UDP_DOWNLOAD_NO_DOUBLE_LOAD)
  if (cl->state == CS_PRIMED)
  {
    //clients will load the map based on the pre-download gamestate, therefore
    //a new one is unnecessary and may cause double loading
    cl->downloading = qfalse;

    //dont immediately trip dropped download gamestate checks in SV_ExecuteClientMessage
    cl->gamestateMessageNum = cl->messageAcknowledge - 1;
    return;
  }
#endif

  //resend the game state to update any clients that entered during the download
  SV_SendClientGameState(cl);
}

/*
==================
SV_NextDownload_f

The argument will be the last acknowledged block from the client, it should be
the same as cl->downloadClientBlock
==================
*/
static const ID_INLINE void
SV_NextDownload_f(client_t *cl)
{
  const qint block = Q_atoi(Cmd_Argv(1));

  if (cl->state == CS_ACTIVE)
  {
    return;
  }

#if defined(UDP_DOWNLOAD_OPTIMIZE)
  if (cl->download && block == cl->downloadClientBlock && block < cl->downloadCurrentBlock)
  {
    const qint blockIndex = cl->downloadClientBlock % MAX_DOWNLOAD_WINDOW;

    Com_DPrintf("clientDownload: %d: client acknowledge of block %d\n", ARRAY_INDEX(svs.clients, cl), block);

    //find out if we are done, a zero length block indicates eof
    if (!cl->downloadBlockSize[blockIndex])
    {
      Com_Printf("clientDownload: %d: file \"%s\" completed\n", ARRAY_INDEX(svs.clients, cl), cl->downloadName);
      SV_CloseDownload(cl);
      return;
    }

    //gradually increment rate
    cl->downloadCurrentRate = DOWNLOAD_RATE_INCREASE(cl->downloadCurrentRate, cl->downloadBlockSize[blockIndex]);

    if (cl->downloadCurrentRate > DOWNLOAD_MAX_RATE)
    {
      cl->downloadCurrentRate = DOWNLOAD_MAX_RATE;
    }

    cl->downloadAckTime = svs.time;
    cl->downloadClientBlock++;
#else
  if (block == cl->downloadClientBlock)
  {
    Com_DPrintf("clientDownload: %d: client acknowledge of block %d\n", ARRAY_INDEX(svs.clients, cl), block);

    //Find out if we are done. A zero-length block indicates EOF
    if (!cl->downloadBlockSize[cl->downloadClientBlock % MAX_DOWNLOAD_WINDOW])
    {
      Com_Printf("clientDownload: %d: file \"%s\" completed\n", ARRAY_INDEX(svs.clients, cl), cl->downloadName);
      SV_CloseDownload(cl);
      return;
    }

    cl->downloadAckTime = svs.time;
    cl->downloadClientBlock++;
#endif
    return;
  }

  //We aren't getting an acknowledge for the correct block, drop the client
  //FIXME: this is bad... the client will never parse the disconnect message
  //because the cgame isn't loaded yet
  SV_DropClient(cl, "broken download");
}

/*
==================
SV_BeginDownload_f
==================
*/
static const ID_INLINE void
SV_BeginDownload_f(client_t *cl)
{
  if (cl->state == CS_ACTIVE)
  {
    return;
  }

  //kill any existing download
  SV_CloseDownload(cl);

  //cl->downloadName is non-zero now, SV_WriteDownloadToClient will see this and open
  //the file itself
  Q_strncpyz(cl->downloadName, Cmd_Argv(1), sizeof(cl->downloadName));

#if defined(UDP_DOWNLOAD_NO_DOUBLE_LOAD)
  //Chey: clients may try to load the map based on the pre-download gamestate, so track configstring updates during the download
  SV_SetClientState(cl, CS_PRIMED);
#else
  //Chey: contrary to primed, this avoids server to client commands from accumulating without being sent which leads to unnecessary commands being sent when downloading finishes, which should avoid potential server command overflows
  SV_SetClientState(cl, CS_CONNECTED);
#endif
  cl->gentity = NULL;
  cl->downloading = qtrue;

#if defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
  cl->downloadGamestateDropCheck = qtrue;
#else
  if (cl->gamestateAck == GSA_ACKED)
  {
    cl->gamestateAck = GSA_SENT_ONCE;
  }
#endif
}

#if defined(UDP_DOWNLOAD_OPTIMIZE)
/*
==================
SV_GetDownloadBlockSize

Determine amount of download data to fit into block. Try to get an amount that
fragments cleanly so each packet has close to the maximum amount of data.
==================
*/
static const ID_INLINE qint
SV_GetDownloadBlockSize(client_t *cl)
{
  //make sure blocksize is large enough to support large pk3s by a safe margin
  const qint paksizeBytesPerBlock = (cl->downloadSize / 32768) * 2 + 50;

  //make sure blocksize is large enough to avoid block window bottleneck
  const qint rateBytesPerBlock = DOWNLOAD_WINDOW_BYTES(cl) / MAX_DOWNLOAD_WINDOW;

  //use smallest packet count that meets pk3 size and rate requirements, since
  //smaller blocks have much better packet loss tolerance
  const qint bytesPerBlock = paksizeBytesPerBlock > rateBytesPerBlock ? paksizeBytesPerBlock:rateBytesPerBlock;
  qint packetsPerBlock = bytesPerBlock / DOWNLOAD_FRAGMENT_SIZE + 1;

  //force small blocks at beginning of download to help with high packet loss scenarios
  if (packetsPerBlock > cl->downloadClientBlock / 4)
  {
    packetsPerBlock = cl->downloadClientBlock / 4;
  }

  if (packetsPerBlock > DOWNLOAD_MAX_PACKETS_PER_BLOCK)
  {
    packetsPerBlock = DOWNLOAD_MAX_PACKETS_PER_BLOCK;
  }

  if (packetsPerBlock < 1)
  {
    packetsPerBlock = 1;
  }

  //reduce size a bit to allow for overhead
  return packetsPerBlock * DOWNLOAD_FRAGMENT_SIZE - 50;
}

/*
==================
SV_ReadDownloadBlock

Reads a new download block from source pk3 and writes to dataOut / sizeOut.

On success: Returns qtrue and sizeOut > 0.
On end of file: Returns qtrue and sizeOut = 0.
On error: Returns qfalse (client should be dropped).
==================
*/
static const ID_INLINE qbool
SV_ReadDownloadBlock(client_t *cl, qchar **dataOut, qint *sizeOut)
{
  const qint tgtSize = SV_GetDownloadBlockSize(cl);
  qint dataPos = 0;
  msg_t msg;
  byte msgBuffer[MAX_MSGLEN_BUF];
  qchar data[16384]; //size matches CL_ParseDownload

  MSG_Init(&msg, msgBuffer, MAX_MSGLEN);

  while(msg.cursize < tgtSize && dataPos < sizeof(data))
  {
    if (cl->downloadSrcChunkPos >= cl->downloadSrcChunkSize)
    {
      //check for eof
      if (cl->downloadSrcFileRemaining <= 0)
      {
        break;
      }

      //read next source chunk
      cl->downloadSrcChunkSize = cl->downloadSrcFileRemaining < DOWNLOAD_READ_CHUNK_SIZE ? cl->downloadSrcFileRemaining:DOWNLOAD_READ_CHUNK_SIZE;

      if (FS_Read(cl->downloadSrcChunk, cl->downloadSrcChunkSize, cl->download) != (unsigned)cl->downloadSrcChunkSize)
      {
        return qfalse;
      }

      cl->downloadSrcFileRemaining -= cl->downloadSrcChunkSize;
      cl->downloadSrcChunkPos = 0;
    }

    //add byte to message
    data[dataPos] = cl->downloadSrcChunk[cl->downloadSrcChunkPos];
    cl->downloadSrcChunkPos++;
    MSG_WriteByte(&msg, ((byte *)data)[dataPos]);
    ++dataPos;
  }

  *sizeOut = dataPos;

  if (*dataOut)
  {
    Z_Free(*dataOut);
  }

  *dataOut = Z_Malloc(dataPos);
  Com_Memcpy(*dataOut, data, dataPos);
  return qtrue;
}

/*
==================
SV_DownloadRetransmit
==================
*/
static const ID_INLINE void
SV_DownloadRetransmit(client_t *cl)
{
  cl->downloadXmitBlock = cl->downloadClientBlock;
  cl->downloadRetransmitMsg = cl->downloadCurrentMsg;

  //decrease current rate due to dropped blocks
  //it will climb back up as blocks are acknowledged, but if the lower rate is needed
  //due to some connection issues, this should at least provide a temporary period for
  //the download to limp forward rather than stalling completely.
  cl->downloadCurrentRate = DOWNLOAD_RETRANSMIT_RATE_DECREASE(cl->downloadCurrentRate);

  if (cl->downloadCurrentRate < DOWNLOAD_MIN_RATE)
  {
    cl->downloadCurrentRate = DOWNLOAD_MIN_RATE;
  }

  Com_Printf("clientDownload: %d: currentRate set to %lf due to retransmit\n", ARRAY_INDEX(svs.clients, cl), cl->downloadCurrentRate);
}

/*
==================
SV_DownloadCountOutgoingBytes

Returns the number of bytes sent to the client that are not yet acknowledged.
==================
*/
static const ID_INLINE qint
SV_DownloadCountOutgoingBytes(client_t *cl)
{
  qint i;
  qint count = 0;

  for(i = cl->downloadClientMsg;i < cl->downloadCurrentMsg;++i)
  {
    count += cl->downloadMsgTable[i % MAX_DOWNLOAD_MESSAGE_HISTORY].size;
  }

  return count;
}
#endif

/*
==================
SV_WriteDownloadToClient

Check to see if the client wants a file, open it if needed and start pumping the client
Fill up msg with data 
==================
*/
#if defined(UDP_DOWNLOAD_OPTIMIZE)
static const qbool
SV_WriteDownloadToClient(client_t *cl)
{
  qint curindex;
  qchar errorMessage[1024];
  msg_t msg;
  qbool skip = qfalse;
  byte msgBuffer[MAX_MSGLEN_BUF];

  if (!*cl->downloadName)
  {
    return qfalse; //nothing being downloaded
  }

  //CVE-2006-2082: validate the download against the list of pak files
  if (!FS_VerifyPak(cl->downloadName))
  {
    //will drop the client and leave it hanging on the other side. good for him
    SV_DropClient(cl, "illegal download request");
    return qfalse;
  }

  if (cl->download == FS_INVALID_HANDLE)
  {
    //we open the file here
    if (!(sv_allowDownload->integer & DLF_ENABLE) || (sv_allowDownload->integer & DLF_NO_UDP) || (cl->downloadSize = FS_SV_FOpenFileRead(cl->downloadName, &cl->download)) < 0)
    {
      //cannot auto-download file
      if (!(sv_allowDownload->integer & DLF_ENABLE) || (sv_allowDownload->integer & DLF_NO_UDP))
      {
        Com_Printf("clientDownload: %d : \"%s\" download disabled", ARRAY_INDEX(svs.clients, cl), cl->downloadName);

        if (sv.pure)
        {
          Com_sprintf(errorMessage, sizeof(errorMessage), "could not download \"%s\" because autodownloading is disabled on the server\n\nyou will need to get this file elsewhere before you can connect to this pure server\n", cl->downloadName);
        }
        else
        {
          Com_sprintf(errorMessage, sizeof(errorMessage), "could not download \"%s\" because autodownloading is disabled on the server\n\nthe server you are connecting to is not a pure server, set autodownload to no in your settings and you might be able to join the game anyway\n", cl->downloadName);
        }
      }
      else
      {
        //NOTE TTimo this is NOT supposed to happen unless bug in our filesystem scheme?
        //if the pk3 is referenced, it must have been found somewhere in the filesystem
        Com_Printf("clientDownload: %d: \"%s\" file not found on server\n", ARRAY_INDEX(svs.clients, cl), cl->downloadName);
        Com_sprintf(errorMessage, sizeof(errorMessage), "File \"%s\" not found on server for autodownloading.\n", cl->downloadName);
      }

      MSG_Init(&msg, msgBuffer, sizeof(msgBuffer) - 8);
      MSG_WriteLong(&msg, cl->lastClientCommand);
      MSG_WriteByte(&msg, svc_download);
      MSG_WriteShort(&msg, 0); //client is expecting block zero
      MSG_WriteLong(&msg, -1); //illegal file size
      MSG_WriteString(&msg, errorMessage);
      MSG_WriteByte(&msg, svc_EOF);
      SV_Netchan_Transmit(cl, &msg);

      *cl->downloadName = '\0';

      if (cl->download != FS_INVALID_HANDLE)
      {
        FS_FCloseFile(cl->download);
        cl->download = FS_INVALID_HANDLE;
      }

      return qtrue;
    }

    Com_Printf("clientDownload: %d: beginning \"%s\"\n", ARRAY_INDEX(svs.clients, cl), cl->downloadName);
    cl->downloadCurrentBlock = cl->downloadClientBlock = cl->downloadXmitBlock = 0;
    cl->downloadSrcFileRemaining = cl->downloadSize;
    cl->downloadSrcChunkPos = 0;
    cl->downloadSrcChunkSize = 0;
    cl->downloadCurrentBlock = 0;
    cl->downloadSrcChunk = Z_Malloc(DOWNLOAD_READ_CHUNK_SIZE);
    cl->downloadSrcChunkPos = cl->downloadSrcChunkSize = 0;
    cl->downloadClientMsg = cl->downloadRetransmitMsg = cl->downloadCurrentMsg = 0;
    cl->downloadLastSentTime = Sys_Milliseconds();
    cl->downloadCurrentRate = DOWNLOAD_MAX_RATE;
    cl->downloadRatePool = 0;

    //reset the ack time to current when we start
    cl->downloadAckTime = svs.time;
  }

  //check if we have unsent fragments or queue already in the pipe and don't add to the issue (too much)
  if (cl->netchan_start_queue)
  {
    netchan_buffer_t *next = cl->netchan_start_queue;
    qint count = 0;

    while(next)
    {
      count++;
      next = next->next;

      if (count > 20)
      {
        return qfalse;
      }
    }
  }

  if (cl->netchan.unsentFragments && cl->netchan.unsentLength > 10000)
  {
    return qfalse;
  }

  //send next packet of fragmented message
  if (cl->netchan.unsentFragments || cl->netchan_start_queue)
  {
    SV_Netchan_TransmitNextFragment(cl);
    cl->downloadLastSentTime = Sys_Milliseconds();
    return qtrue;
  }

  //check acknowledged messages
  while(cl->downloadClientMsg < cl->downloadCurrentMsg)
  {
    downloadMessageRecord_t *record = &cl->downloadMsgTable[cl->downloadClientMsg % MAX_DOWNLOAD_MESSAGE_HISTORY];

    if (cl->messageAcknowledge < record->msgNumber)
    {
      break;
    }

    if (record->blockNumber >= cl->downloadClientBlock && cl->downloadClientMsg >= cl->downloadRetransmitMsg)
    {
      Com_Printf("clientDownload: %d: reset due to message acknowledge with dropped block\n", ARRAY_INDEX(svs.clients, cl));
      SV_DownloadRetransmit(cl);
    }

    ++cl->downloadClientMsg;
  }

  if (cl->downloadXmitBlock > 0 && !cl->downloadBlockSize[(cl->downloadXmitBlock - 1) % MAX_DOWNLOAD_WINDOW])
  {
    //sent the final block
    if (cl->downloadClientBlock >= cl->downloadXmitBlock)
    {
      //client already acknowledged, should not happen, download should be closed in SV_NextDownload_f
      Com_Printf(S_COLOR_YELLOW "clientDownload: %d: WARNING: attempt to write completed download\n", ARRAY_INDEX(svs.clients, cl));
      return qfalse;
    }
    else
    {
      Com_DPrintf("clientDownload: %d: skip due to final block sent\n", ARRAY_INDEX(svs.clients, cl));
      skip = qtrue;
    }
  }
  else if (cl->downloadXmitBlock - cl->downloadClientBlock >= MAX_DOWNLOAD_WINDOW)
  {
    Com_DPrintf("clientDownload: %d: skip due to download window (max blocks)\n", ARRAY_INDEX(svs.clients, cl));
    skip = qtrue;
  }
  else if (SV_DownloadCountOutgoingBytes(cl) > DOWNLOAD_WINDOW_BYTES(cl))
  {
    Com_DPrintf("clientDownload: %d: skip due to download window (max bytes)\n", ARRAY_INDEX(svs.clients, cl));
    skip = qtrue;
  }

  //if skip is set, either return here, or if 500ms has elapsed since last sent
  //message, continue forward to send keepalive message.
  if (skip && Sys_Milliseconds() - cl->downloadLastSentTime < 500)
  {
    return qfalse;
  }

  //send current block
  curindex = (cl->downloadXmitBlock % MAX_DOWNLOAD_WINDOW);
  MSG_Init(&msg, msgBuffer, sizeof(msgBuffer) - 8);
  MSG_WriteLong(&msg, cl->lastClientCommand);

  if (skip)
  {
    //send an empty message with no download block to update message sequence number on
    //the client, fixes potential deadlock due to dropped messages in which client is
    //stuck on old sequence number and server is stuck due to full download window
    Com_Printf("clientDownload: %d: writing keepalive message\n", ARRAY_INDEX(svs.clients, cl));
    MSG_WriteByte(&msg, svc_EOF);
    SV_Netchan_Transmit(cl, &msg);
    cl->downloadLastSentTime = Sys_Milliseconds();
    return qtrue;
  }

  MSG_WriteByte(&msg, svc_download);
  MSG_WriteShort(&msg, cl->downloadXmitBlock);

  //block zero is special, contains file size
  if (!cl->downloadXmitBlock)
  {
    MSG_WriteLong(&msg, cl->downloadSize);
  }

  //read next current block from pk3 if needed
  if (cl->downloadXmitBlock >= cl->downloadCurrentBlock)
  {
    if (cl->downloadXmitBlock != cl->downloadCurrentBlock)
    {
      //should not happen
      SV_DropClient(cl, "unexpected download current block number");
      return qfalse;
    }

    if (!SV_ReadDownloadBlock(cl, (qchar **)&cl->downloadBlocks[curindex], &cl->downloadBlockSize[curindex]))
    {
      SV_DropClient(cl, "failed to read download pk3");
      return qfalse;
    }

    cl->downloadCurrentBlock = cl->downloadXmitBlock + 1;
  }

  MSG_WriteShort(&msg, cl->downloadBlockSize[curindex]);

  //write the block
  if (cl->downloadBlockSize[curindex] > 0)
  {
    MSG_WriteData(&msg, cl->downloadBlocks[curindex], cl->downloadBlockSize[curindex]);
  }

  MSG_WriteByte(&msg, svc_EOF);
  SV_Netchan_Transmit(cl, &msg);
  Com_DPrintf("clientDownload: %d: writing block %d\n", ARRAY_INDEX(svs.clients, cl), cl->downloadXmitBlock);

  //in case of MAX_DOWNLOAD_MESSAGE_HISTORY overflow, delete top entry to make space
  if (cl->downloadCurrentMsg - cl->downloadClientMsg >= MAX_DOWNLOAD_MESSAGE_HISTORY)
  {
    Com_Printf("clientDownload: %d: message history overfow\n", ARRAY_INDEX(svs.clients, cl));
    ++cl->downloadClientMsg;
  }

  //generate message entry
  {
    downloadMessageRecord_t *record = &cl->downloadMsgTable[cl->downloadCurrentMsg % MAX_DOWNLOAD_MESSAGE_HISTORY];
    cl->downloadCurrentMsg++;
    record->blockNumber = cl->downloadXmitBlock;
    record->msgNumber = cl->netchan.outgoingSequence;
    record->size = cl->downloadBlockSize[curindex];
    Com_DPrintf("clientDownload: %d: outgoing size %i\n", ARRAY_INDEX(svs.clients, cl), SV_DownloadCountOutgoingBytes(cl));
  }

  cl->downloadLastSentTime = Sys_Milliseconds();

  //move on to the next block
  cl->downloadXmitBlock++;
  return qtrue;
}
#else
const qbool
SV_WriteDownloadToClient(client_t *cl, msg_t *msg)
{
  qint curindex;
  qchar errorMessage[1024];

  if (!*cl->downloadName)
  {
    return qfalse; //nothing being downloaded
  }

  //CVE-2006-2082: validate the download against the list of pak files
  if (!FS_VerifyPak(cl->downloadName))
  {
    //will drop the client and leave it hanging on the other side. good for him
    SV_DropClient(cl, "illegal download request");
    return qfalse;
  }

  if (cl->download == FS_INVALID_HANDLE)
  {
    //we open the file here
    if (!(sv_allowDownload->integer & DLF_ENABLE) || (sv_allowDownload->integer & DLF_NO_UDP) || unreferenced || (cl->downloadSize = FS_SV_FOpenFileRead(cl->downloadName, &cl->download)) < 0)
    {
      //cannot auto-download file
      if (!(sv_allowDownload->integer & DLF_ENABLE) || (sv_allowDownload->integer & DLF_NO_UDP))
      {
        Com_Printf("clientDownload: %d : \"%s\" download disabled", ARRAY_INDEX(svs.clients, cl), cl->downloadName);

        if (sv.pure)
        {
          Com_sprintf(errorMessage, sizeof(errorMessage), "could not download \"%s\" because autodownloading is disabled on the server\n\nyou will need to get this file elsewhere before you can connect to this pure server\n", cl->downloadName);
        }
        else
        {
          Com_sprintf(errorMessage, sizeof(errorMessage), "could not download \"%s\" because autodownloading is disabled on the server\n\nthe server you are connecting to is not a pure server, set autodownload to no in your settings and you might be able to join the game anyway\n", cl->downloadName);
        }
      }
      else
      {
        //NOTE TTimo this is NOT supposed to happen unless bug in our filesystem scheme?
        //if the pk3 is referenced, it must have been found somewhere in the filesystem
        Com_Printf("clientDownload: %d: \"%s\" file not found on server\n", ARRAY_INDEX(svs.clients, cl), cl->downloadName);
        Com_sprintf(errorMessage, sizeof(errorMessage), "File \"%s\" not found on server for autodownloading.\n", cl->downloadName);
      }

      MSG_WriteByte(msg, svc_download);
      MSG_WriteShort(msg, 0); //client is expecting block zero
      MSG_WriteLong(msg, -1); //illegal file size
      MSG_WriteString(msg, errorMessage);

      *cl->downloadName = '\0';

      if (cl->download != FS_INVALID_HANDLE)
      {
        FS_FCloseFile(cl->download);
        cl->download = FS_INVALID_HANDLE;
      }

      return qtrue;
    }

    Com_Printf("clientDownload: %d: beginning \"%s\"\n", ARRAY_INDEX(svs.clients, cl), cl->downloadName);
    cl->downloadCurrentBlock = cl->downloadClientBlock = cl->downloadXmitBlock = 0;
    cl->downloadCount = 0;
    cl->downloadEOF = qfalse;

    //reset the ack time to current when we start
    cl->downloadAckTime = svs.time;
  }

  //check if we have unsent fragments or queue already in the pipe and don't add to the issue (too much)
  if (cl->netchan_start_queue)
  {
    netchan_buffer_t *next = cl->netchan_start_queue;
    qint count = 0;

    while(next)
    {
      count++;
      next = next->next;

      if (count > 20)
      {
        return qfalse;
      }
    }
  }

  if (cl->netchan.unsentFragments && cl->netchan.unsentLength > 10000)
  {
    return qfalse;
  }

  //perform any reads that we need to
  while(cl->downloadCurrentBlock - cl->downloadClientBlock < MAX_DOWNLOAD_WINDOW && cl->downloadSize != cl->downloadCount)
  {
    curindex = (cl->downloadCurrentBlock % MAX_DOWNLOAD_WINDOW);

    if (!cl->downloadBlocks[curindex])
    {
      cl->downloadBlocks[curindex] = (unsigned qchar *)Z_Malloc(MAX_DOWNLOAD_BLKSIZE);
    }

    cl->downloadBlockSize[curindex] = FS_Read(cl->downloadBlocks[curindex], MAX_DOWNLOAD_BLKSIZE, cl->download);

    if (cl->downloadBlockSize[curindex] < 0)
    {
      //EOF right now
      cl->downloadCount = cl->downloadSize;
      break;
    }

    cl->downloadCount += cl->downloadBlockSize[curindex];

    //load in next block
    cl->downloadCurrentBlock++;
  }

  //check to see if we have eof condition and add the EOF block
  if (cl->downloadCount == cl->downloadSize && !cl->downloadEOF && cl->downloadCurrentBlock - cl->downloadClientBlock < MAX_DOWNLOAD_WINDOW)
  {
    cl->downloadBlockSize[cl->downloadCurrentBlock % MAX_DOWNLOAD_WINDOW] = 0;
    cl->downloadCurrentBlock++;
    cl->downloadEOF = qtrue; //we have added the EOF block
  }

  if (cl->downloadClientBlock == cl->downloadCurrentBlock)
  {
    return qfalse; //nothing to transmit
  }

  //write out the next section of the file, if we have already reached our window,
  //automatically start retransmitting
  if (cl->downloadXmitBlock == cl->downloadCurrentBlock)
  {
    //we have transmitted the complete window, should we start resending?
    if (svs.time - cl->downloadSendTime > 1000)
    {
      cl->downloadXmitBlock = cl->downloadClientBlock;
    }
    else
    {
      return qfalse;
    }
  }

  //send current block
  curindex = (cl->downloadXmitBlock % MAX_DOWNLOAD_WINDOW);

  MSG_WriteByte(msg, svc_download);
  MSG_WriteShort(msg, cl->downloadXmitBlock);

  //block zero is special, contains file size
  if (!cl->downloadXmitBlock)
  {
    MSG_WriteLong(msg, cl->downloadSize);
  }

  MSG_WriteShort(msg, cl->downloadBlockSize[curindex]);

  //write the block
  if (cl->downloadBlockSize[curindex])
  {
    MSG_WriteData(msg, cl->downloadBlocks[curindex], cl->downloadBlockSize[curindex]);
  }

  Com_DPrintf("clientDownload: %d: writing block %d\n", ARRAY_INDEX(svs.clients, cl), cl->downloadXmitBlock);

  //move on to the next block
  //it will get sent with next snap shot, the rate will keep us in line
  cl->downloadXmitBlock++;
  cl->downloadSendTime = svs.time;

  return qtrue;
}
#endif
/*
==================
SV_SendQueuedMessages

Send one round of fragments, or queued messages to all clients that have data pending.
Return the shortest time interval for sending next packet to client
==================
*/
const qint
SV_SendQueuedMessages(void)
{
  unsigned i;
  qint retval = -1;
  client_t *cl;

  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

#if defined(UDP_DOWNLOAD_OPTIMIZE)
    if (*cl->downloadName)
    {
      continue; //handled using SV_SendDownloadMessages
    }
#endif

    if (cl->state)
    {
      const qint nextFragT = SV_Netchan_TransmitNextFragment(cl);

      if (nextFragT >= 0 && (retval == -1 || retval > nextFragT))
      {
        retval = nextFragT;
      }
    }
  }

  return retval;
}

/*
==================
SV_SendDownloadMessages

Send one round of download messages to all clients
==================
*/
const qint
SV_SendDownloadMessages(void)
{
#if defined(UDP_DOWNLOAD_OPTIMIZE)
//pending bytes should hold up to 5ms worth of traffic, or ~1.5 packets, whichever is higher
#define MAX_PENDING_BYTES(rate) ((rate) * 5 > DOWNLOAD_RATE_PACKET_SIZE * 3 / 2 ? (rate) * 5:DOWNLOAD_RATE_PACKET_SIZE * 3 / 2)
  static qint lastTime;
  static unsigned currentClient = 0;
  static qint globalRatePool; //bytes available to send
  qint round;
  qint i;
  client_t *cl;
  qint timeElapsed = Sys_Milliseconds() - lastTime;
  const qint globalRate = sv_dlRate->integer > 0 && sv_dlRate->integer < 100000 ? sv_dlRate->integer:100000; //KB/s
  qbool downloadsActive = qfalse;

  //update elapsed time
  if (timeElapsed < 0)
  {
    timeElapsed = 0;
  }

  if (timeElapsed > 5)
  {
    timeElapsed = 5;
  }

  lastTime = Sys_Milliseconds();

  //increment global rate
  globalRatePool += globalRate * timeElapsed;

  if (globalRatePool > MAX_PENDING_BYTES(globalRate))
  {
    globalRatePool = MAX_PENDING_BYTES(globalRate);
  }

  //increment client rates
  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (cl->state >= CS_CONNECTED && *cl->downloadName)
    {
      cl->downloadRatePool += cl->downloadCurrentRate * timeElapsed;

      if (cl->downloadRatePool > MAX_PENDING_BYTES(DOWNLOAD_CLIENT_RATE(cl)))
      {
        cl->downloadRatePool = MAX_PENDING_BYTES(DOWNLOAD_CLIENT_RATE(cl));
      }
    }
  }

  //send download packets
  for(round = 0;round < DOWNLOAD_MAX_PACKETS_PER_MS;round++)
  {
    for(i = 0;i < sv.maxclients;i++)
    {
      cl = &svs.clients[currentClient];
      currentClient = (currentClient + 1) % sv.maxclients;

      if (cl->state >= CS_CONNECTED && *cl->downloadName)
      {
        downloadsActive = qtrue;

        if (globalRatePool < DOWNLOAD_RATE_PACKET_SIZE)
        {
          goto end;
        }

        if (cl->downloadCurrentRate > 0.0)
        {
          if (cl->downloadRatePool < DOWNLOAD_RATE_PACKET_SIZE)
          {
            continue;
          }

          cl->downloadRatePool -= DOWNLOAD_RATE_PACKET_SIZE;
        }

        if (SV_WriteDownloadToClient(cl))
        {
          globalRatePool -= DOWNLOAD_RATE_PACKET_SIZE;
        }
      }
    }

    if (!downloadsActive)
    {
      break;
    }
  }

end:
  //for now, just use 1ms wait when downloads are running
  return downloadsActive ? 1:INT_MAX;
#else
  qint i;
  qint numDLs;
  client_t *client;
  msg_t msg;
  byte msgBuffer[MAX_MSGLEN_BUF];

  numDLs = 0;

  for(i = 0;i < sv.maxclients;i++)
  {
    client = &svs.clients[i];

    if (client->state >= CS_CONNECTED && *client->downloadName)
    {
      if (client->netchan.unsentFragments)
      {
        SV_Netchan_TransmitNextFragment(client);
      }
      else
      {
        MSG_Init(&msg, msgBuffer, MAX_MSGLEN);
        MSG_WriteLong(&msg, client->lastClientCommand);
        const qbool retval = SV_WriteDownloadToClient(client, &msg);

        if (retval)
        {
          MSG_WriteByte(&msg, svc_EOF);
          SV_Netchan_Transmit(client, &msg);
          numDLs += retval;
        }
      }
    }
  }

  return numDLs;
#endif
}

#if defined(USE_VOIP)
/*
==================
SV_WriteVoipToClient

Check to see if there is any VoIP queued for a client, and send if there is.
==================
*/
const void
SV_WriteVoipToClient(client_t *cl, msg_t *msg)
{
  voipServerPacket_t *packet = &cl->voipPacket[0];
  qint totalbytes = 0;
  qint i;

  if (*cl->downloadName)
  {
    cl->queuedVoipPackets = 0;
    return; //no VoIP allowed if download is going, to save bandwidth.
  }

  //write as many VoIP packets as we reasonably can...
  for(i = 0;i < cl->queuedVoipPackets;i++, packet++)
  {
    totalbytes += packet->len;

    if (totalbytes > MAX_DOWNLOAD_BLKSIZE)
    {
      break;
    }

    //You have to start with a svc_EOF, so legacy clients drop the
    //rest of this packet. Otherwise, those without VoIP support will
    //see the svc_voip command, then panic and disconnect.
    //Generally we don't send VoIP packets to legacy clients, but this
    //serves as both a safety measure and a means to keep demo files
    //compatible.
    MSG_WriteByte(msg, svc_EOF);
    MSG_WriteByte(msg, svc_extension);
    MSG_WriteByte(msg, svc_voip);
    MSG_WriteShort(msg, packet->sender);
    MSG_WriteByte(msg, (byte) packet->generation);
    MSG_WriteLong(msg, packet->sequence);
    MSG_WriteByte(msg, packet->frames);
    MSG_WriteShort(msg, packet->len);
    MSG_WriteData(msg, packet->data, packet->len);
  }

  //!!! FIXME: I hate this queue system.
  cl->queuedVoipPackets -= i;

  if (cl->queuedVoipPackets > 0)
  {
    memmove(&cl->voipPacket[0], &cl->voipPacket[i], sizeof(voipServerPacket_t) * i);
  }
}
#endif


/*
=================
SV_Disconnect_f

The client is going to disconnect, so remove the connection immediately  FIXME: move to game?
=================
*/
static void
SV_Disconnect_f(client_t *cl)
{
  SV_DropClient(cl, "disconnected");
}

/*
=================
SV_VerifyPaks_f

If we are pure, disconnect the client if they do no meet the following conditions:

1. the first two checksums match our view of cgame and ui
2. there are no any additional checksums that we do not have

This routine would be a bit simpler with a goto but i abstained

=================
*/
static const void
SV_VerifyPaks_f(client_t *cl)
{
  qint nChkSum1;
  qint nChkSum2;
  qint nClientPaks;
  qint i;
  qint j;
  qint nCurArg;
  qint nClientChkSum[512];
  const qchar *pArg;
  qbool bGood = qtrue;

  //if we are pure, we "expect" the client to load certain things from 
  //certain pk3 files, namely we want the client to have loaded the
  //ui and cgame that we think should be loaded based on the pure setting
  //
  if (sv.pure)
  {
    nChkSum1 = nChkSum2 = 0;

    //we run the game, so determine which cgame and ui the client "should" be running
    bGood = FS_FileIsInPAK("vm/cgame.qvm", &nChkSum1, NULL);
    bGood &= FS_FileIsInPAK("vm/ui.qvm", &nChkSum2, NULL);

    nClientPaks = Cmd_Argc();

    if (nClientPaks > ARRAY_LEN(nClientChkSum))
    {
      nClientPaks = ARRAY_LEN(nClientChkSum);
    }

    //start at arg 2 (skip serverId cl_paks)
    nCurArg = 1;

    pArg = Cmd_Argv(nCurArg++);

    if (!*pArg)
    {
      bGood = qfalse;
    }
    else
    {
      //https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=475
      //we may get incoming cp sequences from a previous checksumFeed, which we need to ignore
      if (Q_atoi(pArg) != sv.serverId)
      {
        Com_DPrintf("ignoring outdated cp command from client %s\n", cl->name);
        return;
      }
    }
	
    //we basically use this while loop to avoid using 'goto' :)
    while(bGood)
    {
      //must be at least 6: "cl_paks cgame ui @ firstref ... numChecksums"
      //numChecksums is encoded
      if (nClientPaks < 6)
      {
        bGood = qfalse;
        break;
      }

      //verify first to be the cgame checksum
      pArg = Cmd_Argv(nCurArg++);

      if (!*pArg || *pArg == '@' || Q_atoi(pArg) != nChkSum1)
      {
        bGood = qfalse;
        break;
      }

      //verify the second to be the ui checksum
      pArg = Cmd_Argv(nCurArg++);

      if (!*pArg || *pArg == '@' || Q_atoi(pArg) != nChkSum2)
      {
        bGood = qfalse;
        break;
      }

      //should be sitting at the delimeter now
      pArg = Cmd_Argv(nCurArg++);

      if (*pArg != '@')
      {
        bGood = qfalse;
        break;
      }

      //store checksums since tokenization is not re-entrant
      for(i = 0;nCurArg < nClientPaks;i++)
      {
        nClientChkSum[i] = Q_atoi(Cmd_Argv(nCurArg++));
      }

      //store number to compare against (minus one cause the last is the number of checksums)
      nClientPaks = i - 1;

      //make sure none of the client check sums are the same
      //so the client can't send 5 the same checksums
      for(i = 0;i < nClientPaks;i++)
      {
        for(j = 0;j < nClientPaks;j++)
        {
          if (i == j)
          {
            continue;
          }

          if (nClientChkSum[i] == nClientChkSum[j])
          {
            bGood = qfalse;
            break;
          }
        }

        if (!bGood)
        {
          break;
        }
      }

      if (!bGood)
      {
        break;
      }

      //check if the client has provided any pure checksums of pk3 files not loaded by the server
      for(i = 0;i < nClientPaks;i++)
      {
        if (!FS_IsPureChecksum(nClientChkSum[i]))
        {
          bGood = qfalse;
          break;
        }
      }

      if (!bGood)
      {
        break;
      }

      //check if the number of checksums was correct
      nChkSum1 = sv.checksumFeed;

      for(i = 0;i < nClientPaks;i++)
      {
        nChkSum1 ^= nClientChkSum[i];
      }

      nChkSum1 ^= nClientPaks;

      if (nChkSum1 != nClientChkSum[nClientPaks])
      {
        bGood = qfalse;
        break;
      }

      //break out
      break;
    }

    cl->gotCP = qtrue;

    if (bGood)
    {
      cl->pureAuthentic = qtrue;
    }
    else
    {
      cl->pureAuthentic = qfalse;
      cl->lastSnapshotTime = svs.time - 9999; //generate a snapshot immediately
      cl->state = CS_ZOMBIE; //skip delta generation
      SV_SendClientSnapshot(cl);
      cl->state = CS_ACTIVE;
      SV_DropClient(cl, "Unpure client detected. Invalid .PK3 files referenced! You may need to set cl_allowDownload 1 if it isn't already set."); //Chey: should never happen... unless someone disabled http downloads on a newer client?
    }
  }
}

/*
=================
SV_ResetPureClient_f
=================
*/
static const void
SV_ResetPureClient_f(client_t *cl)
{
  cl->pureAuthentic = qfalse;
  cl->gotCP = qfalse;
}

/*
=================
SV_CheckFunstuffExploit

Make sure each comma-separated token of the specified userinfo key
is at most 13 characters to protect the game against buffer overflow.
=================
*/
static const qbool
SV_CheckFunstuffExploit(qchar *userinfo, const qchar *key)
{
  const qchar *token = Info_ValueForKey(userinfo, key);

  if (!token)
  {
    return qfalse;
  }

  while(token && *token)
  {
    qint len;
    const qchar *next = strchr(token, ',');

    if (next == NULL)
    {
      len = strlen(token);
      token = NULL;
    }
    else
    {
      len = next - token;
      token = next + 1;
    }

    if (len > 13)
    {
      Info_SetValueForKey(userinfo, key, "");
      return qtrue;
    }
  }

  return qfalse;
}

static const qchar *checkedTypeKeys[CHECKEDTYPE_TYPECOUNT] =
{
  "rate",
  "snaps"
};

/*
=================
SV_GetUserInfoKeyVerifiedNumber
=================
*/
static const qint
SV_GetUserInfoKeyVerifiedNumber(client_t *cl, checkedNumberType_t type)
{
  const qchar *valueString;
  const qchar *s;
  qbool invalidValue = qfalse;

  if (type >= CHECKEDTYPE_TYPECOUNT || type < 0)
  {
    Com_Error(ERR_FATAL, "SV_GetUserInfoKeyVerifiedNumber: unknown type %d", (qint)type);
    return 0;
  }

  if (cl->state != CS_ACTIVE)
  {
    return INT_MAX; //shut up double print
  }

  valueString = Info_ValueForKey(cl->userinfo, checkedTypeKeys[type]);

  if (!*valueString)
  {
    invalidValue = qtrue;
  }
  else
  {
    s = valueString;

    while(*s)
    {
      if (!((*s >= '0' && *s <= '9') || *s == '-'))
      {
        invalidValue = qtrue;
        break;
      }

      s++;
    }
  }

  if (invalidValue)
  {
    cl->invalidValues |= BIT((qint)type);
    Com_Printf(S_COLOR_YELLOW "Invalid '%s' value detected for client '%s' (%d): '%s'\n", checkedTypeKeys[type], cl->name, ARRAY_INDEX(svs.clients, cl), valueString);
  }
  else
  {
    cl->invalidValues &= ~BIT((qint)type);
  }

  cl->lastInvalidValuesWarning = 0;
  return Q_atoi(valueString);
}

/*
=================
SV_UserinfoChanged

Pull specific info from a newly changed userinfo string
into a more C friendly form.
=================
*/
const void
SV_UserinfoChanged(client_t *cl, const qbool updateUserinfo, const qbool runFilter)
{
  qchar buf[MAX_NAME_LENGTH];
  const qchar *val;
  const qchar *ip;
  qint i;

  if ((cl->netchan.remoteAddress.type == NA_BOT) || (cl->gentity && cl->gentity->r.svFlags & SVF_BOT))
  {
    cl->lastSnapshotTime = svs.time - 9999; //generate a snapshot immediately
    cl->snapshotMsec = 1000 / sv_fps->integer;
    cl->rate = sv_maxRate->integer;
    return;
  }

  //rate command

  //if the client is on the same subnet as the server and we aren't running an
  //internet public server, assume they don't need a rate choke
  cl->rate = SV_GetUserInfoKeyVerifiedNumber(cl, CHECKEDTYPE_RATE);
  cl->snaps = SV_GetUserInfoKeyVerifiedNumber(cl, CHECKEDTYPE_SNAPS);

  if (cl->netchan.remoteAddress.type == NA_LOOPBACK || (cl->netchan.isLANAddress && com_dedicated->integer != 2 && sv_lanForceRate->integer))
  {
    cl->rate = 100000; //lans should not rate limit
  }
  else
  {
    val = Info_ValueForKey(cl->userinfo, "rate");

    if (val[0])
    {
      cl->rate = Q_atoi(val);
    }
    else
    {
      cl->rate = 10000; //was 3000
    }

    if (sv_maxRate->integer)
    {
      if (cl->rate > sv_maxRate->integer)
      {
        cl->rate = sv_maxRate->integer;
      }
    }

    if (sv_minRate->integer)
    {
      if (cl->rate < sv_minRate->integer)
      {
        cl->rate = sv_minRate->integer;
      }
    }
  }

  //snaps command
  val = Info_ValueForKey(cl->userinfo, "snaps");

  if (val[0] && !NET_IsLocalAddress(&cl->netchan.remoteAddress))
  {
    i = Q_atoi(val);
  }
  else
  {
    i = sv_fps->integer; //sync with server
  }

  if (i < sv_minSnaps->integer)
  {
    i = sv_minSnaps->integer;
  }

  //range check
  if (i < 1)
  {
    i = 1;
  }
  else if (i > sv_fps->integer)
  {
    i = sv_fps->integer;
  }

  i = 1000 / i;

  if (i != cl->snapshotMsec)
  {
    //reset last sent snapshot to avoid desync between server and snapshot timings
    cl->lastSnapshotTime = svs.time - 9999; //generate a snapshot immediately
    cl->snapshotMsec = i;
  }
	
#if defined(USE_VOIP)
  //in the future, (val) will be a protocol version string, so only
  //accept explicitly 1, not generally non-zero.
  val = Info_ValueForKey(cl->userinfo, "cl_voip");
  cl->hasVoip = (Q_atoi(val) == 1) ? qtrue:qfalse;
#endif

  if (!updateUserinfo)
  {
    return;
  }

  //name for C code
  val = Info_ValueForKey(cl->userinfo, "name");

  //truncate if it is too long as it may cause memory corruption
  if (strlen(val) >= sizeof(buf))
  {
    Q_strncpyz(buf, val, sizeof(buf));
    Info_SetValueForKey(cl->userinfo, "name", buf);
    val = buf;
  }

  Q_strncpyz(cl->name, val, sizeof(cl->name));

  val = Info_ValueForKey(cl->userinfo, "handicap");

  if (val[0])
  {
    i = Q_atoi(val);

    if (i <= 0 || i > 100 || strlen(val) > 4)
    {
      Info_SetValueForKey(cl->userinfo, "handicap", "100"); //Chey: FIXME: use this?
    }
  }

  //TTimo
  //maintain the IP information
  //the banning code relies on this being consistently present
  if (NET_IsLocalAddress(&cl->netchan.remoteAddress))
  {
    ip = "localhost";
  }
  else
  {
    ip = NET_AdrToString(&cl->netchan.remoteAddress);
  }

  if (!Info_SetValueForKey(cl->userinfo, "ip", ip))
  {
    SV_DropClient(cl, "userinfo string length exceeded");
  }

  if (runFilter)
  {
    val = SV_RunFilters(cl->userinfo, &cl->netchan.remoteAddress);

    if (*val != '\0')
    {
      SV_DropClient(cl, val);
    }
  }

  if (SV_CheckFunstuffExploit(cl->userinfo, "funfree") || SV_CheckFunstuffExploit(cl->userinfo, "funred") || SV_CheckFunstuffExploit(cl->userinfo, "funblue"))
  {
    SV_WriteAttackLog(va(NULL, "funstuff exploit attempt from %s\n", NET_AdrToString(&cl->netchan.remoteAddress)));
  }
}


/*
==================
SV_UpdateUserinfo_f
==================
*/
#define INFO_CHANGE_MIN_INTERVAL 6000 //6 seconds is reasonable i suppose
#define INFO_CHANGE_MAX_COUNT 3 //only 3 changes allowed in the 6 seconds
const void
SV_UpdateUserinfo_f(client_t *cl)
{
  qchar *arg = Cmd_Argv(1);
  qchar info[MAX_INFO_STRING];

  //stop random empty userinfo calls without hurting anything
  if (!arg || !*arg)
  {
    return;
  }

  if (sv_userInfoFloodProtect->integer && cl->state >= CS_ACTIVE && svs.time < cl->nextReliableUserTime)
  {
    Q_strncpyz(cl->userinfobuffer, arg, sizeof(cl->userinfobuffer));
    SV_SendServerCommand(cl, "print \"command delayed due to userinfo flood protection\n\"");
    return;
  }

  cl->userinfobuffer[0] = '\0';
  cl->nextReliableUserTime = svs.time + 5000;

  if (cl->lastUserinfoChange > svs.time)
  {
    cl->lastUserinfoCount++;

    if (cl->lastUserinfoCount >= INFO_CHANGE_MAX_COUNT)
    {
      Q_strncpyz(cl->userinfoPostponed, arg, sizeof(cl->userinfoPostponed));
      SV_SendServerCommand(cl, "print \"too many info changes last info postponed\n\"\n");
      return;
    }
  }
  else
  {
    cl->userinfoPostponed[0] = '\0';
    cl->lastUserinfoCount = 0;
    cl->lastUserinfoChange = svs.time + INFO_CHANGE_MIN_INTERVAL;
  }

  Q_strncpyz(cl->userinfo, arg, sizeof(cl->userinfo));
  SV_FilterNameStringed(cl->userinfo);
  SV_UserinfoChanged(cl, qtrue, qtrue); //update userinfo, run filter

  //prog code to allow overrides
#if defined(USE_JAVA)
  Java_G_ClientUserInfoChanged(ARRAY_INDEX(svs.clients, cl));
#else
  VM_Call(sv.gvm, GAME_CLIENT_USERINFO_CHANGED, ARRAY_INDEX(svs.clients, cl));
#endif

  //get name out of game and send to engine
  SV_GetConfigstring(CS_PLAYERS + (ARRAY_INDEX(svs.clients, cl)), info, sizeof(info));
  Info_SetValueForKey(cl->userinfo, "name", Info_ValueForKey(info, "n"));
  Q_strncpyz(cl->name, Info_ValueForKey(info, "n"), sizeof(cl->name));
}


#if defined(USE_VOIP)
static void
SV_UpdateVoipIgnore(client_t *cl, const qchar *idstr, qbool ignore)
{
  if ((*idstr >= '0') && (*idstr <= '9'))
  {
    const qint id = Q_atoi(idstr);

    if ((id >= 0) && (id < MAX_CLIENTS))
    {
      cl->ignoreVoipFromClient[id] = ignore;
    }
  }
}

/*
==================
SV_UpdateUserinfo_f
==================
*/
static void
SV_Voip_f(client_t *cl)
{
  const qchar *cmd = Cmd_Argv(1);

  if (strcmp(cmd, "ignore") == 0)
  {
    SV_UpdateVoipIgnore(cl, Cmd_Argv(2), qtrue);
  }
  else if (strcmp(cmd, "unignore") == 0)
  {
    SV_UpdateVoipIgnore(cl, Cmd_Argv(2), qfalse);
  }
  else if (strcmp(cmd, "muteall") == 0)
  {
    cl->muteAllVoip = qtrue;
  }
  else if (strcmp(cmd, "unmuteall") == 0)
  {
    cl->muteAllVoip = qfalse;
  }
}
#endif


typedef struct
{
  const qchar *name;
  void (*func)(client_t *cl);
}
ucmd_t;

static ucmd_t ucmds[] =
{
  {"userinfo", SV_UpdateUserinfo_f},
  {"disconnect", SV_Disconnect_f},
  {"cp", SV_VerifyPaks_f},
  {"vdr", SV_ResetPureClient_f},
  {"download", SV_BeginDownload_f},
  {"nextdl", SV_NextDownload_f},
  {"stopdl", SV_StopDownload_f},
  {"donedl", SV_DoneDownload_f},
#if defined(USE_VOIP)
  {"voip", SV_Voip_f},
#endif
  {NULL, NULL}
};

/*
==================
SV_FloodProtect
==================
*/
static const qbool
SV_FloodProtect(client_t *cl)
{
  const unsigned now = Sys_Milliseconds();

  if (sv_floodProtect->integer)
  {
    return SVC_RateLimit(&cl->cmd_rate, sv_floodLimit->integer, sv_floodWait->integer, now);
  }
  else
  {
    return qfalse;
  }
}

/*
==================
SV_ExecuteClientCommand

Also called by bot code
==================
*/
const qbool
SV_ExecuteClientCommand(client_t *cl, const qchar *s)
{
  const unsigned now = Sys_Milliseconds();
  const ucmd_t *ucmd;
  qbool bFloodProtect;
  qbool isBot;
	
  Cmd_TokenizeString(s);

  //malicious users may try using too many string commands
  //to lag other players.  If we decide that we want to stall
  //the command, we will stop processing the rest of the packet,
  //including the usercmd.  This causes flooders to lag themselves
  //but not other people
  //We don't do this when the client hasn't been active yet since its
  //normal to spam a lot of commands when downloading
  //flood protection in game for trem

  //applying flood protection only to "CS_ACTIVE" clients leaves too much room for abuse, extending this flood protection to clients pre CS_ACTIVE should not cause any issues, as the download-commands are handled within the engine and floodprotect only filters calls to the qvm
  isBot = /*(*/(cl->netchan.remoteAddress.type == NA_BOT) /*|| (cl->gentity->r.svFlags & SVF_BOT))*/ ? qtrue:qfalse;
  bFloodProtect = !isBot; //&& cl->state >= CS_ACTIVE;

  //see if it is a server level command
  for(ucmd = ucmds;ucmd->name;ucmd++)
  {
    if (!Q_stricmp(Cmd_Argv(0), ucmd->name))
    {
      if (ucmd->func == SV_UpdateUserinfo_f)
      {
        if (bFloodProtect)
        {
          if (sv_userInfoFloodProtect->integer)
          {
            if (!SVC_RateLimit(&cl->info_rate, 5, 1000, now))
            {
              return qfalse; //lag flooder
            }
          }
        }
      }

      ucmd->func(cl);
      bFloodProtect = qfalse;
      break;
    }
  }

#if !defined(DEDICATED)
  if (!com_cl_running->integer && bFloodProtect && SV_FloodProtect(cl))
#else
  if (bFloodProtect && SV_FloodProtect(cl))
#endif
  {
    //ignore any other text messages from this client but let them keep playing
    Com_DPrintf("client text ignored for %s: %s\n", cl->name, Cmd_Argv(0));
  }
  else
  {
    //pass unknown strings to the game
    if (!ucmd->name && sv.state == SS_GAME && cl->state >= CS_PRIMED)
    {
      if (cl->state != CS_ACTIVE && !Q_strncmp(Cmd_Argv(0), "say", 3))
      {
        Com_DPrintf("client spam ignored for %s\n", cl->name);
        return qfalse;
      }

      Cmd_Args_Sanitize();

      if (sv_filterCommands->integer)
      {
        Cmd_Args_Sanitize2(MAX_CVAR_VALUE_STRING, "\n\r", "  ");

        if (sv_filterCommands->integer >= 2)
        {
          Cmd_Args_Sanitize2(MAX_CVAR_VALUE_STRING, ";", " ");
        }
      }

#if defined(USE_JAVA)
      Java_G_ClientCommand(ARRAY_INDEX(svs.clients, cl));
#else
      VM_Call(sv.gvm, GAME_CLIENT_COMMAND, ARRAY_INDEX(svs.clients, cl));
#endif
    }
  }

  return qtrue;
}

/*
===============
SV_ClientCommand
===============
*/
static const qbool
SV_ClientCommand(client_t *cl, msg_t *msg)
{
  qint seq;
  const qchar *s;

  seq = MSG_ReadLong(msg);
  s = MSG_ReadString(msg);

  //see if command was already executed
  if (seq - cl->lastClientCommand <= 0)
  {
    return qtrue;
  }

  Com_DPrintf("clientCommand: %s : %i : %s\n", cl->name, seq, s);

  //drop the connection if we have somehow lost commands
  if (seq - cl->lastClientCommand > 1)
  {
    Com_Printf("Client %s lost %i clientCommands\n", cl->name, seq - cl->lastClientCommand - 1);
    SV_DropClient(cl, "Lost reliable commands");
    return qfalse;
  }

  if (!SV_ExecuteClientCommand(cl, s))
  {
    return qfalse;
  }

  cl->lastClientCommand = seq;
  Q_strncpyz(cl->lastClientCommandString, s, sizeof(cl->lastClientCommandString));

  return qtrue; //continue procesing
}


//==================================================================================


/*
==================
SV_ClientThink

Also called by bot code
==================
*/
const void
SV_ClientThink(qint client, usercmd_t *cmd)
{
  client_t *cl;
  qchar info[MAX_INFO_STRING];

  if (client < 0 || sv.maxclients <= client)
  {
    Com_DPrintf(S_COLOR_YELLOW "SV_ClientThink: bad clientNum %i\n", client);
    return;
  }

  cl = &svs.clients[client];
  svs.clients[client].lastUsercmd = *cmd;

  if (cl->state != CS_ACTIVE)
  {
    return; //may have been kicked during the last usercmd
  }

  if (cl->lastUserinfoCount >= INFO_CHANGE_MAX_COUNT && cl->lastUserinfoChange < svs.time && cl->userinfoPostponed[0])
  {
    Q_strncpyz(cl->userinfo, cl->userinfoPostponed, sizeof(cl->userinfo));
    SV_UserinfoChanged(cl, qtrue, qfalse); //update userinfo, do not run filter

    //call prog code to allow overrides
#if defined(USE_JAVA)
    Java_G_ClientUserInfoChanged(ARRAY_INDEX(svs.clients, cl));
#else
    VM_Call(sv.gvm, GAME_CLIENT_USERINFO_CHANGED, ARRAY_INDEX(svs.clients, cl));
#endif

    //get the name out of the game and set it in the engine
    SV_GetConfigstring(CS_PLAYERS + (ARRAY_INDEX(svs.clients, cl)), info, sizeof(info));
    Info_SetValueForKey(cl->userinfo, "name", Info_ValueForKey(info, "n"));
    Q_strncpyz(cl->name, Info_ValueForKey(info, "n"), sizeof(cl->name));

    //clear it
    cl->userinfoPostponed[0] = 0;
    cl->lastUserinfoCount = 0;
    cl->lastUserinfoChange = svs.time + INFO_CHANGE_MIN_INTERVAL;
  }

#if defined(USE_JAVA)
  Java_G_ClientThink(ARRAY_INDEX(svs.clients, cl));
#else
  VM_Call(sv.gvm, GAME_CLIENT_THINK, ARRAY_INDEX(svs.clients, cl));
#endif
}

/*
==================
SV_UserMove

The message usually contains all the movement commands 
that were in the last three packets, so that the information
in dropped packets can be recovered.

On very fast clients, there may be multiple usercmd packed into
each of the backup packets.
==================
*/
static const void
SV_UserMove(client_t *cl, msg_t *msg, qbool delta)
{
  qint i;
  qint key;
  qint cmdCount;
  const unsigned now = Sys_Milliseconds();
  static const usercmd_t nullcmd = {0};
  usercmd_t cmds[MAX_PACKET_USERCMDS];
  usercmd_t *cmd;
  const usercmd_t *oldcmd;

  if (delta)
  {
    cl->deltaMessage = cl->messageAcknowledge;
  }
  else
  {
    cl->deltaMessage = cl->netchan.outgoingSequence - (PACKET_BACKUP + 1); //force delta reset
  }

  cmdCount = MSG_ReadByte(msg);

  if (cmdCount < 1)
  {
    Com_Printf("cmdCount < 1\n");
    return;
  }

  if (cmdCount > MAX_PACKET_USERCMDS)
  {
    Com_Printf("cmdCount > MAX_PACKET_USERCMDS\n");
    return;
  }

  //use the checksum feed in the key
  key = sv.checksumFeed;

  //also use the message acknowledge
  key ^= cl->messageAcknowledge;

  //also use the last acknowledged server command in the key
  key ^= MSG_HashKey(cl->reliableCommands[cl->reliableAcknowledge & (MAX_RELIABLE_COMMANDS - 1)], 32);

  oldcmd = &nullcmd;

  for(i = 0;i < cmdCount;i++)
  {
    cmd = &cmds[i];
    MSG_ReadDeltaUsercmdKey(msg, key, oldcmd, cmd);
    oldcmd = cmd;
  }

  //save time for ping calculation
  if (cl->frames[ cl->messageAcknowledge & PACKET_MASK ].messageAcked <= 0)
  {
    cl->frames[cl->messageAcknowledge & PACKET_MASK].messageAcked = Sys_Milliseconds();
  }
	
  //if this is the first usercmd we have received
  //this gamestate, put the client into the world
  if (cl->state == CS_PRIMED)
  {
    if (sv.pure && !cl->gotCP)
    {
      //we didn't get a cp yet, don't assume anything and just send the gamestate all over again
      if (sv_protect->integer & SVP_XREAL)
      {
        if (!SVC_RateLimit(&cl->gamestate_rate, 2, 1000, now))
        {
          Com_DPrintf("%s: didn't get cp command, resending gamestate\n", cl->name);
          SV_SendClientGameState(cl);
        }
      }
      else
      {
        Com_DPrintf("%s: didn't get cp command, resending gamestate\n", cl->name);
        SV_SendClientGameState(cl);
      }

      return;
    }

    SV_ClientEnterWorld(cl);
    //the moves can be processed normaly
  }
	
  //a bad cp command was sent, drop the client
  if (sv.pure && !cl->pureAuthentic)
  {		
    SV_DropClient(cl, "Cannot validate pure client!");
    return;
  }

  if (cl->state != CS_ACTIVE)
  {
    cl->deltaMessage = cl->netchan.outgoingSequence - (PACKET_BACKUP + 1); //force delta reset
    return;
  }

  //usually, the first couple commands will be duplicates
  //of ones we have previously received, but the servertimes
  //in the commands will cause them to be immediately discarded
  for(i = 0;i < cmdCount;i++)
  {
    //if this is a cmd from before a map_restart ignore it
    if (cmds[i].serverTime - cmds[cmdCount-1].serverTime > 0)
    {
      continue;
    }

    //extremely lagged or cmd from before a map_restart
    //if (cmds[i].serverTime > svs.time + 3000)
    //{
      //continue;
    //}

    //don't execute if this is an old cmd which is already executed
    //these old cmds are included when cl_packetdup > 0
    //if (cmds[i].serverTime <= cl->lastUsercmd.serverTime)
    if (cmds[i].serverTime - cl->lastUsercmd.serverTime <= 0)
    {
      continue;
    }

    SV_ClientThink(ARRAY_INDEX(svs.clients, cl), &cmds[i]);
  }
}


#if defined(USE_VOIP)
static const qbool
SV_ShouldIgnoreVoipSender(const client_t *cl)
{
  if (!sv_voip->integer)
  {
    return qtrue; //VoIP disabled on this server.
  }
  else if (!cl->hasVoip) //client doesn't have VoIP support?!
  {
    return qtrue;
  }
    
  //!!! FIXME: implement player blacklist.

  return qfalse; //don't ignore.
}

static const void
SV_UserVoip(client_t *cl, msg_t *msg)
{
  const qint sender = ARRAY_INDEX(svs.clients, cl);
  const qint generation = MSG_ReadByte(msg);
  const qint sequence = MSG_ReadLong(msg);
  const qint frames = MSG_ReadByte(msg);
  const qint recip1 = MSG_ReadLong(msg);
  const qint recip2 = MSG_ReadLong(msg);
  const qint recip3 = MSG_ReadLong(msg);
  const qint packetsize = MSG_ReadShort(msg);
  byte encoded[sizeof(cl->voipPacket[0].data)];
  client_t *client = NULL;
  voipServerPacket_t *packet = NULL;
  qint i;

  if (generation < 0)
  {
    return; //short/invalid packet, bail.
  }
  else if (sequence < 0)
  {
    return; //short/invalid packet, bail.
  }
  else if (frames < 0)
  {
    return; //short/invalid packet, bail.
  }
  else if (recip1 < 0)
  {
    return; //short/invalid packet, bail.
  }
  else if (recip2 < 0)
  {
    return; //short/invalid packet, bail.
  }
  else if (recip3 < 0)
  {
    return; //short/invalid packet, bail.
  }
  else if (packetsize < 0)
  {
    return; //short/invalid packet, bail.
  }

  if (packetsize > sizeof(encoded)) //overlarge packet?
  {
    qint bytesleft = packetsize;

    while(bytesleft)
    {
      qint br = bytesleft;

      if (br > sizeof (encoded))
      {
        br = sizeof (encoded);
      }

      MSG_ReadData(msg, encoded, br);
      bytesleft -= br;
    }

    return; //overlarge packet, bail.
  }

  MSG_ReadData(msg, encoded, packetsize);

  if (SV_ShouldIgnoreVoipSender(cl))
  {
    return; //blacklisted, disabled, etc.
  }

  //!!! FIXME: see if we read past end of msg...

  //!!! FIXME: reject if not speex narrowband codec.
  //!!! FIXME: decide if this is bogus data?

  //(the three recip* values are 31 bits each (ignores sign bit so we can
  //get a -1 error from MSG_ReadLong() ...), allowing for 93 clients.)
  assert(sv.maxclients < 93);

  //decide who needs this VoIP packet sent to them...
  for(i = 0;i < sv.maxclients;i++)
  {
    client = &svs.clients[i];

    if (client->state != CS_ACTIVE)
    {
      continue; //not in the game yet, don't send to this guy.
    }
    else if (i == sender)
    {
      continue; //don't send voice packet back to original author.
    }
    else if (!client->hasVoip)
    {
      continue; //no VoIP support, or support disabled.
    }
    else if (client->muteAllVoip)
    {
      continue; //client is ignoring everyone.
    }
    else if (client->ignoreVoipFromClient[sender])
    {
      continue; //client is ignoring this talker.
    }
    else if (*cl->downloadName) //!!! FIXME: possible to DoS?
    {
      continue; //no VoIP allowed if downloading, to save bandwidth.
    }
    else if (((i >= 0) && (i < 31)) && ((recip1 & (1 << (i-0))) == 0))
    {
      continue; //not addressed to this player.
    }
    else if (((i >= 31) && (i < 62)) && ((recip2 & (1 << (i-31))) == 0))
    {
      continue; //not addressed to this player.
    }
    else if (((i >= 62) && (i < 93)) && ((recip3 & (1 << (i-62))) == 0))
    {
      continue; //not addressed to this player.
    }

    //Transmit this packet to the client.
    //!!! FIXME: I don't like this queueing system.
    if (client->queuedVoipPackets >= ARRAY_LEN(client->voipPacket))
    {
      Com_Printf("Too many VoIP packets queued for client #%d\n", i);
      continue; //no room for another packet right now.
    }

    packet = &client->voipPacket[client->queuedVoipPackets];
    packet->sender = sender;
    packet->frames = frames;
    packet->len = packetsize;
    packet->generation = generation;
    packet->sequence = sequence;
    Com_Memcpy(packet->data, encoded, packetsize);
    client->queuedVoipPackets++;
  }
}
#endif



/*
===========================================================================

USER CMD EXECUTION

===========================================================================
*/

#if !defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
/*
===================
SV_AcknowledgeGamestate
===================
*/
static const qbool
SV_AcknowledgeGamestate(client_t *cl, qint serverId)
{
  if (serverId == sv.serverId)
  {
    const qint messageDelta = cl->messageAcknowledge - cl->gamestateMessageNum;

    //accept either exact message delta or any positive delta with known identical gamestate sent before
    if (!messageDelta || (messageDelta > 0 && cl->gamestateAck == GSA_SENT_ONCE))
    {
      cl->gamestateAck = GSA_ACKED;
      //this client has acknowledged the new gamestate so it's
      //safe to start sending it the real time again
      Com_DPrintf("%s acknowledged gamestate\n", cl->name);
      cl->oldServerTime = 0;
      return qtrue;
    }
  }

  return qfalse;
}
#endif

/*
===================
SV_ExecuteClientMessage

Parse a client packet
===================
*/
const void
SV_ExecuteClientMessage(client_t *cl, msg_t *msg)
{
  const unsigned now = Sys_Milliseconds();
  qint c;

  MSG_Bitstream(msg);
  const qint serverId = MSG_ReadLong(msg);
  cl->messageAcknowledge = MSG_ReadLong(msg);

  //if (cl->messageAcknowledge < 0)
  if (cl->netchan.outgoingSequence - cl->messageAcknowledge <= 0)
  {
    //usually only hackers create messages like this
    //it is more annoying for them to let them hanging
#if defined(_DEBUG)
    SV_DropClient(cl, "DEBUG: illegible client message");
#endif
    return;
  }

  const qint reliableAcknowledge = MSG_ReadLong(msg);

  if (cl->reliableSequence - reliableAcknowledge < 0)
  {
#if defined(_DEBUG)
    SV_DropClient(cl, "DEBUG: illegible client message");
#endif
    return;
  }

  //NOTE: when the client message is fux0red the acknowledgement numbers
  //can be out of range, this could cause the server to send thousands of server
  //commands which the server thinks are not yet acknowledged in SV_UpdateServerCommandsToClient
  if (reliableAcknowledge < 0 || cl->reliableSequence - reliableAcknowledge > MAX_RELIABLE_COMMANDS || reliableAcknowledge > cl->reliableSequence)
  {
    //usually only hackers create messages like this
    //it is more annoying for them to let them hanging
#if defined(_DEBUG)
    SV_DropClient(cl, "DEBUG: illegible client message");
#else
    Com_Printf(S_COLOR_YELLOW "WARNING: dropping %i commands from %s\n", cl->reliableSequence - cl->reliableAcknowledge, cl->name);
#endif

    cl->reliableAcknowledge = cl->reliableSequence;
    return;
  }

  cl->reliableAcknowledge = reliableAcknowledge;
  cl->justConnected = qfalse;

#if defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
  if (cl->oldServerTime && serverId == sv.serverId)
  {
    //this client has acknowledged the new gamestate so it is safe to start sending it the real time again
    Com_DPrintf("%s acknowledged gamestate\n", cl->name);
    cl->oldServerTime = 0;
  }
#else
  //if this is a usercmd from a previous gamestate,
  //ignore it or retransmit the current gamestate
  //
  //if the client was downloading, let it stay at whatever serverId and
  //gamestate it was at.  This allows it to keep downloading even when
  //the gamestate changes.  After the download is finished, we'll
  //notice and send it a new game state
  //
  //https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=536
  //don't drop as long as previous command was a nextdl, after a dl is done, downloadName is set back to ""
  //but we still need to read the next message to move to next download or send gamestate
  //I don't like this hack though, it must have been working fine at some point, suspecting the fix is somewhere else

  //if the client has not been sent the gamestate yet, or if we can tell that the client has dropped the last gamestate we sent them, resend it
  if (cl->state == CS_CONNECTED)
  {
    if (!cl->downloading)
    {
      //send initial gamestate, client may not acknowledge it in next command but start downloading after SV_ClientCommand()
      if (sv_protect->integer & SVP_XREAL)
      {
        if (!SVC_RateLimit(&cl->gamestate_rate, 1, 1000, now))
        {
          Com_DPrintf("%s: sending gamestate\n", cl->name);
          SV_SendClientGameState(cl);
        }
      }
      else
      {
        Com_DPrintf("%s: sending gamestate\n", cl->name);
        SV_SendClientGameState(cl);
      }

      return;
    }
  }
  else if (cl->gamestateAck != GSA_ACKED)
  {
    //early check for gamestate acknowledge
    SV_AcknowledgeGamestate(cl, serverId);
  }
  //else if (cl->state == CS_PRIMED)
  //{
    //in case of download intention client replies with (messageAcknowledge - gamestateMessageNum) >= 0 and (serverId == sv.serverId), sv.serverId can drift away later
    //in case of lost gamestate client replies with (messageAcknowledge - gamestateMessageNum) > 0 and (serverId == sv.serverId)
    //in case of disconnect/etc. client replies with any serverId
  //}
#endif
  //read optional clientCommand strings
  do
  {
    c = MSG_ReadByte(msg);

#if 0
    //See if this is an extension command after the EOF, which means we
    //got data that a legacy server should ignore.
    if ((c == clc_EOF) && (MSG_LookaheadByte(msg) == clc_extension))
    {
      MSG_ReadByte(msg); //throw the clc_extension byte away.
      c = MSG_ReadByte(msg); //something legacy servers can't do!
      //sometimes you get a clc_extension at end of stream...dangling
      //bits in the huffman decoder giving a bogus value?
      if (c == -1)
      {
        c = clc_EOF;
      }
    }
#endif
    if (c == clc_EOF)
    {
      break;
    }

    if (c != clc_clientCommand)
    {
      break;
    }

    if (!SV_ClientCommand(cl, msg))
    {
      return; //we couldn't execute it because of the flood protection
    }

    if (cl->state == CS_ZOMBIE)
    {
      return; //disconnect command
    }
  }
  while(1);

#if defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
  if (cl->downloading)
  {
    return; //waiting for donedl
  }

  //check for sending initial gamestate
  if (cl->state == CS_CONNECTED)
  {
    if (sv_protect->integer & SVP_XREAL)
    {
      if (!SVC_RateLimit(&cl->gamestate_rate, 2, 1000, now))
      {
        SV_SendClientGameState(cl);
      }
    }
    else
    {
      SV_SendClientGameState(cl);
    }

    return;
  }

  //check for dropped gamestate
  if (cl->state != CS_ACTIVE && serverId != sv.serverId)
  {
    if (cl->messageAcknowledge - cl->gamestateMessageNum > 0)
    {
      if (sv_protect->integer & SVP_XREAL)
      {
        if (!SVC_RateLimit(&cl->gamestate_rate, 2, 1000, now))
        {
          Com_DPrintf("%s: %s gamestate, resending\n", cl->name, serverId != sv.serverId ? "outdated":"dropped");
          SV_SendClientGameState(cl);
        }
      }
      else
      {
        Com_DPrintf("%s: %s gamestate, resending\n", cl->name, serverId != sv.serverId ? "outdated":"dropped");
        SV_SendClientGameState(cl);
      }
    }

    return;
  }
#else
#if 0  //Chey: FIXME: this causes stock 1.1 to get stuck in an infinite gamestate resend loop specifically after downloading completes
  if (cl->gamestateAck != GSA_ACKED)
  {
    //late check for gamestate resend
    if (cl->state == CS_PRIMED)
    {
      if (!SV_AcknowledgeGamestate(cl, serverId))
      {
        Com_DPrintf("%s: dropped gamestate, resending\n", cl->name);

        if (sv_protect->integer & SVP_XREAL)
        {
          if (!SVC_RateLimit(&cl->gamestate_rate, 1, 1000, now))
          {
            SV_SendClientGameState(cl);
          }
        }
        else
        {
          SV_SendClientGameState(cl);
        }

        return; //message delta or serverId mismatch
      }
    }
    else
    {
      return; //cl->state <= CS_CONNECTED
    }
  }
#endif
#endif
  //read voip data
  if (c == clc_voip)
  {
#if defined(USE_VOIP)
    SV_UserVoip(cl, msg);
    c = MSG_ReadByte(msg);
#endif
  }

  //read the usercmd_t
  if (c == clc_move)
  {
    SV_UserMove(cl, msg, qtrue);
  }
  else if (c == clc_moveNoDelta)
  {
    SV_UserMove(cl, msg, qfalse);
  }
  else if (c != clc_EOF)
  {
    Com_Printf("WARNING: bad command byte %i for client %i\n", c, ARRAY_INDEX(svs.clients, cl));
  }

  if (msg->readcount != msg->cursize && sv_collectClientJunkInfo->integer)
  {
    Com_DPrintf("WARNING: junk at end of packet for client %i\n", ARRAY_INDEX(svs.clients, cl));
  }

#if defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
  //extra check for dropped gamestate for post udp download clients, since after
  //download client can have correct serverid but still be awaiting gamestate
  if (cl->downloadGamestateDropCheck)
  {
    const qint gsDelta = cl->messageAcknowledge - cl->gamestateMessageNum;

    //either a move command (implied by CS_ACTIVE) or exact acknowledge of
    //gamestate message number implies client has the new gamestate
    if (cl->state == CS_ACTIVE || !gsDelta)
    {
      Com_DPrintf("%s: acknowledged post-download gamestate (state: %i gsDelta: %i)\n", cl->name, cl->state, gsDelta);
      cl->downloadGamestateDropCheck = qfalse;
    }
    else if (gsDelta > 20)
    {
      if (sv_protect->integer & SVP_XREAL)
      {
        if (!SVC_RateLimit(&cl->gamestate_rate, 2, 1000, now))
        {
          Com_DPrintf("%s: dropped post-download gamestate, resending\n", cl->name);
          SV_SendClientGameState(cl);
        }
      }
      else
      {
        Com_DPrintf("%s: dropped post-download gamestate, resending\n", cl->name);
        SV_SendClientGameState(cl);
      }
    }
  }
#endif
}
