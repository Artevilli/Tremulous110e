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

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

cvar_t *cl_shownet;

void CL_Shutdown( const qchar *finalmsg ) {
}

void CL_Init( void ) {
	cl_shownet = Cvar_Get ("cl_shownet", "0", CVAR_TEMP );
}

void CL_MouseEvent( qint dx, qint dy, qint time ) {
}

void Key_WriteBindings( fileHandle_t f ) {
}

void CL_Frame ( qint msec ) {
}

void CL_PacketEvent( const netadr_t *from, msg_t *msg ) {
}

void CL_CharEvent( qint key ) {
}

void CL_Disconnect( qbool showMainMenu ) {
}

void CL_MapLoading( void ) {
}

qbool CL_GameCommand( void ) {
  return qfalse;
}

void CL_KeyEvent (qint key, qbool down, unsigned time) {
}

qbool UI_GameCommand( void ) {
	return qfalse;
}

void CL_ForwardCommandToServer( const qchar *string ) {
}

void CL_ConsolePrint( qchar *txt ) {
}

void CL_JoystickEvent( qint axis, qint value, qint time ) {
}

void CL_InitKeyCommands( void ) {
}

void CL_CDDialog( void ) {
}

void CL_FlushMemory( void ) {
}

void CL_StartHunkUsers( qbool rendererOnly ) {
}

void
CL_Snd_Restart(void)
{
}

void CL_ShutdownAll(void) {}
