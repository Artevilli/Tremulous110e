/*
 ===========================================================================
 Copyright (C) 1998 Steve Yeager
 Copyright (C) 2006 Cheyenne Spring Barnes
 Copyright (C) 2008 Robert Beckebans <trebor_7@users.sourceforge.net>

 This file is part of XreaL source code.

 XreaL source code is free software; you can redistribute it
 and/or modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; either version 2 of the License,
 or (at your option) any later version.

 XreaL source code is distributed in the hope that it will be
 useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with XreaL source code; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ===========================================================================
 */
 
//sv_demo.c -  serverside demo recording
 
#include "server.h"
 
//headers for demo messages
typedef enum
{
 demo_endFrame,
 demo_serverCommand,
 demo_gameCommand,
 demo_entityState,
 demo_entityShared,
 demo_playerState,
 demo_endDemo,
 demo_EOF
}
demo_ops_e;

//large buffer for storage
static byte buf[0x400000];

//save maxclients/democlients and restore after demo
static qint savedMaxClients, savedDemoClients;

/*
====================
SV_DemoWriteMessage

write a messages to demo file
====================
*/
static void
SV_DemoWriteMessage(msg_t *msg)
{
  qint len;

  //write entire message to file, prefixed by length
  MSG_WriteByte(msg, demo_EOF);
  len = LittleLong(msg->cursize);
  FS_Write(&len, 4, sv.demoFile);
  FS_Write(msg->data, msg->cursize, sv.demoFile);
  MSG_Clear(msg);
}

/*
====================
SV_DemoWriteServerCommand

write a server command to demo file
====================
*/
void
SV_DemoWriteServerCommand(const qchar *str)
{
  msg_t msg;

  MSG_Init(&msg, buf, sizeof(buf));
  MSG_WriteByte(&msg, demo_serverCommand);
  MSG_WriteString(&msg, str);
  SV_DemoWriteMessage(&msg);
}

/*
====================
SV_DemoWriteGameCommand

write a game command to demo file
====================
*/
void
SV_DemoWriteGameCommand(qint cmd, const qchar *str)
{
  msg_t msg;

  MSG_Init(&msg, buf, sizeof(buf));
  MSG_WriteByte(&msg, demo_gameCommand);
  MSG_WriteByte(&msg, cmd);
  MSG_WriteString(&msg, str);
  SV_DemoWriteMessage(&msg);
}

/*
====================
SV_DemoWriteFrame

record all entries and players at end of frame
====================
*/
void
SV_DemoWriteFrame(void)
{
  msg_t msg;
  playerState_t *player;
  sharedEntity_t *entity;
  qint i;

  MSG_Init(&msg, buf, sizeof(buf));

  //write entities
  MSG_WriteByte(&msg, demo_entityState);

  for(i = 0;i < sv.num_entities;i++)
  {
    if (i >= sv.maxclients && i < MAX_CLIENTS)
    {
      continue;
    }

    entity = SV_GentityNum(i);
    entity->s.number = i;
    MSG_WriteDeltaEntity(&msg, &sv.demoEntities[i].s, &entity->s, qfalse);
    sv.demoEntities[i].s = entity->s;
  }

  MSG_WriteBits(&msg, ENTITYNUM_NONE, GENTITYNUM_BITS);
  MSG_WriteByte(&msg, demo_entityShared);

  for(i = 0;i < sv.num_entities;i++)
  {
    if (i >= sv.maxclients && i < MAX_CLIENTS)
    {
      continue;
    }

    entity = SV_GentityNum(i);
    MSG_WriteDeltaSharedEntity(&msg, &sv.demoEntities[i].r, &entity->r, qfalse, i);
    sv.demoEntities[i].r = entity->r;
  }

  MSG_WriteBits(&msg, ENTITYNUM_NONE, GENTITYNUM_BITS);
  SV_DemoWriteMessage(&msg);

  //write clients
  for(i = 0;i < sv.maxclients;i++)
  {
    if (svs.clients[i].state < CS_ACTIVE)
    {
      continue;
    }

    player = SV_GameClientNum(i);
    MSG_WriteByte(&msg, demo_playerState);
    MSG_WriteBits(&msg, i, CLIENTNUM_BITS);
    MSG_WriteDeltaPlayerstate(&msg, &sv.demoPlayerStates[i], player);
    sv.demoPlayerStates[i] = *player;
  }

  MSG_WriteByte(&msg, demo_endFrame);
  MSG_WriteLong(&msg, sv.time);
  SV_DemoWriteMessage(&msg);
}

