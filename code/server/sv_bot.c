/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006-2008 Cheyenne Spring Barnes
Copyright (C) 2006-2008 Robert Beckebans <trebor_7@users.sourceforge.net>

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
// sv_bot.c

#include "server.h"
#include "../botlib/botlib.h"

typedef struct
bot_debugpoly_s
{
  qint inuse;
  qint color;
  qint numPoints;
  vec3_t points[128];
}
bot_debugpoly_t;

static bot_debugpoly_t *debugpolygons;
static qint bot_maxdebugpolys;

extern botlib_export_t *botlib_export;

/*
==================
SV_BotClientCommand
==================
*/
void
BotClientCommand(qint client, const qchar *command)
{
  SV_ExecuteClientCommand(&svs.clients[client], command);
}


/*
==================
BotImport_PointContents
==================
*/
static qint
BotImport_PointContents(vec3_t point)
{
  return SV_PointContents(point, -1);
}

/*
==================
BotImport_inPVS
==================
*/
static qint
BotImport_inPVS(vec3_t p1, vec3_t p2)
{
  return SV_inPVS(p1, p2);
}

/*
==================
BotImport_BSPEntityData
==================
*/
static qchar *
BotImport_BSPEntityData(void)
{
  return CM_EntityString();
}

/*
==================
BotImport_BSPModelMinsMaxsOrigin
==================
*/
static void
BotImport_BSPModelMinsMaxsOrigin(qint modelnum, vec3_t angles, vec3_t outmins, vec3_t outmaxs, vec3_t origin)
{
  clipHandle_t h;
  vec3_t mins, maxs;
  float max;
  qint i;

  h = CM_InlineModel(modelnum);
  CM_ModelBounds(h, mins, maxs);

  //if the model is rotated
  if ((angles[0] || angles[1] || angles[2]))
  {
    //expand for rotation

    max = RadiusFromBounds(mins, maxs);

    for(i = 0;i < 3;i++)
    {
      mins[i] = -max;
      maxs[i] = max;
    }
  }

  if (outmins)
  {
    VectorCopy(mins, outmins);
  }

  if (outmaxs)
  {
    VectorCopy(maxs, outmaxs);
  }

  if (origin)
  {
    VectorClear(origin);
  }
}


/*
==================
BotImport_GetMemory
==================
*/
static void *
BotImport_GetMemory(size_t size)
{
  void *ptr;

  ptr = Z_TagMalloc(size, TAG_BOTLIB);
  return ptr;
}


/*
==================
BotImport_FreeMemory
==================
*/
static void
BotImport_FreeMemory(void *ptr)
{
  Z_Free(ptr);
}


/*
=================
BotImport_HunkAlloc
=================
*/
static void *
BotImport_HunkAlloc(size_t size)
{
  if (Hunk_CheckMark())
  {
    Com_Error(ERR_DROP, "%s(): Alloc with marks already set", __func__);
  }

  return Hunk_Alloc(size, h_high);
}


/*
==================
BotImport_DebugPolygonCreate
==================
*/
qint
BotImport_DebugPolygonCreate(qint color, qint numPoints, vec3_t *points)
{
  bot_debugpoly_t *poly;
  int i;

  if (!debugpolygons)
  {
    return 0;
  }

  for(i = 1;i < bot_maxdebugpolys;i++)
  {
    if (!debugpolygons[i].inuse)
    {
      break;
    }
  }

  if (i >= bot_maxdebugpolys)
  {
    return 0;
  }

  poly = &debugpolygons[i];
  poly->inuse = qtrue;
  poly->color = color;
  poly->numPoints = numPoints;
  Com_Memcpy(poly->points, points, numPoints * sizeof(vec3_t));
  //
  return i;
}


