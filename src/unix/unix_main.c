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
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#include <libgen.h> //dirname

#include <dlfcn.h>

#if defined(__linux__)
#if defined(__GLIBC__)
#include <fpu_control.h> //bk001213 - force dumps on divide by zero
#endif
#endif

#if defined(__sun)
#include <sys/file.h>
#endif

//FIXME TTimo should we gard this? most *nix system should comply?
#include <termios.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
//#include "../renderercommon/tr_public.h"

#include "linux_local.h" //bk001204

#if !defined(DEDICATED)
#include "../client/client.h"
#endif

unsigned sys_frame_time;

qbool stdin_active = qfalse;
qint stdin_flags = 0;

//=============================================================
//tty console variables
//=============================================================

typedef enum
{
  TTY_ENABLED,
  TTY_DISABLED,
  TTY_ERROR
}
tty_err;

//enable/disabled tty input mode
//NOTE TTimo this is used during startup, cannot be changed during run
static cvar_t *ttycon = NULL;

//general flag to tell about tty console mode
static qbool ttycon_on = qfalse;

//when printing general stuff to stdout stderr (Sys_Printf)
//we need to disable the tty console stuff
//this increments so we can recursively disable
static qint ttycon_hide = 0;

//some key codes that the terminal may be using
//TTimo NOTE: I'm not sure how relevant this is
static qint tty_erase;
static qint tty_eof;

static struct termios tty_tc;

static field_t tty_con;

static qbool ttycon_color_on = qfalse;

tty_err
Sys_ConsoleInputInit(void);

//=======================================================================
//General routines
//=======================================================================

//bk001207
#define MEM_THRESHOLD 96 * 1024 * 1024

/*
==================
Sys_LowPhysicalMemory()
==================
*/
qbool
Sys_LowPhysicalMemory(void)
{
  //MEMORYSTATUS stat;
  //GlobalMemoryStatus(&stat);
  //return (stat.dwTotalPhys <= MEM_THRESHOLD) ? qtrue:qfalse;
  return qfalse; //bk001207 - FIXME
}

void
Sys_BeginProfiling(void)
{
}

//=============================================================
//tty console routines
//NOTE: if the user is editing a line when something gets printed to the early console then it won't look good
//so we provide tty_Clear and tty_Show to be called before and after a stdout or stderr output
//=============================================================

//flush stdin, I suspect some terminals are sending a LOT of shit
//FIXME TTimo relevant?
static void
tty_FlushIn(void)
{
#if 1
  tcflush(STDIN_FILENO, TCIFLUSH);
#else
  qchar key;

  while(read(STDIN_FILENO, &key, 1) > 0);
#endif
}

//do a backspace
//TTimo NOTE: it seems on some terminals just sending '\b' is not enough
//so for now, in any case we send "\b \b" .. yeah well ..
//(there may be a way to find out if '\b' alone would work though)
static void
tty_Back(void)
{
  write(STDOUT_FILENO, "\b \b", 3);
}

//clear the display of the line currently edited
//bring cursor back to beginning of line
void
tty_Hide(void)
{
  qint i;

  if (!ttycon_on)
  {
    return;
  }

  if (ttycon_hide)
  {
    ttycon_hide++;
    return;
  }

  if (tty_con.cursor > 0)
  {
    for(i = 0;i < tty_con.cursor;i++)
    {
      tty_Back();
    }
  }

  tty_Back(); //delete "]" ? -EC-
  ttycon_hide++;
}

//show the current line
//FIXME TTimo need to position the cursor if needed?
void
tty_Show(void)
{
  if (!ttycon_on)
  {
    return;
  }

  if (ttycon_hide > 0)
  {
    ttycon_hide--;

    if (ttycon_hide == 0)
    {
      write(STDOUT_FILENO, "]", 1); //-EC-

      if (tty_con.cursor > 0)
      {
        write(STDOUT_FILENO, tty_con.buffer, tty_con.cursor);
      }
    }
  }
}

//never exit without calling this, or your terminal will be left in a pretty bad state
void
Sys_ConsoleInputShutdown(void)
{
  if (ttycon_on)
  {
    //Com_Printf("Shutdown tty console\n"); //-EC-
    tty_Back(); //delete "]" ? -EC-
    tcsetattr(STDIN_FILENO, TCSADRAIN, &tty_tc);
  }

  //restore blocking to stdin reads
  if (stdin_active)
  {
    fcntl(STDIN_FILENO, F_SETFL, stdin_flags);
    //fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) & ~O_NONBLOCK);
  }

  Com_Memset(&tty_con, 0, sizeof(tty_con));

  stdin_active = qfalse;
  ttycon_on = qfalse;

  ttycon_hide = 0;
}

