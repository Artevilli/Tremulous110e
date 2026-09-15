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
 * name:		botlib.h
 *
 * desc:		bot AI library
 *
 * $Archive: /source/code/game/botai.h $
 *
 *****************************************************************************/

#define	BOTLIB_API_VERSION		2

struct aas_clientmove_s;
struct aas_entityinfo_s;
struct aas_areainfo_s;
struct aas_altroutegoal_s;
struct aas_predictroute_s;
struct bot_consolemessage_qvm_s;
struct bot_match_s;
struct bot_goal_s;
struct bot_moveresult_s;
struct bot_initmove_s;
struct weaponinfo_s;

#define BOTFILESBASEFOLDER		"botfiles"
//debug line colors
#define LINECOLOR_NONE			-1
#define LINECOLOR_RED			1//0xf2f2f0f0L
#define LINECOLOR_GREEN			2//0xd0d1d2d3L
#define LINECOLOR_BLUE			3//0xf3f3f1f1L
#define LINECOLOR_YELLOW		4//0xdcdddedfL
#define LINECOLOR_ORANGE		5//0xe0e1e2e3L

//Print types
#define PRT_MESSAGE				1
#define PRT_WARNING				2
#define PRT_ERROR				3
#define PRT_FATAL				4
#define PRT_EXIT				5

//console message types
#define CMS_NORMAL				0
#define CMS_CHAT				1

//botlib error codes
#define BLERR_NOERROR					0	//no error
#define BLERR_LIBRARYNOTSETUP			1	//library not setup
#define BLERR_INVALIDENTITYNUMBER		2	//invalid entity number
#define BLERR_NOAASFILE					3	//no AAS file available
#define BLERR_CANNOTOPENAASFILE			4	//cannot open AAS file
#define BLERR_WRONGAASFILEID			5	//incorrect AAS file id
#define BLERR_WRONGAASFILEVERSION		6	//incorrect AAS file version
#define BLERR_CANNOTREADAASLUMP			7	//cannot read AAS file lump
#define BLERR_CANNOTLOADICHAT			8	//cannot load initial chats
#define BLERR_CANNOTLOADITEMWEIGHTS		9	//cannot load item weights
#define BLERR_CANNOTLOADITEMCONFIG		10	//cannot load item config
#define BLERR_CANNOTLOADWEAPONWEIGHTS	11	//cannot load weapon weights
#define BLERR_CANNOTLOADWEAPONCONFIG	12	//cannot load weapon config

//action flags
#define ACTION_ATTACK			0x00000001
#define ACTION_USE			0x00000002
#define ACTION_RESPAWN			0x00000008
#define ACTION_JUMP			0x00000010
#define ACTION_MOVEUP			0x00000020
#define ACTION_CROUCH			0x00000080
#define ACTION_MOVEDOWN			0x00000100
#define ACTION_MOVEFORWARD		0x00000200
#define ACTION_MOVEBACK			0x00000800
#define ACTION_MOVELEFT			0x00001000
#define ACTION_MOVERIGHT		0x00002000
#define ACTION_DELAYEDJUMP		0x00008000
#define ACTION_TALK			0x00010000
#define ACTION_GESTURE			0x00020000
#define ACTION_WALK			0x00080000
#define ACTION_AFFIRMATIVE		0x00100000
#define ACTION_NEGATIVE			0x00200000
#define ACTION_GETFLAG			0x00800000
#define ACTION_GUARDBASE		0x01000000
#define ACTION_PATROL			0x02000000
#define ACTION_FOLLOWME			0x08000000
#define ACTION_JUMPEDLASTFRAME		0x10000000

//the bot input, will be converted to a usercmd_t
typedef struct bot_input_s
{
	float thinktime;		//time since last output (in seconds)
	vec3_t dir;				//movement direction
	float speed;			//speed in the range [0, 400]
	vec3_t viewangles;		//the view angles
	qint actionflags;		//one of the ACTION_? flags
	qint weapon;				//weapon to use
} bot_input_t;

