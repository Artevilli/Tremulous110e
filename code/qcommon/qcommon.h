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
// qcommon.h -- definitions common between client and server, but not game.or ref modules
#pragma once

#include "../qcommon/cm_public.h"

//Ignore __attribute__ on non-gcc platforms
#if !defined(__GNUC__)
#if !defined(__attribute__)
#define __attribute__(x)
#endif
#endif

/* C99 defines __func__ */
#if __STDC_VERSION__ < 199901L
#if __GNUC__ >= 2 || _MSC_VER >= 1300
#define __func__ __FUNCTION__
#else
#define __func__ "(unknown)"
#endif
#endif

#if defined(_WIN32) || defined(__linux__)
#define USE_AFFINITY_MASK
#endif

//stringify macro
#define XSTRING(x) STRING(x)
#define STRING(x) #x

#define DELAY_WRITECONFIG

//============================================================================

//
// msg.c
//
typedef struct {
	qbool	overflowed;		// set to true if the buffer size failed
	qbool	oob;			//raw out-of-band operation, no static huffman encoding/decoding
	byte	*data;
	qint		maxsize;
	qint maxbits; //maxsize in bits, for overflow checks
	qint		cursize;
	qint uncompsize;
	qint		readcount;
	qint		bit;				// for bitwise reads and writes
} msg_t;

void MSG_Init (msg_t *buf, byte *data, qint length);
void MSG_InitOOB( msg_t *buf, byte *data, qint length );
void MSG_Clear (msg_t *buf);
void MSG_WriteData (msg_t *buf, const void *data, qint length);
void MSG_Bitstream( msg_t *buf );

// TTimo
// copy a msg_t in case we need to store it as is for a bit
// (as I needed this to keep an msg_t from a static var for later use)
// sets data buffer as MSG_Init does prior to do the copy
void MSG_Copy(msg_t *buf, byte *data, qint length, const msg_t *src);

struct usercmd_s;
struct entityState_s;
struct playerState_s;

void MSG_WriteBits( msg_t *msg, qint value, qint bits );

void MSG_WriteChar (msg_t *sb, qint c);
void MSG_WriteByte (msg_t *sb, qint c);
void MSG_WriteShort (msg_t *sb, qint c);
void MSG_WriteLong (msg_t *sb, qint c);
void MSG_WriteFloat (msg_t *sb, float f);
void MSG_WriteString (msg_t *sb, const qchar *s);
void MSG_WriteBigString (msg_t *sb, const qchar *s);
void MSG_WriteAngle16 (msg_t *sb, float f);
qint
MSG_HashKey(const qchar *string, const qint maxlen);

void	MSG_BeginReading (msg_t *sb);
void	MSG_BeginReadingOOB(msg_t *sb);

qint
MSG_ReadBits(msg_t *msg, qint bits);

qint		MSG_ReadChar (msg_t *sb);
qint		MSG_ReadByte (msg_t *sb);
qint		MSG_ReadShort (msg_t *sb);
qint		MSG_ReadLong (msg_t *sb);
float	MSG_ReadFloat (msg_t *sb);
const qchar *MSG_ReadString (msg_t *sb);
const qchar *MSG_ReadBigString (msg_t *sb);
const qchar *MSG_ReadStringLine (msg_t *sb);
float	MSG_ReadAngle16 (msg_t *sb);
void	MSG_ReadData (msg_t *sb, void *buffer, qint size);
qint
MSG_ReadEntitynum(msg_t *sb);

void MSG_WriteDeltaUsercmdKey( msg_t *msg, qint key, const usercmd_t *from, usercmd_t *to );
void MSG_ReadDeltaUsercmdKey( msg_t *msg, qint key, const usercmd_t *from, usercmd_t *to );

void MSG_WriteDeltaEntity( msg_t *msg, const entityState_t *from, const entityState_t *to
						   , qbool force );
void MSG_ReadDeltaEntity( msg_t *msg, const entityState_t *from, entityState_t *to, 
						 qint number );

void
MSG_WriteDeltaSharedEntity(msg_t *msg, void *from, void *to, qbool force, qint number);
void
MSG_ReadDeltaSharedEntity(msg_t *msg, void *from, void *to, qint number);
void MSG_WriteDeltaPlayerstate( msg_t *msg, const playerState_t *from, const playerState_t *to );
void MSG_ReadDeltaPlayerstate( msg_t *msg, const playerState_t *from, playerState_t *to );


void MSG_ReportChangeVectors_f( void );

//============================================================================

/*
==============================================================

NET

==============================================================
*/
//#define USE_IPV6
#define NET_ENABLEV4            0x01
#define NET_ENABLEV6            0x02
//if this flag is set, always attempt ipv6 connections instead of ipv4 if a v6 address is found.
#define NET_PRIOV6              0x04
//disables ipv6 multicast support if set.
#define NET_DISABLEMCAST        0x08

#define	PACKET_BACKUP	32	// number of old messages that must be kept on client and
							// server for delta comrpession and ping estimation
#define	PACKET_MASK		(PACKET_BACKUP-1)

#define	MAX_PACKET_USERCMDS		32		// max number of usercmd_t in a packet

#define MAX_SNAPSHOT_ENTITIES 256
#define	PORT_ANY			-1

#define	MAX_RELIABLE_COMMANDS	128			// max string commands buffered for restransmit

#define	MAX_PACKETLEN			1400		// max size of a network packet

#define	FRAGMENT_SIZE			(MAX_PACKETLEN - 100)
#define	PACKET_HEADER			10			// two ints and a short

#define	FRAGMENT_BIT	(1U<<31)

typedef enum {
	NA_BAD = 0,					// an address lookup failed
	NA_BOT,
	NA_LOOPBACK,
	NA_BROADCAST,
	NA_IP,
#if defined(USE_IPV6)
	NA_IP6,
	NA_MULTICAST6,
#endif
	NA_UNSPEC
} netadrtype_t;

typedef enum {
	NS_CLIENT,
	NS_SERVER
} netsrc_t;

