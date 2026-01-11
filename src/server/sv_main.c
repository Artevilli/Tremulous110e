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
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
===========================================================================
*/

#include "server.h"

serverStatic_t svs; //persistant server info
server_t sv = {}; //local server

//Chey
#if defined(INCLUDE_REMOTE_COMMANDS)
serverRconPassword_t rconWhitelist[MAXIMUM_RCON_WHITELIST];
qint rconWhitelistCount = 0;
#endif
#define MAX_QUEUE 10
netadr_t queue[MAX_QUEUE];
unsigned lastQueue[MAX_QUEUE];
unsigned queueCount;

/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/*
===============
SV_ExpandNewlines

Converts newlines to "\n" so a line prints nicer
===============
*/
static const qchar *
SV_ExpandNewlines(const qchar *in)
{
  static qchar string[1024];
  size_t l;

  l = 0;

  while(*in && l < sizeof(string) - 3)
  {
    if (*in == '\n')
    {
      string[l] = '\\';
      l++;
      string[l] = 'n';
      l++;
    }
    else
    {
      string[l] = *in;
      l++;
    }

    in++;
  }

  string[l] = '\0';

  return string;
}

/*
======================
SV_ReplacePendingServerCommands

FIXME: This is ugly
======================
*/
#if 0 //unused
static const qbool
SV_ReplacePendingServerCommands(client_t *client, const qchar *cmd)
{
  qint i;
  qint index;
  qint csnum1;
  qint csnum2;

  for(i = client->reliableSent + 1;i <= client->reliableSequence;i++)
  {
    index = i & (MAX_RELIABLE_COMMANDS - 1);
    //
    if (!Q_strncmp(cmd, client->reliableCommands[index], strlen("cs")))
    {
      if (Q_sscanf(cmd, "cs %i", &csnum1) != 1)
      {
        return qfalse;
      }

      if (Q_sscanf(client->reliableCommands[index], "cs %i", &csnum2) != 1)
      {
        return qfalse;
      }

      if (csnum1 == csnum2)
      {
        Q_strncpyz(client->reliableCommands[index], cmd, sizeof(client->reliableCommands[index]));

        if ((client->netchan.remoteAddress.type != NA_BOT) || !(client->gentity && client->gentity->r.svFlags & SVF_BOT))
        {
          Com_Printf(S_COLOR_YELLOW "WARNING: client %i removed double pending config string %i: %s\n", ARRAY_INDEX(svs.clients, client), csnum1, cmd);
        }

        return qtrue;
      }
    }
  }
  return qfalse;
}
#endif

/*
======================
SV_AddServerCommand

The given command will be transmitted to the client, and is guaranteed to
not have future snapshot_t executed before it is executed
======================
*/
const void
SV_AddServerCommand(client_t *client, const qchar *cmd)
{
  qint index;
  qint i;
  qint n;

  //this is very ugly but it's also a waste to for instance send multiple config string updates
  //for the same config string index in one snapshot
  //if (SV_ReplacePendingServerCommands(client, cmd))
  //{
    //return;
  //}

  if (!client)
  {
    return;
  }

#if defined(SKIP_PRE_ACTIVE_COMMANDS)
  if (client->state < CS_ACTIVE)
  {
    return;
  }
#else
  //do not send commands until the gamestate has been sent
  if (client->state < CS_PRIMED)
  {
    return;
  }
#endif

#if defined(UDP_DOWNLOAD_NO_DOUBLE_LOAD)
  if (client->downloading)
  {
    return;
  }
#endif

  client->reliableSequence++;
  /*
  if we would be losing an old command that hasn't been acknowledged,
  we must drop the connection
  we check == instead of >= so a broadcast print added by SV_DropClient()
  doesn't cause a recursive drop client
  */
  if (client->reliableSequence - client->reliableAcknowledge == MAX_RELIABLE_COMMANDS + 1)
  {
    if (client->gamestateMessageNum == -1)
    {
      //invalid gamestate message can occur in sv_dropclient() to avoid calling it multiple times
      return;
    }

    Com_Printf("===== pending server commands =====\n");

    n = client->reliableSequence - client->reliableAcknowledge;

    for(i = 0;i < n;i++)
    {
      const qint idx = client->reliableAcknowledge + 1 + i;

      Com_Printf("cmd %5d: %s\n", i, client->reliableCommands[idx & (MAX_RELIABLE_COMMANDS - 1)]);
    }

    Com_Printf("cmd %5d: %s\n", i, cmd);
    SV_DropClient(client, "Server command overflow");
    return;
  }

  index = client->reliableSequence & (MAX_RELIABLE_COMMANDS - 1);
  Q_strncpyz(client->reliableCommands[index], cmd, sizeof(client->reliableCommands[index]));
}

/*
=================
SV_SendServerCommand

Sends a reliable command string to be interpreted by 
the client game module: "cp", "print", "chat", etc
A NULL client will broadcast to all clients
=================
*/
const void QDECL
SV_SendServerCommand(client_t *cl, const qchar *fmt, ...)
{
  va_list argptr;
  qchar message[MAX_STRING_CHARS + 128]; //slightly larger than allowed, to detect overflows
  client_t *client;
  unsigned j;
  unsigned len;

  va_start(argptr,fmt);
  len = Q_vsnprintf(message, sizeof(message), fmt, argptr);
  va_end(argptr);

  //fix to http://aluigi.altervista.org/adv/q3msgboom-adv.txt
  //the actual cause of the bug is probably further downstream and should maybe be addressed later, but this certainly fixes the problem for now
  if (len > 1022)
  {
    SV_WriteAttackLog(va("Warning: q3infoboom/q3msgboom exploit attack from client slot %i.\n", ARRAY_INDEX(svs.clients, cl)));
    return;
  }

  if (cl != NULL)
  {
    if (len <= 1022)
    {
      SV_AddServerCommand(cl, message);
    }

    return;
  }

  //hack to echo broadcast prints to console
  if (com_dedicated->integer && !strncmp(message, "print", 5))
  {
    Com_Printf("broadcast: %s\n", SV_ExpandNewlines(message));
  }

  //save broadcasts to demo
  if (sv.demoState == DS_RECORDING)
  {
    SV_DemoWriteServerCommand(message);
  }

  //send the data to all relevent clients
  for(j = 0;j < sv.maxclients;j++)
  {
    client = &svs.clients[j];

    if (client->gentity && (client->gentity->r.svFlags & SVF_BOT))
    {
      continue; //bots dont need server commands
    }

    //if(client->state == CS_ACTIVE)
    if (len <= 1022)
    {
      SV_AddServerCommand(client, message);
    }
  }
}

/*
==============================================================================

MASTER SERVER FUNCTIONS

==============================================================================
*/

#define NEW_RESOLVE_DURATION 86400000 //24 hours
static unsigned g_lastResolveTime[MAX_MASTER_SERVERS];

/*
================
SV_MasterNeedsResolving

Refresh every so often regardless of if the actual address was modified
================
*/
static const ID_INLINE qbool
SV_MasterNeedsResolving(const unsigned server, const unsigned time)
{
  if (g_lastResolveTime[server] > time)
  {
    return qtrue; //time flowed backwards?
  }

  if ((time - g_lastResolveTime[server]) > NEW_RESOLVE_DURATION)
  {
    return qtrue; //it is time again
  }

  return qfalse;
}

