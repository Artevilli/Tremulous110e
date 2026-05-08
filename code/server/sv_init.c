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

#include "server.h"

/*
===============
SV_SendConfigstring

Creates and sends the server command necessary to update the CS index for the
given client
===============
*/
static void
SV_SendConfigstring(client_t *client, qint index)
{
  qint maxChunkSize = MAX_STRING_CHARS - 24;
  qint len;

  len = strlen(sv.configstrings[index]);

  if (len >= maxChunkSize)
  {
    qint sent = 0;
    qint remaining = len;
    const qchar *cmd;
    qchar buf[MAX_STRING_CHARS];

    while(remaining > 0)
    {
      if (sent == 0)
      {
        cmd = "bcs0";
      }
      else if (remaining < maxChunkSize)
      {
        cmd = "bcs2";
      }
      else
      {
        cmd = "bcs1";
      }

      Q_strncpyz(buf, &sv.configstrings[index][sent], maxChunkSize);
      SV_SendServerCommand(client, "%s %i \"%s\"", cmd, index, buf);
      sent += (maxChunkSize - 1);
      remaining -= (maxChunkSize - 1);
    }
  }
  else
  {
    //standard cs, just send it
    SV_SendServerCommand(client, "cs %i \"%s\"", index, sv.configstrings[index]);
  }
}

/*
===============
SV_UpdateConfigstrings

Called when a client goes from CS_PRIMED to CS_ACTIVE.  Updates all
Configstring indexes that have changed while the client was in CS_PRIMED
===============
*/
void
SV_UpdateConfigstrings(client_t *client)
{
  qint index;

  for(index = 0;index < MAX_CONFIGSTRINGS;index++)
  {
    //ignore if cs not changed since primed
    if (!client->csUpdated[index])
    {
      continue;
    }

    //do not always send server info to all clients
    if (index == CS_SERVERINFO && (SV_GentityNum(ARRAY_INDEX(svs.clients, client))->r.svFlags & SVF_NOSERVERINFO))
    {
      continue;
    }

    if (client->gentity && (client->gentity->r.svFlags & SVF_BOT))
    {
      continue; //Chey: bots dont need config string updates
    }

    SV_SendConfigstring(client, index);
    client->csUpdated[index] = qfalse;
  }
}

/*
===============
SV_SetConfigstring
===============
*/
void
SV_SetConfigstring(const qint index, const qchar *val)
{
  qint i;
  client_t *client;

  //Chey: FIXME: what the fuck?
  if (index > MAX_CONFIGSTRINGS - 1)
  {
    return;
  }

  if (index < 0 || index >= MAX_CONFIGSTRINGS)
  {
    Com_Error(ERR_DROP, "SV_SetConfigstring: bad index %i", index);
  }

  if (!val)
  {
    val = "";
  }

  //dont bother broadcasting an update if no change
  if (!strcmp(val, sv.configstrings[index]))
  {
    return;
  }

  //change the string in sv
  Z_Free(sv.configstrings[index]);
  sv.configstrings[index] = CopyString(val);

  //send it to all the clients if we aren't
  //spawning a new server
  if (sv.state == SS_GAME || sv.restarting)
  {
    //send the data to all relevent clients
    for(i = 0;i < sv.maxclients;i++)
    {
      client = &svs.clients[i];

      if (client->state < CS_ACTIVE)
      {
#if defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
        if (client->state == CS_PRIMED)
#else
        if (client->state == CS_PRIMED || client->state == CS_CONNECTED) //track CS_CONNECTED clients as well to optimize gamestate acknowledge after downloading/retransmission
#endif
        {
          client->csUpdated[index] = qtrue;
	}

        continue;
      }

      //do not always send server info to all clients
      if (index == CS_SERVERINFO && (SV_GentityNum(i)->r.svFlags & SVF_NOSERVERINFO))
      {
        continue;
      }

      SV_SendConfigstring(client, index);
    }
  }
}