#define NET_ADDRSTRMAXLEN 48 //maximum length of an IPv6 address string including trailing '\0'
#define NET_ADDRSTRMAXLEN_EXT (NET_ADDRSTRMAXLEN + 8) //2 x brackets, colon, 5 x port
typedef struct {
	netadrtype_t	type;

	union
	{
	  byte _4[4];
#if defined(USE_IPV6)
	  byte _6[16];
#endif
	}
	ipv;

	uint16_t port;
#if defined(USE_IPV6)
	unsigned long scope_id; //needed for IPv6 link-local addresses
#endif
} netadr_t;

void		NET_Init( void );
void		NET_Shutdown( void );
void		NET_FlushPacketQueue(qint time_diff);
void
NET_QueuePacket(netsrc_t sock, qint length, const void *data, const netadr_t *to, qint offset);
void		NET_SendPacket (netsrc_t sock, qint length, const void *data, const netadr_t *to);
void		QDECL NET_OutOfBandPrint( netsrc_t net_socket, const netadr_t *adr, const qchar *format, ...) __attribute__ ((format (printf, 3, 4)));
void		QDECL NET_OutOfBandCompress( netsrc_t sock, const netadr_t *adr, const byte *data, qint len );

qbool	NET_CompareAdr(const netadr_t *a, const netadr_t *b);
qbool
NET_CompareBaseAdrMask(const netadr_t *a, const netadr_t *b, unsigned netmask);
qbool
NET_CompareBaseAdr(const netadr_t *a, const netadr_t *b);
qbool	NET_IsLocalAddress (const netadr_t *adr);
const qchar	*NET_AdrToString (const netadr_t *a);
const qchar      *NET_AdrToStringwPort (const netadr_t *a);
qint		NET_StringToAdr ( const qchar *s, netadr_t *a, netadrtype_t family);
#if !defined(DEDICATED)
qbool	NET_GetLoopPacket (netsrc_t sock, netadr_t *net_from, msg_t *net_message);
#endif
#if defined(USE_IPV6)
void		NET_JoinMulticast6(void);
void		NET_LeaveMulticast6(void);
#endif
qbool
NET_Sleep(qint timeout);


#define	MAX_MSGLEN 16384 //FIXME: HACK: should be 16384		// max length of a message, which may
											// be fragmented into multiple packets
#define MAX_MSGLEN_BUF (MAX_MSGLEN + 8) //real buffer size that we need to allocate to safely handle overflows

#define MAX_DOWNLOAD_WINDOW 48 //max of eight download frames
#define MAX_DOWNLOAD_BLKSIZE 1024 //4096 //2048 byte block chunks (FIXME: HACK: should be 1024)
#define NETCHAN_GENCHECKSUM(challenge, sequence) ((challenge) ^ ((sequence) * (challenge)))
 

/*
Netchan handles packet fragmentation and out of order / duplicate suppression
*/

typedef struct {
	netsrc_t	sock;

	qint			dropped;			// between last packet and previous

	netadr_t	remoteAddress;
	qint			qport;				// qport value to write when transmitting

	// sequencing variables
	qint			incomingSequence;
	qint			outgoingSequence;

	// incoming fragment assembly buffer
	qint			fragmentSequence;
	qint			fragmentLength;	
	byte		fragmentBuffer[MAX_MSGLEN];

	// outgoing fragment buffer
	// we need to space out the sending of large fragmented messages
	qbool	unsentFragments;
	qint			unsentFragmentStart;
	qint			unsentLength;
	byte		unsentBuffer[MAX_MSGLEN];

        qint challenge;
	qint lastSentTime;
	qint lastSentSize;

        qbool compat; //ioq3 extension
        qbool isLANAddress;
} netchan_t;

void Netchan_Init( qint qport );
void Netchan_Setup( const netsrc_t sock, netchan_t *chan, const netadr_t *adr, const qint port, const qint challenge, qbool compat );

void Netchan_Transmit( netchan_t *chan, qint length, const byte *data );
void Netchan_TransmitNextFragment( netchan_t *chan );
void
Netchan_Enqueue(netchan_t *chan, qint length, const byte *data);

qbool Netchan_Process( netchan_t *chan, msg_t *msg );


/*
==============================================================

PROTOCOL

==============================================================
*/

#define	OLD_PROTOCOL_VERSION	69
//new protocol with UDP spoofing protection:
#define	NEW_PROTOCOL_VERSION	72
//1.31 - 67

#define DEFAULT_PROTOCOL_VERSION OLD_PROTOCOL_VERSION

// maintain a list of compatible protocols for demo playing
// NOTE: that stuff only works with two digits protocols
extern const qint demo_protocols[];

// override on command line, config files etc.
#if !defined(MASTER_SERVER_NAME)
#define MASTER_SERVER_NAME	"master.tremulous.net"
#endif

#define	PORT_MASTER			30710
#define	PORT_SERVER			30720
#define	NUM_SERVER_PORTS	4		// broadcast scan this many ports after
									// PORT_SERVER so a single machine can
									// run multiple servers


// the svc_strings[] array in cl_parse.c should mirror this
//
// server to client
//
enum svc_ops_e {
	svc_bad,
	svc_nop,
	svc_gamestate,
	svc_configstring,			// [short] [string] only in gamestate messages
	svc_baseline,				// only in gamestate messages
	svc_serverCommand,			// [string] to be executed by client game module
	svc_download,				// [short] size [size bytes]
	svc_snapshot,
	svc_EOF,

	//new commands, supported only by ioquake3 protocol but not legacy
	svc_voipSpeex,     //not wrapped in USE_VOIP, so this value is reserved.
	svc_voipOpus,      //
};


//
// client to server
//
enum clc_ops_e {
	clc_bad,
	clc_nop, 		
	clc_move,				// [[usercmd_t]
	clc_moveNoDelta,		// [[usercmd_t]
	clc_clientCommand,		// [string] message
	clc_EOF,

	// clc_extension follows a clc_EOF, followed by another clc_* ...
	//  this keeps legacy servers compatible.
	clc_extension,
	clc_voip,   // not wrapped in USE_VOIP, so this value is reserved.
};

