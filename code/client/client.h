/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// client.h -- primary header for client

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../renderercommon/tr_public.h"
#include "../qcommon/vm_local.h"
#include "../ui/ui_public.h"
#include "../cgame/cg_public.h"
#include "../game/bg_public.h"
#include "snd_public.h"
#include "keys.h"

#ifdef USE_CURL
#include "cl_curl.h"
#endif

// file full of random crap that gets used to create cl_guid
#define QKEY_FILE "qkey"
#define QKEY_SIZE 2048

#define	RETRANSMIT_TIMEOUT	3000	// time between connection packet retransmits

// snapshots are a view of the server at a given time
typedef struct {
	qbool		valid;			// cleared if delta parsing was invalid
	qint				snapFlags;		// rate delayed and dropped commands

	qint				serverTime;		// server time the message is valid for (in msec)

	qint				messageNum;		// copied from netchan->incoming_sequence
	qint				deltaNum;		// messageNum the delta is from
	qint				ping;			// time from when cmdNum-1 was sent to time packet was reeceived
	qint				areabytes;
	byte			areamask[MAX_MAP_AREA_BYTES];		// portalarea visibility bits

	qint				cmdNum;			// the next cmdNum the server is expecting
	playerState_t	ps;						// complete information about the current player at this time

	qint				numEntities;			// all of the entities that need to be presented
	qint				parseEntitiesNum;		// at the time of this snapshot

	qint				serverCommandNum;		// execute all commands up to this before
											// making the snapshot current
} clSnapshot_t;



/*
=============================================================================

the clientActive_t structure is wiped completely at every
new gamestate_t, potentially several times during an established connection

=============================================================================
*/

typedef struct {
	qint		p_cmdNumber;		// cl.cmdNumber when packet was sent
	qint		p_serverTime;		// usercmd->serverTime when packet was sent
	qint		p_realtime;			// cls.realtime when packet was sent
} outPacket_t;

// the parseEntities array must be large enough to hold PACKET_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original 
#define	MAX_PARSE_ENTITIES	( PACKET_BACKUP * MAX_SNAPSHOT_ENTITIES )

extern qint g_console_field_width;

typedef struct {
	qint			timeoutcount;		// it requres several frames in a timeout condition
									// to disconnect, preventing debugging breaks from
									// causing immediate disconnects on continue
	clSnapshot_t	snap;			// latest received from server

	qint			serverTime;			// may be paused during play
	qint			oldServerTime;		// to prevent time from flowing bakcwards
	qint			oldFrameServerTime;	// to check tournament restarts
	qint			serverTimeDelta;	// cl.serverTime = cls.realtime + cl.serverTimeDelta
									// this value changes as net lag varies
	qbool	extrapolatedSnapshot;	// set if any cgame frame has been forced to extrapolate
									// cleared when CL_AdjustTimeDelta looks at it
	qbool	newSnapshots;		// set on parse of any valid packet

	gameState_t	gameState;			// configstrings
	qchar		mapname[MAX_QPATH];	// extracted from CS_SERVERINFO

	qint			parseEntitiesNum;	// index (not anded off) into cl_parse_entities[]

	qint			mouseDx[2], mouseDy[2];	// added to by mouse events
	qint			mouseIndex;
	qint			joystickAxis[MAX_JOYSTICK_AXIS];	// set by joystick events

	// cgame communicates a few values to the client system
	qint			cgameUserCmdValue;	// current weapon to add to usercmd_t
	float		cgameSensitivity;

	// cmds[cmdNumber] is the predicted command, [cmdNumber-1] is the last
	// properly generated command
	usercmd_t	cmds[CMD_BACKUP];	// each message will send several old cmds
	qint			cmdNumber;			// incremented each frame, because multiple
									// frames may need to be packed into a single packet

	outPacket_t	outPackets[PACKET_BACKUP];	// information about each packet we have sent out

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects,
	// and teleport direction changes
	vec3_t		viewangles;

	qint			serverId;			// included in each client message so the server
												// can tell if it is for a prior map_restart
	// big stuff at end of structure so most offsets are 15 bits or less
	clSnapshot_t	snapshots[PACKET_BACKUP];

	entityState_t	entityBaselines[MAX_GENTITIES];	// for delta compression when not in previous frame

	entityState_t	parseEntities[MAX_PARSE_ENTITIES];

	byte			baselineUsed[MAX_GENTITIES];
} clientActive_t;