/*
====================
SV_DemoReadFrame

play a frame from demo file
====================
*/
void
SV_DemoReadFrame(void)
{
  msg_t msg;
  qint cmd;
  qint r;
  qint num;
  qint i;
  playerState_t *player;
  sharedEntity_t *entity;

  MSG_Init(&msg, buf, sizeof(buf));

  while(1)
  {
exit_loop:

    //get a message
    r = FS_Read(&msg.cursize, 4, sv.demoFile);

    if (r != 4)
    {
      SV_DemoStopPlayback();
      return;
    }

    msg.cursize = LittleLong(msg.cursize);

    if (msg.cursize > msg.maxsize)
    {
      Com_Error(ERR_DROP, "SV_DemoReadFrame: demo message too long");
    }

    r = FS_Read(msg.data, msg.cursize, sv.demoFile);

    if (r != msg.cursize)
    {
      Com_Printf("demo file is truncated\n");
      SV_DemoStopPlayback();
      return;
    }

    //parse message
    while(1)
    {
      cmd = MSG_ReadByte(&msg);

      switch (cmd)
      {
        default:
          Com_Error(ERR_DROP, "SV_DemoReadFrame: illegible message\n");
          return;

        case
        demo_EOF:
          MSG_Clear(&msg);
          goto exit_loop;

        case
        demo_endDemo:
          SV_DemoStopPlayback();
          return;

        case
        demo_endFrame:
          //overwrite anything game changes
          for(i = 0;i < sv.num_entities;i++)
          {
            if (i >= sv_democlients->integer && i < MAX_CLIENTS)
            {
              continue;
            }

            *SV_GentityNum(i) = sv.demoEntities[i];
          }

          for(i = 0;i < sv_democlients->integer;i++)
          {
            *SV_GameClientNum(i) = sv.demoPlayerStates[i];
          }

          //set server time
          sv.time = MSG_ReadLong(&msg);
          return;

        case
        demo_serverCommand:
          //Cmd_SaveCmdContext(); //Chey: FIXME: this is no longer a valid function
          Cmd_TokenizeString(MSG_ReadString(&msg));
          SV_SendServerCommand(NULL, "%s \"^3[DEMO] ^7%s\"", Cmd_Argv(0), Cmd_ArgsFrom(1));
          //Cmd_RestoreCmdContext(); //Chey: FIXME: this is no longer a valid function
          break;

        case
        demo_gameCommand:
          num = MSG_ReadByte(&msg);
          //Cmd_SaveCmdContext(); //Chey: FIXME: this is no longer a valid function
          Cmd_TokenizeString(MSG_ReadString(&msg));
          VM_Call(sv.gvm, G_DEMO_COMMAND, 2, num);
          //Cmd_RestoreCmdContext(); //Chey: FIXME: this is no longer a valid function
          break;

        case
        demo_playerState:
          num = MSG_ReadBits(&msg, CLIENTNUM_BITS);
          player = SV_GameClientNum(num);
          MSG_ReadDeltaPlayerstate(&msg, &sv.demoPlayerStates[num], player);
          sv.demoPlayerStates[num] = *player;
          break;

        case
        demo_entityState:
          while(1)
          {
            num = MSG_ReadBits(&msg, GENTITYNUM_BITS);

            if (num == ENTITYNUM_NONE)
            {
              break;
            }

            entity = SV_GentityNum(num);
            MSG_ReadDeltaEntity(&msg, &sv.demoEntities[num].s, &entity->s, num);
            sv.demoEntities[num].s = entity->s;
          }
          break;

        case
        demo_entityShared:
          while(1)
          {
            num = MSG_ReadBits(&msg, GENTITYNUM_BITS);

            if (num == ENTITYNUM_NONE)
            {
              break;
            }

            entity = SV_GentityNum(num);
            MSG_ReadDeltaSharedEntity(&msg, &sv.demoEntities[num].r, &entity->r, num);

            //link or unlink entity
            if (entity->r.linked && (!sv.demoEntities[num].r.linked || entity->r.linkcount != sv.demoEntities[num].r.linkcount))
            {
              SV_LinkEntity(entity);
            }
            else if (!entity->r.linked && sv.demoEntities[num].r.linked)
            {
              SV_UnlinkEntity(entity);
            }
            else
            {
            }

            sv.demoEntities[num].r = entity->r;

            if (num > sv.num_entities)
            {
              sv.num_entities = num;
            }
          }

          break;
      }
    }
  }
}

/*
====================
SV_DemoStartRecord

sv.demo* already set and demo file open, write gamestate info
====================
*/
void
SV_DemoStartRecord(void)
{
  msg_t msg;

  MSG_Init(&msg, buf, sizeof(buf));
  MSG_WriteLong(&msg, sv.time); //write current line
  MSG_WriteString(&msg, sv_mapname->string); //write map name
  MSG_WriteBits(&msg, sv.maxclients, CLIENTNUM_BITS); //write number of clients (sv_maxclients < MAX_CLIENTS else cant playback)
  SV_DemoWriteMessage(&msg);
  //write entities and players
  Com_Memset(sv.demoEntities, 0, sizeof(sv.demoEntities));
  Com_Memset(sv.demoPlayerStates, 0, sizeof(sv.demoPlayerStates));
  SV_DemoWriteFrame();
  Com_Printf("recording demo %s.\n", sv.demoName);
  sv.demoState = DS_RECORDING;
  Cvar_SetValue("sv_demoState", DS_RECORDING);
}

