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

//sv_challenge.c - stateless challenge creation and verification functions

#include "server.h"
#include "../qcommon/md5.h"

#if defined(STATELESS_CHALLENGES_VERSION_ONE)

//#define DEBUG_SV_CHALLENGE //enable for com_dprintf debugging output (moved to sv_cvars.h)

static const size_t SECRET_KEY_LENGTH = MD5_DIGEST_SIZE; //key length equal to digest length is adequate

static qbool challengerInitialized = qfalse;
static hmacMD5Context_t challenger;

#if defined(DEBUG_SV_CHALLENGE)
/*
====================
BufferHexToString

Format a byte buffer as a lower-case hex string.
====================
*/
static const qchar *
BufferHexToString(const byte *buffer, size_t bufferLen)
{
  static qchar hexString[1023];
  static const size_t maxBufferLen = (sizeof(hexString) - 1) / 2;
  static const qchar *hex = "0123456789abcdef";
  size_t i;

  if (bufferLen > maxBufferLen)
  {
    bufferLen = maxBufferLen;
  }

  for(i = 0;i < bufferLen;i++)
  {
    hexString[i * 2] = hex[buffer[i] / 16];
    hexString[i * 2 + 1] = hex[buffer[i] % 16];
  }

  hexString[bufferLen * 2] = '\0';
  return hexString;
}
#endif

/*
====================
SV_ChallengeInit

Initialize the HMAC context for generating challenges.
====================
*/
void
SV_ChallengeInit(void)
{
  if (challengerInitialized)
  {
    SV_ChallengeShutdown();
  }

  //generate a secret key from the OS RNG
  byte secretKey[SECRET_KEY_LENGTH];

  if (!Sys_RandomBytes(secretKey, sizeof(secretKey)))
  {
    Com_Error(ERR_FATAL, "SV_ChallengeInit: Sys_RandomBytes failed");
  }

#if defined(DEBUG_SV_CHALLENGE)
  if (sv_debugChallenges->integer)
  {
    Com_Printf("Initialize challenger: %s\n", BufferHexToString(secretKey, sizeof(secretKey)));
  }
#endif

  HMAC_MD5_Init(&challenger, secretKey, sizeof(secretKey));
  challengerInitialized = qtrue;
}

/*
====================
SV_ChallengeShutdown

Clear the HMAC context used to generate challenges.
====================
*/
void
SV_ChallengeShutdown(void)
{
  if (challengerInitialized)
  {
    Com_Memset(&challenger, 0, sizeof(challenger));
    challengerInitialized = qfalse;
  }
}

/*
====================
SV_CreateChallengeInternal

Create a challenge for the given client address and timestamp.
====================
*/
static const qint
SV_CreateChallengeInternal(const qint timestamp, const netadr_t *from)
{
  const qchar *clientParams = NET_AdrToString(from);
  const size_t clientParamsLen = strlen(clientParams);

  //create an unforgeable, temporal challenge for this client using HMAC(secretKey, clientParams + timestamp)
  byte digest[MD5_DIGEST_SIZE];
  HMAC_MD5_Update(&challenger, (byte *)clientParams, clientParamsLen);
  HMAC_MD5_Update(&challenger, (byte *)&timestamp, sizeof(timestamp));
  HMAC_MD5_Final(&challenger, digest);
  HMAC_MD5_Reset(&challenger);

  //use first 4 bytes of the HMAC digest as an qint (client only deals with numeric challenges)
  //the most significant bit stores whether the timestamp is odd or even, this lets later verification code handle the
  //case where the engine timestamp has incremented between the time this challenge is sent and the client replies
  qint challenge;
  Com_Memcpy(&challenge, digest, sizeof(challenge));
  challenge &= 0x7FFFFFFF;
  challenge |= (const unsigned)(timestamp & 0x1) << 31;

#if defined(DEBUG_SV_CHALLENGE)
  if (sv_debugChallenges->integer)
  {
    Com_Printf("Generated challenge %d (timestamp = %d) for %s\n", challenge, timestamp, NET_AdrToString(from));
  }
#endif
  return challenge;
}

/*
====================
SV_CreateChallenge

Create an unforgeable, temporal challenge for the given client address
====================
*/
const qint
SV_CreateChallenge(const netadr_t *from)
{
  if (!challengerInitialized)
  {
    Com_Error(ERR_FATAL, "SV_CreateChallenge: The challenge subsystem has not been initialized!");
  }

  //the current time gets 14 bits chopped off to create a challenge timestamp that changes every 16.384 seconds
  //this allows clients at least ~16 seconds from now to reply to the challenge
  //const qint currentTimestamp = svs.time >> 14;
  const qint currentTimestamp = svs.time >> 13; //Chey: 16 seconds is way too long, this gives clients around 8 seconds to respond
  return SV_CreateChallengeInternal(currentTimestamp, from);
}

/*
====================
SV_VerifyChallenge

Verify a challenge received by the client matches the expected challenge.
====================
*/
const qbool
SV_VerifyChallenge(const qint receivedChallenge, const netadr_t *from)
{
  if (!challengerInitialized)
  {
    Com_Error(ERR_FATAL, "SV_VerifyChallenge: The challenge subsystem has not been initialized!");
  }

  //const qint currentTimestamp = svs.time >> 14;
  const qint currentTimestamp = svs.time >> 13; //Chey: see comment in SV_CreateChallenge
  const qint currentPeriod = currentTimestamp & 0x1;

  //use the current tiemstamp for verification if the current period matches the client challenge's period
  //otherwise use the previous timestamp in case the current timestamp incremented in the time between the
  //client being sent ta challenge and the client's reply that's being verified now
  const qint challengePeriod = ((const unsigned)receivedChallenge >> 31) & 0x1;
  const qint challengeTimestamp = currentTimestamp - (currentPeriod ^ challengePeriod);

#if defined(DEBUG_SV_CHALLENGE)
  if (sv_debugChallenges->integer)
  {
    Com_Printf("Verifying challenge %d (timestamp = %d) for %s\n", receivedChallenge, challengeTimestamp, NET_AdrToString(from));
  }
#endif
  const qint expectedChallenge = SV_CreateChallengeInternal(challengeTimestamp, from);
  return (qbool)(receivedChallenge == expectedChallenge);
}
#endif //defined(STATELESS_CHALLENGES_VERSION_ONE