#ifndef BSPTRACE

#define BSPTRACE

//bsp_trace_t hit surface
typedef struct bsp_surface_s
{
	qchar name[16];
	qint flags;
	qint value;
} bsp_surface_t;

//remove the bsp_trace_s structure definition l8r on
//a trace is returned when a box is swept through the world
typedef struct bsp_trace_s
{
	qbool		allsolid;	// if true, plane is not valid
	qbool		startsolid;	// if true, the initial point was in a solid area
	float			fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t			endpos;		// final position
	cplane_t		plane;		// surface normal at impact
	float			exp_dist;	// expanded plane distance
	qint				sidenum;	// number of the brush side hit
	bsp_surface_t	surface;	// the hit point surface
	qint				contents;	// contents on other side of surface hit
	qint				ent;		// number of entity hit
} bsp_trace_t;

#endif	// BSPTRACE

//entity state
typedef struct bot_entitystate_s
{
	qint		type;			// entity type
	qint		flags;			// entity flags
	vec3_t	origin;			// origin of the entity
	vec3_t	angles;			// angles of the model
	vec3_t	old_origin;		// for lerping
	vec3_t	mins;			// bounding box minimums
	vec3_t	maxs;			// bounding box maximums
	qint		groundent;		// ground entity
	qint		solid;			// solid type
	qint		modelindex;		// model used
	qint		modelindex2;	// weapons, CTF flags, etc
	qint		frame;			// model frame number
	qint		event;			// impulse events -- muzzle flashes, footsteps, etc
	qint		eventParm;		// even parameter
	qint		powerups;		// bit flags
	qint		weapon;			// determines weapon and flash model, etc
	qint		legsAnim;		// mask off ANIM_TOGGLEBIT
	qint		torsoAnim;		// mask off ANIM_TOGGLEBIT
} bot_entitystate_t;

//bot AI library exported functions
typedef struct botlib_import_s
{
	//print messages from the bot library
	void		(QDECL *Print)(qint type, const qchar *fmt, ...) __attribute__ ((format (printf, 2, 3)));
	//trace a bbox through the world
	void		(*Trace)(bsp_trace_t *trace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, qint passent, qint contentmask);
	//trace a bbox against a specific entity
	void		(*EntityTrace)(bsp_trace_t *trace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, qint entnum, qint contentmask);
	//retrieve the contents at the given point
	qint			(*PointContents)(vec3_t point);
	//check if the point is in potential visible sight
	qint			(*inPVS)(vec3_t p1, vec3_t p2);
	//retrieve the BSP entity data lump
	qchar		*(*BSPEntityData)(void);
	//
	void		(*BSPModelMinsMaxsOrigin)(qint modelnum, vec3_t angles, vec3_t mins, vec3_t maxs, vec3_t origin);
	//send a bot client command
	void		(*BotClientCommand)( qint client, const qchar *command );
	//memory allocation
	void		*(*GetMemory)(size_t size);		// allocate from Zone
	void		(*FreeMemory)(void *ptr);		// free memory from Zone
	qint			(*AvailableMemory)(void);		// available Zone memory
	void		*(*HunkAlloc)(size_t size);		// allocate from hunk
	//file system access
	qint			(*FS_FOpenFile)( const qchar *qpath, fileHandle_t *file, fsMode_t mode );
	qint			(*FS_Read)( void *buffer, qint len, fileHandle_t f );
	qint			(*FS_Write)( const void *buffer, qint len, fileHandle_t f );
	void		(*FS_FCloseFile)( fileHandle_t f );
	qint			(*FS_Seek)( fileHandle_t f, long offset, fsOrigin_t origin );
	//debug visualisation stuff
	qint			(*DebugLineCreate)(void);
	void		(*DebugLineDelete)(qint line);
	void		(*DebugLineShow)(qint line, vec3_t start, vec3_t end, qint color);
	//
	qint			(*DebugPolygonCreate)(qint color, qint numPoints, vec3_t *points);
	void		(*DebugPolygonDelete)(qint id);

	qint			(*Sys_Milliseconds)(void);
} botlib_import_t;