/*
==============================================================

VIRTUAL MACHINE

==============================================================
*/
typedef struct vm_s vm_t;

typedef enum
{
  VMI_NATIVE,
  VMI_BYTECODE,
  VMI_COMPILED
}
vmInterpret_t;

typedef enum
{
  TRAP_MEMSET = 100,
  TRAP_MEMCPY,
  TRAP_STRNCPY,
  TRAP_SIN,
  TRAP_COS,
  TRAP_ATAN2,
  TRAP_SQRT,
}
sharedTraps_t;

typedef enum
{
  VM_BAD = -1,
  VM_GAME = 0,
#if !defined(USE_DEDICATED)
  VM_CGAME,
  VM_UI,
#endif
  VM_COUNT
}
vmIndex_t;

//we don't need more than 4 arguments (counting callnum) for vmMain, at least in Tremulous
#define MAX_VMMAIN_CALL_ARGS 4

typedef intptr_t (QDECL *vmMainFunc_t)(qint command, qint arg0, qint arg1, qint arg2);

typedef intptr_t (*syscall_t)(intptr_t *parms);
typedef intptr_t (QDECL *dllSyscall_t)(intptr_t callNum, ...);
typedef void (QDECL *dllEntry_t)(dllSyscall_t syscallptr);

void
VM_Init(void);
vm_t *
VM_Create(vmIndex_t index, syscall_t systemCalls, dllSyscall_t dllSyscalls, vmInterpret_t interpret);

void
VM_Free(vm_t *vm);
void
VM_Clear(void);
void
VM_Forced_Unload_Start(void);
void
VM_Forced_Unload_Done(void);
vm_t *
VM_Restart(vm_t *vm);

intptr_t QDECL
VM_Call(vm_t *vm, qint nargs, qint callNum, ...);

void
VM_Debug(qint level);
void
VM_CheckBounds(const vm_t *vm, unsigned address, unsigned length);
void
VM_CheckBounds2(const vm_t *vm, unsigned addr1, unsigned addr2, unsigned length);

#if 1
#define VM_CHECKBOUNDS VM_CheckBounds
#define VM_CHECKBOUNDS2 VM_CheckBounds2
#else //for performance evaluation purposes
#define VM_CHECKBOUNDS(vm, a, b)
#define VM_CHECKBOUNDS2(vm, a, b, c)
#endif

void *
GVM_ArgPtr(intptr_t intValue);

#define	VMA(x) VM_ArgPtr(args[x])
static ID_INLINE float
_vmf(intptr_t x)
{
  floatint_t v;

  v.i = (qint)x;
  return v.f;
}
#define	VMF(x) _vmf(args[x])


/*
==============================================================

CMD

Command text buffering and command execution

==============================================================
*/

/*

Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but entire text
files can be execed.

*/

#define MAX_CMD_LINE 1024

void
Cbuf_Init(void);
//allocates an initial text buffer that will grow as needed

void
Cbuf_AddText(const qchar *text);
//adds command text at the end of the buffer, does NOT add a final \n

void
Cbuf_NestedAdd(const qchar *text);
//adds nested command text at the specified position of the buffer, adds \n when needed

void
Cbuf_NestedReset(void);
//resets nested cmd offset

void
Cbuf_InsertText(const qchar *text);
//adds command text at the beginning of the buffer, add \n

void
Cbuf_ExecuteText(cbufExec_t exec_when, const qchar *text);
//this can be used in place of either Cbuf_AddText or Cbuf_InsertText

void
Cbuf_Execute(void);
//pulls off \n terminated lines of text from the command buffer and sends
//them through Cmd_ExecuteString,  stops when the buffer is empty
//normally called once per frame, but may be explicitly invoked
//do not call inside a command function, or current args will be destroyed

void
Cbuf_Wait(void);
//checks if wait command timeout remaining

//===========================================================================

/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

*/

typedef void (*xcommand_t) (void);

void	Cmd_Init (void);

void	Cmd_AddCommand( const qchar *cmd_name, xcommand_t function );
// called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory
// if function is NULL, the command will be forwarded to the server
// as a clc_clientCommand instead of executed locally

void
Cmd_RemoveCommand(const qchar *cmd_name);
void
Cmd_RemoveCgameCommands(void);

typedef void
(*completionFunc_t)(const qchar *args, qint argNum);

//dont allow VMs to remove system commands
void
Cmd_RemoveCommandSafe(const qchar *cmd_name);

void
Cmd_CommandCompletion( void(*callback)(const qchar *s) );

// callback with each valid string
void
Cmd_SetCommandCompletionFunc(const qchar *command, completionFunc_t complete);
qbool
Cmd_CompleteArgument(const qchar *command, const qchar *args, qint argNum);
void
Cmd_CompleteWriteCfgName(const qchar *args, qint argNum);

qint
Cmd_Argc(void);
void
Cmd_Clear(void);
const qchar *
Cmd_Argv(qint arg);
void
Cmd_ArgvBuffer(qint arg, qchar *buffer, qint bufferLength);
qchar *
Cmd_Args(void);
qchar *
Cmd_ArgsFrom(qint arg);
void
Cmd_ArgsBuffer(qchar *buffer, qint bufferLength);
void
Cmd_LiteralArgsBuffer(qchar *buffer, qint bufferLength);
qchar *
Cmd_Cmd(void);
void
Cmd_Args_Sanitize(const qchar *separators);
// The functions that execute commands get their parameters with these
// functions. Cmd_Argv () will return an empty string, not a NULL
// if arg > argc, so string operations are allways safe.

void	Cmd_TokenizeString( const qchar *text );
void	Cmd_TokenizeStringIgnoreQuotes( const qchar *text_in );
// Takes a null terminated string.  Does not need to be /n terminated.
// breaks the string up into arg tokens.

void	Cmd_ExecuteString( const qchar *text );
// Parses a single line of text into arguments and tries to execute it
// as if it was typed at the console

void Cmd_SaveCmdContext( void );
void Cmd_RestoreCmdContext( void );