/*
================
SV_MasterHeartbeat

Send a message to the masters every few minutes to
let it know we are alive, and log information.
We will also have a heartbeat sent when a server
changes from empty to non-empty, and full to non-full,
but not on every player enter or exit.
================
*/
#define HEARTBEAT_MSEC  (300 * 1000)
#define HEARTBEAT_GAME  "XreaL-1"
#define HEARTBEAT_DEAD  "XreaL-FlatLine-1"
static const void
SV_MasterHeartbeat(const qchar *message)
{
  static netadr_t adr[MAX_MASTER_SERVERS][2]; //[2] for v4 and v6 addresses for the same address string.
  unsigned i;

  if (sv_hidden->integer)
  {
    return;
  }

  const qint netenabled = Cvar_VariableIntegerValue("net_enabled");

  //dedicated 1 is for lan play, dedicated 2 is for inet public play
  if (!com_dedicated || com_dedicated->integer != 2 || !(netenabled & (NET_ENABLEV4 | NET_ENABLEV6)))
  {
    return; //only dedicated servers send heartbeats
  }

  //if not time yet, don't send anything
  if (svs.time < svs.nextHeartbeatTime)
  {
    return;
  }

  svs.nextHeartbeatTime = svs.time + HEARTBEAT_MSEC;

  //this needs to be used instead of svs.time because svs.time resets every time the map changes (actually
  //every time the game restarts) and resolving every map change is not really needed
  const unsigned time = Com_Milliseconds();

  //send to group masters
  for(i = 0;i < MAX_MASTER_SERVERS;i++)
  {
    if (!sv_master[i] || !sv_master[i]->string[0])
    {
      continue;
    }

    //see if we haven't already resolved the name, resolving usually causes hitches on win95 so only do it when needed
    if (sv_master[i]->modified || (adr[i][0].type == NA_BAD && adr[i][1].type == NA_BAD) || SV_MasterNeedsResolving(i, time))
    {
      sv_master[i]->modified = qfalse;
      g_lastResolveTime[i] = time;

      if (netenabled & NET_ENABLEV4)
      {
        Com_Printf("Resolving %s (IPv4)\n", sv_master[i]->string);
        const qint res = NET_StringToAdr(sv_master[i]->string, &adr[i][0], NA_IP);

        if (res == 2)
        {
          //if no port was specified, use the default master port
          adr[i][0].port = BigShort(PORT_MASTER);
        }

        if (res)
        {
          Com_Printf("%s resolved to %s\n", sv_master[i]->string, NET_AdrToStringwPort(&adr[i][0]));
        }
        else
        {
          Com_Printf("%s has no IPv4 address.\n", sv_master[i]->string);
        }
      }
#if defined(USE_IPV6)
      if (netenabled & NET_ENABLEV6)
      {
        Com_Printf("Resolving %s (IPv6)\n", sv_master[i]->string);
        const qint res = NET_StringToAdr(sv_master[i]->string, &adr[i][1], NA_IP6);

        if (res == 2)
        {
          //if no port was specified, use the default master port
          adr[i][1].port = BigShort(PORT_MASTER);
        }

        if (res)
        {
          Com_Printf("%s resolved to %s\n", sv_master[i]->string, NET_AdrToStringwPort(&adr[i][1]));
        }
        else
        {
          Com_Printf("%s has no IPv6 address.\n", sv_master[i]->string);
        }
      }
#endif
      if (adr[i][0].type == NA_BAD && adr[i][1].type == NA_BAD)
      {
        //if the address failed to resolve, clear it to avoid taking repeated dns hits
        Com_Printf("Couldn't resolve address: %s\n", sv_master[i]->string);
        Cvar_Set(sv_master[i]->name, "");
        sv_master[i]->modified = qfalse;
        continue;
      }
    }

    Com_Printf("Sending heartbeat to %s\n", sv_master[i]->string);

    //this command should be changed if the server info / status format ever incompatably changes
    if (adr[i][0].type != NA_BAD)
    {
      NET_OutOfBandPrint(NS_SERVER, &adr[i][0], "heartbeat %s\n", message);
    }

    if (adr[i][1].type != NA_BAD)
    {
      NET_OutOfBandPrint(NS_SERVER, &adr[i][1], "heartbeat %s\n", message);
    }
  }
}

/*
=================
SV_MasterShutdown

Informs all masters that this server is going down
=================
*/
const void
SV_MasterShutdown(void)
{
  //send a hearbeat right now
  svs.nextHeartbeatTime = svs.time;
  SV_MasterHeartbeat(HEARTBEAT_DEAD);

  //send it again to minimize chance of drops
  svs.nextHeartbeatTime = svs.time;
  SV_MasterHeartbeat(HEARTBEAT_DEAD);

  //when the master tries to poll the server, it won't respond, so
  //it will be removed from the list
}

/*
=================
SV_MasterGameStat
=================
*/
const void
SV_MasterGameStat(const qchar *data)
{
  static netadr_t adr;

  if (!com_dedicated || com_dedicated->integer != 2)
  {
    return; //only dedicated servers send stats
  }

  //it would be kinda weird to receive gamestat from a "nonexistent" server...
  if (sv_hidden->integer)
  {
    return;
  }

  Com_Printf("Resolving %s\n", MASTER_SERVER_NAME);

  switch(NET_StringToAdr(MASTER_SERVER_NAME, &adr, NA_UNSPEC))
  {
    case
    0:
      Com_Printf("Couldn't resolve address: %s\n", MASTER_SERVER_NAME);
      return;

    case
    2:
      adr.port = BigShort(PORT_MASTER);

    default:
      break;
  }

  Com_Printf("%s resolved to %s\n", MASTER_SERVER_NAME, NET_AdrToStringwPort(&adr));

  Com_Printf("Sending gamestat to %s\n", MASTER_SERVER_NAME);
  NET_OutOfBandPrint(NS_SERVER, &adr, "gamestat %s", data);
}

// This is deliberately quite large to make it more of an effort to DoS
//Chey: XXX: these new values may work better, just in case they dont, max buckets was 16384 and max hashes was 1024
#define MAX_BUCKETS (2 << 14)
#define MAX_HASHES (2 * MAX_BUCKETS)

static leakyBucket_t buckets[MAX_BUCKETS];
static leakyBucket_t *bucketHashes[MAX_HASHES];
//static rateLimit_t outboundRateLimit;

/*
================
SVC_HashForAddress
================
*/
static const ID_INLINE unsigned
SVC_HashForAddress(const netadr_t *address)
{
  const byte *ip = NULL;
  unsigned size = 0;
  unsigned hash = 0;
  unsigned i;

  switch(address->type)
  {
    case
    NA_IP:
      ip = address->ipv._4;
      size = 4;
      break;
#if defined(USE_IPV6)
    case
    NA_IP6:
      ip = address->ipv._6;
      size = 16;
      break;
#endif
    default:
      break;
  }

  //Chey: prevent the possibility of a NULL pointer leaking out
  if (!ip)
  {
    Com_Printf("SVC_HashForAddress: invalid ip - hash value is zero\n");
    return 0;
  }

  for(i = 0;i < size;i++)
  {
    hash += (qint)(ip[i]) * (i + 119);
  }

  hash = (hash ^ (hash >> 10) ^ (hash >> 20));
  hash &= (MAX_HASHES - 1);
  return hash;
}

/*
================
SVC_RelinkToHead
================
*/
static const ID_INLINE void
SVC_RelinkToHead(leakyBucket_t *bucket, const unsigned hash)
{
  if (bucket->prev != NULL)
  {
    bucket->prev->next = bucket->next;
  }
  else
  {
    return;
  }

  if (bucket->next != NULL)
  {
    bucket->next->prev = bucket->prev;
  }

  bucket->next = bucketHashes[hash];

  if (bucketHashes[hash] != NULL)
  {
    bucketHashes[hash]->prev = bucket;
  }

  bucket->prev = NULL;
  bucketHashes[hash] = bucket;
}

/*
================
SVC_BucketForAddress

Find or allocate a bucket for an address
================
*/
static leakyBucket_t *
SVC_BucketForAddress(const netadr_t *address, const unsigned burst, const unsigned period, const unsigned now)
{
  //const unsigned now = Sys_Milliseconds() ? Sys_Milliseconds():1; //Chey: do not use a time of zero for leaky buckets as it may cause... "leaky bucket leaking"?
  //const unsigned now = Sys_Milliseconds(); //Chey: NULL time prevention is not relevant anymore
  static leakyBucket_t dummy = {0};
  static unsigned start = 0;
  const unsigned hash = SVC_HashForAddress(address);
  leakyBucket_t *bucket;
  unsigned i;
  unsigned n;

  n = 0;

  for(bucket = bucketHashes[hash];bucket;bucket = bucket->next)
  {
    n++;

    switch(bucket->type)
    {
      case
      NA_IP:
        if (!(memcmp(bucket->ipv._4, address->ipv._4, 4)))
        {
          if (n > 8)
          {
            SVC_RelinkToHead(bucket, hash);
          }

          return bucket;
        }

        break;
#if defined(USE_IPV6)
      case
      NA_IP6:
        if (!(memcmp(bucket->ipv._6, address->ipv._6, 16)))
        {
          if (n > 8)
          {
            SVC_RelinkToHead(bucket, hash);
          }

          return bucket;
        }

        break;
#endif
      default:
        return &dummy;
    }
  }

  for(i = 0;i < MAX_BUCKETS;i++)
  {
    if (start >= MAX_BUCKETS)
    {
      start = 0;
    }

    bucket = &buckets[start];
    start++;

    const unsigned interval = now - bucket->rate.lastTime;

    //reclaim expired buckets
    if (bucket->type != NA_BAD && interval > (bucket->rate.burst * period))
    {
      if (bucket->prev != NULL)
      {
        bucket->prev->next = bucket->next;
      }
      else
      {
        bucketHashes[bucket->hash] = bucket->next;
      }

      if (bucket->next != NULL)
      {
        bucket->next->prev = bucket->prev;
      }

      bucket->type = NA_BAD;
    }

    if (bucket->type == NA_BAD)
    {
      bucket->type = address->type;

      switch(address->type)
      {
        case
        NA_IP:
          Com_Memcpy(bucket->ipv._4, address->ipv._4, 4);
          break;
#if defined(USE_IPV6)
        case
        NA_IP6:
          Com_Memcpy(bucket->ipv._6, address->ipv._6, 16);
          break;
#endif
        default:
          break;
      }

      bucket->rate.lastTime = now;
      bucket->rate.burst = 0;
      bucket->hash = hash;
      bucket->toxic = 0;

      //add to the head of the relevant hash chain
      bucket->next = bucketHashes[hash];

      if (bucketHashes[hash] != NULL)
      {
        bucketHashes[hash]->prev = bucket;
      }

      bucket->prev = NULL;
      bucketHashes[hash] = bucket;
      return bucket;
    }
  }

  //could not allocate a bucket for this address so write to attack log since it is relevant info
  SV_WriteAttackLogD(va("%s: Could not allocate a bucket for client from %s\n", __func__, NET_AdrToString(address)));
  return NULL;
}

