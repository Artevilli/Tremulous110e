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
// common.c -- misc functions used in client and server

#include "q_shared.h"
#include "qcommon.h"

#if defined(USE_JAVA)
#include "vm_java.h"
#elif defined(USE_MONO)
#include "vm_mono.h"
#endif

#include <setjmp.h>
#if !defined(_WIN32)
#include <netinet/in.h>
#include <sys/stat.h> // umask
#else
#include <winsock.h>
#if defined(_DEBUG)
#include "../win32/win_local.h"
#endif
#endif

qint demo_protocols[] =
{ PROTOCOL_VERSION, 0 };

#define USE_MULTI_SEGMENT //allocate additional zone segments on demand

#define MIN_COMHUNKMEGS 96
#define DEF_COMHUNKMEGS 256
#if defined(USE_MULTI_SEGMENT)
#define DEF_COMZONEMEGS 12
#else
#define DEF_COMZONEMEGS 25
#endif

qint com_argc;

static jmp_buf abortframe; //an ERR_DROP occured, exit the entire frame

qint CPU_Flags = 0;

FILE *debuglogfile;
static fileHandle_t pipefile = FS_INVALID_HANDLE;
static fileHandle_t logfile = FS_INVALID_HANDLE;
fileHandle_t com_journalFile = FS_INVALID_HANDLE; //events are written here
fileHandle_t com_journalDataFile = FS_INVALID_HANDLE; //config files are written here

cvar_t *com_speeds;
cvar_t *com_developer;
cvar_t *com_dedicated;
cvar_t *com_timescale;
static cvar_t *com_fixedtime;
cvar_t *com_journal;
cvar_t *com_homepath;
#if !defined(DEDICATED)
cvar_t *com_maxfps;
cvar_t *com_maxfpsUnfocused;
cvar_t *com_yieldCPU;
cvar_t *com_timedemo;
#endif
#if defined(USE_AFFINITY_MASK)
cvar_t *com_affinityMask;
#endif
cvar_t *com_altivec;
cvar_t *com_sv_running;
cvar_t *com_cl_running;
#if defined(_WIN32) && defined(_DEBUG)
cvar_t *com_noErrorInterrupt;
#endif
static cvar_t *com_logfile;		// 1 = buffer log, 2 = flush after each print
cvar_t *com_pipefile;
static cvar_t *com_showtrace;
cvar_t *com_version;
cvar_t *com_blood;
static cvar_t *com_buildScript;	// for automated data building scripts
#if !defined(DEDICATED)
cvar_t *cl_paused;
cvar_t *cl_packetdelay;
#endif
cvar_t *sv_paused;
cvar_t *cl_packetloss;
cvar_t *sv_packetdelay;
cvar_t *sv_packetloss;
cvar_t *com_cameraMode;
cvar_t *com_ansiColor;
cvar_t *com_abnormalExit;
cvar_t *com_homepath;

//com_speeds times
qint time_game;
qint time_frontend; //renderer frontend time
qint time_backend; //renderer backend time

static qint lastTime;
qint com_frameTime;
static qint com_frameNumber;

qbool com_errorEntered = qfalse;
qbool com_fullyInitialized = qfalse;

//renderer window states
qbool gw_minimized = qfalse; //this will always be true for dedicated servers
#if !defined(DEDICATED)
qbool gw_active = qtrue;
#endif

qchar com_errorMessage[MAXPRINTMSG];

static void Com_Shutdown(void);
static void Com_WriteConfig_f( void );
void CIN_CloseAllVideos( void );

//============================================================================

static qchar *rd_buffer;
static qint rd_buffersize;
static qbool rd_flushing = qfalse;
static void (*rd_flush)(const qchar *buffer);

void
Com_BeginRedirect(qchar *buffer, qint buffersize, void (*flush)(const qchar *))
{
  if (!buffer || !buffersize || !flush)
  {
    return;
  }

  rd_buffer = buffer;
  rd_buffersize = buffersize;
  rd_flush = flush;
  *rd_buffer = '\0';
}

void
Com_EndRedirect(void)
{
  if (rd_flush)
  {
    rd_flushing = qtrue;
    rd_flush(rd_buffer);
    rd_flushing = qfalse;
  }

  rd_buffer = NULL;
  rd_buffersize = 0;
  rd_flush = NULL;
}

/*
=============
Com_Printf

Both client and server can use this, and it will output
to the appropriate place.

A raw string should NEVER be passed as fmt, because of "%f" type crashers.
=============
*/
void FORMAT_PRINTF(1, 2) QDECL
Com_Printf(const qchar *fmt, ...)
{
  static qbool opening_qconsole = qfalse;
  va_list argptr;
  qchar msg[MAXPRINTMSG];
  qint len;

  va_start(argptr, fmt);
  len = Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);

  if (rd_buffer && !rd_flushing)
  {
    if (len + (qint)strlen(rd_buffer) > (rd_buffersize - 1))
    {
      rd_flushing = qtrue;
      rd_flush(rd_buffer);
      rd_flushing = qfalse;
      *rd_buffer = '\0';
    }

    Q_strcat(rd_buffer, rd_buffersize, msg);
    //TTimo nooo .. that would defeat the purpose
    //rd_flush(rd_buffer);
    //*rd_buffer = '\0';
    return;
  }

#if !defined(DEDICATED)
  //echo to client console if we're not a dedicated server
  if (!com_dedicated || !com_dedicated->integer)
  {
    CL_ConsolePrint(msg);
  }
#endif

  //echo to dedicated console and early console
  Sys_Print(msg);

  //logfile
  if (com_logfile && com_logfile->integer)
  {
    //TTimo: only open the qconsole.log if the filesystem is in an initialized state
    //also, avoid recursing in the qconsole.log opening (i.e. if fs_debug is on)
    if (logfile == FS_INVALID_HANDLE && FS_Initialized() && !opening_qconsole)
    {
      const qchar *logName = "qconsole.log";
      qint mode;

      opening_qconsole = qtrue;

      mode = com_logfile->integer - 1;

      if (mode & 2)
      {
        logfile = FS_FOpenFileAppend(logName);
      }
      else
      {
        logfile = FS_FOpenFileWrite( logName );
      }

      if (logfile != FS_INVALID_HANDLE)
      {
        struct tm *newtime;
        time_t aclock;
        qchar timestr[32];

        time(&aclock);
        newtime = localtime(&aclock);
        strftime(timestr, sizeof(timestr), "%a %b %d %X %Y", newtime);

        Com_Printf("logfile opened on %s\n", timestr);

        if (mode & 1)
        {
          //force it to not buffer so we get valid
          //data even if we are crashing
          FS_ForceFlush(logfile);
        }
      }
      else
      {
        Com_Printf(S_COLOR_YELLOW "Opening %s failed!\n", logName);
        Cvar_Set("logfile", "0");
      }

      opening_qconsole = qfalse;
    }

    if (logfile != FS_INVALID_HANDLE && FS_Initialized())
    {
      FS_Write(msg, len, logfile);
    }
  }
}


/*
================
Com_DPrintf

A Com_Printf that only shows up if the "developer" cvar is set
================
*/
void FORMAT_PRINTF(1, 2) QDECL
Com_DPrintf(const qchar *fmt, ...)
{
  va_list argptr;
  qchar msg[MAXPRINTMSG];

  if (!com_developer || !com_developer->integer)
  {
    return; //don't confuse non-developers with techie stuff...
  }

  va_start(argptr, fmt);
  Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);

  Com_Printf(S_COLOR_CYAN "%s", msg);
}

/*
=============
Com_Error

Both client and server can use this, and it will
do the appropriate things.
=============
*/
//void NORETURN FORMAT_PRINTF(2, 3) QDECL //WHERE DOES THIS RETURN?
void FORMAT_PRINTF(2, 3) QDECL
Com_Error(errorParm_t code, const qchar *fmt, ...)
{
  va_list argptr;
  static qint lastErrorTime;
  static qint errorCount;
  static qbool calledSysError = qfalse;
  qint currentTime;

#if defined(_WIN32) && defined(_DEBUG)
  if (code != ERR_DISCONNECT && code != ERR_NEED_CD)
  {
    if (!com_noErrorInterrupt->integer)
    {
      ShowWindow(g_wv.hWnd, SW_MINIMIZE);
      DebugBreak();
    }
  }
#endif

  if (com_errorEntered)
  {
    if (!calledSysError)
    {
      calledSysError = qtrue;
      Sys_Error("recursive error after: %s", com_errorMessage);
    }
  }

  com_errorEntered = qtrue;

  Cvar_SetIntegerValue("com_errorCode", code);

  //when we are running automated scripts, make sure we
  //know if anything failed
  if (com_buildScript && com_buildScript->integer)
  {
    code = ERR_FATAL;
  }

  //if we are getting a solid stream of ERR_DROP, do an ERR_FATAL
  currentTime = Sys_Milliseconds();

  if (currentTime - lastErrorTime < 100)
  {
    if (++errorCount > 3)
    {
      code = ERR_FATAL;
    }
  }
  else
  {
    errorCount = 0;
  }

  lastErrorTime = currentTime;

  va_start(argptr, fmt);
  Q_vsnprintf(com_errorMessage, sizeof(com_errorMessage), fmt, argptr);
  va_end(argptr);

  if (code != ERR_DISCONNECT && code != ERR_NEED_CD)
  {
    //we can't recover from ERR_FATAL so there is no recipients for com_errorMessage
    //also if ERR_FATAL was called from S_Malloc - CopyString for a long (2+ chars) text
    //will trigger recursive error without proper client/server shutdown
    if (code != ERR_FATAL)
    {
      Cvar_Set("com_errorMessage", com_errorMessage);
    }
  }

  Cbuf_Init();

  if (code == ERR_DISCONNECT || code == ERR_SERVERDISCONNECT)
  {
    VM_Forced_Unload_Start();
    SV_Shutdown("Server disconnected");
    Com_EndRedirect();
#if !defined(DEDICATED)
    CL_Disconnect(qfalse);
    CL_FlushMemory();
#endif
    VM_Forced_Unload_Done();

    //make sure we can get at our local stuff
    FS_PureServerSetLoadedPaks("", "");
    com_errorEntered = qfalse;

    Q_longjmp(abortframe, 1);
  }
  else if (code == ERR_DROP)
  {
    Com_Printf("********************\nERROR: %s\n********************\n", com_errorMessage);
    VM_Forced_Unload_Start();
    SV_Shutdown(va(NULL, "Server crashed: %s",  com_errorMessage));
    Com_EndRedirect();
#if !defined(DEDICATED)
    CL_Disconnect(qfalse);
    CL_FlushMemory();
#endif
    VM_Forced_Unload_Done();

    FS_PureServerSetLoadedPaks("", "");
    com_errorEntered = qfalse;

    Q_longjmp(abortframe, 1);
  }
  else if (code == ERR_NEED_CD)
  {
    SV_Shutdown("Server didn't have CD");
    Com_EndRedirect();
#if !defined(DEDICATED)
    if (com_cl_running && com_cl_running->integer)
    {
      CL_Disconnect(qfalse);
      VM_Forced_Unload_Start();
      CL_FlushMemory();
      VM_Forced_Unload_Done();
      CL_CDDialog();
    }
    else
    {
      Com_Printf("Server didn't have CD\n");
    }
#endif
    FS_PureServerSetLoadedPaks("", "");
    com_errorEntered = qfalse;

    Q_longjmp(abortframe, 1);
  }
  else
  {
    VM_Forced_Unload_Start();
#if !defined(DEDICATED)
    CL_Shutdown(va(NULL, "Server fatal crashed: %s", com_errorMessage));
#endif
    SV_Shutdown(va(NULL, "Server fatal crashed: %s", com_errorMessage));
    Com_EndRedirect();
    VM_Forced_Unload_Done();
  }

  Com_Shutdown();

  calledSysError = qtrue;
  Sys_Error("%s", com_errorMessage);
}


/*
=============
Com_Quit_f

Both client and server can use this, and it will
do the apropriate things.
=============
*/
void
Com_Quit_f(void)
{
  const qchar *p = Cmd_ArgsFrom(1);

  //don't try to shutdown if we are in a recursive error
  if (!com_errorEntered)
  {
    //some VMs might execute "quit" command directly,
    //which would trigger an unload of active VM error.
    //Sys_Quit will kill this process anyways, so
    //a corrupt call stack makes no difference
    VM_Forced_Unload_Start();
    SV_Shutdown(p[0] ? p:"Server quit");
#if !defined(DEDICATED)
    CL_Shutdown(p[0] ? p:"Client quit");
#endif
    VM_Forced_Unload_Done();
    Com_Shutdown();
    FS_Shutdown(qtrue);
  }

  Sys_Quit();
}



/*
============================================================================

COMMAND LINE FUNCTIONS

+ characters seperate the commandLine string into multiple console
command lines.

All of these are valid:

tremulous +set test blah +map test
tremulous set test blah+map test
tremulous set test blah + map test

============================================================================
*/

#define	MAX_CONSOLE_LINES	32
qint		com_numConsoleLines;
qchar	*com_consoleLines[MAX_CONSOLE_LINES];

/*
==================
Com_ParseCommandLine

Break it up into multiple console lines
==================
*/
void Com_ParseCommandLine( qchar *commandLine ) {
    qint inq = 0;
    com_consoleLines[0] = commandLine;
    com_numConsoleLines = 1;

    while ( *commandLine ) {
        if (*commandLine == '"') {
            inq = !inq;
        }
        // look for a + separating character
        // if commandLine came from a file, we might have real line seperators
        if ( (*commandLine == '+' && !inq) || *commandLine == '\n'  || *commandLine == '\r' ) {
            if ( com_numConsoleLines == MAX_CONSOLE_LINES ) {
                return;
            }
            com_consoleLines[com_numConsoleLines] = commandLine + 1;
            com_numConsoleLines++;
            *commandLine = '\0';
        }
        commandLine++;
    }
}


/*
===================
Com_SafeMode

Check for "safe" on the command line, which will
skip loading of autogen.cfg
===================
*/
qbool Com_SafeMode( void ) {
	qint		i;

	for ( i = 0 ; i < com_numConsoleLines ; i++ ) {
		Cmd_TokenizeString( com_consoleLines[i] );
		if ( !Q_stricmp( Cmd_Argv(0), "safe" )
			|| !Q_stricmp( Cmd_Argv(0), "cvar_restart" ) ) {
			com_consoleLines[i][0] = '\0';
			return qtrue;
		}
	}
	return qfalse;
}


/*
===============
Com_StartupVariable

Searches for command line parameters that are set commands.
If match is not NULL, only that cvar will be looked for.
That is necessary because cddir and basedir need to be set
before the filesystem is started, but all other sets should
be after execing the config and default.
===============
*/
void Com_StartupVariable( const qchar *match ) {
	qint		i;
	qchar	*s;

	for (i=0 ; i < com_numConsoleLines ; i++) {
		Cmd_TokenizeString( com_consoleLines[i] );
		if ( strcmp( Cmd_Argv(0), "set" ) ) {
			continue;
		}

		s = Cmd_Argv(1);

		if (!match || !strcmp(s, match))
		{
                  if (Cvar_Flags(s) == CVAR_NONEXISTENT)
                  {
                    Cvar_Get(s, Cmd_Argv(2), CVAR_USER_CREATED);
                  }
                  else
                  {
                    Cvar_Set(s, Cmd_Argv(2));
                  }
		}
	}
}

#if 0 //FIXME: broken
void
Com_CommandLineCheck(qbool(*clb)(void))
{
  qint i;

  for(i = 0;i < com_numConsoleLines;i++)
  {
    Cmd_TokenizeString(com_consoleLines[i]);

    if (!clb())
    {
      continue;
    }

    com_consoleLines[i][0] = '\0';
  }
}
#endif

/*
=================
Com_AddStartupCommands

Adds command line parameters as script statements
Commands are seperated by + signs

Returns qtrue if any late commands were added, which
will keep the demoloop from immediately starting
=================
*/
static qbool
Com_AddStartupCommands(void)
{
  qint i;
  qbool added;

  added = qfalse;

  //quote every token, so args with semicolons can work
  for(i = 0;i < com_numConsoleLines;i++)
  {
    if (!com_consoleLines[i] || !com_consoleLines[i][0])
    {
      continue;
    }

    //set commands already added with Com_StartupVariable
    if (!Q_stricmpn(com_consoleLines[i], "set", 4))
    {
      continue;
    }

    added = qtrue;
    Cbuf_AddText(com_consoleLines[i]);
    Cbuf_AddText("\n" );
  }

  return added;
}