/*
==================
BotImport_DebugPolygonShow
==================
*/
static void
BotImport_DebugPolygonShow(qint id, qint color, qint numPoints, vec3_t *points)
{
  bot_debugpoly_t *poly;

  if (!debugpolygons)
  {
    return;
  }

  if ((unsigned) id >= bot_maxdebugpolys)
  {
    return;
  }

  poly = &debugpolygons[id];
  poly->inuse = qtrue;
  poly->color = color;
  poly->numPoints = numPoints;
  Com_Memcpy(poly->points, points, numPoints * sizeof(vec3_t));
}


/*
==================
BotImport_DebugPolygonDelete
==================
*/
void
BotImport_DebugPolygonDelete(qint id)
{
  if (!debugpolygons)
  {
    return;
  }

  if ((unsigned)id >= bot_maxdebugpolys)
  {
    return;
  }

  debugpolygons[id].inuse = qfalse;
}


/*
==================
BotImport_DebugLineCreate
==================
*/
static qint
BotImport_DebugLineCreate(void)
{
  vec3_t points[1];
  return BotImport_DebugPolygonCreate(0, 0, points);
}

/*
==================
BotImport_DebugLineDelete
==================
*/
static void
BotImport_DebugLineDelete(qint line)
{
  BotImport_DebugPolygonDelete(line);
}

/*
==================
BotImport_DebugLineShow
==================
*/
static void
BotImport_DebugLineShow(qint line, vec3_t start, vec3_t end, qint color)
{
  vec3_t points[4], dir, cross, up = {0, 0, 1};
  float dot;

  VectorCopy(start, points[0]);
  VectorCopy(start, points[1]);
  //points[1][2] -= 2;
  VectorCopy(end, points[2]);
  //points[2][2] -= 2;
  VectorCopy(end, points[3]);


  VectorSubtract(end, start, dir);
  VectorNormalize(dir);
  dot = DotProduct(dir, up);

  if (dot > 0.99 || dot < -0.99)
  {
    VectorSet(cross, 1, 0, 0);
  }
  else
  {
    CrossProduct(dir, up, cross);
  }

  VectorNormalize(cross);

  VectorMA(points[0], 2, cross, points[0]);
  VectorMA(points[1], -2, cross, points[1]);
  VectorMA(points[2], -2, cross, points[2]);
  VectorMA(points[3], 2, cross, points[3]);

  BotImport_DebugPolygonShow(line, color, 4, points);
}


/*
==================
BotImport_Print
==================
*/
static __attribute__ ((format (printf, 2, 3))) void QDECL
BotImport_Print(qint type, const char *fmt, ...)
{
  qchar str[2048];
  va_list ap;

  va_start(ap, fmt);
  Q_vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);

  switch(type)
  {
    case
    PRT_MESSAGE:
    {
      Com_Printf("%s", str);
      break;
    }

    case
    PRT_WARNING:
    {
      Com_Printf(S_COLOR_WARNING "Warning: %s", str);
      break;
    }

    case
    PRT_ERROR:
    {
      Com_Printf(S_COLOR_ERROR "Error: %s", str);
      break;
    }

    case
    PRT_FATAL:
    {
      Com_Printf(S_COLOR_ERROR "Fatal: %s", str);
      break;
    }

    case
    PRT_EXIT:
    {
      Com_Error(ERR_DROP, S_COLOR_ERROR "Exit: %s", str);
      break;
    }

    default:
    {
      Com_Printf("unknown print type\n");
      break;
    }
  }
}


/*
==================
BotImport_Trace
==================
*/
static void
BotImport_Trace(bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, qint passent, qint contentmask)
{
  trace_t trace;

  SV_Trace(&trace, start, mins, maxs, end, passent, contentmask, qfalse);

  //copy the trace information
  bsptrace->allsolid = trace.allsolid;
  bsptrace->startsolid = trace.startsolid;
  bsptrace->fraction = trace.fraction;
  VectorCopy(trace.endpos, bsptrace->endpos);
  bsptrace->plane.dist = trace.plane.dist;
  VectorCopy(trace.plane.normal, bsptrace->plane.normal);
  bsptrace->plane.signbits = trace.plane.signbits;
  bsptrace->plane.type = trace.plane.type;
  bsptrace->surface.value = 0;
  bsptrace->surface.flags = trace.surfaceFlags;
  bsptrace->ent = trace.entityNum;
  bsptrace->exp_dist = 0;
  bsptrace->sidenum = 0;
  bsptrace->contents = 0;
}


