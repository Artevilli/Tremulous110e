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
#include <time.h>

static time_t uptimeSince;

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/


/*
==================
SV_GetPlayerByHandle

Returns the player with player id or name from Cmd_Argv(1)
==================
*/
client_t *
SV_GetPlayerByHandle(void)
{
  client_t *cl;
  qint i;
  const qchar *s;
  qchar cleanName[MAX_NAME_LENGTH];

  //make sure server is running
  if (!com_sv_running->integer)
  {
    return NULL;
  }

  if (Cmd_Argc() < 2)
  {
    Com_Printf("No player specified.\n");
    return NULL;
  }

  s = Cmd_Argv(1);

  //check whether this is a numeric player handle
  for(i = 0;s[i] >= '0' && s[i] <= '9';i++);
	
  if (!s[i])
  {
    const qint plid = Q_atoi(s);

    //check for numeric playerid match
    if (plid >= 0 && plid < sv.maxclients)
    {
      cl = &svs.clients[plid];
			
      if (cl->state >= CS_CONNECTED)
      {
        return cl;
      }
    }
  }

  //check for a name match
  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (cl->state < CS_CONNECTED)
    {
      continue;
    }

    if (!Q_stricmp(cl->name, s))
    {
      return cl;
    }

    Q_strncpyz(cleanName, cl->name, sizeof(cleanName));
    Q_CleanStr(cleanName);

    if (!Q_stricmp(cleanName, s))
    {
      return cl;
    }
  }

  Com_Printf("Player %s is not on the server\n", s);
  return NULL;
}

/*
==================
SV_GetPlayerByNum

Returns the player with idnum from Cmd_Argv(1)
==================
*/
static client_t *
SV_GetPlayerByNum(void)
{
  client_t *cl;
  qint i;
  qint idnum;
  const qchar *s;

  //make sure server is running
  if (!com_sv_running->integer)
  {
    return NULL;
  }

  if (Cmd_Argc() < 2)
  {
    Com_Printf("No player specified.\n");
    return NULL;
  }

  s = Cmd_Argv(1);

  for(i = 0;s[i];i++)
  {
    if (s[i] < '0' || s[i] > '9')
    {
      Com_Printf("Bad slot number: %s\n", s);
      return NULL;
    }
  }

  idnum = Q_atoi(s);

  if (idnum < 0 || idnum >= sv.maxclients)
  {
    Com_Printf("Bad client slot: %i\n", idnum);
    return NULL;
  }

  cl = &svs.clients[idnum];

  if (cl->state < CS_CONNECTED)
  {
    Com_Printf("Client %i is not active\n", idnum);
    return NULL;
  }

  return cl;
}

//=========================================================

/*
====================
SV_CompleteDemoName
====================
*/
static void
SV_CompleteDemoName(const qchar *args, qint argNum)
{
  qchar demoExt[16];

  if (argNum == 2)
  {
    Com_sprintf(demoExt, sizeof(demoExt), ".svdm_%d", PROTOCOL_VERSION);
    Field_CompleteFilename("svdemos", demoExt, qtrue, FS_MATCH_PK3s | FS_MATCH_STICK);
  }
}