typedef struct aas_export_s
{
	//-----------------------------------
	// be_aas_entity.c
	//-----------------------------------
	void		(*AAS_EntityInfo)(qint entnum, struct aas_entityinfo_s *info);
	//-----------------------------------
	// be_aas_main.c
	//-----------------------------------
	qint			(*AAS_Initialized)(void);
	void		(*AAS_PresenceTypeBoundingBox)(qint presencetype, vec3_t mins, vec3_t maxs);
	float		(*AAS_Time)(void);
	//--------------------------------------------
	// be_aas_sample.c
	//--------------------------------------------
	qint			(*AAS_PointAreaNum)(vec3_t point);
	qint			(*AAS_PointReachabilityAreaIndex)( vec3_t point );
	qint			(*AAS_TraceAreas)(vec3_t start, vec3_t end, qint *areas, vec3_t *points, qint maxareas);
	qint			(*AAS_BBoxAreas)(vec3_t absmins, vec3_t absmaxs, qint *areas, qint maxareas);
	qint			(*AAS_AreaInfo)( qint areanum, struct aas_areainfo_s *info );
	//--------------------------------------------
	// be_aas_bspq3.c
	//--------------------------------------------
	qint			(*AAS_PointContents)(vec3_t point);
	qint			(*AAS_NextBSPEntity)(qint ent);
	qint			(*AAS_ValueForBSPEpairKey)(qint ent, const qchar *key, qchar *value, qint size);
	qint			(*AAS_VectorForBSPEpairKey)(qint ent, const qchar *key, vec3_t v);
	qint			(*AAS_FloatForBSPEpairKey)(qint ent, const qchar *key, float *value);
	qint			(*AAS_IntForBSPEpairKey)(qint ent, const qchar *key, qint *value);
	//--------------------------------------------
	// be_aas_reach.c
	//--------------------------------------------
	qint			(*AAS_AreaReachability)(qint areanum);
	//--------------------------------------------
	// be_aas_route.c
	//--------------------------------------------
	qint			(*AAS_AreaTravelTimeToGoalArea)(qint areanum, vec3_t origin, qint goalareanum, qint travelflags);
	qint			(*AAS_EnableRoutingArea)(qint areanum, qint enable);
	qint			(*AAS_PredictRoute)(struct aas_predictroute_s *route, qint areanum, vec3_t origin,
							qint goalareanum, qint travelflags, qint maxareas, qint maxtime,
							qint stopevent, qint stopcontents, qint stoptfl, qint stopareanum);
	//--------------------------------------------
	// be_aas_altroute.c
	//--------------------------------------------
	qint			(*AAS_AlternativeRouteGoals)(vec3_t start, qint startareanum, vec3_t goal, qint goalareanum, qint travelflags,
										struct aas_altroutegoal_s *altroutegoals, qint maxaltroutegoals,
										qint type);
	//--------------------------------------------
	// be_aas_move.c
	//--------------------------------------------
	qint			(*AAS_Swimming)(vec3_t origin);
	qint			(*AAS_PredictClientMovement)(struct aas_clientmove_s *move,
											qint entnum, const vec3_t origin,
											qint presencetype, qint onground,
											const vec3_t velocity, const vec3_t cmdmove,
											qint cmdframes,
											qint maxframes, float frametime,
											qint stopevent, qint stopareanum, qint visualize);
} aas_export_t;