/*
===============
SV_GetConfigstring
===============
*/
void
SV_GetConfigstring(const qint index, qchar *buffer, const qint bufferSize)
{
  if (bufferSize < 1)
  {
    Com_Error(ERR_DROP, "SV_GetConfigstring: bufferSize == %i", bufferSize);
  }

  if (index < 0 || index >= MAX_CONFIGSTRINGS)
  {
    Com_Error(ERR_DROP, "SV_GetConfigstring: bad index %i", index);
  }

  if (!sv.configstrings[index])
  {
    buffer[0] = '\0';
    return;
  }

  Q_strncpyz(buffer, sv.configstrings[index], bufferSize);
}


/*
===============
SV_SetUserinfo
===============
*/
void
SV_SetUserinfo(const qint index, const qchar *val)
{
  if (index < 0 || index >= sv.maxclients)
  {
    Com_Error(ERR_DROP, "%s: bad index %i", __func__, index);
  }

  if (!val)
  {
    val = "";
  }

  Q_strncpyz(svs.clients[index].userinfo, val, sizeof(svs.clients[index].userinfo));
  Q_strncpyz(svs.clients[index].name, Info_ValueForKey(val, "name"), sizeof(svs.clients[index].name));
}



/*
===============
SV_GetUserinfo
===============
*/
void
SV_GetUserinfo(const qint index, qchar *buffer, const qint bufferSize)
{
  if (bufferSize < 1)
  {
    Com_Error(ERR_DROP, "%s: bufferSize == %i", __func__, bufferSize);
  }

  if (index < 0 || index >= sv.maxclients)
  {
    Com_Error(ERR_DROP, "%s: bad index %i", __func__, index);
  }

  Q_strncpyz(buffer, svs.clients[index].userinfo, bufferSize);
}


/*
================
SV_CreateBaseline

Entity baselines are used to compress non-delta messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
static void
SV_CreateBaseline(void)
{
  sharedEntity_t *ent;
  qint entnum;	

  for (entnum = 0;entnum < sv.num_entities;entnum++)
  {
    ent = SV_GentityNum(entnum);

    if (!ent->r.linked)
    {
      continue;
    }

    ent->s.number = entnum;

    //
    //take current state as baseline
    //
    sv.svEntities[entnum].baseline = ent->s;
    sv.baselineUsed[entnum] = 1;
  }
}


/*
===============
SV_BoundMaxClients

===============
*/
static const qint
SV_BoundMaxClients(const qint minimum)
{
  //get the current maxclients value
  Cvar_Get("sv_maxclients", "8", 0);

  if (sv_maxclients->integer < minimum)
  {
    Cvar_SetIntegerValue("sv_maxclients", minimum);
    sv_maxclients->modified = qfalse;
    return minimum;
  }

  sv_maxclients->modified = qfalse;
  return sv_maxclients->integer;
}

/*
===============
SV_SetSnapshotParams
===============
*/
static void
SV_SetSnapshotParams(void)
{
  //PACKET_BACKUP frames is just about 6.67MB so use that even on listen servers
  svs.numSnapshotEntities = PACKET_BACKUP * MAX_GENTITIES;
}

/*
===============
SV_AllocClients
===============
*/
static void
SV_AllocClients(const qint count)
{
  svs.clients = Z_TagMalloc(count * sizeof(client_t), TAG_CLIENTS);
  Com_Memset(svs.clients, 0x0, count * sizeof(client_t));
  sv.maxclients = count;
  SV_SetSnapshotParams();
}

/*
===============
SV_Startup

Called when a host starts a map when it wasn't running
one before.  Successive map or map_restart commands will
NOT cause this to be called, unless the game is exited to
the menu system first.
===============
*/
static void
SV_Startup(void)
{
  if (svs.initialized)
  {
    Com_Error(ERR_FATAL, "SV_Startup: svs.initialized");
  }

  SV_AllocClients(sv_maxclients->integer);
#if defined(STATELESS_CHALLENGES_VERSION_ONE)
  SV_ChallengeInit();
#endif
  sv_maxclients->modified = qfalse;
  svs.initialized = qtrue;

  //dont respect killserver unless a server is actually running
  if (sv_killserver->integer)
  {
    Cvar_Set("sv_killserver", "0");
  }

  Cvar_Set("sv_running", "1");

  //join ipv6 multicast group since a map is running so clients can scan for us on the local network
#if defined(USE_IPV6)
  NET_JoinMulticast6();
#endif
}


