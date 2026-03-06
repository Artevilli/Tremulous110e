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
// server.h

#pragma once

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/vm_local.h"
#include "../qcommon/md5.h"
#include "../game/g_public.h"
#include "../game/bg_public.h"
#include "sv_cvars.h"

//=============================================================================

#define	PERS_SCORE 0 //!!! MUST NOT CHANGE, SERVER AND GAME BOTH REFERENCE !!!

//server attack protection
typedef enum
protect_flags_s
{
  SVP_XREAL = BIT(0), //1 xreal
  SVP_OWOLF = BIT(1), //2 openwolf
  SVP_CONSOLE = BIT(2), //4 console print
}
protect_flags_t;

#define MAX_BPS_WINDOW 20

#define	MAX_ENT_CLUSTERS 16

//use an alternative dropped gamestate, simplifying code and fixing potential udp download issues
#define GAMESTATE_RETRANSMIT_VERSION_TWO

//avoid a potential issue where clients load the map twice after a download completes
#if defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
#define UDP_DOWNLOAD_NO_DOUBLE_LOAD
#endif

//avoid sending reliable commands to loading clients, testing needed
#define SKIP_PRE_ACTIVE_COMMANDS

//fix for certain cases of delta parseentitiesnum too old errors in certain cases, such as 40+ sv_fps values with high client ping or bad connection
#define SNAPSHOT_DELTA_BUFFER_FIX

//prevent gamestate overflows by dropping entity baselines, fixes errors on certain maps under certain conditions
#define GAMESTATE_OVERFLOW_FIX

//allow mods to set custom player scores that are sent in response to status queries rather than using the playerstate score field
#define SUPPORT_STATUS_SCORES_OVERRIDE

//webconsole support
//#define USE_WEBCONSOLE

#if defined(USE_VOIP)
typedef struct
voipServerPacket_s
{
  qint generation;
  qint sequence;
  qint frames;
  qint len;
  qint sender;
  byte data[1024];
}
voipServerPacket_t;
#endif

typedef struct
svEntity_s
{
  struct worldSector_s *worldSector;
  struct svEntity_s *nextEntityInWorldSector;
	
  entityState_t	baseline; //for delta compression of initial sighting
  qint numClusters; //if -1, use headnode instead
  qint clusternums[MAX_ENT_CLUSTERS];
  qint lastCluster; //if all the clusters don't fit in clusternums
  qint areanum, areanum2;
  qint snapshotCounter; //used to prevent double adding from portal views
}
svEntity_t;

typedef enum
{
  SS_DEAD, //no map loaded
  SS_LOADING, //spawning level entities
  SS_GAME //actively running
}
serverState_t;

//we might not use all MAX_GENTITIES every frame
//so leave more room for slow snaps clients etc.
#define NUM_SNAPSHOT_FRAMES (PACKET_BACKUP * 4)

typedef struct
snapshotFrame_s
{
  entityState_t *ents[MAX_GENTITIES];
  qint frameNum;
  qint start;
  qint count;
}
snapshotFrame_t;