//============================================================================

void
Info_Print(const qchar *s)
{
  qchar key[BIG_INFO_KEY];
  qchar value[BIG_INFO_VALUE];

  do
  {
    s = Info_NextPair(s, key, value);

    if (key[0] == '\0')
    {
      break;
    }

    if (value[0] == '\0')
    {
      strcpy(value, "MISSING VALUE");
    }

    Com_Printf("%-20s %s\n", key, value);
  }
  while(*s != '\0');
}

/*
============
Com_StringContains
============
*/
static const qchar *Com_StringContains(const qchar *str1, const qchar *str2, qint len2) {
	qint len, i, j;

	len = strlen(str1) - len2;
	for (i = 0; i <= len; i++, str1++) {
		for (j = 0; str2[j]; j++) {
			if (locase[(byte)str1[j]] != locase[(byte)str2[j]])
			{
			  break;
			}
		}
		if (!str2[j]) {
			return str1;
		}
	}
	return NULL;
}

/*
============
Com_Filter
============
*/
qint Com_Filter(const qchar *filter, const qchar *name)
{
	qchar buf[MAX_TOKEN_CHARS];
	const qchar *ptr;
	qint i, found;

	while(*filter) {
		if (*filter == '*') {
			filter++;
			for (i = 0; *filter; i++) {
				if (*filter == '*' || *filter == '?')
				  break;
				buf[i] = *filter;
				filter++;
			}
			buf[i] = '\0';
			if (i)
			{
				ptr = Com_StringContains(name, buf, i);
				if (!ptr)
					return qfalse;
				name = ptr + i;
			}
		}
		else if (*filter == '?') {
			filter++;
			name++;
		}
		else if (*filter == '[' && *(filter+1) == '[') {
			filter++;
		}
		else if (*filter == '[') {
			filter++;
			found = qfalse;
			while(*filter && !found) {
				if (*filter == ']' && *(filter+1) != ']') break;
				if (*(filter+1) == '-' && *(filter+2) && (*(filter+2) != ']' || *(filter+3) == ']')) {
					if (locase[(byte)*name] >= locase[(byte)*filter] && locase[(byte)*name] <= locase[(byte)*(filter + 2)])
					{
					  found = qtrue;
					}
					filter += 3;
				}
				else {
					if (locase[(byte)*filter] == locase[(byte)*name])
					{
					  found = qtrue;
					}
					filter++;
				}
			}
			if (!found) return qfalse;
			while(*filter) {
				if (*filter == ']' && *(filter+1) != ']') break;
				filter++;
			}
			filter++;
			name++;
		}
		else {
			if (locase[(byte)*filter] != locase[(byte)*name])
			{
			  return qfalse;
			}
			filter++;
			name++;
		}
	}
	return qtrue;
}

/*
============
Com_FilterExt
============
*/
qbool
Com_FilterExt(const qchar *filter, const qchar *name)
{
  qchar buf[MAX_TOKEN_CHARS];
  const qchar *ptr;
  qint i;

  while(*filter)
  {
    if (*filter == '*')
    {
      filter++;

      for(i = 0;*filter != '\0' && i < sizeof(buf) - 1;i++)
      {
        if (*filter == '*' || *filter == '?')
        {
          break;
        }

        buf[i] = *filter++;
      }

      buf[i] = '\0';

      if (i)
      {
        ptr = Com_StringContains(name, buf, i);

        if (!ptr)
        {
          return qfalse;
        }

        name = ptr + i;
      }
      else if (*filter == '\0')
      {
        return qtrue;
      }
    }
    else if (*filter == '?')
    {
      if (*name == '\0')
      {
        return qfalse;
      }

      filter++;
      name++;
    }
    else
    {
      if (locase[(byte)*filter] != locase[(byte)*name])
      {
        return qfalse;
      }

      filter++;
      name++;
    }
  }

  if (*name)
  {
    return qfalse;
  }

  return qtrue;
}

/*
============
Com_HasPatterns
============
*/
qbool
Com_HasPatterns(const qchar *str)
{
  qint c;

  while((c = *str++) != '\0')
  {
    if (c == '*' || c == '?')
    {
      return qtrue;
    }
  }

  return qfalse;
}

/*
============
Com_FilterPath
============
*/
qint Com_FilterPath(const qchar *filter, const qchar *name)
{
	qint i;
	qchar new_filter[MAX_QPATH];
	qchar new_name[MAX_QPATH];

	for (i = 0; i < MAX_QPATH-1 && filter[i]; i++) {
		if ( filter[i] == '\\' || filter[i] == ':' ) {
			new_filter[i] = '/';
		}
		else {
			new_filter[i] = filter[i];
		}
	}
	new_filter[i] = '\0';
	for (i = 0; i < MAX_QPATH-1 && name[i]; i++) {
		if ( name[i] == '\\' || name[i] == ':' ) {
			new_name[i] = '/';
		}
		else {
			new_name[i] = name[i];
		}
	}
	new_name[i] = '\0';
	return Com_Filter(new_filter, new_name);
}

/*
============
Com_HashKey
============
*/
qint Com_HashKey(qchar *string, qint maxlen) {
	qint register hash, i;

	hash = 0;
	for (i = 0; i < maxlen && string[i] != '\0'; i++) {
		hash += string[i] * (119 + i);
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	return hash;
}

/*
================
Com_RealTime
================
*/
unsigned
Com_RealTime(qtime_t *qtime)
{
  time_t t;
  struct tm *tms;

  t = time(NULL);

  if (!qtime)
  {
    return t;
  }

  tms = localtime(&t);

  if (tms)
  {
    qtime->tm_sec = tms->tm_sec;
    qtime->tm_min = tms->tm_min;
    qtime->tm_hour = tms->tm_hour;
    qtime->tm_mday = tms->tm_mday;
    qtime->tm_mon = tms->tm_mon;
    qtime->tm_year = tms->tm_year;
    qtime->tm_wday = tms->tm_wday;
    qtime->tm_yday = tms->tm_yday;
    qtime->tm_isdst = tms->tm_isdst;
  }
  else
  {
    Com_Memset(qtime, 0, sizeof(qtime_t));
  }

  return t;
}


/*
==============================================================================

						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

The zone calls are pretty much only used for small strings and structures,
all big things are allocated on the hunk.
==============================================================================
*/

#define	ZONEID	0x1d4a11
#define MINFRAGMENT	64

#if defined(USE_MULTI_SEGMENT)
#if 1 // forward lookup, faster allocation
#define DIRECTION next
// we may have up to 4 lists to group free blocks by size
//#define TINY_SIZE	32
#define SMALL_SIZE	64
#define MEDIUM_SIZE	128
#else // backward lookup, better free space consolidation
#define DIRECTION prev
#define TINY_SIZE	64
#define SMALL_SIZE	128
#define MEDIUM_SIZE	256
#endif
#endif

#define USE_STATIC_TAGS
#define USE_TRASH_TEST

#if defined(ZONE_DEBUG)
typedef struct zonedebug_s {
	const qchar *label;
	const qchar *file;
	qint line;
	qint allocSize;
} zonedebug_t;
#endif

typedef struct memblock_s {
	struct memblock_s	*next, *prev;
	qint			size;	// including the header and possibly tiny fragments
	memtag_t	tag;	// a tag of 0 is a free block
	qint			id;		// should be ZONEID
#if defined(ZONE_DEBUG)
	zonedebug_t d;
#endif
} memblock_t;

typedef struct freeblock_s {
	struct freeblock_s *prev;
	struct freeblock_s *next;
} freeblock_t;

typedef struct memzone_s {
	qint		size;			// total bytes malloced, including header
	qint		used;			// total bytes used
	memblock_t	blocklist;	// start / end cap for linked list
#if defined(USE_MULTI_SEGMENT)
	memblock_t	dummy0;		// just to allocate some space before freelist
	freeblock_t	freelist_tiny;
	memblock_t	dummy1;
	freeblock_t	freelist_small;
	memblock_t	dummy2;
	freeblock_t	freelist_medium;
	memblock_t	dummy3;
	freeblock_t	freelist;
#else
	memblock_t	*rover;
#endif
} memzone_t;

static qint minfragment = MINFRAGMENT; // may be adjusted at runtime

// main zone for all "dynamic" memory allocation
static memzone_t *mainzone;

// we also have a small zone for small allocations that would only
// fragment the main zone (think of cvar and cmd strings)
static memzone_t *smallzone;


#if defined(USE_MULTI_SEGMENT)

static void InitFree( freeblock_t *fb )
{
	memblock_t *block = (memblock_t*)( (byte*)fb - sizeof( memblock_t ) );
	Com_Memset( block, 0, sizeof( *block ) );
}


static void RemoveFree( memblock_t *block )
{
	freeblock_t *fb = (freeblock_t*)( block + 1 );
	freeblock_t *prev;
	freeblock_t *next;

#if defined(ZONE_DEBUG)
	if ( fb->next == NULL || fb->prev == NULL || fb->next == fb || fb->prev == fb ) {
		Com_Error( ERR_FATAL, "RemoveFree: bad pointers fb->next: %p, fb->prev: %p\n", fb->next, fb->prev );
	}
#endif

	prev = fb->prev;
	next = fb->next;

	prev->next = next;
	next->prev = prev;
}


static void InsertFree( memzone_t *zone, memblock_t *block )
{
	freeblock_t *fb = (freeblock_t*)( block + 1 );
	freeblock_t *prev, *next;
#if defined(TINY_SIZE)
	if ( block->size <= TINY_SIZE )
		prev = &zone->freelist_tiny;
	else
#endif
#if defined(SMALL_SIZE)
	if ( block->size <= SMALL_SIZE )
		prev = &zone->freelist_small;
	else
#endif
#if defined(MEDIUM_SIZE)
	if ( block->size <= MEDIUM_SIZE )
		prev = &zone->freelist_medium;
	else
#endif
		prev = &zone->freelist;

	next = prev->next;

#if defined(ZONE_DEBUG)
	if ( block->size < sizeof( *fb ) + sizeof( *block ) ) {
		Com_Error( ERR_FATAL, "InsertFree: bad block size: %i\n", block->size );
	}
#endif

	prev->next = fb;
	next->prev = fb;

	fb->prev = prev;
	fb->next = next;
}


/*
================
NewBlock

Allocates new free block within specified memory zone

Separator is needed to avoid additional runtime checks in Z_Free()
to prevent merging it with previous free block
================
*/
static freeblock_t *NewBlock( memzone_t *zone, qint size )
{
	memblock_t *prev, *next;
	memblock_t *block, *sep;
	qint alloc_size;

	// zone->prev is pointing on last block in the list
	prev = zone->blocklist.prev;
	next = prev->next;

	size = PAD( size, 1<<21 ); // round up to 2M blocks
	// allocate separator block before new free block
	alloc_size = size + sizeof( *sep );

	sep = (memblock_t *) calloc( alloc_size, 1 );
	if ( sep == NULL ) {
		Com_Error( ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes from the %s zone",
			size, zone == smallzone ? "small" : "main" );
		return NULL;
	}
	block = sep+1;

	// link separator with prev
	prev->next = sep;
	sep->prev = prev;

	// link separator with block
	sep->next = block;
	block->prev = sep;

	// link block with next
	block->next = next;
	next->prev = block;

	sep->tag = TAG_GENERAL; // in-use block
	sep->id = -ZONEID;
	sep->size = 0;

	block->tag = TAG_FREE;
	block->id = ZONEID;
	block->size = size;

	// update zone statistics
	zone->size += alloc_size;
	zone->used += sizeof( *sep );

	InsertFree( zone, block );

	return (freeblock_t*)( block + 1 );
}


static memblock_t *SearchFree( memzone_t *zone, qint size )
{
	const freeblock_t *fb;
	memblock_t *base;

#if defined(TINY_SIZE)
	if ( size <= TINY_SIZE )
		fb = zone->freelist_tiny.DIRECTION;
	else
#endif
#if defined(SMALL_SIZE)
	if ( size <= SMALL_SIZE )
		fb = zone->freelist_small.DIRECTION;
	else
#endif
#if defined(MEDIUM_SIZE)
	if ( size <= MEDIUM_SIZE )
		fb = zone->freelist_medium.DIRECTION;
	else
#endif
		fb = zone->freelist.DIRECTION;

	for ( ;; ) {
		// not found, allocate new segment?
		if ( fb == &zone->freelist ) {
			fb = NewBlock( zone, size );
		} else {
#if defined(TINY_SIZE)
			if ( fb == &zone->freelist_tiny ) {
				fb = zone->freelist_small.DIRECTION;
				continue;
			}
#endif
#if defined(SMALL_SIZE)
			if ( fb == &zone->freelist_small ) {
				fb = zone->freelist_medium.DIRECTION;
				continue;
			}
#endif
#if defined(MEDIUM_SIZE)
			if ( fb == &zone->freelist_medium ) {
				fb = zone->freelist.DIRECTION;
				continue;
			}
#endif
		}
		base = (memblock_t*)( (byte*) fb - sizeof( *base ) );
		fb = fb->DIRECTION;
		if ( base->size >= size ) {
			return base;
		}
	}
	return NULL;
}
#endif // USE_MULTI_SEGMENT


/*
========================
Z_ClearZone
========================
*/
static void Z_ClearZone( memzone_t *zone, memzone_t *head, qint size, qint segnum ) {
	memblock_t	*block;
	qint min_fragment;

#if defined(USE_MULTI_SEGMENT)
	min_fragment = sizeof( memblock_t ) + sizeof( freeblock_t );
#else
	min_fragment = sizeof( memblock_t );
#endif

	if ( minfragment < min_fragment ) {
		// in debug mode size of memblock_t may exceed MINFRAGMENT
		minfragment = PAD( min_fragment, sizeof( intptr_t ) );
		Com_DPrintf( "zone.minfragment adjusted to %i bytes\n", minfragment );
	}

	// set the entire zone to one free block
	zone->blocklist.next = zone->blocklist.prev = block = (memblock_t *)( zone + 1 );
	zone->blocklist.tag = TAG_GENERAL; // in use block
	zone->blocklist.id = -ZONEID;
	zone->blocklist.size = 0;
#if !defined(USE_MULTI_SEGMENT)
	zone->rover = block;
#endif
	zone->size = size;
	zone->used = 0;

	block->prev = block->next = &zone->blocklist;
	block->tag = TAG_FREE;	// free block
	block->id = ZONEID;

	block->size = size - sizeof(memzone_t);

#if defined(USE_MULTI_SEGMENT)
	InitFree( &zone->freelist );
	zone->freelist.next = zone->freelist.prev = &zone->freelist;

	InitFree( &zone->freelist_medium );
	zone->freelist_medium.next = zone->freelist_medium.prev = &zone->freelist_medium;

	InitFree( &zone->freelist_small );
	zone->freelist_small.next = zone->freelist_small.prev = &zone->freelist_small;

	InitFree( &zone->freelist_tiny );
	zone->freelist_tiny.next = zone->freelist_tiny.prev = &zone->freelist_tiny;

	InsertFree( zone, block );
#endif
}


/*
========================
Z_AvailableZoneMemory
========================
*/
static qint Z_AvailableZoneMemory( const memzone_t *zone ) {
#if defined(USE_MULTI_SEGMENT)
	return (1024*1024*1024); // unlimited
#else
	return zone->size - zone->used;
#endif
}


/*
========================
Z_AvailableMemory
========================
*/
qint Z_AvailableMemory( void ) {
	return Z_AvailableZoneMemory( mainzone );
}


static void MergeBlock( memblock_t *curr_free, const memblock_t *next )
{
	curr_free->size += next->size;
	curr_free->next = next->next;
	curr_free->next->prev = curr_free;
}


/*
========================
Z_Free
========================
*/
void Z_Free( void *ptr ) {
	memblock_t	*block, *other;
	memzone_t *zone;

	if (!ptr) {
		Com_Error( ERR_DROP, "Z_Free: NULL pointer" );
	}

	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID) {
		Com_Error( ERR_FATAL, "Z_Free: freed a pointer without ZONEID" );
	}

	if (block->tag == TAG_FREE) {
		Com_Error( ERR_FATAL, "Z_Free: freed a freed pointer" );
	}

	// if static memory
#if defined(USE_STATIC_TAGS)
	if (block->tag == TAG_STATIC) {
		return;
	}
#endif

	// check the memory trash tester
#if defined(USE_TRASH_TEST)
	if ( *(qint *)((byte *)block + block->size - 4 ) != ZONEID ) {
		Com_Error( ERR_FATAL, "Z_Free: memory block wrote past end" );
	}
#endif

	if ( block->tag == TAG_SMALL ) {
		zone = smallzone;
	} else {
		zone = mainzone;
	}

	zone->used -= block->size;

	// set the block to something that should cause problems
	// if it is referenced...
	Com_Memset( ptr, 0xaa, block->size - sizeof( *block ) );

	block->tag = TAG_FREE; // mark as free
	block->id = ZONEID;

	other = block->prev;
	if ( other->tag == TAG_FREE ) {
#if defined(USE_MULTI_SEGMENT)
		RemoveFree( other );
#endif
		// merge with previous free block
		MergeBlock( other, block );
#if !defined(USE_MULTI_SEGMENT)
		if ( block == zone->rover ) {
			zone->rover = other;
		}
#endif
		block = other;
	}

#if !defined(USE_MULTI_SEGMENT)
	zone->rover = block;
#endif

	other = block->next;
	if ( other->tag == TAG_FREE ) {
#if defined(USE_MULTI_SEGMENT)
		RemoveFree( other );
#endif
		// merge the next free block onto the end
		MergeBlock( block, other );
	}

#if defined(USE_MULTI_SEGMENT)
	InsertFree( zone, block );
#endif
}