/*
================
SVC_RateLimit

dont call if sv_protect 1 xreal isnt set
================
*/
const qbool
SVC_RateLimit(rateLimit_t *bucket, const unsigned burst, const unsigned period, const unsigned now)
{
  const unsigned interval = now - bucket->lastTime;
  const unsigned expired = interval / period;
  const unsigned expiredRemainder = interval % period;

  if (expired > bucket->burst || interval < 0)
  {
    bucket->burst = 0;
    bucket->lastTime = now;
  }
  else
  {
    bucket->burst -= expired;
    bucket->lastTime = now - expiredRemainder;
  }

  if (bucket->burst < burst)
  {
    bucket->burst++;
    return qfalse;
  }

  SV_WriteAttackLogD(va("%s: burst limit exceeded for bucket: %i limit: %i\n", __func__, bucket->burst, burst));
  return qtrue;
}

/*
================
SVC_RateDrop
================
*/
static const ID_INLINE void
SVC_RateDrop(leakyBucket_t *bucket, const unsigned burst, const unsigned now)
{
  if (bucket != NULL)
  {
    if (bucket->toxic < 10000)
    {
      ++bucket->toxic;
    }

    bucket->rate.burst = burst * bucket->toxic;
    bucket->rate.lastTime = now;
  }
}

/*
================
SVC_RateRestoreBurst
================
*/
static const ID_INLINE void
SVC_RateRestoreBurst(leakyBucket_t *bucket)
{
  if (bucket != NULL)
  {
    if (bucket->rate.burst > 0)
    {
      bucket->rate.burst--;
    }
  }
}

/*
================
SVC_RateRestoreToxic
================
*/
static const ID_INLINE void
SVC_RateRestoreToxic(leakyBucket_t *bucket)
{
  if (bucket != NULL)
  {
    if (bucket->toxic > 0)
    {
      bucket->toxic--;
    }
  }
}

/*
================
SVC_RateLimitAddress

Rate limit for a particular address
================
*/
static const ID_INLINE qbool
SVC_RateLimitAddress(const netadr_t *from, const unsigned burst, const unsigned period, const unsigned now)
{
  leakyBucket_t *bucket = SVC_BucketForAddress(from, burst, period, now);

  return bucket ? SVC_RateLimit(&bucket->rate, burst, period, now):qtrue;
}

/*
================
SVC_RateRestoreAddress

Decrease burst rate
================
*/
const void
SVC_RateRestoreBurstAddress(const netadr_t *from, const unsigned burst, const unsigned period, const unsigned now)
{
  leakyBucket_t *bucket = SVC_BucketForAddress(from, burst, period, now);

  SVC_RateRestoreBurst(bucket);
}

/*
================
SVC_RateRestoreToxicAddress

Decrease toxicity
================
*/
const void
SVC_RateRestoreToxicAddress(const netadr_t *from, const unsigned burst, const unsigned period, const unsigned now)
{
  leakyBucket_t *bucket = SVC_BucketForAddress(from, burst, period, now);

  SVC_RateRestoreToxic(bucket);
}

/*
================
SVC_RateDropAddress
================
*/
static const ID_INLINE void
SVC_RateDropAddress(const netadr_t *from, const unsigned burst, const unsigned period, const unsigned now)
{
  leakyBucket_t *bucket = SVC_BucketForAddress(from, burst, period, now);

  SVC_RateDrop(bucket, burst, now);
}

/*
==============================================================================

CONNECTIONLESS COMMANDS

==============================================================================
*/

#if defined(SUPPORT_STATUS_SCORES_OVERRIDE)
static struct
{
  qbool active[MAX_CLIENTS];
  qint scores[MAX_CLIENTS];
}
statusScoresOverride_state;

/*
================
SV_HandleGameInfoMessage
================
*/
void
SV_HandleGameInfoMessage(const qchar *info)
{
  const qchar *token = Info_ValueForKey(info, "type");

  if (!Q_stricmp(token, "clientstate"))
  {
    token = Info_ValueForKey(info, "client");

    if (*token)
    {
      const qint clientNum = Q_atoi(token);

      if (clientNum >= 0 && clientNum < MAX_CLIENTS)
      {
        token = Info_ValueForKey(info, "score");

        if (*token)
        {
          statusScoresOverride_state.active[clientNum] = qtrue;
          statusScoresOverride_state.scores[clientNum] = Q_atoi(token);
        }
        else
        {
          statusScoresOverride_state.active[clientNum] = qfalse;
        }
      }
    }
  }
}

/*
================
SV_StatusScoresOverride_Reset
================
*/
void
SV_StatusScoresOverride_Reset(void)
{
  Com_Memset(&statusScoresOverride_state, 0, sizeof(statusScoresOverride_state));
}

/*
================
SV_StatusScoresOverride_AdjustScore
================
*/
qint
SV_StatusScoresOverride_AdjustScore(qint defaultScore, qint clientNum)
{
  if (clientNum >= 0 && clientNum < MAX_CLIENTS && statusScoresOverride_state.active[clientNum])
  {
    return statusScoresOverride_state.scores[clientNum];
  }

  return defaultScore;
}
#endif

#if defined(GAMESTATE_OVERFLOW_FIX)
/*
================
SV_CalculateMaxBaselines

Sets client->maxEntityBaseline to the highest entity baseline index that can be written
safely without potential gamestate overflow.
================
*/
void
SV_CalculateMaxBaselines(client_t *client, msg_t msg)
{
  qint start;
  const svEntity_t *svEnt;
  entityState_t nullstate;
  byte msgBuffer[MAX_MSGLEN_BUF];

  client->maxEntityBaseline = -1;
  Com_Memset(&nullstate, 0, sizeof(nullstate));
  msg.data = msgBuffer;

  for(start = 0;start < MAX_GENTITIES;start++)
  {
    if (!sv.baselineUsed[start])
    {
      continue;
    }

    svEnt = &sv.svEntities[start];
    MSG_WriteByte(&msg, svc_baseline);
    MSG_WriteDeltaEntity(&msg, &nullstate, &svEnt->baseline, qtrue);

    if (msg.cursize + 32 >= msg.maxsize)
    {
      break;
    }

    client->maxEntityBaseline = start;
  }
}
#endif

/*
================
SVC_VerifyChallenge
================
*/
static const ID_INLINE qbool
SVC_VerifyChallenge(const qchar *challenge)
{
  if (challenge)
  {
    unsigned i;
    const unsigned j = strlen(challenge);

    if (j > 64)
    {
      return qfalse;
    }

    for(i = 0;i < j;i++)
    {
      if (challenge[i] == '\\' || challenge[i] == '/' || challenge[i] == '%' || challenge[i] == ';' || challenge[i] == '"' || challenge[i] < 32 /*//non-ascii*/ || challenge[i] > 128) //non-ascii
      {
        return qfalse;
      }
    }

    return qtrue;
  }

  return qfalse;
}

/*
================
SVC_Status

Responds with all the info that qplug or qspy can see about the server
and all connected players.  Used for getting detailed information after
the simple info query.
================
*/
static const void
SVC_Status(const netadr_t *from)
{
  qchar player[MAX_NAME_LENGTH + 32]; //score + ping + name
  qchar status[MAX_PACKETLEN];
  qchar *s;
  unsigned i;
  client_t *cl;
  playerState_t *ps;
  unsigned statusLength;
  unsigned playerLength;
  qchar infostring[MAX_INFO_STRING + 160]; //add some space for challenge string

  //Chey: bugtraq 12534
  if (!SVC_VerifyChallenge(Cmd_Argv(1)))
  {
    SV_WriteAttackLog(va("%s: Invalid challenge from %s, dropping request.\n", __func__, NET_AdrToString(from)));
    return;
  }

  //max challenge of 128
  if (strlen(Cmd_Argv(1)) > 128)
  {
    SV_WriteAttackLog(va("%s: Challenge length exceeded from %s, dropping request.\n", __func__, NET_AdrToString(from)));
    return;
  }

  Q_strncpyz(infostring, Cvar_InfoString(CVAR_SERVERINFO, NULL), sizeof(infostring));

  // echo back the parameter to status. so master servers can use it as a challenge
  // to prevent timed spoofed reply packets that add ghost servers
  Info_SetValueForKey(infostring, "challenge", Cmd_Argv(1));

  s = status;
  status[0] = '\0';
  statusLength = strlen(infostring) + 16; //strlen("statusResponse\n\n")

  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (cl->state >= CS_CONNECTED)
    {
      ps = SV_GameClientNum(i);
      playerLength = Com_sprintf(player, sizeof(player), "%i %i \"%s\"\n", 
#if defined(SUPPORT_STATUS_SCORES_OVERRIDE)
      SV_StatusScoresOverride_AdjustScore(ps->persistant[PERS_SCORE], i), cl->ping, cl->name);
#else
      ps->persistant[PERS_SCORE], cl->ping, cl->name);
#endif
      if (statusLength + playerLength >= MAX_PACKETLEN - 4)
      {
        break; //can't hold any more
      }

      s = Q_stradd(s, player);
      statusLength += playerLength;
    }
  }

  NET_OutOfBandPrint(NS_SERVER, from, "statusResponse\n%s\n%s", infostring, status);
}