/*
==================
SV_Map_f

Restart the server on a different map
==================
*/
static void
SV_Map_f(void)
{
  const qchar *cmd;
  const qchar *map;
  qbool killBots;
  qbool cheat;
  qchar expanded[MAX_QPATH];
  qchar mapname[MAX_QPATH];
  qint i;
  qint len;

  map = Cmd_Argv(1);

  if (!map || !*map)
  {
    return;
  }

  if (strchr(map, '\\') != NULL)
  {
    Com_Printf(S_COLOR_YELLOW "WARNING: You are not allowed to have '\\\\' in your bsp name.\n");
    return;
  }

  //make sure the level exists before trying to change, so that a typo at the server console won't end the game
  Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", map);

  //bypass pure check so we can open downloaded map
  FS_BypassPure();
  len = FS_FOpenFileRead(expanded, NULL, qfalse);
  FS_RestorePure();

  if (len == -1)
  {
    Com_Printf("Can't find map %s\n", expanded);
    return;
  }

  cmd = Cmd_Argv(0);

  if (!Q_stricmp(cmd, "devmap"))
  {
    cheat = qtrue;
    killBots = qtrue;
  }
  else
  {
    cheat = qfalse;
    killBots = qfalse;
  }

  //stop demos
  if (sv.demoState == DS_RECORDING)
  {
    SV_DemoStopRecord();
  }

  if (sv.demoState == DS_PLAYBACK)
  {
    SV_DemoStopPlayback();
  }
  
  //save the map name here cause on a map restart we reload the autogen.cfg and thus nuke the arguments of the map command
  Q_strncpyz(mapname, map, sizeof(mapname));

  //enforce lowercase names for consistency
  Q_strlwr(mapname);

  //start up the map
  SV_SpawnServer(mapname, killBots);
  
  //set the cheat value if the level was started with "map <levelname>", then cheats will not be allowed. If started with "devmap <levelname>" then cheats will be allowed
  if (cheat)
  {
    Cvar_Set("sv_cheats", "1");
  }
  else
  {
    Cvar_Set("sv_cheats", "0");
  }
  
  //This forces the local master server IP address cache to be updated on sending the next heartbeat
  for(i = 0;i < MAX_MASTER_SERVERS;i++)
  {
    sv_master[i]->modified = qtrue;
  }
}

/*
================
SV_MapRestart_f

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
static void
SV_MapRestart_f(void)
{
  qint i;
  client_t *client;
  const qchar *denied;
  qchar mapname[MAX_QPATH];
  qbool isBot;
  qtime_t now;
  qint delay;
  qbool isDownloading = qfalse;

  //make sure we aren't restarting twice in the same frame
  if (com_frameTime == sv.restartedServerId)
  {
    return;
  }

  //make sure server is running
  if (!com_sv_running->integer)
  {
    Com_Printf("Server is not running.\n");
    return;
  }

  if (sv.restartTime)
  {
    return;
  }

  if (Cmd_Argc() > 1)
  {
    delay = Q_atoi(Cmd_Argv(1));
  }
  else
  {
    delay = 5;
  }

  if (delay && !Cvar_VariableValue("g_doWarmup"))
  {
    sv.restartTime = sv.time + delay * 1000;

    if (!sv.restartTime)
    {
      sv.restartTime = 1;
    }

    SV_SetConfigstring(CS_WARMUP, va("%i", sv.restartTime));
    return;
  }

  for(i = 0;i < sv.maxclients;i++)
  {
    client = &svs.clients[i];

    if (*client->downloadName)
    {
      isDownloading = qtrue;
      break;
    }
  }

  //check for changes in variables that can't just be restarted
  //check for maxclients and democlients change
  if (sv_maxclients->modified || sv_democlients->modified || sv_pure->modified || isDownloading)
  {
    Com_Printf("variable change and/or client downloading -- restarting.\n");

    //restart the map the slow way
    Q_strncpyz(mapname, Cvar_VariableString("mapname"), sizeof(mapname));
    SV_SpawnServer(mapname, qfalse);
    return;
  }

  //stop demos
  if (sv.demoState == DS_RECORDING)
  {
    SV_DemoStopRecord();
  }

  if (sv.demoState == DS_PLAYBACK)
  {
    SV_DemoStopPlayback();
  }

  //toggle the server bit so clients can detect that a map_restart has happened
  svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

  //generate a new restartedserverid
  sv.restartedServerId = com_frameTime;

  //if a map_restart occurs while a client is changing maps, we need
  //to give them the correct time so that when they finish loading
  //they don't violate the backwards time check in cl_cgame.c
  for(i = 0;i < sv.maxclients;i++)
  {
    if (svs.clients[i].state == CS_PRIMED)
    {
      svs.clients[i].oldServerTime = sv.restartTime;
    }
  }

  //reset all the vm data in place without changing memory allocation note that we do NOT set sv.state = SS_LOADING, so configstrings that had been changed from their default values will generate broadcast updates
  sv.state = SS_LOADING;
  sv.restarting = qtrue;

  SV_RestartGameProgs();

  //run a few frames to allow everything to settle
  for(i = 0;i < 3;i++)
  {
    Cbuf_Wait();
    sv.time += 100;
#if defined(USE_JAVA)
    Java_G_RunFrame(sv.time);
#else
    VM_Call(sv.gvm, 1, GAME_RUN_FRAME, sv.time);
#endif
  }

  sv.state = SS_GAME;
  sv.restarting = qfalse;

  //connect and begin all the clients
  for(i = 0;i < sv.maxclients;i++)
  {
    client = &svs.clients[i];

    //send the new gamestate to all connected clients
    if (client->state < CS_CONNECTED)
    {
      continue;
    }

    if (client->netchan.remoteAddress.type == NA_BOT)
    {
      isBot = qtrue;
    }
    else
    {
      isBot = qfalse;
    }

    //add the map_restart command
    SV_AddServerCommand(client, "map_restart\n");

    //connect the client again, without the firstTime flag
#if defined(USE_JAVA)
    denied = Java_G_ClientConnect(i, qfalse, isBot);
#else
    denied = GVM_ArgPtr(VM_Call(sv.gvm, 3, GAME_CLIENT_CONNECT, i, isBot));
#endif
    if (denied)
    {
      //this generally shouldn't happen, because the client was connected before the level change
      SV_DropClient(client, denied);

      if (!isBot)
      {
        Com_Printf("SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, i);
      }

      continue;
    }

    if (client->state == CS_ACTIVE)
    {
      SV_ClientEnterWorld(client);
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
  svs.time += 100;

  for(i = 0;i < sv.maxclients;i++)
  {
    client = &svs.clients[i];

    if (client->state >= CS_PRIMED)
    {
      //accept usercmds starting from current server time only to emulate original behavior which dropped pre restart commands via serverid check
      Com_Memset(&client->lastUsercmd, 0x0, sizeof(client->lastUsercmd));
      client->lastUsercmd.serverTime = sv.time - 1;
    }
  }

  //lastRestartFrame = com_frameTime;
  
  //start recording demo
  if (sv_autoDemo->integer)
  {
    Com_RealTime(&now);
    Cbuf_AddText(va("demo_record %04d%02d%02d%02d%02d%02d-%s\n", 1900 + now.tm_year, 1 + now.tm_mon, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec, Cvar_VariableString("mapname")));
  }
}

//===============================================================

/*
==================
SV_KickAll_f

Kick all users off of the server
==================
*/
static void
SV_KickAll_f(void)
{
  client_t *cl;
  qint i;

  //make sure server is running
  if (!com_sv_running->integer)
  {
    Com_Printf("Server is not running.\n");
    return;
  }

  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (cl->state < CS_CONNECTED)
    {
      continue;
    }

    if (cl->netchan.remoteAddress.type == NA_LOOPBACK)
    {
      continue;
    }

    SV_DropClient(cl, "was kicked");
    cl->lastPacketTime = svs.time; //in case there is a funny zombie
  }
}

