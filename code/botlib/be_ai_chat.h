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
//
/*****************************************************************************
 * name:		be_ai_chat.h
 *
 * desc:		qchar AI
 *
 * $Archive: /source/code/botlib/be_ai_chat.h $
 *
 *****************************************************************************/

#define MAX_MESSAGE_SIZE		256
#define MAX_CHATTYPE_NAME		32
#define MAX_MATCHVARIABLES		8

#define CHAT_GENDERLESS			0
#define CHAT_GENDERFEMALE		1
#define CHAT_GENDERMALE			2

#define CHAT_ALL					0
#define CHAT_TEAM					1
#define CHAT_TELL					2

//a console message
typedef struct bot_consolemessage_s
{
	struct bot_consolemessage_s *prev, *next;	//prev and next in list
	qchar message[MAX_MESSAGE_SIZE];				//message
	float time;									//message time
	qint type;									//message type
	qint handle;
} bot_consolemessage_t;

//a console message, fixed layout, exported to the QVM
typedef struct bot_consolemessage_qvm_s
{
	qint handle;
	float time;									//message time
	qint type;									//message type
	qchar message[MAX_MESSAGE_SIZE];				//message
	qint prev;									//non-portable/unused
	qint next;									//non-portable/unused
} bot_consolemessage_qvm_t;

//match variable
typedef struct bot_matchvariable_s
{
	signed qchar offset;
	qint length;
} bot_matchvariable_t;
//returned to AI when a match is found
typedef struct bot_match_s
{
	qchar string[MAX_MESSAGE_SIZE];
	qint type;
	qint subtype;
	bot_matchvariable_t variables[MAX_MATCHVARIABLES];
} bot_match_t;

//setup the chat AI
qint BotSetupChatAI(void);
//shutdown the chat AI
void BotShutdownChatAI(void);
//returns the handle to a newly allocated chat state
qint BotAllocChatState(void);
//frees the chatstate
void BotFreeChatState(qint handle);
//adds a console message to the chat state
void BotQueueConsoleMessage(qint chatstate, qint type, const qchar *message);
//removes the console message from the chat state
void BotRemoveConsoleMessage(qint chatstate, qint handle);
//returns the next console message from the state
qint BotNextConsoleMessage(qint chatstate, struct bot_consolemessage_qvm_s *cm);
//returns the number of console messages currently stored in the state
qint BotNumConsoleMessages(qint chatstate);
//selects a chat message of the given type
void BotInitialChat(qint chatstate, const qchar *type, qint mcontext, const qchar *var0, const qchar *var1, const qchar *var2, const qchar *var3, const qchar *var4, const qchar *var5, const qchar *var6, const qchar *var7);
//returns the number of initial chat messages of the given type
qint BotNumInitialChats(qint chatstate, const qchar *type);
//find and select a reply for the given message
qint BotReplyChat(qint chatstate, const qchar *message, qint mcontext, qint vcontext, const qchar *var0, const qchar *var1, const qchar *var2, const qchar *var3, const qchar *var4, const qchar *var5, const qchar *var6, const qchar *var7);
//returns the length of the currently selected chat message
qint BotChatLength(qint chatstate);
//enters the selected chat message
void BotEnterChat(qint chatstate, qint clientto, qint sendto);
//get the chat message ready to be output
void BotGetChatMessage(qint chatstate, qchar *buf, qint size);
//checks if the first string contains the second one, returns index into first string or -1 if not found
qint StringContains(const qchar *str1, const qchar *str2, qint casesensitive);
//finds a match for the given string using the match templates
qint BotFindMatch(const qchar *str, bot_match_t *match, unsigned long qint context);
//returns a variable from a match
void BotMatchVariable(bot_match_t *match, qint variable, qchar *buf, qint size);
//unify all the white spaces in the string
void UnifyWhiteSpaces(qchar *string);
//replace all the context related synonyms in the string
void BotReplaceSynonyms(qchar *string, qint size, unsigned long qint context);
//loads a chat file for the chat state
qint BotLoadChatFile(qint chatstate, const qchar *chatfile, const qchar *chatname);
//store the gender of the bot in the chat state
void BotSetChatGender(qint chatstate, qint gender);
//store the bot name in the chat state
void BotSetChatName(qint chatstate, const qchar *name, qint client);