/*
==================
SV_ChangeMaxClients
==================
*/
static void
SV_ChangeMaxClients(void)
{
  client_t *oldClients;
  qint maxclients;
  qint count;
  qint i;

  //get the highest client number in use
  count = 0;

  for(i = 0;i < sv.maxclients;i++)
  {
    if (svs.clients[i].state >= CS_CONNECTED)
    {
      if (i > count)
      {
        count = i;
      }
    }
  }

  count++;

  //never go below the highest client number in use
  maxclients = SV_BoundMaxClients(count);

  //if still the same
  if (maxclients == sv.maxclients)
  {
    return;
  }

  oldClients = Hunk_AllocateTempMemory(count * sizeof(client_t));

  //copy the clients to hunk memory
  for(i = 0;i < count;i++)
  {
    if (svs.clients[i].state >= CS_CONNECTED)
    {
      oldClients[i] = svs.clients[i];
    }
    else
    {
      Com_Memset(&oldClients[i], 0, sizeof(client_t));
    }
  }

  //free old clients arrays
  Z_Free(svs.clients);

  //allocate new clients
  SV_AllocClients(maxclients);

  //copy the clients over
  for(i = 0;i < count;i++)
  {
    if (oldClients[i].state >= CS_CONNECTED)
    {
      svs.clients[i] = oldClients[i];
    }
  }

  //free the old clients on the hunk
  Hunk_FreeTempMemory(oldClients);
}

/*
================
SV_ClearServer
================
*/
static void
SV_ClearServer(void)
{
  qint i;

  for(i = 0;i < MAX_CONFIGSTRINGS;i++)
  {
    if (sv.configstrings[i])
    {
      Z_Free(sv.configstrings[i]);
    }
  }

  if (!sv_levelTimeReset->integer)
  {
    i = sv.time;
    Com_Memset(&sv, 0, sizeof(sv));
    sv.time = i;
  }
  else
  {
    Com_Memset(&sv, 0, sizeof(sv));
  }
}

/*
================
SV_TouchCGame

  touch the cgame.vm so that a pure client can load it if it's in a seperate pk3
================
*/
#if 0 //Chey: FIXME: this is the ideal, however it is in early stages so this version remains unused
static void
SV_TouchCGame(void)
{
#ifdef USE_LLVM
  fileHandle_t f;

  //LLVM - even if the server doesn't use llvm itself, it should still add the references.
  FS_FOpenFileRead("cgamellvm.bc", &f, qfalse);

  if (f)
  {
    FS_FCloseFile(f);
  }
#endif
}

static void
SV_TouchCGame(const qchar *filename)
{
  fileHandle_t f;

  FS_FOpenFileRead(filename, &f, qfalse);
  if (f)
  {
    FS_FCloseFile(f);
  }
}
#endif