typedef struct
{
  serverState_t	state;
  qbool restarting; //if true, send configstring changes during SS_LOADING
  qint pure; //fixed at level spawn
  qint maxclients; //fixed at level spawn
  qint serverId; //changes each server start
  qint restartedServerId;
  qint checksumFeed; //the feed key that we use to compute the pure checksum strings
  qint snapshotCounter; //incremented for each snapshot built
  qint timeResidual; //<= 1000 / sv_frame->value
  //struct cmodel_s *models[MAX_MODELS];
  qchar *configstrings[MAX_CONFIGSTRINGS];
  svEntity_t svEntities[MAX_GENTITIES];

  //parsed by the game.qvm to spawn map entities
  const qchar *entityParsePoint; //used during game VM init

  //the game virtual machine will update these on init and changes
#if !defined(USE_JAVA)
  sharedEntity_t *gentities;
  qint gentitySize;
#endif
  qint num_entities; //current number, <= MAX_GENTITIES
  
  //demo recording
  fileHandle_t demoFile;
  demoState_t demoState;
  qchar demoName[MAX_QPATH];
  
  //previous frame for delta compression
  sharedEntity_t demoEntities[MAX_GENTITIES];
  playerState_t demoPlayerStates[MAX_CLIENTS];

#if !defined(USE_JAVA)
  playerState_t	*gameClients;
  qint gameClientSize; //will be > sizeof(playerState_t) due to game private data
#endif

  qint restartTime;
  qint time;

  //net debugging
  qint bpsWindow[MAX_BPS_WINDOW];
  qint bpsWindowSteps;
  qint bpsTotalBytes;
  qint bpsMaxBytes;

  qint ubpsWindow[MAX_BPS_WINDOW];
  qint ubpsTotalBytes;
  qint ubpsMaxBytes;

  float ucompAve;
  qint ucompNum;

  byte baselineUsed[MAX_GENTITIES];

#if !defined(USE_JAVA)
  vm_t *gvm; //game virtual machine
#endif
}
server_t;

typedef enum
{
  SVC_INVALID,
  SVC_CONNECT,
  SVC_CONNECTW,
  SVC_GETSTATUS,
  SVC_FIRST = SVC_GETSTATUS,
  SVC_GETINFO,
  SVC_GETCHALLENGE,
  SVC_RCON,
  SVC_PING,
  SVC_DISCONNECT,
  SVC_MAX
}
svcType_t;

typedef struct
{
  qint areabytes;
  byte areabits[MAX_MAP_AREA_BYTES]; //portalarea visibility bits
  playerState_t	ps;
  qint num_entities;
#if 0
  qint first_entity; //into the circular sv_packet_entities[]
  //the entities MUST be in increasing state number order, otherwise the delta compression will fail
#endif
  qint messageSent; //time the message was transmitted
  qint messageAcked; //time the message was acked
  qint messageSize; //used to rate drop packets

  qint frameNum; //from snapshot storage to compare with last valid
  entityState_t *ents[MAX_SNAPSHOT_ENTITIES];
}
clientSnapshot_t;

typedef enum
{
  CS_FREE, //can be reused for a new connection
  CS_ZOMBIE, //client has been disconnected, but don't reuse connection for a couple seconds
  CS_CONNECTED, //has been assigned to a client_t, but no gamestate yet or downloading
  CS_PRIMED, //gamestate has been sent, but client hasn't sent a usercmd
  CS_ACTIVE //client is fully in game
}
clientState_t;

typedef struct netchan_buffer_s
{
  msg_t msg;
  byte *msgBuffer;
  qchar *lastClientCommandString; //valid command string for SV_Netchan_Encode
  struct netchan_buffer_s *next;
}
netchan_buffer_t;

typedef struct
rateLimit_s
{
  unsigned lastTime;
  unsigned burst;
}
rateLimit_t;

typedef struct
leakyBucket_s
leakyBucket_t;

struct
leakyBucket_s
{
  netadrtype_t type;

  union
  {
    byte _4[4];
    byte _6[16];
  }
  ipv;

  rateLimit_t rate;

  qint hash;
  qint toxic;

  leakyBucket_t *prev;
  leakyBucket_t *next;
};

#if !defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
typedef enum
{
  GSA_INIT = 0, //gamestate never sent with current sv.serverId
  GSA_SENT_ONCE, //gamestate sent once, client can reply with any (messageAcknowledge - gamestateMessageNum) >= 0 and correct serverId
  GSA_SENT_MANY, //gamestate sent many times, client must reply with exact gamestateMessageNum == gamestateMessageNum and correct serverId
  GSA_ACKED //gamestate acknowledged, no retransmissions needed
}
gameStateAck_t;
#endif