/*
================
Z_FreeTags
================
*/
qint Z_FreeTags( memtag_t tag ) {
	qint			count;
	memzone_t	*zone;
	memblock_t	*block, *freed;

	if ( tag == TAG_STATIC ) {
		Com_Error( ERR_FATAL, "Z_FreeTags( TAG_STATIC )" );
		return 0;
	} else if ( tag == TAG_SMALL ) {
		zone = smallzone;
	} else {
		zone = mainzone;
	}

	count = 0;
	for ( block = zone->blocklist.next ; ; ) {
		if ( block->tag == tag && block->id == ZONEID ) {
			if ( block->prev->tag == TAG_FREE )
				freed = block->prev;  // current block will be merged with previous
			else
				freed = block; // will leave in place
			Z_Free( (void*)( block + 1 ) );
			block = freed;
			count++;
		}
		if ( block->next == &zone->blocklist ) {
			break;	// all blocks have been hit
		}
		block = block->next;
	}

	return count;
}


/*
================
Z_TagMalloc
================
*/
#if defined(ZONE_DEBUG)
void *Z_TagMallocDebug( qint size, memtag_t tag, qchar *label, qchar *file, qint line ) {
	qint		allocSize;
#else
void *Z_TagMalloc( qint size, memtag_t tag ) {
#endif
	qint		extra;
#if !defined(USE_MULTI_SEGMENT)
	memblock_t	*start, *rover;
#endif
	memblock_t *base;
	memzone_t *zone;

	if ( tag == TAG_FREE ) {
		Com_Error( ERR_FATAL, "Z_TagMalloc: tried to use with TAG_FREE" );
	}

	if ( tag == TAG_SMALL ) {
		zone = smallzone;
	} else {
		zone = mainzone;
	}

#if defined(ZONE_DEBUG)
	allocSize = size;
#endif

#if defined(USE_MULTI_SEGMENT)
	if ( size < (sizeof( freeblock_t ) ) ) {
		size = (sizeof( freeblock_t ) );
	}
#endif

	//
	// scan through the block list looking for the first free block
	// of sufficient size
	//
	size += sizeof( *base );	// account for size of block header
#if defined(USE_TRASH_TEST)
	size += 4;					// space for memory trash tester
#endif

	size = PAD(size, sizeof(intptr_t));		// align to 32/64 bit boundary

#if defined(USE_MULTI_SEGMENT)
	base = SearchFree( zone, size );

	RemoveFree( base );
#else

	base = rover = zone->rover;
	start = base->prev;

	do {
		if ( rover == start ) {
			// scanned all the way around the list
#if defined(ZONE_DEBUG)
			Z_LogHeap();
			Com_Error( ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes from the %s zone: %s, line: %d (%s)",
								size, zone == smallzone ? "small" : "main", file, line, label );
#else
			Com_Error( ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes from the %s zone",
								size, zone == smallzone ? "small" : "main" );
#endif
			return NULL;
		}
		if ( rover->tag != TAG_FREE ) {
			base = rover = rover->next;
		} else {
			rover = rover->next;
		}
	} while (base->tag != TAG_FREE || base->size < size);
#endif

	//
	// found a block big enough
	//
	extra = base->size - size;
	if ( extra >= minfragment ) {
		memblock_t *fragment;
		// there will be a free fragment after the allocated block
		fragment = (memblock_t *)( (byte *)base + size );
		fragment->size = extra;
		fragment->tag = TAG_FREE; // free block
		fragment->id = ZONEID;
		fragment->prev = base;
		fragment->next = base->next;
		fragment->next->prev = fragment;
		base->next = fragment;
		base->size = size;
#if defined(USE_MULTI_SEGMENT)
		InsertFree( zone, fragment );
#endif
	}

#if !defined(USE_MULTI_SEGMENT)
	zone->rover = base->next;	// next allocation will start looking here
#endif
	zone->used += base->size;

	base->tag = tag;			// no longer a free block
	base->id = ZONEID;

#if defined(ZONE_DEBUG)
	base->d.label = label;
	base->d.file = file;
	base->d.line = line;
	base->d.allocSize = allocSize;
#endif

#if defined(USE_TRASH_TEST)
	// marker for memory trash testing
	*(qint *)((byte *)base + base->size - 4) = ZONEID;
#endif

	return (void *) ( base + 1 );
}


/*
========================
Z_Malloc
========================
*/
#if defined(ZONE_DEBUG)
void *Z_MallocDebug( qint size, qchar *label, qchar *file, qint line ) {
#else
void *Z_Malloc( qint size ) {
#endif
	void	*buf;

  //Z_CheckHeap ();	// DEBUG

#if defined(ZONE_DEBUG)
	buf = Z_TagMallocDebug( size, TAG_GENERAL, label, file, line );
#else
	buf = Z_TagMalloc( size, TAG_GENERAL );
#endif
	Com_Memset( buf, 0, size );

	return buf;
}


/*
========================
S_Malloc
========================
*/
#if defined(ZONE_DEBUG)
void *S_MallocDebug( qint size, qchar *label, qchar *file, qint line ) {
	return Z_TagMallocDebug( size, TAG_SMALL, label, file, line );
}
#else
void *S_Malloc( qint size ) {
	return Z_TagMalloc( size, TAG_SMALL );
}
#endif


/*
========================
Z_CheckHeap
========================
*/
static void Z_CheckHeap( void ) {
	const memblock_t *block;
	const memzone_t *zone;

	zone =  mainzone;
	for ( block = zone->blocklist.next ; ; ) {
		if ( block->next == &zone->blocklist ) {
			break;	// all blocks have been hit
		}
		if ( (byte *)block + block->size != (byte *)block->next) {
#if defined(USE_MULTI_SEGMENT)
			const memblock_t *next = block->next;
			if ( next->size == 0 && next->id == -ZONEID && next->tag == TAG_GENERAL ) {
				block = next; // new zone segment
			} else
#endif
			Com_Error( ERR_FATAL, "Z_CheckHeap: block size does not touch the next block" );
		}
		if ( block->next->prev != block) {
			Com_Error( ERR_FATAL, "Z_CheckHeap: next block doesn't have proper back link" );
		}
		if ( block->tag == TAG_FREE && block->next->tag == TAG_FREE ) {
			Com_Error( ERR_FATAL, "Z_CheckHeap: two consecutive free blocks" );
		}
		block = block->next;
	}
}


/*
========================
Z_LogZoneHeap
========================
*/
static void Z_LogZoneHeap( memzone_t *zone, const qchar *name ) {
#if defined(ZONE_DEBUG)
	qchar dump[32], *ptr;
	qint  i, j;
#endif
	memblock_t	*block;
	qchar		buf[4096];
	qint size, allocSize, numBlocks;
	qint len;

	if ( logfile == FS_INVALID_HANDLE || !FS_Initialized() )
		return;

	size = numBlocks = 0;
#if defined(ZONE_DEBUG)
	allocSize = 0;
#endif
	len = Com_sprintf( buf, sizeof(buf), "\r\n================\r\n%s log\r\n================\r\n", name );
	FS_Write( buf, len, logfile );
	for ( block = zone->blocklist.next ; ; ) {
		if ( block->tag != TAG_FREE ) {
#if defined(ZONE_DEBUG)
			ptr = ((qchar *) block) + sizeof(memblock_t);
			j = 0;
			for (i = 0; i < 20 && i < block->d.allocSize; i++) {
				if (ptr[i] >= 32 && ptr[i] < 127) {
					dump[j++] = ptr[i];
				}
				else {
					dump[j++] = '_';
				}
			}
			dump[j] = '\0';
			len = Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s) [%s]\r\n", block->d.allocSize, block->d.file, block->d.line, block->d.label, dump);
			FS_Write( buf, len, logfile );
			allocSize += block->d.allocSize;
#endif
			size += block->size;
			numBlocks++;
		}
		if ( block->next == &zone->blocklist ) {
			break; // all blocks have been hit
		}
		block = block->next;
	}
#if defined(ZONE_DEBUG)
	// subtract debug memory
	size -= numBlocks * sizeof(zonedebug_t);
#else
	allocSize = numBlocks * sizeof(memblock_t); // + 32 bit alignment
#endif
	len = Com_sprintf( buf, sizeof( buf ), "%d %s memory in %d blocks\r\n", size, name, numBlocks );
	FS_Write( buf, len, logfile );
	len = Com_sprintf( buf, sizeof( buf ), "%d %s memory overhead\r\n", size - allocSize, name );
	FS_Write( buf, len, logfile );
	FS_Flush( logfile );
}


/*
========================
Z_LogHeap
========================
*/
void Z_LogHeap( void ) {
	Z_LogZoneHeap( mainzone, "MAIN" );
	Z_LogZoneHeap( smallzone, "SMALL" );
}

#if defined(USE_STATIC_TAGS)

// static mem blocks to reduce a lot of small zone overhead
typedef struct memstatic_s {
	memblock_t b;
	byte mem[2];
} memstatic_t;

#define MEM_STATIC(chr) { { NULL, NULL, PAD(sizeof(memstatic_t),4), TAG_STATIC, ZONEID }, {chr,'\0'} }

static const memstatic_t emptystring =
	MEM_STATIC( '\0' );

static const memstatic_t numberstring[] = {
	MEM_STATIC( '0' ),
	MEM_STATIC( '1' ),
	MEM_STATIC( '2' ),
	MEM_STATIC( '3' ),
	MEM_STATIC( '4' ),
	MEM_STATIC( '5' ),
	MEM_STATIC( '6' ),
	MEM_STATIC( '7' ),
	MEM_STATIC( '8' ),
	MEM_STATIC( '9' )
};
#endif // USE_STATIC_TAGS

/*
========================
CopyString

 NOTE:	never write over the memory CopyString returns because
		memory from a memstatic_t might be returned
========================
*/
qchar *CopyString( const qchar *in ) {
	qchar *out;
#if defined(USE_STATIC_TAGS)
	if ( in[0] == '\0' ) {
		return ((qchar *)&emptystring) + sizeof(memblock_t);
	}
	else if ( in[0] >= '0' && in[0] <= '9' && in[1] == '\0' ) {
		return ((qchar *)&numberstring[in[0]-'0']) + sizeof(memblock_t);
	}
#endif
	out = S_Malloc( strlen( in ) + 1 );
	strcpy( out, in );
	return out;
}


/*
==============================================================================

Goals:
	reproducible without history effects -- no out of memory errors on weird map to map changes
	allow restarting of the client without fragmentation
	minimize total pages in use at run time
	minimize total pages needed during load time

  Single block of memory with stack allocators coming from both ends towards the middle.

  One side is designated the temporary memory allocator.

  Temporary memory can be allocated and freed in any order.

  A highwater mark is kept of the most in use at any time.

  When there is no temporary memory allocated, the permanent and temp sides
  can be switched, allowing the already touched temp memory to be used for
  permanent storage.

  Temp memory must never be allocated on two ends at once, or fragmentation
  could occur.

  If we have any in-use temp memory, additional temp allocations must come from
  that side.

  If not, we can choose to make either side the new temp side and push future
  permanent allocations to the other side.  Permanent allocations should be
  kept on the side that has the current greatest wasted highwater mark.

==============================================================================
*/


#define	HUNK_MAGIC	0x89537892
#define	HUNK_FREE_MAGIC	0x89537893

typedef struct {
	unsigned qint magic;
	unsigned qint size;
} hunkHeader_t;

typedef struct {
	qint		mark;
	qint		permanent;
	qint		temp;
	qint		tempHighwater;
} hunkUsed_t;

typedef struct hunkblock_s {
	qint size;
	byte printed;
	struct hunkblock_s *next;
	const qchar *label;
	const qchar *file;
	qint line;
} hunkblock_t;

static	hunkblock_t *hunkblocks;

static	hunkUsed_t	hunk_low, hunk_high;
static	hunkUsed_t	*hunk_permanent, *hunk_temp;

static	byte	*s_hunkData = NULL;
static	qint		s_hunkTotal;

static const qchar *tagName[ TAG_COUNT ] = {
	"FREE",
	"GENERAL",
	"PACK",
	"SEARCH-PATH",
	"SEARCH-PACK",
	"SEARCH-DIR",
	"BOTLIB",
	"RENDERER",
	"CLIENTS",
	"SMALL",
	"STATIC"
};

typedef struct zone_stats_s {
	qint	zoneSegments;
	qint zoneBlocks;
	qint	zoneBytes;
	qint	botlibBytes;
	qint	rendererBytes;
	qint freeBytes;
	qint freeBlocks;
	qint freeSmallest;
	qint freeLargest;
} zone_stats_t;


static void Zone_Stats( const qchar *name, const memzone_t *z, qbool printDetails, zone_stats_t *stats )
{
	const memblock_t *block;
	const memzone_t *zone;
	zone_stats_t st;

	memset( &st, 0, sizeof( st ) );
	zone = z;
	st.zoneSegments = 1;
	st.freeSmallest = 0x7FFFFFFF;

	//if ( printDetails ) {
	//	Com_Printf( "---------- %s zone segment #%i ----------\n", name, zone->segnum );
	//}

	for ( block = zone->blocklist.next ; ; ) {
		if ( printDetails ) {
			qint tag = block->tag;
			Com_Printf( "block:%p  size:%8i  tag: %s\n", (void *)block, block->size,
				(unsigned)tag < TAG_COUNT ? tagName[ tag ] : va( NULL, "%i", tag ) );
		}
		if ( block->tag != TAG_FREE ) {
			st.zoneBytes += block->size;
			st.zoneBlocks++;
			if ( block->tag == TAG_BOTLIB ) {
				st.botlibBytes += block->size;
			} else if ( block->tag == TAG_RENDERER ) {
				st.rendererBytes += block->size;
			}
		} else {
			st.freeBytes += block->size;
			st.freeBlocks++;
			if ( block->size > st.freeLargest )
				st.freeLargest = block->size;
			if ( block->size < st.freeSmallest )
				st.freeSmallest = block->size;
		}
		if ( block->next == &zone->blocklist ) {
			break; // all blocks have been hit
		}
		if ( (byte *)block + block->size != (byte *)block->next) {
#if defined(USE_MULTI_SEGMENT)
			const memblock_t *next = block->next;
			if ( next->size == 0 && next->id == -ZONEID && next->tag == TAG_GENERAL ) {
				st.zoneSegments++;
				if ( printDetails ) {
					Com_Printf( "---------- %s zone segment #%i ----------\n", name, st.zoneSegments );
				}
				block = next->next;
				continue;
			} else
#endif
				Com_Printf( "ERROR: block size does not touch the next block\n" );
		}
		if ( block->next->prev != block) {
			Com_Printf( "ERROR: next block doesn't have proper back link\n" );
		}
		if ( block->tag == TAG_FREE && block->next->tag == TAG_FREE ) {
			Com_Printf( "ERROR: two consecutive free blocks\n" );
		}
		block = block->next;
	}

	// export stats
	if ( stats ) {
		memcpy( stats, &st, sizeof( *stats ) );
	}
}


/*
=================
Com_Meminfo_f
=================
*/
static void Com_Meminfo_f( void ) {
	zone_stats_t st;
	qint		unused;

	Com_Printf( "%8i bytes total hunk\n", s_hunkTotal );
	Com_Printf( "\n" );
	Com_Printf( "%8i low mark\n", hunk_low.mark );
	Com_Printf( "%8i low permanent\n", hunk_low.permanent );
	if ( hunk_low.temp != hunk_low.permanent ) {
		Com_Printf( "%8i low temp\n", hunk_low.temp );
	}
	Com_Printf( "%8i low tempHighwater\n", hunk_low.tempHighwater );
	Com_Printf( "\n" );
	Com_Printf( "%8i high mark\n", hunk_high.mark );
	Com_Printf( "%8i high permanent\n", hunk_high.permanent );
	if ( hunk_high.temp != hunk_high.permanent ) {
		Com_Printf( "%8i high temp\n", hunk_high.temp );
	}
	Com_Printf( "%8i high tempHighwater\n", hunk_high.tempHighwater );
	Com_Printf( "\n" );
	Com_Printf( "%8i total hunk in use\n", hunk_low.permanent + hunk_high.permanent );
	unused = 0;
	if ( hunk_low.tempHighwater > hunk_low.permanent ) {
		unused += hunk_low.tempHighwater - hunk_low.permanent;
	}
	if ( hunk_high.tempHighwater > hunk_high.permanent ) {
		unused += hunk_high.tempHighwater - hunk_high.permanent;
	}
	Com_Printf( "%8i unused highwater\n", unused );
	Com_Printf( "\n" );

	Zone_Stats( "main", mainzone, !Q_stricmp( Cmd_Argv(1), "main" ) || !Q_stricmp( Cmd_Argv(1), "all" ), &st );
	Com_Printf( "%8i bytes total main zone\n\n", mainzone->size );
	Com_Printf( "%8i bytes in %i main zone blocks%s\n", st.zoneBytes, st.zoneBlocks,
		st.zoneSegments > 1 ? va( NULL, " and %i segments", st.zoneSegments ) : "" );
	Com_Printf( "        %8i bytes in botlib\n", st.botlibBytes );
	Com_Printf( "        %8i bytes in renderer\n", st.rendererBytes );
	Com_Printf( "        %8i bytes in other\n", st.zoneBytes - ( st.botlibBytes + st.rendererBytes ) );
	Com_Printf( "        %8i bytes in %i free blocks\n", st.freeBytes, st.freeBlocks );
	if ( st.freeBlocks > 1 ) {
		Com_Printf( "        (largest: %i bytes, smallest: %i bytes)\n\n", st.freeLargest, st.freeSmallest );
	}

	Zone_Stats( "small", smallzone, !Q_stricmp( Cmd_Argv(1), "small" ) || !Q_stricmp( Cmd_Argv(1), "all" ), &st );
	Com_Printf( "%8i bytes total small zone\n\n", smallzone->size );
	Com_Printf( "%8i bytes in %i small zone blocks%s\n", st.zoneBytes, st.zoneBlocks,
		st.zoneSegments > 1 ? va( NULL, " and %i segments", st.zoneSegments ) : "" );
	Com_Printf( "        %8i bytes in %i free blocks\n", st.freeBytes, st.freeBlocks );
	if ( st.freeBlocks > 1 ) {
		Com_Printf( "        (largest: %i bytes, smallest: %i bytes)\n\n", st.freeLargest, st.freeSmallest );
	}
}


/*
===============
Com_TouchMemory

Touch all known used data to make sure it is paged in
===============
*/
unsigned qint Com_TouchMemory( void ) {
	const memblock_t *block;
	const memzone_t *zone;
	qint		start, end;
	qint		i, j;
	unsigned qint sum;

	Z_CheckHeap();

	start = Sys_Milliseconds();

	sum = 0;

	j = hunk_low.permanent >> 2;
	for ( i = 0 ; i < j ; i+=64 ) {			// only need to touch each page
		sum += ((unsigned qint *)s_hunkData)[i];
	}

	i = ( s_hunkTotal - hunk_high.permanent ) >> 2;
	j = hunk_high.permanent >> 2;
	for (  ; i < j ; i+=64 ) {			// only need to touch each page
		sum += ((unsigned qint *)s_hunkData)[i];
	}

	zone = mainzone;
	for (block = zone->blocklist.next ; ; block = block->next) {
		if ( block->tag != TAG_FREE ) {
			j = block->size >> 2;
			for ( i = 0 ; i < j ; i+=64 ) {				// only need to touch each page
				sum += ((unsigned qint *)block)[i];
			}
		}
		if ( block->next == &zone->blocklist ) {
			break; // all blocks have been hit
		}
	}

	end = Sys_Milliseconds();

	Com_Printf( "Com_TouchMemory: %i msec\n", end - start );

	return sum; // just to silent compiler warning
}


/*
=================
Com_InitSmallZoneMemory
=================
*/
static void Com_InitSmallZoneMemory( void ) {
	static byte s_buf[ 512 * 1024 ];
	qint smallZoneSize;

	smallZoneSize = sizeof( s_buf );
	Com_Memset( s_buf, 0, smallZoneSize );
	smallzone = (memzone_t *)s_buf;
	Z_ClearZone( smallzone, smallzone, smallZoneSize, 1 );
}


/*
=================
Com_InitZoneMemory
=================
*/
static void Com_InitZoneMemory( void ) {
	qint		mainZoneSize;
	cvar_t	*cv;

	// Please note: com_zoneMegs can only be set on the command line, and
	// not in q3config.cfg or Com_StartupVariable, as they haven't been
	// executed by this point. It's a chicken and egg problem. We need the
	// memory manager configured to handle those places where you would
	// configure the memory manager.

	// allocate the random block zone
	cv = Cvar_GetAndDescribe( "com_zoneMegs", XSTRING( DEF_COMZONEMEGS ), CVAR_LATCH | CVAR_ARCHIVE, "Initial amount of memory (RAM) allocated for the main block zone (in MB)." );

#if !defined(USE_MULTI_SEGMENT)
	if ( cv->integer < DEF_COMZONEMEGS )
		mainZoneSize = 1024 * 1024 * DEF_COMZONEMEGS;
	else
#endif
		mainZoneSize = cv->integer * 1024 * 1024;

	mainzone = calloc( mainZoneSize, 1 );
	if ( !mainzone ) {
		Com_Error( ERR_FATAL, "Zone data failed to allocate %i megs", mainZoneSize / (1024*1024) );
	}
	Z_ClearZone( mainzone, mainzone, mainZoneSize, 1 );
}


/*
=================
Hunk_Log
=================
*/
void Hunk_Log( void ) {
	hunkblock_t	*block;
	qchar		buf[4096];
	qint size, numBlocks;

	if ( logfile == FS_INVALID_HANDLE || !FS_Initialized() )
		return;

	size = 0;
	numBlocks = 0;
	Com_sprintf(buf, sizeof(buf), "\r\n================\r\nHunk log\r\n================\r\n");
	FS_Write(buf, strlen(buf), logfile);
	for (block = hunkblocks ; block; block = block->next) {
#if defined(HUNK_DEBUG)
		Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", block->size, block->file, block->line, block->label);
		FS_Write(buf, strlen(buf), logfile);
#endif
		size += block->size;
		numBlocks++;
	}
	Com_sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", size);
	FS_Write(buf, strlen(buf), logfile);
	Com_sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
	FS_Write(buf, strlen(buf), logfile);
}


/*
=================
Hunk_SmallLog
=================
*/
#if defined(HUNK_DEBUG)
void Hunk_SmallLog( void ) {
	hunkblock_t	*block, *block2;
	qchar		buf[4096];
	qint size, locsize, numBlocks;

	if ( logfile == FS_INVALID_HANDLE || !FS_Initialized() )
		return;

	for (block = hunkblocks ; block; block = block->next) {
		block->printed = qfalse;
	}
	size = 0;
	numBlocks = 0;
	Com_sprintf(buf, sizeof(buf), "\r\n================\r\nHunk Small log\r\n================\r\n");
	FS_Write(buf, strlen(buf), logfile);
	for (block = hunkblocks; block; block = block->next) {
		if (block->printed) {
			continue;
		}
		locsize = block->size;
		for (block2 = block->next; block2; block2 = block2->next) {
			if (block->line != block2->line) {
				continue;
			}
			if (Q_stricmp(block->file, block2->file)) {
				continue;
			}
			size += block2->size;
			locsize += block2->size;
			block2->printed = qtrue;
		}
		Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", locsize, block->file, block->line, block->label);
		FS_Write(buf, strlen(buf), logfile);
		size += block->size;
		numBlocks++;
	}
	Com_sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", size);
	FS_Write(buf, strlen(buf), logfile);
	Com_sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
	FS_Write(buf, strlen(buf), logfile);
}
#endif


/*
=================
Com_InitHunkMemory
=================
*/
static void Com_InitHunkMemory( void ) {
	cvar_t	*cv;

	// make sure the file system has allocated and "not" freed any temp blocks
	// this allows the config and product id files ( journal files too ) to be loaded
	// by the file system without redundant routines in the file system utilizing different
	// memory systems
	if ( FS_LoadStack() != 0 ) {
		Com_Error( ERR_FATAL, "Hunk initialization failed. File system load stack not zero" );
	}

	// allocate the stack based hunk allocator
	cv = Cvar_GetAndDescribe( "com_hunkMegs", XSTRING( DEF_COMHUNKMEGS ), CVAR_LATCH | CVAR_ARCHIVE, "The size of the hunk memory segment." );
	Cvar_CheckRange(cv, XSTRING(MIN_COMHUNKMEGS), NULL, CV_INTEGER);

	s_hunkTotal = cv->integer * 1024 * 1024;

	s_hunkData = calloc( s_hunkTotal + 63, 1 );
	if ( !s_hunkData ) {
		Com_Error( ERR_FATAL, "Hunk data failed to allocate %i megs", s_hunkTotal / (1024*1024) );
	}

	// cacheline align
	s_hunkData = PADP( s_hunkData, 64 );
	Hunk_Clear();

	Cmd_AddCommand( "meminfo", Com_Meminfo_f );
#if defined(ZONE_DEBUG)
	Cmd_AddCommand( "zonelog", Z_LogHeap );
#endif
#if defined(HUNK_DEBUG)
	Cmd_AddCommand( "hunklog", Hunk_Log );
	Cmd_AddCommand( "hunksmalllog", Hunk_SmallLog );
#endif
}


/*
====================
Hunk_MemoryRemaining
====================
*/
qint	Hunk_MemoryRemaining( void ) {
	qint		low, high;

	low = hunk_low.permanent > hunk_low.temp ? hunk_low.permanent : hunk_low.temp;
	high = hunk_high.permanent > hunk_high.temp ? hunk_high.permanent : hunk_high.temp;

	return s_hunkTotal - ( low + high );
}


/*
===================
Hunk_SetMark

The server calls this after the level and game VM have been loaded
===================
*/
void Hunk_SetMark( void ) {
	hunk_low.mark = hunk_low.permanent;
	hunk_high.mark = hunk_high.permanent;
}


/*
=================
Hunk_ClearToMark

The client calls this before starting a vid_restart or snd_restart
=================
*/
void Hunk_ClearToMark( void ) {
	hunk_low.permanent = hunk_low.temp = hunk_low.mark;
	hunk_high.permanent = hunk_high.temp = hunk_high.mark;
}


/*
=================
Hunk_CheckMark
=================
*/
qbool Hunk_CheckMark( void ) {
	if( hunk_low.mark || hunk_high.mark ) {
		return qtrue;
	}
	return qfalse;
}

void CL_ShutdownCGame( void );
void CL_ShutdownUI( void );
void SV_ShutdownGameProgs( void );

/*
=================
Hunk_Clear

The server calls this before shutting down or loading a new map
=================
*/
void Hunk_Clear( void ) {

#if !defined(DEDICATED)
	CL_ShutdownCGame();
	CL_ShutdownUI();
#endif
	SV_ShutdownGameProgs();
#if !defined(DEDICATED)
	CIN_CloseAllVideos();
#endif
	hunk_low.mark = 0;
	hunk_low.permanent = 0;
	hunk_low.temp = 0;
	hunk_low.tempHighwater = 0;

	hunk_high.mark = 0;
	hunk_high.permanent = 0;
	hunk_high.temp = 0;
	hunk_high.tempHighwater = 0;

	hunk_permanent = &hunk_low;
	hunk_temp = &hunk_high;

	Com_Printf( "Hunk_Clear: reset the hunk ok\n" );
	VM_Clear();
#if defined(HUNK_DEBUG)
	hunkblocks = NULL;
#endif
}


static void Hunk_SwapBanks( void ) {
	hunkUsed_t	*swap;

	// can't swap banks if there is any temp already allocated
	if ( hunk_temp->temp != hunk_temp->permanent ) {
		return;
	}

	// if we have a larger highwater mark on this side, start making
	// our permanent allocations here and use the other side for temp
	if ( hunk_temp->tempHighwater - hunk_temp->permanent >
		hunk_permanent->tempHighwater - hunk_permanent->permanent ) {
		swap = hunk_temp;
		hunk_temp = hunk_permanent;
		hunk_permanent = swap;
	}
}


/*
=================
Hunk_Alloc

Allocate permanent (until the hunk is cleared) memory
=================
*/
#if defined(HUNK_DEBUG)
void *Hunk_AllocDebug( qint size, ha_pref preference, qchar *label, qchar *file, qint line ) {
#else
void *Hunk_Alloc( qint size, ha_pref preference ) {
#endif
	void	*buf;

	if ( s_hunkData == NULL)
	{
		Com_Error( ERR_FATAL, "Hunk_Alloc: Hunk memory system not initialized" );
	}

	// can't do preference if there is any temp allocated
	if (preference == h_dontcare || hunk_temp->temp != hunk_temp->permanent) {
		Hunk_SwapBanks();
	} else {
		if (preference == h_low && hunk_permanent != &hunk_low) {
			Hunk_SwapBanks();
		} else if (preference == h_high && hunk_permanent != &hunk_high) {
			Hunk_SwapBanks();
		}
	}

#if defined(HUNK_DEBUG)
	size += sizeof(hunkblock_t);
#endif

	// round to cacheline
	size = PAD( size, 64 );

	if ( hunk_low.temp + hunk_high.temp + size > s_hunkTotal ) {
#if defined(HUNK_DEBUG)
		Hunk_Log();
		Hunk_SmallLog();

		Com_Error(ERR_DROP, "Hunk_Alloc failed on %i: %s, line: %d (%s)", size, file, line, label);
#else
		Com_Error(ERR_DROP, "Hunk_Alloc failed on %i", size);
#endif
	}

	if ( hunk_permanent == &hunk_low ) {
		buf = (void *)(s_hunkData + hunk_permanent->permanent);
		hunk_permanent->permanent += size;
	} else {
		hunk_permanent->permanent += size;
		buf = (void *)(s_hunkData + s_hunkTotal - hunk_permanent->permanent );
	}

	hunk_permanent->temp = hunk_permanent->permanent;

	Com_Memset( buf, 0, size );

#if defined(HUNK_DEBUG)
	{
		hunkblock_t *block;

		block = (hunkblock_t *) buf;
		block->size = size - sizeof(hunkblock_t);
		block->file = file;
		block->label = label;
		block->line = line;
		block->next = hunkblocks;
		hunkblocks = block;
		buf = ((byte *) buf) + sizeof(hunkblock_t);
	}
#endif
	return buf;
}


/*
=================
Hunk_AllocateTempMemory

This is used by the file loading system.
Multiple files can be loaded in temporary memory.
When the files-in-use count reaches zero, all temp memory will be deleted
=================
*/
void *Hunk_AllocateTempMemory( qint size ) {
	void		*buf;
	hunkHeader_t	*hdr;

	// return a Z_Malloc'd block if the hunk has not been initialized
	// this allows the config and product id files ( journal files too ) to be loaded
	// by the file system without redundant routines in the file system utilizing different
	// memory systems
	if ( s_hunkData == NULL )
	{
		return Z_Malloc(size);
	}

	Hunk_SwapBanks();

	size = PAD(size, sizeof(intptr_t)) + sizeof( hunkHeader_t );

	if ( hunk_temp->temp + hunk_permanent->permanent + size > s_hunkTotal ) {
		Com_Error( ERR_DROP, "Hunk_AllocateTempMemory: failed on %i", size );
	}

	if ( hunk_temp == &hunk_low ) {
		buf = (void *)(s_hunkData + hunk_temp->temp);
		hunk_temp->temp += size;
	} else {
		hunk_temp->temp += size;
		buf = (void *)(s_hunkData + s_hunkTotal - hunk_temp->temp );
	}

	if ( hunk_temp->temp > hunk_temp->tempHighwater ) {
		hunk_temp->tempHighwater = hunk_temp->temp;
	}

	hdr = (hunkHeader_t *)buf;
	buf = (void *)(hdr+1);

	hdr->magic = HUNK_MAGIC;
	hdr->size = size;

	// don't bother clearing, because we are going to load a file over it
	return buf;
}


/*
==================
Hunk_FreeTempMemory
==================
*/
void Hunk_FreeTempMemory( void *buf ) {
	hunkHeader_t	*hdr;

	// free with Z_Free if the hunk has not been initialized
	// this allows the config and product id files ( journal files too ) to be loaded
	// by the file system without redundant routines in the file system utilizing different
	// memory systems
	if ( s_hunkData == NULL )
	{
		Z_Free(buf);
		return;
	}

	hdr = ( (hunkHeader_t *)buf ) - 1;
	if ( hdr->magic != HUNK_MAGIC ) {
		Com_Error( ERR_FATAL, "Hunk_FreeTempMemory: bad magic" );
	}

	hdr->magic = HUNK_FREE_MAGIC;

	// this only works if the files are freed in stack order,
	// otherwise the memory will stay around until Hunk_ClearTempMemory
	if ( hunk_temp == &hunk_low ) {
		if ( hdr == (void *)(s_hunkData + hunk_temp->temp - hdr->size ) ) {
			hunk_temp->temp -= hdr->size;
		} else {
			Com_Printf( "Hunk_FreeTempMemory: not the final block\n" );
		}
	} else {
		if ( hdr == (void *)(s_hunkData + s_hunkTotal - hunk_temp->temp ) ) {
			hunk_temp->temp -= hdr->size;
		} else {
			Com_Printf( "Hunk_FreeTempMemory: not the final block\n" );
		}
	}
}


/*
=================
Hunk_ClearTempMemory

The temp space is no longer needed.  If we have left more
touched but unused memory on this side, have future
permanent allocs use this side.
=================
*/
void Hunk_ClearTempMemory( void ) {
	if ( s_hunkData != NULL ) {
		hunk_temp->temp = hunk_temp->permanent;
	}
}

/*
===================================================================

EVENTS AND JOURNALING

In addition to these events, .cfg files are also copied to the
journaled file
===================================================================
*/

#define	MAX_PUSHED_EVENTS	            1024
static qint com_pushedEventsHead = 0;
static qint com_pushedEventsTail = 0;
static sysEvent_t	com_pushedEvents[MAX_PUSHED_EVENTS];

/*
=================
Com_InitJournaling
=================
*/
static void Com_InitJournaling( void ) {
	if ( !com_journal->integer ) {
		return;
	}

	if ( com_journal->integer == 1 ) {
		Com_Printf( "Journaling events\n" );
		com_journalFile = FS_FOpenFileWrite( "journal.dat" );
		com_journalDataFile = FS_FOpenFileWrite( "journaldata.dat" );
	} else if ( com_journal->integer == 2 ) {
		Com_Printf( "Replaying journaled events\n" );
		FS_FOpenFileRead( "journal.dat", &com_journalFile, qtrue );
		FS_FOpenFileRead( "journaldata.dat", &com_journalDataFile, qtrue );
	}

	if ( com_journalFile == FS_INVALID_HANDLE || com_journalDataFile == FS_INVALID_HANDLE ) {
		Cvar_Set( "com_journal", "0" );
		if ( com_journalFile != FS_INVALID_HANDLE ) {
			FS_FCloseFile( com_journalFile );
			com_journalFile = FS_INVALID_HANDLE;
		}
		if ( com_journalDataFile != FS_INVALID_HANDLE ) {
			FS_FCloseFile( com_journalDataFile );
			com_journalDataFile = FS_INVALID_HANDLE;
		}
		Com_Printf( "Couldn't open journal files\n" );
	}
}

/*
========================================================================

EVENT LOOP

========================================================================
*/

#define MAX_QUEUED_EVENTS  256
#define MASK_QUEUED_EVENTS ( MAX_QUEUED_EVENTS - 1 )

static sysEvent_t  eventQueue[ MAX_QUEUED_EVENTS ];
static qint         eventHead = 0;
static qint         eventTail = 0;

/*
================
Com_QueueEvent

A time of 0 will get the current time
Ptr should either be null, or point to a block of data that can
be freed by the game later.
================
*/
void Com_QueueEvent( qint time, sysEventType_t type, qint value, qint value2, qint ptrLength, void *ptr )
{
	sysEvent_t  *ev;

	ev = &eventQueue[ eventHead & MASK_QUEUED_EVENTS ];

	if ( eventHead - eventTail >= MAX_QUEUED_EVENTS )
	{
		Com_Printf("Com_QueueEvent: overflow\n");
		// we are discarding an event, but don't leak memory
		if ( ev->evPtr )
		{
			Z_Free( ev->evPtr );
		}
		eventTail++;
	}

	eventHead++;

	if ( time == 0 )
	{
		time = Sys_Milliseconds();
	}

	ev->evTime = time;
	ev->evType = type;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;
}

/*
================
Com_GetSystemEvent

================
*/
static sysEvent_t Com_GetSystemEvent( void )
{
	sysEvent_t  ev;
	qchar        *s;

	// return if we have data
	if ( eventHead > eventTail )
	{
		eventTail++;
		return eventQueue[ ( eventTail - 1 ) & MASK_QUEUED_EVENTS ];
	}

	// check for console commands
	s = Sys_ConsoleInput();
	if ( s )
	{
		qchar  *b;
		qint   len;

		len = strlen( s ) + 1;
		b = Z_Malloc( len );
		strcpy( b, s );
		Com_QueueEvent( 0, SE_CONSOLE, 0, 0, len, b );
	}

	// return if we have data
	if ( eventHead > eventTail )
	{
		eventTail++;
		return eventQueue[ ( eventTail - 1 ) & MASK_QUEUED_EVENTS ];
	}

	// create an empty event to return
	memset( &ev, 0, sizeof( ev ) );
	ev.evTime = Sys_Milliseconds();

	return ev;
}

/*
=================
Com_GetRealEvent
=================
*/
static sysEvent_t Com_GetRealEvent( void ) {
	
	// get or save an event from/to the journal file
	if ( com_journalFile != FS_INVALID_HANDLE ) {
		int			r;
		sysEvent_t	ev;

		if ( com_journal->integer == 2 ) {
			Sys_SendKeyEvents();
			r = FS_Read( &ev, sizeof(ev), com_journalFile );
			if ( r != sizeof(ev) ) {
				Com_Error( ERR_FATAL, "Error reading from journal file" );
			}
			if ( ev.evPtrLength ) {
				ev.evPtr = Z_Malloc( ev.evPtrLength );
				r = FS_Read( ev.evPtr, ev.evPtrLength, com_journalFile );
				if ( r != ev.evPtrLength ) {
					Com_Error( ERR_FATAL, "Error reading from journal file" );
				}
			}
		} else {
			ev = Com_GetSystemEvent();

			// write the journal value out if needed
			if ( com_journal->integer == 1 ) {
				r = FS_Write( &ev, sizeof(ev), com_journalFile );
				if ( r != sizeof(ev) ) {
					Com_Error( ERR_FATAL, "Error writing to journal file" );
				}
				if ( ev.evPtrLength ) {
					r = FS_Write( ev.evPtr, ev.evPtrLength, com_journalFile );
					if ( r != ev.evPtrLength ) {
						Com_Error( ERR_FATAL, "Error writing to journal file" );
					}
				}
			}
		}

		return ev;
	}

	return Com_GetSystemEvent();
}


/*
=================
Com_InitPushEvent
=================
*/
static void Com_InitPushEvent( void ) {
  // clear the static buffer array
  // this requires SE_NONE to be accepted as a valid but NOP event
  memset( com_pushedEvents, 0, sizeof(com_pushedEvents) );
  // reset counters while we are at it
  // beware: GetEvent might still return an SE_NONE from the buffer
  com_pushedEventsHead = 0;
  com_pushedEventsTail = 0;
}


/*
=================
Com_PushEvent
=================
*/
static void Com_PushEvent( const sysEvent_t *event ) {
	sysEvent_t		*ev;
	static qint printedWarning = 0;

	ev = &com_pushedEvents[ com_pushedEventsHead & (MAX_PUSHED_EVENTS-1) ];

	if ( com_pushedEventsHead - com_pushedEventsTail >= MAX_PUSHED_EVENTS ) {

		// don't print the warning constantly, or it can give time for more...
		if ( !printedWarning ) {
			printedWarning = qtrue;
			Com_Printf( "WARNING: Com_PushEvent overflow\n" );
		}

		if ( ev->evPtr ) {
			Z_Free( ev->evPtr );
		}
		com_pushedEventsTail++;
	} else {
		printedWarning = qfalse;
	}

	*ev = *event;
	com_pushedEventsHead++;
}

/*
=================
Com_GetEvent
=================
*/
static sysEvent_t	Com_GetEvent( void ) {
	if ( com_pushedEventsHead > com_pushedEventsTail ) {
		com_pushedEventsTail++;
		return com_pushedEvents[ (com_pushedEventsTail-1) & (MAX_PUSHED_EVENTS-1) ];
	}
	return Com_GetRealEvent();
}

/*
=================
Com_RunAndTimeServerPacket
=================
*/
void Com_RunAndTimeServerPacket( const netadr_t *evFrom, msg_t *buf ) {
	qint		t1, t2, msec;

	t1 = 0;

	if ( com_speeds->integer ) {
		t1 = Sys_Milliseconds ();
	}

	SV_ReadPackets( evFrom, buf );

	if ( com_speeds->integer ) {
		t2 = Sys_Milliseconds ();
		msec = t2 - t1;
		if ( com_speeds->integer == 3 ) {
			Com_Printf( "SV_ReadPackets time: %i\n", msec );
		}
	}
}

/*
=================
Com_EventLoop

Returns last event time
=================
*/
qint Com_EventLoop( void ) {
	sysEvent_t	ev;
	byte		bufData[MAX_MSGLEN_BUF];
	msg_t		buf;

	MSG_Init( &buf, bufData, MAX_MSGLEN );

	while ( 1 ) {
		ev = Com_GetEvent();

		// if no more events are available
		if ( ev.evType == SE_NONE ) {
			// manually send packet events for the loopback channel
#if !defined(DEDICATED)
                        netadr_t evFrom;
			while ( NET_GetLoopPacket( NS_CLIENT, &evFrom, &buf ) ) {
				CL_PacketEvent( &evFrom, &buf );
			}

			while ( NET_GetLoopPacket( NS_SERVER, &evFrom, &buf ) ) {
				// if the server just shut down, flush the events
				if ( com_sv_running->integer ) {
					Com_RunAndTimeServerPacket( &evFrom, &buf );
				}
			}
#endif
			return ev.evTime;
		}


		switch ( ev.evType ) {
#if !defined(DEDICATED)
		case SE_KEY:
			CL_KeyEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
		case SE_CHAR:
			CL_CharEvent( ev.evValue );
			break;
		case SE_MOUSE:
			CL_MouseEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
		case SE_JOYSTICK_AXIS:
			CL_JoystickEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
#endif
		case SE_CONSOLE:
			Cbuf_AddText( (qchar *)ev.evPtr );
			Cbuf_AddText( "\n" );
			break;
		default:
			Com_Error( ERR_FATAL, "Com_EventLoop: bad event type %i", ev.evType );
			break;
		}

		// free any block data
		if ( ev.evPtr ) {
			Z_Free( ev.evPtr );
			ev.evPtr = NULL;
		}
	}

	return 0;	// never reached
}

/*
================
Com_Milliseconds

Can be used for profiling, but will be journaled accurately
================
*/
qint Com_Milliseconds (void) {
	if ( com_journal->integer ) {
		sysEvent_t	ev;

		// get events and push them until we get a null event with the current time
		do {
			ev = Com_GetRealEvent();
			if ( ev.evType != SE_NONE ) {
				Com_PushEvent( &ev );
			}
		} while ( ev.evType != SE_NONE );

		return ev.evTime;
	}

	return Sys_Milliseconds();
}

//============================================================================

/*
=============
Com_Error_f

Just throw a fatal error to
test error shutdown procedures
=============
*/
static void Com_Error_f (void) {
	if ( Cmd_Argc() > 1 ) {
		Com_Error( ERR_DROP, "Testing drop error" );
	} else {
		Com_Error( ERR_FATAL, "Testing fatal error" );
	}
}


/*
=============
Com_Freeze_f

Just freeze in place for a given number of seconds to test
error recovery
=============
*/
static void Com_Freeze_f (void) {
	float	s;
	qint		start, now;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "freeze <seconds>\n" );
		return;
	}
	s = Q_atof( Cmd_Argv(1) );

	start = Com_Milliseconds();

	while ( 1 ) {
		now = Com_Milliseconds();
		if ( ( now - start ) * 0.001 > s ) {
			break;
		}
	}
}