/*
================
SVC_Info

Responds with a short info message that should be enough to determine
if a user is interested in a server to do a full status
================
*/
static const void
SVC_Info(const netadr_t *from)
{
  unsigned i;
  unsigned count;
  const qchar *gamedir;
  qchar infostring[MAX_INFO_STRING];

  //Chey: bugtraq 12534
  if (!SVC_VerifyChallenge(Cmd_Argv(1)))
  {
    SV_WriteAttackLog(va("%s: Invalid challenge from %s, dropping request.\n", __func__, NET_AdrToString(from)));
    return;
  }

  /*
   * Check whether Cmd_Argv(1) has a sane length. This was not done in the original Quake3 version which led
   * to the Infostring bug discovered by Luigi Auriemma. See http://aluigi.altervista.org/ for the advisory.
   */

  //a maximum challenge length of 128 should be more than plenty
  if (strlen(Cmd_Argv(1)) > 128)
  {
    SV_WriteAttackLog(va("%s: Challenge length exceeded from %s, dropping request.\n", __func__, NET_AdrToString(from)));
    return;
  }

  //don't count privateclients
  count = 0;
  
  for(i = sv_privateClients->integer;i < sv.maxclients;i++)
  {
    if (svs.clients[i].state >= CS_CONNECTED)
    {
      count++;
    }
  }

  infostring[0] = '\0';
  //echo back the parameter to status. so servers can use it as a challenge to prevent timed spoofed reply packets that add ghost servers
  Info_SetValueForKey(infostring, "challenge", Cmd_Argv(1));
  Info_SetValueForKey(infostring, "protocol", va("%i", PROTOCOL_VERSION));
  Info_SetValueForKey(infostring, "hostname", sv_hostname->string);
  Info_SetValueForKey(infostring, "serverload", va("%i", svs.serverLoad)); //Chey: included in SV_Status_f

  if (sv_cpuusagepublic->integer)
  {
    Info_SetValueForKey(infostring, "cpuusage", va("%lf", svs.stats.cpu));
  }

  if (sv_avgframetimepublic->integer)
  {
    Info_SetValueForKey(infostring, "avgframetime", va("%lf", svs.stats.avg));
  }

  Info_SetValueForKey(infostring, "mapname", sv_mapname->string);
  Info_SetValueForKey(infostring, "clients", va("%i", count));
  Info_SetValueForKey(infostring, "sv_maxclients", va("%i", sv.maxclients - sv_privateClients->integer - sv_democlients->integer));
  Info_SetValueForKey(infostring, "pure", va("%i", sv.pure));
  Info_SetValueForKey(infostring, "gamename", GAMENAME_FOR_MASTER);

#if defined(USE_VOIP)
    if (sv_voip->integer)
    {
      Info_SetValueForKey(infostring, "voip", va("%i", sv_voip->integer));
    }
#endif
#if defined(INCLUDE_LEGACY_CHALLENGE)
  if (sv_minPing->integer)
  {
    Info_SetValueForKey(infostring, "minPing", va("%i", sv_minPing->integer));
  }
  if (sv_maxPing->integer)
  {
    Info_SetValueForKey(infostring, "maxPing", va("%i", sv_maxPing->integer));
  }
#endif
  gamedir = Cvar_VariableString("fs_game");

  if (*gamedir != '\0')
  {
    Info_SetValueForKey(infostring, "game", gamedir);
  }

  NET_OutOfBandPrint(NS_SERVER, from, "infoResponse\n%s", infostring);
}

#if defined(INCLUDE_REMOTE_COMMANDS) //Chey: the #endif is only after SV_DropClientsByAddress to shut up a compiler warning, dunno if i will use it elsewhere
/*
================
SV_IsRconWhitelisted

Check whether a certain address is rcon whitelisted
================
*/
static const ID_INLINE qbool
SV_IsRconWhitelisted(const netadr_t *from)
{
  unsigned index;
  const serverRconPassword_t *curPass;

  for(index = 0;index < rconWhitelistCount;index++)
  {
    curPass = &rconWhitelist[index];

    if (NET_CompareBaseAdrMask(&curPass->ip, from, curPass->subNet))
    {
      return qtrue;
    }
  }

  return qfalse;
}

/*
================
SV_DropClientsByAddress
================
*/
static const ID_INLINE void
SV_DropClientsByAddress(const netadr_t *drop, const qchar *reason)
{
  unsigned i;
  client_t *cl;

  //for all clients
  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    //skip free slots
    if (cl->state == CS_FREE)
    {
      continue;
    }

    //skip other addresses
    if (NET_CompareBaseAdr(drop, &cl->netchan.remoteAddress))
    {
      continue;
    }

    //address matches so drop
    SV_DropClient(cl, reason);
  }
}

/*
================
SVC_FlushRedirect
================
*/
static const void
SVC_FlushRedirect(const qchar *outputbuf)
{
  if (*outputbuf)
  {
    NET_OutOfBandPrint(NS_SERVER, &svs.redirectAddress, "print\n%s", outputbuf);
  }
}
#endif

/*
================
SVC_Ping

Simple server ping, used to check online status.
================
*/
static const ID_INLINE void
SVC_Ping(const netadr_t *from)
{
  NET_OutOfBandPrint(NS_SERVER, from, "print\nacknowledged\n");
}

/*
================
SVC_CheckDRDoS

DRDoS stands for "Distributed Reflected Denial of Service"
See here: http://www.lemuria.org/security/application-drdos.html

If the address isn't NA_IP it is automatically denied.

Return qfalse if we're good.
Otherwise, return qtrue if we need to block.

NOTE: Do not call if sv_protect 2 is not set!
================
*/
static const qbool
SVC_CheckDRDoS(const netadr_t *from)
{
  unsigned i;
  unsigned oldestBan;
  unsigned oldestBanTime;
  unsigned globalCount;
  unsigned specificCount;
  receipt_t *receipt;
  netadr_t modifiedFrom;
  unsigned oldest;
  unsigned oldestTime;
  static unsigned lastGlobalLogTime = 0;
  floodBan_t *ban;

  //Chey: i added this cvar because i dont believe this should be a de facto rule, also id say its good for testing purposes, makes things easier
  if (!sv_owolfAffectsLan->integer)
  {
    //network is usually smart enough to block incoming udp packets with a source being a spoofed lan and if not then sending packets to other lan is not a big deal na loopback qualifies as lan
    if (Sys_IsLANAddress(from))
    {
      return qfalse;
    }
  }

  modifiedFrom = *from;

  if (modifiedFrom.type == NA_IP)
  {
    modifiedFrom.ipv._4[3] = 0; //xx.xx.xx.0
  }
#if defined(USE_IPV6)
  else if (modifiedFrom.type == NA_IP6)
  {
    Com_Memset(modifiedFrom.ipv._6 + 7, 0, 9); //mask to /56
  }
#endif
  else
  {
    //Chey: i have no idea what this could possibly be
    return qtrue;
  }

  //quick exit strategy which does not impact server performance
  oldestBan = 0;
  oldestBanTime = 0x7fffffff;

  for(i = 0;i < MAX_INFO_FLOOD_BANS;i++)
  {
    ban = &svs.infoFloodBans[i];

    if (svs.time - ban->time < 120000 && NET_CompareBaseAdr(&modifiedFrom, &ban->adr)) //two minutes ban
    {
      ban->count++;

      if (!ban->flood && ((svs.time - ban->time) >= 3000) && ban->count <= 5)
      {
        SV_WriteAttackLog(va("%s: Unban info flood protect for address %s, they're not flooding.\n", __func__, NET_AdrToString(from)));
        Com_Memset(ban, 0, sizeof(floodBan_t));
        oldestBan = i;
        break;
      }

      if (ban->count >= 180)
      {
        SV_WriteAttackLog(va("%s: Renewing info flood ban for address %s, received %i getinfo/getstatus requests in %i milliseconds.\n", __func__, NET_AdrToString(from), ban->count, svs.time - ban->time));
        ban->time = svs.time;
        ban->count = 0;
        ban->flood = qtrue;
      }

      return qtrue;
    }

    if (ban->time < oldestBanTime)
    {
      oldestBanTime = ban->time;
      oldestBan = i;
    }
  }

  //count receipts in last two seconds
  globalCount = 0;
  specificCount = 0;
  oldest = 0;
  oldestTime = 0x7fffffff;

  for(i = 0;i < MAX_INFO_RECEIPTS;i++)
  {
    receipt = &svs.infoReceipts[i];

    if (receipt->time + 2000 > svs.time)
    {
      if (receipt->time)
      {
        //all receipts start at zero and svs time is close to zero so check that receipt time is set so master server query doesnt get ignored but that means an unlimited number of getinfo and getstatus responses can be sent during the first frame of a servers life
        globalCount++;
      }

      if (NET_CompareBaseAdr(&modifiedFrom, &receipt->adr))
      {
        specificCount++;
      }
    }

    if (receipt->time < oldestTime)
    {
      oldestTime = receipt->time;
      oldest = i;
    }
  }

  if (specificCount >= 3) //sent three to ip in last two seconds
  {
    SV_WriteAttackLog(va("%s: Possible DRDoS attack to address %s, putting into temporary getinfo/getstatus ban list.\n", __func__, NET_AdrToString(from)));
    ban = &svs.infoFloodBans[oldestBan];
    ban->adr = modifiedFrom;
    ban->time = svs.time;
    ban->count = 0;
    ban->flood = qfalse;
    return qtrue;
  }

  if (globalCount == MAX_INFO_RECEIPTS) //all in last two seconds
  {
    //detect time wrap where server sets time back to zero since static variable does not get zeroed when time wraps
    //TTimo way is casting everything including the difference to uint but this may be confusing
    if (svs.time < lastGlobalLogTime)
    {
      lastGlobalLogTime = 0;
    }

    if (lastGlobalLogTime + 1000 <= svs.time) //one log per second
    {
      SV_WriteAttackLog("%s: Detected flood of arbitrary getinfo/getstatus connectionless packets.\n");
      lastGlobalLogTime = svs.time;
    }

    return qtrue;
  }

  receipt = &svs.infoReceipts[oldest];
  receipt->adr = modifiedFrom;
  receipt->time = svs.time;
  return qfalse;
}

