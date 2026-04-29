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
// sv_game.c -- interface to the game dll

#include "server.h"

#if !defined(USE_JAVA)

static void
SV_GameError(const qchar *string)
{
  Com_Error(ERR_DROP, "%s", string);
}

static void
SV_GamePrint(const qchar *string)
{
  Com_Printf("%s", string);
}

//these functions must be used instead of pointer arithmetic, because
//the game allocates gentities with private information after the server shared part
qint
SV_NumForGentity(sharedEntity_t *ent)
{
  qint num;

  num = ARRAY_INDEX((byte *)sv.gentities, (byte *)ent) / sv.gentitySize;
  return num;
}

sharedEntity_t *
SV_GentityNum(qint num)
{
  sharedEntity_t *ent;

  if (num < 0 || num >= MAX_GENTITIES)
  {
    Com_Error(ERR_DROP, "%s: bad num %d", __func__, num);
  }

  ent = (sharedEntity_t *)((byte *)sv.gentities + sv.gentitySize*(num));
  return ent;
}

playerState_t *
SV_GameClientNum(qint num)
{
  playerState_t *ps;

  ps = (playerState_t *)((byte *)sv.gameClients + sv.gameClientSize*(num));
  return ps;
}

svEntity_t *
SV_SvEntityForGentity(sharedEntity_t *gEnt)
{
  if (!gEnt || gEnt->s.number < 0 || gEnt->s.number >= MAX_GENTITIES)
  {
    Com_Error(ERR_DROP, "SV_SvEntityForGentity: bad gEnt");
  }

  return &sv.svEntities[gEnt->s.number];
}

sharedEntity_t *
SV_GEntityForSvEntity(svEntity_t *svEnt)
{
  qint num;

  num = ARRAY_INDEX(sv.svEntities, svEnt);
  return SV_GentityNum(num);
}

/*
===============
SV_GameSendServerCommand

Sends a command string to a client
===============
*/
static void
SV_GameSendServerCommand(qint clientNum, const qchar *text)
{
  qint i;
  client_t *client;

  if (clientNum == -1)
  {
    SV_SendServerCommand(NULL, "%s", text);
  }
  else if (clientNum == -2)
  {
    for(i = 0;i < sv.maxclients;i++)
    {
      client = &svs.clients[i];

      if (client->state < CS_PRIMED)
      {
        continue;
      }

      if (client->netchan.remoteAddress.type == NA_LOOPBACK || client->netchan.remoteAddress.type == NA_BOT || client->gentity->r.svFlags & SVF_BOT)
      {
        continue;
      }

      SV_AddServerCommand(client, text);
    }
  }
  else
  {
    if (clientNum < 0 || clientNum >= sv.maxclients)
    {
      return;
    }

    SV_SendServerCommand(svs.clients + clientNum, "%s", text);	
  }
}


/*
===============
SV_GameDropClient

Disconnects the client with a message
===============
*/
static void
SV_GameDropClient(qint clientNum, const qchar *reason)
{
  if (clientNum < 0 || clientNum >= sv.maxclients)
  {
    return;
  }

  SV_DropClient(svs.clients + clientNum, reason);	
}


/*
=================
SV_SetBrushModel

sets mins and maxs for inline bmodels
=================
*/
static void
SV_SetBrushModel(sharedEntity_t *ent, const qchar *name)
{
  clipHandle_t h;
  vec3_t mins;
  vec3_t maxs;

  if (!name)
  {
    Com_Error(ERR_DROP, "SV_SetBrushModel: NULL");
  }

  if (name[0] != '*')
  {
    Com_Error(ERR_DROP, "SV_SetBrushModel: %s isn't a brush model", name);
  }

  ent->s.modelindex = Q_atoi(name + 1);

  h = CM_InlineModel(ent->s.modelindex);
  CM_ModelBounds(h, mins, maxs);
  VectorCopy(mins, ent->r.mins);
  VectorCopy(maxs, ent->r.maxs);
  ent->r.bmodel = qtrue;

  ent->r.contents = -1; //we don't know exactly what is in the brushes
}



