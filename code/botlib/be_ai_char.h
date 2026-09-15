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
 * name:		be_ai_char.h
 *
 * desc:		bot characters
 *
 * $Archive: /source/code/botlib/be_ai_char.h $
 *
 *****************************************************************************/

//loads a bot character from a file
qint BotLoadCharacter(const qchar *charfile, float skill);
//frees a bot character
void BotFreeCharacter(qint character);
//returns a float characteristic
float Characteristic_Float(qint character, qint index);
//returns a bounded float characteristic
float Characteristic_BFloat(qint character, qint index, float min, float max);
//returns an integer characteristic
qint Characteristic_Integer(qint character, qint index);
//returns a bounded integer characteristic
qint Characteristic_BInteger(qint character, qint index, qint min, qint max);
//returns a string characteristic
void Characteristic_String(qint character, qint index, qchar *buf, qint size);
//free cached bot characters
void BotShutdownCharacters(void);