/*
====================
SV_DemoStopRecord

write eof and close demo file
====================
*/
void
SV_DemoStopRecord(void)
{
  msg_t msg;

  //end demo
  MSG_Init(&msg, buf, sizeof(buf));
  MSG_WriteByte(&msg, demo_endDemo);
  SV_DemoWriteMessage(&msg);
  FS_FCloseFile(sv.demoFile);
  sv.demoState = DS_NONE;
  Cvar_SetValue("sv_demoState", DS_NONE);
  Com_Printf("stopped demo %s.\n", sv.demoName);
}

/*
====================
SV_DemoStartPlayback

sv.demo* already set and demo file opened, read gamestate info
====================
*/
void
SV_DemoStartPlayback(void)
{
  msg_t msg;
  qint r;
  qint i;
  qint clients;
  qint count;
  const qchar *s;

  MSG_Init(&msg, buf, sizeof(buf));
  //get demo header
  r = FS_Read(&msg.cursize, 4, sv.demoFile);

  if (r != 4)
  {
    SV_DemoStopPlayback();
    return;
  }

  msg.cursize = LittleLong(msg.cursize);

  if (msg.cursize == -1)
  {
    SV_DemoStopPlayback();
    return;
  }

  if (msg.cursize > msg.maxsize)
  {
    Com_Error(ERR_DROP, "SV_DemoReadFrame: demo msg too long");
  }

  r = FS_Read(msg.data, msg.cursize, sv.demoFile);

  if (r != msg.cursize)
  {
    Com_Printf("demo file is truncated\n");
    SV_DemoStopPlayback();
    return;
  }

  //check slots, time, map
  savedMaxClients = sv.maxclients;
  savedDemoClients = sv_democlients->integer;
  r = MSG_ReadLong(&msg);

  if (r < 400)
  {
    Com_Printf("demo time too small: %d.\n", r);
    SV_DemoStopPlayback();
    return;
  }

  s = MSG_ReadString(&msg);

  if (!FS_FOpenFileRead(va(NULL, "maps/%s.bsp", s), NULL, qfalse))
  {
    Com_Printf("map doesnt exist: %s.\n", s);
    SV_DemoStopPlayback();
    return;
  }

  clients = MSG_ReadBits(&msg, CLIENTNUM_BITS);

  if (sv_democlients->integer < clients)
  {
    count = 0;

    //get number of clients
    for(i = 0;i < sv.maxclients;i++)
    {
      if (svs.clients[i].state >= CS_CONNECTED)
      {
        count++;
      }
    }

    if (clients + count > MAX_CLIENTS)
    {
      Com_Printf("not enough slots, %d clients need to disconnect\n", clients + count - MAX_CLIENTS);
      SV_DemoStopPlayback();
      return;
    }

    Cvar_SetValue("sv_democlients", clients);
    Cvar_SetValue("sv_maxclients", clients + count);
  }

  if (!com_sv_running->integer || strcmp(sv_mapname->string, s) || !Cvar_VariableIntegerValue("sv_cheats") || r < sv.time || sv_maxclients->modified || sv_democlients->modified)
  {
    //change to correct map and start demo with warmup delay
    Cbuf_AddText(va(NULL, "devmap %s\ndelay %d %s\n", s, Cvar_VariableIntegerValue("g_warmup") * 1000, Cmd_Cmd()));
    SV_DemoStopPlayback();
    return;
  }

  //initialize
  Com_Memset(sv.demoEntities, 0, sizeof(sv.demoEntities));
  Com_Memset(sv.demoPlayerStates, 0, sizeof(sv.demoPlayerStates));
  Cvar_SetValue("sv_democlients", clients);
  SV_DemoReadFrame();
  Com_Printf("playing demo %s\n", sv.demoName);
  sv.demoState = DS_PLAYBACK;
  Cvar_SetValue("sv_demoState", DS_PLAYBACK);
}

/*
====================
SV_DemoStopPlayback

close demo and restart map
====================
*/
void
SV_DemoStopPlayback(void)
{
  FS_FCloseFile(sv.demoFile);
  sv.demoState = DS_NONE;
  Cvar_SetValue("sv_demoState", DS_NONE);
  Com_Printf("stopped demo %s\n", sv.demoName);
  //restore clients
  Cvar_SetValue("sv_maxclients", savedMaxClients);
  Cvar_SetValue("sv_democlients", savedDemoClients);

  //demo hasnt started
  if (sv.demoState != DS_PLAYBACK)
  {
#if defined(DEDICATED)
    Cbuf_AddText("map_restart 0\n");
#else
    Cbuf_AddText("killserver\n");
#endif
  }
}