/*
=================
SV_inPVS

Also checks portalareas so that doors block sight
=================
*/
static qbool
SV_inPVS(const vec3_t p1, const vec3_t p2)
{
  qint leafnum;
  qint cluster;
  qint area1;
  qint area2;
  const byte *mask;

  leafnum = CM_PointLeafnum(p1);
  cluster = CM_LeafCluster(leafnum);

  if (cluster < 0)
  {
    return qfalse;
  }

  area1 = CM_LeafArea(leafnum);
  mask = CM_ClusterPVS(cluster);

  leafnum = CM_PointLeafnum(p2);
  cluster = CM_LeafCluster(leafnum);

  if (cluster < 0)
  {
    return qfalse;
  }

  area2 = CM_LeafArea(leafnum);

  if (mask && (!(mask[cluster >> 3] & (BIT(cluster & 7)))))
  {
    return qfalse;
  }

  if (!CM_AreasConnected (area1, area2))
  {
    return qfalse; //a door blocks sight
  }

  return qtrue;
}


/*
=================
SV_inPVSIgnorePortals

Does NOT check portalareas
=================
*/
static qbool
SV_inPVSIgnorePortals(const vec3_t p1, const vec3_t p2)
{
  qint leafnum;
  qint cluster;
  const byte *mask;

  leafnum = CM_PointLeafnum(p1);
  cluster = CM_LeafCluster(leafnum);

  if (cluster < 0)
  {
    return qfalse;
  }

  mask = CM_ClusterPVS(cluster);

  leafnum = CM_PointLeafnum(p2);
  cluster = CM_LeafCluster(leafnum);

  if (cluster < 0)
  {
    return qfalse;
  }

  if (mask && (!(mask[cluster >> 3] & (BIT(cluster & 7)))))
  {
    return qfalse;
  }

  return qtrue;
}


/*
========================
SV_AdjustAreaPortalState
========================
*/
static void
SV_AdjustAreaPortalState(sharedEntity_t *ent, qbool open)
{
  const svEntity_t *svEnt;

  svEnt = SV_SvEntityForGentity(ent);

  if (svEnt->areanum2 == -1)
  {
    return;
  }

  CM_AdjustAreaPortalState(svEnt->areanum, svEnt->areanum2, open);
}


/*
==================
SV_GameAreaEntities
==================
*/
static qbool
SV_EntityContact(const vec3_t mins, const vec3_t maxs, const sharedEntity_t *gEnt, const traceType_t type)
{
  const float *origin;
  const float *angles;
  clipHandle_t ch;
  trace_t trace;

  //check for exact collision
  origin = gEnt->r.currentOrigin;
  angles = gEnt->r.currentAngles;

  ch = SV_ClipHandleForEntity(gEnt);
  CM_TransformedBoxTrace(&trace, vec3_origin, vec3_origin, mins, maxs, ch, -1, origin, angles, type);
  return trace.startsolid;
}


/*
===============
SV_GetServerinfo

===============
*/
static void
SV_GetServerinfo(qchar *buffer, qint bufferSize)
{
  if (bufferSize < 1)
  {
    Com_Error(ERR_DROP, "SV_GetServerinfo: bufferSize == %i", bufferSize);
  }

  if (sv.state != SS_GAME || !sv.configstrings[CS_SERVERINFO])
  {
    Q_strncpyz(buffer, Cvar_InfoString(CVAR_SERVERINFO, NULL), bufferSize);
  }
  else
  {
    Q_strncpyz(buffer, sv.configstrings[CS_SERVERINFO], bufferSize);
  }
}