/*
=================
Com_Crash_f

A way to force a bus error for development reasons
=================
*/
static void Com_Crash_f( void ) {
	*(volatile qint *) 0 = 0x12345678;
}

/*
=================
Com_Setenv_f

For controlling environment variables
=================
*/
void
Com_Setenv_f(void)
{
  const qint argc = Cmd_Argc();
  qchar *arg1 = Cmd_Argv(1);

  if (argc > 2)
  {
    const qchar *arg2 = Cmd_ArgsFrom(2);

    Sys_SetEnv(arg1, arg2);
  }
  else if (argc == 2)
  {
    const qchar *env = getenv(arg1);

    if (env)
    {
      Com_Printf("%s=%s\n", arg1, env);
    }
    else
    {
      Com_Printf("%s undefined\n", arg1);
    }
  }
}

/*
=================
Com_ExecuteCfg

For controlling environment variables.
=================
*/
static void
Com_ExecuteCfg(void)
{
  Cbuf_ExecuteText(EXEC_NOW, "exec default.cfg\n");
  Cbuf_Execute(); //always execute after exec to prevent text buffer overflowing

  if (!Com_SafeMode())
  {
    //skip the q3config.cfg and autoexec.cfg if "safe" is on the command line
    Cbuf_ExecuteText(EXEC_NOW, "exec " Q3CONFIG_CFG "\n");
    Cbuf_Execute();
    Cbuf_ExecuteText(EXEC_NOW, "exec autoexec.cfg\n");
    Cbuf_Execute();
  }
}