#if defined(UDP_DOWNLOAD_OPTIMIZE)
#define MAX_DOWNLOAD_MESSAGE_HISTORY 64

typedef struct
{
  qint blockNumber;
  qint msgNumber;
  qint size;
}
downloadMessageRecord_t;
#endif

typedef enum
checkedNumberType_s
{
  CHECKEDTYPE_RATE,
  CHECKEDTYPE_SNAPS,
  CHECKEDTYPE_TYPECOUNT,
}
checkedNumberType_t;

typedef struct
client_s
{
  clientState_t	state;
  qchar userinfo[MAX_INFO_STRING]; //name, etc

  qchar reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
  qint reliableSequence; //last added reliable message, not necesarily sent or acknowledged yet
  qint reliableAcknowledge; //last acknowledged reliable message
  qint reliableSent; //last sent reliable message, not necesarily acknowledged yet
  qint messageAcknowledge;

  qint gamestateMessageNum; //netchan->outgoingSequence of gamestate
  qint challenge;

  usercmd_t lastUsercmd;
  qint lastMessageNum; //for delta compression
  qint lastClientCommand; //reliable client message sequence
  qchar lastClientCommandString[MAX_STRING_CHARS];
  sharedEntity_t *gentity; //SV_GentityNum(clientnum)
  qchar name[MAX_NAME_LENGTH]; //extracted from userinfo, high bits masked
#if defined(GAMESTATE_RETRANSMIT_VERSION_TWO)
  qbool downloadGamestateDropCheck; //perform extra dropped gamestate check after downloads
#else
  gameStateAck_t gamestateAck;
#endif
  qbool downloading; //set at "download", reset at gamestate retransmission
  //qint serverId; //last acknowledged server id

  //downloading
#if defined(UDP_DOWNLOAD_OPTIMIZE)
  qchar downloadName[MAX_QPATH]; //if not empty string, we are downloading

  //source file
  fileHandle_t download; //file being downloaded
  qint downloadSize; //total bytes in pk3
  unsigned downloadSrcFileRemaining; //number of bytes left to read from file

  //file read buffer
  qchar *downloadSrcChunk; //current chunk buffer
  unsigned downloadSrcChunkPos; //number of bytes read from current chunk
  unsigned downloadSrcChunkSize; //total bytes in current chunk

  //download blocks
  unsigned qchar *downloadBlocks[MAX_DOWNLOAD_WINDOW];
  qint downloadBlockSize[MAX_DOWNLOAD_WINDOW];
  qint downloadClientBlock; //one more than last block acknowledged by client
  qint downloadXmitBlock; //one more than last block sent (may go backwards for retransmit)
  qint downloadCurrentBlock; //one more than last block generated on server

  //download messages
  downloadMessageRecord_t downloadMsgTable[MAX_DOWNLOAD_MESSAGE_HISTORY];
  qint downloadClientMsg; //one more than last msg (table index) acknowledged by client
  qint downloadRetransmitMsg; //first message (table index) since xmit block was reset for retransmit
  qint downloadCurrentMsg; //one more than last msg (table index) generated on server
  qint downloadLastSentTime; //time in Sys_Milliseconds() of last outgoing packet

  //rate limiting
  double downloadCurrentRate; //rate in KB/s
  qint downloadRatePool; //bytes available to send

  //dropping dead connections
  qint downloadAckTime; //time we last got an ack from the client
#else
  qchar downloadName[MAX_QPATH]; //if not empty string, we are downloading
  fileHandle_t download; //file being downloaded
  qint downloadSize; //total bytes (can't use EOF because of paks)
  qint downloadCount; //bytes sent
  qint downloadClientBlock; //last block we sent to the client, awaiting ack
  qint downloadCurrentBlock; //current block number
  qint downloadXmitBlock; //last block we xmited
  unsigned qchar	*downloadBlocks[MAX_DOWNLOAD_WINDOW]; //the buffers for the download blocks
  qint downloadBlockSize[MAX_DOWNLOAD_WINDOW];
  qbool downloadEOF; //We have sent the EOF block
  qint downloadSendTime;	//time we last sent a package
  unsigned downloadAckTime; //time we last got an ack from the client
#endif

  qint deltaMessage; //frame last client usercmd message
  qint nextReliableUserTime; //svs.time when another useinfo change will be allowed
  qint lastPacketTime; //svs.time when packet was last received
  qint lastConnectTime; //svs.time when connection started
  qint lastDisconnectTime;
  qint lastSnapshotTime; //svs.time of last sent snapshot
  //qint nextSnapshotTime; //send another snapshot when svs.time >= nextSnapshotTime
  qbool rateDelayed; //true if nextSnapshotTime was set based on rate instead of snapshotMsec
  qint timeoutCount; //must timeout a few frames in a row so debugging doesn't break
  clientSnapshot_t frames[PACKET_BACKUP]; //updates can be delta'd from here
  qint ping;
  qint rate; //bytes / second
  qint snaps;
  qint snapshotMsec; //requests a snapshot every snapshotMsec unless rate choked
  qbool pureAuthentic;
  qbool gotCP; //TTimo - additional flag to distinguish between a bad pure checksum, and no cp command at all
  netchan_t netchan;
  //TTimo - queuing outgoing fragmented messages to send them properly, without udp packet bursts in case large fragmented messages are stacking up buffer them into this queue, and hand them out to netchan as needed
  netchan_buffer_t *netchan_start_queue;
  netchan_buffer_t **netchan_end_queue;

#if defined(USE_VOIP)
  qbool hasVoip;
  qbool muteAllVoip;
  qbool ignoreVoipFromClient[MAX_CLIENTS];
  voipServerPacket_t voipPacket[64]; //!!! FIXME: WAY too much memory!
  qint queuedVoipPackets;
#endif

  qint oldServerTime;
  qbool csUpdated[MAX_CONFIGSTRINGS];
  qbool compat;

  qint invalidValues; //checkedNumberType_t
  qint lastInvalidValuesWarning;

  //flood protection
  rateLimit_t cmd_rate;
  rateLimit_t info_rate;
  rateLimit_t gamestate_rate;

  //client can decode long strings
  qbool longstr;

  qbool justConnected;

  qchar tld[3]; //"XX\0"
  const qchar *country;

#if defined(GAMESTATE_OVERFLOW_FIX)
  qint maxEntityBaseline;
#endif
}
client_t;