extern	clientActive_t		cl;

#define EM_GAMESTATE 1
#define EM_SNAPSHOT  2
#define EM_COMMAND   4

/*
=============================================================================

the clientConnection_t structure is wiped when disconnecting from a server,
either to go to a full screen console, play a demo, or connect to a different server

A connection can be to either a server through the network layer or a
demo through a file.

=============================================================================
*/

typedef struct {

	qint			clientNum;
	qint			lastPacketSentTime;			// for retransmits during connection
	qint			lastPacketTime;				// for timeouts

	netadr_t	serverAddress;
	qint			connectTime;				// for connection retransmits
	qint			connectPacketCount;			// for display on connection dialog
	qchar		serverMessage[MAX_STRING_CHARS]; // for display on connection dialog

	qint			challenge;					// from the server to use for connecting
	qint			checksumFeed;				// from the server for checksum calculations

	// these are our reliable messages that go to the server
	qint			reliableSequence;
	qint			reliableAcknowledge;		// the last one the server has executed
	qchar		reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];

	// server message (unreliable) and command (reliable) sequence
	// numbers are NOT cleared at level changes, but continue to
	// increase as long as the connection is valid

	// message sequence is used by both the network layer and the
	// delta compression layer
	qint			serverMessageSequence;

	// reliable messages received from server
	qint			serverCommandSequence;
	qint			lastExecutedServerCommand;		// last server command grabbed or executed with CL_GetServerCommand
	qchar		serverCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
	qbool	serverCommandsIgnore[MAX_RELIABLE_COMMANDS];

	// file transfer from server
	fileHandle_t download;
	qchar		downloadName[MAX_OSPATH];
	qchar		downloadTempName[MAX_OSPATH + 4]; // downloadName + ".tmp"
	qint			sv_allowDownload;
	qchar		sv_dlURL[MAX_CVAR_VALUE_STRING];
	qint			downloadNumber;
	qint			downloadBlock;	// block we are waiting for
	qint			downloadCount;	// how many bytes we got
	qint			downloadSize;	// how many bytes we got
	qchar		downloadList[BIG_INFO_STRING]; // list of paks we need to download
	qbool	downloadRestart;	// if true, we need to do another FS_Restart because we downloaded a pak

#ifdef USE_CURL
	qbool	cURLEnabled;
	qbool	cURLUsed;
	qbool	cURLDisconnected;
	qchar		downloadURL[MAX_OSPATH];
	CURL		*downloadCURL;
	CURLM		*downloadCURLM;
#endif /* USE_CURL */

	// demo information
	qchar		demoName[MAX_OSPATH];
	qchar		recordName[MAX_OSPATH]; // without extension
	qbool	explicitRecordName;
	qchar		recordNameShort[TRUNCATE_LENGTH]; // for recording message
	qbool	dm68compat;
	qbool	spDemoRecording;
	qbool	demorecording;
	qbool	demoplaying;
	qbool	demowaiting;	// don't record until a non-delta message is received
	qbool	firstDemoFrameSkipped;
	fileHandle_t	demofile;
	fileHandle_t	recordfile;

	qint		timeDemoFrames;		// counter of rendered frames
	qint		timeDemoStart;		// cls.realtime before first frame
	qint		timeDemoBaseTime;	// each frame will be at this time + frameNum * 50

	float	aviVideoFrameRemainder;
	float	aviSoundFrameRemainder;
	qint		aviFrameEndTime;
	qchar	videoName[MAX_QPATH];
	qint		videoIndex;

	// big stuff at end of structure so most offsets are 15 bits or less
	netchan_t	netchan;

	qbool compat;

	// simultaneous demo playback and recording
	qint		eventMask;
	qint		demoCommandSequence;
	qint		demoDeltaNum;
	qint		demoMessageSequence;

} clientConnection_t;

extern	clientConnection_t clc;