/*
==============================================================

CVAR

==============================================================
*/

/*

cvar_t variables are used to hold scalar or string variables that can be changed
or displayed at the console or prog code as well as accessed directly
in C code.

The user can access cvars from the console in three ways:
r_draworder			prints the current value
r_draworder 0		sets the current value to 0
set r_draworder 0	as above, but creates the cvar if not present

Cvars are restricted from having the same names as commands to keep this
interface from being ambiguous.

The are also occasionally used to communicated information between different
modules of the program.

*/

cvar_t *Cvar_Get( const qchar *var_name, const qchar *value, qint flags );
// creates the variable if it doesn't exist, or returns the existing one
// if it exists, the value will not be changed, but flags will be ORed in
// that allows variables to be unarchived without needing bitflags
// if value is "", the value will not override a previously set value.

void	Cvar_Register( vmCvar_t *vmCvar, const qchar *varName, const qchar *defaultValue, qint flags, qint privateFlag );
// basically a slightly modified Cvar_Get for the interpreted modules

void	Cvar_Update( vmCvar_t *vmCvar, qint privateFlag );
// updates an interpreted modules' version of a cvar

void 	Cvar_Set( const qchar *var_name, const qchar *value );
// will create the variable with no flags if it doesn't exist

cvar_t *
Cvar_Set2(const qchar *var_name, const qchar *value, qbool force);
//same as Cvar_Set, but allows more control over setting of cvar

void
Cvar_SetSafe(const qchar *var_name, const qchar *value);
//sometimes we set variables from an untrusted source: fail if flags & CVAR_PROTECTED

void Cvar_SetLatched( const qchar *var_name, const qchar *value);
// don't set the cvar immediately

void	Cvar_SetValue( const qchar *var_name, float value );
void
Cvar_SetIntegerValue(const qchar *var_name, qint value);
void
Cvar_SetValueSafe(const qchar *var_name, float value);
// expands value to a string and calls Cvar_Set/Cvar_SetSafe

qbool
Cvar_SetModified(const qchar *var_name, qbool modified);

float	Cvar_VariableValue( const qchar *var_name );
qint		Cvar_VariableIntegerValue( const qchar *var_name );
// returns 0 if not defined or non numeric

const qchar	*Cvar_VariableString( const qchar *var_name );
void	Cvar_VariableStringBuffer( const qchar *var_name, qchar *buffer, qint bufsize );
void
Cvar_VariableStringBufferSafe(const qchar *var_name, qchar *buffer, qint bufsize, qint flag);
// returns an empty string if not defined

unsigned	Cvar_Flags(const qchar *var_name);
// returns CVAR_NONEXISTENT if cvar doesn't exist or the flags of that particular CVAR.

void	Cvar_CommandCompletion( void(*callback)(const qchar *s) );
// callback with each valid string

void 	Cvar_Reset( const qchar *var_name );
void 	Cvar_ForceReset(const qchar *var_name);

void	Cvar_SetCheatState( void );
// reset all testing vars to a safe value

qbool Cvar_Command( void );
// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)

void 	Cvar_WriteVariables( fileHandle_t f );
// writes lines containing "set variable value" for all variables
// with the archive flag set to true.

void	Cvar_Init( void );

const qchar *
Cvar_InfoString(qint bit, qbool *truncated);
const qchar *
Cvar_InfoString_Big(qint bit, qbool *truncated);
// returns an info string containing all the cvars that have the given bit set
// in their flags (CVAR_USERINFO, CVAR_SERVERINFO, CVAR_SYSTEMINFO, etc)
void
Cvar_InfoStringBuffer(qint bit, qchar *buff, qint buffsize);
void
Cvar_CheckRange(cvar_t *cv, const qchar *minVal, const qchar *maxVal, cvarValidator_t type);
const void
Cvar_SetDescription(cvar_t *var, const qchar *var_description);
void
Cvar_SetDescription2(const qchar *var_name, const qchar *var_description);
cvar_t *
Cvar_GetAndDescribe(const qchar *varName, const qchar *value, const qint flags, const qchar *description);

void
Cvar_SetGroup(cvar_t *var, cvarGroup_t group);
qint
Cvar_CheckGroup(cvarGroup_t group);
void
Cvar_ResetGroup(cvarGroup_t group, qbool resetModifiedFlags);

void
Cvar_Restart(qbool unsetVM);

void
Cvar_CompleteCvarName(const qchar *args, qint argNum);

extern qint cvar_modifiedFlags;
// whenever a cvar is modifed, its flags will be OR'd into this, so
// a single check can determine if any CVAR_USERINFO, CVAR_SERVERINFO,
// etc, variables have been modified since the last check.  The bit
// can then be cleared to allow another change detection.

unsigned
crc32_buffer(const byte *buf, unsigned len);

/*
==============================================================

FILESYSTEM

No stdio calls should be used by any part of the game, because
we need to deal with all sorts of directory and seperator qchar
issues.
==============================================================
*/

// referenced flags
// these are in loop specific order so don't change the order
#define FS_GENERAL_REF	0x01
#define FS_UI_REF		0x02
#define FS_CGAME_REF	0x04

#define FS_MATCH_EXTERN BIT(0)
#define FS_MATCH_PURE BIT(1)
#define FS_MATCH_UNPURE BIT(2)
#define FS_MATCH_STICK BIT(3)
#define FS_MATCH_SUBDIRS BIT(4)
#define FS_MATCH_PK3s (FS_MATCH_PURE | FS_MATCH_UNPURE)
#define FS_MATCH_ANY (FS_MATCH_EXTERN | FS_MATCH_PURE | FS_MATCH_UNPURE)

#define FS_MAX_SUBDIRS 8 //should be enough for practical use with FS_MATCH_SUBDIRS

typedef enum
{
  H_SYSTEM,
  H_GAME,
  H_CGAME,
  H_UI
}
handleOwner_t;

#define	MAX_FILE_HANDLES	64

#define FS_INVALID_HANDLE 0

#define MAX_FOUND_FILES 0x5000