#if defined(INCLUDE_REMOTE_COMMANDS)
/*
===============
SVC_RemoteCommand

An rcon packet arrived from the network.
Shift down the remaining args
Redirect all printfs
===============
*/
static const void
SVC_RemoteCommand(const netadr_t *from, msg_t *msg)
{
  qbool valid;
  //TTimo - scaled down to accumulate, but not overflow anything network wise, print wise, etc. (OOB messages are the bottleneck here)
  qchar sv_outputbuf[1024 - 16];
  const qchar *cmd_aux;
  const qchar *pw;
  fileHandle_t rconLog = 0;

  if (sv_rconWhitelist->string && *sv_rconWhitelist->string)
  {
    if (SV_IsRconWhitelisted(from))
    {
      if (com_developer->integer)
      {
        Com_Printf("unauthorized rcon attempt from %s\n", NET_AdrToString(from));
      }

      NET_OutOfBandPrint(NS_SERVER, from, "print\nsorry but you can not use rcon commands\n");
      SV_DropClientsByAddress(from, "you are not admin");
      return;
    }
  }

  if (strlen(sv_rconLog->string))
  {
    rconLog = FS_FOpenFileAppend(sv_rconLog->string);

    if (!rconLog)
    {
      Com_Printf("WARNING: unable to open sv_rconLog: %s\n", sv_rconLog->string);
      Cvar_Set("sv_rconLog", "");
    }
  }

  const qchar *message = "";

  pw = Cmd_Argv(1);

  if ((sv_rconPassword->string[0] && strcmp(pw, sv_rconPassword->string) == 0) || (rconPassword2[0] && strcmp(pw, rconPassword2) == 0)
  {
    valid = qtrue;
    message = va("Rcon from %s: %s\n", NET_AdrToString(from), Cmd_ArgsFrom(2));
  }
  else
  {
    valid = qfalse;
    message = ("Bad rcon from %s: %s\n", NET_AdrToString(from), Cmd_ArgsFrom(2));
  }

  Com_Printf("%s", message);

  if (rconLog)
  {
    qtime_t qt;
    Com_RealTime(&qt);
    const qchar *timestamp = va("%02i/%02i/%02i %02i:%02i:%02i ", qt.tm_mday, qt.tm_mon, qt.tm_year - 100, qt.tm_hour, qt.tm_min, qt.tm_sec);
    FS_Write(timestamp, strlen(timestamp), rconLog);
    FS_Write(message, strlen(message), rconLog);
    FS_FCloseFile(rconLog);
  }

  //start redirecting all print outputs to the packet
  //FIXME: rcon redirection could be improved as big commands such as status lead to sending out of band packets on every call to com printf and leads to client overflows
  redirectAddress = *from;
  Com_BeginRedirect(sv_outputbuf, sizeof(sv_outputbuf), SV_FlushRedirect);

  if (!sv_rconPassword->string[0] && !rconPassword2[0])
  {
    Com_Printf("No rconpassword set on the server.\n");
  }
  else if (!valid)
  {
    Com_Printf("Bad rconpassword.\n");
  }
  else
  {
    //https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=543
    //get the command directly, "rcon <pass> <command>" to avoid quoting issues
    //extract the command by walking
    //since the cmd formatting can fuckup (amount of spaces), using a dumb step by step parsing
    cmd_aux = Cmd_Cmd();

    while(*cmd_aux && *cmd_aux <= ' ') //skip whitespace
    {
      cmd_aux;;
    }

    cmd_aux += 4; //"rcon"

    while(*cmd_aux == ' ')
    {
      cmd_aux++;
    }

    if (*cmd_aux == '"')
    {
      cmd_aux++;

      while(*cmd_aux && *cmd_aux != '"') //quoted password
      {
        cmd_aux++;
      }

      if (*cmd_aux == '"')
      {
        cmd_aux++;
      }
    }
    else
    {
      while(*cmd_aux && *cmd_aux != ' ') //password
      {
        cmd_aux++;
      }
    }

    while(*cmd_aux == ' ')
    {
      cmd_aux++;
    }

    Cmd_ExecuteString(cmd_aux);
  }

  Com_EndRedirect();
}
#endif

/*
=================
SVC_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/
static const void
SVC_ConnectionlessPacket(const netadr_t *from, msg_t *msg)
{
  const qchar *s;
  const qchar *c;
  const unsigned now = Sys_Milliseconds();
  const unsigned rate = sv_maxOOBRate->integer;
  const unsigned iprate = sv_maxOOBRateIP->integer;
  const unsigned period = 1000 / rate;
  const unsigned ipperiod = 1000 / iprate;
  const unsigned burst = rate; //one second worth of packets
  const unsigned ipburst = 10 * iprate; //one second worth of packets
  unsigned i;
  static unsigned droppedAdr;
  static unsigned dropped[SVC_MAX];
  static unsigned lastMsgAdr;
  static unsigned lastMsg[SVC_MAX];
  static rateLimit_t bucket[SVC_MAX];
  static const qchar * const commands[SVC_MAX] =
  {
    "invalid",
    "connect",
    "getstatus",
    "getinfo",
    "getchallenge",
    "rcon",
    "ping",
    "disconnect"
  };
  svcType_t cmd;

  if (!com_sv_running->integer)
  {
    return;
  }

  if ((sv_protect->integer & SVP_OWOLF) && SVC_CheckDRDoS(from))
  {
    return;
  }

  //Chey: XXX: this is an irrelevant message now c:
  //Chey: DO NOT use this with SVP_OWOLF enabled, as this will
  //prevent SVC_CheckDRDoS from having valid counters by returning
  //early, this is purely for making SVP_XREAL stricter than it
  //already is
  if (sv_protect->integer & SVP_XREAL)
  {
    if (SVC_RateLimitAddress(from, ipburst, ipperiod, now))
    {
      droppedAdr++;
      return;
    }
  }

  cmd = SVC_INVALID;

  MSG_BeginReadingOOB(msg);
  MSG_ReadLong(msg); //skip -1 marker

  if (!memcmp("connect ", msg->data + 4, 8))
  {
    //if we assume 200% compression ratio on userinfo
    if (msg->cursize > MAX_INFO_STRING * 2)
    {
      if (com_developer->integer)
      {
        Com_Printf("%s: connect packet is too long - %i\n", NET_AdrToString(from), msg->cursize);
      }

      return;
    }

    Huff_Decompress(msg, 12);
    cmd = SVC_CONNECT;
  }

  s = MSG_ReadStringLine(msg);
  Cmd_TokenizeString(s);

  c = Q_strlwr(Cmd_Argv(0));

  for(i = SVC_FIRST;i < SVC_MAX && cmd == SVC_INVALID;i++)
  {
    if (!Q_stricmp(c, commands[i]))
    {
      cmd = (svcType_t)i;
    }
  }

  //Chey: XXX: this is an irrelevant message now c:
  //Chey: DO NOT use this with SVP_OWOLF enabled, as this will
  //prevent SVC_CheckDRDoS from having valid counters by returning
  //early, this is purely for making SVP_XREAL stricter than it
  //already is
  if (sv_protect->integer & SVP_XREAL)
  {
    if (SVC_RateLimit(&bucket[cmd], burst, period, now))
    {
      dropped[cmd]++;
      return;
    }

    //this will print every 5 'period' msecs
    if (dropped[cmd] > 0 && lastMsg[cmd] + 5000 < now)
    {
      SV_WriteAttackLog(va("%s: \"%s\" rate limit exceeded, dropped %d request%s.\n", __func__, commands[cmd], dropped[cmd], dropped[cmd] == 1 ? "":"s"));
      dropped[cmd] = 0;
      lastMsg[cmd] = now;
    }

    if (droppedAdr > 0 && lastMsgAdr + 5000 < now)
    {
      SV_WriteAttackLog(va("%s: IP rate limit exceeded, dropped %d request%s.\n", __func__, droppedAdr, droppedAdr == 1 ? "":"s"));
      droppedAdr = 0;
      lastMsgAdr = now;
    }
  }

  if (com_developer->integer)
  {
    Com_Printf("sv packet %s: %s\n", NET_AdrToString(from), c);
  }

  switch(cmd)
  {
    case
    SVC_GETSTATUS:
      if (sv_hidden->integer)
      {
        return;
      }

      SVC_Status(from);
      break;

    case
    SVC_GETINFO:
      //if the server is hidden do not respond to getinfo requests by default
#if defined(INCLUDE_LEGACY_CHALLENGE)
      if (sv_hidden->integer)
      {
        if (sv_legacyChallenge->integer)
        {
          if (!SV_CheckChallenge(from))
          {
            return;
          }
        }
        else
        {
          return;
        }
      }
#else
      if (sv_hidden->integer)
      {
        return;
      }
#endif

      SVC_Info(from);
      break;

    case
    SVC_GETCHALLENGE:
      SV_GetChallenge(from);
      break;

    case
    SVC_CONNECT:
      SV_DirectConnect(from);
      break;

    case
    SVC_RCON:
#if defined(INCLUDE_REMOTE_COMMANDS)
      SVC_RemoteCommand(from, msg);
#endif
      break;

    case
    SVC_PING:
      if (sv_hidden->integer)
      {
        return;
      }

      SVC_Ping(from);
      break;

    case
    SVC_DISCONNECT:
      //if client starts local server some spurious server disconnect messages may happen when new server sees final sequenced messages to old client
      break;

    default:
      SV_WriteAttackLog(va("Bad connectionless packet from %s:\n%s\n", NET_AdrToString(from), s)); //changed from com dprintf to print in attack log and do com printf if attack log not set
      break;
  }
}

//============================================================================

/*
=================
SV_IsValidNetwork

Used by SV_ReadPackets, checks if a network connection is valid.
=================
*/
static const ID_INLINE qbool
SV_IsValidNetwork(const netadr_t *from)
{
  if (Sys_IsLANAddress(from))
  {
    return qtrue;
  }

  if (NET_IsLocalAddress(from))
  {
    return qtrue;
  }

  if (from->type == NA_LOOPBACK)
  {
    return qtrue;
  }

  if (from->type == NA_IP)
  {
    return qtrue;
  }
#if defined(USE_IPV6)
  if (from->type == NA_IP6)
  {
    return qtrue;
  }
#endif
  return qfalse;
}

/*
=================
SV_ReadPackets
=================
*/
const void
SV_ReadPackets(const netadr_t *from, msg_t *msg)
{
  unsigned i;
  client_t *cl;
  unsigned qport;

  if (!SV_IsValidNetwork(from))
  {
    return;
  }

  if (msg->cursize < 6) //too short for anything
  {
    return;
  }

  //check for connectionless packet (0xffffffff) first
  if (*(int32_t *)msg->data == -1)
  {
    SVC_ConnectionlessPacket(from, msg);
    return;
  }

  if (sv.state == SS_DEAD)
  {
    return;
  }

  //read the qport out of the message so we can fix up stupid address translating routers
  MSG_BeginReadingOOB(msg);
  MSG_ReadLong(msg); //sequence number
  qport = MSG_ReadShort(msg) & 0xffff;

  //find which client the message is from
  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (cl->state == CS_FREE)
    {
      continue;
    }

    if (!NET_CompareBaseAdr(from, &cl->netchan.remoteAddress))
    {
      continue;
    }

    //it is possible to have multiple clients from a single IP address, so they are differentiated by the qport variable
    if (cl->netchan.qport != qport)
    {
      continue;
    }

    //make sure it is a valid, in sequence packet
    if (SV_Netchan_Process(cl, msg))
    {
      //the IP port can't be used to differentiate them, because
      //some address translating routers periodically change UDP
      //port assignments
      if (cl->netchan.remoteAddress.port != from->port)
      {
        Com_Printf("sv read packets fixing up translated port\n");
        cl->netchan.remoteAddress.port = from->port;
      }

      //zombie clients still need to do the Netchan_Process to make sure they don't need to retransmit the final reliable message, but they don't do any other processing
      if (cl->state != CS_ZOMBIE)
      {
        cl->lastPacketTime = svs.time; //dont timeout
        SV_ExecuteClientMessage(cl, msg);
      }
      return;
    }
  }
}