typedef struct ea_export_s
{
	//ClientCommand elementary actions
	void	(*EA_Command)(qint client, const qchar *command);
	void	(*EA_Say)(qint client, const qchar *str);
	void	(*EA_SayTeam)(qint client, const qchar *str);
	//
	void	(*EA_Action)(qint client, qint action);
	void	(*EA_Gesture)(qint client);
	void	(*EA_Talk)(qint client);
	void	(*EA_Attack)(qint client);
	void	(*EA_Use)(qint client);
	void	(*EA_Respawn)(qint client);
	void	(*EA_MoveUp)(qint client);
	void	(*EA_MoveDown)(qint client);
	void	(*EA_MoveForward)(qint client);
	void	(*EA_MoveBack)(qint client);
	void	(*EA_MoveLeft)(qint client);
	void	(*EA_MoveRight)(qint client);
	void	(*EA_Crouch)(qint client);

	void	(*EA_SelectWeapon)(qint client, qint weapon);
	void	(*EA_Jump)(qint client);
	void	(*EA_DelayedJump)(qint client);
	void	(*EA_Move)(qint client, vec3_t dir, float speed);
	void	(*EA_View)(qint client, vec3_t viewangles);
	//send regular input to the server
	void	(*EA_EndRegular)(qint client, float thinktime);
	void	(*EA_GetInput)(qint client, float thinktime, bot_input_t *input);
	void	(*EA_ResetInput)(qint client);
} ea_export_t;