#if defined(DEDICATED)
#define Q3CONFIG_CFG "autogen_server.cfg"
#define CONSOLE_HISTORY_FILE "autogen_server_history"
#else
#define Q3CONFIG_CFG "autogen.cfg"
#define CONSOLE_HISTORY_FILE "autogen_history"
#endif

typedef time_t fileTime_t;
typedef off_t fileOffset_t;

qbool FS_Initialized( void );

void	FS_InitFilesystem ( void );
void	FS_Shutdown( qbool closemfp );

qbool	FS_ConditionalRestart( qint checksumFeed, qbool clientRestart );
void	FS_Restart( qint checksumFeed );
// shutdown and restart the filesystem so changes to fs_gamedir can take effect

void
FS_Reload(void);

void FS_AddGameDirectory( const qchar *path, const qchar *dir );

qchar	**FS_ListFiles( const qchar *directory, const qchar *extension, qint *numfiles );
// directory should not have either a leading or trailing /
// if extension is "/", only subdirectories will be returned
// the returned files will not include any directories or /

void	FS_FreeFileList( qchar **list );

qbool FS_FileExists( const qchar *file );

qbool
FS_CreatePath(const qchar *OSPath);

qchar *
FS_FindDll(const qchar *filename);

qchar   *FS_BuildOSPath( const qchar *base, const qchar *game, const qchar *qpath );
qbool
FS_CompareZipChecksum(const qchar *zipfile);
qint
FS_GetZipChecksum(const qchar *zipfile);

qint		FS_LoadStack( void );

qint		FS_GetFileList(  const qchar *path, const qchar *extension, qchar *listbuf, qint bufsize );

qint
FS_GetModList(qchar *listbuf, qint bufsize);

fileHandle_t	FS_FOpenFileWrite( const qchar *qpath );
qbool
FS_ResetReadOnlyAttribute(const qchar *filename);
qbool
FS_SV_FileExists(const qchar *file);
fileHandle_t	FS_FOpenFileAppend( const qchar *filename );
// will properly create any needed paths and deal with seperater character issues

fileHandle_t FS_SV_FOpenFileWrite( const qchar *filename );
qint		FS_SV_FOpenFileRead( const qchar *filename, fileHandle_t *fp );
void	FS_SV_Rename( const qchar *from, const qchar *to );
qint		FS_FOpenFileRead( const qchar *qpath, fileHandle_t *file, qbool uniqueFILE );
// if uniqueFILE is true, then a new FILE will be fopened even if the file
// is found in an already open pak file.  If uniqueFILE is false, you must call
// FS_FCloseFile instead of fclose, otherwise the pak FILE would be improperly closed
// It is generally safe to always set uniqueFILE to true, because the majority of
// file IO goes through FS_ReadFile, which Does The Right Thing already.

void
FS_TouchFileInPak(const qchar *filename);

void
FS_BypassPure(void);
void
FS_RestorePure(void);

qint
FS_Home_FOpenFileRead(const qchar *filename, fileHandle_t *file);

qbool		FS_FileIsInPAK(const qchar *filename, qint *pChecksum, qchar *pakName );
// returns 1 if a file is in the PAK file, otherwise -1

qint
FS_PakIndexForHandle(fileHandle_t f);
//returns pak index or -1 if file is not in pak

extern qint fs_lastPakIndex;
extern qbool fs_reordered;

qint		FS_Write( const void *buffer, qint len, fileHandle_t f );

qint		FS_Read( void *buffer, qint len, fileHandle_t f );
// properly handles partial reads and reads from other dlls

void	FS_FCloseFile( fileHandle_t f );
// note: you can't just fclose from another DLL, due to MS libc issues

qint		FS_ReadFile( const qchar *qpath, void **buffer );
// returns the length of the file
// a null buffer will just return the file length without loading
// as a quick check for existence. -1 length == not present
// A 0 byte will always be appended at the end, so string ops are safe.
// the buffer should be considered read-only, because it may be cached
// for other uses.

void	FS_ForceFlush( fileHandle_t f );
// forces flush on files we're writing to.

void	FS_FreeFile( void *buffer );
// frees the memory returned by FS_ReadFile

void	FS_WriteFile( const qchar *qpath, const void *buffer, qint size );
// writes a complete file, creating any subdirectories needed

qint		FS_filelength( fileHandle_t f );
// doesn't work for files that are opened from a pack file

qint		FS_FTell( fileHandle_t f );
// where are we?

void	FS_Flush( fileHandle_t f );

void 	QDECL FS_Printf( fileHandle_t f, const qchar *fmt, ... ) __attribute__ ((format (printf, 2, 3)));
// like fprintf

qint		FS_FOpenFileByMode( const qchar *qpath, fileHandle_t *f, fsMode_t mode );
// opens a file for reading, writing, or appending depending on the value of mode

qint		FS_Seek( fileHandle_t f, long offset, fsOrigin_t origin );
// seek on a file

qbool FS_FilenameCompare( const qchar *s1, const qchar *s2 );

const qchar *FS_LoadedPakNames( void );
const qchar *FS_LoadedPakChecksums( qbool *overflowed );
// Returns a space separated string containing the checksums of all loaded pk3 files.
// Servers with sv_pure set will get this string and pass it to clients.

qbool
FS_ExcludeReference(void);
const qchar *FS_ReferencedPakNames( void );
const qchar *FS_ReferencedPakChecksums( void );
const qchar *
FS_ReferencedPakPureChecksums(qint maxlen);
// Returns a space separated string containing the checksums of all loaded 
// AND referenced pk3 files. Servers with sv_pure set will get this string 
// back from clients for pure validation 

void FS_ClearPakReferences( qint flags );
// clears referenced booleans on loaded pk3s

void FS_PureServerSetReferencedPaks( const qchar *pakSums, const qchar *pakNames );
void FS_PureServerSetLoadedPaks( const qchar *pakSums, const qchar *pakNames );
// If the string is empty, all data sources will be allowed.
// If not empty, only pk3 files that match one of the space
// separated checksums will be checked for files, with the
// sole exception of .cfg files.

