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
static void
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
  challenge &= (1U << 31) - 1;
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
void
SV_GetChallenge(const netadr_t *from)
{
  qint challenge;
  qint clientChallenge;

  //prevent using getchallenge as an amplifier
  if (SVC_RateLimitAddress(from, 10, 1000))
  {
    if (com_developer->integer)
    {
      Com_Printf("SV_GetChallenge: rate limit from %s exceeded, dropping request\n", NET_AdrToString(from));
    }

    return;
  }

  //create a unique challenge for this client without storing state on server
#if defined(STATELESS_CHALLENGES_VERSION_ONE)
    challenge = SV_CreateChallenge(from);
#else
    challenge = SV_CreateChallenge(svs.time >> TS_SHIFT, from);
#endif

  if (Cmd_Argc() < 2)
  {
    //legacy client query so do not send not needed info
    NET_OutOfBandPrint(NS_SERVER, from, "challengeResponse %i", challenge);
  }
  else
  {
    qint sv_proto = com_protocol->integer;

    if (sv_proto == DEFAULT_PROTOCOL_VERSION)
    {
      //we support new protocol features by default
      sv_proto = NEW_PROTOCOL_VERSION;
    }

    //grab the client's challenge to echo back if given
    clientChallenge = Q_atoi(Cmd_Argv(1));

    NET_OutOfBandPrint(NS_SERVER, from, "challengeResponse %i %i %i", challenge, clientChallenge, sv_proto);
  }
}

/*
==================
SV_SetClientTLD
==================
*/
#pragma pack(push, 1)

typedef struct
iprange_s
{
  uint32_t from;
  uint32_t to;
}
iprange_t;

typedef struct
iprange_tld_s
{
  qchar tld[2];
}
iprange_tld_t;

#pragma pack(pop)

static qbool ipdb_loaded;
static iprange_t *ipdb_range;
static iprange_tld_t *ipdb_tld;
static qint num_tlds;

typedef struct
tld_info_s
{
  const qchar *tld;
  const qchar *country;
}
tld_info_t;

static const tld_info_t tld_info[] =
{
  #include "tlds.h"
};

/*
==================
SV_FreeIP4DB
==================
*/
void
SV_FreeIP4DB(void)
{
  if (ipdb_range)
  {
    Z_Free(ipdb_range);
  }

  ipdb_loaded = qfalse;
  ipdb_range = NULL;
  ipdb_tld = NULL;
}

/*
==================
SV_LoadIP4DB

Loads geoip database into memory
==================
*/
static qbool
SV_LoadIP4DB(const qchar *filename)
{
  fileHandle_t fh = FS_INVALID_HANDLE;
  uint32_t last_ip;
  void *buf;
  qint len;
  qint res;
  qint i;

  len = FS_SV_FOpenFileRead(filename, &fh);

  if (len <= 0)
  {
    if (fh != FS_INVALID_HANDLE)
    {
      FS_FCloseFile(fh);
    }

    return qfalse;
  }

  if (len % 10) //should be a power of IP4:IP4:TLD2
  {
    Com_DPrintf("%s(%s): invalid file size %i\n", __func__, filename, len);

    if (fh != FS_INVALID_HANDLE)
    {
      FS_FCloseFile(fh);
    }

    return qfalse;
  }

  SV_FreeIP4DB();

  buf = Z_Malloc(len);

  res = FS_Read(buf, len, fh);
  FS_FCloseFile(fh);

  if (res != len)
  {
    Z_Free(buf);
    return qfalse;
  }

  //check integrity of loaded database
  last_ip = 0;
  num_tlds = len / 10;

  //database format:
  //[range1][range2]...[rangeN]
  //[tld1][tld2]...[tldN]

  ipdb_range = (iprange_t *)buf;
  ipdb_tld = (iprange_tld_t *)(ipdb_range + num_tlds);

  for(i = 0;i < num_tlds;i++)
  {
#if defined(Q3_LITTLE_ENDIAN)
    ipdb_range[i].from = LongSwap(ipdb_range[i].from);
    ipdb_range[i].to = LongSwap(ipdb_range[i].to);
#endif

    if (last_ip && last_ip >= ipdb_range[i].from)
    {
      break;
    }

    if (ipdb_range[i].from > ipdb_range[i].to)
    {
      break;
    }

    if (ipdb_tld[i].tld[0] < 'A' || ipdb_tld[i].tld[0] > 'Z' || ipdb_tld[i].tld[1] < 'A' || ipdb_tld[i].tld[1] < 'Z')
    {
      break;
    }

    last_ip = ipdb_range[i].to;
  }

  if (i != num_tlds)
  {
    Com_Printf(S_COLOR_YELLOW "invalid ip4db entry #%i: range=[%08x..%08x], tld=%c%c\n", i, ipdb_range[i].from, ipdb_range[i].to, ipdb_tld[i].tld[0], ipdb_tld[i].tld[1]);
    SV_FreeIP4DB();
    return qtrue; //to not try to load it again
  }

  Com_Printf("ip4db: %i entries loaded\n", num_tlds);
  return qtrue;
}