/*
==================
SV_Kick_f

Kick a user off of the server
==================
*/
static void
SV_Kick_f(void)
{
  client_t *cl;

  //make sure server is running
  if (!com_sv_running->integer)
  {
    Com_Printf("Server is not running.\n");
    return;
  }

  if (Cmd_Argc() != 2)
  {
    Com_Printf("Usage: kick <player name>\n");
    return;
  }

  cl = SV_GetPlayerByHandle();

  if (!cl)
  {
    return;
  }

  if (cl->netchan.remoteAddress.type == NA_LOOPBACK)
  {
    SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
    return;
  }

  SV_DropClient(cl, "was kicked");
  cl->lastPacketTime = svs.time; //in case there is a funny zombie
}

/*
==================
SV_KickNum_f

Kick a user off of the server
==================
*/
static void
SV_KickNum_f(void)
{
  client_t *cl;

  //make sure server is running
  if (!com_sv_running->integer)
  {
    Com_Printf( "Server is not running.\n" );
    return;
  }

  if (Cmd_Argc() != 2)
  {
    Com_Printf("Usage: kicknum <client number>\n");
    return;
  }

  cl = SV_GetPlayerByNum();

  if (!cl)
  {
    return;
  }

  if (cl->netchan.remoteAddress.type == NA_LOOPBACK)
  {
    SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
    return;
  }

  SV_DropClient(cl, "was kicked");
  cl->lastPacketTime = svs.time; //in case there is a funny zombie
}