qbool
FS_IsPureChecksum(qint sum);

qbool
FS_InvalidGameDir(const qchar *gamedir);
qbool FS_ComparePaks( qchar *neededpaks, qint len, qbool dlstring );

void FS_Rename( const qchar *from, const qchar *to );

void FS_Remove( const qchar *osPath );
void FS_HomeRemove( const qchar *homePath );

void
FS_FilenameCompletion(const qchar *dir, const qchar *ext, qbool stripExt, void(*callback)(const qchar *s), qint flags);

const qchar *
FS_GetCurrentGameDir(void);
const qchar *
FS_GetBaseGameDir(void);

const qchar *
FS_GetBasePath(void);
const qchar *
FS_GetHomePath(void);
const qchar *
FS_GetGamePath(void);

qbool
FS_StripExt(qchar *filename, const qchar *ext);
qbool
FS_AllowedExtension(const qchar *fileName, qbool allowPk3s, const qchar **ext);

void *
FS_LoadLibrary(const qchar *name);

typedef qbool (*fnamecallback_f)(const qchar *filename, qint length);

void
FS_SetFilenameCallback(fnamecallback_f func);

qchar *
FS_CopyString(const qchar *in);

//AVI pipes
fileHandle_t
FS_PipeOpenWrite(const qchar *cmd, const qchar *filename);
void
FS_PipeClose(fileHandle_t f);

qint
FS_VM_OpenFile(const qchar *qpath, fileHandle_t *f, fsMode_t mode, handleOwner_t owner);
qint
FS_VM_ReadFile(void *buffer, qint len, fileHandle_t f, handleOwner_t owner);
void
FS_VM_WriteFile(void *buffer, const qint len, fileHandle_t f, handleOwner_t owner);
qint
FS_VM_SeekFile(fileHandle_t f, long offset, fsOrigin_t origin, handleOwner_t owner);
void
FS_VM_CloseFile(fileHandle_t f, handleOwner_t owner);
void
FS_VM_CloseFiles(handleOwner_t owner);

const qbool
FS_VerifyPak(const qchar *pak);

/*
==============================================================

Edit fields and command line history/completion

==============================================================
*/

#define	MAX_EDIT_LINE	256
typedef struct {
	qint		cursor;
	qint		scroll;
	qint		widthInChars;
	qchar	buffer[MAX_EDIT_LINE];
} field_t;

void
Field_Clear(field_t *edit);
void
Field_AutoComplete(field_t *edit);
void
Field_CompleteKeyname(void);
void
Field_CompleteKeyBind(qint key);
void
Field_CompleteFilename(const qchar *dir, const qchar *ext, qbool stripExt, qint flags);
void
Field_CompleteCommand(const qchar *cmd, qbool doCommands, qbool doCvars);

void
Con_ResetHistory(void);
void
Con_SaveField(const field_t *field);
qbool
Con_HistoryGetPrev(field_t *field);
qbool
Con_HistoryGetNext(field_t *field);

/*
==============================================================

MISC

==============================================================
*/

//customizable client window title
extern qchar cl_title[MAX_CVAR_VALUE_STRING];

extern qint CPU_Flags;

//x86 flags
#define CPU_FCOM 0x01
#define CPU_MMX 0x02
#define CPU_SSE 0x04
#define CPU_SSE2 0x08
#define CPU_SSE3 0x10
#define CPU_SSE41 0x20

//ARM flags
#define CPU_ARMv7 0x01
#define CPU_IDIVA 0x02
#define CPU_VFPv3 0x04

// centralized and cleaned, that's the max string you can send to a Com_Printf / Com_DPrintf (above gets truncated)
#define	MAXPRINTMSG	8192

qchar *
CopyString(const qchar *in);
void
Info_Print(const qchar *s);

void
Com_BeginRedirect(qchar *buffer, qint buffersize, void (*flush)(const qchar *));
void
Com_EndRedirect(void);
void QDECL
Com_Printf(const qchar *fmt, ...) __attribute__((format(printf, 1, 2)));
void QDECL
Com_DPrintf(const qchar *fmt, ...) __attribute__((format(printf, 1, 2)));
void
Com_Quit_f(void);
void
Com_GameRestart(qint checksumFeed, qbool clientRestart);

qint
Com_EventLoop(void);
qint
Com_Milliseconds(void); //will be journaled properly

//md4 functions
unsigned
Com_BlockChecksum(const void *buffer, qint length);

//md5 functions
qchar *
Com_MD5File(const qchar *filename, qint length, const qchar *prefix, qint prefix_len);
qchar *
Com_MD5Buf(const qchar *data, qint length, const qchar *data2, qint length2);

qbool
Com_EarlyParseCmdLine(qchar *cmdLine, qchar *con_title, qint title_size, qint *vid_xpos, qint *vid_ypos);
qint
Com_Split(qchar *in, qchar **out, qint outsz, qint delim);

qint
Com_Filter(const qchar *filter, const qchar *name);
qbool
Com_FilterExt(const qchar *filter, const qchar *name);
qbool
Com_HasPatterns(const qchar *str);
qint
Com_FilterPath(const qchar *filter, const qchar *name);
qint
Com_RealTime(qtime_t *qtime);
qbool
Com_SafeMode(void);
void
Com_RunAndTimeServerPacket(const netadr_t *evFrom, msg_t *buf);

void
Com_StartupVariable(const qchar *match);
//checks for and removes command line "+set var arg" constructs
//if match is NULL, all set commands will be executed, otherwise
//only a set with the exact name. Only used during startup.

void
Com_WriteConfiguration(void);
qint
Com_HexStrToInt(const qchar *str);
qbool
Com_GetHashColor(const qchar *str, byte *color);

static ID_INLINE unsigned
log2pad(unsigned v, qint roundup)
{
  unsigned x = 1;

  while(x < v)
  {
    x <<= 1;
  }

  if (roundup == 0)
  {
    if (x > v)
    {
      x >>= 1;
    }
  }

  return x;
}

