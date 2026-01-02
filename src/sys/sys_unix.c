/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

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

#define _GNU_SOURCE
#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <pwd.h>
#include <dlfcn.h>
#include <libgen.h>
#include <fcntl.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

qbool stdinIsATTY;

/*
=================
Sys_DefaultHomePath
=================
*/
const qchar *
Sys_DefaultHomePath(void)
{
  //used to determine where to store user-specific files
  static qchar homePath[MAX_OSPATH];

  const qchar *p;

  if (*homePath)
  {
    return homePath;
  }
            
  if ((p = getenv("HOME")) != NULL) 
  {
    Q_strncpyz(homePath, p, sizeof(homePath));
#if defined(MACOS_X)
    Q_strcat(homePath, sizeof(homePath), "/Library/Application Support/Tremulous");
#else
    Q_strcat(homePath, sizeof(homePath), "/.tremulous");
#endif
    if (mkdir( homePath, 0750)) 
    {
      if (errno != EEXIST) 
      {
        Sys_Error("Unable to create directory \"%s\", error is %s(%d)\n", homePath, strerror(errno), errno);
      }
    }

    return homePath;
  }

  return ""; // assume current dir
}

#if !defined(MACOS_X)
/*
================
Sys_TempPath
================
*/
const qchar *
Sys_TempPath(void)
{
  const qchar *TMPDIR = getenv("TMPDIR");

  if (TMPDIR == NULL || TMPDIR[0] == '\0')
  {
    return "/tmp";
  }
  else
  {
    return TMPDIR;
  }
}
#endif

/*
================
Sys_Milliseconds
================
*/
/* base time in seconds, that's our origin
   timeval:tv_sec is an qint:
   assuming this wraps every 0x7fffffff - ~68 years since the Epoch (1970) - we're safe till 2038
   using unsigned long data type to work right with Sys_XTimeToSysTime */
unsigned long sys_timeBase = 0;
/* current time in ms, using sys_timeBase as origin
   NOTE: sys_timeBase*1000 + curtime -> ms since the Epoch
     0x7fffffff ms - ~24 days
   although timeval:tv_usec is an qint, I'm not sure wether it is actually used as an unsigned qint
     (which would affect the wrap period) */
qint curtime;
qint Sys_Milliseconds (void)
{
	struct timeval tp;

	gettimeofday(&tp, NULL);

	if (!sys_timeBase)
	{
		sys_timeBase = tp.tv_sec;
		return tp.tv_usec/1000;
	}

	curtime = (tp.tv_sec - sys_timeBase)*1000 + tp.tv_usec/1000;

	return curtime;
}

#if !id386
/*
==================
fastftol
==================
*/
long fastftol( float f )
{
	return (long)f;
}

/*
==================
Sys_SnapVector
==================
*/
void Sys_SnapVector( float *v )
{
	v[0] = rint(v[0]);
	v[1] = rint(v[1]);
	v[2] = rint(v[2]);
}
#endif


/*
==================
Sys_RandomBytes
==================
*/
qbool Sys_RandomBytes( byte *string, qint len )
{
	FILE *fp;

	fp = fopen( "/dev/urandom", "r" );
	if( !fp )
		return qfalse;

        setvbuf(fp, NULL, _IONBF, 0); //dont buffer reads from urandom

	if( !fread( string, sizeof( byte ), len, fp ) )
	{
		fclose( fp );
		return qfalse;
	}

	fclose( fp );
	return qtrue;
}

/*
==================
Sys_GetCurrentUser
==================
*/
qchar *Sys_GetCurrentUser( void )
{
	struct passwd *p;

	if ( (p = getpwuid( getuid() )) == NULL ) {
		return "player";
	}
	return p->pw_name;
}

/*
==================
Sys_GetClipboardData
==================
*/
qchar *Sys_GetClipboardData(void)
{
	return NULL;
}

#define MEM_THRESHOLD 96*1024*1024

/*
==================
Sys_LowPhysicalMemory

TODO
==================
*/
qbool Sys_LowPhysicalMemory( void )
{
	return qfalse;
}