/*
** SV_Strlen -- skips color escape codes
*/
qint
SV_Strlen(const qchar *str)
{
  const qchar *s = str;
  qint count = 0;

  while(*s)
  {
    if (Q_IsColorString(s))
    {
      s += 2;
    }
    else
    {
      count++;
      s++;
    }
  }

  return count;
}

/*
================
SV_Status_f
================
*/
static void
SV_Status_f(void)
{
  qint i;
  qint j;
  qint l;
  const client_t *cl;
  const playerState_t *ps;
  const qchar *s;
  qint max_namelength;
  qint max_addrlength;
  qchar names[MAX_CLIENTS * MAX_NAME_LENGTH];
  qchar *np[MAX_CLIENTS];
  qchar nl[MAX_CLIENTS];
  qchar *nc;
  qchar addrs[MAX_CLIENTS * 48];
  qchar *ap[MAX_CLIENTS];
  qchar al[MAX_CLIENTS];
  qchar *ac;

  //make sure server is running
  if (!com_sv_running->integer)
  {
    Com_Printf("Server is not running.\n");
    return;
  }

  max_namelength = 4; //strlen("name")
  max_addrlength = 7; //strlen("address")

  nc = names;
  *nc = '\0';

  ac = addrs;
  *ac = '\0';

  Com_Memset(np, 0, sizeof(np));
  Com_Memset(nl, 0, sizeof(nl));

  Com_Memset(ap, 0, sizeof(ap));
  Com_Memset(al, 0, sizeof(al));

  //first pass: save and determine max.lengths of name/address fields
  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (cl->state == CS_FREE)
    {
      continue;
    }

    l = strlen(cl->name) + 1;
    strcpy(nc, cl->name);

    //name pointer in name buffer
    np[i] = nc;
    nc += l;

    if (com_ansiColor->integer)
    {
      //name length without color sequences
      nl[i] = SV_Strlen(cl->name);
    }
    else
    {
      //name length with color sequences
      nl[i] = strlen(cl->name);
    }

    if (nl[i] > max_namelength)
    {
      max_namelength = nl[i];
    }

    s = NET_AdrToString(&cl->netchan.remoteAddress);
    l = strlen(s) + 1;
    strcpy(ac, s);

    //address pointer in address buffer
    ap[i] = ac;
    ac += l;

    //address length
    al[i] = l - 1;

    if (al[i] > max_addrlength)
    {
      max_addrlength = al[i];
    }
  }

  Com_Printf("cpu server utilization: %i%%\navg response time: %i ms\nserver load: %i\nmap: %s\n", (qint)svs.stats.cpu, (qint)svs.stats.avg, (qint)svs.serverLoad, sv_mapname->string);
#if 0
  Com_Printf("cl score ping name                        address                     rate\n");
  Com_Printf("-- ----- ---- --------------------------- --------------------------- -----\n");
#else //variable-length fields

  Com_Printf("cl score ping name");

  for(i = 0;i < max_namelength - 4;i++)
  {
    Com_Printf(" ");
  }

  Com_Printf(" address");

  for(i = 0;i < max_addrlength - 7;i++)
  {
    Com_Printf(" ");
  }

  Com_Printf(" rate\n");

  Com_Printf("-- ----- ---- ");

  for(i = 0;i < max_namelength;i++)
  {
    Com_Printf("-");
  }

  Com_Printf(" ");

  for(i = 0;i < max_addrlength;i++)
  {
    Com_Printf("-");
  }

  Com_Printf(" ----\n");