/*
=================
Com_GameRestart

Change to a new mod properly with cleaning up cvars before switching.
=================
*/
void
Com_GameRestart(qint checksumFeed, qbool clientRestart)
{
  static qbool com_gameRestarting = qfalse;

  //make sure no recursion can be triggered
  if (!com_gameRestarting && com_fullyInitialized)
  {
    com_gameRestarting = qtrue;
#if !defined(DEDICATED)
    if (clientRestart)
    {
      CL_Disconnect(qfalse);
      CL_ShutdownAll();
    }
#endif
    //kill server if we have one
    if (com_sv_running->integer)
    {
      SV_Shutdown("Game directory changed");
    }

    //shutdown fs early so cvar_restart will not reset old game cvars
    FS_Shutdown(qfalse);

    //clean out any user and qvm created cvars
    Cvar_Restart(qtrue);

    FS_Restart(checksumFeed);

    //clean out any user and vm created cvars
    Cvar_Restart(qtrue);
    Com_ExecuteCfg();
#if !defined(DEDICATED)
    //restart sound subsystem so old handles are flushed
    CL_Snd_Restart();

    if (clientRestart)
    {
      CL_StartHunkUsers(qfalse);
    }
#endif
    com_gameRestarting = qfalse;
  }
}

/*
=================
Com_GameRestart

Expose possibility to change current running mod to the user
=================
*/
void
Com_GameRestart_f(void)
{
  Cvar_Set("fs_game", Cmd_Argv(1));

  Com_GameRestart(0, qtrue);
}

