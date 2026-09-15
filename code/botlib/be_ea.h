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
 * name:		be_ea.h
 *
 * desc:		elementary actions
 *
 * $Archive: /source/code/botlib/be_ea.h $
 *
 *****************************************************************************/

//ClientCommand elementary actions
void EA_Say(qint client, const qchar *str);
void EA_SayTeam(qint client, const qchar *str);
void EA_Command(qint client, const qchar *command );

void EA_Action(qint client, qint action);
void EA_Crouch(qint client);
void EA_Walk(qint client);
void EA_MoveUp(qint client);
void EA_MoveDown(qint client);
void EA_MoveForward(qint client);
void EA_MoveBack(qint client);
void EA_MoveLeft(qint client);
void EA_MoveRight(qint client);
void EA_Attack(qint client);
void EA_Respawn(qint client);
void EA_Talk(qint client);
void EA_Gesture(qint client);
void EA_Use(qint client);

//regular elementary actions
void EA_SelectWeapon(qint client, qint weapon);
void EA_Jump(qint client);
void EA_DelayedJump(qint client);
void EA_Move(qint client, vec3_t dir, float speed);
void EA_View(qint client, vec3_t viewangles);

//send regular input to the server
void EA_EndRegular(qint client, float thinktime);
void EA_GetInput(qint client, float thinktime, bot_input_t *input);
void EA_ResetInput(qint client);
//setup and shutdown routines
qint EA_Setup(void);
void EA_Shutdown(void);