#endif

  for(i = 0;i < sv.maxclients;i++)
  {
    cl = &svs.clients[i];

    if (cl->state == CS_FREE)
    {
      continue;
    }

    Com_Printf("%2i ", i); //id
    ps = SV_GameClientNum(i);
#if defined(SUPPORT_STATUS_SCORES_OVERRIDE)
    Com_Printf("%5i ", SV_StatusScoresOverride_AdjustScore(ps->persistant[PERS_SCORE], i));
#else
    Com_Printf("%5i ", ps->persistant[PERS_SCORE]);
#endif

    if (cl->state == CS_PRIMED)
    {
      Com_Printf(" PRM ");
    }
    else if (cl->state == CS_CONNECTED)
    {
      Com_Printf(" CON ");
    }
    else if (cl->state == CS_ZOMBIE)
    {
      Com_Printf(" ZMB ");
    }
    else
    {
      Com_Printf("%4i ", cl->ping < 999 ? cl->ping:999);
    }

    //variable-length name field
    s = np[i];
    Com_Printf("%s", s);
    l = max_namelength - nl[i];

    for(j = 0;j < l;j++)
    {
      Com_Printf(" ");
    }

    //variable-length address field
    s = ap[i];

    if (com_ansiColor && com_ansiColor->integer)
    {
      Com_Printf(S_COLOR_WHITE " %s", s);
    }
    else
    {
      Com_Printf(" %s", s);
    }

    l = max_addrlength - al[i];

    for(j = 0;j < l;j++)
    {
      Com_Printf(" ");
    }

    //rate
    Com_Printf(" %5i\n", cl->rate);
  }

  Com_Printf("\n");
}

/*
==================
SV_ConSay_f
==================
*/
static void
SV_ConSay_f(void)
{
  qchar *p;
  qchar text[MAX_STRING_CHARS];
  qint len;

  //make sure server is running
  if (!com_sv_running->integer)
  {
    Com_Printf("Server is not running.\n");
    return;
  }

  if (Cmd_Argc () < 2)
  {
    return;
  }

  p = Cmd_ArgsFrom(1);
  len = (qint)strlen(p);

  if (len > 1000)
  {
    return;
  }

  if (*p == '"')
  {
    p[len-1] = '\0';
    p++;
  }

  strcpy(text, "console: " );
  strcat(text, p);

  SV_SendServerCommand(NULL, "chat \"%s\"", text);
}

/*
==================
SV_ConTell_f
==================
*/
static void
SV_ConTell_f(void)
{
  qchar *p;
  qchar text[MAX_STRING_CHARS];
  client_t *cl;
  qint len;

  //make sure server is running
  if (!com_sv_running->integer)
  {
    Com_Printf("Server is not running.\n");
    return;
  }

  if (Cmd_Argc() < 3)
  {
    Com_Printf("Usage: tell <client number> <text>\n");
    return;
  }

  cl = SV_GetPlayerByNum();

  if (!cl)
  {
    return;
  }

  p = Cmd_ArgsFrom(2);
  len = (qint)strlen(p);

  if (len > 1000)
  {
    return;
  }

  if (*p == '"')
  {
    p[len-1] = '\0';
    p++;
  }

  strcpy(text, S_COLOR_MAGENTA "console: ");
  strcat(text, p);

  Com_Printf("%s\n", text);
  SV_SendServerCommand(cl, "chat \"%s\"", text);
}


/*
==================
SV_Heartbeat_f

Also called by SV_DropClient, SV_DirectConnect, and SV_SpawnServer
==================
*/
void
SV_Heartbeat_f(void)
{
  svs.nextHeartbeatTime = svs.time;
}


/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void
SV_Serverinfo_f(void)
{
  const qchar *info;

  //make sure server is running
  if (!com_sv_running->integer)
  {
    Com_Printf("Server is not running.\n");
    return;
  }

  Com_Printf("Server info settings:\n");
  info = sv.configstrings[CS_SERVERINFO];

  if (info)
  {
    Info_Print(info);
  }
}


/*
===========
SV_Systeminfo_f

Examine the systeminfo string
===========
*/
static void
SV_Systeminfo_f(void)
{
  const qchar *info;

  //make sure server is running
  if (!com_sv_running->integer)
  {
    Com_Printf("Server is not running.\n");
    return;
  }

  Com_Printf("System info settings:\n");
  info = sv.configstrings[CS_SYSTEMINFO];

  if (info)
  {
    Info_Print(info);
  }
}


