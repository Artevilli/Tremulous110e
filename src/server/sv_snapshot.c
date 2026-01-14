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
=============================================================================

Delta encode a client frame onto the network channel

A normal server packet will look like:

4 sequence number (high bit set if an oversize fragment)
<optional reliable commands>
1 svc_snapshot
4 last client reliable command
4 serverTime
1 lastframe for delta compression
1 snapFlags
1 areaBytes
<areabytes>
<playerstate>
<packetentities>

=============================================================================
*/

//Chey: add this here so the compiler knows what it is
static qbool
SV_UpdateServerCommandsToClients(client_t *client, msg_t *msg, qbool allowPartial);

/*
=============
SV_EmitPacketEntities

Writes a delta update of an entityState_t list to the message.
=============
*/
#if defined(GAMESTATE_OVERFLOW_FIX)
static void
SV_EmitPacketEntities(const clientSnapshot_t *from, const clientSnapshot_t *to, msg_t *msg, qint maxEntityBaseline)
{
  entityState_t *newent = NULL;
  entityState_t *oldent = NULL;
  qint oldindex = 0;
  qint newindex = 0;
  qint oldnum;
  qint newnum;
  const qint from_num_entities = from ? from->num_entities:0;

  while(newindex < to->num_entities || oldindex < from_num_entities)
  {
    if (newindex >= to->num_entities)
    {
      newnum = MAX_GENTITIES + 1;
    }
    else
    {
      newent = to->ents[newindex];
      newnum = newent->number;
    }

    if (oldindex >= from_num_entities)
    {
      oldnum = MAX_GENTITIES + 1;
    }
    else
    {
      oldent = from->ents[oldindex];
      oldnum = oldent->number;
    }

    if (newnum == oldnum)
    {
      //delta update from old position
      //because the force parm is qfalse, this will not result
      //in any bytes being emited if the entity has not changed at all
      MSG_WriteDeltaEntity(msg, oldent, newent, qfalse);
      oldindex++;
      newindex++;
      continue;
    }

    if (newnum < oldnum)
    {
      //this is a new entity, send it from the baseline
      if (newnum > maxEntityBaseline)
      {
        //treat baselines excluded from gamestate as null
        entityState_t null_baseline;

        Com_Memset(&null_baseline, 0, sizeof(null_baseline));
        MSG_WriteDeltaEntity(msg, &null_baseline, newent, qtrue);
      }
      else
      {
        MSG_WriteDeltaEntity(msg, &sv.svEntities[newnum].baseline, newent, qtrue);
      }

      newindex++;
      continue;
    }

    if (newnum > oldnum)
    {
      //the old entity isn't present in the new message
      MSG_WriteDeltaEntity(msg, oldent, NULL, qtrue);
      oldindex++;
      continue;
    }
  }

  MSG_WriteBits(msg, (MAX_GENTITIES - 1), GENTITYNUM_BITS); //end of packetentities
}
#else
static void
SV_EmitPacketEntities(const clientSnapshot_t *from, const clientSnapshot_t *to, msg_t *msg)
{
  entityState_t *newent = NULL;
  entityState_t *oldent = NULL;
  qint oldindex = 0;
  qint newindex = 0;
  qint oldnum;
  qint newnum;
  const qint from_num_entities = from ? from->num_entities:0;

  while(newindex < to->num_entities || oldindex < from_num_entities)
  {
    if (newindex >= to->num_entities)
    {
      newnum = MAX_GENTITIES + 1;
    }
    else
    {
      newent = to->ents[newindex];
      newnum = newent->number;
    }

    if (oldindex >= from_num_entities)
    {
      oldnum = MAX_GENTITIES + 1;
    }
    else
    {
      oldent = from->ents[oldindex];
      oldnum = oldent->number;
    }

    if (newnum == oldnum)
    {
      //delta update from old position
      //because the force parm is qfalse, this will not result
      //in any bytes being emited if the entity has not changed at all
      MSG_WriteDeltaEntity(msg, oldent, newent, qfalse);
      oldindex++;
      newindex++;
      continue;
    }

    if (newnum < oldnum)
    {
      //this is a new entity, send it from the baseline
      MSG_WriteDeltaEntity(msg, &sv.svEntities[newnum].baseline, newent, qtrue);
      newindex++;
      continue;
    }

    if (newnum > oldnum)
    {
      //the old entity isn't present in the new message
      MSG_WriteDeltaEntity(msg, oldent, NULL, qtrue);
      oldindex++;
      continue;
    }
  }

  MSG_WriteBits(msg, (MAX_GENTITIES - 1), GENTITYNUM_BITS); //end of packetentities
}
#endif