static void
SV_SetTLD(qchar *str, const netadr_t *from, qbool isLAN)
{
  const iprange_t *e;
  qint lo;
  qint hi;
  qint m;
  uint32_t ip;

  str[0] = '\0';

  if (!sv_clientTLD->integer)
  {
    return;
  }

  if (isLAN)
  {
    strcpy(str, "**");
    return;
  }

  if (from->type != NA_IP) //ipv4-only
  {
    return;
  }

  if (!ipdb_loaded)
  {
    ipdb_loaded = SV_LoadIP4DB("ip4db.dat");
  }

  if (!ipdb_range)
  {
    return;
  }

  lo = 0;
  hi = num_tlds;

  //big-endian to host-endian
#if defined(Q3_LITTLE_ENDIAN)
  ip = from->ipv._4[3] | from->ipv._4[2] << 8 | from->ipv._4[1] << 16 | from->ipv._4[0] << 24;
#else
  ip = from->ipv._4[0] | from->ipv._4[1] << 8 | from->ipv._4[2] << 16 | from->ipv._4[3] << 24;
#endif

  //binary search
  while(lo <= hi)
  {
    m = (lo + hi) / 2;
    e = ipdb_range + m;

    if (ip >= e->from && ip <= e->to)
    {
      const iprange_tld_t *tld = ipdb_tld + m;
      str[0] = tld->tld[0];
      str[1] = tld->tld[1];
      str[2] = '\0';
      return;
    }

    if (e->from > ip)
    {
      hi = m - 1;
    }
    else
    {
      lo = m + 1;
    }
  }
}

static qint seqs[MAX_CLIENTS];

static void
SV_SaveSequences(void)
{
  qint i;

  for(i = 0;i < sv.maxclients;i++)
  {
    seqs[i] = svs.clients[i].reliableSequence;
  }
}

static void
SV_InjectLocation(const char *tld, const char *country)
{
  qchar *cmd;
  qchar *str;
  qint i;
  qint n;

  for(i = 0;i < sv.maxclients;i++)
  {
    if (seqs[i] != svs.clients[i].reliableSequence)
    {
      for(n = seqs[i];n != svs.clients[i].reliableSequence + 1;n++)
      {
        cmd = svs.clients[i].reliableCommands[n & (MAX_RELIABLE_COMMANDS - 1)];
        str = strstr(cmd, "connected\n\"");

        if (str && str[11] == '\0' && str < cmd + 512)
        {
          if (*tld == '\0')
          {
            sprintf(str, S_COLOR_WHITE "connected (%s)\n\"", country);
          }
          else
          {
            sprintf(str, S_COLOR_WHITE "connected (" S_COLOR_RED "%s" S_COLOR_WHITE ", %s)\n\"", tld, country);
          }

          break;
        }
      }
    }
  }
}

static const qchar *
SV_FindCountry(const qchar *tld)
{
  qint i;

  if (*tld == '\0')
  {
    return "Unknown Location";
  }

  for(i = 0;i < ARRAY_LEN(tld_info);i++)
  {
    if (!strcmp(tld, tld_info[i].tld))
    {
      return tld_info[i].country;
    }
  }

  return "Unknown Location";
}

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
void
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
SV_DirectConnect