//=============================================================================

#define STATFRAMES(frametime) (frametime * 5) //FIXME: i think this is actually FRAMETIME / @sv_fps //5 seconds, assume running at sv_fps

typedef struct
{
  double active;
  double idle;
  qint count;
  double latched_active;
  double latched_idle;
  float cpu;
  float avg;
}
svstats_t;

typedef struct
{
  netadr_t adr;
  qint time;
}
receipt_t;

typedef struct
{
  netadr_t adr;
  qint time;
  qint count;
  qbool flood;
}
floodBan_t;

#define MAX_QUEUE 10
extern netadr_t queue[MAX_QUEUE];
extern unsigned lastQueue[MAX_QUEUE];
extern unsigned queueCount;

//webconsole
#if defined(USE_WEBCONSOLE)
extern qint sv_webconsoleSocket;
extern qbool sv_webconsoleConnected;
#endif

#define MAX_INFO_RECEIPTS 48 //max number of getstatus and getinfo responses sent in two second time period

#define MAX_INFO_FLOOD_BANS 36

#define SERVER_PERFORMANCECOUNTER_FRAMES 600
#define SERVER_PERFORMANCECOUNTER_SAMPLES 6

//this structure will be cleared only when the game dll changes
typedef struct
{
  qbool initialized; //sv_init has completed

  qint time; //will be strictly increasing across level changes
  qint msgTime; //will be used as precise sent time

  qint snapFlagServerBit; //^= SNAPFLAG_SERVERCOUNT every SV_SpawnServer()

  client_t *clients; //[sv_maxclients->integer];
  qint numSnapshotEntities; //PACKET_BACKUP * MAX_SNAPSHOT_ENTITIES
  entityState_t *snapshotEntities; //[numSnapshotEntities]
  qint nextHeartbeatTime;
  receipt_t infoReceipts[MAX_INFO_RECEIPTS];
  floodBan_t infoFloodBans[MAX_INFO_FLOOD_BANS];
#if defined(INCLUDE_REMOTE_COMMANDS)
  netadr_t redirectAddress; //for rcon return messages

  netadr_t authorizeAddress; //for rcon return messages
#endif

  unsigned sampleTimes[SERVER_PERFORMANCECOUNTER_SAMPLES];
  unsigned currentSampleIndex;
  unsigned totalFrameTime;
  unsigned currentFrameIndex;
  unsigned serverLoad;
  svstats_t stats;

  //common snapshot storage
  qint freeStorageEntities;
  qint currentStoragePosition; //next snapshotentities to use
  qint snapshotFrame; //incremented with each common snapshot built
  qint currentSnapshotFrame; //for initializing empty frames
  qint lastValidFrame; //updated with each snapshot built
  snapshotFrame_t snapFrames[NUM_SNAPSHOT_FRAMES];
  snapshotFrame_t *currFrame; //current frame that clients can refer
}
serverStatic_t;