/*
** --------------------------------------------------------------------------------
**
** PROCESSOR STUFF
**
** --------------------------------------------------------------------------------
*/

#if defined(USE_AFFINITY_MASK)
static uint64_t eCoreMask;
static uint64_t pCoreMask;
static uint64_t affinityMask; //saved at startup
#endif

#if (idx64 || id386)

#if defined(_MSC_VER)
#include <intrin.h>

static void
CPUID(qint func, unsigned *regs)
{
  __cpuid((qint *)regs, func);
}

#if defined(USE_AFFINITY_MASK)
#if idx64
extern void
CPUID_EX(qint func, qint param, unsigned *regs);
#else
void
CPUID_EX(qint func, qint param, unsigned *regs)
{
  __asm {
    push edi
    mov eax, func
    mov ecx, param
    cpuid
    mov edi, regs
    mov [edi + 0], eax
    mov [edi + 4], ebx
    mov [edi + 8], ecx
    mov [edi + 12], edx
    pop edi
  }
}
#endif //!idx64
#endif //USE_AFFINITY_MASK

#else //clang/gcc/mingw

static void
CPUID(qint func, unsigned *regs)
{
  __asm__ __volatile__("cpuid" :
    "=a"(regs[0]),
    "=b"(regs[1]),
    "=c"(regs[2]),
    "=d"(regs[3]) :
    "a"(func));
}

#if defined(USE_AFFINITY_MASK)
static void
CPUID_EX(qint func, qint param, unsigned *regs)
{
  __asm__ __volatile__("cpuid" :
    "=a"(regs[0]),
    "=b"(regs[1]),
    "=c"(regs[2]),
    "=d"(regs[3]) :
    "a"(func),
    "c"(param));
}
#endif //USE_AFFINITY_MASK

#endif //clang/gcc/mingw

static void
Sys_GetProcessorId(qchar *vendor)
{
  uint32_t regs[4]; //EAX, EBX, ECX, EDX
  uint32_t cpuid_level_ex;
  qchar vendor_str[12 + 1]; //short CPU vendor string

  //setup initial features
#if idx64
  CPU_Flags |= CPU_SSE | CPU_SSE2 | CPU_FCOM;
#else
  CPU_Flags = 0;
#endif
  vendor[0] = '\0';

  CPUID(0x80000000, regs);
  cpuid_level_ex = regs[0];

  //get CPUID level & short CPU vendor string
  CPUID(0x0, regs);
  Com_Memcpy(vendor_str + 0, (qchar *)&regs[1], 4);
  Com_Memcpy(vendor_str + 4, (qchar *)&regs[3], 4);
  Com_Memcpy(vendor_str + 8, (qchar *)&regs[2], 4);
  vendor_str[12] = '\0';

  //get CPU feature bits
  CPUID(0x1, regs);

  //bit 15 of EDX denotes CMOV/FCMOV/FCOMI existence
  if (regs[3] & (BIT(15)))
  {
    CPU_Flags |= CPU_FCOM;
  }

  //bit 23 of EDX denotes MMX existence
  if (regs[3] & (BIT(23)))
  {
    CPU_Flags |= CPU_MMX;
  }

  //bit 25 of EDX denotes SSE existence
  if (regs[3] & (BIT(25)))
  {
    CPU_Flags |= CPU_SSE;
  }

  //bit 26 of EDX denotes SSE2 existence
  if (regs[3] & (BIT(26)))
  {
    CPU_Flags |= CPU_SSE2;
  }

  //bit 0 of ECX denotes SSE3 existence
  //if (regs[2] & (BIT(0)))
  //{
    //CPU_Flags |= CPU_SSE3;
  //}

  //bit 19 of ECX denotes SSE41 existence
  if (regs[2] & (BIT(19)))
  {
    CPU_Flags |= CPU_SSE41;
  }

  if (vendor)
  {
    if (cpuid_level_ex >= 0x80000004)
    {
      //read CPU Brand string
      uint32_t i;

      for(i = 0x80000002;i <= 0x80000004;i++)
      {
        CPUID(i, regs);
        Com_Memcpy(vendor + 0, (qchar *)&regs[0], 4);
        Com_Memcpy(vendor + 4, (qchar *)&regs[1], 4);
        Com_Memcpy(vendor + 8, (qchar *)&regs[2], 4);
        Com_Memcpy(vendor + 12, (qchar *)&regs[3], 4);
        vendor[16] = '\0';
        vendor += strlen(vendor);
      }
    }
    else
    {
      const qint print_flags = CPU_Flags;

      vendor = Q_stradd(vendor, vendor_str);

      if (print_flags)
      {
        //print features
        Q_strcat(vendor, sizeof(vendor), " w/");

        if (print_flags & CPU_FCOM)
        {
          Q_strcat(vendor, sizeof(vendor), " CMOV");
        }

        if (print_flags & CPU_MMX)
        {
          Q_strcat(vendor, sizeof(vendor), " MMX");
        }

        if (print_flags & CPU_SSE)
        {
          Q_strcat(vendor, sizeof(vendor), " SSE");
        }

        if (print_flags & CPU_SSE2)
        {
          Q_strcat(vendor, sizeof(vendor), " SSE2");
        }

        //if (CPU_Flags & CPU_SSE3)
        //{
          //Q_strcat(vendor, sizeof(vendor), " SSE3");
        //}

        if (print_flags & CPU_SSE41)
        {
          Q_strcat(vendor, sizeof(vendor), " SSE4.1");
        }
      }
    }
  }
}

#if defined(USE_AFFINITY_MASK)
static void
DetectCPUCoresConfig(void)
{
  uint32_t regs[4];
  uint32_t i;

  //get highest function parameter and vendor id
  CPUID(0x0, regs);

  if (regs[1] != 0x756E6547 || regs[2] != 0x6C65746E || regs[3] != 0x49656E69 || regs[0] < 0x1A)
  {
    //non-intel signature or too low cpuid level - unsupported
    eCoreMask = pCoreMask = affinityMask;
    return;
  }

  eCoreMask = 0;
  pCoreMask = 0;

  for(i = 0;i < sizeof(affinityMask) * 8;i++)
  {
    const uint64_t mask = 1ULL << i;

    if ((mask & affinityMask) && Sys_SetAffinityMask(mask))
    {
      CPUID_EX(0x1A, 0x0, regs);

      switch((regs[0] >> 24) & 0xFF)
      {
        case
        0x20:
          eCoreMask |= mask;
          break;

        case
        0x40:
          pCoreMask |= mask;
          break;

        default: //non-existing leaf
          eCoreMask = pCoreMask = 0;
          break;
      }
    }
  }

  //restore original affinity
  Sys_SetAffinityMask(affinityMask);

  if (pCoreMask == 0 || eCoreMask == 0)
  {
    //if either mask is empty - assume non-hybrid configuration
    eCoreMask = pCoreMask = affinityMask;
  }
}
#endif //USE_AFFINITY_MASK

#else //non-x86

#if !defined(__linux__)

static void
Sys_GetProcessorId(qchar *vendor)
{
  Com_sprintf(vendor, 100, "%s", ARCH_STRING);
}

#else //__linux__

#include <sys/auxv.h>

#if arm32
#include <asm/hwcap.h>
#endif

static void
Sys_GetProcessorId(qchar *vendor)
{
#if arm32
  const qchar *platform;
  long hwcaps;
  CPU_Flags = 0;

  platform = (const qchar *)getauxval(AT_PLATFORM);

  if (!platform || *platform == '\0')
  {
    platform = "(unknown)";
  }

  if (platform[0] == 'v' || platform[0] == 'V')
  {
    if (Q_atoi(platform + 1) >= 7)
    {
      CPU_Flags |= CPU_ARMv7;
    }
  }

  Com_sprintf(vendor, 100, "ARM %s", platform);
  hwcaps = getauxval(AT_HWCAP);

  if (hwcaps & (HWCAP_IDIVA | HWCAP_VFPv3))
  {
    Q_strcat(vendor, sizeof(vendor), " /w");

    if (hwcaps & HWCAP_IDIVA)
    {
      CPU_Flags |= CPU_IDIVA;
      Q_strcat(vendor, sizeof(vendor), " IDIVA");
    }

    if (hwcaps & HWCAP_VFPv3)
    {
      CPU_Flags |= CPU_VFPv3;
      Q_strcat(vendor, sizeof(vendor), " VFPv3");
    }

    if ((CPU_Flags & (CPU_ARMv7 | CPU_VFPv3)) == (CPU_ARMv7 | CPU_VFPv3))
    {
      Q_strcat(vendor, sizeof(vendor), " QVM-bytecode");
    }
  }
#else //!arm32
  CPU_Flags = 0;
#if arm64
  Com_sprintf(vendor, 100, "%s", ARCH_STRING);
#else
  Com_sprintf(vendor, 128, "%s %s", ARCH_STRING, (const qchar *)getauxval(AT_PLATFORM));
#endif
#endif //!arm32
}

#endif //__linux__

#endif //non-x86

/*
================
Com_SnapVector
================
*/
#if defined(_MSC_VER)
#if idx64
void
Com_SnapVector(float *vector)
{
  __m128 vf0;
  __m128 vf1;
  __m128 vf2;
  DWORD mxcsr;

  mxcsr = _mm_getcsr();
  vf0 = _mm_setr_ps(vector[0], vector[1], vector[2], 0.0f);

  _mm_setcsr(mxcsr & ~0x6000); //enforce rounding mode to "round to nearest"

  vi = _mm_cvtps_epi32(vf0);
  vf0 = _mm_cvtepi32_ps(vi);

  vf1 = _mm_shuffle_ps(vf0, vf0, _MM_SHUFFLE(1, 1, 1, 1));
  vf2 = _mm_shuffle_ps(vf0, vf0, _MM_SHUFFLE(2, 2, 2, 2));

  _mm_setcsr(mxcsr); //restore rounding mode

  _mm_store_ss(&vector[0], vf0);
  _mm_store_ss(&vector[1], vf1);
  _mm_store_ss(&vector[2], vf2);
}
#endif //idx64

#if id386
void
Com_SnapVector(float *vector)
{
  static const DWORD cw037F = 0x037F;
  DWORD cwCurr;

  __asm {
    fnstcw word ptr [cwCurr]
    mov ecx, vector
    fldcw word ptr [cw037F]

    fld dword ptr[ecx + 8]
    fistp dword ptr[ecx + 8]
    fild dword ptr[ecx + 8]
    fstp dword ptr[ecx + 8]

    fld dword ptr[ecx + 4]
    fistp dword ptr[ecx + 4]
    fild dword ptr[ecx + 4]
    fstp dword ptr[ecx + 4]

    fld dword ptr[ecx + 0]
    fistp dword ptr[ecx + 0]
    fild dword ptr[ecx + 0]
    fstp dword ptr[ecx + 0]

    fldcw word ptr cwCurr
  }; //__asm
}
#endif //id386

#if arm64
void
Com_SnapVector(float *vector)
{
  vector[0] = rint(vector[0]);
  vector[1] = rint(vector[1]);
  vector[2] = rint(vector[2]);
}
#endif

#else //clang/gcc/mingw

#if id386

#define QROUNDX87(src) \
  "flds " src "\n" \
  "fistpl " src "\n" \
  "fildl " src "\n" \
  "fstps " src "\n"

void
Com_SnapVector(float *vector)
{
  static const unsigned short cw037F = 0x037F;
  unsigned short cwCurr;

  __asm__ volatile
  (
    "fnstcw %1\n" \
    "fldcw %2\n" \
    QROUNDX87("0(%0)")
    QROUNDX87("4(%0)")
    QROUNDX87("8(%0)")
    "fldcw %1\n" \
    :
    : "r" (vector), "m"(cwCurr), "m"(cw037F)
    : "memory", "st"
  );
}

#else //idx64, non-x86

void
Com_SnapVector(float *vector)
{
  vector[0] = rint(vector[0]);
  vector[1] = rint(vector[1]);
  vector[2] = rint(vector[2]);
}

#endif

#endif //clang/gcc/mingw

#if defined(USE_AFFINITY_MASK)

static qint
hex_code(const qint code)
{
  if (code >= '0' && code <= '9')
  {
    return code - '0';
  }

  if (code >= 'A' && code <= 'F')
  {
    return code - 'A' + 10;
  }

  if (code >= 'a' && code <= 'f')
  {
    return code - 'a' + 10;
  }

  return -1;
}

static const qchar *
parseAffinityMask(const qchar *str, uint64_t *outv, qint level)
{
  uint64_t v;
  uint64_t mask = 0;

  while(*str != '\0')
  {
    if (*str == 'A' || *str == 'a')
    {
      mask = affinityMask;
      ++str;
      continue;
    }
    else if (*str == 'P' || *str == 'p')
    {
      mask = pCoreMask;
      ++str;
      continue;
    }
    else if (*str == 'E' || *str == 'e')
    {
      mask = eCoreMask;
      ++str;
      continue;
    }
    else if (*str == '0' && (str[1] == 'x' || str[1] == 'X') && (v = hex_code(str[2])) >= 0)
    {
      qint hex;

      str += 3; //0xH

      while((hex = hex_code(*str)) >= 0)
      {
        v = v * 16 + hex;
        str++;
      }

      mask = v;
      continue;
    }
    else if (*str >= '0' && *str <= '9')
    {
      mask = *str++ - '0';

      while(*str >= '0' && *str <= '9')
      {
        mask = mask * 10 + *str - '0';
        ++str;
      }

      continue;
    }

    if (level == 0)
    {
      while(*str == '+' || *str == '-')
      {
        str = parseAffinityMask(str + 1, &v, level + 1);

        switch(*str)
        {
          case
          '+':
            mask |= v;
            break;

          case
          '-':
            mask &= ~v;
            break;

          default:
            str = "";
            break;
        }
      }

      if (*str != '\0')
      {
        ++str; //skip unknown characters
      }
    }
    else
    {
      break;
    }
  }

  *outv = mask;
  return str;
}

//parse and set affinity mask
static void
Com_SetAffinityMask(const qchar *str)
{
  uint64_t mask = 0;

  parseAffinityMask(str, &mask, 0);

  if ((mask & affinityMask) == 0)
  {
    mask = affinityMask; //reset to default
  }

  if (mask != 0)
  {
    Sys_SetAffinityMask(mask);
  }
}
#endif //USE_AFFINITY_MASK

static void Com_DetectAltivec(void)
{
	// Only detect if user hasn't forcibly disabled it.
	if (com_altivec->integer) {
		static qbool altivec = qfalse;
		static qbool detected = qfalse;
		if (!detected) {
			altivec = ( Sys_GetProcessorFeatures( ) & CF_ALTIVEC );
			detected = qtrue;
		}

		if (!altivec) {
			Cvar_Set( "com_altivec", "0" );  // we don't have it! Disable support!
		}
	}
}

/*
=================
Com_InitRand

Seed the random numbr generator, if possible with an OS supplied random seed.
=================
*/
static void
Com_InitRand(void)
{
  unsigned seed;

  if (Sys_RandomBytes((byte *)&seed, sizeof(seed)))
  {
    srand(seed);
  }
  else
  {
    srand(time(NULL));
  }
}

#if 0 //FIXME: broken
static qbool
Com_InitExecs(void)
{
  if (Q_stricmp(Cmd_Argv(0), "exec") && Q_stricmp(Cmd_Argv(0), "execq"))
  {
    return qfalse;
  }

  Cbuf_AddText(va(NULL, "%s\n", Cmd_ArgsFrom(0)));
  return qtrue;
}
#endif