/*
==================
SV_WriteSnapshotToClient
==================
*/
static void
SV_WriteSnapshotToClient(client_t *client, msg_t *msg)
{
  const clientSnapshot_t *oldframe;
  const clientSnapshot_t *frame;
  qint lastframe;
  qint i;
  qint snapFlags;

  //this is the snapshot we are creating
  frame = &client->frames[client->netchan.outgoingSequence & PACKET_MASK];

  // try to use a previous frame as the source for delta compressing the snapshot
  if (/*client->deltaMessage <= 0 || */client->state != CS_ACTIVE)
  {
    //client is asking for a retransmit
    oldframe = NULL;
    lastframe = 0;
  }
  else if (client->netchan.outgoingSequence - client->deltaMessage >= (PACKET_BACKUP - 3))
  {
    //client hasn't gotten a good message through in a long time
    if (com_developer->integer)
    {
      if (client->deltaMessage != client->netchan.outgoingSequence - (PACKET_BACKUP + 1))
      {
        Com_Printf("%s: Delta request from out of date packet.\n", client->name);
      }
    }

    oldframe = NULL;
    lastframe = 0;
  }
  else
  {
    // we have a valid snapshot to delta from
    oldframe = &client->frames[client->deltaMessage & PACKET_MASK];
    lastframe = client->netchan.outgoingSequence - client->deltaMessage;

    //we may refer on outdated frame
    if (oldframe->frameNum - svs.lastValidFrame < 0)
    {
      Com_DPrintf("%s: Delta request from out of date frame.\n", client->name);
      oldframe = NULL;
      lastframe = 0;
    }
  }

  MSG_WriteByte(msg, svc_snapshot);

  //NOTE, MRE: now sent at the start of every message from server to client
  //let the client know which reliable clientCommands we have received
  //MSG_WriteLong( msg, client->lastClientCommand );

  // send over the current server time so the client can drift
  // its view of time to try to match
  if (client->oldServerTime)
  {
    //The server has not yet got an acknowledgement of the
    //new gamestate from this client, so continue to send it
    //a time as if the server has not restarted. Note from
    //the client's perspective this time is strictly speaking
    //incorrect, but since it'll be busy loading a map at
    //the time it doesn't really matter.
    MSG_WriteLong(msg, sv.time + client->oldServerTime);
  }
  else
  {
    MSG_WriteLong(msg, sv.time);
  }

  // what we are delta'ing from
  MSG_WriteByte(msg, lastframe);

  snapFlags = svs.snapFlagServerBit;

  if (client->rateDelayed)
  {
    snapFlags |= SNAPFLAG_RATE_DELAYED;
  }

  if (client->state != CS_ACTIVE)
  {
    snapFlags |= SNAPFLAG_NOT_ACTIVE;
  }

  MSG_WriteByte(msg, snapFlags);

  // send over the areabits
  MSG_WriteByte(msg, frame->areabytes);
  MSG_WriteData(msg, frame->areabits, frame->areabytes);

  //dont send any changes to zombies
  if (client->state <= CS_ZOMBIE)
  {
    //playerstate
    MSG_WriteByte(msg, 0); //# of changes
    MSG_WriteBits(msg, 0, 1); //no array changes

    //packet entities
    MSG_WriteBits(msg, (MAX_GENTITIES - 1), GENTITYNUM_BITS);
    return;
  }

  // delta encode the playerstate
  if (oldframe)
  {
    MSG_WriteDeltaPlayerstate(msg, &oldframe->ps, &frame->ps);
  }
  else
  {
    MSG_WriteDeltaPlayerstate(msg, NULL, &frame->ps);
  }

  //delta encode the entities
#if defined(GAMESTATE_OVERFLOW_FIX)
  SV_EmitPacketEntities(oldframe, frame, msg, client->maxEntityBaseline);
#else
  SV_EmitPacketEntities(oldframe, frame, msg);
#endif

  //padding for rate debugging
  if (sv_padPackets->integer)
  {
    for(i = 0;i < sv_padPackets->integer;i++)
    {
      MSG_WriteByte(msg, svc_nop);
    }
  }
}


/*
==================
SV_UpdateServerCommandsToClient

(re)send all server commands the client hasn't acknowledged yet
==================
*/
void
SV_UpdateServerCommandsToClient(client_t *client, msg_t *msg)
{
  SV_UpdateServerCommandsToClients(client, msg, qfalse);
}