/*
===================
SV_CalcPings

Updates the cl->ping variables
===================
*/
static const void
SV_CalcPings(void)
{
  unsigned i;
  unsigned j;
  client_t *cl;
  unsigned total;
  unsigned count;
  unsigned delta;
  playerState_t *ps;

  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (!cl)
    {
      continue;
    }

    if (cl->state != CS_ACTIVE)
    {
      cl->ping = 999;
      continue;
    }

    if (!cl->gentity)
    {
      cl->ping = 999;
      continue;
    }

    if (cl->netchan.remoteAddress.type == NA_BOT || (cl->gentity && (cl->gentity->r.svFlags & SVF_BOT)))
    {
      cl->ping = 0; //Chey: bots have no real ping
      continue;
    }

    total = 0;
    count = 0;

#if defined(INCLUDE_SV_PINGFIX)
    if (sv_pingFix->integer)
    {
      qint current_frame = cl->netchan.outgoingSequence - 1;
      qint current_ack_time = 0;

      for(j = 0;j < PACKET_BACKUP && current_frame > 0;j++)
      {
        //read frames backwards from recent to old
        clientSnapshot_t *frame = &cl->frames[(current_frame--) & PACKET_MASK];

        if (frame->messageAcked && (!current_ack_time || frame->messageAcked < current_ack_time))
        {
          current_ack_time = frame->messageAcked;
        }

        if (current_ack_time)
        {
          delta = current_ack_time - frame->messageSent;
          count++;
          total += delta;
        }
      }
    }
    else
    {
#endif
      for(j = 0;j < PACKET_BACKUP;j++)
      {
        if (!cl->frames[j].messageAcked)
        {
          continue;
        }

        delta = cl->frames[j].messageAcked - cl->frames[j].messageSent;
        count++;
        total += delta;
      }
#if defined(INCLUDE_SV_PINGFIX)
    }
#endif

    if (!count)
    {
      cl->ping = 999;
    }
    else
    {
      cl->ping = total/count;

      if (cl->ping > 999)
      {
        cl->ping = 999;
      }

#if defined(INCLUDE_SV_PINGFIX)
      if (sv_pingFix->integer)
      {
        if (cl->ping < 1)
        {
          //certain server browsers assume that players with 0 ping are bots, so make sure the minimum ping for humans is 1
          cl->ping = 1;
        }
      }
#endif
    }

    // let the game qvm know about the ping
    ps = SV_GameClientNum(i);
    ps->ping = cl->ping;
  }
}