/*
===========
SV_DumpUser_f

Examine all a users info strings
===========
*/
static void
SV_DumpUser_f(void)
{
  client_t *cl;

  //make sure server is running
  if (!com_sv_running->integer)
  {
    Com_Printf("Server is not running.\n");
    return;
  }

  if (Cmd_Argc() != 2)
  {
    Com_Printf("Usage: info <userid>\n");
    return;
  }

  cl = SV_GetPlayerByHandle();

  if (!cl)
  {
    return;
  }

  Com_Printf("userinfo\n");
  Com_Printf("--------\n");
  Info_Print(cl->userinfo);
}


/*
=================
SV_KillServer
=================
*/
static void
SV_KillServer_f(void)
{
  SV_Shutdown("killserver");
}

/*
=================
SV_Locations
=================
*/
static void
SV_Locations_f(void)
{
  //make sure server is running
  if (!com_sv_running->integer)
  {
    Com_Printf("Server is not running.\n");
    return;
  }

  if (!sv_clientTLD->integer)
  {
    Com_Printf("Disabled on this server.\n");
    return;
  }

  SV_PrintLocations_f(NULL);
}

/*
=================
SV_Demo_Record_f
=================
*/
static void
SV_Demo_Record_f(void)
{
  qint number;

  //make sure server is running
  if (!com_sv_running->integer)
  {
    Com_Printf("server not running\n");
    return;
  }

  if (Cmd_Argc() > 2)
  {
    Com_Printf("use demo_record demoname\n");
    return;
  }

  if (sv.demoState != DS_NONE)
  {
    Com_Printf("demo is already being recorded or played\n");
    return;
  }

  if (sv.maxclients == MAX_CLIENTS)
  {
    Com_Printf("reduce sv_maxclients\n");
    return;
  }

  if (Cmd_Argc() == 2)
  {
    sprintf(sv.demoName, "svdemos/%s.svdm_%d", Cmd_Argv(1), PROTOCOL_VERSION);
  }
  else
  {
    //scan for free demo name
    for(number = 0;number >= 0;number++)
    {
      Com_sprintf(sv.demoName, sizeof(sv.demoName), "svdemos/%d.svdm_%d", number, PROTOCOL_VERSION);

      if (!FS_FileExists(sv.demoName))
      {
        break; //no file
      }
      if (number < 0)
      {
        Com_Printf("cant make filename for demo, delete old\n");
        return;
      }
    }
  }

  sv.demoFile = FS_FOpenFileWrite(sv.demoName);

  if (!sv.demoFile)
  {
    Com_Printf("could not open %s", sv.demoName);
    return;
  }

  SV_DemoStartRecord();
}

/*
=================
SV_Demo_Play_f
=================
*/
static void
SV_Demo_Play_f(void)
{
  qchar *arg;

  if (Cmd_Argc() != 2)
  {
    Com_Printf("use demo_play demoname\n");
    return;
  }

  if (sv.demoState != DS_NONE)
  {
    Com_Printf("demo is already being recorded or played\n");
    return;
  }

  if (sv_democlients->integer <= 0)
  {
    Com_Printf("set sv_democlients greater than 0\n");
    return;
  }

  //check for extension and protocol
  arg = Cmd_Argv(1);

  if (!strcmp(arg + strlen(arg) - 6, va(".svdm_%d", PROTOCOL_VERSION)))
  {
    Com_sprintf(sv.demoName, sizeof(sv.demoName), "svdemos/%s", arg);
  }
  else
  {
    Com_sprintf(sv.demoName, sizeof(sv.demoName), "svdemos/%s.svdm_%d", arg, PROTOCOL_VERSION);
  }

  FS_FOpenFileRead(sv.demoName, &sv.demoFile, qtrue);

  if (!sv.demoFile)
  {
    Com_Printf("cannot open file %s\n", sv.demoName);
    return;
  }

  SV_DemoStartPlayback();
}

/*
=================
SV_Demo_Stop_f
=================
*/
static void
SV_Demo_Stop_f(void)
{
  if (sv.demoState == DS_NONE)
  {
    Com_Printf("no active demo\n");
    return;
  }

  //close demo file
  if (sv.demoState == DS_PLAYBACK)
  {
    SV_DemoStopPlayback();
  }
  else
  {
    SV_DemoStopRecord();
  }
}