/*
==================
SV_UpdateServerCommandsToClients
==================
*/
static qbool
SV_UpdateServerCommandsToClients(client_t *client, msg_t *msg, qbool allowPartial)
{
  qint i;
  qint n;
  qint countSent = 0;

  //write any unacknowledged serverCommands
  n = client->reliableSequence - client->reliableAcknowledge;

  for(i = 0;i < n;i++, countSent++)
  {
    const qint index = client->reliableAcknowledge + 1 + i;
    const qchar * const cmd = client->reliableCommands[index & (MAX_RELIABLE_COMMANDS - 1)];
    //msg overflow checks for 4 byte internally, we want to write svc_servercommand (1 byte), the index (4 byte) and the string and 0 terminator (1 byte)
    //also multiply with 2 because worst theoretical case each byte might end up 2 bytes with huffman
    if (allowPartial && msg->maxsize - msg->cursize - 4 < 2 * (1 + 4 + (qint)strlen(client->reliableCommands[i & (MAX_RELIABLE_COMMANDS - 1)]) + 1))
    {
      client->reliableSent = i - 1;
      return qfalse;
    }

    //try to reduce the risk of a "CL_GetServerCommand: a reliable command was cycled out" error by limiting the total amount of
    //new servercommands we send in a single message
    //this server is modded to have a higher serverside buffer (MAX_RELIABLE_COMMANDS) than a vanilla client
    //so don't send more than the client will conservatively be able to process
    //also consider demo playback, so do like half of what the client would probably be able to handle because
    //it might parse two messages in a single frame, i suppose it might parse even more but oh well, nothing is perfect
    if (allowPartial && countSent >= (MAX_RELIABLE_COMMANDS_VANILLA / 2))
    {
      client->reliableSent = i - 1;
      return qfalse;
    }

    MSG_WriteByte(msg, svc_serverCommand);
    MSG_WriteLong(msg, index);
    MSG_WriteString(msg, cmd);
  }

  client->reliableSent = client->reliableSequence;
  return qtrue;
}

/*
=============================================================================

Build a client snapshot structure

=============================================================================
*/

typedef qint entityNum_t;

typedef struct
{
  qint numSnapshotEntities;
  entityNum_t snapshotEntities[MAX_SNAPSHOT_ENTITIES];  
  qbool unordered;
}
snapshotEntityNumbers_t;

/*
=======================
SV_SortEntityNumbers

Insertion sort is about 10 times faster than quicksort for our task
=======================
*/
static const void
SV_SortEntityNumbers(entityNum_t *num, const qint size)
{
  entityNum_t tmp;
  qint i;
  qint d;

  for(i = 1;i < size;i++)
  {
    d = i;

    while(d > 0 && num[d] < num[d - 1])
    {
      tmp = num[d];
      num[d] = num[d - 1];
      num[d - 1] = tmp;
      d--;
    }
  }

#if defined(_DEBUG)
  //consistency check for delta encoding
  for(i = 1;i < size;i++)
  {
    if (num[i - 1] >= num[i])
    {
      Com_Error(ERR_DROP, "%s: invalid entity number %i", __func__, num[i]);
    }
  }
#endif
}

/*
===============
SV_AddIndexToSnapshot
===============
*/
static void
SV_AddIndexToSnapshot(svEntity_t *svEnt, qint index, snapshotEntityNumbers_t *eNums)
{
  svEnt->snapshotCounter = sv.snapshotCounter;

  //if we are full, silently discard entities
  if (eNums->numSnapshotEntities >= MAX_SNAPSHOT_ENTITIES)
  {
    return;
  }

  eNums->snapshotEntities[eNums->numSnapshotEntities] = index;
  eNums->numSnapshotEntities++;
}