/*
==================
CON_SigCont

Reinitialize console input after receiving SIGCONT, as on Linux the terminal seems to lose all
set attributes if user did CTRL + Z and then does fg again.
==================
*/
void
CON_SigCont(qint signum)
{
  Sys_ConsoleInputInit();
}

void
CON_SigTStp(qint signum)
{
  sigset_t mask;

  tty_FlushIn();
  Sys_ConsoleInputShutdown();

  sigemptyset(&mask);
  sigaddset(&mask, SIGTSTP);
  sigprocmask(SIG_UNBLOCK, &mask, NULL);

  signal(SIGTSTP, SIG_DFL);

  kill(getpid(), SIGTSTP);
}

//=============================================================
//general sys routines
//=============================================================

//single exit point (regular exit or in case of signal fault)
void NORETURN
Sys_Exit(qint code)
{
  Sys_ConsoleInputShutdown();

#if defined(NDEBUG) //regular behavior
  //we can't do this
  //as long as GL DLL's keep installign with atexit...
  //exit(ex);
  _exit(code);
#else
  //give me a backtrace on error exits.
  assert(code == 0);
  exit(code);
#endif
}

void NORETURN
Sys_Quit(void)
{
#if !defined(DEDICATED)
  CL_Shutdown("", qtrue);
#endif

  Sys_Exit(0);
}

void
Sys_Init(void)
{
  Cvar_Set("arch", OS_STRING " " ARCH_STRING);
  //IN_Init(); //rcg08312005 moved to glimp.
}

void NORETURN FORMAT_PRINTF(1, 2) QDECL
Sys_Error(const qchar *format, ...)
{
  va_list argptr;
  qchar text[1024];

  //change stdin to non blocking
  //NOTE TTimo not sure how well that goes with tty console mode
  if (stdin_active)
  {
    //fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) & ~FNDELAY);
    fcntl(STDIN_FILENO, F_SETFL, stdin_flags);
  }

  //don't bother do a show on this one heh
  if (ttycon_on)
  {
    tty_Hide();
  }

  va_start(argptr, format);
  Q_vsnprintf(text, sizeof(text), format, argptr);
  va_end(argptr);

#if !defined(DEDICATED)
  CL_Shutdown(text, qtrue);
#endif

  fprintf(stderr, "Sys_Error: %s\n", text);

  Sys_Exit(1); //bk010104 - use single exit point.
}

void
floating_point_exception_handler(qint whatever)
{
  signal(SIGFPE, floating_point_exception_handler);
}

//initialize the console input(tty mode if wanted and possible)
//warning: might be called from signal handler
tty_err
Sys_ConsoleInputInit(void)
{
  struct termios tc;
  const qchar *term;

  //TTimo
  //https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=390
  //ttycon 0 or 1, if the process is backgrounded (running non interactively)
  //then SIGTTIN or SIGTOU is emitted, if not catched, turns into SIGSTP
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);

  //if SIGCONT is received, reinitialize console
  signal(SIGCONT, CON_SigCont);

  if (signal(SIGTSTP, SIG_IGN) == SIG_DFL)
  {
    signal(SIGTSTP, CON_SigTStp);
  }

  stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);

  if (stdin_flags == -1)
  {
    stdin_active = qfalse;
    return TTY_ERROR;
  }

  //set non-blocking mode
  fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK);
  stdin_active = qtrue;

  //FIXME TTimo initialize this in Sys_Init or something?
  if (!ttycon || !ttycon->integer)
  {
    ttycon_on = qfalse;
    return TTY_DISABLED;
  }

  term = getenv("TERM");

  if (isatty(STDIN_FILENO) != 1 || !term || !strcmp(term, "dumb") || !strcmp(term, "raw"))
  {
    ttycon_on = qfalse;
    return TTY_ERROR;
  }

  Field_Clear(&tty_con);
  tcgetattr(STDIN_FILENO, &tty_tc);
  tty_erase = tty_tc.c_cc[VERASE];
  tty_eof = tty_tc.c_cc[VEOF];
  tc = tty_tc;

  /*
    ECHO: don't echo input characters
    ICANON: enable canonical mode. This enables the special
            characters EOF, EOL, EOL2, ERASE, KILL, REPRINT,
            STATUS, and WERASE, and buffers by lines.
    ISIG: when any of the characters INTR, QUIT, SUSP, or
            DSUSP are received, generate the corresponding signal
  */
  tc.c_lflag &= ~(ECHO | ICANON);
  /*
    ISTRIP strip off bit 8
    INPCK enable input parity checking
  */
  tc.c_iflag &= ~(ISTRIP | INPCK);
  tc.c_cc[VMIN] = 1;
  tc.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSADRAIN, &tc);

  if (com_ansiColor && com_ansiColor->integer)
  {
    ttycon_color_on = qtrue;
  }

  ttycon_on = qtrue;

  tty_Hide();
  tty_Show();

  return TTY_ENABLED;
}