/*
==================
Sys_Basename
==================
*/
const qchar *Sys_Basename( qchar *path )
{
	return basename( path );
}

/*
==================
Sys_Dirname
==================
*/
const qchar *Sys_Dirname( qchar *path )
{
	return dirname( path );
}

/*
==================
Sys_Mkdir
==================
*/
qbool Sys_Mkdir( const qchar *path )
{
  qint result = mkdir(path, 0750);

  if (result)
  {
    return errno == EEXIST;
  }

  return qtrue;
}

/*
==================
Sys_FOpen
==================
*/
FILE *
Sys_FOpen(const qchar *ospath, const qchar *mode)
{
  struct stat buf;

  //check if path exists and is not a directory
  if (!stat(ospath, &buf) && S_ISDIR(buf.st_mode))
  {
    return NULL;
  }

  return fopen(ospath, mode);
}

/*
==============
Sys_ResetReadOnlyAttribute
==============
*/
qbool
Sys_ResetReadOnlyAttribute(const qchar *ospath)
{
  return qfalse;
}

/*
==================
Sys_Mkfifo
==================
*/
FILE *
Sys_Mkfifo(const qchar *ospath)
{
  FILE *fifo;
  qint result;
  qint fn;
  struct stat buf;

  //if file already exists AND is a pipefile, remove it
  if (!stat(ospath, &buf) && S_ISFIFO(buf.st_mode))
  {
    FS_Remove(ospath);
  }

  result = mkfifo(ospath, 0600);

  if (result)
  {
    return NULL;
  }

  fifo = fopen(ospath, "w+");

  if (fifo)
  {
    fn = fileno(fifo);
    fcntl(fn, F_SETFL, O_NONBLOCK);
  }

  return fifo;
}

/*
==============
Sys_Pwd
==============
*/
const qchar *
Sys_Pwd(void)
{
  static qchar pwd[MAX_OSPATH];

  if (pwd[0])
  {
    return pwd;
  }

  //more reliable, linux-specific
  if (readlink("/proc/self/exe", pwd, sizeof(pwd) - 1) != -1)
  {
    pwd[sizeof(pwd) - 1] = '\0';
    dirname(pwd);
    return pwd;
  }

  if (!getcwd(pwd, sizeof(pwd)))
  {
    pwd[0] = '\0';
  }

  return pwd;
}

/*
==============
Sys_DefaultBasePath
==============
*/
const qchar *
Sys_DefaultBasePath(void)
{
  return Sys_Pwd();
}