/*
===============
SV_AddEntitiesVisibleFromPoint
===============
*/
static void
SV_AddEntitiesVisibleFromPoint(const vec3_t origin, clientSnapshot_t *frame, snapshotEntityNumbers_t *eNums, qbool portal)
{
  qint e;
  qint i;
  sharedEntity_t *ent;
  svEntity_t *svEnt;
  entityState_t *es;
  qint l;
  qint clientarea;
  qint clientcluster;
  qint leafnum;
  qint eventNumber;
  byte *clientpvs;
  byte *bitvector;

  //during an error shutdown message we may need to transmit
  //the shutdown message after the server has shutdown, so
  //specfically check for it
  if (sv.state == SS_DEAD)
  {
    return;
  }

  leafnum = CM_PointLeafnum(origin);
  clientarea = CM_LeafArea(leafnum);
  clientcluster = CM_LeafCluster(leafnum);

  //calculate the visible areas
  frame->areabytes = CM_WriteAreaBits(frame->areabits, clientarea);

  clientpvs = CM_ClusterPVS(clientcluster);

  for(e = 0;e < svs.currFrame->count;e++)
  {
    es = svs.currFrame->ents[e];
    ent = SV_GentityNum(es->number);

    //entities can be flagged to be sent to only one client
    if (ent->r.svFlags & SVF_SINGLECLIENT)
    {
      if (ent->r.singleClient != frame->ps.clientNum)
      {
        continue;
      }
    }

    //entities can be flagged to be sent to everyone but one client
    if (ent->r.svFlags & SVF_NOTSINGLECLIENT)
    {
      if (ent->r.singleClient == frame->ps.clientNum)
      {
        continue;
      }
    }

    //entities can be flagged to be sent to a given mask of clients
    if (ent->r.svFlags & SVF_CLIENTMASK)
    {
      if (frame->ps.clientNum >= 128)
      {
        Com_Error(ERR_DROP, "SVF_CLIENTMASK: clientNum >= 128");
      }

      if (~ent->r.singleClient & BIT(frame->ps.clientNum))
      {
        continue;
      }
    }

    if (ent->s.eType >= ET_EVENTS)
    {
      eventNumber = (ent->s.eType - ET_EVENTS) & ~EV_EVENT_BITS;

      if (eventNumber == EV_JUMP || eventNumber == EV_SWIM || eventNumber == EV_FOOTSTEP)
      {
        continue;
      }
    }

    svEnt = &sv.svEntities[es->number];

    //don't double add an entity through portals
    if (svEnt->snapshotCounter == sv.snapshotCounter)
    {
      continue;
    }

    if (sv_novis->integer)
    {
      SV_AddIndexToSnapshot(svEnt, e, eNums);
      continue;
    }

    //broadcast entities are always sent
    if (ent->r.svFlags & SVF_BROADCAST)
    {
      SV_AddIndexToSnapshot(svEnt, e, eNums);
      continue;
    }

    if (sv_sendNearbyEnts->integer)
    {
      vec3_t range;

      VectorSubtract(origin, ent->r.currentOrigin, range);

      if (VectorLength(range) < sv_sendNearbyEntsRange->integer)
      {
        SV_AddIndexToSnapshot(svEnt, e, eNums);
        continue;
      }
    }

    //ignore if not touching a PV leaf
    //check area
    if (!CM_AreasConnected(clientarea, svEnt->areanum))
    {
      //doors can legally straddle two areas, so
      //we may need to check another one
      if (!CM_AreasConnected(clientarea, svEnt->areanum2))
      {
        continue; //blocked by a door
      }
    }

    bitvector = clientpvs;

    //check individual leafs
    if (!svEnt->numClusters)
    {
      continue;
    }

    l = 0;

    for(i = 0;i < svEnt->numClusters;i++)
    {
      l = svEnt->clusternums[i];

      if (bitvector[l >> 3] & UBIT(l & 7))
      {
        break;
      }
    }

    //if we haven't found it to be visible,
    //check overflow clusters that coudln't be stored
    if (i == svEnt->numClusters)
    {
      if (svEnt->lastCluster)
      {
        for(;l <= svEnt->lastCluster;l++)
        {
          if (bitvector[l >> 3] & UBIT(l & 7))
          {
            break;
          }
        }

        if (l == svEnt->lastCluster)
        {
          continue; //not visible
        }
      }
      else
      {
        continue;
      }
    }

    if (sv_antiWallhack->integer)
    {
      trace_t trace;
      vec3_t corners;
      vec3_t shift;
      vec3_t viewpoint;
      qbool visible;
      unsigned vectors;
      unsigned pass;
      float delta = 0.25f;

      visible = qfalse;

      for(pass = 0;pass < 2;pass++)
      {
        if (visible)
        {
          break;
        }

        if (!pass)
        {
          VectorCopy(origin, viewpoint);
          VectorClear(shift);
        }
        else
        {
          VectorMA(origin, delta, frame->ps.velocity, viewpoint);

          if (ent->s.pos.trType != TR_STATIONARY)
          {
            VectorScale(ent->s.pos.trDelta, delta, shift);
          }
          else
          {
            VectorClear(shift);
          }
        }

        for(vectors = 0;vectors < 8;vectors++)
        {
          corners[0] = ent->r.currentOrigin[0] + shift[0] + (vectors & 1 ? ent->r.maxs[0]:ent->r.mins[0]);
          corners[1] = ent->r.currentOrigin[1] + shift[1] + (vectors & 2 ? ent->r.maxs[1]:ent->r.mins[1]);
          corners[2] = ent->r.currentOrigin[2] + shift[2] + (vectors & 4 ? ent->r.maxs[2]:ent->r.mins[2]);

          SV_Trace(&trace, viewpoint, NULL, NULL, corners, frame->ps.clientNum, CONTENTS_SOLID, qfalse);

          if (trace.fraction == 1.0f || trace.entityNum == ent->s.number || (trace.contents & CONTENTS_TRANSLUCENT))
          {
            visible = qtrue;
            break;
          }
        }
      }

      switch(sv_antiWallhack->integer)
      {
        case
        1:
          if (!visible && ent->s.eType == ET_PLAYER)
          {
            continue;
          }

          break;

        case
        2:
          if (!visible && ent->s.eType != ET_PLAYER)
          {
            continue;
          }

          break;

        default:
          if (!visible)
          {
            continue;
          }

          break;
      }
    }

    //add it
    SV_AddIndexToSnapshot(svEnt, e, eNums);

    //if its a portal entity, add everything visible from its camera position
    if (ent->r.svFlags & SVF_PORTAL && !portal)
    {
      if (ent->s.generic1)
      {
        vec3_t dir;

        VectorSubtract(ent->s.origin, origin, dir);
        if (VectorLengthSquared(dir) > (float)ent->s.generic1 * ent->s.generic1)
        {
          continue;
        }
      }

      eNums->unordered = qtrue;
      SV_AddEntitiesVisibleFromPoint(ent->s.origin2, frame, eNums, portal);
    }
  }

  ent = SV_GentityNum(frame->ps.clientNum);

  //extension: merge second PVS at ent->r.s.origin2
  if (ent->r.svFlags & SVF_SELF_PORTAL2 && !portal)
  {
    SV_AddEntitiesVisibleFromPoint(ent->r.s.origin2, frame, eNums, qtrue);
    eNums->unordered = qtrue;
  }
}