A "connect" OOB command has been received
==================
*/
void
SV_DirectConnect(const netadr_t *from)
{
  static rateLimit_t bucket;
  qchar userinfo[MAX_INFO_STRING];
  qchar tld[3];
  qint i;
  qint n;
  client_t *cl;
  client_t *newcl;
  //sharedEntity_t *ent;
  qint clientNum;
  qint qport;
  qint challenge;
  const qchar *password;
  qint startIndex;
  intptr_t denied;
  qint count;
  qint cl_proto;
  qint sv_proto;
  const qchar *ip;
  const qchar *v;
  const qchar *info;
  qbool compat;
  qbool longstr;

  Com_DPrintf("SVC_DirectConnect()\n");

  //prevent using connect as an amplifier
  if (SVC_RateLimitAddress(from, 10, 1000))
  {
    if (com_developer->integer)
    {
      Com_Printf("SV_DirectConnect: rate limit from %s exceeded, dropping request\n", NET_AdrToString(from));
    }

    return;
  }

  for(i = 0, n = 0;i < sv.maxclients;i++)
  {
    const netadr_t *addr = &svs.clients[i].netchan.remoteAddress;

    if (addr->type != NA_BOT && NET_CompareBaseAdr(addr, from))
    {
      if (svs.clients[i].state >= CS_CONNECTED && !svs.clients[i].justConnected)
      {
        if (++n >= sv_maxclientsPerIP->integer)
        {
          //avoid excessive outgoing traffic
          if (!SVC_RateLimit(&bucket, 10, 200))
          {
            NET_OutOfBandPrint(NS_SERVER, from, "print\ntoo many connections\n");
          }

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
    if (!SVC_RateLimit(&bucket, 10, 200))
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nmissing challenge in userinfo\n");
    }

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
      //avoid excessive outgoing traffic
      if (!SVC_RateLimit(&bucket, 10, 200))
      {
        NET_OutOfBandPrint(NS_SERVER, from, "print\nincorrect challenge for your address\n");
      }

      return;
    }
#else
    if (!SV_VerifyChallenge(challenge, from))
    {
      //avoid excessive outgoing traffic
      if (!SVC_RateLimit(&bucket, 10, 200))
      {
        NET_OutOfBandPrint(NS_SERVER, from, "print\nincorrect challenge, please reconnect\n");
      }

      return;
    }
#endif
  }

  Q_strncpyz(userinfo, info, sizeof(userinfo));
  v = Info_ValueForKey(userinfo, "protocol");

  if (*v == '\0')
  {
    if (!SVC_RateLimit(&bucket, 10, 200))
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nmissing protocol in userinfo\n");
    }

    return;
  }

  cl_proto = Q_atoi(v);

  sv_proto = com_protocol->integer;

  if (sv_proto == DEFAULT_PROTOCOL_VERSION)
  {
    //we support new protocol features by default
    sv_proto = NEW_PROTOCOL_VERSION;
  }

  if (cl_proto <= OLD_PROTOCOL_VERSION)
  {
    compat = qtrue;
  }
  else
  {
    if (cl_proto != sv_proto)
    {
      if (!SVC_RateLimit(&bucket, 10, 200))
      {
        NET_OutOfBandPrint(NS_SERVER, from, "print\nServer uses protocol version %i (yours is %i).\n", sv_proto, cl_proto);
      }

      Com_DPrintf("    rejected connection from version %i\n", cl_proto);
      return;
    }

    compat = qfalse;
  }

  v = Info_ValueForKey(userinfo, "qport");

  if (*v == '\0')
  {
    if (!SVC_RateLimit(&bucket, 10, 200))
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nmissing qport in userinfo\n");
    }

    return;
  }

  qport = Q_atoi(Info_ValueForKey(userinfo, "qport"));

  //if "client" is present in userinfo and it is a modern client
  //then assume it can properly decode long strings and protocol extensions
  if (!compat && *Info_ValueForKey(userinfo, "client") != '\0')
  {
    longstr = qtrue;
  }
  else
  {
    longstr = qfalse;

    if (com_protocolCompat)
    {
      //enforce dm69-compatible stream for other clients
      compat = qtrue;
    }
  }

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
    //avoid excessive outgoing traffic
    if (!SVC_RateLimit(&bucket, 10, 200))
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nUserinfo string length exceeded. Try removing setu cvars from your config.\n");
    }

    return;
  }

  //run userinfo filter
  SV_SetTLD(tld, from, Sys_IsLANAddress(from));
  Info_SetValueForKey(userinfo, "tld", tld);
  v = SV_RunFilters(userinfo, from);

  if (*v != '\0')
  {
    NET_OutOfBandPrint(NS_SERVER, from, "print\n%s\n", v);
    Com_DPrintf("Engine rejected a connection: %s.\n", v);
    return;
  }

  //restore burst capacity
  SVC_RateRestoreBurstAddress(from, 10, 1000);

  //quick reject
  newcl = NULL;

  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (NET_CompareAdr(from, &cl->netchan.remoteAddress))
    {
      const qint elapsed = svs.time - cl->lastConnectTime;

      if (elapsed < (sv_reconnectlimit->integer * 1000) && elapsed >= 0)
      {
        const qint remains = ((sv_reconnectlimit->integer * 1000) - elapsed + 999) / 1000;

        if (com_developer->integer)
        {
          Com_Printf("%s: reconnect rejected: too soon\n", NET_AdrToString(from));
        }

        //avoid excessive outgoing traffic
        if (!SVC_RateLimit(&bucket, 10, 200))
        {
          NET_OutOfBandPrint(NS_SERVER, from, "print\nreconnecting, please wait %i second%s\n", remains, remains != 1 ? "s":"");
        }

        return;
      }

      newcl = cl; //we may reuse this slot
      break;
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
    if (NET_CompareAdr(from, &cl->netchan.remoteAddress) && cl->netchan.qport == qport)
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
        VM_Call(sv.gvm, 1, GAME_CLIENT_DISCONNECT, ARRAY_INDEX(svs.clients, newcl));
#endif

        //don't leak memory or file handles due to e.g. downloads in progress
        SV_FreeClient(newcl);
      }

      goto gotnewcl;
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
    startIndex = 0;
  }
  else
  {
    //skip past the reserved slots
    startIndex = sv_privateClients->integer;
  }

  if (newcl && newcl >= svs.clients + startIndex && newcl->state == CS_FREE)
  {
    Com_Printf("%s: reuse slot %i\n", NET_AdrToString(from), ARRAY_INDEX(svs.clients, newcl));
    goto gotnewcl;
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
    if (NET_IsLocalAddress(from))
    {
      count = 0;

      for(i = startIndex;i < sv.maxclients;i++)
      {
        cl = &svs.clients[i];

        if (cl->netchan.remoteAddress.type == NA_BOT)
        {
          count++;
        }
      }

      //if they're all bots
      if (count >= sv.maxclients - startIndex)
      {
        SV_DropClient(&svs.clients[sv.maxclients - 1], "only bots on server");
        newcl = &svs.clients[sv.maxclients - 1];
      }
      else
      {
        Com_Error(ERR_DROP, "server is full on local connect");
        return;
      }
    }
    else
    {
      NET_OutOfBandPrint(NS_SERVER, from, "print\nServer is full.\n");
      Com_DPrintf("Rejected a connection.\n");
      return;
    }
  }