/*
==============================================================

DIRECTORY SCANNING

==============================================================
*/
static qint Sys_ListExtFiles( const qchar *directory, const qchar *subdir, const qchar *extension, const qchar *filter, qchar **list, qint maxfiles, qint subdirs )
{
	qchar		search[MAX_OSPATH * 2 + MAX_QPATH + 1];
	qchar		filename[MAX_OSPATH * 2];
	qint		nfiles;
	struct dirent	*d;
	DIR		*fdir;
	qint		extLen;
	struct stat st;
	qbool	hasPatterns;
	const qchar	*x;
	qbool	dironly;

	if ( extension[0] == '/' && extension[1] == 0 ) {
		extension = "";
		dironly = qtrue;
	} else {
		dironly = qfalse;
	}

	extLen = (qint)strlen( extension );
	hasPatterns = Com_HasPatterns( extension ); // contains either '?' or '*'
	if ( hasPatterns && extension[0] == '.' && extension[1] != '\0' ) {
		extension++;
	}

	nfiles = 0;

	if ( *subdir != '\0' ) {
		Com_sprintf( search, sizeof( search ), "%s/%s", directory, subdir );
	} else {
		Com_sprintf( search, sizeof( search ), "%s", directory );
	}

	if ((fdir = opendir(search)) == NULL) {
		return nfiles;
	}

	// search
	while ((d = readdir(fdir)) != NULL) {
		if ( search[0] != '\0' ) {
			Com_sprintf( filename, sizeof( filename ), "%s/%s", search, d->d_name );
		} else {
			Q_strncpyz( filename, d->d_name, sizeof( filename ) );
		}
		if (stat(filename, &st) == -1) {
			continue;
		}
		if (st.st_mode & S_IFDIR) {
			// handle recursion
			if ( subdirs > 0 ) {
				if ( !Q_streq( d->d_name, "." ) && !Q_streq( d->d_name, ".." ) ) {
					qchar subdir2[MAX_OSPATH * 2 + MAX_QPATH + 1];
					if ( *subdir != '\0' ) {
						Com_sprintf( subdir2, sizeof( subdir2 ), "%s/%s", subdir, d->d_name );
					} else {
						Q_strncpyz( subdir2, d->d_name, sizeof( subdir2 ) );
					}
					if ( nfiles >= maxfiles ) {
						break;
					}
					nfiles += Sys_ListExtFiles( directory, subdir2, extension, filter, list + nfiles, maxfiles - nfiles, subdirs - 1);
				}
			}
			if ( !dironly ) {
				continue;
			}
		} else {
			if ( dironly ) {
				continue;
			}
		}
		if ( *subdir != '\0' ) {
			Com_sprintf( filename, sizeof( filename ), "%s/%s", subdir, d->d_name );
		} else {
			Q_strncpyz( filename, d->d_name, sizeof( filename ) );
		}
		if ( filter != NULL && *filter != '\0' ) {
			if ( !Com_FilterPath( filter, filename ) ) {
				continue;
			}
		} else if ( *extension != '\0' ) {
			if ( hasPatterns ) {
				x = strrchr( d->d_name, '.' );
				if ( x == NULL || !Com_FilterExt( extension, x + 1 ) ) {
					continue;
				}
			} else {
				// check for exact extension
				const qint length = strlen( d->d_name );
				if ( length < extLen || Q_stricmp( d->d_name + length - extLen, extension ) ) {
					continue;
				}
			}
		}
		if ( nfiles >= maxfiles ) {
			break;
		}
		list[ nfiles++ ] = FS_CopyString( filename );
	}

	closedir( fdir );

	return nfiles;
}