/*
=============
SV_InitSnapshotStorage
=============
*/
void
SV_InitSnapshotStorage(void)
{
  //initialize snapshot storage
  Com_Memset(svs.snapFrames, 0, sizeof(svs.snapFrames));
  svs.freeStorageEntities = svs.numSnapshotEntities;
  svs.currentStoragePosition = 0;

  svs.snapshotFrame = 0;
  svs.currentSnapshotFrame = 0;
  svs.lastValidFrame = 0;

  svs.currFrame = NULL;
}

/*
=============
SV_IssueNewSnapshot
=============
*/
void
SV_IssueNewSnapshot(void)
{
  svs.currFrame = NULL;

  //value that clients can use even for their empty frames
  //as it will not increment on new snapshot built
  svs.currentSnapshotFrame = svs.snapshotFrame;
}

/*
=============
SV_BuildCommonSnapshot

This always allocates new common snapshot frame
=============
*/
static void
SV_BuildCommonSnapshot(void) 
{
  sharedEntity_t *list[MAX_GENTITIES];
  sharedEntity_t *ent;	
  snapshotFrame_t *tmp;
  snapshotFrame_t *sf;
  qint count;
  qint index;
  qint num;
  qint i;

  count = 0;

  //gather all linked entities
  if (sv.state != SS_DEAD)
  {
    for(num = 0;num < sv.num_entities;num++)
    {
      ent = SV_GentityNum(num);

#if 1 //!defined(USE_JAVA)
      //never send entities that arent linked in
      if (!ent->r.linked)
      {
        continue;
      }
#endif

      if (ent->s.number != num)
      {
        Com_DPrintf("FIXING ENT->S.NUMBER %i => %i\n", ent->s.number, num);
        ent->s.number = num;
      }

      //entities can be flagged to explicitly not be sent to the client
      if (ent->r.svFlags & SVF_NOCLIENT)
      {
        continue;
      }

      list[count++] = ent;
      sv.svEntities[num].snapshotCounter = -1;
    }
  }

  sv.snapshotCounter = -1;

  sf = &svs.snapFrames[svs.snapshotFrame % NUM_SNAPSHOT_FRAMES];
	
  //track last valid frame
  if (svs.snapshotFrame - svs.lastValidFrame > (NUM_SNAPSHOT_FRAMES - 1))
  {
    svs.lastValidFrame = svs.snapshotFrame - (NUM_SNAPSHOT_FRAMES - 1);

    //release storage
    svs.freeStorageEntities += sf->count;
    sf->count = 0;
  }

  //release more frames if needed
  while(svs.freeStorageEntities < count && svs.lastValidFrame != svs.snapshotFrame)
  {
    tmp = &svs.snapFrames[svs.lastValidFrame % NUM_SNAPSHOT_FRAMES];
    svs.lastValidFrame++;

    //release storage
    svs.freeStorageEntities += tmp->count;
    tmp->count = 0;
  }

  //should never happen but anyway
  if (svs.freeStorageEntities < count)
  {
    Com_Error(ERR_DROP, "Not enough snapshot storage: %i < %i", svs.freeStorageEntities, count);
  }

  //allocate storage
  sf->count = count;
  svs.freeStorageEntities -= count;

  sf->start = svs.currentStoragePosition; 
  svs.currentStoragePosition = (svs.currentStoragePosition + count) % svs.numSnapshotEntities;

  sf->frameNum = svs.snapshotFrame;
  svs.snapshotFrame++;

  svs.currFrame = sf; //clients can refer to this

  //setup start index
  index = sf->start;

  for(i = 0;i < count;i++, index = (index + 1) % svs.numSnapshotEntities)
  {
    //index %= svs.numSnapshotEntities;
    svs.snapshotEntities[index] = list[i]->s;
    sf->ents[i] = &svs.snapshotEntities[index];
  }
}