/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.
This is NOT called for map_restart
================
*/
void
SV_SpawnServer(const qchar *mapname, qbool killBots)
{
  qint i;
  qint checksum;
  const qchar *p;
  const qchar *denied;
  qbool isBot;

  //shut down the existing game if it is running
  SV_ShutdownGameProgs();

  Com_Printf("------ Server Initialization ------\n");
  Com_Printf("Server: %s\n", mapname);

  Sys_SetStatus("Initializing server...");

#if !defined(DEDICATED)
  //if not running a dedicated server CL_MapLoading will connect the client to the server
  //also print some status stuff
  CL_MapLoading();

  //make sure all the client stuff is unloaded
  CL_ShutdownAll();
#endif
  //clear the whole hunk because we're (re)loading the server
  Hunk_Clear();

  //clear collision map data
  CM_ClearMap();

  //timescale can be updated before SV_Frame() and cause division-by-zero in SV_RateMsec()
  Cvar_CheckRange(com_timescale, "0.001", NULL, CV_FLOAT);

  //restart renderer?
  //CL_StartHunkUsers();

  //init client structures and svs.numSnapshotEntities 
  if (!Cvar_VariableValue("sv_running"))
  {
    SV_Startup();
  }
  else
  {
    //check for maxclients change
    if (sv_maxclients->modified)
    {
      SV_ChangeMaxClients();
    }
  }

#if !defined(DEDICATED)
  //remove pure paks that may left from client-side
  FS_PureServerSetLoadedPaks("", "");
  FS_PureServerSetReferencedPaks("", "");
#endif

  //clear pak references
  FS_ClearPakReferences(0);

  //allocate the snapshot entities on the hunk
  svs.snapshotEntities = Hunk_Alloc(sizeof(entityState_t) * svs.numSnapshotEntities, h_high);
  //Com_Memset(svs.snapshotEntities, 0, sizeof(entityState_t) * svs.numSnapshotEntities);

  //initialize snapshot storage
  SV_InitSnapshotStorage();

  //toggle the server bit so clients can detect that a
  //server has changed
  svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

  //try to reset level time if server is empty
  if (!sv_levelTimeReset->integer && !sv.restartTime)
  {
    for(i = 0;i < sv.maxclients;i++)
    {
      if (svs.clients[i].state >= CS_CONNECTED)
      {
        break;
      }
    }

    if (i == sv.maxclients)
    {
      sv.time = 0;
    }
  }

  for(i = 0;i < sv.maxclients;i++)
  {
    //save when the server started for each client already connected
    if (svs.clients[i].state >= CS_CONNECTED && sv_levelTimeReset->integer)
    {
      svs.clients[i].oldServerTime = sv.time;
    }
    else
    {
      svs.clients[i].oldServerTime = 0;
    }
  }

  //preserve maxclients
  i = sv.maxclients;

  //wipe the entire per-level structure
  SV_ClearServer();

  sv.maxclients = i;

  for(i = 0;i < MAX_CONFIGSTRINGS;i++)
  {
    sv.configstrings[i] = CopyString("");
  }

#if !defined(DEDICATED)
  //make sure we are not paused
  Cvar_Set("cl_paused", "0");
#endif

  //get latched value
  sv_pure = Cvar_Get("sv_pure", "1", CVAR_SYSTEMINFO | CVAR_LATCH);

  //vms can change latched cvars instantly which could cause side effects in sv usermove
  sv.pure = sv_pure->integer;

  //get a new checksum feed and restart the file system
  srand(Com_Milliseconds());
  Com_RandomBytes((byte *)&sv.checksumFeed, sizeof(sv.checksumFeed));
  FS_Restart(sv.checksumFeed);

  Sys_SetStatus("Loading map %s", mapname);
  CM_LoadMap(va("maps/%s.bsp", mapname), qfalse, &checksum);

  //set serverinfo visible name
  Cvar_Set("mapname", mapname);

  Cvar_SetIntegerValue("sv_mapChecksum", checksum);

  //serverid should be different each time
  sv.serverId = com_frameTime;
  sv.restartedServerId = sv.serverId;
  Cvar_SetIntegerValue("sv_serverid", sv.serverId);

  //clear physics interaction links
  SV_ClearWorld();
	
  //media configstring setting should be done during
  //the loading stage, so connected clients don't have
  //to load during actual gameplay
  sv.state = SS_LOADING;

  //load and spawn all other entities
  SV_InitGameProgs();

  sv_pure->modified = qfalse;

  //run a few frames to allow everything to settle
  for(i = 0;i < 3; i++)
  {
    Cbuf_Wait();
    sv.time += 100;
#if defined(USE_JAVA)
    Java_G_RunFrame(sv.time);
#else
    VM_Call(sv.gvm, 1, GAME_RUN_FRAME, sv.time);
#endif
    SV_BotFrame(sv.time);
  }

  //create a baseline for more efficient communications
  SV_CreateBaseline();

  for(i = 0;i < sv.maxclients;i++)
  {
    //send the new gamestate to all connected clients
    if (svs.clients[i].state >= CS_CONNECTED)
    {
      if (svs.clients[i].netchan.remoteAddress.type == NA_BOT) // || svs.clients[i].gentity->r.svFlags & SVF_BOT) //Chey: FIXME: what is this?
      {
        if (killBots)
        {
          SV_DropClient(&svs.clients[i], "was kicked");
          continue;
        }

        isBot = qtrue;
      }
      else
      {
        isBot = qfalse;
      }

      //connect the client again
#if defined(USE_JAVA)
      denied = Java_G_ClientConnect(i, qfalse, isBot);
#else
      denied = GVM_ArgPtr(VM_Call(sv.gvm, 3, GAME_CLIENT_CONNECT, i, isBot)); //firstTime = qfalse
#endif
      if (denied)
      {
        //this generally shouldn't happen, because the client
        //was connected before the level change
        SV_DropClient(&svs.clients[i], denied);
      }
      else
      {
        if (!isBot)
        {
#if defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
          svs.clients[i].downloadGamestateDropCheck = qfalse;
#else
          svs.clients[i].gamestateAck = GSA_INIT; //resend gamestate, accept first correct serverId
#endif
          //when we get the next packet from a connected client,
          //the new gamestate will be sent
          svs.clients[i].state = CS_CONNECTED;
          svs.clients[i].gentity = NULL;
        }
        else
        {
          SV_ClientEnterWorld(&svs.clients[i]);
        }
      }
    }
  }	

  //run another frame to allow things to look at all the players
  Cbuf_Wait();
  sv.time += 100;
#if defined(USE_JAVA)
  Java_G_RunFrame(sv.time);
#else
  VM_Call(sv.gvm, 1, GAME_RUN_FRAME, sv.time);
#endif
  SV_BotFrame(sv.time);
  svs.time += 100;

  //we need to touch cgame and ui qvm because they could be in
  //separate pk3 files and the client will need to download the pk3
  //files with the latest cgame and ui qvm to pass the pure check
  FS_TouchFileInPak("vm/cgame.qvm");
  FS_TouchFileInPak("vm/ui.qvm");

  //the server sends these to the clients so they can figure
  //out which pk3s should be auto-downloaded
  p = FS_ReferencedPakNames();

  if (FS_ExcludeReference())
  {
    //\fs_excludeReference may mask our current ui/cgame qvms
    FS_TouchFileInPak("vm/cgame.qvm");
    FS_TouchFileInPak("vm/ui.qvm");

    //rebuild referenced paks list
    p = FS_ReferencedPakNames();
  }

  Cvar_Set("sv_referencedPakNames", p);

  p = FS_ReferencedPakChecksums();
  Cvar_Set("sv_referencedPaks", p);

  Cvar_Set("sv_paks", "");
  Cvar_Set("sv_pakNames", ""); //not used on client-side

  if (sv.pure)
  {
    qint pakslen;
    qint infolen;
    qint freespace;
    qbool overflowed = qfalse;
    qbool infoTruncated = qfalse;

    p = FS_LoadedPakChecksums(&overflowed);

    pakslen = strlen(p) + 9; //+ strlen("\\sv_paks\\")
    freespace = SV_RemainingGameState();
    infolen = strlen(Cvar_InfoString_Big(CVAR_SYSTEMINFO, &infoTruncated));

    if (infoTruncated)
    {
      Com_Printf(S_COLOR_YELLOW "WARNING: truncated systeminfo!\n");
    }

    if (pakslen > freespace || infolen + pakslen >= BIG_INFO_STRING || overflowed)
    {
      //switch to degraded pure mode
      //this could *potentially* lead to a false "unpure client" detection
      //which is better than guaranteed drop
      Com_DPrintf(S_COLOR_YELLOW "WARNING: skipping sv_paks setup to avoid gamestate overflow\n");
    }
    else
    {
      //the server sends these to the clients so they will only
      //load pk3s also loaded at the server
      Cvar_Set("sv_paks", p);

      if (*p == '\0')
      {
        Com_Printf(S_COLOR_YELLOW "WARNING: sv_pure set but no PK3 files loaded\n");
      }
    }
  }

  //save systeminfo and serverinfo strings
  SV_SetConfigstring(CS_SYSTEMINFO, Cvar_InfoString_Big(CVAR_SYSTEMINFO, NULL));
  cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;

  SV_SetConfigstring(CS_SERVERINFO, Cvar_InfoString(CVAR_SERVERINFO, NULL));
  cvar_modifiedFlags &= ~CVAR_SERVERINFO;

  //any media configstring setting now should issue a warning
  //and any configstring changes should be reliably transmitted
  //to all clients
  sv.state = SS_GAME;

  //send a heartbeat now so the master will get up to date info
  SV_Heartbeat_f();

  Hunk_SetMark();

  Com_Printf("-----------------------------------\n");

  Sys_SetStatus("Running map %s", mapname);

  //suppress hitch warning
  Com_FrameInit();
}

