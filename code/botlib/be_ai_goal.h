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
 * name:		be_ai_goal.h
 *
 * desc:		goal AI
 *
 * $Archive: /source/code/botlib/be_ai_goal.h $
 *
 *****************************************************************************/

#define MAX_AVOIDGOALS			256
#define MAX_GOALSTACK			8

#define GFL_NONE				0
#define GFL_ITEM				1
#define GFL_ROAM				2
#define GFL_DROPPED				4

//a bot goal
typedef struct bot_goal_s
{
	vec3_t origin;				//origin of the goal
	qint areanum;				//area number of the goal
	vec3_t mins, maxs;			//mins and maxs of the goal
	qint entitynum;				//number of the goal entity
	qint number;					//goal number
	qint flags;					//goal flags
	qint iteminfo;				//item information
} bot_goal_t;

//reset the whole goal state, but keep the item weights
void BotResetGoalState(qint goalstate);
//reset avoid goals
void BotResetAvoidGoals(qint goalstate);
//remove the goal with the given number from the avoid goals
void BotRemoveFromAvoidGoals(qint goalstate, qint number);
//push a goal onto the goal stack
void BotPushGoal(qint goalstate, bot_goal_t *goal);
//pop a goal from the goal stack
void BotPopGoal(qint goalstate);
//empty the bot's goal stack
void BotEmptyGoalStack(qint goalstate);
//dump the avoid goals
void BotDumpAvoidGoals(qint goalstate);
//dump the goal stack
void BotDumpGoalStack(qint goalstate);
//get the name name of the goal with the given number
void BotGoalName(qint number, qchar *name, qint size);
//get the top goal from the stack
qint BotGetTopGoal(qint goalstate, bot_goal_t *goal);
//get the second goal on the stack
qint BotGetSecondGoal(qint goalstate, bot_goal_t *goal);
//choose the best long term goal item for the bot
qint BotChooseLTGItem(qint goalstate, vec3_t origin, qint *inventory, qint travelflags);
//choose the best nearby goal item for the bot
//the item may not be further away from the current bot position than maxtime
//also the travel time from the nearby goal towards the long term goal may not
//be larger than the travel time towards the long term goal from the current bot position
qint BotChooseNBGItem(qint goalstate, vec3_t origin, qint *inventory, qint travelflags,
							bot_goal_t *ltg, float maxtime);
//returns true if the bot touches the goal
qint BotTouchingGoal(const vec3_t origin, const bot_goal_t *goal);
//returns true if the goal should be visible but isn't
qint BotItemGoalInVisButNotVisible(qint viewer, vec3_t eye, vec3_t viewangles, bot_goal_t *goal);
//search for a goal for the given classname, the index can be used
//as a start point for the search when multiple goals are available with that same classname
qint BotGetLevelItemGoal(qint index, const qchar *classname, bot_goal_t *goal);
//get the next camp spot in the map
qint BotGetNextCampSpotGoal(qint num, bot_goal_t *goal);
//get the map location with the given name
qint BotGetMapLocationGoal(const qchar *name, bot_goal_t *goal);
//returns the avoid goal time
float BotAvoidGoalTime(qint goalstate, qint number);
//set the avoid goal time
void BotSetAvoidGoalTime(qint goalstate, qint number, float avoidtime);
//initializes the items in the level
void BotInitLevelItems(void);
//regularly update dynamic entity items (dropped weapons, flags etc.)
void BotUpdateEntityItems(void);
//interbreed the goal fuzzy logic
void BotInterbreedGoalFuzzyLogic(qint parent1, qint parent2, qint child);
//save the goal fuzzy logic to disk
void BotSaveGoalFuzzyLogic(qint goalstate, const qchar *filename);
//mutate the goal fuzzy logic
void BotMutateGoalFuzzyLogic(qint goalstate, float range);
//loads item weights for the bot
qint BotLoadItemWeights(qint goalstate, const qchar *filename);
//frees the item weights of the bot
void BotFreeItemWeights(qint goalstate);
//returns the handle of a newly allocated goal state
qint BotAllocGoalState(qint client);
//free the given goal state
void BotFreeGoalState(qint handle);
//setup the goal AI
qint BotSetupGoalAI(void);
//shut down the goal AI
void BotShutdownGoalAI(void);