extern cvar_t *com_developer;
extern cvar_t *com_dedicated;
extern cvar_t *com_speeds;
extern cvar_t *com_timescale;
extern cvar_t *com_viewlog; // 0 = hidden, 1 = visible, 2 = minimized
extern cvar_t *com_version;
extern cvar_t *com_journal;
extern cvar_t *com_cameraMode;
extern cvar_t *com_ansiColor;
extern cvar_t *com_protocol;
extern qbool com_protocolCompat;

// both client and server must agree to pause
extern cvar_t *sv_paused;
extern cvar_t *sv_packetdelay;
extern cvar_t *com_sv_running;

#ifndef DEDICATED
extern cvar_t *cl_paused;
extern cvar_t *cl_packetdelay;
extern cvar_t *com_cl_running;
extern cvar_t *com_yieldCPU;
#endif

extern cvar_t *vm_rtChecks;
#ifdef USE_AFFINITY_MASK
extern cvar_t *com_affinityMask;
#endif

// com_speeds times
extern qint time_game;
extern qint time_frontend;
extern qint time_backend; //renderer backend time

extern qint com_frameTime;

#ifndef DEDICATED
extern qbool gw_minimized;
extern qbool gw_active;
#endif

extern qbool com_errorEntered;

extern fileHandle_t com_journalDataFile;

extern qchar rconPassword2[MAX_CVAR_VALUE_STRING];

typedef enum
{
  TAG_FREE,
  TAG_GENERAL,
  TAG_PACK,
  TAG_SEARCH_PATH,
  TAG_SEARCH_PACK,
  TAG_SEARCH_DIR,
  TAG_BOTLIB,
  TAG_RENDERER,
  TAG_CLIENTS,
  TAG_SMALL,
  TAG_STATIC,
  TAG_COUNT
}
memtag_t;

/*

--- low memory ----
server vm
server clipmap
---mark---
renderer initialization (shaders, etc)
UI vm
cgame vm
renderer map
renderer models

---free---

temp file loading
--- high memory ---

*/

#if defined(_DEBUG) && !defined(BSPC)
#define ZONE_DEBUG
#endif

#if defined(ZONE_DEBUG)
#define Z_TagMalloc(size, tag) Z_TagMallocDebug(size, tag, #size, __FILE__, __LINE__)
#define Z_Malloc(size) Z_MallocDebug(size, #size, __FILE__, __LINE__)
#define S_Malloc(size) S_MallocDebug(size, #size, __FILE__, __LINE__)
void *Z_TagMallocDebug( qint size, memtag_t tag, qchar *label, qchar *file, qint line );	// NOT 0 filled memory
void *Z_MallocDebug( qint size, qchar *label, qchar *file, qint line );			// returns 0 filled memory
void *S_MallocDebug( qint size, qchar *label, qchar *file, qint line );			// returns 0 filled memory
#else
void *Z_TagMalloc( qint size, memtag_t tag );	// NOT 0 filled memory
void *Z_Malloc( qint size );			// returns 0 filled memory
void *S_Malloc( qint size );			// NOT 0 filled memory only for small allocations
#endif
void Z_Free( void *ptr );
qint Z_FreeTags( memtag_t tag );
qint Z_AvailableMemory( void );
void Z_LogHeap( void );

void Hunk_Clear( void );
void Hunk_ClearToMark( void );
void Hunk_SetMark( void );
qbool Hunk_CheckMark( void );
void Hunk_ClearTempMemory( void );
void *Hunk_AllocateTempMemory( qint size );
void Hunk_FreeTempMemory( void *buf );
qint	Hunk_MemoryRemaining( void );
void Hunk_Log( void);
void Hunk_Trash( void );

unsigned qint
Com_TouchMemory(void);

// commandLine should not include the executable name (argv[0])
void
Com_Init(qchar *commandLine);
void
Com_FrameInit(void);
void
Com_Frame(qbool noDelay);


/*
==============================================================

CLIENT / SERVER SYSTEMS

==============================================================
*/

//
// client interface
//
void CL_Init( void );
qbool CL_Disconnect( qbool showMainMenu );
void
CL_ResetOldGame(void);
void CL_Shutdown( const qchar *finalmsg, qbool quit );
void CL_Frame( qint msec, qint realMsec );
qbool CL_GameCommand( void );
void CL_KeyEvent (qint key, qbool down, unsigned time);

void CL_CharEvent( qint key );
// qchar events are for field typing, not game control

void CL_MouseEvent( qint dx, qint dy /*, qint time*/ );

void CL_JoystickEvent( qint axis, qint value, qint time );

void CL_PacketEvent( const netadr_t *from, msg_t *msg );

void CL_ConsolePrint( const qchar *text );

void CL_MapLoading( void );
// do a screen update before starting to load a map
// when the server is going to load a new map, the entire hunk
// will be cleared, so the client must shutdown cgame, ui, and
// the renderer

void	CL_ForwardCommandToServer( const qchar *string );
// adds the current command line as a clc_clientCommand to the client message.
// things like godmode, noclip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.

void CL_CDDialog( void );
// bring up the "need a cd to play" dialog

void CL_ShutdownAll( void );
// shutdown all the client stuff

void
CL_ClearMemory(void);
//clear memory

void CL_FlushMemory( void );
// dump all memory on an error

void CL_StartHunkUsers( void );
// start all the client stuff using the hunk

void
CL_Snd_Restart(void);
//restart sound subsystem

void Key_KeynameCompletion( void(*callback)(const qchar *s) );
// for keyname autocompletion

void Key_WriteBindings( fileHandle_t f );
// for writing the config files

void S_ClearSoundBuffer( void );
// call before filesystem access

void
CL_SystemInfoChanged(qbool onlyGame);

qbool
CL_GameSwitch(void);

//AVI files have the start of pixel lines 4 byte-aligned
#define AVI_LINE_PADDING 4

//
// server interface
//
void
SV_Init(void);
const void
SV_Shutdown(const qchar *finalmsg);
void
SV_Frame(qint msec);
void
SV_TrackCvarChanges(void);
void
SV_ReadPackets(const netadr_t *from, msg_t *msg);
qint
SV_FrameMsec(void);
qbool
SV_GameCommand( void );
qint
SV_SendQueuedPackets(void);