gotnewcl:	
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
  newcl->compat = compat;
  Netchan_Setup(NS_SERVER, &newcl->netchan , from, qport, challenge, compat);

  //init the netchan queue
  newcl->netchan_end_queue = &newcl->netchan_start_queue;

  //save the userinfo
  Q_strncpyz(newcl->userinfo, userinfo, sizeof(newcl->userinfo));

  newcl->longstr = longstr;

  strcpy(newcl->tld, tld);
  newcl->country = SV_FindCountry(newcl->tld);

  SV_UserinfoChanged(newcl, qtrue, qfalse); //update userinfo, do not run filter

  if (sv_clientTLD->integer)
  {
    SV_SaveSequences();
  }

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
  denied = VM_Call(sv.gvm, 3, GAME_CLIENT_CONNECT, clientNum, qtrue, qfalse);	//firstTime = qtrue

  if (denied)
  {
    //we can't just use VM_ArgPtr, because that is only valid inside a VM_Call
    const qchar *str = GVM_ArgPtr(denied);

    NET_OutOfBandPrint(NS_SERVER, from, "print\n%s\n", str);
    Com_DPrintf("Game rejected a connection: %s.\n", str);
    return;
  }
#endif

  if (sv_clientTLD->integer)
  {
    SV_InjectLocation(newcl->tld, newcl->country);
  }

  //send the connect packet to the client
  if (longstr /*&& !compat*/)
  {
    NET_OutOfBandPrint(NS_SERVER, from, "connectResponse %d %d", challenge, sv_proto);
  }
  else
  {
    NET_OutOfBandPrint(NS_SERVER, from, "connectResponse %d", challenge);
  }

  SV_SetClientState(newcl, CS_CONNECTED);

  newcl->lastSnapshotTime = svs.time - 9999; //generate a snapshot immediately
  newcl->lastPacketTime = svs.time;
  newcl->lastConnectTime = svs.time;
  newcl->lastDisconnectTime = svs.time;

  SVC_RateRestoreToxicAddress(&newcl->netchan.remoteAddress, 10, 1000);

  newcl->justConnected = qtrue;
	  
  //when we receive the first packet from the client, we will
  //notice that it is from a different serverid and that the
  //gamestate message was not just sent, forcing a retransmit
  newcl->gamestateMessageNum = newcl->messageAcknowledge - 1; //force gamestate retransmit

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