/*
===============
SV_Init

Only called at main exe startup, not for each game
===============
*/
void
SV_Init(void)
{
  SV_UptimeReset();

  SV_AddOperatorCommands();

  if (com_dedicated->integer)
  {
    SV_AddDedicatedCommands();
  }

  //initialize cvars
  SV_InitCvars();

  //track group cvar changes
  Cvar_SetGroup(sv_lanForceRate, CVG_SERVER);
  Cvar_SetGroup(sv_minRate, CVG_SERVER);
  Cvar_SetGroup(sv_maxRate, CVG_SERVER);
  Cvar_SetGroup(sv_fps, CVG_SERVER);

  //force initial check
  SV_TrackCvarChanges();

#if !defined(STATELESS_CHALLENGES_VERSION_ONE)
  SV_InitChallenger();
#endif
}


/*
==================
SV_FinalMessage

Used by SV_Shutdown to send a final message to all
connected clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
static void
SV_FinalMessage(const qchar *message)
{
  qint i;
  qint j;
  client_t *cl;

  //send twice, ignoring rate
  for(j = 0;j < 2;j++)
  {
    for(i = 0;i < sv.maxclients;i++)
    {
      cl = &svs.clients[i];

      if (cl->state >= CS_CONNECTED)
      {
        //dont send a disconnect to a local client
        if (cl->netchan.remoteAddress.type != NA_LOOPBACK)
        {
          SV_SendServerCommand(cl, "print \"%s\n\"\n", message);
          SV_SendServerCommand(cl, "disconnect \"%s\"", message);
        }

        if (sv.gameClients != NULL)
        {
          //force a snapshot to be sent
          cl->lastSnapshotTime = svs.time - 9999; //generate a snapshot immediately
          cl->state = CS_ZOMBIE; //skip delta generation
          SV_SendClientSnapshot(cl);
        }
      }
    }
  }

  NET_FlushPacketQueue(99999);
}


/*
================
SV_Shutdown

Called when each game quits,
before Sys_Quit or Sys_Error
================
*/
void
SV_Shutdown(const qchar *finalmsg)
{
  qint index;

  if (!com_sv_running || !com_sv_running->integer)
  {
    return;
  }

  Com_Printf("----- Server Shutdown (%s) -----\n", finalmsg);

#if defined(USE_IPV6)
  NET_LeaveMulticast6();
#endif

  if (svs.clients && !com_errorEntered)
  {
    SV_FinalMessage(finalmsg);
  }

  //Chey: XXX: should this go here?
  NET_Shutdown();

  SV_RemoveOperatorCommands();
  SV_MasterShutdown();
#if defined(STATELESS_CHALLENGES_VERSION_ONE)
  SV_ChallengeShutdown();
#endif
  SV_ShutdownGameProgs();
#if !defined(STATELESS_CHALLENGES_VERSION_ONE)
  SV_InitChallenger();
#endif

  //free current level
  SV_ClearServer();

  SV_FreeIP4DB();

  // free server static data
  if (svs.clients)
  {
    for(index = 0;index < sv.maxclients;index++)
    {
      SV_FreeClient(&svs.clients[index]);
    }

    Z_Free(svs.clients);
  }

  Com_Memset(&svs, 0, sizeof(svs));
  sv.time = 0;

  Cvar_Set("sv_running", "0");

  //allow setting timescale 0 for demo playback
  Cvar_CheckRange(com_timescale, "0", NULL, CV_FLOAT);

#if !defined(DEDICATED)
  Cvar_Set("ui_singlePlayerActive", "0");
#endif

  Com_Printf("---------------------------\n");

#if !defined(DEDICATED)
  // disconnect any local clients
  if (sv_killserver->integer != 2)
  {
    CL_Disconnect(qfalse);
  }
#endif

  //clean some server cvars
  Cvar_Set("sv_referencedPaks", "");
  Cvar_Set("sv_referencedPakNames", "");
  Cvar_Set("sv_mapChecksum", "");
  Cvar_Set("sv_serverid", "0");

  Sys_SetStatus("Server is not running");
}
