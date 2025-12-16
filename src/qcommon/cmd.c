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
// cmd.c -- Quake script command processing module

#include "q_shared.h"
#include "qcommon.h"

#define	MAX_CMD_BUFFER	65536
#define	MAX_CMD_LINE	1024

typedef struct {
	byte	*data;
	qint		maxsize;
	qint		cursize;
} cmd_t;

qint			cmd_wait;
cmd_t		cmd_text;
byte		cmd_text_buf[MAX_CMD_BUFFER];


//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "cmd use rocket ; +attack ; wait ; -attack ; cmd use blaster"
============
*/
void
Cmd_Wait_f(void)
{
  if (Cmd_Argc() == 2)
  {
    cmd_wait = Q_atoi(Cmd_Argv(1));

    if (cmd_wait < 0)
    {
      cmd_wait = 1; //ignore the argument
    }
  }
  else
  {
    cmd_wait = 1;
  }
}


/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

/*
============
Cbuf_Init
============
*/
void
Cbuf_Init(void)
{
  cmd_text.data = cmd_text_buf;
  cmd_text.maxsize = MAX_CMD_BUFFER;
  cmd_text.cursize = 0;
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer, does NOT add a final \n
============
*/
void
Cbuf_AddText(const qchar *text)
{
  const qint l = (qint)strlen(text);

  if (cmd_text.cursize + l >= cmd_text.maxsize)
  {
    Com_Printf("Cbuf_AddText: overflow\n");
    return;
  }

  Com_Memcpy(&cmd_text.data[cmd_text.cursize], text, l);
  cmd_text.cursize += l;
}

static qint nestedCmdOffset;

void
Cbuf_NestedReset(void)
{
  nestedCmdOffset = 0;
}

/*
============
Cbuf_NestedAdd

//Adds command text at the specified position of the buffer, adds \n when needed
============
*/
void
Cbuf_NestedAdd(const qchar *text)
{
  qint len = (qint)strlen(text);
  qint pos = nestedCmdOffset;
  qbool separate = qfalse;
  qint i;

  if (len <= 0)
  {
    nestedCmdOffset = cmd_text.cursize;
    return;
  }

#if 0
  if (cmd_text.cursize > 0)
  {
    const qint c = cmd_text.data[cmd_text.cursize - 1];

    //insert separator for already existing command(s)
    if (c != '\n' && c != ';' && text[0] != '\n' && text[0] != ';')
    {
      if (cmd_text.cursize < cmd_text.maxsize)
      {
        cmd_text.data[cmd_text.cursize] = ';';
        cmd_text.cursize++;
      }
      else
      {
        Com_Printf(S_COLOR_YELLOW "%s(%i) overflowed\n", __func__, pos);
        nestedCmdOffset = cmd_text.cursize;
        return;
      }
    }
  }
#endif

  if (pos > cmd_text.cursize || pos < 0)
  {
    pos = cmd_text.cursize;
  }

  if (text[len - 1] == '\n' || text[len - 1] == ';')
  {
    //command already has separator
  }
  else
  {
    separate = qtrue;
    len += 1;
  }

  if (len + cmd_text.cursize > cmd_text.maxsize)
  {
    Com_Printf(S_COLOR_YELLOW "%s(%i) overflowed\n", __func__, pos);
    nestedCmdOffset = cmd_text.cursize;
    return;
  }

  //move the existing command text
  for(i = cmd_text.cursize - 1;i >= pos;i--)
  {
    cmd_text.data[i + len] = cmd_text.data[i];
  }

  if (separate)
  {
    //copy the new text in + add a \n
    Com_Memcpy(cmd_text.data + pos, text, len - 1);
    cmd_text.data[pos + len - 1] = '\n';
  }
  else
  {
    //copy the new text in
    Com_Memcpy(cmd_text.data + pos, text, len);
  }

  cmd_text.cursize += len;
  nestedCmdOffset = cmd_text.cursize;
}

/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
============
*/
void
Cbuf_InsertText(const qchar *text)
{
  const qint len = (qint)(strlen(text) + 1);
  qint i;

  if (len + cmd_text.cursize > cmd_text.maxsize)
  {
    Com_Printf("Cbuf_InsertText overflowed\n");
    return;
  }

  //move the existing command text
  for(i = cmd_text.cursize - 1;i >= 0;i--)
  {
    cmd_text.data[i + len] = cmd_text.data[i];
  }

  //copy the new text in
  Com_Memcpy(cmd_text.data, text, len - 1);

  //add a \n
  cmd_text.data[len - 1] = '\n';

  cmd_text.cursize += len;
}

/*
============
Cbuf_ExecuteText
============
*/
void
Cbuf_ExecuteText(qint exec_when, const qchar *text)
{
  switch(exec_when)
  {
    case
    EXEC_NOW:
      cmd_wait = 0; //discard any pending waiting

      if (text && text[0] != '\0')
      {
        Com_DPrintf(S_COLOR_YELLOW "EXEC_NOW %s\n", text);
        Cmd_ExecuteString(text);
      }
      else
      {
        Cbuf_Execute();
        Com_DPrintf(S_COLOR_YELLOW "EXEC_NOW %s\n", cmd_text.data);
      }

      break;

    case
    EXEC_INSERT:
      Cbuf_InsertText(text);
      break;

    case
    EXEC_APPEND:
      Cbuf_AddText(text);
      break;

    default:
      Com_Error(ERR_FATAL, "Cbuf_ExecuteText: bad exec_when");
  }
}

/*
============
Cbuf_Execute
============
*/
void
Cbuf_Execute(void)
{
  qchar line[MAX_CMD_LINE];
  qchar *text;
  qint i;
  qint n;
  qint quotes;
  qbool in_star_comment;
  qbool in_slash_comment;

  if (cmd_wait > 0)
  {
    //delay command buffer execution
    return;
  }

  //this will keep // style comments all on one line by not breaking on
  //a semicolon, it will keep /* ... */ style comments all on one line by not
  //breaking it for semicolon or newline
  in_star_comment = qfalse;
  in_slash_comment = qfalse;

  while(cmd_text.cursize > 0)
  {
    //find a \n or ; line break or comment: // or /* */
    text = (qchar *)cmd_text.data;

    quotes = 0;

    for(i = 0;i < cmd_text.cursize;i++)
    {
      if (text[i] == '"')
      {
        quotes++;
      }

      if (!(quotes & 1))
      {
        if (i < cmd_text.cursize - 1)
        {
          if (!in_star_comment && text[i] == '/' && text[i + 1] == '/')
          {
            in_slash_comment = qtrue;
          }
          else if (!in_slash_comment && text[i] == '/' && text[i + 1] == '*')
          {
            in_star_comment = qtrue;
          }
          else if (in_star_comment && text[i] == '*' && text[i + 1] == '/')
          {
            in_star_comment = qfalse;
            //if we are in a star comment, then the part after it is valid
            //NOTE: this will cause it to NUL out the terminating '/'
            //but ExecuteString doesn't require it anyway
            i++;
            break;
          }
        }

        if (!in_slash_comment && !in_star_comment && text[i] == ';')
        {
          break;
        }
      }

      if (!in_star_comment && (text[i] == '\n' || text[i] == '\r'))
      {
        in_slash_comment = qfalse;
        break;
      }
    }

    //copy up to (MAX_CMD_LINE - 1) chars but keep buffer position intact to prevent parsing truncated leftover
    if (i > (MAX_CMD_LINE - 1))
    {
      n = MAX_CMD_LINE - 1;
    }
    else
    {
      n = i;
    }

    Com_Memcpy(line, text, n);
    line[n] = '\0';

    //delete the text from the command buffer and move remaining commands down
    //this is necessary because commands (exec) can insert data at the
    //beginning of the text buffer

    if (i == cmd_text.cursize)
    {
      //cmd_text.cursize = 0;
    }
    else
    {
      ++i;

      //skip all repeating newlines/semicolons/whitespaces
      while(i < cmd_text.cursize && (text[i] == '\n' || text[i] == '\r' || text[i] == ';' || (text[i] != '\0' && text[i] <= ' ')))
      {
        ++i;
      }
    }

    cmd_text.cursize -= i;

    if (cmd_text.cursize)
    {
      memmove(text, text + i, cmd_text.cursize);
    }

    if (nestedCmdOffset > 0)
    {
      nestedCmdOffset -= i;

      if (nestedCmdOffset < 0)
      {
        nestedCmdOffset = 0;
      }
    }

    //execute the command line
    Cmd_ExecuteString(line);

    //break on wait command
    if (cmd_wait > 0)
    {
      break;
    }
  }
}

/*
============
Cbuf_Wait
============
*/
void
Cbuf_Wait(void)
{
  if (cmd_wait > 0)
  {
    --cmd_wait;
  }
}

/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/

/*
===============
Cmd_Exec_f
===============
*/
void
Cmd_Exec_f(void)
{
  union
  {
    qchar *c;
    void *v;
  }
  f;
  qchar filename[MAX_QPATH];

  if (Cmd_Argc() != 2)
  {
    Com_Printf("exec <filename> : execute a script file\n");
    return;
  }

  Q_strncpyz(filename, Cmd_Argv(1), sizeof(filename));
  COM_DefaultExtension(filename, sizeof(filename), ".cfg");
  FS_BypassPure();
  FS_ReadFile(filename, &f.v);
  FS_RestorePure();

  if (!f.c)
  {
    Com_Printf("couldn't exec %s\n", Cmd_Argv(1));
    return;
  }

  Com_Printf("ececing %s\n", Cmd_Argv(1));
  Cbuf_InsertText(f.c);

#if defined(DELAY_WRITECONFIG)
  if (!Q_stricmp(filename, Q3CONFIG_CFG))
  {
    Com_WriteConfiguration(); //to avoid loading outdated values
  }
#endif

  FS_FreeFile(f.v);
}

/*
===============
Cmd_Vstr_f

Inserts the current value of a variable as command text
===============
*/
void
Cmd_Vstr_f(void)
{
  qchar *v;

  if (Cmd_Argc() != 2)
  {
    Com_Printf("vstr <variablename> : execute a variable command\n");
    return;
  }

  v = Cvar_VariableString(Cmd_Argv(1));
  Cbuf_InsertText(va(NULL, "%s\n", v));
}

/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void
Cmd_Echo_f(void)
{
  Com_Printf("%s\n", Cmd_Args());
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

typedef struct
cmd_function_s
{
  struct cmd_function_s *next;
  qchar *name;
  xcommand_t function;
  completionFunc_t complete;
}
cmd_function_t;

static qint cmd_argc;
static qchar *cmd_argv[MAX_STRING_TOKENS]; //points to cmd_tokenized
static qchar cmd_tokenized[BIG_INFO_STRING + MAX_STRING_TOKENS]; //will have 0 bytes inserted
static qchar cmd_cmd[BIG_INFO_STRING]; //the original command we received (no token processing)

static cmd_function_t *cmd_functions; //possible commands to execute

/*
============
Cmd_Argc
============
*/
qint
Cmd_Argc(void)
{
  return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
qchar *
Cmd_Argv(qint arg)
{
  if ((unsigned)arg >= cmd_argc)
  {
    return "";
  }

  return cmd_argv[arg];	
}

/*
============
Cmd_ArgvBuffer

The interpreted versions use this because
they can't have pointers returned to them
============
*/
void
Cmd_ArgvBuffer(qint arg, qchar *buffer, qint bufferLength)
{
  Q_strncpyz(buffer, Cmd_Argv(arg), bufferLength);
}

/*
============
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
============
*/
qchar *
Cmd_Args(void)
{
  static qchar cmd_args[MAX_STRING_CHARS];
  qint i;

  cmd_args[0] = '\0';

  for(i = 1;i < cmd_argc;i++)
  {
    strcat(cmd_args, cmd_argv[i]);

    if (i != cmd_argc - 1)
    {
      strcat(cmd_args, " ");
    }
  }

  return cmd_args;
}

/*
============
Cmd_Args

Returns a single string containing argv(arg) to argv(argc()-1)
============
*/
qchar *
Cmd_ArgsFrom(qint arg)
{
  static qchar cmd_args[BIG_INFO_STRING];
  qint i;

  cmd_args[0] = '\0';

  if (arg < 0)
  {
    arg = 0;
  }

  for(i = arg;i < cmd_argc;i++)
  {
    strcat(cmd_args, cmd_argv[i]);

    if (i != cmd_argc-1)
    {
      strcat(cmd_args, " ");
    }
  }

  return cmd_args;
}

/*
============
Cmd_ArgsBuffer

The interpreted versions use this because
they can't have pointers returned to them
============
*/
void
Cmd_ArgsBuffer(qchar *buffer, qint bufferLength)
{
  Q_strncpyz(buffer, Cmd_Args(), bufferLength);
}

/*
  Description: Replace strip[x] in string with repl[x] or remove characters entirely
  Mutates: string
  Return: --
  Examples: Q_strstrip("Bo\nb is h\rairy!!", "\n\r!", "123"); //"Bo1b is h2airy33"
  Examples: Q_strstrip("Bo\nb is h\rairy!!", "\n\r!", "12"); //"Bo1b is h2airy"
  Examples: Q_strstrip("Bo\nb is h\rairy!!", "\n\r!", NULL);	// "Bob is hairy"
*/
void
Q_strstrip(qchar *string, const qchar *strip, const qchar *repl)
{
  qchar *out = string;
  qchar *p = string;
  qchar c;
  const qchar *s = strip;
  const qint replaceLen = repl ? strlen(repl):0;
  qint offset = 0;
  qbool recordChar = qtrue;

  while((c = *p++) != '\0')
  {
    recordChar = qtrue;

    for(s = strip;*s;s++)
    {
      offset = s - strip;

      if (c == *s)
      {
        if (!repl || offset >= replaceLen)
        {
          recordChar = qfalse;
        }
        else
        {
          c = repl[offset];
        }

        break;
      }
    }

    if (recordChar)
    {
      *out++ = c;
    }
  }

  *out = '\0';
}

/*
============
Cmd_Cmd

Retrieve the unmodified command string
For rcon use when you want to transmit without altering quoting
https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=543
============
*/
qchar *
Cmd_Cmd(void)
{
  return cmd_cmd;
}

/*
  Replace command separators with space to prevent interpretation
  This is a hack to protect buggy qvms
  https://bugzilla.icculus.org/show_bug.cgi?id=3593
  https://bugzilla.icculus.org/show_bug.cgi?id=4769
*/
void
Cmd_Args_Sanitize(void)
{
  qint i;

  for(i = 1;i < cmd_argc;i++)
  {
    qchar *c = cmd_argv[i];

    if (strlen(c) > MAX_CVAR_VALUE_STRING - 1)
    {
      c[MAX_CVAR_VALUE_STRING - 1] = '\0';
    }

    while((c = strpbrk(c, "\n\r;")) != NULL)
    {
      *c = ' ';
      ++c;
    }
  }
}

/*
  Replace command separators with space to prevent interpretation
  This is a hack to protect buggy qvms
  https://bugzilla.icculus.org/show_bug.cgi?id=4769
*/
void
Cmd_Args_Sanitize2(size_t length, const qchar *strip, const qchar *repl)
{
  qint i;

  for(i = 1;i < cmd_argc;i++)
  {
    qchar *c = cmd_argv[i];

    if (length > 0 && strlen(c) >= length)
    {
      c[length - 1] = '\0';
    }

    if (VALIDSTRING(strip) && VALIDSTRING(repl))
    {
      Q_strstrip(c, strip, repl);
    }
  }
}

/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
The text is copied to a seperate buffer and 0 characters
are inserted in the apropriate place, The argv array
will point into this temporary buffer.
============
*/
// NOTE TTimo define that to track tokenization issues
//#define TKN_DBG
static void
Cmd_TokenizeString2(const qchar *text_in, qbool ignoreQuotes)
{
  const qchar *text;
  qchar *textOut;

#ifdef TKN_DBG
  //FIXME: TTimo blunt hook to try to find the tokenization of userinfo
  Com_DPrintf("Cmd_TokenizeString: %s\n", text_in);
#endif

  //clear previous args
  cmd_argc = 0;

  if (!text_in)
  {
    return;
  }

  Q_strncpyz(cmd_cmd, text_in, sizeof(cmd_cmd));
  text = text_in;
  textOut = cmd_tokenized;

  while(1)
  {
    if (cmd_argc == MAX_STRING_TOKENS)
    {
      return; //this is usually something malicious
    }

    while(1)
    {
      //skip whitespace
      while(*text && *text <= ' ')
      {
        text++;
      }

      if (!*text)
      {
        return; //all tokens parsed
      }

      //skip // comments
      if (text[0] == '/' && text[1] == '/')
      {
        return; //all tokens parsed
      }

      //skip /* */ comments
      if (text[0] == '/' && text[1] == '*')
      {
        while(*text && (text[0] != '*' || text[1] != '/'))
        {
          text++;
        }

        if (!*text)
        {
          return; //all tokens parsed
        }

        text += 2;
      }
      else
      {
        break; //we are ready to parse a token
      }
    }

    //handle quoted strings
    //NOTE: TTimo this doesn't handle \" escaping
    if (!ignoreQuotes && *text == '"')
    {
      cmd_argv[cmd_argc] = textOut;
      cmd_argc++;
      text++;

      while(*text && *text != '"')
      {
        *textOut++ = *text++;
      }

      *textOut++ = '\0';

      if (!*text)
      {
        return; //all tokens parsed
      }

      text++;
      continue;
    }

    //regular token
    cmd_argv[cmd_argc] = textOut;
    cmd_argc++;

    //skip until whitespace, quote, or command
    while(*text > ' ')
    {
      if (!ignoreQuotes && text[0] == '"')
      {
        break;
      }

      if (text[0] == '/' && text[1] == '/')
      {
        break;
      }

      //skip /* */ comments
      if (text[0] == '/' && text[1] == '*')
      {
        break;
      }

      *textOut++ = *text++;
    }

    *textOut++ = '\0';

    if (!*text)
    {
      return; //all tokens parsed
    }
  }
}

/*
============
Cmd_TokenizeString
============
*/
void
Cmd_TokenizeString(const qchar *text_in)
{
  Cmd_TokenizeString2(text_in, qfalse);
}

/*
============
Cmd_TokenizeStringIgnoreQuotes
============
*/
void
Cmd_TokenizeStringIgnoreQuotes(const qchar *text_in)
{
  Cmd_TokenizeString2(text_in, qtrue);
}

/*
============
Cmd_FindCommand
============
*/
static cmd_function_t *
Cmd_FindCommand(const qchar *cmd_name)
{
  cmd_function_t *cmd;

  for(cmd = cmd_functions;cmd;cmd = cmd->next)
  {
    if (!Q_stricmp(cmd_name, cmd->name))
    {
      return cmd;
    }
  }

  return NULL;
}

/*
============
Cmd_AddCommand
============
*/
void
Cmd_AddCommand(const qchar *cmd_name, xcommand_t function)
{
  cmd_function_t *cmd;
	
  //fail if the command already exists
  if (Cmd_FindCommand(cmd_name))
  {
    //allow completion-only commands to be silently doubled
    if (function != NULL)
    {
      Com_Printf("Cmd_AddCommand: %s already defined\n", cmd_name);
    }

    return;
  }

  // use a small malloc to avoid zone fragmentation
  cmd = S_Malloc(sizeof(cmd_function_t));
  cmd->name = CopyString(cmd_name);
  cmd->function = function;
  cmd->complete = NULL;
  cmd->next = cmd_functions;
  cmd_functions = cmd;
}

/*
============
Cmd_SetCommandCompletionFunc
============
*/
void
Cmd_SetCommandCompletionFunc(const qchar *command, completionFunc_t complete)
{
  cmd_function_t *cmd;

  for(cmd = cmd_functions;cmd;cmd = cmd->next)
  {
    if (!Q_stricmp(command, cmd->name))
    {
      cmd->complete = complete;
    }
  }
}

/*
============
Cmd_RemoveCommand
============
*/
void
Cmd_RemoveCommand(const qchar *cmd_name)
{
  cmd_function_t *cmd;
  cmd_function_t **back;

  back = &cmd_functions;

  while(1)
  {
    cmd = *back;

    if (!cmd)
    {
      //command wasn't active
      return;
    }

    if (!strcmp(cmd_name, cmd->name))
    {
      *back = cmd->next;

      if (cmd->name)
      {
        Z_Free(cmd->name);
      }

      Z_Free(cmd);
      return;
    }

    back = &cmd->next;
  }
}

/*
============
Cmd_RemoveCommandSafe

Only remove commands with no associated function
============
*/
void
Cmd_RemoveCommandSafe(const qchar *cmd_name)
{
  cmd_function_t *cmd = Cmd_FindCommand(cmd_name);

  if (!cmd)
  {
    return;
  }

  if (cmd->function)
  {
    Com_Error(ERR_DROP, "Restricted source tried to remove system command \"%s\"\n", cmd_name);
    return;
  }

  Cmd_RemoveCommand(cmd_name);
}

/*
============
Cmd_CommandCompletion
============
*/
void
Cmd_CommandCompletion(void (*callback)(const qchar *s))
{
  cmd_function_t *cmd;
	
  for(cmd = cmd_functions;cmd;cmd = cmd->next)
  {
    callback(cmd->name);
  }
}

/*
============
Cmd_CompleteArgument
============
*/
qbool
Cmd_CompleteArgument(const qchar *command, const qchar *args, qint argNum)
{
  const cmd_function_t *cmd;

  for(cmd = cmd_functions;cmd;cmd = cmd->next)
  {
    if (!Q_stricmp(command, cmd->name))
    {
      if (cmd->complete)
      {
        cmd->complete((qchar *)args, argNum);
      }

      return qtrue;
    }
  }

  return qfalse;
}

/*
============
Cmd_GetCommandFunction

Return the function registered for the command (may be NULL.
Return NULL if the command is not registered.
============
*/
xcommand_t
Cmd_GetCommandFunction(const qchar *cmdName)
{
  cmd_function_t *cmd;

  for(cmd = cmd_functions;cmd;cmd = cmd->next)
  {
    if (!strcmp(cmdName, cmd->name))
    {
      return cmd->function;
    }
  }

  return NULL;
}

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
============
*/
void
Cmd_ExecuteString(const qchar *text)
{
  cmd_function_t *cmd, **prev;

  //execute the command line
  Cmd_TokenizeString(text);

  if (!Cmd_Argc())
  {
    return; //no tokens
  }

  //check registered command functions   
  for(prev = &cmd_functions;*prev;prev = &cmd->next)
  {
    cmd = *prev;

    if (!Q_stricmp(cmd_argv[0], cmd->name))
    {
      //rearrange the links so that the command will be
      //near the head of the list next time it is used
      *prev = cmd->next;
      cmd->next = cmd_functions;
      cmd_functions = cmd;

      //perform the action
      if (!cmd->function)
      {
        //let the cgame or game handle it
        break;
      }
      else
      {
        cmd->function();
      }

      return;
    }
  }

  //check cvars
  if (Cvar_Command())
  {
    return;
  }
#if !defined(DEDICATED)
  //check client game commands
  if (com_cl_running && com_cl_running->integer && CL_GameCommand())
  {
    return;
  }
#endif
  //check server game commands
  if (com_sv_running && com_sv_running->integer && SV_GameCommand())
  {
    return;
  }
#if !defined(DEDICATED)
  //check ui commands
  if (com_cl_running && com_cl_running->integer && UI_GameCommand())
  {
    return;
  }

  //send it as a server command if we are connected
  //this will usually result in a chat message
  CL_ForwardCommandToServer(text);
#endif
}

/*
============
Cmd_List_f
============
*/
void
Cmd_List_f(void)
{
  const cmd_function_t *cmd;
  const qchar *match;
  qint i;

  if (Cmd_Argc() > 1)
  {
    match = Cmd_Argv(1);
  }
  else
  {
    match = NULL;
  }

  i = 0;

  for(cmd = cmd_functions;cmd;cmd = cmd->next)
  {
    if (match && !Com_Filter(match, cmd->name))
    {
      continue;
    }

    Com_Printf("%s\n", cmd->name);
    i++;
  }

  Com_Printf("%i commands\n", i);
}

/*
============
Cmd_CompleteCfgName
============
*/
static void
Cmd_CompleteCfgName(const qchar *args, qint argNum)
{
  if (argNum == 2)
  {
    Field_CompleteFilename("", "cfg", qfalse, FS_MATCH_ANY | FS_MATCH_STICK | FS_MATCH_SUBDIRS);
  }
}

/*
============
Cmd_CompleteWriteCfgName
============
*/
void
Cmd_CompleteWriteCfgName(const qchar *args, qint argNum)
{
  if (argNum == 2)
  {
    Field_CompleteFilename("", "cfg", qfalse, FS_MATCH_EXTERN | FS_MATCH_STICK);
  }
}

/*
============
Cmd_Init
============
*/
void
Cmd_Init(void)
{
  Cmd_AddCommand("cmdlist", Cmd_List_f);
  Cmd_AddCommand("exec", Cmd_Exec_f);
  Cmd_SetCommandCompletionFunc("exec", Cmd_CompleteCfgName);
  Cmd_AddCommand("vstr", Cmd_Vstr_f);
  Cmd_SetCommandCompletionFunc("vstr", Cvar_CompleteCvarName);
  Cmd_AddCommand("echo", Cmd_Echo_f);
  Cmd_AddCommand("wait", Cmd_Wait_f);
}