/*
==================
SV_CheckTimeouts

If a packet has not been received from a client for timeout->integer 
seconds, drop the conneciton.  Server time is used instead of
realtime to avoid dropping the local client while debugging.

When a client is normally dropped, the client_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
static const ID_INLINE void
SV_CheckTimeouts(void)
{
  const unsigned now = Sys_Milliseconds();
  qint i;
  client_t *cl;
  qint droppoint;
  qint zombiepoint;

  droppoint = svs.time - 1000 * sv_timeout->integer;
  zombiepoint = svs.time - 1000 * sv_zombietime->integer;

  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    //message times may be wrong across a changelevel
    if (cl->lastPacketTime - svs.time > 0)
    {
      cl->lastPacketTime = svs.time;
    }

    if (cl->state == CS_ZOMBIE && cl->lastPacketTime - zombiepoint < 0)
    {
      //using the client id cause the cl->name is empty at this point
      SV_SetClientState(cl, CS_FREE); //can now be reused
      continue;
    }

    if (cl->justConnected && svs.time - cl->lastPacketTime > 4000)
    {
      //for real client 4 seconds is more than enough to respond
      SVC_RateDropAddress(&cl->netchan.remoteAddress, 10, 1000, now); //enforce burst with progressive multiplier
      SV_DropClient(cl, NULL); //drop silently
      cl->state = CS_FREE;
      continue;
    }

    if (cl->state >= CS_CONNECTED && cl->lastPacketTime - droppoint < 0)
    {
      //wait several frames so a debugger session doesn't
      //cause a timeout
      if (++cl->timeoutCount > 5)
      {
        SV_DropClient(cl, "timed out"); 
        cl->state = CS_FREE;  //dont bother with zombie state
      }
    }
    else
    {
      cl->timeoutCount = 0;
    }
  }
}


/*
==================
SV_CheckPaused
==================
*/
static const ID_INLINE qbool
SV_CheckPaused(void)
{
#if defined(DEDICATED)
  //can't pause on dedicated servers
  return qfalse;
#else
  qbool players;
  client_t *cl;
  unsigned i;

  if (!cl_paused->integer)
  {
    return qfalse;
  }

  //do not pause if anyone is connected
  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (cl->state >= CS_CONNECTED && ((!(cl->gentity->r.svFlags & SVF_BOT) || cl->netchan.remoteAddress.type != NA_BOT)))
    {
      players = qtrue;
      break;
    }
  }

  if (players)
  {
    //dont pause
    if (sv_paused->integer)
    {
      Cvar_Set("sv_paused", "0");
    }

    return qfalse;
  }

  if (!sv_paused->integer)
  {
    Cvar_Set("sv_paused", "1");
  }

  return qtrue;
#endif //!DEDICATED
}

/*
==================
SV_CheckCvars
==================
*/
static const void
SV_CheckCvars(void)
{
  static qint lastMod = -1;
  qbool changed = qfalse;

  if (sv_hostname->modificationCount != lastMod)
  {
    qchar hostname[MAX_INFO_STRING];
    qchar *c = hostname;
    lastMod = sv_hostname->modificationCount;

    Q_strncpyz(hostname, sv_hostname->string, sizeof(hostname));

    while(*c)
    {
      if ((*c == '\\') || (*c == ';') || (*c == '"'))
      {
        *c = '.';
        changed = qtrue;
      }

      c++;
    }

    if (changed)
    {
      Cvar_Set("sv_hostname", hostname);
    }
  }
}

/*
==================
SV_IntegerOverflowShutDown
==================
*/
static const ID_INLINE void
SV_Restart(const qchar *reason)
{
  qbool sv_shutdown = qfalse;
  qchar mapName[MAX_QPATH];
  qint i;

  if (svs.clients)
  {
    //check if we can reset map time without full server shutdown
    for(i = 0;i < sv.maxclients;i++)
    {
      if (svs.clients[i].state >= CS_CONNECTED)
      {
        sv_shutdown = qtrue;
        break;
      }
    }
  }

  sv.time = 0; //force level time reset
  sv.restartTime = 0;

  //save map name in case it gets cleared during shut down
  Cvar_VariableStringBuffer("mapname", mapName, sizeof(mapName));

  if (sv_shutdown)
  {
    SV_Shutdown(reason);
  }

  Cbuf_AddText(va("map %s\n", mapName));
}

/*
==================
SV_UpdateQueue
==================
*/
static const ID_INLINE void
SV_UpdateQueue(void)
{
  netadr_t temp[MAX_QUEUE];
  unsigned templast[MAX_QUEUE];
  unsigned i;
  unsigned j;

  j = 0;

  for(i = 0;i < queueCount;i++)
  {
    if (svs.time - lastQueue[i] > 4000)
    {
      SV_WriteAttackLog(va("Queue position %d for %s has timed out! - %d ms\n", i, NET_AdrToString(&queue[i]), svs.time - lastQueue[i]));
      continue;
    }

    temp[j] = queue[i];
    templast[j] = lastQueue[i];
    j++;
  }

  queueCount = 0;

  for(i = 0;i < j;i++)
  {
    queue[i] = temp[i];
    lastQueue[i] = templast[i];
    queueCount++;
  }
}

/*
==================
SV_FrameMsec

Return time in msec until processing of next server frame.
==================
*/
const qint
SV_FrameMsec(void)
{
  if (sv_fps)
  {
    const unsigned frameMsec = 1000.0f / sv_fps->value;
    const unsigned scaledResidual = (const unsigned)(sv.timeResidual / com_timescale->value);

    if (frameMsec < scaledResidual)
    {
      return 0;
    }

    return frameMsec - scaledResidual;
  }

  return 1;
}

/*
==================
SV_TrackCvarChanges
==================
*/
void
SV_TrackCvarChanges(void)
{
  client_t *cl;
  qint i;

  if (sv_maxRate->integer && sv_maxRate->integer < 1000)
  {
    Cvar_Set("sv_maxRate", "1000");
    Com_DPrintf("sv_maxRate adjusted to 1000\n");
  }

  if (sv_minRate->integer && sv_minRate->integer < 1000)
  {
    Cvar_Set("sv_minRate", "1000");
    Com_DPrintf("sv_minRate adjusted to 1000\n");
  }

  if (sv_cl_fps->integer != sv_fps->integer)
  {
    Cvar_Set("sv_cl_fps", va("%i", sv_fps->integer));
  }

  Cvar_ResetGroup(CVG_SERVER, qfalse);

  if (sv.state == SS_DEAD || !svs.clients)
  {
    return;
  }

  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (cl->state >= CS_CONNECTED)
    {
      SV_UserinfoChanged(cl, qfalse, qfalse); //do not update userinfo, do not run filter
    }
  }
}

