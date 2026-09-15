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
 * name:		be_ai_weap.h
 *
 * desc:		weapon AI
 *
 * $Archive: /source/code/botlib/be_ai_weap.h $
 *
 *****************************************************************************/

//projectile flags
#define PFL_WINDOWDAMAGE			1		//projectile damages through window
#define PFL_RETURN					2		//set when projectile returns to owner
//weapon flags
#define WFL_FIRERELEASED			1		//set when projectile is fired with key-up event
//damage types
#define DAMAGETYPE_IMPACT			1		//damage on impact
#define DAMAGETYPE_RADIAL			2		//radial damage
#define DAMAGETYPE_VISIBLE			4		//damage to all entities visible to the projectile

typedef struct projectileinfo_s
{
	qchar name[MAX_STRINGFIELD];
	qchar model[MAX_STRINGFIELD];
	qint flags;
	float gravity;
	qint damage;
	float radius;
	qint visdamage;
	qint damagetype;
	qint healthinc;
	float push;
	float detonation;
	float bounce;
	float bouncefric;
	float bouncestop;
} projectileinfo_t;

typedef struct weaponinfo_s
{
	qint valid;					//true if the weapon info is valid
	qint number;									//number of the weapon
	qchar name[MAX_STRINGFIELD];
	qchar model[MAX_STRINGFIELD];
	qint level;
	qint weaponindex;
	qint flags;
	qchar projectile[MAX_STRINGFIELD];
	qint numprojectiles;
	float hspread;
	float vspread;
	float speed;
	float acceleration;
	vec3_t recoil;
	vec3_t offset;
	vec3_t angleoffset;
	float extrazvelocity;
	qint ammoamount;
	qint ammoindex;
	float activate;
	float reload;
	float spinup;
	float spindown;
	projectileinfo_t proj;						//pointer to the used projectile
} weaponinfo_t;

//setup the weapon AI
qint BotSetupWeaponAI(void);
//shut down the weapon AI
void BotShutdownWeaponAI(void);
//returns the best weapon to fight with
qint BotChooseBestFightWeapon(qint weaponstate, qint *inventory);
//returns the information of the current weapon
void BotGetWeaponInfo(qint weaponstate, qint weapon, weaponinfo_t *weaponinfo);
//loads the weapon weights
qint BotLoadWeaponWeights(qint weaponstate, const qchar *filename);
//returns a handle to a newly allocated weapon state
qint BotAllocWeaponState(void);
//frees the weapon state
void BotFreeWeaponState(qint weaponstate);
//resets the whole weapon state
void BotResetWeaponState(qint weaponstate);
