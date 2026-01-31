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
#include "keycodes.h"

typedef struct
{
  qbool down;
  qbool bound;
  qint repeats; //if > 1, it is autorepeating
  qchar *binding;
}
qkey_t;

extern qbool key_overstrikeMode;
extern qkey_t keys[MAX_KEYS];

extern qint anykeydown;

//NOTE TTimo the declaration of field_t and Field_Clear is now in qcommon/qcommon.h

void
Key_WriteBindings(fileHandle_t f);
void
Key_SetBinding(qint keynum, const qchar *binding);
const char *
Key_GetBinding(qint keynum);
void
Key_ParseBinding(qint key, qbool down, unsigned time);

qint
Key_GetKey(const qchar *binding);
const qchar *
Key_KeynumToString(qint keynum);
qint
Key_StringToKeynum(const qchar *str);

qbool
Key_IsDown(qint keynum);
void
Key_ClearStates(void);

qbool
Key_GetOverstrikeMode(void);
void
Key_SetOverstrikeMode(qbool state);

void
Com_InitKeyCommands(void);