//=============================================================================

#if defined(INCLUDE_REMOTE_COMMANDS)
#define MAXIMUM_RCON_WHITELIST 32

//Chey: struct for managing rcon password from file
typedef struct
{
  netadr_t ip;

  //for cidr notation type suffix
  qint subNet;
  qbool isException;
}
serverRconPassword_t;

extern serverRconPassword_t rconWhitelist[MAXIMUM_RCON_WHITELIST];
extern qint rconWhitelistCount;
#endif

//=============================================================================

extern serverStatic_t svs; //persistant server info across maps
extern server_t sv; //cleared each map

//===========================================================

//
//sv_main.c
//
const qbool
SVC_RateLimit(rateLimit_t *bucket, qint burst, qint period, qint now);
const void
SVC_RateRestoreBurstAddress(const netadr_t *from, qint burst, qint period, qint now);
const void
SVC_RateRestoreToxicAddress(const netadr_t *from, qint burst, qint period, qint now);
const void
SVC_RateDropAddress(const netadr_t *from, qint burst, qint period, qint now);
#if defined(SUPPORT_STATUS_SCORES_OVERRIDE)
void
SV_HandleGameInfoMessage(const qchar *info);
void
SV_StatusScoresOverride_Reset(void);
qint
SV_StatusScoresOverride_AdjustScore(qint defaultScore, qint clientNum);
#endif
#if defined(GAMESTATE_OVERFLOW_FIX)
void
SV_CalculateMaxBaselines(client_t *client, msg_t msg);
#endif
void QDECL
SV_SendServerCommand(client_t *cl, const qchar *fmt, ...);
void
SV_MasterShutdown(void);
void
SV_MasterGameStat(const qchar *data);
qint
SV_RateMsec(const client_t *client);

//sv_webconsole.c
#if defined(USE_WEBCONSOLE)
qbool
sv_webconsole_connect(qint *sockfd);
void
sv_webconsole_send(qint *sockfd, qchar *message, qbool *connected);
void
sv_webconsole_close(qint *sockfd);
const qchar *
sv_webconsole_read(qint *sockfd, qbool *connected);
#endif