qchar** Sys_ListFiles( const qchar *directory, const qchar *extension, const qchar *filter, qint *numfiles, qint subdirs )
{
	qchar**	listCopy;
	qchar*	list[MAX_FOUND_FILES];
	qint	i, nfiles;

	if ( extension == NULL ) {
		extension = "";
	}

	nfiles = Sys_ListExtFiles( directory, "", extension, filter, list, ARRAY_LEN( list ), subdirs );

	// copy list from stack
	listCopy = Z_Malloc( (nfiles + 1) * sizeof( listCopy[0] ) );
	for ( i = 0; i < nfiles; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

        if (nfiles > 1)
        {
          Com_SortList(listCopy, nfiles - 1);

          if (nfiles > 2)
          {
            if (Q_streq(listCopy[0], ".") && Q_streq(listCopy[1], ".."))
            {
              //emulate old strgtr() function sort behavior for special entries
              qchar *dot1 = listCopy[0];
              qchar *dot2 = listCopy[1];

              for(i = 0;i < nfiles - 2;i++)
              {
                listCopy[i] = listCopy[i + 2];
              }

              listCopy[nfiles - 2] = dot1;
              listCopy[nfiles - 1] = dot2;
            }
          }
        }

	*numfiles = nfiles;
	return listCopy;
}

/*
==================
Sys_FreeFileList
==================
*/
void Sys_FreeFileList( qchar **list )
{
	qint i;

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}

/*
=============
Sys_GetFileStats
=============
*/
qbool
Sys_GetFileStats(const qchar *filename, fileOffset_t *size, fileTime_t *mtime, fileTime_t *ctime)
{
  struct stat s;

  if (!stat(filename, &s))
  {
    *size = (fileOffset_t)s.st_size;
    *mtime = (fileTime_t)s.st_mtime;
    *ctime = (fileTime_t)s.st_ctime;
    return qtrue;
  }

  *size = 0;
  *mtime = *ctime = 0;
  return qfalse;
}

/*
==================
Sys_Sleep

Block execution for msec or until input is recieved.
==================
*/
void Sys_Sleep( qint msec )
{
	if( msec == 0 )
		return;

	if( stdinIsATTY )
	{
		fd_set fdset;

                FD_ZERO(&fdset);
                FD_SET(STDIN_FILENO, &fdset);

                if (msec < 0)
                {
                  select(STDIN_FILENO + 1, &fdset, NULL, NULL, NULL);
                }
                else
                {
                  struct timeval timeout;

                  timeout.tv_sec = msec/1000;
                  timeout.tv_usec = (msec % 1000) * 1000;
                  select(STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout);
                }
	}
	else
	{
	        //with nothing to select() on, we can't wait indefinitely
	        if (msec < 0)
	        {
	          msec = 10;
	        }

		usleep(msec * 1000);
	}
}

/*
==============
Sys_ErrorDialog

Display an error message
==============
*/
void Sys_ErrorDialog( const qchar *error )
{
	qchar buffer[ 1024 ];
	unsigned qint size;
	qint f = -1;
	const qchar *homepath = Cvar_VariableString("fs_homepath");
	const qchar *gamedir = Cvar_VariableString("fs_game");
	const qchar *fileName = "crashlog.txt";
	qchar *ospath = FS_BuildOSPath(homepath, gamedir, fileName);

	Sys_Print(va(NULL, "%s\n", error));

#if !defined(DEDICATED)
        Sys_Dialog(DT_ERROR, va(NULL, "%s. See \"%s\" for details.", error, ospath), "Error");
#endif

        /*make sure the write path for the crashlog exists*/
        if (FS_CreatePath(ospath))
        {
          Com_Printf("ERROR: couldn't create path '%s' for crash log.\n", ospath);
          return;
        }

        /*we might be crashing because we maxed out the quake MAX_FILE_HANDLES which will come through here so we dont want to use recurse forever by calling FS_FOpenFileWrite(), use the unix system apis instead*/
        f = open(ospath, O_CREAT | O_TRUNC | O_WRONLY, 0640);

        if (f == -1)
	{
		Com_Printf( "ERROR: couldn't open %s\n", fileName );
		return;
	}

        /*we are crashing so we dont care much if write or close fails*/
	while( ( size = CON_LogRead( buffer, sizeof( buffer ) ) ) > 0 )
	{
	  if (write(f, buffer, size) != size)
	  {
	    Com_Printf("ERROR: couldn't fully write to %s\n", fileName);
	    break;
	  }
	}

	close(f);
}

#ifndef MACOS_X
/*
==============
Sys_ZenityCommand
==============
*/
static qint Sys_ZenityCommand( dialogType_t type, const qchar *message, const qchar *title )	
{
	const qchar *options = "";
	qchar       command[ 1024 ];

	switch( type )
	{
		default:
		case DT_INFO:      options = "--info"; break;
		case DT_WARNING:   options = "--warning"; break;
		case DT_ERROR:     options = "--error"; break;
		case DT_YES_NO:    options = "--question --ok-label=\"Yes\" --cancel-label=\"No\""; break;
		case DT_OK_CANCEL: options = "--question --ok-label=\"OK\" --cancel-label=\"Cancel\""; break;
	}

	Com_sprintf( command, sizeof( command ), "zenity %s --text=\"%s\" --title=\"%s\"",
		options, message, title );

	return system( command );
}

/*
==============
Sys_KdialogCommand
==============
*/
static qint Sys_KdialogCommand( dialogType_t type, const qchar *message, const qchar *title )
{
	const qchar *options = "";
	qchar       command[ 1024 ];

	switch( type )
	{
		default:
		case DT_INFO:      options = "--msgbox"; break;
		case DT_WARNING:   options = "--sorry"; break;
		case DT_ERROR:     options = "--error"; break;
		case DT_YES_NO:    options = "--warningyesno"; break;
		case DT_OK_CANCEL: options = "--warningcontinuecancel"; break;
	}

	Com_sprintf( command, sizeof( command ), "kdialog %s \"%s\" --title \"%s\"",
		options, message, title );

	return system( command );
}

/*
==============
Sys_XmessageCommand
==============
*/
static qint Sys_XmessageCommand( dialogType_t type, const qchar *message, const qchar *title )
{
	const qchar *options = "";
	qchar       command[ 1024 ];

	switch( type )
	{
		default:           options = "-buttons OK"; break;
		case DT_YES_NO:    options = "-buttons Yes:0,No:1"; break;
		case DT_OK_CANCEL: options = "-buttons OK:0,Cancel:1"; break;
	}

	Com_sprintf( command, sizeof( command ), "xmessage -center %s \"%s\"",
		options, message );

	return system( command );
}

/*
==============
Sys_Dialog

Display a *nix dialog box
==============
*/
dialogResult_t Sys_Dialog( dialogType_t type, const qchar *message, const qchar *title )
{
	typedef enum
	{
		NONE = 0,
		ZENITY,
		KDIALOG,
		XMESSAGE,
		NUM_DIALOG_PROGRAMS
	} dialogCommandType_t;
	typedef qint (*dialogCommandBuilder_t)( dialogType_t, const qchar *, const qchar * );

	const qchar              *session = getenv( "DESKTOP_SESSION" );
	qbool                tried[ NUM_DIALOG_PROGRAMS ] = { qfalse };
	dialogCommandBuilder_t  commands[ NUM_DIALOG_PROGRAMS ] = { NULL };
	dialogCommandType_t     preferredCommandType = NONE;

	commands[ ZENITY ] = &Sys_ZenityCommand;
	commands[ KDIALOG ] = &Sys_KdialogCommand;
	commands[ XMESSAGE ] = &Sys_XmessageCommand;

	// This may not be the best way
	if( !Q_stricmp( session, "gnome" ) )
		preferredCommandType = ZENITY;
	else if( !Q_stricmp( session, "kde" ) )
		preferredCommandType = KDIALOG;

	while( 1 )
	{
		qint i;
		qint exitCode;

		for( i = NONE + 1; i < NUM_DIALOG_PROGRAMS; i++ )
		{
			if( preferredCommandType != NONE && preferredCommandType != i )
				continue;

			if( !tried[ i ] )
			{
				exitCode = commands[ i ]( type, message, title );

				if( exitCode >= 0 )
				{
					switch( type )
					{
						case DT_YES_NO:    return exitCode ? DR_NO : DR_YES;
						case DT_OK_CANCEL: return exitCode ? DR_CANCEL : DR_OK;
						default:           return DR_OK;
					}
				}

				tried[ i ] = qtrue;

				// The preference failed, so start again in order
				if( preferredCommandType != NONE )
				{
					preferredCommandType = NONE;
					break;
				}
			}
		}

		for( i = NONE + 1; i < NUM_DIALOG_PROGRAMS; i++ )
		{
			if( !tried[ i ] )
				continue;
		}

		break;
	}

	Com_DPrintf( S_COLOR_YELLOW "WARNING: failed to show a dialog\n" );
	return DR_OK;
}
#endif

/*
==============
Sys_GLimpSafeInit

Unix specific "safe" GL implementation initialization
==============
*/
void
Sys_GLimpSafeInit(void)
{
  //NOP
}

/*
==============
Sys_GLimpInit

Unix specific GL implementation initialisation
==============
*/
void Sys_GLimpInit( void )
{
	// NOP
}

/*
==============
Sys_PlatformInit

Unix specific initialisation
==============
*/
void
Sys_PlatformInit(void)
{
  const qchar *term = getenv("TERM");

#if !defined(USE_JAVA)
  signal(SIGHUP, Sys_SigHandler);
  signal(SIGQUIT, Sys_SigHandler);
  signal(SIGTRAP, Sys_SigHandler);
  signal(SIGIOT, Sys_SigHandler);
  signal(SIGBUS, Sys_SigHandler);
#endif

  stdinIsATTY = isatty(STDIN_FILENO) && !(term && (!strcmp(term, "raw") || !strcmp(term, "dumb")));
}

/*
==================
Sys_PlatformExit

Unix specific deinitialization
==================
*/
void
Sys_PlatformExit(void)
{
}

/*
==============
Sys_SetEnv

set/unset environment variables (empty value removes it)
==============
*/
void
Sys_SetEnv(const qchar *name, const qchar *value)
{
  if (value && *value)
  {
    setenv(name, value, 1);
  }
  else
  {
    unsetenv(name);
  }
}

/*
==============
Sys_PID
==============
*/
qint
Sys_PID(void)
{
  return getpid();
}

/*
==============
Sys_PIDIsRunning
==============
*/
qbool
Sys_PIDIsRunning(qint pid)
{
  return !kill(pid, 0);
}

/*
========================================================================

LOAD/UNLOAD DLL

========================================================================
*/


static qint dll_err_count = 0;


/*
=================
Sys_LoadLibrary
=================
*/
void *
Sys_LoadLibrary(const qchar *name)
{
  const qchar *ext;
  void *handle;

  if (FS_AllowedExtension(name, qfalse, &ext))
  {
    Com_Error(ERR_FATAL, "Sys_LoadLibrary: Unable to load library with '%s' extension", ext);
  }

  handle = dlopen(name, RTLD_NOW);
  return handle;
}


/*
=================
Sys_UnloadLibrary
=================
*/
void
Sys_UnloadLibrary(void *handle)
{
  if (handle != NULL)
  {
    dlclose(handle);
  }
}


/*
=================
Sys_LoadFunction
=================
*/
void *
Sys_LoadFunction(void *handle, const qchar *name)
{
  const qchar *error;
  qchar buf[1024];
  void *symbol;
  size_t nlen;

  if (handle == NULL || name == NULL || *name == '\0') 
  {
    dll_err_count++;
    return NULL;
  }

  dlerror(); /*clear old error state*/
  symbol = dlsym(handle, name);
  error = dlerror();

  if (error != NULL)
  {
    nlen = strlen(name) + 1;

    if (nlen >= sizeof(buf))
    {
      return NULL;
    }

    buf[0] = '_';
    strcpy(buf + 1, name);
    dlerror(); /*clear old error state*/
    symbol = dlsym(handle, buf);
  }

  if (!symbol)
  {
    dll_err_count++;
  }

  return symbol;
}


/*
=================
Sys_LoadFunctionErrors
=================
*/
qint
Sys_LoadFunctionErrors(void)
{
  qint result = dll_err_count;

  dll_err_count = 0;
  return result;
}

#if defined(USE_AFFINITY_MASK)
/*
=================
Sys_GetAffinityMask
=================
*/
uint64_t
Sys_GetAffinityMask(void)
{
  cpu_set_t cpu_set;

  if (sched_getaffinity(getpid(), sizeof(cpu_set), &cpu_set) == 0)
  {
    uint64_t mask = 0;
    qint cpu;

    for(cpu = 0;cpu < sizeof(mask) * 8;cpu++)
    {
      if (CPU_ISSET(cpu, &cpu_set))
      {
        mask |= (1ULL << cpu);
      }
    }

    return mask;
  }

  return 0;
}

/*
=================
Sys_SetAffinityMask
=================
*/
qbool
Sys_SetAffinityMask(const uint64_t mask)
{
  cpu_set_t cpu_set;
  qint cpu;

  CPU_ZERO(&cpu_set);

  for(cpu = 0;cpu < sizeof(mask) * 8;cpu++)
  {
    if (mask & (1ULL << cpu))
    {
      CPU_SET(cpu, &cpu_set);
    }
  }

  if (sched_setaffinity(getpid(), sizeof(cpu_set), &cpu_set) == 0)
  {
    return qtrue;
  }

  return qfalse;
}
#endif //USE_AFFINITY_MASK