void
SV_AddDedicatedCommands(void);
void
SV_RemoveDedicatedCommands(void);


//
// UI interface
//
qbool UI_GameCommand( void );

/*
==============================================================

NON-PORTABLE SYSTEM SERVICES

==============================================================
*/

typedef enum
{
  AXIS_SIDE,
  AXIS_FORWARD,
  AXIS_UP,
  AXIS_ROLL,
  AXIS_YAW,
  AXIS_PITCH,
  MAX_JOYSTICK_AXIS
}
joystickAxis_t;

typedef enum
{
  //bk001129 - make sure SE_NONE is zero
  SE_NONE = 0, //evTime is still valid
  SE_KEY, //evValue is a key code, evValue2 is the down flag
  SE_CHAR, //evValue is an ascii char
  SE_MOUSE, //evValue and evValue2 are relative signed x / y moves
  SE_JOYSTICK_AXIS, //evValue is an axis number and evValue2 is the current state (-127 to 127)
  SE_CONSOLE, //evPtr is a char*
  SE_MAX,
}
sysEventType_t;

typedef struct
{
  qint evTime;
  sysEventType_t evType;
  qint evValue;
  qint evValue2;
  qint evPtrLength; //bytes of data pointed to by evPtr, for journaling
  void *evPtr; //this must be manually freed if not NULL
}
sysEvent_t;

void
Sys_Init(void);
void
Sys_QueEvent(qint evTime, sysEventType_t evType, qint value, qint value2, qint ptrLength, void *ptr);
void
Sys_SendKeyEvents(void);
void
Sys_Sleep(qint msec);
qchar *
Sys_ConsoleInput(void);

void NORETURN FORMAT_PRINTF(1, 2) QDECL
Sys_Error(const qchar *error, ...);
void NORETURN
Sys_Quit(void);
qchar *
Sys_GetClipboardData(void); //note that this isn't journaled...
void
Sys_SetClipboardBitmap(const byte *bitmap, qint length);

void
Sys_Print(const qchar *msg);

//dedicated console status, win32-only at the moment
void QDECL
Sys_SetStatus(const qchar *format, ...) __attribute__((format(printf, 1, 2)));

#if defined(USE_AFFINITY_MASK)
uint64_t
Sys_GetAffinityMask(void);
qbool
Sys_SetAffinityMask(const uint64_t mask);
#endif

// Sys_Milliseconds should only be used for profiling purposes,
// any game related timing information should come from event timestamps
qint
Sys_Milliseconds(void);
int64_t
Sys_Microseconds(void);

void
Sys_SnapVector(float *v);

qbool
Sys_RandomBytes(byte *string, qint len);

// the system console is shown when a dedicated server is running
void
Sys_DisplaySystemConsole(qbool show);

void
Sys_ShowConsole(qint level, qbool quitOnClose);
void
Sys_SetErrorText(const qchar *text);

void
Sys_SendPacket(qint length, const void *data, const netadr_t *to);

qbool
Sys_StringToAdr(const qchar *s, netadr_t *a, netadrtype_t family);
//Does NOT parse port numbers, only base addresses.

qbool
Sys_IsLANAddress(const netadr_t *adr);
void
Sys_ShowIP(void);

qbool
Sys_Mkdir(const qchar *path);
FILE *
Sys_FOpen(const qchar *ospath, const qchar *mode);
qbool
Sys_ResetReadOnlyAttribute(const qchar *ospath);

const qchar *
Sys_Pwd(void);
const qchar *
Sys_DefaultBasePath(void);
const qchar *
Sys_DefaultHomePath(void);
const qchar *
Sys_SteamPath(void);

#if defined(__APPLE__)
qchar *
Sys_DefaultAppPath(void);
#endif

qchar **
Sys_ListFiles(const qchar *directory, const qchar *extension, const qchar *filter, qint *numfiles, qint subdirs);
void
Sys_FreeFileList(qchar **list);

qbool
Sys_GetFileStats(const qchar *filename, fileOffset_t *size, fileTime_t *mtime, fileTime_t *ctime);

void
Sys_BeginProfiling(void);
void
Sys_EndProfiling(void);

qbool
Sys_LowPhysicalMemory(void);

qint
Sys_MonkeyShouldBeSpanked(void);

void *
Sys_LoadLibrary(const qchar *name);
void *
Sys_LoadFunction(void *handle, const qchar *name);
qint
Sys_LoadFunctionErrors(void);
void
Sys_UnloadLibrary(void *handle);

void
Sys_SetEnv(const qchar *name, const qchar *value);

//adaptive huffman functions
void
Huff_Compress(msg_t *buf, qint offset);
void
Huff_Decompress(msg_t *buf, qint offset);

//static huffman functions
void
HuffmanPutBit(byte *fout, int32_t bitIndex, qint bit);
qint
HuffmanPutSymbol(byte* fout, uint32_t offset, qint symbol);
qint
HuffmanGetBit(const byte* buffer, qint bitIndex);
qint
HuffmanGetSymbol(unsigned *symbol, const byte* buffer, qint bitIndex);

qint
Parse_AddGlobalDefine(qchar *string);
qint
Parse_LoadSourceHandle(const qchar *filename);
qint
Parse_FreeSourceHandle(qint handle);
qint
Parse_ReadTokenHandle(qint handle, pc_token_t *pc_token);
qint
Parse_SourceFileAndLine(qint handle, qchar *filename, qint *line);

#define	SV_ENCODE_START 4
#define SV_DECODE_START 12
#define	CL_ENCODE_START 12
#define CL_DECODE_START 4

//flags for sv_allowDownload and cl_allowDownload
#define DLF_ENABLE 1
#define DLF_NO_REDIRECT 2
#define DLF_NO_UDP 4
#define DLF_NO_DISCONNECT 8

//functional gate syscall number
#define COM_TRAP_GETVALUE 700