/*
==================================================================

the clientStatic_t structure is never wiped, and is used even when
no client connection is active at all

==================================================================
*/

typedef struct {
	netadr_t	adr;
	qint			start;
	qint			time;
	qchar		info[MAX_INFO_STRING];
} ping_t;

typedef struct {
	netadr_t	adr;
	qchar	  	hostName[MAX_NAME_LENGTH];
	qchar	  	mapName[MAX_NAME_LENGTH];
	qchar	  	game[MAX_NAME_LENGTH];
	qint			netType;
	qint			gameType;
	qint		  	clients;
	qint		  	maxClients;
	qint			minPing;
	qint			maxPing;
	qint			ping;
	qbool	visible;
	qint			punkbuster;
	qint			g_humanplayers;
	qint			g_needpass;
} serverInfo_t;

typedef struct {
	connstate_t	state;				// connection status
	qbool	gameSwitch;

	qbool	cddialog;			// bring up the cd needed dialog next frame

	qchar		servername[MAX_OSPATH];		// name of server from original connect (used by reconnect)

	// when the server clears the hunk, all of these must be restarted
	qbool	rendererStarted;
	qbool	soundStarted;
	qbool	soundRegistered;
	qbool	uiStarted;
	qbool	cgameStarted;

	qint			framecount;
	qint			frametime;			// msec since last frame

	qint			realtime;			// ignores pause
	qint			realFrametime;		// ignoring pause, so console always works

	qint			numlocalservers;
	serverInfo_t	localServers[MAX_OTHER_SERVERS];

	qint			numglobalservers;
	serverInfo_t  globalServers[MAX_GLOBAL_SERVERS];
	// additional global servers
	qint			numGlobalServerAddresses;
	netadr_t		globalServerAddresses[MAX_GLOBAL_SERVERS];

	qint			numfavoriteservers;
	serverInfo_t	favoriteServers[MAX_OTHER_SERVERS];

	qint pingUpdateSource;		// source currently pinging or updating

	// update server info
	netadr_t	updateServer;
	qchar		updateChallenge[MAX_TOKEN_CHARS];
	qchar		updateInfoString[MAX_INFO_STRING];

	netadr_t	authorizeServer;

	// rendering info
	glconfig_t	glconfig;
	qhandle_t	charSetShader;
	qhandle_t	whiteShader;
	qhandle_t	consoleShader;

	qint			lastVidRestart;
	qint			soundMuted;

	qbool	startCgame;

	qint			captureWidth;
	qint			captureHeight;

	float		con_factor;

	float		scale;
	float		biasX;
	float		biasY;

} clientStatic_t;

extern qint bigchar_width;
extern qint bigchar_height;
extern qint smallchar_width;
extern qint smallchar_height;

extern	clientStatic_t		cls;

extern	qchar		cl_oldGame[MAX_QPATH];
extern	qbool	cl_oldGameSet;

#ifdef USE_CURL

extern		download_t	download;
qbool	Com_DL_Perform( download_t *dl );
void		Com_DL_Cleanup( download_t *dl );
qbool	Com_DL_Begin( download_t *dl, const qchar *localName, const qchar *remoteURL, qbool autoDownload );
qbool	Com_DL_InProgress( const download_t *dl );
qbool	Com_DL_ValidFileName( const qchar *fileName );
qbool	CL_Download( const qchar *cmd, const qchar *pakname, qbool autoDownload );

#endif

//=============================================================================

extern	vm_t			*cgvm;	// interface to cgame dll or vm
extern	vm_t			*uivm;	// interface to ui dll or vm
extern	refexport_t		re;		// interface to refresh .dll


//
// cvars
//
extern	cvar_t	*cl_noprint;
extern	cvar_t	*cl_debugMove;
extern	cvar_t	*cl_timegraph;
extern	cvar_t	*cl_shownet;
extern	cvar_t	*cl_autoNudge;
extern	cvar_t	*cl_timeNudge;
extern	cvar_t	*cl_showTimeDelta;

extern	cvar_t	*com_timedemo;
extern	cvar_t	*cl_aviFrameRate;
extern	cvar_t	*cl_aviMotionJpeg;
extern	cvar_t	*cl_aviPipeFormat;