/*
===============
SV_LocateGameData

===============
*/
void
SV_LocateGameData(sharedEntity_t *gEnts, unsigned numGEntities, unsigned sizeofGEntity_t, playerState_t *clients, unsigned sizeofGameClient)
{
  if (!sv.gvm->entryPoint)
  {
    if (numGEntities > MAX_GENTITIES)
    {
      Com_Error(ERR_DROP, "%s: bad entity count %u", __func__, numGEntities);
    }

    if (sizeofGEntity_t < sizeof(sharedEntity_t) || sizeofGEntity_t > sv.gvm->exactDataLength / MAX_GENTITIES)
    {
      Com_Error(ERR_DROP, "%s: bad entity size %u", __func__, sizeofGEntity_t);
    }
    else if ((byte *)gEnts - sv.gvm->dataBase > sv.gvm->exactDataLength - sizeofGEntity_t * MAX_GENTITIES)
    {
      Com_Error(ERR_DROP, "%s: entities located out of data segment", __func__);
    }

    if (sizeofGameClient < sizeof(playerState_t) || sizeofGameClient > sv.gvm->exactDataLength / MAX_CLIENTS)
    {
      Com_Error(ERR_DROP, "%s: bad game client size %u", __func__, sizeofGameClient);
    }
    else if ((byte *)clients - sv.gvm->dataBase > sv.gvm->exactDataLength - sizeofGameClient * MAX_CLIENTS)
    {
      Com_Error(ERR_DROP, "%s: clients located out of data segment", __func__);
    }
  }

  if (gEnts && (sizeofGEntity_t < (qint)sizeof(sharedEntity_t) || numGEntities < 0))
  {
    Com_Error(ERR_DROP, "%s: incorrect game entity data", __func__);
  }

  if (clients && sizeofGameClient < (qint)sizeof(playerState_t))
  {
    Com_Error(ERR_DROP, "%s: incorrect player state data", __func__);
  }

  sv.gentities = gEnts;
  sv.gentitySize = sizeofGEntity_t;
  sv.num_entities = numGEntities;

  sv.gameClients = clients;
  sv.gameClientSize = sizeofGameClient;
}


/*
===============
SV_GetUsercmd

===============
*/
static void
SV_GetUsercmd(qint clientNum, usercmd_t *cmd)
{
  if ((unsigned)clientNum < sv.maxclients)
  {
    *cmd = svs.clients[clientNum].lastUsercmd;
  }
  else
  {
    Com_Error(ERR_DROP, "%s(): bad clientNum: %i", __func__, clientNum);
  }
}

//==============================================

static qint
FloatAsInt(float f)
{
  floatint_t fi;
	
  fi.f = f;
  return fi.i;
}

/*
====================
VM_ArgPtr
====================
*/
static void *
VM_ArgPtr(intptr_t intValue)
{
  if (!intValue || sv.gvm == NULL)
  {
    return NULL;
  }

  if (sv.gvm->entryPoint)
  {
    return (void *)(sv.gvm->dataBase + intValue);
  }
  else
  {
    return (void *)(sv.gvm->dataBase + (intValue & sv.gvm->dataMask));
  }
}

/*
====================
GVM_ArgPtr

exported version
====================
*/
void *
GVM_ArgPtr(intptr_t intValue)
{
  return VM_ArgPtr(intValue);
}

static qbool
SV_GetValue(qchar *value, qint valueSize, const qchar *key)
{
  if (!Q_stricmp(key, "SVF_SELF_PORTAL2_T110E"))
  {
    Com_sprintf(value, valueSize, "%i", SVF_SELF_PORTAL2);
    return qtrue;
  }

  if (!Q_stricmp(key, "trap_Cvar_SetDescription_T110E"))
  {
    Com_sprintf(value, valueSize, "%i", G_CVAR_SETDESCRIPTION);
    return qtrue;
  }

  return qfalse;
}