/*
=============
SV_BuildClientSnapshot

Decides which entities are going to be visible to the client, and
copies off the playerstate and areabits.

This properly handles multiple recursive portals, but the render
currently doesn't.

For viewing through other player's eyes, clent can be something other than client->gentity
=============
*/
static void
SV_BuildClientSnapshot(client_t *client)
{
  vec3_t org;
  clientSnapshot_t *frame;
  snapshotEntityNumbers_t entityNumbers;
  qint i;
  svEntity_t *svEnt;
  qint clientNum;
  playerState_t *ps;

  //this is the frame we are creating
  frame = &client->frames[client->netchan.outgoingSequence & PACKET_MASK];

  //clear everything in this snapshot
  Com_Memset(frame->areabits, 0, sizeof(frame->areabits));
  frame->areabytes = 0;

  //https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=62
  frame->num_entities = 0;
  frame->frameNum = svs.currentSnapshotFrame;

  if (client->state == CS_ZOMBIE)
  {
    return;
  }

  // grab the current playerState_t
  ps = SV_GameClientNum(ARRAY_INDEX(svs.clients, client));
  frame->ps = *ps;

  //never send client's own entity, because it can
  //be regenerated from the playerstate
  clientNum = frame->ps.clientNum;

  if (clientNum < 0 || clientNum >= MAX_GENTITIES)
  {
    Com_Error(ERR_DROP, "SV_SvEntityForGentity: bad gEnt");
  }

  //we set client->gentity only after sending gamestate
  //so dont send any packetentities changes until cs primed
  //because new gamestate will invalidate them anyway
  if (!client->gentity)
  {
    return;
  }

  if (svs.currFrame == NULL)
  {
    //this will always success and setup current frame
    SV_BuildCommonSnapshot();
  }

  //bump the counter used to prevent double adding
  sv.snapshotCounter++;

  //empty entities before visibility check
  entityNumbers.numSnapshotEntities = 0;

  frame->frameNum = svs.currFrame->frameNum;

  //never send clients own gentity because it can
  //be regenerated from the playerstate
  svEnt = &sv.svEntities[clientNum];
  svEnt->snapshotCounter = sv.snapshotCounter;

  //find the client's viewpoint
  VectorCopy(ps->origin, org);
  org[2] += ps->viewheight;

  //add all the entities directly visible to the eye, which
  //may include portal entities that merge other viewpoints
  entityNumbers.unordered = qfalse;
  SV_AddEntitiesVisibleFromPoint(org, frame, &entityNumbers, qfalse);

  //if there were portals visible, there may be out of order entities
  //in the list which will need to be resorted for the delta compression
  //to work correctly.  This also catches the error condition
  //of an entity being included twice.
  if (entityNumbers.unordered)
  {
    SV_SortEntityNumbers(&entityNumbers.snapshotEntities[0], entityNumbers.numSnapshotEntities);
  }

  //now that all viewpoint's areabits have been OR'd together, invert
  //all of them to make it a mask vector, which is what the renderer wants
  for(i = 0;i < MAX_MAP_AREA_BYTES / sizeof(qint);i++)
  {
    ((qint *)frame->areabits)[i] = ((qint *)frame->areabits)[i] ^ -1;
  }

  frame->num_entities = entityNumbers.numSnapshotEntities;

  //get pointers from common snapshot
  for(i = 0;i < entityNumbers.numSnapshotEntities;i++)
  {
    frame->ents[i] = svs.currFrame->ents[entityNumbers.snapshotEntities[i]];
  }
}

/*
=======================
SV_SnapshotRateMsec

Return the number of msec a given size message is supposed
to take to clear, based on the current rate
=======================
*/
#define HEADER_RATE_BYTES 48
const qint
SV_SnapshotRateMsec(const client_t *client, qint messageSize)
{
  //individual messages will never be larger than fragment size
  //FIXME - use MAX_PACKETLEN or FRAGMENT_SIZE here, not random numbers...
  if (messageSize > 1500)
  {
    messageSize = 1500;
  }

  if (!client->rate)
  {
    return 0;
  }

  qint rate = client->rate;

  return ((messageSize + HEADER_RATE_BYTES) * 1000 / (qint)(rate * com_timescale->value));
}

/*
=======================
SV_SendMessageToClient

Called by SV_SendClientSnapshot and SV_SendClientGameState
=======================
*/
void
SV_SendMessageToClient(msg_t *msg, client_t *client)
{
  //qint rateMsec;

  if (client->gentity && (client->gentity->r.svFlags & SVF_BOT))
  {
    return; //Chey: bots dont need snapshots
  }

  //record information about the message
  client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSize = msg->cursize;
  client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSent = svs.msgTime;
  client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageAcked = 0;

  SV_Netchan_Transmit(client, msg);
}

/*
=======================
SV_SendClientIdle

There is no need to send full snapshots who are loading a map.
Send them idle packets with the bare minimum required to keep the client on the server.
=======================
*/
static void
SV_SendClientIdle(client_t *client)
{
  byte msg_buf[MAX_MSGLEN_BUF];
  msg_t msg;
#if !defined(UDP_DOWNLOAD_OPTIMIZE)
  msg_t msgBackup;
#endif

  MSG_Init(&msg, msg_buf, MAX_MSGLEN);

  //NOTE, MRE: all server->client messages now acknowledge
  //let the client know which reliable clientCommands we have received
  MSG_WriteLong(&msg, client->lastClientCommand);

  //(re)send any reliable server commands
  if (!SV_UpdateServerCommandsToClients(client, &msg, qtrue))
  {
    //if all commands cant fit in a single message send anyways without entities
    SV_SendMessageToClient(&msg, client);
    return;
  }

  if (msg.overflowed)
  {
    Com_Printf("sendclientidle msg overflowed for %s\n", client->name);
    MSG_Clear(&msg);
    SV_DropClient(client, "sendclientidle msg overflowed");
    return;
  }

  SV_SendMessageToClient(&msg, client);

  if (sv_forceSendFragments->integer)
  {
    while(client->netchan.unsentFragments)
    {
      Netchan_TransmitNextFragment(&client->netchan);
    }
  }
}