/*
=================
Com_Init
=================
*/
void Com_Init( qchar *commandLine ) {
	//qchar	*s;
	qint qport;

        //get the initial time base
        Sys_Milliseconds();

	Com_Printf( "%s %s %s\n", Q3_VERSION, PLATFORM_STRING, __DATE__ );

	if ( Q_setjmp (abortframe) ) {
		Sys_Error ("Error during initialization");
	}

	// Clear queues
	Com_Memset( &eventQueue[ 0 ], 0, MAX_QUEUED_EVENTS * sizeof( sysEvent_t ) );

        //initialize the weak pseudo-random number generator for use later
        Com_InitRand();

        // do this before anything else decides to push events
        Com_InitPushEvent();

	Com_InitSmallZoneMemory();
	Cvar_Init ();

#if defined(_WIN32) && defined(_DEBUG)
        com_noErrorInterrupt = Cvar_Get("com_noErrorInterrupt", "0", 0);
#endif

	// prepare enough of the subsystems to handle
	// cvar and command buffer management
	Com_ParseCommandLine( commandLine );

//	Swap_Init ();
	Cbuf_Init ();

	// override anything from the config files with command line args
	Com_StartupVariable( NULL );

        Com_InitZoneMemory();
        Cmd_Init();

	// get the developer cvar set as early as possible
	com_developer = Cvar_Get("developer", "0", CVAR_TEMP);

	Com_StartupVariable("vm_rtChecks");
	vm_rtChecks = Cvar_GetAndDescribe("vm_rtChecks", "15", CVAR_INIT | CVAR_PROTECTED | CVAR_SERVERINFO, "Runtime checks in compiled vm code, bitmask:\n1 - program stack overflow\n2 - opcode stack overflow\n4 - jump target range\n8 - data read/write range");

	Com_StartupVariable( "journal" );
	com_journal = Cvar_GetAndDescribe("journal", "0", CVAR_INIT | CVAR_PROTECTED, "When enabled, writes events and its data to 'journal.dat' and 'journaldata.dat'.");
	Cvar_CheckRange(com_journal, "0", "2", CV_INTEGER);

        com_homepath = Cvar_Get("com_homepath", "", CVAR_INIT | CVAR_PROTECTED);

#if !defined(DEDICATED)
	// done early so bind command exists
	CL_InitKeyCommands();
#endif
        com_homepath = Cvar_Get("com_homepath", "", CVAR_INIT);

        //Com_StartupVariable(
	FS_InitFilesystem();

	Com_InitJournaling();

        //FIXME: broken Com_CommandLineCheck(&Com_InitExecs);

        //add some commands here already so users can use them from config files
        Cmd_AddCommand("setenv", Com_Setenv_f);

        if (com_developer && com_developer->integer)
        {
          Cmd_AddCommand("error", Com_Error_f);
          Cmd_AddCommand("crash", Com_Crash_f);
          Cmd_AddCommand("freeze", Com_Freeze_f);
        }

        Cmd_AddCommand("quit", Com_Quit_f);
        Cmd_AddCommand("changeVectors", MSG_ReportChangeVectors_f);
        Cmd_AddCommand("writeconfig", Com_WriteConfig_f);
        Cmd_SetCommandCompletionFunc("writeconfig", Cmd_CompleteWriteCfgName);

	Com_ExecuteCfg();

	// override anything from the config files with command line args
	Com_StartupVariable( NULL );

  // get dedicated here for proper hunk megs initialization
#if defined(DEDICATED)
	com_dedicated = Cvar_GetAndDescribe("dedicated", "1", CVAR_INIT, "Enables dedicated server mode.\n 0: Listen server\n 1: Unlisted dedicated server\n 2: Listed dedicated server");
	Cvar_CheckRange( com_dedicated, "1", "2", CV_INTEGER );
#else
	com_dedicated = Cvar_Get ("dedicated", "0", CVAR_LATCH);
	Cvar_CheckRange( com_dedicated, "0", "2", CV_INTEGER );
#endif

        if (com_dedicated->integer)
        {
          gw_minimized = qtrue;
        }
        else
        {
          gw_minimized = qfalse;
        }

	// allocate the stack based hunk allocator
	Com_InitHunkMemory();

	// if any archived cvars are modified after this, we will trigger a writing
	// of the config file
	cvar_modifiedFlags &= ~CVAR_ARCHIVE;

	//
	// init commands and vars
	//
	com_altivec = Cvar_Get ("com_altivec", "1", CVAR_ARCHIVE);
	com_blood = Cvar_Get ("com_blood", "1", CVAR_ARCHIVE);

	com_logfile = Cvar_GetAndDescribe("logfile", "0", CVAR_TEMP, "System console logging:\n"
		" 0 - disabled\n"
		" 1 - overwrite mode, buffered\n"
		" 2 - overwrite mode, synced\n"
		" 3 - append mode, buffered\n"
		" 4 - append mode, synced\n");
	Cvar_CheckRange(com_logfile, "0", "4", CV_INTEGER);

	com_timescale = Cvar_GetAndDescribe("timescale", "1", CVAR_CHEAT | CVAR_SYSTEMINFO, "System timing factor:\n < 1: Slows the game down\n = 1: Regular speed\n > 1: Speeds the game up");
	Cvar_CheckRange(com_timescale, "0", NULL, CV_FLOAT);
	com_fixedtime = Cvar_GetAndDescribe("fixedtime", "0", CVAR_CHEAT, "Toggle the rendering of every frame the game will wait until each frame is completely rendered before sending the next frame.");
	com_showtrace = Cvar_GetAndDescribe("com_showtrace", "0", CVAR_CHEAT, "Debugging tool that prints out trace information.");
	com_speeds = Cvar_GetAndDescribe("com_speeds", "0", 0, "Prints speed information per frame to the console. Used for debugging.");
#if !defined(DEDICATED)
	com_timedemo = Cvar_GetAndDescribe("timedemo", "0", CVAR_CHEAT, "When set to '1' times a demo and returns frames per second like a benchmark.");
	Cvar_CheckRange(com_timedemo, "0", "1", CV_INTEGER);
#endif
	com_cameraMode = Cvar_Get ("com_cameraMode", "0", CVAR_CHEAT);

#if !defined(DEDICATED)
	cl_paused = Cvar_GetAndDescribe("cl_paused", "0", CVAR_ROM, "Read-only CVAR to toggle functionality of paused games (the variable holds the status of the paused flag on the client side).");
	cl_packetdelay = Cvar_GetAndDescribe("cl_packetdelay", "0", CVAR_CHEAT, "Artificially set the client's latency. Simulates packet delay, which can lead to packet loss.");
	com_cl_running = Cvar_GetAndDescribe("cl_running", "0", CVAR_ROM, "Can be used to check the status of the client game.");
#endif
	sv_paused = Cvar_Get ("sv_paused", "0", CVAR_ROM);
	cl_packetloss = Cvar_Get("cl_packetloss", "0", CVAR_CHEAT);
	sv_packetdelay = Cvar_GetAndDescribe("sv_packetdelay", "0", CVAR_CHEAT, "Simulates packet delay, which can lead to packet loss. Server side.");
	sv_packetloss = Cvar_Get("sv_packetloss", "0", CVAR_CHEAT);
	com_sv_running = Cvar_GetAndDescribe("sv_running", "0", CVAR_ROM, "Communicates to game modules if there is a server currently running.");
	com_buildScript = Cvar_GetAndDescribe("com_buildScript", "0", 0, "Loads all game assets, regardless whether they are required or not.");
	com_ansiColor = Cvar_GetAndDescribe("com_ansiColor", "0", CVAR_ARCHIVE, "Use ANSI color in the terminal window instead of color codes");

        Cvar_Get("com_errorMessage", "", CVAR_ROM);

#if !defined(DEDICATED)
        com_maxfps = Cvar_GetAndDescribe("com_maxfps", "125", 0, "Sets maximum frames per second."); //try to force that in some light way
        Cvar_CheckRange(com_maxfps, "0", "1000", CV_INTEGER);
	com_maxfpsUnfocused = Cvar_GetAndDescribe("com_maxfpsUnfocused", "60", CVAR_ARCHIVE_ND, "Sets maximum frames per second in unfocused game window.");
	Cvar_CheckRange(com_maxfpsUnfocused, "0", "1000", CV_INTEGER);
	com_yieldCPU = Cvar_GetAndDescribe("com_yieldCPU", "1", CVAR_ARCHIVE_ND, "Attempt to sleep specified amount of time between rendered frames when game is active, this will greatly reduce CPU load. Use 0 only if you're experiencing some lag.");
	Cvar_CheckRange(com_yieldCPU, "0", "16", CV_INTEGER);
#endif

#if defined(USE_AFFINITY_MASK)
        com_affinityMask = Cvar_GetAndDescribe("com_affinityMask", "", CVAR_ARCHIVE | CVAR_SERVERINFO, "Bind game process to bitmask-specified CPU core(s), special characters:\n A or a - all default cores\n P or p - performance cores\n E or e - efficiency cores\n 0x<value> - use hexadecimal notation\n + or - can be used to add or exclude particular cores");
        com_affinityMask->modified = qfalse;
#endif


	com_abnormalExit = Cvar_Get("com_abnormalExit", "0", CVAR_ROM);

	Cmd_AddCommand("game_restart", Com_GameRestart_f);

	const qchar *s = va(NULL, "%s %s %s", Q3_VERSION, PLATFORM_STRING, __DATE__ );
	com_version = Cvar_GetAndDescribe("version", s, CVAR_PROTECTED | CVAR_ROM | CVAR_SERVERINFO, "Read-only CVAR to see the version of the game.");

	Sys_Init();

        //CPU detection
        Cvar_Get("sys_cpustring", "detect", CVAR_PROTECTED | CVAR_ROM);

        if (!Q_stricmp(Cvar_VariableString("sys_cpustring"), "detect"))
        {
          qchar vendor[128];

          Com_Printf("...detecting CPU, found ");
          Sys_GetProcessorId(vendor);
          Cvar_Set("sys_cpustring", vendor);
        }

        Com_Printf("%s\n", Cvar_VariableString("sys_cpustring"));

#if defined(USE_AFFINITY_MASK)
        //get initial process affinity - we will respect it when setting custom affinity mask
        eCoreMask = pCoreMask = affinityMask = Sys_GetAffinityMask();
#if (idx64 || id386)
        DetectCPUCoresConfig();
#endif
        if (com_affinityMask->string[0] != '\0')
        {
          Com_SetAffinityMask(com_affinityMask->string);
          com_affinityMask->modified = qfalse;
        }
#endif

        if (Sys_WritePIDFile())
        {
#if !defined(DEDICATED)
          const qchar *message = "The last time " CLIENT_WINDOW_TITLE " ran, it didn't exit properly. This may be due to inappropriate video settings. Would you like to start with \"safe\" video settings?";

          if (Sys_Dialog(DT_YES_NO, message, "Abnormal Exit") == DR_YES)
          {
            Cvar_Set("com_abnormalExit", "1");
          }
#endif
        }

        //pick a random port value
        Com_RandomBytes((byte *)&qport, sizeof(qint));
        Netchan_Init(qport & 0xffff);

	VM_Init();
	SV_Init();

	com_dedicated->modified = qfalse;
#if !defined(DEDICATED)
	CL_Init();
#endif

	// add + commands from command line
	if ( !Com_AddStartupCommands() ) {
		// if the user didn't give any commands, run default action
		if ( !com_dedicated->integer ) {
			Cbuf_AddText ("cinematic splash.RoQ\n");
		}
	}

	// start in full screen ui mode
	Cvar_Set("r_uiFullScreen", "1");
#if !defined(DEDICATED)
	CL_StartHunkUsers( qfalse );
#endif

	// set com_frameTime so that if a map is started on the
	// command line it will still be able to count on com_frameTime
	// being random enough for a serverid
	//lastTime = com_frameTime = Com_Milliseconds();
	Com_FrameInit();

#if !defined(DEDICATED)
	// make sure single player is off by default
	Cvar_Set("ui_singlePlayerActive", "0");
#endif

	com_fullyInitialized = qtrue;

	// always set the cvar, but only print the info if it makes sense.
	Com_DetectAltivec();
#if idppc
	Com_Printf ("Altivec support is %s\n", com_altivec->integer ? "enabled" : "disabled");
#endif

        com_pipefile = Cvar_Get("com_pipefile", "", CVAR_ARCHIVE | CVAR_LATCH);

        if (com_pipefile->string[0])
        {
          pipefile = FS_FCreateOpenPipeFile(com_pipefile->string);
        }

	Com_Printf ("--- Common Initialization Complete ---\n");

        NET_Init();

        Com_Printf("Working directory: %s\n", Sys_Pwd());
}

/*
===============
Com_ReadFromPipe

Read whatever is in com_pipefile, if anything, and execute it
===============
*/
void
Com_ReadFromPipe(void)
{
  qchar buffer[MAX_STRING_CHARS] = {""};
  qbool read;

  if (!pipefile)
  {
    return;
  }

  read = FS_Read(buffer, sizeof(buffer), pipefile);

  if (read)
  {
    Cbuf_ExecuteText(EXEC_APPEND, buffer);
  }
}

//==================================================================

static void Com_WriteConfigToFile( const qchar *filename ) {
	fileHandle_t	f;

	f = FS_FOpenFileWrite( filename );
	if ( f == FS_INVALID_HANDLE ) {
		if (!FS_ResetReadOnlyAttribute(filename) || (f = FS_FOpenFileWrite(filename)) == FS_INVALID_HANDLE)
		{
                  Com_Printf("Couldn't write %s.\n", filename);
                  return;
		}
	}

	FS_Printf (f, "// generated by tremulous, do not modify" Q_NEWLINE);
#if !defined(DEDICATED)
	Key_WriteBindings (f);
#endif
	Cvar_WriteVariables (f);
	FS_FCloseFile( f );
}


/*
===============
Com_WriteConfiguration

Writes key bindings and archived cvars to config file if modified
===============
*/
void Com_WriteConfiguration( void ) {
	// if we are quiting without fully initializing, make sure
	// we don't write out anything
	if ( !com_fullyInitialized ) {
		return;
	}

	if ( !(cvar_modifiedFlags & CVAR_ARCHIVE ) ) {
		return;
	}
	cvar_modifiedFlags &= ~CVAR_ARCHIVE;

	Com_WriteConfigToFile( Q3CONFIG_CFG );
}


/*
===============
Com_WriteConfig_f

Write the config file to a specific name
===============
*/
static void Com_WriteConfig_f( void ) {
	qchar	filename[MAX_QPATH];
	const qchar *ext;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: writeconfig <filename>\n" );
		return;
	}

	Q_strncpyz( filename, Cmd_Argv(1), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".cfg" );

        if (!FS_AllowedExtension(filename, qfalse, &ext))
        {
          Com_Printf("%s: Invalid filename extension: '%s'.\n", __func__, ext);
          return;
        }

	Com_Printf( "Writing %s.\n", filename );
	Com_WriteConfigToFile( filename );
}

/*
================
Com_ModifyMsec
================
*/
static qint Com_ModifyMsec( qint msec ) {
	qint		clampTime;

	//
	// modify time for debugging values
	//
	if ( com_fixedtime->integer ) {
		msec = com_fixedtime->integer;
	} else if ( com_timescale->value ) {
		msec *= com_timescale->value;
	} else if (com_cameraMode->integer) {
		msec *= com_timescale->value;
	}
	
	// don't let it scale below 1 msec
	if ( msec < 1 && com_timescale->value) {
		msec = 1;
	}

	if ( com_dedicated->integer ) {
		// dedicated servers don't want to clamp for a much longer
		// period, because it would mess up all the client's views
		// of time.
		if (com_sv_running->integer && msec > 500)
			Com_Printf( "Hitch warning: %i msec frame time\n", msec );

		clampTime = 5000;
	} else 
	if ( !com_sv_running->integer ) {
		// clients of remote servers do not want to clamp time, because
		// it would skew their view of the server's time temporarily
		clampTime = 5000;
	} else {
		// for local single player gaming
		// we may want to clamp the time to prevent players from
		// flying off edges when something hitches.
		clampTime = 200;
	}

	if ( msec > clampTime ) {
		msec = clampTime;
	}

	return msec;
}

/*
=================
Com_TimeVal
=================
*/
static const qint
Com_TimeVal(const qint minMsec)
{
  qint timeVal;

  timeVal = Sys_Milliseconds() - com_frameTime;

  if (timeVal >= minMsec)
  {
    timeVal = 0;
  }
  else
  {
    timeVal = minMsec - timeVal;
  }

  return timeVal;
}

/*
=================
Com_FrameInit
=================
*/
void
Com_FrameInit(void)
{
  lastTime = com_frameTime = Com_Milliseconds();
}