typedef struct ai_export_s
{
	//-----------------------------------
	// be_ai_char.h
	//-----------------------------------
	qint		(*BotLoadCharacter)(const qchar *charfile, float skill);
	void	(*BotFreeCharacter)(qint character);
	float	(*Characteristic_Float)(qint character, qint index);
	float	(*Characteristic_BFloat)(qint character, qint index, float min, float max);
	qint		(*Characteristic_Integer)(qint character, qint index);
	qint		(*Characteristic_BInteger)(qint character, qint index, qint min, qint max);
	void	(*Characteristic_String)(qint character, qint index, qchar *buf, qint size);
	//-----------------------------------
	// be_ai_chat.h
	//-----------------------------------
	qint		(*BotAllocChatState)(void);
	void	(*BotFreeChatState)(qint handle);
	void	(*BotQueueConsoleMessage)(qint chatstate, qint type, const qchar *message);
	void	(*BotRemoveConsoleMessage)(qint chatstate, qint handle);
	qint		(*BotNextConsoleMessage)(qint chatstate, struct bot_consolemessage_qvm_s *cm);
	qint		(*BotNumConsoleMessages)(qint chatstate);
	void	(*BotInitialChat)(qint chatstate, const qchar *type, qint mcontext, const qchar *var0, const qchar *var1, const qchar *var2, const qchar *var3, const qchar *var4, const qchar *var5, const qchar *var6, const qchar *var7);
	qint		(*BotNumInitialChats)(qint chatstate, const qchar *type);
	qint		(*BotReplyChat)(qint chatstate, const qchar *message, qint mcontext, qint vcontext, const qchar *var0, const qchar *var1, const qchar *var2, const qchar *var3, const qchar *var4, const qchar *var5, const qchar *var6, const qchar *var7);
	qint		(*BotChatLength)(qint chatstate);
	void	(*BotEnterChat)(qint chatstate, qint client, qint sendto);
	void	(*BotGetChatMessage)(qint chatstate, qchar *buf, qint size);
	qint		(*StringContains)(const qchar *str1, const qchar *str2, qint casesensitive);
	qint		(*BotFindMatch)(const qchar *str, struct bot_match_s *match, unsigned long qint context);
	void	(*BotMatchVariable)(struct bot_match_s *match, qint variable, qchar *buf, qint size);
	void	(*UnifyWhiteSpaces)(qchar *string);
	void	(*BotReplaceSynonyms)(qchar *string, qint size, unsigned long qint context);
	qint		(*BotLoadChatFile)(qint chatstate, const qchar *chatfile, const qchar *chatname);
	void	(*BotSetChatGender)(qint chatstate, qint gender);
	void	(*BotSetChatName)(qint chatstate, const qchar *name, qint client);
	//-----------------------------------
	// be_ai_goal.h
	//-----------------------------------
	void	(*BotResetGoalState)(qint goalstate);
	void	(*BotResetAvoidGoals)(qint goalstate);
	void	(*BotRemoveFromAvoidGoals)(qint goalstate, qint number);
	void	(*BotPushGoal)(qint goalstate, struct bot_goal_s *goal);
	void	(*BotPopGoal)(qint goalstate);
	void	(*BotEmptyGoalStack)(qint goalstate);
	void	(*BotDumpAvoidGoals)(qint goalstate);
	void	(*BotDumpGoalStack)(qint goalstate);
	void	(*BotGoalName)(qint number, qchar *name, qint size);
	qint		(*BotGetTopGoal)(qint goalstate, struct bot_goal_s *goal);
	qint		(*BotGetSecondGoal)(qint goalstate, struct bot_goal_s *goal);
	qint		(*BotChooseLTGItem)(qint goalstate, vec3_t origin, qint *inventory, qint travelflags);
	qint		(*BotChooseNBGItem)(qint goalstate, vec3_t origin, qint *inventory, qint travelflags,
								struct bot_goal_s *ltg, float maxtime);
	qint		(*BotTouchingGoal)(const vec3_t origin, const struct bot_goal_s *goal);
	qint		(*BotItemGoalInVisButNotVisible)(qint viewer, vec3_t eye, vec3_t viewangles, struct bot_goal_s *goal);
	qint		(*BotGetLevelItemGoal)(qint index, const qchar *classname, struct bot_goal_s *goal);
	qint		(*BotGetNextCampSpotGoal)(qint num, struct bot_goal_s *goal);
	qint		(*BotGetMapLocationGoal)(const qchar *name, struct bot_goal_s *goal);
	float	(*BotAvoidGoalTime)(qint goalstate, qint number);
	void	(*BotSetAvoidGoalTime)(qint goalstate, qint number, float avoidtime);
	void	(*BotInitLevelItems)(void);
	void	(*BotUpdateEntityItems)(void);
	qint		(*BotLoadItemWeights)(qint goalstate, const qchar *filename);
	void	(*BotFreeItemWeights)(qint goalstate);
	void	(*BotInterbreedGoalFuzzyLogic)(qint parent1, qint parent2, qint child);
	void	(*BotSaveGoalFuzzyLogic)(qint goalstate, const qchar *filename);
	void	(*BotMutateGoalFuzzyLogic)(qint goalstate, float range);
	qint		(*BotAllocGoalState)(qint client);
	void	(*BotFreeGoalState)(qint handle);
	//-----------------------------------
	// be_ai_move.h
	//-----------------------------------
	void	(*BotResetMoveState)(qint movestate);
	void	(*BotMoveToGoal)(struct bot_moveresult_s *result, qint movestate, struct bot_goal_s *goal, qint travelflags);
	qint		(*BotMoveInDirection)(qint movestate, vec3_t dir, float speed, qint type);
	void	(*BotResetAvoidReach)(qint movestate);
	void	(*BotResetLastAvoidReach)(qint movestate);
	qint		(*BotReachabilityArea)(vec3_t origin, qint testground);
	qint		(*BotMovementViewTarget)(qint movestate, struct bot_goal_s *goal, qint travelflags, float lookahead, vec3_t target);
	qint		(*BotPredictVisiblePosition)(vec3_t origin, qint areanum, struct bot_goal_s *goal, qint travelflags, vec3_t target);
	qint		(*BotAllocMoveState)(void);
	void	(*BotFreeMoveState)(qint handle);
	void	(*BotInitMoveState)(qint handle, struct bot_initmove_s *initmove);
	void	(*BotAddAvoidSpot)(qint movestate, const vec3_t origin, float radius, qint type);
	//-----------------------------------
	// be_ai_weap.h
	//-----------------------------------
	qint		(*BotChooseBestFightWeapon)(qint weaponstate, qint *inventory);
	void	(*BotGetWeaponInfo)(qint weaponstate, qint weapon, struct weaponinfo_s *weaponinfo);
	qint		(*BotLoadWeaponWeights)(qint weaponstate, const qchar *filename);
	qint		(*BotAllocWeaponState)(void);
	void	(*BotFreeWeaponState)(qint weaponstate);
	void	(*BotResetWeaponState)(qint weaponstate);
	//-----------------------------------
	// be_ai_gen.h
	//-----------------------------------
	qint		(*GeneticParentsAndChildSelection)(qint numranks, float *ranks, qint *parent1, qint *parent2, qint *child);
} ai_export_t;

