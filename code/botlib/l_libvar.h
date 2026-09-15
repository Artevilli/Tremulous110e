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
 * name:		l_libvar.h
 *
 * desc:		botlib vars
 *
 * $Archive: /source/code/botlib/l_libvar.h $
 *
 *****************************************************************************/

//library variable
typedef struct libvar_s
{
	qchar		*name;
	qchar		*string;
	qint		flags;
	qbool	modified;	// set each time the cvar is changed
	float		value;
	struct	libvar_s *next;
} libvar_t;

//removes all library variables
void LibVarDeAllocAll(void);
//gets the library variable with the given name
libvar_t *LibVarGet( const qchar *var_name );
//gets the string of the library variable with the given name
const qchar *LibVarGetString( const qchar *var_name );
//gets the value of the library variable with the given name
float LibVarGetValue( const qchar *var_name );
//creates the library variable if not existing already and returns it
libvar_t *LibVar( const qchar *var_name, const qchar *value );
//creates the library variable if not existing already and returns the value
float LibVarValue( const qchar *var_name, const qchar *value );
//creates the library variable if not existing already and returns the integer value
qint LibVarInteger( const qchar *var_name, const qchar *value, qint min_v, qint max_v );
//creates the library variable if not existing already and returns the value string
const qchar *LibVarString( const qchar *var_name, const qchar *value );
//sets the library variable
void LibVarSet( const qchar *var_name, const qchar *value );
#if 0
//returns true if the library variable has been modified
qbool LibVarChanged( const qchar *var_name );
//sets the library variable to unmodified
void LibVarSetNotModified( const qchar *var_name );
#endif