extern	cvar_t	*cl_activeAction;

extern	cvar_t	*cl_allowDownload;
#ifdef USE_CURL
extern	cvar_t	*cl_mapAutoDownload;
extern	cvar_t	*cl_dlDirectory;
#endif
extern	cvar_t	*cl_conXOffset;
extern	cvar_t	*cl_conColor;
extern	cvar_t	*cl_inGameVideo;

extern	cvar_t	*cl_lanForcePackets;
extern	cvar_t	*cl_autoRecordDemo;
extern	cvar_t	*cl_drawRecording;

extern	cvar_t	*com_maxfps;

extern	cvar_t	*vid_xpos;
extern	cvar_t	*vid_ypos;
extern	cvar_t	*r_noborder;

extern	cvar_t	*r_allowSoftwareGL;
extern	cvar_t	*r_swapInterval;
extern	cvar_t	*r_glDriver;

extern	cvar_t	*r_displayRefresh;
extern	cvar_t	*r_fullscreen;
extern	cvar_t	*r_mode;
extern	cvar_t	*r_modeFullscreen;
extern	cvar_t	*r_customwidth;
extern	cvar_t	*r_customheight;
extern	cvar_t	*r_customPixelAspect;
extern	cvar_t	*r_colorbits;
extern	cvar_t	*cl_stencilbits;
extern	cvar_t	*cl_depthbits;
extern	cvar_t	*cl_drawBuffer;

//=================================================

//
// cl_main
//
void CL_AddReliableCommand( const qchar *cmd, qbool isDisconnectCmd );

void CL_StartHunkUsers( void );

void CL_Disconnect_f( void );
void CL_ReadDemoMessage( void );
demoState_t CL_DemoState( void );
qint CL_DemoPos( void );
void CL_DemoName( qchar *buffer, qint size );
void CL_StopRecord_f( void );

void CL_InitDownloads( void );
void CL_NextDownload( void );

void CL_GetPing( qint n, qchar *buf, qint buflen, qint *pingtime );
void CL_GetPingInfo( qint n, qchar *buf, qint buflen );
void CL_ClearPing( qint n );
qint CL_GetPingQueueCount( void );

void CL_ClearState( void );

qint CL_ServerStatus( const qchar *serverAddress, qchar *serverStatusString, qint maxLen );

qbool CL_CheckPaused( void );
qbool CL_NoDelay( void );

qbool CL_GetModeInfo( qint *width, qint *height, float *windowAspect, qint mode, const qchar *modeFS, qint dw, qint dh, qbool fullscreen );


//
// cl_input
//
void CL_InitInput( void );
void CL_ClearInput( void );
void CL_SendCmd( void );
void CL_WritePacket( qint repeat );

//
// cl_keys.c
//
extern  field_t     chatField;
extern  field_t     g_consoleField;

void Field_Draw( field_t *edit, qint x, qint y, qint width, qbool showCursor, qbool noColorEscape );
void Field_BigDraw( field_t *edit, qint x, qint y, qint width, qbool showCursor, qbool noColorEscape );

//
// cl_parse.c
//
extern qint cl_connectedToPureServer;
extern qint cl_connectedToCheatServer;

void CL_ParseServerMessage( msg_t *msg );

//====================================================================

qbool CL_UpdateVisiblePings_f( qint source );
qbool CL_ValidPakSignature( const byte *data, qint len );


//
// console
//

extern cvar_t *con_scale;

void Con_CheckResize( void );
void Con_Init( void );
void Con_Shutdown( void );
void Con_ToggleConsole_f( void );
void Con_ClearNotify( void );
void Con_RunConsole( void );
void Con_DrawConsole( void );
void Con_PageUp( qint lines );
void Con_PageDown( qint lines );
void Con_Top( void );
void Con_Bottom( void );
void Con_Close( void );

void CL_LoadConsoleHistory( void );
void CL_SaveConsoleHistory( void );

//
// cl_scrn.c
//
void	SCR_Init( void );
void	SCR_Done( void );
void	SCR_UpdateScreen( void );

void	SCR_DebugGraph( float value );