/*
==================
SV_ListQueue_f
==================
*/
static void
SV_ListQueue_f(void)
{
  qint i;

  for(i = 0;i < queueCount;i++)
  {
    if (!i)
    {
      Com_Printf("connection ips in queue\n_____________________\n");
    }

    Com_Printf("%d: %s : %d\n", i + 1, NET_AdrToString(&queue[i]), svs.time - lastQueue[i]);
  }
}

/*
==================
SV_CompleteMapName
==================
*/
static void
SV_CompleteMapName(const qchar *args, qint argNum)
{
  if (argNum == 2)
  {
    if (sv.pure)
    {
      Field_CompleteFilename("maps", "bsp", qtrue, FS_MATCH_PK3s | FS_MATCH_STICK);
    }
    else
    {
      Field_CompleteFilename("maps", "bsp", qtrue, FS_MATCH_ANY | FS_MATCH_STICK);
    }
  }
}

/*
==================
SV_UptimeReset
==================
*/
const void
SV_UptimeReset(void)
{
  uptimeSince = time(NULL);
}

/*
==================
SV_Uptime_f

Prints the server uptime.
==================
*/
static const void
SV_Uptime_f(void)
{
  const unsigned long uptime = (unsigned long)(difftime(time(NULL), uptimeSince));
  const unsigned s = uptime % 60;
  const unsigned m = (uptime % 3600) / 60;
  const unsigned h = (uptime % 86400) / 3600;
  const unsigned d = uptime / 86400;

  if (Cmd_Argc() == 2)
  {
    Com_Printf("uptime: %u days, %u hours, %u min, %u sec\nserver time: %i\ninternal time: %i\n", d, h, m, s, svs.time, Sys_Milliseconds());
  }
  else
  {
    Com_Printf("uptime: %u days, %u hours, %u min, %u sec\n", d, h, m, s);
  }
}

#if defined(INCLUDE_REMOTE_COMMANDS)
/*
==================
SV_RconFileRehash

Helper to reload a "serverBan_t" type of file
Returns number of entries loaded
==================
*/
static qint
SV_RconFileRehash(qint maxEntries, serverRconPassword_t buffer[])
{
  const qchar *fileName;
  qint index;
  qint filelen;
  fileHandle_t readfrom;
  qchar *textbuf;
  qchar *curpos;
  qchar *maskpos;
  qchar *newlinepos;
  qchar *endpos;
  qchar filepath[MAX_QPATH];
  qint numEntries;

  numEntries = 0;
  fileName = sv_rconWhitelist->string;

  if (!fileName || !*fileName)
  {
    goto exit;
  }

  if (!(curpos = Cvar_VariableString("fs_game")) || !*curpos)
  {
    curpos = BASEGAME;
  }

  Com_sprintf(filepath, sizeof(filepath), "%s/%s", curpos, fileName);

  if ((filelen = FS_FOpenFileRead(filepath, &readfrom, qfalse)) < 0)
  {
    Com_Printf("sv rehash server rcon file failed to open %s\n", filepath);
    goto exit;
  }

  if (filelen < 2)
  {
    //dont bother if too short
    FS_FCloseFile(readfrom);
    goto exit;
  }

  curpos = textbuf = Z_Malloc(filelen);
  filelen = FS_Read(textbuf, filelen, readfrom);
  FS_FCloseFile(readfrom);
  endpos = textbuf + filelen;

  for(index = 0;index < maxEntries && curpos + 2 < endpos;index++)
  {
    //find end of addr string
    for(maskpos = curpos + 2;maskpos < endpos && *maskpos != ' ';maskpos++);

    if (maskpos + 1 >= endpos)
    {
      break;
    }

    *maskpos = '\0';
    maskpos++;

    //find end of subnet specifier
    for(newlinepos = maskpos;newlinepos < endpos && *newlinepos != '\n';newlinepos++);

    if (newlinepos >= endpos)
    {
      break;
    }

    *newlinepos = '\0';

    if (NET_StringToAdr(curpos + 2, &buffer[index].ip, NA_UNSPEC))
    {
      buffer[index].isException = (curpos[0] != '0');
      buffer[index].subNet = Q_atoi(maskpos);

      if (buffer[index].ip.type == NA_IP && (buffer[index].subNet < 1 || buffer[index].subNet > 32))
      {
        buffer[index].subNet = 32;
      }
      else if (buffer[index].ip.type == NA_IP6 && (buffer[index].subNet < 1 || buffer[index].subNet > 128))
      {
        buffer[index].subNet = 128;
      }
    }

    curpos = newlinepos + 1;
  }

  Z_Free(textbuf);
  numEntries = index;
  exit:
  return numEntries;
}