/*
=======================
SV_SendClientSnapshot

Also called by SV_FinalMessage

=======================
*/
void
SV_SendClientSnapshot(client_t *client)
{
  byte msg_buf[MAX_MSGLEN_BUF];
  msg_t msg;
  msg_t msgBackup;

  //Chey: bots dont need snapshots
  if (client->gentity && client->gentity->r.svFlags & SVF_BOT)
  {
    return;
  }

  //zombie clients need full snaps to process reliable commands such as picking up disconnect reason
  if (client->state < CS_ACTIVE)
  {
    if (client->state != CS_ZOMBIE)
    {
      SV_SendClientIdle(client);
      return;
    }
  }

  //build the snapshot
  SV_BuildClientSnapshot(client);

  //bots need to have snapshots built but they query without directly needing to be sent
  //if (client->gentity && client->gentity->r.svFlags & SVF_BOT)
  //{
    //return;
  //}

  MSG_Init(&msg, msg_buf, MAX_MSGLEN);

  //NOTE, MRE: all server->client messages now acknowledge
  //let the client know which reliable clientCommands we have received
  MSG_WriteLong(&msg, client->lastClientCommand);

  //(re)send any reliable server commands
  if (!SV_UpdateServerCommandsToClients(client, &msg, qtrue))
  {
    //if all commands cant fit in a single message send anyways without entities
    SV_SendMessageToClient(&msg, client);
    return;
  }

  //back up msg state in case snapshot overflows
  Com_Memcpy(&msgBackup, &msg, sizeof(msgBackup));

  //send over all the relevant entityState_t
  //and the playerState_t
  SV_WriteSnapshotToClient(client, &msg);

#if defined(USE_VOIP)
  SV_WriteVoipToClient(client, &msg);
#endif

  if (msg.overflowed && !msgBackup.overflowed)
  {
    //entity states were too much so send old states as net code wont send msg buf content afterwards old msg values point to updated buffer
    SV_SendMessageToClient(&msgBackup, client);
    return;
  }

  //check for overflow
  if (msg.overflowed)
  {
    //This always end fucking the server.
    Com_Printf("WARNING: msg overflowed for %s\n", client->name);
    MSG_Clear(&msg);
    SV_DropClient(client, "SV_SendClientSnapshot: msg overflowed");
    return;
  }

  SV_SendMessageToClient(&msg, client);

/* this works fine on lan (160k/s dl, yay) and SEEMS okay over the net, but needs more testing */
#define UNSUCK_DOWNLOADS
#if defined(UNSUCK_DOWNLOADS)
  if (sv_forceSendFragments->integer)
  {
    //KHB 071111 the whole reason Q3 dl's SUCK is that there's a "secret" artificial cap
    //based on UDP packet sizes. so the server "sends" a 2k block each snap, but it's actually
    //cut down to 1300 bytes, MINUS the game traffic, so the ABSOLUTE peak dl speed is roughly
    //1K * 30 snaps, EVEN ON LAN; and the realistic dl speed from an active server is about
    //400 bytes * svfps, depending on how many random newbs are spamming pulse and chat etc
    while(client->netchan.unsentFragments)
    {
      SV_Netchan_TransmitNextFragment(client);
    }
  }
#endif
}

/*
=======================
SV_CheckInvalidUserInfoValues
=======================
*/
static void
SV_CheckInvalidUserInfoValues(client_t *cl)
{
  const qchar *warning = NULL;
  qint timeout = 120000;
  qbool critical = qfalse;

  if (cl->state != CS_ACTIVE)
  {
    return;
  }

  if (cl->invalidValues & BIT((qint)CHECKEDTYPE_RATE))
  {
    warning = ("^1Your 'rate' value is invalid. Please check it and set a proper value.");
    critical = qtrue;
  }
  else if (cl->invalidValues & BIT((qint)CHECKEDTYPE_SNAPS))
  {
    warning = ("^1Your 'snaps' value is invalid. Please check it and set a proper value.");
    critical = qtrue;
  }
  else if (cl->rate < 5000)
  {
    warning = va("^3Your 'rate' value is extremely low (%d). Please consider a higher value.", cl->rate);
    timeout = 1200000; //(60000 * 20) every 20 minutes
  }
  else if (cl->snaps < 20) //30)
  {
    //FIXME: only warn about this in spectators
    warning = va("^3Your 'snaps' value is extremely low (%d). Please consider a higher value.", cl->snaps);
    timeout = 3600000; //(60000 * 60) every 60 minutes
  }

  if (!warning || (cl->lastInvalidValuesWarning && svs.time - timeout < cl->lastInvalidValuesWarning && svs.time > cl->lastInvalidValuesWarning))
  {
    return;
  }

  //SV_SendServerCommand(cl, va("print \"%s\n\"", warning));

  if (critical)
  {
    //SV_SendServerCommand(cl, va("cp \"%s\n\"", warning));
    SV_SendServerCommand(cl, va("print \"%s\n\"", warning)); //FIXME: cp gets cut off very fast for some reason...
    Com_Printf(S_COLOR_RED "Sending critical warning to client %s: %s\n", cl->name, warning);
  }
  else
  {
    SV_SendServerCommand(cl, va("print \"%s\n\"", warning));
    Com_Printf(S_COLOR_YELLOW "Sending non-critical warning to client %s: %s\n", cl->name, warning);
  }

  cl->lastInvalidValuesWarning = svs.time;
}

