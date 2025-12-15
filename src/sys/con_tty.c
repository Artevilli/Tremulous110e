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


#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <stdio.h>

/*
=============================================================
tty console routines

NOTE: if the user is editing a line when something gets printed to the early
console then it won't look good so we provide CON_Hide and CON_Show to be
called before and after a stdout or stderr output
=============================================================
*/

extern qbool stdinIsATTY;
static qbool stdin_active;
// general flag to tell about tty console mode
static qbool ttycon_on = qfalse;
static qint ttycon_hide = 0;

// some key codes that the terminal may be using, initialised on start up
static qint TTY_erase;
static qint TTY_eof;

static struct termios TTY_tc;

static field_t TTY_con;

// This is somewhat of aduplicate of the graphical console history
// but it's safer more modular to have our own here
#define CON_HISTORY 32
static field_t ttyEditLines[ CON_HISTORY ];
static qint hist_current = -1, hist_count = 0;

/*
==================
CON_FlushIn

Flush stdin, I suspect some terminals are sending a LOT of shit
FIXME relevant?
==================
*/
static void CON_FlushIn( void )
{
	qchar key;
	while (read(STDIN_FILENO, &key, 1)!=-1);
}

/*
==================
CON_Back

Output a backspace

NOTE: it seems on some terminals just sending '\b' is not enough so instead we
send "\b \b"
(FIXME there may be a way to find out if '\b' alone would work though)
==================
*/
static void
CON_Back(void)
{
  qchar key;

  key = '\b';

  if (write(STDOUT_FILENO, &key, 1) == -1)
  {
    return;
  }

  key = ' ';

  if (write(STDOUT_FILENO, &key, 1) == -1)
  {
    return;
  }

  key = '\b';

  if (write(STDOUT_FILENO, &key, 1) == -1)
  {
    return;
  }
}

/*
==================
CON_Hide

Clear the display of the line currently edited
bring cursor back to beginning of line
==================
*/
static void CON_Hide( void )
{
	if( ttycon_on )
	{
		qint i;
		if (ttycon_hide)
		{
			ttycon_hide++;
			return;
		}
		if (TTY_con.cursor>0)
		{
			for (i=0; i<TTY_con.cursor; i++)
			{
				CON_Back();
			}
		}
		CON_Back(); // Delete "]"
		ttycon_hide++;
	}
}

/*
==================
CON_Show

Show the current line
FIXME need to position the cursor if needed?
==================
*/
static void
CON_Show(void)
{
  qint i;
  size_t result;
  qchar message[64];

  if (ttycon_on)
  {
    assert(ttycon_hide > 0);
    ttycon_hide--;

    if (ttycon_hide == 0)
    {
      result = write(STDOUT_FILENO, "]", 1);

      if (result == -1)
      {
        perror("write failed for ']'");
      }

      if (TTY_con.cursor)
      {
        for(i = 0;i < TTY_con.cursor;i++)
        {
          result = write(STDOUT_FILENO, TTY_con.buffer + i, 1);

          if (result == -1)
          {
            snprintf(message, sizeof(message), "write failed at index %d", i);
            perror(message);
            break;
          }
        }
      }
    }
  }
}

/*
==================
CON_Shutdown

Never exit without calling this, or your terminal will be left in a pretty bad state
==================
*/
void CON_Shutdown( void )
{
	if (ttycon_on)
	{
		CON_Back(); // Delete "]"
		tcsetattr (STDIN_FILENO, TCSADRAIN, &TTY_tc);
	}

  // Restore blocking to stdin reads
  fcntl( STDIN_FILENO, F_SETFL, fcntl( STDIN_FILENO, F_GETFL, 0 ) & ~O_NONBLOCK );
}

/*
==================
Hist_Add
==================
*/
void Hist_Add(field_t *field)
{
	qint i;
	assert(hist_count <= CON_HISTORY);
	assert(hist_count >= 0);
	assert(hist_current >= -1);
	assert(hist_current <= hist_count);
	// make some room
	for (i=CON_HISTORY-1; i>0; i--)
	{
		ttyEditLines[i] = ttyEditLines[i-1];
	}
	ttyEditLines[0] = *field;
	if (hist_count<CON_HISTORY)
	{
		hist_count++;
	}
	hist_current = -1; // re-init
}

/*
==================
Hist_Prev
==================
*/
field_t *Hist_Prev( void )
{
	qint hist_prev;
	assert(hist_count <= CON_HISTORY);
	assert(hist_count >= 0);
	assert(hist_current >= -1);
	assert(hist_current <= hist_count);
	hist_prev = hist_current + 1;
	if (hist_prev >= hist_count)
	{
		return NULL;
	}
	hist_current++;
	return &(ttyEditLines[hist_current]);
}

/*
==================
Hist_Next
==================
*/
field_t *Hist_Next( void )
{
	assert(hist_count <= CON_HISTORY);
	assert(hist_count >= 0);
	assert(hist_current >= -1);
	assert(hist_current <= hist_count);
	if (hist_current >= 0)
	{
		hist_current--;
	}
	if (hist_current == -1)
	{
		return NULL;
	}
	return &(ttyEditLines[hist_current]);
}

/*
==================
CON_SigCont

Reinitialize console input after receiving SIGCONT, as on Linux the terminal seems to lose all
set attributes if user did CTRL+Z and then does fg again.
==================
*/
void
CON_SigCont(qint signum)
{
  CON_Init();
}