/*
=================
Com_Frame
=================
*/
void
Com_Frame(qbool noDelay)
{
  qint msec;
  qint realMsec;
  qint minMsec;
  qint sleepMsec;
  qint timeVal;
  qint timeValSV;
#if !defined(DEDICATED)
  static qint bias = 0;
#endif
  qint timeBeforeFirstEvents;
  qint timeBeforeServer;
  qint timeBeforeEvents;
  qint timeBeforeClient;
  qint timeAfter;
  qint all;
  qint sv;
  qint ev;
  qint cl;
  extern qint c_traces;
  extern qint c_brush_traces;
  extern qint c_patch_traces;
  extern qint c_pointcontents;

  if (Q_setjmp(abortframe))
  {
    return; //an ERR_DROP was thrown
  }

  minMsec = 0; //silent compiler warning

  //bk001204 - init to zero.
  //also: might be clobbered by 'longjmp' or 'vfork'
  timeBeforeFirstEvents = 0;
  timeBeforeServer = 0;
  timeBeforeEvents = 0;
  timeBeforeClient = 0;
  timeAfter = 0;

  //write config file if anything changed
#if !defined(DELAY_WRITECONFIG)
  Com_WriteConfiguration();
#endif

#if defined(USE_AFFINITY_MASK)
  if (com_affinityMask->modified)
  {
    Com_SetAffinityMask(com_affinityMask->string);
    com_affinityMask->modified = qfalse;
  }
#endif

  //main event loop
  if (com_speeds->integer)
  {
    timeBeforeFirstEvents = Sys_Milliseconds();
  }

  //we may want to spin here if things are going too fast
  if (com_dedicated->integer)
  {
    minMsec = SV_FrameMsec();
#if !defined(DEDICATED)
    bias = 0;
#endif
  }
  else
  {
#if !defined(DEDICATED)
    if (noDelay)
    {
      minMsec = 0;
      bias = 0;
    }
    else
    {
      if (!gw_active && com_maxfpsUnfocused->integer > 0)
      {
        minMsec = 1000 / com_maxfpsUnfocused->integer;
      }
      else if (com_maxfps->integer > 0)
      {
        minMsec = 1000 / com_maxfps->integer;
      }
      else
      {
        minMsec = 1;
      }

      timeVal = com_frameTime - lastTime;
      bias += timeVal - minMsec;

      if (bias > minMsec)
      {
        bias = minMsec;
      }

      //adjust minMsec if previous frame took too long to render so
      //that framerate is stable at the requested value
      minMsec -= bias;
    }
#endif
  }

  //waiting for incoming packets
  if (noDelay == qfalse)
  {
    do
    {
      if (com_sv_running->integer)
      {
        timeValSV = SV_SendQueuedPackets();
        timeVal = Com_TimeVal(minMsec);

        if (timeValSV < timeVal)
        {
          timeVal = timeValSV;
        }
      }
      else
      {
        timeVal = Com_TimeVal(minMsec);
      }

      sleepMsec = timeVal;
#if !defined(DEDICATED)
      if (!gw_minimized && timeVal > com_yieldCPU->integer)
      {
        sleepMsec = com_yieldCPU->integer;
      }

      if (timeVal > sleepMsec)
      {
        Com_EventLoop();
      }
#endif
      NET_Sleep(sleepMsec * 1000 - 500);
    }
    while(Com_TimeVal(minMsec));
  }

  lastTime = com_frameTime;
  com_frameTime = Com_EventLoop();
  realMsec = com_frameTime - lastTime;

  Cbuf_Execute();

  //mess with msec if needed
  msec = Com_ModifyMsec(realMsec);

  //serverside
  if (com_speeds->integer)
  {
    timeBeforeServer = Sys_Milliseconds();
  }

  SV_Frame(msec);

  //if dedicated is modified start or shut down client system, after server starts but before client auto connect
  if (com_dedicated->modified)
  {
    //get latched value
    Cvar_Get("dedicated", "0", 0);
    com_dedicated->modified = qfalse;

    if (!com_dedicated->integer)
    {
      SV_Shutdown("dedicated set to zero");
      SV_RemoveDedicatedCommands();
#if !defined(DEDICATED)
      CL_Init();
#endif
#if !defined(DEDICATED)
      gw_minimized = qfalse;
      CL_StartHunkUsers();
#endif
    }
    else
    {
#if !defined(DEDICATED)
      CL_Shutdown();
      CL_FlushMemory();
#endif
      gw_minimized = qtrue;
    }
  }

#if defined(DEDICATED)
  if (com_speeds->integer)
  {
    timeAfter = Sys_Milliseconds();
    timeBeforeEvents = timeAfter;
    timeBeforeClient = timeAfter;
  }
#else
    //client system, run event loop a second time for server to client packets with no latency
  if (!com_dedicated->integer)
  {
    if (com_speeds->integer)
    {
      timeBeforeEvents = Sys_Milliseconds();
    }

    Com_EventLoop();
    Cbuf_Execute();

    //clientside
    if (com_speeds->integer)
    {
      timeBeforeClient = Sys_Milliseconds();
    }

    CL_Frame(msec);

    if (com_speeds->integer)
    {
      timeAfter = Sys_Milliseconds();
    }
  }
#endif

  NET_FlushPacketQueue(0);

  Cbuf_Wait();

  //report timing info
  if (com_speeds->integer)
  {
    all = timeAfter - timeBeforeServer;
    sv = timeBeforeEvents - timeBeforeServer;
    ev = timeBeforeServer - timeBeforeFirstEvents + timeBeforeClient - timeBeforeEvents;
    cl = timeAfter - timeBeforeClient;
    sv -= time_game;
    cl -= time_frontend + time_backend;
    Com_Printf("frame %i all %3i sv %3i ev %3i cl %3i gm %3i rf %3i bk %3i\n", com_frameNumber, all, sv, ev, cl, time_game, time_frontend, time_backend);
  }

  //trace optimization tracking
  if (com_showtrace->integer)
  {
    Com_Printf("%4i traces (%ib %ip) %4i points\n", c_traces, c_brush_traces, c_patch_traces, c_pointcontents);
    c_traces = 0;
    c_brush_traces = 0;
    c_patch_traces = 0;
    c_pointcontents = 0;
  }

  com_frameNumber++;
}

/*
=================
Com_Shutdown
=================
*/
static void
Com_Shutdown(void)
{
  if (logfile != FS_INVALID_HANDLE)
  {
    FS_FCloseFile(logfile);
    logfile = FS_INVALID_HANDLE;
  }

  if (com_journalFile != FS_INVALID_HANDLE)
  {
    FS_FCloseFile(com_journalFile);
    com_journalFile = FS_INVALID_HANDLE;
  }

  if (com_journalDataFile != FS_INVALID_HANDLE)
  {
    FS_FCloseFile(com_journalDataFile);
    com_journalDataFile = FS_INVALID_HANDLE;
  }

  if (pipefile)
  {
    FS_FCloseFile(pipefile);
    FS_HomeRemove(com_pipefile->string);
  }
}

//------------------------------------------------------------------------

/*
===========================================
command line completion
===========================================
*/

/*
==================
Field_Clear
==================
*/
void Field_Clear( field_t *edit ) {
  Com_Memset(edit->buffer, 0, sizeof(edit->buffer));
	edit->cursor = 0;
	edit->scroll = 0;
}

static const qchar *completionString;
static qchar shortestMatch[MAX_TOKEN_CHARS];
static qint	matchCount;
// field we are working on, passed to Field_AutoComplete(&g_consoleCommand for instance)
static field_t *completionField;

/*
===============
FindMatches

===============
*/
static void FindMatches( const qchar *s ) {
	qint		i;
	const qint n = (qint)strlen(s);

	if ( Q_stricmpn( s, completionString, strlen( completionString ) ) ) {
		return;
	}
	matchCount++;
	if ( matchCount == 1 ) {
		Q_strncpyz( shortestMatch, s, sizeof( shortestMatch ) );
		return;
	}

	// cut shortestMatch to the amount common with s
	for ( i = 0 ; shortestMatch[i] ; i++ ) {
		if ( i >= n ) {
			shortestMatch[i] = '\0';
			break;
		}

		if ( tolower(shortestMatch[i]) != tolower(s[i]) ) {
			shortestMatch[i] = '\0';
		}
	}
}

/*
===============
PrintMatches

===============
*/
static void PrintMatches( const qchar *s ) {
	if ( !Q_stricmpn( s, shortestMatch, strlen( shortestMatch ) ) ) {
		Com_Printf( "    %s\n", s );
	}
}

/*
===============
PrintCvarMatches

===============
*/
static void PrintCvarMatches( const qchar *s ) {
	qchar value[ TRUNCATE_LENGTH ];

	if ( !Q_stricmpn( s, shortestMatch, strlen( shortestMatch ) ) ) {
		Com_TruncateLongString( value, Cvar_VariableString( s ) );
		Com_Printf( "    %s = \"%s\"\n", s, value );
	}
}

/*
===============
Field_FindFirstSeparator
===============
*/
static const qchar *
Field_FindFirstSeparator(const qchar *s)
{
  qchar c;

  while((c = *s) != '\0')
  {
    if (c == ';')
    {
      return s;
    }

    s++;
  }

  return NULL;
}

/*
===============
Field_Complete
===============
*/
static qbool Field_Complete( void )
{
	qint completionOffset;

	if( matchCount == 0 )
		return qtrue;

	completionOffset = strlen( completionField->buffer ) - strlen( completionString );

	Q_strncpyz( &completionField->buffer[ completionOffset ], shortestMatch,
		sizeof( completionField->buffer ) - completionOffset );

	completionField->cursor = strlen( completionField->buffer );

	if( matchCount == 1 )
	{
		Q_strcat( completionField->buffer, sizeof( completionField->buffer ), " " );
		completionField->cursor++;
		return qtrue;
	}

	Com_Printf( "]%s\n", completionField->buffer );

	return qfalse;
}

#if !defined(DEDICATED)
/*
===============
Field_CompleteKeyname
===============
*/
void Field_CompleteKeyname( void )
{
	matchCount = 0;
	shortestMatch[ 0 ] = '\0';

	Key_KeynameCompletion( FindMatches );

	if( !Field_Complete( ) )
		Key_KeynameCompletion( PrintMatches );
}
#endif

/*
===============
Field_CompleteFilename
===============
*/
void
Field_CompleteFilename(const qchar *dir, const qchar *ext, qbool stripExt, qint flags)
{
  matchCount = 0;
  shortestMatch[0] = '\0';

  FS_FilenameCompletion(dir, ext, stripExt, FindMatches, flags);

  if(!Field_Complete())
  {
    FS_FilenameCompletion(dir, ext, stripExt, PrintMatches, flags);
  }
}

/*
===============
Field_CompleteCommand
===============
*/
void
Field_CompleteCommand(const qchar *cmd, qbool doCommands, qbool doCvars)
{
  qint completionArgument = 0;

  //skip leading whitespace and quotes
  cmd = Com_SkipCharset(cmd, " \"");

  Cmd_TokenizeStringIgnoreQuotes(cmd);
  completionArgument = Cmd_Argc();

  //if there is trailing whitespace on the cmd
  if (*(cmd + strlen(cmd) - 1) == ' ')
  {
    completionString = "";
    completionArgument++;
  }
  else
  {
    completionString = Cmd_Argv(completionArgument - 1);
  }

#if !defined(DEDICATED)
  //unconditionally add a '\' to the start of the buffer
  if (completionField->buffer[0] && completionField->buffer[0] != '\\')
  {
    if (completionField->buffer[0] != '/')
    {
      //buffer is full, refuse to complete
      if (strlen(completionField->buffer) + 1 >= sizeof(completionField->buffer))
      {
        return;
      }

      memmove(&completionField->buffer[1], &completionField->buffer[0], strlen(completionField->buffer) + 1);
      completionField->cursor++;
    }

    completionField->buffer[0] = '\\';
  }
#endif

  if (completionArgument > 1)
  {
    const qchar *baseCmd = Cmd_Argv(0);
    const qchar *p;

#if !defined(DEDICATED)
    //this should always be true
    if (baseCmd[0] == '\\' || baseCmd[0] == '/')
    {
      baseCmd++;
    }
#endif

    if ((p = Field_FindFirstSeparator(cmd)))
    {
      Field_CompleteCommand(p + 1, qtrue, qtrue); //compound command
    }
    else
    {
      Cmd_CompleteArgument(baseCmd, cmd, completionArgument);
    }
  }
  else
  {
    if (completionString[0] == '\\' || completionString[0] == '/')
    {
      completionString++;
    }

    matchCount = 0;
    shortestMatch[0] = '\0';

    if (completionString[0] == '\0')
    {
      return;
    }

    if (doCommands)
    {
      Cmd_CommandCompletion(FindMatches);
    }

    if (doCvars)
    {
      Cvar_CommandCompletion(FindMatches);
    }

    if (!Field_Complete())
    {
      //run through again, printing matches
      if (doCommands)
      {
        Cmd_CommandCompletion(PrintMatches);
      }

      if (doCvars)
      {
        Cvar_CommandCompletion(PrintCvarMatches);
      }
    }
  }
}

/*
===============
Field_AutoComplete

Perform Tab expansion
===============
*/
void Field_AutoComplete( field_t *field )
{
	completionField = field;

	Field_CompleteCommand( completionField->buffer, qtrue, qtrue );
}

/*
==================
Com_RandomBytes

fills string array with len radom bytes, peferably from the OS randomizer
==================
*/
void Com_RandomBytes( byte *string, qint len )
{
	qint i;

	if( Sys_RandomBytes( string, len ) )
		return;

	Com_Printf( "Com_RandomBytes: using weak randomization\n" );
	srand(time(NULL));
	for( i = 0; i < len; i++ )
		string[i] = (unsigned qchar)( rand() % 256 );
}

#if 0
static qbool
strgtr(const qchar *s0, const qchar *s1)
{
  qint l0;
  qint l1;
  qint i;

  l0 = strlen(s0);
  l1 = strlen(s1);

  if (l1 < l0)
  {
    l0 = l1;
  }

  for(i = 0;i < 10;i++)
  {
    if (s1[i] > s0[i])
    {
      return qtrue;
    }

    if (s1[i] < s0[i])
    {
      return qfalse;
    }
  }

  return qfalse;
}
#endif

/*
==================
Com_SortList
==================
*/
void
Com_SortList(qchar **list, qint n)
{
  const qchar *m;
  qchar *temp;
  qint i;
  qint j;

  i = 0;
  j = n;
  m = list[n >> 1];

  do
  {
    while(Q_stricmp(list[i], m) < 0)
    {
      i++;
    }

    while(Q_stricmp(list[j], m) > 0)
    {
      j--;
    }

    if (i <= j)
    {
      temp = list[i];
      list[i] = list[j];
      list[j] = temp;
      i++;
      j--;
    }
  }
  while(i <= j);

  if (j > 0)
  {
    Com_SortList(list, j);
  }

  if (n > i)
  {
    Com_SortList(list + i, n - i);
  }
}

/*
==================
Com_SortFileList
==================
*/
#if 0
void
Com_SortFileList(qchar **list, qint nfiles, qbool fastSort)
{
  if (nfiles > 1 && fastSort)
  {
    Com_SortList(list, nfiles - 1);
  }
  else //defrag mod demo UI can't handle _properly_ sorted directories
  {
    qint i;
    qint flag;

    do
    {
      flag = 0;

      for(i = 1;i < nfiles;i++)
      {
        if (strgtr(list[i - 1], list[i]))
        {
          qchar *temp = list[i];
          list[i] = list[i - 1];
          list[i - 1] = temp;
          flag = 1;
        }
      }
    }
    while(flag);
  }
}
#endif

/* 
==================
crc32_buffer
==================
*/
unsigned
crc32_buffer(const byte *buf, unsigned len)
{
  static unsigned crc32_table[256];
  static qbool crc32_inited = qfalse;
  unsigned crc = 0xFFFFFFFFUL;

  if (!crc32_inited)  
  {
    unsigned int c;
    qint i;
    qint j;

    for(i = 0;i < 256;i++)
    {
      c = i;

      for(j = 0;j < 8;j++)
      {
        c = c & 1 ? (c >> 1) ^ 0xEDB88320UL : c >> 1;
      }

      crc32_table[i] = c;
    }

    crc32_inited = qtrue;
  }

  while(len--) 
  {
    crc = crc32_table[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
  }

  return crc ^ 0xFFFFFFFFUL;
}