/*
==================
SV_Frame

Player movement occurs as a result of packet events, which
happen before SV_Frame is called
==================
*/
const void
SV_Frame(const qint msec)
{
  unsigned frameMsec;
  unsigned startTime;
  unsigned frameStartTime = 0;
  unsigned frameEndTime;
  unsigned totalTime;
  unsigned averageFrameTime;
  unsigned timeVal;
  unsigned i;
  static unsigned updatequeue;
  static unsigned start;
  static unsigned end;

  start = Sys_Milliseconds();
  svs.stats.idle += (double)(start - end) / 1000;

  if (Cvar_CheckGroup(CVG_SERVER))
  {
    SV_TrackCvarChanges(); //update rate settings, etc.
  }

  //the menu kills the server with this cvar
  if (sv_killserver->integer)
  {
    SV_Shutdown("Server was killed");
    Cvar_Set("sv_killserver", "0");
    return;
  }

  if (!com_sv_running->integer)
  {
    //running as a server, but no map loaded
#if defined(DEDICATED)
    //block until something interesting happens
    Sys_Sleep(-1);
#endif

    return;
  }

  //allow pause if only the local client is connected
  if (SV_CheckPaused())
  {
    return;
  }

  if (com_dedicated->integer)
  {
    frameStartTime = Sys_Milliseconds();
  }

  //if it isn't time for the next frame, do nothing
  if (sv_fps->integer < 1)
  {
    Cvar_Set("sv_fps", "10");
  }

  frameMsec = 1000 / sv_fps->integer * com_timescale->value;

  //don't let it scale below 1ms
  if (frameMsec < 1)
  {
    Cvar_Set("timescale", va("%f", sv_fps->integer / 1000.0f));
    frameMsec = 1;
  }

  sv.timeResidual += msec;

  //Chey: FIXME: make this use scaledResidual?
  if (com_dedicated && com_dedicated->integer && sv.timeResidual < frameMsec && (!com_timescale || com_timescale->value >= 1))
  {
    //first check if we need to send any pending packets
    timeVal = SV_SendQueuedPackets();

    //NET_Sleep will give the OS time slices until either get a packet
    //or time enough for a server frame has gone by
    NET_Sleep((Q_min(frameMsec - sv.timeResidual, timeVal)) * (1000 - 500));
    return;
  }

  if (!com_dedicated->integer)
  {
    SV_BotFrame(sv.time + sv.timeResidual);
  }

  //if time is about to hit the 32nd bit, kick all clients
  //and clear sv.time, rather
  //than checking for negative time wraparound everywhere.
  //2giga-milliseconds = 23 days, so it won't be too often
  if (sv.time > 0x78000000)
  {
    SV_Restart("Restarting server due to time wrapping");
    return;
  }

  //try to do silent restart earlier if possible
  if (sv.time > (12 * 3600 * 1000) && (!sv_levelTimeReset->integer || sv.time > 0x40000000))
  {
    if (svs.clients)
    {
      for(i = 0;i < sv.maxclients;i++)
      {
        //FIXME: deal with bots... maybe reconnect?
        if (svs.clients[i].state != CS_FREE)
        {
          break;
        }
      }

      if (i == sv.maxclients)
      {
        SV_Restart("Restarting server");
        return;
      }
    }
  }

  if (sv.restartTime && sv.time - sv.restartTime >= 0)
  {
    sv.restartTime = 0;
    Cbuf_AddText("map_restart 0\n");
    return;
  }

  //update infostrings if anything has been changed
  if (cvar_modifiedFlags & CVAR_SERVERINFO)
  {
    SV_SetConfigstring(CS_SERVERINFO, Cvar_InfoString(CVAR_SERVERINFO, NULL));
    cvar_modifiedFlags &= ~CVAR_SERVERINFO;
  }

  if (cvar_modifiedFlags & CVAR_SYSTEMINFO)
  {
    SV_SetConfigstring(CS_SYSTEMINFO, Cvar_InfoString_Big(CVAR_SYSTEMINFO, NULL));
    cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;
  }

  if (com_speeds->integer)
  {
    startTime = Sys_Milliseconds();
  }
  else
  {
    startTime = 0;  //quite a compiler warning
  }

  //update ping based on the all received frames
  SV_CalcPings();

  if (com_dedicated->integer)
  {
    SV_BotFrame(sv.time);
  }

  //run the game simulation in chunks
  while(sv.timeResidual >= frameMsec)
  {
    sv.timeResidual -= frameMsec;
    svs.time += frameMsec;
    sv.time += frameMsec;

    //let everything in the world think and move
#if defined(USE_JAVA)
    Java_G_RunFrame(sv.time);
#else
    VM_Call(sv.gvm, 1, GAME_RUN_FRAME, sv.time);
#endif

    if (sv.demoState == DS_RECORDING)
    {
      SV_DemoWriteFrame();
    }
    else if (sv.demoState == DS_PLAYBACK)
    {
      SV_DemoReadFrame();
    }
  }

  if (com_speeds->integer)
  {
    time_game = Sys_Milliseconds() - startTime;
  }

  SV_CheckTimeouts(); //check timeouts
  SV_IssueNewSnapshot(); //reset current and build new snapshot on first query
  SV_SendClientMessages(); //send messages back to the clients
  SV_CheckCvars(); //Chey
  SV_MasterHeartbeat(HEARTBEAT_GAME); //send a heartbeat to the master if needed

  if (com_dedicated->integer)
  {
    frameEndTime = Sys_Milliseconds();

    svs.totalFrameTime += (frameEndTime - frameStartTime);

    //we may send warnings to the game in case the frametime is unacceptable
    //Com_Printf("FRAMETIME frame: %i total %i\n", frameEndTime - frameStartTime, svs.totalFrameTime);

    svs.currentFrameIndex++;

    //if (!(svs.currentFrameIndex % 50))
    //{
      //Com_Printf("currentFrameIndex: %i\n", svs.currentFrameIndex);
    //}

    if (svs.currentFrameIndex == SERVER_PERFORMANCECOUNTER_FRAMES)
    {
      averageFrameTime = svs.totalFrameTime / SERVER_PERFORMANCECOUNTER_FRAMES;
      svs.sampleTimes[svs.currentSampleIndex % SERVER_PERFORMANCECOUNTER_SAMPLES] = averageFrameTime;
      svs.currentSampleIndex++;

      if (svs.currentSampleIndex > SERVER_PERFORMANCECOUNTER_SAMPLES)
      {
        totalTime = 0;

        for(i = 0;i < SERVER_PERFORMANCECOUNTER_SAMPLES;i++)
        {
          totalTime += svs.sampleTimes[i];
        }

        if (!totalTime)
        {
          totalTime = 1;
        }

        averageFrameTime = totalTime / SERVER_PERFORMANCECOUNTER_SAMPLES;
        svs.serverLoad = (qint)((averageFrameTime / (float)frameMsec) * 100);
      }

      //Com_Printf("serverload: %i (%i/%i)\n", svs.serverLoad, averageFrameTime, frameMsec);
      svs.totalFrameTime = 0;
      svs.currentFrameIndex = 0;
    }
  }
  else
  {
    svs.serverLoad = 0;
  }

  //collect timing statistics
  //the above 2.60 performance thingy is just inaccurate (30 seconds 'stats')
  //to give good warning messages and is only done for dedicated
  end = Sys_Milliseconds();
  svs.stats.active += ((double)(end - start)) / 1000;

  if (++svs.stats.count == STATFRAMES(sv_fps->integer)) //@sv_fps //5 seconds
  {
    svs.stats.latched_active = svs.stats.active;
    svs.stats.latched_idle = svs.stats.idle;
    svs.stats.active = 0;
    svs.stats.idle = 0;
    svs.stats.count = 0;
    svs.stats.cpu = svs.stats.latched_active + svs.stats.latched_idle;

    if (svs.stats.cpu != 0.f)
    {
      svs.stats.cpu = 100 * svs.stats.latched_active / svs.stats.cpu;
    }

    svs.stats.avg = 1000 * svs.stats.latched_active / STATFRAMES(sv_fps->integer);

    //FIXME: add mail, irc, player info, etc for both warnings
    //TODO: inspect/adjust these values and/or add cvars
    if (sv_warningscpu->integer > 0)
    {
      if (svs.stats.cpu > sv_warningscpu->integer)
      {
        Com_Printf("^3WARNING: Server CPU has reached a critical usage of %i%%\n", (qint)svs.stats.cpu);
      }
    }
    else
    {
      if (svs.stats.cpu > CPU_USAGE_WARNING)
      {
        Com_Printf("^3WARNING: Server CPU has reached a critical usage of %i%%\n", (qint)svs.stats.cpu);
      }
    }

    if (sv_warningsframetime->integer > 0)
    {
      if (svs.stats.avg > sv_warningsframetime->integer)
      {
        Com_Printf("^3WARNING: Average frame time has reached a critical value of %ims\n", (qint)svs.stats.avg);
      }
    }
    else
    {
      if (svs.stats.avg > FRAME_TIME_WARNING)
      {
        Com_Printf("^3WARNING: Average frame time has reached a critical value of %ims\n", (qint)svs.stats.avg);
      }
    }
  }

  //Chey
  updatequeue = 0;

  if (queueCount)
  {
    updatequeue++;

    //four seconds
    if (updatequeue >= 80)
    {
      SV_UpdateQueue();
      updatequeue = 0;
    }
  }
}

/*
====================
SV_RateMsec

Return the number of msec until another message can be sent to
a client based on its rate settings
====================
*/
#define UDPIP_HEADER_SIZE 28
#define UDPIP6_HEADER_SIZE 48

const qint
SV_RateMsec(const client_t *client)
{
  qint rate;
  qint rateMsec;
  qint messageSize;

  if (!client->rate)
  {
    return 0;
  }

  messageSize = client->netchan.lastSentSize;

#if defined(USE_IPV6)
  if (client->netchan.remoteAddress.type == NA_IP6)
  {
    messageSize += UDPIP6_HEADER_SIZE;
  }
  else
#endif
    messageSize += UDPIP_HEADER_SIZE;

  rateMsec = messageSize * 1000 / ((qint)(client->rate * com_timescale->value));
  rate = Sys_Milliseconds() - client->netchan.lastSentTime;

  if (rate > rateMsec)
  {
    return 0;
  }
  else
  {
    return rateMsec - rate;
  }
}

/*
====================
SV_SendQueuedPackets

Send download messages and queued packets in the time that we're idle, i.e.
not computing a server frame or sending client snapshots.
Return the time in msec until we expect to be called next
====================
*/
const qint
SV_SendQueuedPackets(void)
{
#if defined(UDP_DOWNLOAD_OPTIMIZE)
  qint delayT;
  qint timeVal = INT_MAX;
#else
  qint numBlocks;
  qint dlStart;
  qint deltaT;
  qint delayT;
  static unsigned dlNextRound = 0;
  qint timeVal = INT_MAX;
#endif

  //send out fragmented packets since idle
  delayT = SV_SendQueuedMessages();

  if (delayT >= 0)
  {
    timeVal = delayT;
  }
#if defined(UDP_DOWNLOAD_OPTIMIZE)
  delayT = SV_SendDownloadMessages();

  if (delayT >= 0 && delayT < timeVal)
  {
    timeVal = delayT;
  }
#else
  if (sv_dlRate->integer)
  {
    //rate limiting, imprecise for high dl rates
    dlStart = Sys_Milliseconds();
    deltaT = dlNextRound - dlStart;

    if (deltaT > 0)
    {
      if (deltaT < timeVal)
      {
        timeVal = deltaT + 1;
      }
    }
    else
    {
      numBlocks = SV_SendDownloadMessages();

      if (numBlocks)
      {
        //active dls
        deltaT = Sys_Milliseconds() - dlStart;
        delayT = 1000 * numBlocks * MAX_DOWNLOAD_BLKSIZE;
        delayT /= sv_dlRate->integer * 1024;

        if (delayT <= deltaT + 1)
        {
          //1ms delay
          if (timeVal > 2)
          {
            timeVal = 2;
          }

          dlNextRound = dlStart + deltaT + 1;
        }
        else
        {
          dlNextRound = dlStart + delayT;
          delayT -= deltaT;

          if (delayT < timeVal)
          {
            timeVal = delayT;
          }
        }
      }
    }
  }
  else
  {
    if (SV_SendDownloadMessages())
    {
      timeVal = 0;
    }
  }
#endif
  return timeVal;
}

//============================================================================