/*
==================
CON_Init

Initialize the console input (tty mode if possible)
==================
*/
void CON_Init( void )
{
	struct termios tc;

	// If the process is backgrounded (running non interactively)
	// then SIGTTIN or SIGTOU is emitted, if not caught, turns into a SIGSTP
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

        //if SIGCONT is received, reinitialize console
        signal(SIGCONT, CON_SigCont);

	// Make stdin reads non-blocking
	fcntl( STDIN_FILENO, F_SETFL, fcntl( STDIN_FILENO, F_GETFL, 0 ) | O_NONBLOCK );

	if (!stdinIsATTY)
	{
		Com_Printf( "tty console mode disabled\n");
		ttycon_on = qfalse;
		stdin_active = qtrue;
		return;
	}

	Field_Clear(&TTY_con);
	tcgetattr (STDIN_FILENO, &TTY_tc);
	TTY_erase = TTY_tc.c_cc[VERASE];
	TTY_eof = TTY_tc.c_cc[VEOF];
	tc = TTY_tc;

	/*
	ECHO: don't echo input characters
	ICANON: enable canonical mode.  This  enables  the  special
	characters  EOF,  EOL,  EOL2, ERASE, KILL, REPRINT,
	STATUS, and WERASE, and buffers by lines.
	ISIG: when any of the characters  INTR,  QUIT,  SUSP,  or
	DSUSP are received, generate the corresponding sig­
	nal
	*/
	tc.c_lflag &= ~(ECHO | ICANON);

	/*
	ISTRIP strip off bit 8
	INPCK enable input parity checking
	*/
	tc.c_iflag &= ~(ISTRIP | INPCK);
	tc.c_cc[VMIN] = 1;
	tc.c_cc[VTIME] = 0;
	tcsetattr (STDIN_FILENO, TCSADRAIN, &tc);
	ttycon_on = qtrue;
}

/*
==================
CON_Input
==================
*/
qchar *
CON_Input(void)
{
  //use when sending back commands
  static qchar text[MAX_EDIT_LINE];
  size_t wret; //for write return value
  qint avail;
  qchar key;
  field_t *history;
  qint len;
  fd_set fdset;
  struct timeval timeout;

  if (ttycon_on)
  {
    avail = read(STDIN_FILENO, &key, 1);

    if (avail != -1)
    {
      //we have something .. backspace?
      //NOTE: TTimo testing a lot of values .. seems it's the only way to get it to work everywhere

      if ((key == TTY_erase) || (key == 127) || (key == 8))
      {
        if (TTY_con.cursor > 0)
        {
          TTY_con.cursor--;
          TTY_con.buffer[TTY_con.cursor] = '\0';
          CON_Back();
        }

        return NULL;
      }

      //check if control qchar
      if ((key) && (key) < ' ')
      {
        if (key == '\n')
        {
          //push it in history
          Hist_Add(&TTY_con);
          Q_strncpyz(text, TTY_con.buffer, sizeof(text));
          Field_Clear(&TTY_con);
          key = '\n';
          wret = write(1, &key, 1);

          if (wret == -1)
          {
            Com_DPrintf("failed to write newline to console: %s\n", strerror(errno));
          }

          wret = write(1, "]", 1);

          if (wret == -1)
          {
            Com_DPrintf("failed to write ']' to console: %s\n", strerror(errno));
          }

          return text;
        }

        if (key == '\t')
        {
          CON_Hide();
          Field_AutoComplete(&TTY_con);
          CON_Show();
          return NULL;
        }

        avail = read(STDIN_FILENO, &key, 1);

        if (avail != -1)
        {
          //vt 100 keys
          if (key == '[' || key == 'O')
          {
            avail = read(STDIN_FILENO, &key, 1);

            if (avail != -1)
            {
              switch(key)
              {
                case
                'A':
                  history = Hist_Prev();

                  if (history)
                  {
                    CON_Hide();
                    TTY_con = *history;
                    CON_Show();
                  }

                  CON_FlushIn();
                  return NULL;
                  break;

                case
                'B':
                  history = Hist_Next();
                  CON_Hide();

                  if (history)
                  {
                    TTY_con = *history;
                  }
                  else
                  {
                    Field_Clear(&TTY_con);
                  }

                  CON_Show();
                  CON_FlushIn();
                  return NULL;
                  break;

                case
                'C':
                  return NULL;

                case
                'D':
                  return NULL;
              }
            }
          }
        }

        Com_DPrintf("droping isctl sequence: %d, tty_erase: %d\n", key, TTY_erase);
        CON_FlushIn();
        return NULL;
      }

      if (TTY_con.cursor >= sizeof(text) - 1)
      {
        return NULL;
      }

      //push regular character
      TTY_con.buffer[TTY_con.cursor] = key;
      TTY_con.cursor++;
      //print the current line (this is differential)
      wret = write(STDOUT_FILENO, &key, 1);

      if (wret == -1)
      {
        Com_DPrintf("failed to write character to console: %s\n", strerror(errno));
      }
    }

    return NULL;
  }
  else if (stdin_active)
  {
    FD_ZERO(&fdset);
    FD_SET(STDIN_FILENO, &fdset); //stdin
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    if (select (STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(STDIN_FILENO, &fdset))
    {
      return NULL;
    }

    len = read (STDIN_FILENO, text, sizeof(text));

    if (len == 0)
    {
      stdin_active = qfalse;
      return NULL; //eof!
    }

    if (len < 1)
    {
      return NULL;
    }

    text[len-1] = 0; //rip off the /n and terminate
    return text;
  }

  return NULL;
}

/*
==================
CON_Print
==================
*/
void CON_Print( const qchar *msg )
{
	CON_Hide( );

	if( com_ansiColor && com_ansiColor->integer )
		Sys_AnsiColorPrint( msg );
	else
		fputs( msg, stderr );

	CON_Show( );
}