/*
=====================
SV_FreeClient

Destructor for data allocated in a client structure
=====================
*/
void
SV_FreeClient(client_t *client)
{
  SV_Netchan_FreeQueue(client);
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
void
SV_DropClient(client_t *drop, const qchar *reason)
{
  qchar name[sizeof(drop->name)];
  qint i;
  qbool isBot;

  if (drop->state == CS_ZOMBIE)
  {
    return; //already dropped
  }

  isBot = drop->netchan.remoteAddress.type == NA_BOT;

  Q_strncpyz(name, drop->name, sizeof(name)); //for further dprintf because drop name will be nuked in sv setuserinfo

  //free all allocated data on the client structure
  SV_FreeClient(drop);

  //tell everyone why they got dropped
  if (reason)
  {
    SV_SendServerCommand(NULL, "print \"%s" S_COLOR_WHITE " %s\n\"", name, reason);
  }

  //call the prog function for removing a client
  //this will remove the body, among other things
#if defined(USE_JAVA)
  Java_G_ClientDisconnect(ARRAY_INDEX(svs.clients, drop));
#else
  VM_Call(sv.gvm, 1, GAME_CLIENT_DISCONNECT, ARRAY_INDEX(svs.clients, drop));
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

  //if this was the last client on the server, send a heartbeat
  //to the master so it is known the server is empty
  //send a heartbeat now so the master will get up to date info
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
void
SV_SendClientGameState(client_t *client)
{
  qint start;
  entityState_t nullstate;
  const svEntity_t *svEnt;
  msg_t msg;
  byte msgBuffer[MAX_MSGLEN_BUF];
  qint csUpdated;

  Com_DPrintf("SV_SendClientGameState() for %s\n", client->name);

  SV_SetClientState(client, CS_PRIMED);

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

  //don't delta from messages prior to this gamestate
  client->deltaStart = client->netchan.outgoingSequence;

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
  csUpdated = 0;

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
        Info_SetValueForKey_s(systemInfo, sizeof(systemInfo), "sv_pure", va("%i", sv.pure));
        MSG_WriteBigString(&msg, systemInfo);
      }
      else
      {
        MSG_WriteBigString(&msg, sv.configstrings[start]);
      }
    }

    if (client->csUpdated[start])
    {
      csUpdated++;
    }
  }

  if (client->gamestateAck == GSA_INIT)
  {
    //initial submission, accept any messageAcknowledge with matching serverId
    client->gamestateAck = GSA_SENT_ONCE;
  }
  else
  {
    const qint cmdCap = client->reliableSequence - client->reliableAcknowledge;

    if (csUpdated > 0 && cmdCap + csUpdated >= MAX_RELIABLE_COMMANDS - 1)
    {
      //too much cs updates, could lead to command overflow
      for(start = 0;start < MAX_CONFIGSTRINGS;start++)
      {
        if (client->csUpdated[start])
        {
          client->csUpdated[start] = qfalse;
        }
      }
    }
    else
    {
      //can handle cs updates later without potential overflow
      csUpdated = 0;
    }

    if ((client->gamestateAck == GSA_SENT_ONCE || client->gamestateAck == GSA_ACKED) && csUpdated == 0)
    {
      //if no configstrings being updated since last submission then assume that we're (re)sending identical gamestate
      client->gamestateAck = GSA_SENT_ONCE;
    }
    else
    {
      //expect exact messageAcknowledge
      client->gamestateAck = GSA_SENT_MANY;
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
      NET_OutOfBandPrint(NS_SERVER, &client->netchan.remoteAddress, "print\n" S_COLOR_ERROR "SERVER ERROR: gamestate overflow\n");
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
void
SV_ClientEnterWorld(client_t *client)
{
  sharedEntity_t *ent;
  qbool isBot;
  qint clientNum;

  isBot = client->netchan.remoteAddress.type == NA_BOT;

  SV_SetClientState(client, CS_ACTIVE);
  client->gamestateAck = GSA_ACKED;

  client->oldServerTime = 0;

  //resend all configstrings using the cs commands since these are
  //no longer sent when the client is CS_PRIMED
  if (!isBot)
  {
    SV_UpdateConfigstrings(client);
  }

  //set up the entity for the client
  clientNum = ARRAY_INDEX(svs.clients, client);
  ent = SV_GentityNum(clientNum);
  ent->s.number = clientNum;
  client->gentity = ent;

  client->deltaActive = qfalse; //force delta reset
  client->lastSnapshotTime = svs.time - 9999; //generate a snapshot immediately

  //call the game begin function
#if defined(USE_JAVA)
  Java_G_ClientBegin(clientNum);
#else
  VM_Call(sv.gvm, 1, GAME_CLIENT_BEGIN, clientNum);
#endif
}

/*
============================================================

CLIENT COMMAND EXECUTION

============================================================
*/

/*
==================
SV_CloseDownload

clear/free any download vars
==================
*/
static ID_INLINE void
SV_CloseDownload(client_t *cl)
{
  qint i;

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
}


/*
==================
SV_StopDownload_f

Abort a download if in progress
==================
*/
static ID_INLINE void
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
static ID_INLINE void
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

  //resend the game state to update any clients that entered during the download
  SV_SendClientGameState(cl);

  //apply rate to avoid retransmission after late gamestate acknowledge check
  SVC_RateLimit(&cl->gamestate_rate, 1, 1000);
}


/*
==================
SV_NextDownload_f

The argument will be the last acknowledged block from the client, it should be
the same as cl->downloadClientBlock
==================
*/
static ID_INLINE void
SV_NextDownload_f(client_t *cl)
{
  const qint block = Q_atoi(Cmd_Argv(1));

  if (cl->state == CS_ACTIVE)
  {
    return;
  }

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

    cl->downloadClientBlock++;
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
static ID_INLINE void
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

  //Chey: contrary to primed, this avoids server to client commands from accumulating without being sent which leads to unnecessary commands being sent when downloading finishes, which should avoid potential server command overflows
  SV_SetClientState(cl, CS_CONNECTED);
  cl->gentity = NULL;
  cl->downloading = qtrue;

  if (cl->gamestateAck == GSA_ACKED)
  {
    cl->gamestateAck = GSA_SENT_ONCE;
  }
}


/*
==================
SV_WriteDownloadToClient

Check to see if the client wants a file, open it if needed and start pumping the client
Fill up msg with data, return if a download block was added.
==================
*/
static const qbool
SV_WriteDownloadToClient(client_t *cl)
{
  qint curindex;
  qchar errorMessage[1024];
  msg_t msg;
  byte msgBuffer[MAX_DOWNLOAD_BLKSIZE * 2 + 8];

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
    cl->downloadCount = 0;
    cl->downloadEOF = qfalse;
  }

  //perform any reads that we need to
  while(cl->downloadCurrentBlock - cl->downloadClientBlock < MAX_DOWNLOAD_WINDOW && cl->downloadSize != cl->downloadCount)
  {
    curindex = (cl->downloadCurrentBlock % MAX_DOWNLOAD_WINDOW);

    if (!cl->downloadBlocks[curindex])
    {
      cl->downloadBlocks[curindex] = Z_Malloc(MAX_DOWNLOAD_BLKSIZE);
    }

    cl->downloadBlockSize[curindex] = FS_Read(cl->downloadBlocks[curindex], MAX_DOWNLOAD_BLKSIZE, cl->download);

    if (cl->downloadBlockSize[curindex] <= 0)
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

  MSG_Init(&msg, msgBuffer, sizeof(msgBuffer) - 8);
  MSG_WriteLong(&msg, cl->lastClientCommand);

  MSG_WriteByte(&msg, svc_download);
  MSG_WriteShort(&msg, cl->downloadXmitBlock);

  //block zero is special, contains file size
  if (cl->downloadXmitBlock == 0)
  {
    MSG_WriteLong(&msg, cl->downloadSize);
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

  //move on to the next block
  //it will get sent with next snap shot, the rate will keep us in line
  cl->downloadXmitBlock++;
  cl->downloadSendTime = svs.time;

  return qtrue;
}


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
  qint i;
  qint retval = -1;
  qint nextFragT;
  client_t *cl;

  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (cl->state)
    {
      nextFragT = SV_RateMsec(cl);

      if (!nextFragT)
      {
        nextFragT = SV_Netchan_TransmitNextFragment(cl);
      }

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
  qint i;
  qint numDLs;
  client_t *cl;

  numDLs = 0;

  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (cl->state >= CS_CONNECTED && *cl->downloadName)
    {
      numDLs += SV_WriteDownloadToClient(cl);
    }
  }

  return numDLs;
}


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
static void
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
static void
SV_ResetPureClient_f(client_t *cl)
{
  cl->pureAuthentic = qfalse;
  cl->gotCP = qfalse;
}


/*
=================
SV_UserinfoChanged

Pull specific info from a newly changed userinfo string
into a more C friendly form.
=================
*/
void
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
    cl->rate = 0;
    return;
  }

  //rate command

  //if the client is on the same subnet as the server and we aren't running an
  //internet public server, assume they don't need a rate choke
  if (cl->netchan.remoteAddress.type == NA_LOOPBACK || (cl->netchan.isLANAddress && com_dedicated->integer != 2 && sv_lanForceRate->integer))
  {
    cl->rate = 0; //lans should not rate limit
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

  //range check
  if (i < 1)
  {
    i = 1;
  }
  else if (i > sv_fps->integer)
  {
    i = sv_fps->integer;
  }

  i = 1000 / i; //from FPS to milliseconds

  if (i != cl->snapshotMsec)
  {
    //reset last sent snapshot to avoid desync between server and snapshot timings
    cl->lastSnapshotTime = svs.time - 9999; //generate a snapshot immediately
    cl->snapshotMsec = i;
  }

  if (!updateUserinfo)
  {
    return;
  }

  //name for C code
  val = Info_ValueForKey(cl->userinfo, "name");

  //truncate if it is too long as it may cause memory corruption in OSP mod
  if (sv.gvm->forceDataMask && strlen(val) >= sizeof(buf))
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
    return;
  }

  Info_SetValueForKey(cl->userinfo, "tld", cl->tld);

  if (runFilter)
  {
    val = SV_RunFilters(cl->userinfo, &cl->netchan.remoteAddress);

    if (*val != '\0')
    {
      SV_DropClient(cl, val);
    }
  }
}