qint		SCR_GetBigStringWidth( const qchar *str );	// returns in virtual 640x480 coordinates

void	SCR_AdjustFrom640( float *x, float *y, float *w, float *h );
void	SCR_FillRect( float x, float y, float width, float height, 
					 const float *color );
void	SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader );
void	SCR_DrawNamedPic( float x, float y, float width, float height, const qchar *picname );

void	SCR_DrawBigString( qint x, qint y, const qchar *s, float alpha, qbool noColorEscape );			// draws a string with embedded color control characters with fade
void	SCR_DrawStringExt( qint x, qint y, float size, const qchar *string, const float *setColor, qbool forceColor, qbool noColorEscape );
void	SCR_DrawSmallStringExt( qint x, qint y, const qchar *string, const float *setColor, qbool forceColor, qbool noColorEscape );
void	SCR_DrawSmallChar( qint x, qint y, qint ch );
void	SCR_DrawSmallString( qint x, qint y, const qchar *s, qint len );

//
// cl_cin.c
//

void CL_PlayCinematic_f( void );
void SCR_DrawCinematic (void);
void SCR_RunCinematic (void);
void SCR_StopCinematic (void);
qint CIN_PlayCinematic( const qchar *arg0, qint xpos, qint ypos, qint width, qint height, qint bits);
e_status CIN_StopCinematic(qint handle);
e_status CIN_RunCinematic (qint handle);
void CIN_DrawCinematic (qint handle);
void CIN_SetExtents (qint handle, qint x, qint y, qint w, qint h);
void CIN_UploadCinematic(qint handle);
void CIN_CloseAllVideos(void);

//
// cl_cgame.c
//
void CL_InitCGame( void );
void CL_ShutdownCGame( void );
qbool CL_GameCommand( void );
void CL_GameConsoleText( void );
void CL_CGameRendering( stereoFrame_t stereo );
void CL_SetCGameTime( void );

//
// cl_ui.c
//
void CL_InitUI( void );
void CL_ShutdownUI( void );
qint Key_GetCatcher( void );
void Key_SetCatcher( qint catcher );


//
// cl_net_chan.c
//
void CL_Netchan_Transmit( netchan_t *chan, msg_t *msg );
void CL_Netchan_Enqueue( netchan_t *chan, msg_t *msg, qint times );
qbool CL_Netchan_Process( netchan_t *chan, msg_t *msg );

//
// cl_avi.c
//
qbool CL_OpenAVIForWriting( const qchar *filename, qbool pipe, qbool reopen );
void CL_TakeVideoFrame( void );
void CL_WriteAVIVideoFrame( const byte *imageBuffer, qint size );
void CL_WriteAVIAudioFrame( const byte *pcmBuffer, qint size );
qbool CL_CloseAVI( qbool reopen );
qbool CL_VideoRecording( void );

//
// cl_jpeg.c
//
size_t	CL_SaveJPGToBuffer( byte *buffer, size_t bufSize, qint quality, qint image_width, qint image_height, byte *image_buffer, qint padding );
void	CL_SaveJPG( const qchar *filename, qint quality, qint image_width, qint image_height, byte *image_buffer, qint padding );
void	CL_LoadJPG( const qchar *filename, unsigned qchar **pic, qint *width, qint *height );


// base backend functions
void	HandleEvents( void );

// platform-specific
void	GLimp_InitGamma(glconfig_t *config);
void	GLimp_SetGamma(unsigned qchar red[256], unsigned qchar green[256], unsigned qchar blue[256]);

// OpenGL
#ifdef USE_OPENGL_API
void	GLimp_Init( glconfig_t *config );
void	GLimp_Shutdown( qbool unloadDLL );
void	GLimp_EndFrame( void );
void	*GL_GetProcAddress( const qchar *name );
#endif

// Vulkan
#ifdef USE_VULKAN_API
void	VKimp_Init( glconfig_t *config );
void	VKimp_Shutdown( qbool unloadDLL );
void	*VK_GetInstanceProcAddr( VkInstance instance, const qchar *name );
qbool VK_CreateSurface( VkInstance instance, VkSurfaceKHR* pSurface );
#endif