/*
==================
SV_RconWhitelistRehash_f

Load rcon whitelist from file
==================
*/
static const void
SV_RconWhitelistRehash_f(void)
{
  rconWhitelistCount = SV_RconFileRehash(MAXIMUM_RCON_WHITELIST, rconWhitelist);
}
#endif

//===========================================================

/*
==================
SV_AddOperatorCommands
==================
*/
void
SV_AddOperatorCommands(void)
{
  static qbool initialized;

  if (initialized)
  {
    return;
  }

  initialized = qtrue;
  Cmd_AddCommand("heartbeat", SV_Heartbeat_f);
  Cmd_AddCommand("kick", SV_Kick_f);
  Cmd_AddCommand("kickAll", SV_KickAll_f);
  Cmd_AddCommand("clientkick", SV_KickNum_f);
  Cmd_AddCommand("status", SV_Status_f);
  Cmd_AddCommand("dumpuser", SV_DumpUser_f);
  Cmd_AddCommand("map_restart", SV_MapRestart_f);
  Cmd_AddCommand("sectorlist", SV_SectorList_f);
  Cmd_AddCommand("map", SV_Map_f);
  Cmd_SetCommandCompletionFunc("map", SV_CompleteMapName);
  Cmd_AddCommand("devmap", SV_Map_f);
  Cmd_SetCommandCompletionFunc("devmap", SV_CompleteMapName);
  Cmd_AddCommand("killserver", SV_KillServer_f);
  Cmd_AddCommand("uptime", SV_Uptime_f);
  Cmd_AddCommand("filter", SV_AddFilter_f);
  Cmd_AddCommand("filtercmd", SV_AddFilterCmd_f);
  Cmd_AddCommand("demo_record", SV_Demo_Record_f);
  Cmd_AddCommand("demo_play", SV_Demo_Play_f);
  Cmd_SetCommandCompletionFunc("demo_play", SV_CompleteDemoName);
  Cmd_AddCommand("demo_stop", SV_Demo_Stop_f);
  Cmd_AddCommand("listqueue", SV_ListQueue_f);
#if defined(INCLUDE_REMOTE_COMMANDS)
  Cmd_AddCommand("rconwhitelistrehash", SV_RconWhitelistRehash_f);
#endif
}

/*
==================
SV_AddOperatorCommands
==================
*/
void
SV_AddDedicatedCommands(void)
{
  Cmd_AddCommand("serverinfo", SV_Serverinfo_f);
  Cmd_AddCommand("systeminfo", SV_Systeminfo_f);
  Cmd_AddCommand("tell", SV_ConTell_f);
  Cmd_AddCommand("say", SV_ConSay_f);
  Cmd_AddCommand("locations", SV_Locations_f);
}

/*
==================
SV_RemoveOperatorCommands
==================
*/
void
SV_RemoveOperatorCommands(void)
{
#if 0
  //removing these won't let the server start again
  Cmd_RemoveCommand("heartbeat");
  Cmd_RemoveCommand("kick");
  Cmd_RemoveCommand("banUser");
  Cmd_RemoveCommand("banClient");
  Cmd_RemoveCommand("status");
  Cmd_RemoveCommand("serverinfo");
  Cmd_RemoveCommand("systeminfo");
  Cmd_RemoveCommand("dumpuser");
  Cmd_RemoveCommand("map_restart");
  Cmd_RemoveCommand("sectorlist");
  Cmd_RemoveCommand("say");
#endif
}

void
SV_RemoveDedicatedCommands(void)
{
  Cmd_RemoveCommand("serverinfo");
  Cmd_RemoveCommand("systeminfo");
  Cmd_RemoveCommand("tell");
  Cmd_RemoveCommand("say");
  Cmd_RemoveCommand("locations");
}