/*
=======================
SV_SendClientMessages
=======================
*/
void
SV_SendClientMessages(void)
{
  qint i;
  client_t *c;

  svs.msgTime = Sys_Milliseconds();

  //send a message to each connected client
  for(i = 0;i < sv.maxclients;i++)
  {
    c = &svs.clients[i];

    if (c->state == CS_FREE)
    {
      continue; //not connected
    }

    if (c->state == CS_CONNECTED)
    {
      continue; //dont send snapshots if downloading
    }

#if defined(UDP_DOWNLOAD_NO_DOUBLE_LOAD)
    //extra check since downloading clients can now be CS_PRIMED
    if (c->downloading)
    {
      continue;
    }
#endif

    if (!SV_IsValidClientSnapshot(c))
    {
      continue; //not a valid snapshot
    }

    if (svs.time - c->lastSnapshotTime < c->snapshotMsec * com_timescale->value)
    {
      continue; //not time yet
    }

    if (*c->downloadName)
    {
      //if the client is downloading via netchan and has not acknowledged a package in 12 seconds, drop it
      if (c->download && (svs.time - c->downloadAckTime) > 12000)
      {
        SV_DropClient(c, "Download failed");
      }
    }

    if (c->gentity && (c->gentity->r.svFlags & SVF_BOT))
    {
      continue; //Chey: bots may cause error drops in the server net chan
    }

    //send additional message fragments if the last message was too large to send at once
    if (c->netchan.unsentFragments)
    {
      c->lastSnapshotTime = svs.time + SV_SnapshotRateMsec(c, c->netchan.unsentLength - c->netchan.unsentFragmentStart);
      SV_Netchan_TransmitNextFragment(c);
      continue;
    }

#if defined(SNAPSHOT_DELTA_BUFFER_FIX)
    qint bufferUsage = 0;

    if (c->state == CS_ACTIVE && c->netchan.outgoingSequence - c->deltaMessage < (PACKET_BACKUP - 3))
    {
      const clientSnapshot_t *deltaFrame = &c->frames[c->deltaMessage & PACKET_MASK];

      if (deltaFrame->frameNum - svs.lastValidFrame >= 0)
      {
        //got valid delta frame according to SV_WriteSnapshotToClient conditions
        //now tally entity usage from delta frame up to current frame
        qint j;

        for(j = 0;j < PACKET_BACKUP;++j)
        {
          const qint messageNum = c->deltaMessage + j;
          const clientSnapshot_t *frame = &c->frames[messageNum & PACKET_MASK];

          if (messageNum >= c->netchan.outgoingSequence)
          {
            break;
          }

          if (frame->frameNum >= deltaFrame->frameNum)
          {
            bufferUsage += frame->num_entities;
          }
        }
      }
    }

    //old clients (prior to a certain ioq3 import from 2013) use a MAX_PARSE_ENTITIES value of 2048 with
    //CL_ParseSnapshot failing if the buffer usage ahead of the new snapshot is greater than
    //MAX_PARSE_ENTITIES - 128, the limit here is reduced to MAX_PARSE_ENTITIES - 256 to be extra safe
    //against theoretically possible (but probably unlikely) entity corruption
    if (bufferUsage >= 2048 - 256)
    {
      Com_DPrintf("forcing non delta snapshot %i for client %i due to entity buffer usage %i\n", c->netchan.outgoingSequence, i, bufferUsage);
      c->deltaMessage = c->netchan.outgoingSequence - (PACKET_BACKUP + 1);
    }
#endif

    //warn user if he has invalid snaps/rate settings
    SV_CheckInvalidUserInfoValues(c);

    //generate and send a new message
    SV_SendClientSnapshot(c);
    c->lastSnapshotTime = svs.time;
    c->rateDelayed = qfalse;
  }
}

/*
=======================
SV_IsValidClientSnapshot
=======================
*/
qbool
SV_IsValidClientSnapshot(const client_t *client)
{
  //if (client->deltaMessage <= 0)
  if (client->deltaMessage == client->netchan.outgoingSequence - (PACKET_BACKUP + 1))
  {
    return qtrue;
  }

  if (client->state != CS_ACTIVE)
  {
    return qtrue;
  }

  if (client->lastPacketTime >= svs.time)
  {
    return qtrue;
  }

  //if (client->netchan.outgoingSequence - client->deltaMessage < 29)
  if (client->netchan.outgoingSequence - client->deltaMessage < (PACKET_BACKUP - 3))
  {
    return qtrue;
  }

  return qfalse;
}