/*
====================
SV_GameSystemCalls

The module is making a system call
====================
*/
static intptr_t
SV_GameSystemCalls(intptr_t *args)
{
  switch(args[0])
  {
    case
    G_PRINT:
      SV_GamePrint(VMA(1));
      return 0;

    case
    G_ERROR:
      SV_GameError(VMA(1));
      return 0;

    case
    G_MILLISECONDS:
      return Sys_Milliseconds();

    case
    G_CVAR_REGISTER:
      Cvar_Register(VMA(1), VMA(2), VMA(3), args[4], sv.gvm->privateFlag); 
      return 0;

    case
    G_CVAR_UPDATE:
      Cvar_Update(VMA(1), sv.gvm->privateFlag);
      return 0;

    case
    G_CVAR_SET:
      Cvar_SetSafe((const qchar *)VMA(1), (const qchar *)VMA(2));
      return 0;

    case
    G_CVAR_VARIABLE_INTEGER_VALUE:
      return Cvar_VariableIntegerValue((const qchar *)VMA(1));

    case
    G_CVAR_VARIABLE_STRING_BUFFER:
      VM_CHECKBOUNDS(sv.gvm, args[2], args[3]);
      Cvar_VariableStringBufferSafe(VMA(1), VMA(2), args[3], sv.gvm->privateFlag);
      return 0;

    case
    G_ARGC:
      return Cmd_Argc();

    case
    G_ARGV:
      VM_CHECKBOUNDS(sv.gvm, args[2], args[3]);
      Cmd_ArgvBuffer(args[1], VMA(2), args[3]);
      return 0;

    case
    G_SEND_CONSOLE_COMMAND:
      Cbuf_ExecuteText(args[1], VMA(2));
      return 0;

    case
    G_FS_FOPEN_FILE:
      return FS_VM_OpenFile(VMA(1), VMA(2), args[3], H_GAME);

    case
    G_FS_READ:
      if (args[3] == 0) //UrT may pass this with args[2]=-1 and cause false bounds check error
      {
        return 0;
      }

      VM_CheckBounds(sv.gvm, args[1], args[2]);
      return FS_VM_ReadFile(VMA(1), args[2], args[3], H_GAME);

    case
    G_FS_WRITE:
      VM_CHECKBOUNDS(sv.gvm, args[1], args[2]);
      FS_VM_WriteFile(VMA(1), args[2], args[3], H_GAME);
      return 0;

    case
    G_FS_FCLOSE_FILE:
      FS_VM_CloseFile(args[1], H_GAME);
      return 0;

    case
    G_FS_SEEK:
      return FS_VM_SeekFile(args[1], args[2], args[3], H_GAME);

    case
    G_FS_GETFILELIST:
      VM_CHECKBOUNDS(sv.gvm, args[3], args[4]);
      return FS_GetFileList(VMA(1), VMA(2), VMA(3), args[4]);

    case
    G_LOCATE_GAME_DATA:
      SV_LocateGameData(VMA(1), args[2], args[3], VMA(4), args[5]);
      return 0;

    case
    G_DROP_CLIENT:
      SV_GameDropClient(args[1], VMA(2));
      return 0;

    case
    G_SEND_SERVER_COMMAND:
      SV_GameSendServerCommand(args[1], VMA(2));
      return 0;

    case
    G_LINKENTITY:
      SV_LinkEntity(VMA(1));
      return 0;

    case
    G_UNLINKENTITY:
      SV_UnlinkEntity(VMA(1));
      return 0;

    case
    G_ENTITIES_IN_BOX:
      VM_CHECKBOUNDS3(sv.gvm, args[3], args[4], sizeof(qint));
      return SV_AreaEntities(VMA(1), VMA(2), VMA(3), args[4]);

    case
    G_ENTITY_CONTACT:
      return SV_EntityContact(VMA(1), VMA(2), VMA(3), TT_AABB);

    case
    G_ENTITY_CONTACTCAPSULE:
      return SV_EntityContact(VMA(1), VMA(2), VMA(3), TT_CAPSULE);

    case
    G_TRACE:
      SV_Trace(VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], TT_AABB);
      return 0;

    case
    G_TRACECAPSULE:
      SV_Trace(VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], TT_CAPSULE);
      return 0;

    case
    G_POINT_CONTENTS:
      return SV_PointContents(VMA(1), args[2]);

    case
    G_SET_BRUSH_MODEL:
      SV_SetBrushModel(VMA(1), VMA(2));
      return 0;

    case
    G_IN_PVS:
      return SV_inPVS(VMA(1), VMA(2));

    case
    G_IN_PVS_IGNORE_PORTALS:
      return SV_inPVSIgnorePortals(VMA(1), VMA(2));

    case
    G_SET_CONFIGSTRING:
      SV_SetConfigstring(args[1], VMA(2));
      return 0;

    case
    G_GET_CONFIGSTRING:
      VM_CHECKBOUNDS(sv.gvm, args[2], args[3]);
      SV_GetConfigstring(args[1], VMA(2), args[3]);
      return 0;

    case
    G_SET_USERINFO:
      SV_SetUserinfo(args[1], VMA(2));
      return 0;

    case
    G_GET_USERINFO:
      VM_CHECKBOUNDS(sv.gvm, args[2], args[3]);
      SV_GetUserinfo(args[1], VMA(2), args[3]);
      return 0;

    case
    G_GET_SERVERINFO:
      VM_CHECKBOUNDS(sv.gvm, args[1], args[2]);
      SV_GetServerinfo(VMA(1), args[2]);
      return 0;

    case
    G_ADJUST_AREA_PORTAL_STATE:
      SV_AdjustAreaPortalState(VMA(1), args[2]);
      return 0;

    case
    G_AREAS_CONNECTED:
      return CM_AreasConnected(args[1], args[2]);

    case
    G_GET_USERCMD:
      SV_GetUsercmd(args[1], VMA(2));
      return 0;

    case
    G_GET_ENTITY_TOKEN:
    {
      qchar *s;

      s = (qchar *)COM_Parse(&sv.entityParsePoint);
      VM_CHECKBOUNDS(sv.gvm, args[1], args[2]);
      //Q_strncpyz(VMA(1), s, args[2]);
      //we can't use our optimized Q_strncpyz() function
      //because of uninitialized memory bug in defrag mod
      {
        qchar *dst = (qchar *)VMA(1);
        const qint size = args[2] - 1;

        if (size >= 0)
        {
          Q_strncpy(dst, s, size);
          dst[size] = '\0';
        }
      }

      if (!sv.entityParsePoint && !s[0])
      {
        return qfalse;
      }

      return qtrue;
    }

    case
    G_REAL_TIME:
      return Com_RealTime(VMA(1));

    case
    G_SNAPVECTOR:
      Sys_SnapVector(VMA(1));
      return 0;

    case
    G_SEND_GAMESTAT:
      SV_MasterGameStat(VMA(1));
      return 0;

    //====================================

    case
    G_PARSE_ADD_GLOBAL_DEFINE:
      return Parse_AddGlobalDefine(VMA(1));

    case
    G_PARSE_LOAD_SOURCE:
      return Parse_LoadSourceHandle(VMA(1));

    case
    G_PARSE_FREE_SOURCE:
      return Parse_FreeSourceHandle(args[1]);

    case
    G_PARSE_READ_TOKEN:
      VM_CHECKBOUNDS(sv.gvm, args[2], sizeof(pc_token_t));
      return Parse_ReadTokenHandle(args[1], VMA(2));

    case
    G_PARSE_SOURCE_FILE_AND_LINE:
      return Parse_SourceFileAndLine(args[1], VMA(2), VMA(3));

    //====================================