qchar *
Sys_ConsoleInput(void)
{
  //we use this when sending back commands
  static qchar text[sizeof(tty_con.buffer)];
  qint avail;
  qchar key;
  qchar *s;
  field_t history;

  if (ttycon_on)
  {
    avail = read(STDIN_FILENO, &key, 1);

    if (avail != -1)
    {
      //we have something
      //backspace?
      //NOTE TTimo testing a lot of values .. seems it's the only way to get it to work everywhere
      if ((key == tty_erase) || (key == 127) || (key == 8))
      {
        if (tty_con.cursor > 0)
        {
          tty_con.cursor--;
          tty_con.buffer[tty_con.cursor] = '\0';
          tty_Back();
        }

        return NULL;
      }

      //check if this is a control char
      if (key && key < ' ')
      {
        if (key == '\n')
        {
          //push it in history
          Con_SaveField(&tty_con);
          s = tty_con.buffer;

          while(*s == '\\' || *s == '/') //skip leading slashes
          {
            s++;
          }

          Q_strncpyz(text, s, sizeof(text));
          Field_Clear(&tty_con);
          write(STDOUT_FILENO, "\n]", 2);
          return text;
        }

        if (key == '\t')
        {
          tty_Hide();
          Field_AutoComplete(&tty_con);
          tty_Show();
          return NULL;
        }

        avail = read(STDIN_FILENO, &key, 1);

        if (avail != -1)
        {
          //VT 100 keys
          if (key == '[' || key == 'O')
          {
            avail = read(STDIN_FILENO, &key, 1);

            if (avail != -1)
            {
              switch(key)
              {
                case
                'A':
                  if (Con_HistoryGetPrev(&history))
                  {
                    tty_Hide();
                    tty_con = history;
                    tty_Show();
                  }

                  tty_FlushIn();
                  return NULL;

                case
                'B':
                  if (Con_HistoryGetNext(&history))
                  {
                    tty_Hide();
                    tty_con = history;
                    tty_Show();
                  }

                  tty_FlushIn();
                  return NULL;

                case
                'C': //right

                case
                'D': //left

                //case
                //'H': //home

                //case
                //'F': //end
                  return NULL;
              }
            }
          }
        }

        if (key == 12) //clear terminal
        {
          write(STDOUT_FILENO, "\ec]", 3);

          if (tty_con.cursor)
          {
            write(STDOUT_FILENO, tty_con.buffer, tty_con.cursor);
          }

          tty_FlushIn();
          return NULL;
        }

        Com_DPrintf("dropping ISCTL sequence: %d, tty_erase: %d\n", key, tty_erase);
        tty_FlushIn();
        return NULL;
      }

      if (tty_con.cursor >= sizeof(text) - 1)
      {
        return NULL;
      }

      //push regular character
      tty_con.buffer[tty_con.cursor] = key;
      tty_con.cursor++;

      //print the current line (this is differential
      write(STDOUT_FILENO, &key, 1);
    }

    return NULL;
  }
  else if (stdin_active && com_dedicated->integer)
  {
    qint len;
    fd_set fdset;
    struct timeval timeout;

    FD_ZERO(&fdset);
    FD_SET(STDIN_FILENO, &fdset); //stdin
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    if (select(STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(STDIN_FILENO, &fdset))
    {
      return NULL;
    }

    len = read(STDIN_FILENO, text, sizeof(text));

    if (len == 0) //eof!
    {
      fcntl(STDIN_FILENO, F_SETFL, stdin_flags);
      stdin_active = qfalse;
      return NULL;
    }

    if (len < 1)
    {
      return NULL;
    }

    text[len - 1] = '\0'; //rip off the /n and terminate
    s = text;

    while(*s == '\\' || *s == '/') //skip leading slashes
    {
      s++;
    }

    return s;
  }

  return NULL;
}

/*
=================
Sys_SendKeyEvents

Platform-dependent event handling
=================
*/
void
Sys_SendKeyEvents(void)
{
#if !defined(DEDICATED)
  HandleEvents();
#endif
}

/*
==================
Sys_Sleep

Block execution for msec or until input is recieved.
==================
*/
void
Sys_Sleep(qint msec)
{
  struct timeval timeout;
  fd_set fdset;
  qint res;

  //if (msec == 0)
  //{
    //return;
  //}

  if (msec < 0)
  {
    //special case: wait for console input or network packet
    if (stdin_active)
    {
      msec = 300;

      do
      {
        FD_ZERO(&fdset);
        FD_SET(STDIN_FILENO, &fdset);
        timeout.tv_sec = msec / 1000;
        timeout.tv_usec = (msec % 1000) * 1000;
        res = select(STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout);
      }
      while(res == 0 && NET_Sleep(10 * 1000));
    }
    else
    {
      //can happen only if no map loaded
      //which means we totally stuck as stdin is also disabled :P
      //usleep(300 * 1000);
      while(NET_Sleep(3000 * 1000));
    }

    return;
  }
#if 1
  struct timespec req;

  req.tv_sec = msec / 1000;
  req.tv_nsec = (msec % 1000) * 1000000;
  nanosleep(&req, NULL);
#else
  if (com_dedicated->integer && stdin_active)
  {
    FD_ZERO(&fdset);
    FD_SET(STDIN_FILENO, &fdset);
    timeout.tv_sec = msec / 1000;
    timeout.tv_usec = (msec % 1000) * 1000;
    select(STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout);
  }
  else
  {
    usleep(msec * 1000);
  }
#endif
}

static const struct
Q3ToAnsiColorTable_s
{
  const qchar Q3color;
  const qchar *ANSIcolor;
}
tty_colorTable[] =
{
  {COLOR_BLACK, "30"},
  {COLOR_RED, "31"},
  {COLOR_GREEN, "32"},
  {COLOR_YELLOW, "33"},
  {COLOR_BLUE, "34"},
  {COLOR_CYAN, "35"},
  {COLOR_MAGENTA, "36"},
  {COLOR_WHITE, "0"}
};

static const qchar *
getANSIcolor(qchar Q3color)
{
  qint i;

  for(i = 0;i < ARRAY_LEN(tty_colorTable);i++)
  {
    if (Q3color == tty_colorTable[i].Q3color)
    {
      return tty_colorTable[i].ANSIcolor;
    }
  }

  return NULL;
}

static qbool
printableChar(qchar c)
{
  if ((c >= ' ' && c <= '~') || c == '\n' || c == '\r' || c == '\t')
  {
    return qtrue;
  }

  return qfalse;
}

void
Sys_ANSIColorify(const qchar *msg, qchar *buffer, qint bufferSize)
{
  qint msgLength;
  qint i;
  qchar tempBuffer[8];
  const qchar *ANSIcolor;

  if (!msg || !buffer)
  {
    return;
  }

  msgLength = strlen(msg);
  i = 0;
  buffer[0] = '\0';

  while(i < msgLength)
  {
    if (msg[i] == '\n')
    {
      Com_sprintf(tempBuffer, sizeof(tempBuffer), "%c[0m\n", 0x1B);
      strncat(buffer, tempBuffer, bufferSize - 1);
      i += 1;
    }
    else if (msg[i] == Q_COLOR_ESCAPE && (ANSIcolor = getANSIcolor(msg[i + 1])) != NULL)
    {
      Com_sprintf(tempBuffer, sizeof(tempBuffer), "%c[%sm", 0x1B, ANSIcolor);
      strncat(buffer, tempBuffer, bufferSize - 1);
      i += 2;
    }
    else
    {
      if (printableChar(msg[i]))
      {
        Com_sprintf(tempBuffer, sizeof(tempBuffer), "%c", msg[i]);
        strncat(buffer, tempBuffer, bufferSize - 1);
      }

      i += 1;
    }
  }
}

void
Sys_Print(const qchar *msg)
{
  qchar printmsg[MAXPRINTMSG];
  size_t len;

  if (ttycon_on)
  {
    tty_Hide();
  }

  if (ttycon_on && ttycon_color_on)
  {
    Sys_ANSIColorify(msg, printmsg, sizeof(printmsg));
    len = strlen(printmsg);
  }
  else
  {
    qchar *out = printmsg;

    while(*msg != '\0' && out < printmsg + sizeof(printmsg))
    {
      if (printableChar(*msg))
      {
        *out++ = *msg;
      }

      msg++;
    }

    len = out - printmsg;
  }

  write(STDERR_FILENO, printmsg, len);

  if (ttycon_on)
  {
    tty_Show();
  }
}

void QDECL
Sys_SetStatus(const qchar *format, ...)
{
  return;
}

void
Sys_ConfigureFPU(void) //bk001213 - divide by zero
{
#if defined(__linux__)
#if defined(__i386)
#if defined(__GLIBC__)
#if !defined(NDEBUG)
  //bk0101022 - enable FPE's in debug mode
  static qint fpu_word = _FPU_DEFAULT & ~(_FPU_MASK_ZM | _FPU_MASK_IM);
  qint current = 0;

  _FPU_GETCW(current);

  if (current != fpu_word)
  {
#if 0
    Com_Printf("FPU Control 0x%x (was 0x%x)\n", fpu_word, current);
    _FPU_SETCW(fpu_word);
    _FPU_GETCW(current);
    assert(fpu_word == current);
#endif
  }
#else //NDEBUG
  static qint fpu_word = _FPU_DEFAULT;

  _FPU_SETCW(fpu_word);
#endif //NDEBUG
#endif //__GLIBC__
#endif //__i386
#endif //__linux
}

void
Sys_PrintBinVersion(const qchar *name)
{
  const qchar *date = __DATE__;
  const qchar *time = __TIME__;
  const qchar *sep = "==============================================================";

  fprintf(stdout, "\n\n%s\n", sep);
#if defined(DEDICATED)
  fprintf(stdout, "Linux Tremulous Dedicated Server [%s %s]\n", date, time);
#else
  fprintf(stdout, "Linux Tremulous Full Executable [%s %s]\n", date, time);
#endif
  fprintf(stdout, " local install: %s\n", name);
  fprintf(stdout, "%s\n\n", sep);
}

#if defined(__APPLE__)
static qchar binaryPath[MAX_OSPATH] = {0};
static qchar installPath[MAX_OSPATH] = {0};

/*
=================
Sys_SetBinaryPath
=================
*/
static void
Sys_SetBinaryPath(const qchar *path)
{
  qchar *d;

  Q_strncpyz(binaryPath, path, sizeof(binaryPath));

  d = dirname(binaryPath);

  if (d != NULL && d != binaryPath)
  {
    Q_strncpyz(binaryPath, d, sizeof(binaryPath));
  }
}

/*
=================
Sys_SetDefaultBasePath
=================
*/
static void
Sys_SetDefaultBasePath(const qchar *path)
{
  Q_strncpyz(installPath, path, sizeof(installPath));
}

/*
=================
Sys_StripAppBundle

Discovers if passed dir is suffixed with the directory structure of a Mac OS X
.app bundle. If it is, the .app directory structure is stripped off the end and
the result is returned. If not, dir is returned untouched.
=================
*/
//used to determine where to store user-specific files
static qchar *
Sys_StripAppBundle(qchar *dir)
{
  static qchar cwd[MAX_OSPATH];

  Q_strncpyz(cwd, dir, sizeof(cwd));

  if (strcmp(basename(cwd), "MacOS") != 0)
  {
    return dir;
  }

  Q_strncpyz(cwd, dirname(cwd), sizeof(cwd));

  if (strcmp(basename(cwd), "Contents") != 0)
  {
    return dir;
  }

  Q_strncpyz(cwd, dirname(cwd), sizeof(cwd));

  if (strstr(basename(cwd), ".app") == NULL)
  {
    return dir;
  }

  Q_strncpyz(cwd, dirname(cwd), sizeof(cwd));

  return cwd;
}

/*
=================
Sys_DefaultAppPath
=================
*/
qchar *
Sys_DefaultAppPath(void)
{
  return binaryPath;
}
#endif //__APPLE__

/*
=================
Sys_DefaultBasePath
=================
*/
const qchar *
Sys_DefaultBasePath(void)
{
#if defined(__APPLE__)
  if (installPath[0] != '\0')
  {
    return installPath;
  }
#endif
  return Sys_Pwd();
}

/*
=================
Sys_BinName

This resolves any symlinks to the binary. It's disabled for debug
builds because there are situations where you are likely to want
to symlink to binaries and /not/ have the links resolved.
=================
*/
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
const qchar *
Sys_BinName(const qchar *arg0)
{
  static qchar dst[PATH_MAX];

#if defined(NDEBUG)

#if defined(__linux__)
  qint n = readlink("/proc/self/exe", dst, PATH_MAX - 1);

  if (n >= 0 && n < PATH_MAX)
  {
    dst[n] = '\0';
  }
  else
  {
    Q_strncpyz(dst, arg0, PATH_MAX);
  }
#elif defined(__APPLE__)
  uint32_t bufsize = sizeof(dst);

  if (_NSGetExecutablePath(dst, &bufsize) == -1)
  {
    Q_strncpyz(dst, arg0, PATH_MAX);
  }
#else

#warning Sys_BinName not implemented
  Q_strncpyz(dst, arg0, PATH_MAX);
#endif

#else //DEBUG
  Q_strncpyz(dst, arg0, PATH_MAX);
#endif
  return dst;
}

static qbool
Sys_ParseArgs(qint argc, const qchar *argv[])
{
  if (argc == 2)
  {
    if ((!strcmp(argv[1], "--version")) || (!strcmp(argv[1], "-v")))
    {
      Sys_PrintBinVersion(Sys_BinName(argv[0]));
      return qtrue;
    }
  }

  return qfalse;
}

qint
main(qint argc, const qchar *argv[])
{
  qchar con_title[MAX_CVAR_VALUE_STRING];
  qint xpos;
  qint ypos;
  //qbool useXYpos;
  qchar *cmdline;
  qint len;
  qint i;
  tty_err err;

#if defined(__APPLE__)
  //this is passed if we are launched by double clicking
  if (argc >= 2 && Q_strncmp(argv[1], "-psn", 4) == 0)
  {
    argc = 1;
  }
#endif

  if (Sys_ParseArgs(argc, argv))
  {
    return 0; //print version and exit
  }

#if defined(__APPLE__)
  Sys_SetBinaryPath(argv[0]);
  Sys_SetDefaultBasePath(Sys_StripAppBundle(binaryPath));
#endif

  //merge the command line, this is kinda silly
  len = 1;

  for(i = 1;i < argc;i++)
  {
    len += strlen(argv[i]) + 1;
  }

  cmdline = malloc(len);
  *cmdline = '\0';

  for(i = 1;i < argc;i++)
  {
    if (i > 1)
    {
      strcat(cmdline, " ");
    }

    strcat(cmdline, argv[i]);
  }

  /*useXYpos = */Com_EarlyParseCmdLine(cmdline, con_title, sizeof(con_title), &xpos, &ypos);

  //bk000306 - clear queues
  //memset(&eventQue[0], 0, sizeof(eventQue));
  //memset(&sys_packetReceived[0], 0, sizeof(sys_packetReceived));

  Com_Init(cmdline);

  //Sys_ConsoleInputInit() might be called in signal handler
  //so modify/init any cvars here
  ttycon = Cvar_GetAndDescribe("ttycon", "1", 0, "Enable access to input/output console terminal.");

  err = Sys_ConsoleInputInit();

  if (err == TTY_ENABLED)
  {
    Com_Printf("Started tty console (use +set ttycon 0 to disable)\n");
  }
  else
  {
    if (err == TTY_ERROR)
    {
      Com_Printf("stdin is not a tty, tty console mode failed\n");
      Cvar_Set("ttycon", "0");
    }
  }

#if defined(DEDICATED)
  //init here for dedicated, as we don't have GLimp_Init
  InitSig();
#endif

  while(1)
  {
#if defined(__linux__)
    Sys_ConfigureFPU();
#endif

#if defined(DEDICATED)
    //run the game
    Com_Frame(qfalse);
#else
    //check for other input devices
    IN_Frame();

    //run the game
    Com_Frame(CL_NoDelay());
#endif
  }

  //never gets here
  return 0;
}