/*
==================
SV_UpdateUserinfo_f
==================
*/
void
SV_UpdateUserinfo_f(client_t *cl)
{
  const qchar *info;

  info = Cmd_Argv(1);

  if (Cmd_Argc() != 2 || *info == '\0')
  {
    //this is something erroneous, client should never send that
    return;
  }

  Q_strncpyz(cl->userinfo, info, sizeof(cl->userinfo));

  SV_UserinfoChanged(cl, qtrue, qtrue); //update userinfo, run filter

  //call prog code to allow overrides
  VM_Call(sv.gvm, 1, GAME_CLIENT_USERINFO_CHANGED, ARRAY_INDEX(svs.clients, cl));
}

extern qint
SV_Strlen(const qchar *str);

/*
==================
SV_PrintLocations_f
==================
*/
void
SV_PrintLocations_f(client_t *client)
{
  qint i;
  qint len;
  client_t *cl;
  qint max_namelength;
  qint max_ctrylength;
  qchar line[128];
  qchar buf[1400 - 4 - 8];
  qchar *s;
  qchar filln[MAX_NAME_LENGTH];
  qchar fillc[64];

  if (!svs.clients)
  {
    return;
  }

  max_namelength = 4; //strlen("name")
  max_ctrylength = 7; //strlen("country")

  //first pass: save and determine max.lengths of name/address fields
  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (cl->state == CS_FREE)
    {
      continue;
    }

    if (com_ansiColor && com_ansiColor->integer)
    {
      len = SV_Strlen(cl->name); //name length without color sequences
    }
    else
    {
      len = (qint)strlen(cl->name); //name length with color sequences
    }

    if (len > max_namelength)
    {
      max_namelength = len;
    }

    len = strlen(cl->country);

    if (len > max_ctrylength)
    {
      max_ctrylength = len;
    }
  }

  s = buf;
  *s = '\0';

  Com_Memset(filln, '-',  max_namelength);
  filln[max_namelength] = '\0';

  Com_Memset(fillc, '-',  max_ctrylength);
  fillc[max_ctrylength] = '\0';

  //start this on a new line to be viewed properly in console
  s = Q_stradd(s, "\n");
  Com_sprintf(line, sizeof(line), "ID %-*s CC Country\n", max_namelength, "Name");
  s = Q_stradd(s, line);
  Com_sprintf(line, sizeof(line), "-- %s -- %s\n", filln, fillc);
  s = Q_stradd(s, line);

  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (cl->state == CS_FREE)
    {
      continue;
    }

    if (com_ansiColor && com_ansiColor->integer)
    {
      len = Com_sprintf(line, sizeof(line), "%2i %s%-*s" S_COLOR_WHITE " %2s %s\n", i, cl->name, max_namelength - SV_Strlen(cl->name), "", cl->tld, cl->country);
    }
    else
    {
      len = Com_sprintf(line, sizeof(line), "%2i %s%-*s %2s %s\n", i, cl->name, max_namelength - (qint)strlen(cl->name), "", cl->tld, cl->country);
    }

    if (s - buf + len >= sizeof(buf) - 1) //flush accumulated buffer
    {
      if (client)
      {
        NET_OutOfBandPrint(NS_SERVER, &client->netchan.remoteAddress, "print\n%s", buf);
      }
      else
      {
        Com_Printf("%s", buf);
      }

      s = buf;
      *s = '\0';
    }

    s = Q_stradd(s, line);
  }

  if (buf[0])
  {
    if (client)
    {
      NET_OutOfBandPrint(NS_SERVER, &client->netchan.remoteAddress, "print\n%s", buf);
    }
    else
    {
      Com_Printf("%s", buf);
    }
  }
}