#if defined(USE_BULLET)
    case
    BULLET_ADD_WORLD_BRUSHES_TO_DYNAMICS_WORLD:
      CM_AddWorldBrushesToDynamicsWorld(VMA(1), VMA(2));
      return 0;
#endif

    case
    TRAP_MEMSET:
      VM_CHECKBOUNDS(sv.gvm, args[1], args[3]);
      Com_Memset(VMA(1), args[2], args[3]);
      return args[1];

    case
    TRAP_MEMCPY:
      VM_CHECKBOUNDS2(sv.gvm, args[1], args[2], args[3]);
      Com_Memcpy(VMA(1), VMA(2), args[3]);
      return args[1];

    case
    TRAP_STRNCPY:
      VM_CHECKBOUNDS(sv.gvm, args[1], args[3]);
      Q_strncpy(VMA(1), VMA(2), args[3]);
      return args[1];

    case
    TRAP_SIN:
      return FloatAsInt(sin(VMF(1)));

    case
    TRAP_COS:
      return FloatAsInt(cos(VMF(1)));

    case
    TRAP_ATAN2:
      return FloatAsInt(atan2(VMF(1), VMF(2)));

    case
    TRAP_SQRT:
      return FloatAsInt(sqrt(VMF(1)));

    case
    G_MATRIXMULTIPLY:
      MatrixMultiply(VMA(1), VMA(2), VMA(3));
      return 0;

    case
    G_ANGLEVECTORS:
      AngleVectors(VMA(1), VMA(2), VMA(3), VMA(4));
      return 0;

    case
    G_PERPENDICULARVECTOR:
      PerpendicularVector(VMA(1), VMA(2));
      return 0;

    case
    G_FLOOR:
      return FloatAsInt(floor(VMF(1)));

    case
    G_CEIL:
      return FloatAsInt(ceil(VMF(1)));

    case
    G_TESTPRINTINT:
      return sprintf(VMA(1), "%i", (qint)args[2]);

    case
    G_TESTPRINTFLOAT:
      return sprintf(VMA(1), "%f", VMF(2));

    case
    G_CVAR_SETDESCRIPTION:
      Cvar_SetDescription2((const qchar *)VMA(1), (const qchar *)VMA(2));
      return 0;

    case
    G_TRAP_GETVALUE:
      VM_CHECKBOUNDS(sv.gvm, args[1], args[2]);
      return SV_GetValue(VMA(1), args[2], VMA(3));

    default:
      Com_Error(ERR_DROP, "Bad game system trap: %ld", (long qint)args[0]);
  }

  return -1;
}