/*
==================
BotImport_EntityTrace
==================
*/
static void
BotImport_EntityTrace(bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, qint entnum, qint contentmask)
{
  trace_t trace;

  if ((unsigned)entnum > MAX_GENTITIES - 1)
  {
    entnum = ENTITYNUM_NONE;
  }

  SV_ClipToEntity(&trace, start, mins, maxs, end, entnum, contentmask, qfalse);

  //copy the trace information
  bsptrace->allsolid = trace.allsolid;
  bsptrace->startsolid = trace.startsolid;
  bsptrace->fraction = trace.fraction;
  VectorCopy(trace.endpos, bsptrace->endpos);
  bsptrace->plane.dist = trace.plane.dist;
  VectorCopy(trace.plane.normal, bsptrace->plane.normal);
  bsptrace->plane.signbits = trace.plane.signbits;
  bsptrace->plane.type = trace.plane.type;
  bsptrace->surface.value = 0;
  bsptrace->surface.flags = trace.surfaceFlags;
  bsptrace->ent = trace.entityNum;
  bsptrace->exp_dist = 0;
  bsptrace->sidenum = 0;
  bsptrace->contents = 0;
}


/*
==================
SV_BotInitBotLib
==================
*/
void
SV_BotInitBotLib(void)
{
  botlib_import_t botlib_import;

  if (debugpolygons)
  {
    Z_Free(debugpolygons);
  }

  debugpolygons = Z_Malloc(sizeof(bot_debugpoly_t) * 2);

  botlib_import.Print = BotImport_Print;
  botlib_import.Trace = BotImport_Trace;
  botlib_import.EntityTrace = BotImport_EntityTrace;
  botlib_import.PointContents = BotImport_PointContents;
  botlib_import.inPVS = BotImport_inPVS;
  botlib_import.BSPEntityData = BotImport_BSPEntityData;
  botlib_import.BSPModelMinsMaxsOrigin = BotImport_BSPModelMinsMaxsOrigin;
  botlib_import.BotClientCommand = BotClientCommand;

  //memory management
  botlib_import.GetMemory = BotImport_GetMemory;
  botlib_import.FreeMemory = BotImport_FreeMemory;
  botlib_import.AvailableMemory = Z_AvailableMemory;
  botlib_import.HunkAlloc = BotImport_HunkAlloc;

  // file system access
  botlib_import.FS_FOpenFile = FS_FOpenFileByMode;
  botlib_import.FS_Read = FS_Read;
  botlib_import.FS_Write = FS_Write;
  botlib_import.FS_FCloseFile = FS_FCloseFile;
  botlib_import.FS_Seek = FS_Seek;

  //debug lines
  botlib_import.DebugLineCreate = BotImport_DebugLineCreate;
  botlib_import.DebugLineDelete = BotImport_DebugLineDelete;
  botlib_import.DebugLineShow = BotImport_DebugLineShow;

  //debug polygons
  botlib_import.DebugPolygonCreate = BotImport_DebugPolygonCreate;
  botlib_import.DebugPolygonDelete = BotImport_DebugPolygonDelete;

  botlib_import.Sys_Milliseconds = Sys_Milliseconds;

  botlib_export = (botlib_export_t *)GetBotLibAPI( BOTLIB_API_VERSION, &botlib_import );
  assert(botlib_export); //somehow we end up with a zero import.
}


/*
===============
SV_ShutdownBotLib

Called when either the entire server is being killed, or
it is changing to a different game directory.
===============
*/
qint
SV_BotLibShutdown(void)
{
  if (!botlib_export)
  {
    return -1;
  }

  return botlib_export->BotLibShutdown();
}


