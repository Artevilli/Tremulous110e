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

/*****************************************************************************
 * name:		l_precomp.h
 *
 * desc:		pre compiler
 *
 * $Archive: /source/code/botlib/l_precomp.h $
 *
 *****************************************************************************/

#ifndef MAX_PATH
	#define MAX_PATH			MAX_QPATH
#endif

#ifndef PATH_SEPERATORSTR
	#if defined(WIN32)|defined(_WIN32)|defined(__NT__)|defined(__WINDOWS__)|defined(__WINDOWS_386__)
		#define PATHSEPERATOR_STR		"\\"
	#else
		#define PATHSEPERATOR_STR		"/"
	#endif
#endif
#ifndef PATH_SEPERATORCHAR
	#if defined(WIN32)|defined(_WIN32)|defined(__NT__)|defined(__WINDOWS__)|defined(__WINDOWS_386__)
		#define PATHSEPERATOR_CHAR		'\\'
	#else
		#define PATHSEPERATOR_CHAR		'/'
	#endif
#endif

#if defined(BSPC) && !defined(QDECL)
#define QDECL
#endif


#define DEFINE_FIXED			0x0001

#define BUILTIN_LINE			1
#define BUILTIN_FILE			2
#define BUILTIN_DATE			3
#define BUILTIN_TIME			4
#define BUILTIN_STDC			5

#define INDENT_IF				0x0001
#define INDENT_ELSE				0x0002
#define INDENT_ELIF				0x0004
#define INDENT_IFDEF			0x0008
#define INDENT_IFNDEF			0x0010

//macro definitions
typedef struct define_s
{
	qchar *name;							//define name
	qint flags;							//define flags
	qint builtin;						// > 0 if builtin define
	qint numparms;						//number of define parameters
	token_t *parms;						//define parameters
	token_t *tokens;					//macro tokens (possibly containing parm tokens)
	struct define_s *next;				//next defined macro in a list
	struct define_s *hashnext;			//next define in the hash chain
} define_t;

//indents
//used for conditional compilation directives:
//#if, #else, #elif, #ifdef, #ifndef
typedef struct indent_s
{
	qint type;								//indent type
	qint skip;								//true if skipping current indent
	script_t *script;						//script the indent was in
	struct indent_s *next;					//next indent on the indent stack
} indent_t;

//source file
typedef struct source_s
{
	qchar filename[1024];					//file name of the script
	qchar includepath[1024];					//path to include files
	punctuation_t *punctuations;			//punctuations to use
	script_t *scriptstack;					//stack with scripts of the source
	token_t *tokens;						//tokens to read first
	define_t *defines;						//list with macro definitions
	define_t **definehash;					//hash chain with defines
	indent_t *indentstack;					//stack with indents
	qint skip;								// > 0 if skipping conditional code
	token_t token;							//last read token
} source_t;


//read a token from the source
qint PC_ReadToken(source_t *source, token_t *token);
//read enumerations
qbool PC_ReadEnumeration(source_t *source);
//expect a certain token
qint PC_ExpectTokenString(source_t *source, qchar *string);
//expect a certain token type
qint PC_ExpectTokenType(source_t *source, qint type, qint subtype, token_t *token);
//expect a token
qint PC_ExpectAnyToken(source_t *source, token_t *token);
//returns true when the token is available
qint PC_CheckTokenString(source_t *source, qchar *string);
//unread the last token read from the script
void PC_UnreadLastToken(source_t *source);
//unread the given token
void PC_UnreadToken(source_t *source, token_t *token);
//add a globals define that will be added to all opened sources
qint PC_AddGlobalDefine(const qchar *string);
//remove all globals defines
void PC_RemoveAllGlobalDefines(void);
//adds a define to the source
qbool PC_AddDefineToSourceFromString(source_t *source, const qchar *string);
#if 0
//skip tokens until the given token string is read
qint PC_SkipUntilString(source_t *source, qchar *string);
//returns true and reads the token when a token with the given type is available
qint PC_CheckTokenType(source_t *source, qint type, qint subtype, token_t *token);
//remove the given global define
qint PC_RemoveGlobalDefine(qchar *name);
//add a define to the source
qint PC_AddDefine(source_t *source, qchar *string);
//add builtin defines
void PC_AddBuiltinDefines(source_t *source);
//set the source include path
void PC_SetIncludePath(source_t *source, const qchar *path);
//set the punction set
void PC_SetPunctuations(source_t *source, punctuation_t *p);
//load a source from memory
source_t *LoadSourceMemory(qchar *ptr, qint length, qchar *name);
#endif
//set the base folder to load files from
void PC_SetBaseFolder(const qchar *path);
//load a source file
source_t *LoadSourceFile(const qchar *filename);
//free the given source
void FreeSource(source_t *source);
//print a source error
void QDECL SourceError(source_t *source, const qchar *fmt, ...) __attribute__ ((format (printf, 2, 3)));
//print a source warning
void QDECL SourceWarning(source_t *source, const qchar *fmt, ...)  __attribute__ ((format (printf, 2, 3)));

#ifdef BSPC
// some of BSPC source does include game/q_shared.h and some does not
// we define pc_token_s pc_token_t if needed (yes, it's ugly)
#ifndef __Q_SHARED_H
#define MAX_TOKENLENGTH		1024
typedef struct pc_token_s
{
	qint type;
	qint subtype;
	qint intvalue;
	float floatvalue;
	qchar string[MAX_TOKENLENGTH];
} pc_token_t;
#endif //!_Q_SHARED_H
#endif //BSPC

//
qint PC_LoadSourceHandle(const qchar *filename);
qint PC_FreeSourceHandle(qint handle);
qint PC_ReadTokenHandle(qint handle, pc_token_t *pc_token);
qint PC_SourceFileAndLine(qint handle, qchar *filename, qint *line);
void PC_CheckOpenSourceHandles(void);