//bot AI library imported functions
typedef struct botlib_export_s
{
	//Area Awareness System functions
	aas_export_t aas;
	//Elementary Action functions
	ea_export_t ea;
	//AI functions
	ai_export_t ai;
	//setup the bot library, returns BLERR_
	qint (*BotLibSetup)(void);
	//shutdown the bot library, returns BLERR_
	qint (*BotLibShutdown)(void);
	//sets a library variable returns BLERR_
	qint (*BotLibVarSet)( const qchar *var_name, const qchar *value );
	//gets a library variable returns BLERR_
	qint (*BotLibVarGet)( const qchar *var_name, qchar *value, qint size );

	//sets a C-like define returns BLERR_
	qint (*PC_AddGlobalDefine)(const qchar *string);
	qint (*PC_LoadSourceHandle)(const qchar *filename);
	qint (*PC_FreeSourceHandle)(qint handle);
	qint (*PC_ReadTokenHandle)(qint handle, pc_token_t *pc_token);
	qint (*PC_SourceFileAndLine)(qint handle, qchar *filename, qint *line);

	//start a frame in the bot library
	qint (*BotLibStartFrame)(float time);
	//load a new map in the bot library
	qint (*BotLibLoadMap)(const qchar *mapname);
	//entity updates
	qint (*BotLibUpdateEntity)(qint ent, bot_entitystate_t *state);
	//just for testing
	qint (*Test)(qint parm0, qchar *parm1, vec3_t parm2, vec3_t parm3);
} botlib_export_t;

//linking of bot library
botlib_export_t *GetBotLibAPI( qint apiVersion, botlib_import_t *import );