/*
==================
SV_BotAllocateClient
==================
*/
qint
SV_BotAllocateClient(void)
{
  qint i;
  client_t *cl;

  //find a client slot
  for(i = 0, cl = svs.clients;i < sv.maxclients;i++, cl++)
  {
    if (cl->state == CS_FREE)
    {
      break;
    }
  }

  if (i == sv.maxclients)
  {
    return -1;
  }

  cl->gentity = SV_GentityNum(i);
  cl->gentity->s.number = i;
  cl->state = CS_ACTIVE;
  cl->lastPacketTime = svs.time;
  cl->snapshotMsec = 1000 / sv_fps->integer;
  cl->netchan.remoteAddress.type = NA_BOT;
  cl->rate = 0;

  svs.clients[i].tld[0] = '\0';
  svs.clients[i].country = "BOT";

  return i;
}


/*
==================
SV_BotFreeClient
==================
*/
void
SV_BotFreeClient(qint clientNum)
{
  client_t *cl;

  if ((unsigned)clientNum >= sv.maxclients)
  {
    Com_Error(ERR_DROP, "SV_BotFreeClient: bad clientNum: %i", clientNum);
  }

  cl = &svs.clients[clientNum];
  cl->state = CS_FREE;
  cl->name[0] = '\0';

  if (cl->gentity)
  {
    cl->gentity->r.svFlags &= ~SVF_BOT;
  }
}


/*
==================
SV_BotFrame
==================
*/
void
SV_BotFrame(qint time)
{
#if defined(USE_JAVA)
  Java_G_RunAIFrame(time);
#else
  //NOTE: maybe the game is already shutdown
  if (!sv.gvm)
  {
    return;
  }

  VM_Call(sv.gvm, 1, BOTAI_START_FRAME, time);
#endif
}


//
//  * * * BOT AI CODE IS BELOW THIS POINT * * *
//

/*
==================
SV_BotGetConsoleMessage
==================
*/
qint
SV_BotGetConsoleMessage(qint client, qchar *buf, qint size)
{
  if ((unsigned)client < sv.maxclients)
  {
    client_t *cl;
    qint index;

    cl = &svs.clients[client];
    cl->lastPacketTime = svs.time;

    if (cl->reliableAcknowledge == cl->reliableSequence)
    {
      return qfalse;
    }

    cl->reliableAcknowledge++;
    index = cl->reliableAcknowledge & (MAX_RELIABLE_COMMANDS - 1);

    if (!cl->reliableCommands[index][0])
    {
      return qfalse;
    }

    Q_strncpyz(buf, cl->reliableCommands[index], size);
    return qtrue;
  }
  else
  {
    return qfalse;
  }
}


#if 0
/*
==================
EntityInPVS
==================
*/
qint
EntityInPVS(qint client, qint entityNum)
{
  client_t *cl;
  clientSnapshot_t *frame;
  qint i;

  cl = &svs.clients[client];
  frame = &cl->frames[cl->netchan.outgoingSequence & PACKET_MASK];

  for(i = 0;i < frame->num_entities;i++)
  {
    if (svs.snapshotEntities[(frame->first_entity + i) % svs.numSnapshotEntities].number == entityNum)
    {
      return qtrue;
    }
  }

  return qfalse;
}
#endif


/*
==================
SV_BotGetSnapshotEntity
==================
*/
qint
SV_BotGetSnapshotEntity(qint client, qint sequence)
{
  if ((unsigned)client < sv.maxclients)
  {
    const client_t *cl = &svs.clients[client];
    const clientSnapshot_t *frame = &cl->frames[cl->netchan.outgoingSequence & PACKET_MASK];

    if ((unsigned)sequence >= frame->num_entities)
    {
      return -1;
    }

    return frame->ents[sequence]->number;
  }
  else
  {
    return -1;
  }
}


/*
==================
SV_BotClientCommand
==================
*/
void
SV_BotClientCommand(qint client, const qchar *command)
{
  SV_ExecuteClientCommand(&svs.clients[client], command);
}