//
//sv_init.c
//
const void
SV_SetConfigstring(const qint index, const qchar *val);
const void
SV_GetConfigstring(const qint index, qchar *buffer, const qint bufferSize);
const void
SV_UpdateConfigstrings(client_t *client);
const void
SV_SetUserinfo(const qint index, const qchar *val);
const void
SV_GetUserinfo(const qint index, qchar *buffer, const qint bufferSize);
const void
SV_SpawnServer(const qchar *server, qbool killBots);
#if defined(DEDICATED)
const void
SV_WriteAttackLog(const qchar *log);
const void
SV_WriteAttackLogUnrestricted(const qchar *log);

#if defined(NDEBUG)
#define SV_WriteAttackLogD(x)
#define SV_WriteAttackLogUnrestrictedD(x)
#else
#define SV_WriteAttackLogD(x) SV_WriteAttackLog(x)
#define SV_WriteAttackLogUnrestrictedD(x) SV_WriteAttackLogUnrestricted(x)
#endif
#endif

#if defined(STATELESS_CHALLENGES_VERSION_ONE)
//
//sv_challenge.c
//
const void
SV_ChallengeInit(void);
const void
SV_ChallengeShutdown(void);
const qint
SV_CreateChallenge(const netadr_t *from);
const qbool
SV_VerifyChallenge(const qint receivedChallenge, const netadr_t *from);
#endif

//
//sv_client.c
//
void
SV_GetChallenge(const netadr_t *from);
const void
SV_InitChallenger(void);
void
SV_DirectConnect(const netadr_t *from);
void
SV_SetClientState(client_t *cl, const clientState_t newState);
void
SV_ExecuteClientMessage(client_t *cl, msg_t *msg);
void
SV_UserinfoChanged(client_t *cl, const qbool updateUserinfo, const qbool runFilter);
void
SV_UpdateUserinfo_f(client_t *cl);
void
SV_ClientEnterWorld(client_t *client);
const void
SV_FreeClient(client_t *client);
void
SV_DropClient(client_t *drop, const qchar *reason);
void
SV_SendClientGameState(client_t *client);
qbool
SV_ExecuteClientCommand(client_t *cl, const qchar *s);
void
SV_ClientThink(client_t *cl, const usercmd_t *cmd);
const qint
SV_SendDownloadMessages(void);
const qint
SV_SendQueuedMessages(void);

void
SV_FreeIP4DB(void);
void
SV_PrintLocations_f(client_t *client);

#if defined(USE_VOIP)
void
SV_WriteVoipToClient(client_t *cl, msg_t *msg);
#endif

//
//sv_cvars.c
//
const void
SV_InitCvars(void);

//
//sv_ccmds.c
//
void
SV_AddOperatorCommands(void);
void
SV_RemoveOperatorCommands(void);
void
SV_Heartbeat_f(void);
client_t *
SV_GetPlayerByHandle(void);
const void
SV_UptimeReset(void);

//
//sv_snapshot.c
//
void
SV_AddServerCommand(client_t *client, const qchar *cmd);
void
SV_UpdateServerCommandsToClient(client_t *client, msg_t *msg);
void
SV_WriteFrameToClient (client_t *client, msg_t *msg);
void
SV_SendMessageToClient(msg_t *msg, client_t *client);
void
SV_SendClientMessages(void);
void
SV_SendClientSnapshot(client_t *client);
void
SV_CheckClientUserinfoTimer(void);
void
SV_InitSnapshotStorage(void);
void
SV_IssueNewSnapshot(void);
qint
SV_RemainingGameState(void);

//
// sv_demo.c
//
void
SV_DemoStartRecord(void);
void
SV_DemoStopRecord(void);
void
SV_DemoStartPlayback(void);
void
SV_DemoStopPlayback(void);
void
SV_DemoReadFrame(void);
void
SV_DemoWriteFrame(void);
void
SV_DemoWriteServerCommand(const qchar *str);
void
SV_DemoWriteGameCommand(qint cmd, const qchar *str);