/* Library variables:

name:						default:			module(s):			description:

"basedir"					"baseq3"			be_interface.c		base directory
"gamedir"					""					be_interface.c		game directory
"homedir"					""					be_interface.c		home directory

"log"						"0"					l_log.c				enable/disable creating a log file
"maxclients"				"4"					be_interface.c		maximum number of clients
"maxentities"				"1024"				be_interface.c		maximum number of entities
"bot_developer"				"0"					be_interface.c		bot developer mode (it's "botDeveloper" in C to prevent symbol clash).

"phys_friction"				"6"					be_aas_move.c		ground friction
"phys_stopspeed"			"100"				be_aas_move.c		stop speed
"phys_gravity"				"800"				be_aas_move.c		gravity value
"phys_waterfriction"		"1"					be_aas_move.c		water friction
"phys_watergravity"			"400"				be_aas_move.c		gravity in water
"phys_maxvelocity"			"320"				be_aas_move.c		maximum velocity
"phys_maxwalkvelocity"		"320"				be_aas_move.c		maximum walk velocity
"phys_maxcrouchvelocity"	"100"				be_aas_move.c		maximum crouch velocity
"phys_maxswimvelocity"		"150"				be_aas_move.c		maximum swim velocity
"phys_walkaccelerate"		"10"				be_aas_move.c		walk acceleration
"phys_airaccelerate"		"1"					be_aas_move.c		air acceleration
"phys_swimaccelerate"		"4"					be_aas_move.c		swim acceleration
"phys_maxstep"				"18"				be_aas_move.c		maximum step height
"phys_maxsteepness"			"0.7"				be_aas_move.c		maximum floor steepness
"phys_maxbarrier"			"32"				be_aas_move.c		maximum barrier height
"phys_maxwaterjump"			"19"				be_aas_move.c		maximum waterjump height
"phys_jumpvel"				"270"				be_aas_move.c		jump z velocity
"phys_falldelta5"			"40"				be_aas_move.c
"phys_falldelta10"			"60"				be_aas_move.c
"rs_waterjump"				"400"				be_aas_move.c
"rs_teleport"				"50"				be_aas_move.c
"rs_barrierjump"			"100"				be_aas_move.c
"rs_startcrouch"			"300"				be_aas_move.c
"rs_startgrapple"			"500"				be_aas_move.c
"rs_startwalkoffledge"		"70"				be_aas_move.c
"rs_startjump"				"300"				be_aas_move.c
"rs_rocketjump"				"500"				be_aas_move.c
"rs_bfgjump"				"500"				be_aas_move.c
"rs_jumppad"				"250"				be_aas_move.c
"rs_aircontrolledjumppad"	"300"				be_aas_move.c
"rs_funcbob"				"300"				be_aas_move.c
"rs_startelevator"			"50"				be_aas_move.c
"rs_falldamage5"			"300"				be_aas_move.c
"rs_falldamage10"			"500"				be_aas_move.c
"rs_maxjumpfallheight"		"450"				be_aas_move.c

"max_aaslinks"				"4096"				be_aas_sample.c		maximum links in the AAS
"max_routingcache"			"12288"				be_aas_route.c		maximum routing cache size in KB
"forceclustering"			"0"					be_aas_main.c		force recalculation of clusters
"forcereachability"			"0"					be_aas_main.c		force recalculation of reachabilities
"forcewrite"				"0"					be_aas_main.c		force writing of aas file
"aasoptimize"				"0"					be_aas_main.c		enable aas optimization
"sv_mapChecksum"			"0"					be_aas_main.c		BSP file checksum
"bot_visualizejumppads"		"0"					be_aas_reach.c		visualize jump pads

"bot_reloadcharacters"		"0"					-					reload bot character files
"ai_gametype"				"0"					be_ai_goal.c		game type
"droppedweight"				"1000"				be_ai_goal.c		additional dropped item weight
"weapindex_rocketlauncher"	"5"					be_ai_move.c		rl weapon index for rocket jumping
"weapindex_bfg10k"			"9"					be_ai_move.c		bfg weapon index for bfg jumping
"weapindex_grapple"			"10"				be_ai_move.c		grapple weapon index for grappling
"entitytypemissile"			"3"					be_ai_move.c		ET_MISSILE
"offhandgrapple"			"0"					be_ai_move.c		enable off hand grapple hook
"cmd_grappleon"				"grappleon"			be_ai_move.c		command to activate off hand grapple
"cmd_grappleoff"			"grappleoff"		be_ai_move.c		command to deactivate off hand grapple
"itemconfig"				"items.c"			be_ai_goal.c		item configuration file
"weaponconfig"				"weapons.c"			be_ai_weap.c		weapon configuration file
"synfile"					"syn.c"				be_ai_chat.c		file with synonyms
"rndfile"					"rnd.c"				be_ai_chat.c		file with random strings
"matchfile"					"match.c"			be_ai_chat.c		file with match strings
"nochat"					"0"					be_ai_chat.c		disable chats
"max_messages"				"1024"				be_ai_chat.c		console message heap size
"max_weaponinfo"			"32"				be_ai_weap.c		maximum number of weapon info
"max_projectileinfo"		"32"				be_ai_weap.c		maximum number of projectile info
"max_iteminfo"				"256"				be_ai_goal.c		maximum number of item info
"max_levelitems"			"256"				be_ai_goal.c		maximum number of level items

*/