typedef struct
{
  const qchar *name;
  void (*func)(client_t *cl);
}
ucmd_t;

static const ucmd_t ucmds[] =
{
  {"userinfo", SV_UpdateUserinfo_f},
  {"disconnect", SV_Disconnect_f},
  {"cp", SV_VerifyPaks_f},
  {"vdr", SV_ResetPureClient_f},
  {"download", SV_BeginDownload_f},
  {"nextdl", SV_NextDownload_f},
  {"stopdl", SV_StopDownload_f},
  {"donedl", SV_DoneDownload_f},
  {"locations", SV_PrintLocations_f},
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
  if (sv_floodProtect->integer)
  {
    return SVC_RateLimit(&cl->cmd_rate, sv_floodLimit->integer, sv_floodWait->integer);
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

 //applying flood protection only to "CS_ACTIVE" clients leaves too much room for abuse, extending this flood protection to clients pre CS_ACTIVE should not cause any issues, as the download-commands are handled within the engine and floodprotect only filters calls to the qvm
  isBot = cl->netchan.remoteAddress.type == NA_BOT ? qtrue:qfalse;
  bFloodProtect = !isBot && cl->state >= CS_ACTIVE;

  //see if it is a server level command
  for(ucmd = ucmds;ucmd->name;ucmd++)
  {
    if (!strcmp(Cmd_Argv(0), ucmd->name))
    {
      if (ucmd->func == SV_UpdateUserinfo_f)
      {
        if (bFloodProtect)
        {
          if (sv_userInfoFloodProtect->integer)
          {
            if (!SVC_RateLimit(&cl->info_rate, 5, 1000))
            {
              return qfalse; //lag flooder
            }
          }
        }
      }
      else if (ucmd->func == SV_PrintLocations_f && !sv_clientTLD->integer)
      {
        continue; //bypass this command to the gamecode
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
      if (sv.gvm->forceDataMask)
      {
        Cmd_Args_Sanitize("\n\r;"); //handle ';' for OSP
      }
      else if (sv_filterCommands->integer)
      {
        if (sv_filterCommands->integer >= 2)
        {
          Cmd_Args_Sanitize("\n\r;");
        }
        else
        {
          Cmd_Args_Sanitize("\n\r");
        }
      }

#if defined(USE_JAVA)
      Java_G_ClientCommand(ARRAY_INDEX(svs.clients, cl));
#else
      VM_Call(sv.gvm, 1, GAME_CLIENT_COMMAND, ARRAY_INDEX(svs.clients, cl));
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
void
SV_ClientThink(client_t *cl, const usercmd_t *cmd)
{
  cl->lastUsercmd = *cmd;

  if (cl->state != CS_ACTIVE)
  {
    return; //may have been kicked during the last usercmd
  }

#if defined(USE_JAVA)
  Java_G_ClientThink(ARRAY_INDEX(svs.clients, cl));
#else
  VM_Call(sv.gvm, 1, GAME_CLIENT_THINK, ARRAY_INDEX(svs.clients, cl));
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
static void
SV_UserMove(client_t *cl, msg_t *msg, qbool delta)
{
  qint i;
  qint key;
  qint cmdCount;
  static const usercmd_t nullcmd = {0};
  usercmd_t cmds[MAX_PACKET_USERCMDS];
  usercmd_t *cmd;
  const usercmd_t *oldcmd;

  cl->deltaActive = delta;

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
  if (cl->frames[cl->messageAcknowledge & PACKET_MASK].messageAcked == 0)
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
      if (!SVC_RateLimit(&cl->gamestate_rate, 1, 1000))
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
    //cl->deltaActive = qfalse; //force delta reset
    return;
  }

  //usually, the first couple commands will be duplicates
  //of ones we have previously received, but the servertimes
  //in the commands will cause them to be immediately discarded
  for(i = 0;i < cmdCount;i++)
  {
    //if this is a cmd from before a map_restart ignore it
    if (cmds[i].serverTime - cmds[cmdCount - 1].serverTime > 0)
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

    SV_ClientThink(cl, &cmds[i]);
  }
}


/*
===========================================================================

USER CMD EXECUTION

===========================================================================
*/


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
    if (messageDelta == 0 || (messageDelta > 0 && cl->gamestateAck == GSA_SENT_ONCE))
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


/*
===================
SV_ExecuteClientMessage

Parse a client packet
===================
*/
void
SV_ExecuteClientMessage(client_t *cl, msg_t *msg)
{
  qint c;
  qint serverId;
  qint reliableAcknowledge;

  MSG_Bitstream(msg);
  serverId = MSG_ReadLong(msg);
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

  reliableAcknowledge = MSG_ReadLong(msg);

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
      if (cl->netchan.remoteAddress.type == NA_LOOPBACK || !SVC_RateLimit(&cl->gamestate_rate, 1, 1000))
      {
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

  //read optional clientCommand strings
  do
  {
    c = MSG_ReadByte(msg);

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

  if (cl->gamestateAck != GSA_ACKED)
  {
    //late check for gamestate resend
    if (cl->state == CS_PRIMED)
    {
      if (!SV_AcknowledgeGamestate(cl, serverId))
      {
        Com_DPrintf("%s: dropped gamestate, resending\n", cl->name);

        if (!SVC_RateLimit(&cl->gamestate_rate, 1, 1000))
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

  //if (msg->readcount != msg->cursize && sv_collectClientJunkInfo->integer)
  //{
    //Com_Printf("WARNING: junk at end of packet for client %i\n", ARRAY_INDEX(svs.clients, cl));
  //}
}