/*
====================
SV_DllSyscall
====================
*/
static intptr_t QDECL
SV_DllSyscall(intptr_t arg, ...)
{
#if !id386 || defined(__clang__)
  intptr_t args[14]; //max.count for game
  va_list ap;
  qint i;

  args[0] = arg;
  va_start(ap, arg);

  for(i = 1;i < ARRAY_LEN(args);i++)
  {
    args[i] = va_arg(ap, intptr_t);
  }

  va_end(ap);
  return SV_GameSystemCalls(args);
#else
  return SV_GameSystemCalls(&arg);
#endif
}

/*
===============
SV_ShutdownGameProgs

Called every time a map changes
===============
*/
void
SV_ShutdownGameProgs(void)
{
  if (!sv.gvm)
  {
    return;
  }

  VM_Call(sv.gvm, 1, GAME_SHUTDOWN, qfalse);
  VM_Free(sv.gvm);
  sv.gvm = NULL;
  FS_VM_CloseFiles(H_GAME);
}

/*
==================
SV_InitGameVM

Called for both a full init and a restart
==================
*/
static void
SV_InitGameVM(qbool restart)
{
  qint i;

  //clear physics interaction links
  SV_ClearWorld();

  //start the entity parsing at the beginning
  sv.entityParsePoint = CM_EntityString();

  //clear all gentity pointers that might still be set from
  //a previous level
  //https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=522
  //now done before GAME_INIT call
  for(i = 0;i < sv.maxclients;i++)
  {
    svs.clients[i].gentity = NULL;
  }

#if defined(SUPPORT_STATUS_SCORES_OVERRIDE)
  SV_StatusScoresOverride_Reset();
#endif

  //use the current msec count for a random seed
  //init for this gamestate
  VM_Call(sv.gvm, 3, GAME_INIT, sv.time, Com_Milliseconds(), restart);
}



/*
===================
SV_RestartGameProgs

Called on a map_restart, but not on a normal map change
===================
*/
void
SV_RestartGameProgs(void)
{
  if (!sv.gvm)
  {
    return;
  }

  VM_Call(sv.gvm, 1, GAME_SHUTDOWN, qtrue);

  //do a restart instead of a free
  sv.gvm = VM_Restart(sv.gvm);

  if (!sv.gvm)
  {
    Com_Error(ERR_FATAL, "VM_Restart on game failed");
  }

  SV_InitGameVM(qtrue);

  //load userinfo filters
  SV_LoadFilters(sv_filter->string);
}


/*
===============
SV_InitGameProgs

Called on a normal map change, not on a map_restart
===============
*/
void
SV_InitGameProgs(void)
{
#if defined(USE_LLVM)
  //load the dll or bytecode
  sv.gvm = VM_Create("game", SV_GameSystemCalls, SV_DllSyscall, Cvar_VariableValue("vm_game"));
#else
  //load the dll
  //sv.gvm = VM_Create("game", SV_GameSystemCalls, VMI_NATIVE);
  //load the dll or bytecode
  sv.gvm = VM_Create(VM_GAME, SV_GameSystemCalls, SV_DllSyscall, Cvar_VariableValue("vm_game"));
#endif

  if (!sv.gvm)
  {
    Com_Error(ERR_FATAL, "VM_Create on game failed");
  }

  SV_InitGameVM(qfalse);

  //load userinfo filters
  SV_LoadFilters(sv_filter->string);
}


/*
====================
SV_GameCommand

See if the current console command is claimed by the game
====================
*/
qbool
SV_GameCommand(void)
{
  if (sv.state != SS_GAME)
  {
    return qfalse;
  }

  return VM_Call(sv.gvm, 0, GAME_CONSOLE_COMMAND);
}

#endif //!defined(USE_JAVA)