//
// sv_filter.c
//
void
SV_LoadFilters(const qchar *filename);
const qchar *
SV_RunFilters(const qchar *userinfo, const netadr_t *addr);
void
SV_AddFilter_f(void);
void
SV_AddFilterCmd_f(void);

//
// sv_game.c
//
qint
SV_NumForGentity(sharedEntity_t *ent);
sharedEntity_t *
SV_GentityNum(qint num);
playerState_t *
SV_GameClientNum(qint num);
svEntity_t *
SV_SvEntityForGentity(sharedEntity_t *gEnt);
sharedEntity_t *
SV_GEntityForSvEntity(svEntity_t *svEnt);
void
SV_InitGameProgs(void);
void
SV_ShutdownGameProgs(void);
void
SV_RestartGameProgs(void);

#if defined(USE_JAVA)
void
Java_G_InitGame(qint levelTime, qint randomSeed, qbool restart);
void
Java_G_ShutdownGame(qbool restart);
qchar *
Java_G_ClientConnect(qint clientNum, qbool firstTime, qbool isBot);
void
Java_G_ClientBegin(qint clientNum);
void
Java_G_ClientUserInfoChanged(qint clientNum);
void
Java_G_ClientDisconnect(qint clientNum);
void
Java_G_ClientCommand(qint clientNum);
void
Java_G_ClientThink(qint clientNum);
void
Java_G_RunFrame(qint time);
void
Java_G_RunAIFrame(qint time);
qbool
Java_G_ConsoleCommand(void);
#endif

//
// sv_bot.c
//
void
SV_BotFrame(qint time);
qint
SV_BotAllocateClient(void);
void
SV_BotFreeClient(qint clientNum);
qint
SV_BotGetSnapshotEntity(qint client, qint ent);
qint
SV_BotGetConsoleMessage(qint client, qchar *buf, qint size);
void
SV_BotClientCommand(qint client, const qchar *command);

//============================================================
//
//high level object sorting to reduce interaction tests
//

void
SV_ClearWorld(void);
//called after the world model has been loaded, before linking any entities

void
SV_UnlinkEntity(sharedEntity_t *ent);
//call before removing an entity, and before trying to move one, so it doesn't clip against itself

void
SV_LinkEntity(sharedEntity_t *ent);
//Needs to be called any time an entity changes origin, mins, maxs, or solid. Automatically unlinks if needed. sets ent->v.absmin and ent->v.absmax sets ent->leafnums[] for pvs determination even if the entity is not solid

clipHandle_t
SV_ClipHandleForEntity(const sharedEntity_t *ent);

void
SV_SectorList_f(void);

qint
SV_AreaEntities(const vec3_t mins, const vec3_t maxs, qint *entityList, qint maxcount);
//fills in a table of entity numbers with entities that have bounding boxes that intersect the given area. It is possible for a non-axial bmodel to be returned that doesn't actually intersect the area on an exact test. returns the number of pointers filled in The world entity is never returned in this list.

qint
SV_PointContents(const vec3_t p, qint passEntityNum);
//returns the CONTENTS_* value from the world and all entities at the given point.


void
SV_Trace(trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, qint passEntityNum, qint contentmask, const traceType_t type);
//mins and maxs are relative if the entire move stays in a solid volume, trace.allsolid will be set, trace.startsolid will be set, and trace.fraction will be 0 if the starting point is in a solid, it will be allowed to move out to an open area passEntityNum is explicitly excluded from clipping checks (normally ENTITYNUM_NONE)

void
SV_ClipToEntity(trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, qint entityNum, qint contentmask, const traceType_t type);
//clip to a specific entity

//
//sv_net_chan.c
//
void
SV_Netchan_Transmit(client_t *client, msg_t *msg);
void
SV_Netchan_FreeQueue(client_t *client);
qint
SV_Netchan_TransmitNextFragment(client_t *client);
qbool
SV_Netchan_Process(client_t *client, msg_t *msg);
