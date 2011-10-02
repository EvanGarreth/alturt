/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2009-2011 Brian Labbie and Dave Richardson.

http://sourceforge.net/projects/alturt/

This file is part of Alturt source code.

Alturt source code is free software: you can redistribute it
and/or modify it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version.

Alturt source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Alturt source code.  If not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/
//
// cg_players.c -- handle the media and animation for player entities
#include "cg_local.h"

char	*cg_customSoundNames[MAX_CUSTOM_SOUNDS] = {
	"*death1.wav",
	"*death2.wav",
	"*death3.wav",
	"*jump1.wav",
	"*pain25_1.wav",
	"*pain50_1.wav",
	"*pain75_1.wav",
	"*pain100_1.wav",
	"*falling1.wav",
	"*gasp.wav",
	"*drown.wav",
	"*fall1.wav",
	"*taunt.wav"
};


/*
================
CG_CustomSound

================
*/
sfxHandle_t	CG_CustomSound( int clientNum, const char *soundName ) {
	clientInfo_t *ci;
	int			i;

	if ( soundName[0] != '*' ) {
		return trap_S_RegisterSound( soundName, qfalse );
	}

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		clientNum = 0;
	}
	ci = &cgs.clientinfo[ clientNum ];

	for ( i = 0 ; i < MAX_CUSTOM_SOUNDS && cg_customSoundNames[i] ; i++ ) {
		if ( !strcmp( soundName, cg_customSoundNames[i] ) ) {
			return ci->sounds[i];
		}
	}

	CG_Error( "Unknown custom sound: %s", soundName );
	return 0;
}



/*
=============================================================================

CLIENT INFO

=============================================================================
*/


/*
======================
CG_ParseAnimationFile

Read a configuration file containing animation coutns and rates
models/players/visor/animation.cfg, etc
======================
*/
static qboolean	CG_ParseAnimationFile( const char *filename, clientInfo_t *ci, const char *modelName ) {
	int			bludtemp; //blud
	//int			j; //blud debug
	int			urtModelAnimOffset; //blud
	char		*text_p, *prev;
	int			len;
	int			i;
	char		*token;
	float		fps;
	int			skip;
	char		text[20000];
	fileHandle_t	f;
	animation_t *animations;


	urtModelAnimOffset = 224; //blud: used for UrT style models (Orion & Athena).
	//224 is how faw my chosen gesture animation (taunt) is from 300
	//which is i guess where it's "supposed" to be (even tho it's not lol
	//but somehow 300 is the magic number)

	animations = ci->animations;

	// load the file
	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( len <= 0 ) {
		return qfalse;
	}
	if ( len >= sizeof( text ) - 1 ) {
		CG_Printf( "File %s too long\n", filename );
		trap_FS_FCloseFile( f );
		return qfalse;
	}
	trap_FS_Read( text, len, f );
	text[len] = 0;
	trap_FS_FCloseFile( f );

	// parse the text
	text_p = text;
	skip = 0;	// quite the compiler warning

	ci->footsteps = FOOTSTEP_NORMAL;
	VectorClear( ci->headOffset );
	ci->gender = GENDER_MALE;
	ci->fixedlegs = qfalse;
	ci->fixedtorso = qfalse;

	// read optional parameters
	while ( 1 ) {
		prev = text_p;	// so we can unget
		token = COM_Parse( &text_p );
		if ( !token ) {
			break;
		}
		if ( !Q_stricmp( token, "footsteps" ) ) {
			token = COM_Parse( &text_p );
			if ( !token ) {
				break;
			}
			if ( !Q_stricmp( token, "default" ) || !Q_stricmp( token, "normal" ) ) {
				ci->footsteps = FOOTSTEP_NORMAL;
			} else if ( !Q_stricmp( token, "boot" ) ) {
				ci->footsteps = FOOTSTEP_BOOT;
			} else if ( !Q_stricmp( token, "flesh" ) ) {
				ci->footsteps = FOOTSTEP_FLESH;
			} else if ( !Q_stricmp( token, "mech" ) ) {
				ci->footsteps = FOOTSTEP_MECH;
			} else if ( !Q_stricmp( token, "energy" ) ) {
				ci->footsteps = FOOTSTEP_ENERGY;
			} else {
				CG_Printf( "Bad footsteps parm in %s: %s\n", filename, token );
			}
			continue;
		} else if ( !Q_stricmp( token, "headoffset" ) ) {
			for ( i = 0 ; i < 3 ; i++ ) {
				token = COM_Parse( &text_p );
				if ( !token ) {
					break;
				}
				ci->headOffset[i] = atof( token );
			}
			continue;
		} else if ( !Q_stricmp( token, "sex" ) ) {
			token = COM_Parse( &text_p );
			if ( !token ) {
				break;
			}
			if ( token[0] == 'f' || token[0] == 'F' ) {
				ci->gender = GENDER_FEMALE;
			} else if ( token[0] == 'n' || token[0] == 'N' ) {
				ci->gender = GENDER_NEUTER;
			} else {
				ci->gender = GENDER_MALE;
			}
			continue;
		} else if ( !Q_stricmp( token, "fixedlegs" ) ) {
			ci->fixedlegs = qtrue;
			continue;
		} else if ( !Q_stricmp( token, "fixedtorso" ) ) {
			ci->fixedtorso = qtrue;
			continue;
		}

		// if it is a number, start parsing animations
		if ( token[0] >= '0' && token[0] <= '9' ) {
			text_p = prev;	// unget the token
			break;
		}
		Com_Printf( "unknown token '%s' is %s\n", token, filename );
	}

	bludtemp = 0; //blud
	// read information for each frame
	for ( i = 0 ; i < MAX_ANIMATIONS ; i++ ) {
		bludtemp++;
		token = COM_Parse( &text_p );
		if ( !*token ) {
			if( i >= TORSO_GETFLAG && i <= TORSO_NEGATIVE ) {
				animations[i].firstFrame = animations[TORSO_GESTURE].firstFrame;
				animations[i].frameLerp = animations[TORSO_GESTURE].frameLerp;
				animations[i].initialLerp = animations[TORSO_GESTURE].initialLerp;
				animations[i].loopFrames = animations[TORSO_GESTURE].loopFrames;
				animations[i].numFrames = animations[TORSO_GESTURE].numFrames;
				animations[i].reversed = qfalse;
				animations[i].flipflop = qfalse;
				continue;
			}
			break;
		}
		animations[i].firstFrame = atoi( token );
		// leg only frames are adjusted to not count the upper body only frames
		if ( i == LEGS_WALKCR ) {
			if (strcmp(modelName, "orion") == 0 || strcmp(modelName, "athena") == 0) { //if this is Orion or Athena, apply weird fix (later I want to make this more sophisticated to detect ANY urt style models)
				skip = animations[LEGS_WALKCR].firstFrame - animations[TORSO_GESTURE].firstFrame - urtModelAnimOffset; //blud: - urtModelAnimOffset for weird animation fix.
			}
			else { //else this is assumed to be a q3 style model
				skip = animations[LEGS_WALKCR].firstFrame - animations[TORSO_GESTURE].firstFrame;
			}
		}
		if ( i >= LEGS_WALKCR && i<TORSO_GETFLAG) {
			animations[i].firstFrame -= skip;
		}

		token = COM_Parse( &text_p );
		if ( !*token ) {
			break;
		}
		animations[i].numFrames = atoi( token );

		animations[i].reversed = qfalse;
		animations[i].flipflop = qfalse;
		// if numFrames is negative the animation is reversed
		if (animations[i].numFrames < 0) {
			animations[i].numFrames = -animations[i].numFrames;
			animations[i].reversed = qtrue;
		}

		token = COM_Parse( &text_p );
		if ( !*token ) {
			break;
		}
		animations[i].loopFrames = atoi( token );

		token = COM_Parse( &text_p );
		if ( !*token ) {
			break;
		}
		fps = atof( token );
		if ( fps == 0 ) {
			fps = 1;
		}
		animations[i].frameLerp = 1000 / fps;
		animations[i].initialLerp = 1000 / fps;
	}

	if ( i != MAX_ANIMATIONS ) {
		CG_Printf( "Error parsing animation file: %s\n", filename );
		return qfalse;
	}

	//blud, let's try to print out the entire animation sequence BLUD DEBUG...
	//CG_Printf( "NUMBER OF ANIMATIONS: %d\n", bludtemp );
	//for ( j = 0 ; j < bludtemp; j++ ) {
	//	CG_Printf( "ANIMATION #%d | FF=%d\n", j, animations[j].firstFrame);
	//}

	// crouch backward animation
	memcpy(&animations[LEGS_BACKCR], &animations[LEGS_WALKCR], sizeof(animation_t));
	animations[LEGS_BACKCR].reversed = qtrue;
	// walk backward animation
	memcpy(&animations[LEGS_BACKWALK], &animations[LEGS_WALK], sizeof(animation_t));
	animations[LEGS_BACKWALK].reversed = qtrue;
	// flag moving fast
	animations[FLAG_RUN].firstFrame = 0;
	animations[FLAG_RUN].numFrames = 16;
	animations[FLAG_RUN].loopFrames = 16;
	animations[FLAG_RUN].frameLerp = 1000 / 15;
	animations[FLAG_RUN].initialLerp = 1000 / 15;
	animations[FLAG_RUN].reversed = qfalse;
	// flag not moving or moving slowly
	animations[FLAG_STAND].firstFrame = 16;
	animations[FLAG_STAND].numFrames = 5;
	animations[FLAG_STAND].loopFrames = 0;
	animations[FLAG_STAND].frameLerp = 1000 / 20;
	animations[FLAG_STAND].initialLerp = 1000 / 20;
	animations[FLAG_STAND].reversed = qfalse;
	// flag speeding up
	animations[FLAG_STAND2RUN].firstFrame = 16;
	animations[FLAG_STAND2RUN].numFrames = 5;
	animations[FLAG_STAND2RUN].loopFrames = 1;
	animations[FLAG_STAND2RUN].frameLerp = 1000 / 15;
	animations[FLAG_STAND2RUN].initialLerp = 1000 / 15;
	animations[FLAG_STAND2RUN].reversed = qtrue;
	//
	// new anims changes
	//
//	animations[TORSO_GETFLAG].flipflop = qtrue;
//	animations[TORSO_GUARDBASE].flipflop = qtrue;
//	animations[TORSO_PATROL].flipflop = qtrue;
//	animations[TORSO_AFFIRMATIVE].flipflop = qtrue;
//	animations[TORSO_NEGATIVE].flipflop = qtrue;
	//
	return qtrue;
}

/*
==========================
CG_FileExists
==========================
*/
static qboolean	CG_FileExists(const char *filename) {
	int len;

	len = trap_FS_FOpenFile( filename, NULL, FS_READ );
	if (len>0) {
		return qtrue;
	}
	return qfalse;
}


/*
==========================
CG_FindClientModelFile - re-written by blud because the old one is dumb
==========================
*/
static qboolean	CG_FindClientModelFile( char *filename, int length, clientInfo_t *ci, const char *teamName, const char *modelName, const char *skinName, const char *base, const char *ext ) {

	// note: in q3a there's a lot more code here. But in my code I fixed it so that this function
	//       is always passed the correct info, and with alturt there's only 1 path possibility
	Com_sprintf( filename, length, "models/players/%s/%s_%s.%s", modelName, base, skinName, ext );

	if ( CG_FileExists( filename ) )
	{
		return qtrue;
	}
	else
	{
		if ( cgs.gametype < GT_TEAM )
		{
			//the non-team GT skin that they asked for doesn't exist so set it to a valid one (default, which must exist)
			Com_sprintf( filename, length, "models/players/%s/%s_default.%s", modelName, base, ext );
			Com_sprintf( ci->skinName, 8, "default" );

			//we'll check if it exists just in case even though it pretty much must exist.
			if ( CG_FileExists( filename ) )
			{
				return qtrue;
			}
			else
			{
				CG_Printf( "Error: default skin does not exist. That should never happen!\n" );
				CG_Printf( "modelName: %s, base: %s, ext: %s.\n", modelName, base, ext);
				return qfalse;
			}
		}
		else
		{
			return qfalse;
		}
	}
}


/*
==========================
CG_FindClientHeadFile - blud re-writing this one too
==========================
*/
static qboolean	CG_FindClientHeadFile( char *filename, int length, clientInfo_t *ci, const char *teamName, const char *headModelName, const char *headSkinName, const char *base, const char *ext ) {

	// note: in q3a there's a lot more code here. But in my code I fixed it so that this function
	//       is always passed the correct info, and with alturt there's only 1 path possibility
	Com_sprintf( filename, length, "models/players/%s/%s_%s.%s", headModelName, base, headSkinName, ext );


	if ( CG_FileExists( filename ) )
	{
		return qtrue;
	}
	else
	{
		if ( cgs.gametype < GT_TEAM )
		{
			//the non-team GT skin that they asked for doesn't exist so set it to a valid one (default, which must exist)
			Com_sprintf( filename, length, "models/players/%s/%s.%s", headModelName, base, ext );
			Com_sprintf( ci->headSkinName, 8, "default" );

			//we'll check if it exists just in case even though it pretty much must exist.
			if ( CG_FileExists( filename ) )
			{
				return qtrue;
			}
			else
			{
				CG_Printf( "Error: default headskin does not exist. That should never happen!\n" );
				return qfalse;
			}
		}
		else
		{
			return qfalse;
		}
	}
}



/*
==========================
CG_RegisterClientSkin
==========================
*/
static qboolean	CG_RegisterClientSkin( clientInfo_t *ci, const char *teamName, const char *modelName, const char *skinName, const char *headModelName, const char *headSkinName ) {
	char filename[MAX_QPATH];



	/*
	Com_sprintf( filename, sizeof( filename ), "models/players/%s/%slower_%s.skin", modelName, teamName, skinName );
	ci->legsSkin = trap_R_RegisterSkin( filename );
	if (!ci->legsSkin) {
		Com_sprintf( filename, sizeof( filename ), "models/players/characters/%s/%slower_%s.skin", modelName, teamName, skinName );
		ci->legsSkin = trap_R_RegisterSkin( filename );
		if (!ci->legsSkin) {
			Com_Printf( "Leg skin load failure: %s\n", filename );
		}
	}


	Com_sprintf( filename, sizeof( filename ), "models/players/%s/%supper_%s.skin", modelName, teamName, skinName );
	ci->torsoSkin = trap_R_RegisterSkin( filename );
	if (!ci->torsoSkin) {
		Com_sprintf( filename, sizeof( filename ), "models/players/characters/%s/%supper_%s.skin", modelName, teamName, skinName );
		ci->torsoSkin = trap_R_RegisterSkin( filename );
		if (!ci->torsoSkin) {
			Com_Printf( "Torso skin load failure: %s\n", filename );
		}
	}
	*/
	if ( CG_FindClientModelFile( filename, sizeof(filename), ci, teamName, modelName, skinName, "lower", "skin" ) ) {
		ci->legsSkin = trap_R_RegisterSkin( filename );
	}
	if (!ci->legsSkin) {
		Com_Printf( "Leg skin load failure: %s\n", filename );
	}

	if ( CG_FindClientModelFile( filename, sizeof(filename), ci, teamName, modelName, skinName, "upper", "skin" ) ) {
		ci->torsoSkin = trap_R_RegisterSkin( filename );
	}
	if (!ci->torsoSkin) {
		Com_Printf( "Torso skin load failure: %s\n", filename );
	}

	if ( CG_FindClientHeadFile( filename, sizeof(filename), ci, teamName, modelName, skinName, "head", "skin" ) ) {
		ci->headSkin = trap_R_RegisterSkin( filename );
	}
	if (!ci->headSkin) {
		Com_Printf( "Head skin load failure: %s\n", filename );
	}


        if ( CG_FindClientModelFile( filename, sizeof(filename), ci, teamName, modelName, skinName, "vest", "skin" ) ) {
        ci->vestSkin = trap_R_RegisterSkin( filename );
        //CG_Printf( "filename for vestSkin = %s\n", filename  ); //xamis debug
        }
        if (!ci->vestSkin) {
          Com_Printf( "Vest skin load failure: %s\n", filename );
        }

	// if any skins failed to load
        if ( !ci->legsSkin ||  !ci->headSkin || !ci->torsoSkin ||!ci->vestSkin ) {
		return qfalse;
	}
	return qtrue;
}

/*
==========================
CG_RegisterClientModelname
==========================
*/
static qboolean CG_RegisterClientModelname( clientInfo_t *ci, const char *modelName, const char *skinName, const char *headModelName, const char *headSkinName, const char *teamName ) {
	char	filename[MAX_QPATH*2];
	const char		*headName;
	char newTeamName[MAX_QPATH*2];



	if ( headModelName[0] == '\0' ) {
		headName = modelName;
	}
	else {
		headName = headModelName;
	}
	Com_sprintf( filename, sizeof( filename ), "models/players/%s/lower.md3", modelName );
	ci->legsModel = trap_R_RegisterModel( filename );
	if ( !ci->legsModel ) {
		Com_sprintf( filename, sizeof( filename ), "models/players/characters/%s/lower.md3", modelName );
		ci->legsModel = trap_R_RegisterModel( filename );
		if ( !ci->legsModel ) {
			Com_Printf( "Failed to load model file %s\n", filename );
			return qfalse;
		}
	}

	Com_sprintf( filename, sizeof( filename ), "models/players/%s/upper.md3", modelName );
	ci->torsoModel = trap_R_RegisterModel( filename );
	if ( !ci->torsoModel ) {
		Com_sprintf( filename, sizeof( filename ), "models/players/characters/%s/upper.md3", modelName );
		ci->torsoModel = trap_R_RegisterModel( filename );
		if ( !ci->torsoModel ) {
			Com_Printf( "Failed to load model file %s\n", filename );
			return qfalse;
		}
	}

        Com_sprintf( filename, sizeof( filename ), "models/players/%s/vesttorso.md3", modelName );
        ci->vestModel = trap_R_RegisterModel( filename );
        if ( !ci->vestModel ) {
          Com_sprintf( filename, sizeof( filename ), "models/players/characters/%s/vesttorso.md3", modelName );
          ci->vestModel = trap_R_RegisterModel( filename );
          if ( !ci->vestModel ) {
            Com_Printf( "Failed to load model file %s\n", filename );
            return qfalse;
          }
        }



	if( headName[0] == '*' ) {
		Com_sprintf( filename, sizeof( filename ), "models/players/heads/%s/%s.md3", &headModelName[1], &headModelName[1] );
	}
	else {
		Com_sprintf( filename, sizeof( filename ), "models/players/%s/head.md3", headName );
	}
	ci->headModel = trap_R_RegisterModel( filename );
	// if the head model could not be found and we didn't load from the heads folder try to load from there
	if ( !ci->headModel && headName[0] != '*' ) {
		Com_sprintf( filename, sizeof( filename ), "models/players/heads/%s/%s.md3", headModelName, headModelName );
		ci->headModel = trap_R_RegisterModel( filename );
	}
	if ( !ci->headModel ) {
		Com_Printf( "Failed to load model file %s\n", filename );
		return qfalse;
	}


        Com_sprintf( filename, sizeof( filename ), "models/players/%s/helmet.md3", modelName );
        //CG_Printf("models/players/%s/helmet.md3\n", modelName); //xamis debug
        ci->helmetModel = trap_R_RegisterModel( filename );
        if ( !ci->helmetModel ) {
          CG_Printf("!ci->helmetModel\n");
          Com_sprintf( filename, sizeof( filename ), "models/players/characters/%s/helmet.md3", modelName );
          ci->helmetModel = trap_R_RegisterModel( filename );
          if ( !ci->helmetModel ) {
            Com_Printf( "Failed to load model file %s\n", filename );
            return qfalse;
          }
        }


        Com_sprintf( filename, sizeof( filename ), "models/players/%s/nvg.md3", modelName );
        ci->nvgModel = trap_R_RegisterModel( filename );
        if ( !ci->nvgModel ) {
          CG_Printf("!ci->nvgModel\n");
		  //what is this line, it cannot be right. I just copied this whole block from above
          Com_sprintf( filename, sizeof( filename ), "models/players/characters/%s/nvg.md3", modelName );
          ci->nvgModel = trap_R_RegisterModel( filename );
          if ( !ci->nvgModel ) {
            Com_Printf( "Failed to load model file %s\n", filename );
            return qfalse;
          }
        }

        Com_sprintf( filename, sizeof( filename ), "models/players/gear/backpack.md3");
        ci->medkitModel = trap_R_RegisterModel( filename );
        if ( !ci->medkitModel ) {
          CG_Printf("!ci->medkitModel\n");
		  //what is this line, it cannot be right. I just copied this whole block from above
          Com_sprintf( filename, sizeof( filename ), "models/players/characters/%s/backpack.md3", modelName );
          ci->medkitModel = trap_R_RegisterModel( filename );
          if ( !ci->medkitModel ) {
            Com_Printf( "Failed to load model file %s\n", filename );
            return qfalse;
          }
        }


	// if any skins failed to load, return failure
	if ( !CG_RegisterClientSkin( ci, teamName, modelName, skinName, modelName, skinName ) ) {
		if ( teamName && *teamName) {
			Com_Printf( "Failed to load skin file: %s : %s : %s, %s : %s\n", teamName, modelName, skinName, modelName, skinName );
			if( ci->team == TEAM_BLUE ) {
				Com_sprintf(newTeamName, sizeof(newTeamName), "%s/", DEFAULT_BLUETEAM_NAME);
			}
			else {
				Com_sprintf(newTeamName, sizeof(newTeamName), "%s/", DEFAULT_REDTEAM_NAME);
			}
			if ( !CG_RegisterClientSkin( ci, newTeamName, modelName, skinName, modelName, skinName ) ) {
				Com_Printf( "Failed to load skin file: %s : %s : %s, %s : %s\n", newTeamName, modelName, skinName, modelName, skinName );
				return qfalse;
			}
		} else {
			Com_Printf( "Failed to load skin file: %s : %s, %s : %s\n", modelName, skinName, modelName, skinName );
			return qfalse;
		}
	}

	// load the animations
	Com_sprintf( filename, sizeof( filename ), "models/players/%s/animation.cfg", modelName );
	if ( !CG_ParseAnimationFile( filename, ci, modelName ) ) { //blud added modelName arg
		Com_sprintf( filename, sizeof( filename ), "models/players/characters/%s/animation.cfg", modelName );
		if ( !CG_ParseAnimationFile( filename, ci, modelName ) ) { //blud added modelName arg
			Com_Printf( "Failed to load animation file %s\n", filename );
			return qfalse;
		}
	}

	//blud. This WAS: if ( CG_FindClientHeadFile( filename, sizeof(filename), ci, teamName, headName, headSkinName, "icon", "skin" ) ) {
	//this is kind of stupid code anyways but I'm just going to leave it.
	if ( CG_FindClientHeadFile( filename, sizeof(filename), ci, teamName, modelName, skinName, "icon", "tga" ) ) {
		ci->modelIcon = trap_R_RegisterShaderNoMip( filename );
	}
	else if ( CG_FindClientHeadFile( filename, sizeof(filename), ci, teamName, modelName, skinName, "icon", "tga" ) ) {
		ci->modelIcon = trap_R_RegisterShaderNoMip( filename );
	}

	if ( !ci->modelIcon ) {
		return qfalse;
	}

	return qtrue;
}

/*
====================
CG_ColorFromString
====================
*/
static void CG_ColorFromString( const char *v, vec3_t color ) {
	int val;

	VectorClear( color );

	val = atoi( v );

	if ( val < 1 || val > 7 ) {
		VectorSet( color, 1, 1, 1 );
		return;
	}

	if ( val & 1 ) {
		color[2] = 1.0f;
	}
	if ( val & 2 ) {
		color[1] = 1.0f;
	}
	if ( val & 4 ) {
		color[0] = 1.0f;
	}
}

/*
===================
CG_LoadClientInfo

Load it now, taking the disk hits.
This will usually be deferred to a safe time
===================
*/
static void CG_LoadClientInfo( int clientNum, clientInfo_t *ci ) {
	const char	*dir, *fallback;
	int			i, modelloaded;
	const char	*s;
	char		teamname[MAX_QPATH];

	teamname[0] = 0;
	modelloaded = qtrue;
        if ( !CG_RegisterClientModelname( ci, CG_GetPlayerModelName(ci), ci->skinName, CG_GetPlayerModelName(ci), ci->headSkinName, teamname ) ) {
		if ( cg_buildScript.integer ) {
                  CG_Error( "CG_RegisterClientModelname( %s, %s, %s, %s %s ) failed", CG_GetPlayerModelName(ci), ci->skinName, ci->headModelName, ci->headSkinName, teamname );
		}

		// fall back to default team name
		if( cgs.gametype >= GT_TEAM) {
			// keep skin name
			if( ci->team == TEAM_BLUE || ci->team == TEAM_BLUE_SPECTATOR) {
				Q_strncpyz(teamname, DEFAULT_BLUETEAM_NAME, sizeof(teamname) );
			} else {
				Q_strncpyz(teamname, DEFAULT_REDTEAM_NAME, sizeof(teamname) );
			}
			if ( !CG_RegisterClientModelname( ci, DEFAULT_TEAM_MODEL, ci->skinName, DEFAULT_TEAM_HEAD, ci->skinName, teamname ) ) {
				CG_Error( "DEFAULT_TEAM_MODEL / skin (%s/%s) failed to register", DEFAULT_TEAM_MODEL, ci->skinName );
			}
		} else {
			if ( !CG_RegisterClientModelname( ci, DEFAULT_MODEL, "default", DEFAULT_MODEL, "default", teamname ) ) {
				CG_Error( "DEFAULT_MODEL (%s) failed to register", DEFAULT_MODEL );
			}
		}
		modelloaded = qfalse;
	}

	ci->newAnims = qfalse;
	if ( ci->torsoModel ) {
		orientation_t tag;
		// if the torso model has the "tag_flag"
		if ( trap_R_LerpTag( &tag, ci->torsoModel, 0, 0, 1, "tag_flag" ) ) {
			ci->newAnims = qtrue;
		}
	}
	//blud trying to fix invisible vest torso (whole if block is new, copied from torso above)
	if ( ci->vestModel ) {
		orientation_t tag;
		// if the vest torso model has the "tag_flag"
		if ( trap_R_LerpTag( &tag, ci->vestModel, 0, 0, 1, "tag_flag" ) ) {
			ci->newAnims = qtrue;
		}
	}


	// sounds
	dir = ci->modelName;
	//bludshot: changing orion or athena to male or female, because urt stores its sounds there contrary
	//to q3 which stores them in the modelname dir
	if (strcmp(dir, "orion") == 0){ dir = "male"; } //blud
	else if (strcmp(dir, "athena") == 0){ dir = "female"; } //blud
	//else who cares I guess

	fallback = (cgs.gametype >= GT_TEAM) ? DEFAULT_TEAM_MODEL : DEFAULT_MODEL;

	for ( i = 0 ; i < MAX_CUSTOM_SOUNDS ; i++ ) {
		s = cg_customSoundNames[i];
		if ( !s ) {
			break;
		}
		ci->sounds[i] = 0;
		// if the model didn't load use the sounds of the default model
		if (modelloaded) {
			ci->sounds[i] = trap_S_RegisterSound( va("sound/player/%s/%s", dir, s + 1), qfalse );
		}
		if ( !ci->sounds[i] ) {
			ci->sounds[i] = trap_S_RegisterSound( va("sound/player/%s/%s", "male", s + 1), qfalse ); //blud changed this to male to be the default sounds.
		}
	}

	ci->deferred = qfalse;

	// reset any existing players and bodies, because they might be in bad
	// frames for this new model
	for ( i = 0 ; i < MAX_GENTITIES ; i++ ) {
		if ( cg_entities[i].currentState.clientNum == clientNum
			&& cg_entities[i].currentState.eType == ET_PLAYER ) {
			CG_ResetPlayerEntity( &cg_entities[i] );
		}
	}
}

/*
======================
CG_CopyClientInfoModel
======================
*/
static void CG_CopyClientInfoModel( clientInfo_t *from, clientInfo_t *to ) {


	VectorCopy( from->headOffset, to->headOffset );
	to->footsteps = from->footsteps;
	to->gender = from->gender;

	to->legsModel = from->legsModel;
	to->legsSkin = from->legsSkin;
	to->torsoModel = from->torsoModel;
	to->torsoSkin = from->torsoSkin;
	to->vestModel = from->vestModel; //blud fix torso
	to->vestSkin = from->vestSkin; //blud fix torso
	to->headModel = from->headModel;
	to->headSkin = from->headSkin;
	to->modelIcon = from->modelIcon;
        to->helmetModel = from->helmetModel;
        to->helmetSkin = from->helmetSkin;
		to->nvgModel = from->nvgModel;
        to->nvgSkin = from->nvgSkin;
		to->medkitModel = from->medkitModel;
        to->medkitSkin = from->medkitSkin;


	to->newAnims = from->newAnims;

	memcpy( to->animations, from->animations, sizeof( to->animations ) );
	memcpy( to->sounds, from->sounds, sizeof( to->sounds ) );
}

/*
======================
CG_ScanForExistingClientInfo
======================
*/
static qboolean CG_ScanForExistingClientInfo( clientInfo_t *ci ) {
	int		i;
	clientInfo_t	*match;

	for ( i = 0 ; i < cgs.maxclients ; i++ ) {
		match = &cgs.clientinfo[ i ];
		if ( !match->infoValid ) {
			continue;
		}
		if ( match->deferred ) {
			continue;
		}
		if ( !Q_stricmp( ci->modelName, match->modelName )
			&& !Q_stricmp( ci->skinName, match->skinName )
			&& !Q_stricmp( ci->headModelName, match->headModelName )
			&& !Q_stricmp( ci->headSkinName, match->headSkinName )
			&& !Q_stricmp( ci->blueTeam, match->blueTeam )
			&& !Q_stricmp( ci->redTeam, match->redTeam )
			&& (cgs.gametype < GT_TEAM || (ci->team == match->team && ((ci->team == TEAM_BLUE && (ci->raceblue == match->raceblue)) || (ci->team == TEAM_RED && (ci->racered == match->racered)))) ) ) {
				//note: the above means if it's not a team gt then (if all the stuff above matches
				//		we have a match. But if it's a team game then the team must match and
				//		the appropriate race must match
				// this clientinfo is identical, so use it's handles
				ci->deferred = qfalse;

				CG_CopyClientInfoModel( match, ci );

				return qtrue;
		}
	}

	// nothing matches, so defer the load
	return qfalse;
}

/*
======================
CG_SetDeferredClientInfo

We aren't going to load it now, so grab some other
client's info to use until we have some spare time.
======================
*/
static void CG_SetDeferredClientInfo( int clientNum, clientInfo_t *ci ) {
	int		i;
	clientInfo_t	*match;
	

	// if someone else is already the same models and skins we
	// can just load the client info
	for ( i = 0 ; i < cgs.maxclients ; i++ ) {
		match = &cgs.clientinfo[ i ];
		if ( !match->infoValid || match->deferred ) {
			continue;
		}
		//if this is a non-team GT //blud: I tweaked this code a bit to consider team vs non-team gts
		if ( cgs.gametype < GT_TEAM )
		{
			if ( Q_stricmp( ci->skinName, match->skinName ) ||
				Q_stricmp( ci->modelName, match->modelName ))
			{
				//blud: This means if either the skin or model don't match then this isn't a good match so 
				//continue and try the next client in the for loop.
				continue;
			}
		}
		else //this is a team GT
		{	
			if (ci->team == TEAM_BLUE|| ci->team == TEAM_BLUE_SPECTATOR) //if it's blue team we compare raceblue (and team because we always compare team)
			{
				if ( ci->raceblue != match->raceblue && ci->team != match->team)
				{
					continue; //if they aren't the same race AND team (if either isn't the same)
								//then this isn't a match either so continue up to the start of the 
								//for loop to look at the next client
				}
			}
			else if (ci->team == TEAM_RED|| ci->team == TEAM_RED_SPECTATOR)
			{
				if ( ci->racered != match->racered && ci->team != match->team)
				{
					continue;
				}
			}
		}

		// just load the real info cause it uses the same models and skins
		CG_LoadClientInfo( clientNum, ci );
		return;
	}

	//now if we did not find a match in the for loop above then we never called CG_LoadClientInfo above

	//below we seem to be trying again to find a match (weird), but then the useful thing is that this time
	//if we don't find a match, then we do CG_LoadClientInfo( clientNum, ci ) to make sure to fix the team
	//skin right away and NOT defer it (since You can't afford to have a red team guy look blue for any
	//amount of time.

	// if we are in teamplay, only grab a model if the skin is correct
	if ( cgs.gametype >= GT_TEAM ) {
		for ( i = 0 ; i < cgs.maxclients ; i++ ) {
			match = &cgs.clientinfo[ i ];
			if ( !match->infoValid || match->deferred ) {
				continue;
			}
			if (ci->team == TEAM_BLUE || ci->team == TEAM_BLUE_SPECTATOR) //if it's blue team we compare raceblue (and team because we always compare team)
			{
				if ( ci->raceblue != match->raceblue && ci->team != match->team)
				{
					continue; //if they aren't the same race AND team (if either isn't the same)
								//then this isn't a match either so continue up to the start of the 
								//for loop to look at the next client
				}
			}
			else if (ci->team == TEAM_RED|| ci->team == TEAM_RED_SPECTATOR)
			{
				if ( ci->racered != match->racered && ci->team != match->team)
				{
					continue;
				}
			}
			ci->deferred = qtrue;
			CG_CopyClientInfoModel( match, ci );
			return;
		}
		// load the full model, because we don't ever want to show
		// an improper team skin.  This will cause a hitch for the first
		// player, when the second enters.  Combat shouldn't be going on
		// yet, so it shouldn't matter
		CG_LoadClientInfo( clientNum, ci );
		return;
	}

	// find the first valid clientinfo and grab its stuff
	for ( i = 0 ; i < cgs.maxclients ; i++ ) {
		match = &cgs.clientinfo[ i ];
		if ( !match->infoValid ) {
			continue;
		}

		ci->deferred = qtrue;
		CG_CopyClientInfoModel( match, ci );
		return;
	}

	// we should never get here...
	CG_Printf( "CG_SetDeferredClientInfo: no valid clients!\n" );

	//blud: wow I really don't get this last for above here.

	CG_LoadClientInfo( clientNum, ci );
}


/*
======================
CG_NewClientInfo
======================
*/
void CG_NewClientInfo( int clientNum ) {
	clientInfo_t *ci;
	clientInfo_t newInfo;
	const char	*configstring;
	const char	*v;
	int			race;

	ci = &cgs.clientinfo[clientNum];

	configstring = CG_ConfigString( clientNum + CS_PLAYERS );
	if ( !configstring[0] ) {
		memset( ci, 0, sizeof( *ci ) );
		return;		// player just left
	}

	//CG_Printf( "configstring: **%s**", configstring ); //blud debug

	// build into a temp buffer so the defer checks can use
	// the old value
	memset( &newInfo, 0, sizeof( newInfo ) );

	// isolate the player's name
	v = Info_ValueForKey(configstring, "n");
	Q_strncpyz( newInfo.name, v, sizeof( newInfo.name ) );

	// colors
	v = Info_ValueForKey( configstring, "c1" );
	CG_ColorFromString( v, newInfo.color1 );

	v = Info_ValueForKey( configstring, "c2" );
	CG_ColorFromString( v, newInfo.color2 );

	// bot skill
	v = Info_ValueForKey( configstring, "skill" );
	newInfo.botSkill = atoi( v );

	// handicap
	v = Info_ValueForKey( configstring, "hc" );
	newInfo.handicap = atoi( v );

	// wins
	v = Info_ValueForKey( configstring, "w" );
	newInfo.wins = atoi( v );

	// losses
	v = Info_ValueForKey( configstring, "l" );
	newInfo.losses = atoi( v );

	// team
	v = Info_ValueForKey( configstring, "t" );
	newInfo.team = atoi( v );

	// team task
	v = Info_ValueForKey( configstring, "tt" );
	newInfo.teamTask = atoi(v);

	// team leader
	v = Info_ValueForKey( configstring, "tl" );
	newInfo.teamLeader = atoi(v);

	v = Info_ValueForKey( configstring, "g_redteam" );
	Q_strncpyz(newInfo.redTeam, v, MAX_TEAMNAME);

	v = Info_ValueForKey( configstring, "g_blueteam" );
	Q_strncpyz(newInfo.blueTeam, v, MAX_TEAMNAME);

	// model
	v = Info_ValueForKey( configstring, "model" );
	//blud: I temporarily set the modelName from model, but down below it might get changed
	//based on racered raceblue and whether it's a team game or not.
	Q_strncpyz( newInfo.modelName, v, sizeof( newInfo.modelName ) );


	// head model stuff used to be here, I got rid of it.


	// racered blud
	v = Info_ValueForKey( configstring, "rr" );
	newInfo.racered = atoi( v );


	// raceblue blud
	v = Info_ValueForKey( configstring, "rb" );
	newInfo.raceblue = atoi( v );


	// skin blud
	v = Info_ValueForKey( configstring, "skin" );
	Q_strncpyz( newInfo.skin, v, sizeof( newInfo.skin ) ); //note this doesn't set the skinName
	Q_strncpyz( newInfo.skinName, v, sizeof( newInfo.skinName ) );
	Q_strncpyz( newInfo.headSkinName, v, sizeof( newInfo.headSkinName ) );



	//blud:
	// We have set the skin and model above correctly for non-team GTs
	// Now we will re-set the skin and model correctly for team GTs if this is a team GT

	if ( cgs.gametype >= GT_TEAM )
	{
		// get the race which determines the model regardless of team
		if ( newInfo.team == TEAM_BLUE ||  newInfo.team == TEAM_BLUE_SPECTATOR)
		{
			race = newInfo.raceblue;
		}
		else //team is red
		{
			race = newInfo.racered;
		}

		// set the model
		switch( race )
		{
			case 0:
			case 1:		Q_strncpyz( newInfo.modelName, "athena", sizeof( newInfo.modelName ) );
						break;
			case 2:
			case 3:		Q_strncpyz( newInfo.modelName, "orion", sizeof( newInfo.modelName ) );
						break;
			default:	Q_strncpyz( newInfo.modelName, "orion", sizeof( newInfo.modelName ) );
						break;
		}

		// determine the skin
		if ( newInfo.team == TEAM_BLUE  || newInfo.team == TEAM_BLUE_SPECTATOR)
		{
			if ( race == 0 || race == 2 )
			{
				Q_strncpyz( newInfo.skinName, "blue", sizeof( newInfo.skinName ) );
				Q_strncpyz( newInfo.headSkinName, "blue", sizeof( newInfo.headSkinName ) );
			}
			else if ( race == 1 || race == 3 )
			{
				Q_strncpyz( newInfo.skinName, "blue2", sizeof( newInfo.skinName ) );
				Q_strncpyz( newInfo.headSkinName, "blue2", sizeof( newInfo.headSkinName ) );
			}
			else //default
			{
				Q_strncpyz( newInfo.skinName, "blue", sizeof( newInfo.skinName ) );
				Q_strncpyz( newInfo.headSkinName, "blue", sizeof( newInfo.headSkinName ) );
			}
		}
		else //team is red
		{
			if ( race == 0 || race == 2 )
			{
				Q_strncpyz( newInfo.skinName, "red", sizeof( newInfo.skinName ) );
				Q_strncpyz( newInfo.headSkinName, "red", sizeof( newInfo.headSkinName ) );
			}
			else if ( race == 1 || race == 3 )
			{
				Q_strncpyz( newInfo.skinName, "red2", sizeof( newInfo.skinName ) );
				Q_strncpyz( newInfo.headSkinName, "red2", sizeof( newInfo.headSkinName ) );
			}
			else //default
			{
				Q_strncpyz( newInfo.skinName, "red", sizeof( newInfo.skinName ) );
				Q_strncpyz( newInfo.headSkinName, "red", sizeof( newInfo.headSkinName ) );
			}
		}
	}



	// scan for an existing clientinfo that matches this modelname
	// so we can avoid loading checks if possible
	if ( !CG_ScanForExistingClientInfo( &newInfo ) ) {
		qboolean	forceDefer;

		forceDefer = trap_MemoryRemaining() < 4000000;


		// if we are defering loads, just have it pick the first valid
		if ( forceDefer || (cg_deferPlayers.integer && !cg_buildScript.integer && !cg.loading ) ) {
			// keep whatever they had if it won't violate team skins
			CG_SetDeferredClientInfo( clientNum, &newInfo );
			// if we are low on memory, leave them with this model
			if ( forceDefer ) {
				CG_Printf( "Memory is low.  Using deferred model.\n" );
				newInfo.deferred = qfalse;
			}
		} else {
			CG_LoadClientInfo( clientNum, &newInfo );
		}
	}

	// replace whatever was there with the new one
	newInfo.infoValid = qtrue;
	*ci = newInfo;
}


/*
======================
CG_LoadDeferredPlayers

Called each frame when a player is dead
and the scoreboard is up
so deferred players can be loaded
======================
*/
void CG_LoadDeferredPlayers( void ) {
	int		i;
	clientInfo_t	*ci;


	// scan for a deferred player to load
	for ( i = 0, ci = cgs.clientinfo ; i < cgs.maxclients ; i++, ci++ ) {
		if ( ci->infoValid && ci->deferred ) {
			// if we are low on memory, leave it deferred
			if ( trap_MemoryRemaining() < 4000000 ) {
				CG_Printf( "Memory is low.  Using deferred model.\n" );
				ci->deferred = qfalse;
				continue;
			}
			CG_LoadClientInfo( i, ci );
//			break;
		}
	}
}

/*
=============================================================================

PLAYER ANIMATION

=============================================================================
*/

/* [QUARANTINE] - Weapon Animations Added by Xamis
===============
CG_SetWeaponLerpFrame

may include ANIM_TOGGLEBIT
===============
*/
static void CG_SetWeaponLerpFrame( clientInfo_t *ci, lerpFrame_t *lf, int newAnimation ) {
  animation_t *anim;

  lf->animationNumber = newAnimation;
  newAnimation &= ~ANIM_TOGGLEBIT;

  if ( newAnimation < 0 || newAnimation >= MAX_WEAPON_ANIMATIONS ) {
    CG_Error( "Bad weapon animation number: %i", newAnimation );
  }

  anim = &cg_weapons[cg.snap->ps.weapon].animations[ newAnimation ];

  lf->animation = anim;
  lf->animationTime = lf->frameTime + anim->initialLerp;

  if ( cg_debugAnim.integer ) {
    CG_Printf( "Weapon Anim: %i\n", newAnimation );
  }
}
// END
/*
===============
CG_SetLerpFrameAnimation

may include ANIM_TOGGLEBIT
===============
*/
static void CG_SetLerpFrameAnimation( clientInfo_t *ci, lerpFrame_t *lf, int newAnimation ) {
	animation_t	*anim;

	lf->animationNumber = newAnimation;
	newAnimation &= ~ANIM_TOGGLEBIT;

	if ( newAnimation < 0 || newAnimation >= MAX_TOTALANIMATIONS ) {
		CG_Error( "Bad animation number: %i", newAnimation );
	}

	anim = &ci->animations[ newAnimation ];

	lf->animation = anim;
	lf->animationTime = lf->frameTime + anim->initialLerp;

	if ( cg_debugAnim.integer ) {
		CG_Printf( "Anim: %i\n", newAnimation );
	}
}

/*
===============
CG_RunLerpFrame

Sets cg.snap, cg.oldFrame, and cg.backlerp
cg.time should be between oldFrameTime and frameTime after exit
===============
*/

/*Xamis, this was:
static void CG_RunLerpFrame( clientInfo_t *ci, lerpFrame_t *lf, int newAnimation, float speedScale)
Had to add check for weapon animation.

*/
static void CG_RunLerpFrame( clientInfo_t *ci, lerpFrame_t *lf, int newAnimation, float speedScale, qboolean weaponAnim ) {
	int			f, numFrames;
	animation_t	*anim;

	// debugging tool to get no animations
	if ( cg_animSpeed.integer == 0 ) {
		lf->oldFrame = lf->frame = lf->backlerp = 0;
		return;
	}

	// see if the animation sequence is switching
// QUARANTINE - Weapon Animation
// Check and see if the animation is switching and then check to see if it is a weapon
// animation or character and call the proper lerp frame function
        if ( newAnimation != lf->animationNumber || !lf->animation ) {
          if (weaponAnim) {
            CG_SetWeaponLerpFrame( ci, lf, newAnimation );
          } else {
            CG_SetLerpFrameAnimation( ci, lf, newAnimation );
          }
        }
// END

	// if we have passed the current frame, move it to
	// oldFrame and calculate a new frame
	if ( cg.time >= lf->frameTime ) {
		lf->oldFrame = lf->frame;
		lf->oldFrameTime = lf->frameTime;

		// get the next frame based on the animation
		anim = lf->animation;
		if ( !anim->frameLerp ) {
			return;		// shouldn't happen
		}
		if ( cg.time < lf->animationTime ) {
			lf->frameTime = lf->animationTime;		// initial lerp
		} else {
			lf->frameTime = lf->oldFrameTime + anim->frameLerp;
		}
		f = ( lf->frameTime - lf->animationTime ) / anim->frameLerp;
		f *= speedScale;		// adjust for haste, etc

		numFrames = anim->numFrames;
		if (anim->flipflop) {
			numFrames *= 2;
		}
		if ( f >= numFrames ) {
			f -= numFrames;
			if ( anim->loopFrames ) {
				f %= anim->loopFrames;
				f += anim->numFrames - anim->loopFrames;
			} else {
				f = numFrames - 1;
				// the animation is stuck at the end, so it
				// can immediately transition to another sequence
				lf->frameTime = cg.time;
			}
		}
		if ( anim->reversed ) {
			lf->frame = anim->firstFrame + anim->numFrames - 1 - f;
		}
		else if (anim->flipflop && f>=anim->numFrames) {
			lf->frame = anim->firstFrame + anim->numFrames - 1 - (f%anim->numFrames);
		}
		else {
			lf->frame = anim->firstFrame + f;
		}
		if ( cg.time > lf->frameTime ) {
			lf->frameTime = cg.time;
			if ( cg_debugAnim.integer ) {
				CG_Printf( "Clamp lf->frameTime\n");
			}
		}
	}

	if ( lf->frameTime > cg.time + 200 ) {
		lf->frameTime = cg.time;
	}

	if ( lf->oldFrameTime > cg.time ) {
		lf->oldFrameTime = cg.time;
	}
	// calculate current lerp value
	if ( lf->frameTime == lf->oldFrameTime ) {
		lf->backlerp = 0;
	} else {
		lf->backlerp = 1.0 - (float)( cg.time - lf->oldFrameTime ) / ( lf->frameTime - lf->oldFrameTime );
	}
}


/*
===============
CG_ClearLerpFrame
===============
*/
static void CG_ClearLerpFrame( clientInfo_t *ci, lerpFrame_t *lf, int animationNumber ) {
	lf->frameTime = lf->oldFrameTime = cg.time;
	CG_SetLerpFrameAnimation( ci, lf, animationNumber );
	lf->oldFrame = lf->frame = lf->animation->firstFrame;
}

/* [QUARANTINE] - Weapon Animations
===============
CG_WeaponAnimation

This is called from cg_weapons.c
===============
*/
void CG_WeaponAnimation( centity_t *cent, int *weaponOld, int *weapon, float *weaponBackLerp ) {
  clientInfo_t *ci;
  int clientNum;
  entityState_t *ent;
  weaponInfo_t    *weap;
  int i;
  clientNum = cent->currentState.clientNum;

  ent = &cent->currentState;
  weap = &cg_weapons[ ent->weapon ];

  if ( cg_noPlayerAnims.integer ) {
    *weaponOld = *weapon = 0;
    return;
  }

  ci = &cgs.clientinfo[ clientNum ];
  
  // Check for sounds that should start on each frame --Xamis
  if    (  (cent->currentState.torsoAnim & ~ANIM_TOGGLEBIT ) == TORSO_RELOAD_PISTOL ||
	(cent->currentState.torsoAnim & ~ANIM_TOGGLEBIT ) == TORSO_RELOAD_RIFLE
          || ent->weapon  == WP_SR8 )   {
  for ( i = 0 ; i < 14 ; i++ ) {

  if (weap->sounds[i].type == 1 && cent->pe.weapon.frame == weap->sounds[i].startFrame  ){
      
      trap_S_StartSound( NULL, clientNum, CHAN_WEAPON,weap->sounds[i].soundPath );
      break;
 }
  }
  }
  CG_RunLerpFrame( ci, &cent->pe.weapon, cent->currentState.generic1, 1, qtrue );

// QUARANTINE - Debug - Animations
#if 0
  if(cent->pe.weapon.oldFrame || cent->pe.weapon.frame || cent->pe.weapon.backlerp) {
  CG_Printf("weaponOld: %i weaponFrame: %i weaponBack: %i\n",
                cent->pe.weapon.oldFrame, cent->pe.weapon.frame, cent->pe.weapon.backlerp);
}
#endif
 
  
  
            *weaponOld = cent->pe.weapon.oldFrame;
            *weapon = cent->pe.weapon.frame;
            *weaponBackLerp = cent->pe.weapon.backlerp;

}
// END

/*
===============
CG_PlayerAnimation
===============
*/
static void CG_PlayerAnimation( centity_t *cent, int *legsOld, int *legs, float *legsBackLerp,
						int *torsoOld, int *torso, float *torsoBackLerp ) {
	clientInfo_t	*ci;
	int				clientNum;
	float			speedScale;

	clientNum = cent->currentState.clientNum;

	if ( cg_noPlayerAnims.integer ) {
		*legsOld = *legs = *torsoOld = *torso = 0;
		return;
	}

	speedScale = 1;

	ci = &cgs.clientinfo[ clientNum ];

	// do the shuffle turn frames locally
	if ( cent->pe.legs.yawing && ( cent->currentState.legsAnim & ~ANIM_TOGGLEBIT ) == LEGS_IDLE ) {
		CG_RunLerpFrame( ci, &cent->pe.legs, LEGS_TURN, speedScale, qfalse );//Xamis, added qfalse
	} else {
          CG_RunLerpFrame( ci, &cent->pe.legs, cent->currentState.legsAnim, speedScale, qfalse  );//Xamis, added qfalse
	}

	*legsOld = cent->pe.legs.oldFrame;
	*legs = cent->pe.legs.frame;
	*legsBackLerp = cent->pe.legs.backlerp;

        CG_RunLerpFrame( ci, &cent->pe.torso, cent->currentState.torsoAnim, speedScale, qfalse  );//Xamis, added qfalse

	*torsoOld = cent->pe.torso.oldFrame;
	*torso = cent->pe.torso.frame;
	*torsoBackLerp = cent->pe.torso.backlerp;
}

/*
=============================================================================

PLAYER ANGLES

=============================================================================
*/

/*
==================
CG_SwingAngles
==================
*/
static void CG_SwingAngles( float destination, float swingTolerance, float clampTolerance,
					float speed, float *angle, qboolean *swinging ) {
	float	swing;
	float	move;
	float	scale;

	if ( !*swinging ) {
		// see if a swing should be started
		swing = AngleSubtract( *angle, destination );
		if ( swing > swingTolerance || swing < -swingTolerance ) {
			*swinging = qtrue;
		}
	}

	if ( !*swinging ) {
		return;
	}

	// modify the speed depending on the delta
	// so it doesn't seem so linear
	swing = AngleSubtract( destination, *angle );
	scale = fabs( swing );
	if ( scale < swingTolerance * 0.5 ) {
		scale = 0.5;
	} else if ( scale < swingTolerance ) {
		scale = 1.0;
	} else {
		scale = 2.0;
	}

	// swing towards the destination angle
	if ( swing >= 0 ) {
		move = cg.frametime * scale * speed;
		if ( move >= swing ) {
			move = swing;
			*swinging = qfalse;
		}
		*angle = AngleMod( *angle + move );
	} else if ( swing < 0 ) {
		move = cg.frametime * scale * -speed;
		if ( move <= swing ) {
			move = swing;
			*swinging = qfalse;
		}
		*angle = AngleMod( *angle + move );
	}

	// clamp to no more than tolerance
	swing = AngleSubtract( destination, *angle );
	if ( swing > clampTolerance ) {
		*angle = AngleMod( destination - (clampTolerance - 1) );
	} else if ( swing < -clampTolerance ) {
		*angle = AngleMod( destination + (clampTolerance - 1) );
	}
}

/*
=================
CG_AddPainTwitch
=================
*/
static void CG_AddPainTwitch( centity_t *cent, vec3_t torsoAngles ) {
	int		t;
	float	f;

	t = cg.time - cent->pe.painTime;
	if ( t >= PAIN_TWITCH_TIME ) {
		return;
	}

	f = 1.0 - (float)t / PAIN_TWITCH_TIME;

	if ( cent->pe.painDirection ) {
		torsoAngles[ROLL] += 20 * f;
	} else {
		torsoAngles[ROLL] -= 20 * f;
	}
}


/*
===============
CG_PlayerAngles

Handles seperate torso motion

  legs pivot based on direction of movement

  head always looks exactly at cent->lerpAngles

  if motion < 20 degrees, show in head only
  if < 45 degrees, also show in torso
===============
*/
/*
===============
CG_PlayerAngles

Handles seperate torso motion

  legs pivot based on direction of movement

  head always looks exactly at cent->lerpAngles

  if motion < 20 degrees, show in head only
  if < 45 degrees, also show in torso
===============
*/
static void CG_PlayerAngles( centity_t *cent, vec3_t srcAngles,
                             vec3_t legs[ 3 ], vec3_t torso[ 3 ], vec3_t head[ 3 ] )
{
	vec3_t		legsAngles, torsoAngles, headAngles;
	float		dest;
	static	int	movementOffsets[8] = { 0, 22, 45, -22, 0, 22, -45, -22 };
	vec3_t		velocity;
	float		speed;
	int			dir, clientNum;
	clientInfo_t	*ci;

  VectorCopy( srcAngles, headAngles );
	headAngles[YAW] = AngleMod( headAngles[YAW] );
	VectorClear( legsAngles );
	VectorClear( torsoAngles );

	// --------- yaw -------------

	// allow yaw to drift a bit
  if( ( cent->currentState.legsAnim & ~ANIM_TOGGLEBIT ) != LEGS_IDLE ||
      ( cent->currentState.torsoAnim & ~ANIM_TOGGLEBIT ) != TORSO_STAND  )
  {
		// if not standing still, always point all in the same direction
		cent->pe.torso.yawing = qtrue;	// always center
		cent->pe.torso.pitching = qtrue;	// always center
		cent->pe.legs.yawing = qtrue;	// always center
	}

	// adjust legs for movement dir
	if ( cent->currentState.eFlags & EF_DEAD ) {
		// don't let dead bodies twitch
		dir = 0;
		}
  else
  {
    // did use angles2.. now uses time2.. looks a bit funny but time2 isn't used othwise
    dir = cent->currentState.time2;
    if( dir < 0 || dir > 7 )
      CG_Error( "Bad player movement angle" );
	}
	legsAngles[YAW] = headAngles[YAW] + movementOffsets[ dir ];
	torsoAngles[YAW] = headAngles[YAW] + 0.25 * movementOffsets[ dir ];

	// torso
  if( cent->currentState.eFlags & EF_DEAD )
  {
    CG_SwingAngles( torsoAngles[ YAW ], 0, 0, cg_swingSpeed.value,
      &cent->pe.torso.yawAngle, &cent->pe.torso.yawing );
    CG_SwingAngles( legsAngles[ YAW ], 0, 0, cg_swingSpeed.value,
      &cent->pe.legs.yawAngle, &cent->pe.legs.yawing );
  }
  else
  {
    CG_SwingAngles( torsoAngles[ YAW ], 25, 90, cg_swingSpeed.value,
      &cent->pe.torso.yawAngle, &cent->pe.torso.yawing );
    CG_SwingAngles( legsAngles[ YAW ], 40, 90, cg_swingSpeed.value,
      &cent->pe.legs.yawAngle, &cent->pe.legs.yawing );
  }

	torsoAngles[YAW] = cent->pe.torso.yawAngle;
	legsAngles[YAW] = cent->pe.legs.yawAngle;


	// --------- pitch -------------

	// only show a fraction of the pitch angle in the torso
	if ( headAngles[PITCH] > 180 ) {
		dest = (-360 + headAngles[PITCH]) * 0.75f;
	} else {
		dest = headAngles[PITCH] * 0.75f;
	}
	CG_SwingAngles( dest, 15, 30, 0.1f, &cent->pe.torso.pitchAngle, &cent->pe.torso.pitching );
	torsoAngles[PITCH] = cent->pe.torso.pitchAngle;

	//
	clientNum = cent->currentState.clientNum;
	if ( clientNum >= 0 && clientNum < MAX_CLIENTS ) {
		ci = &cgs.clientinfo[ clientNum ];
		if ( ci->fixedtorso ) {
			torsoAngles[PITCH] = 0.0f;
		}
	}

	// --------- roll -------------


	// lean towards the direction of travel
	VectorCopy( cent->currentState.pos.trDelta, velocity );
	speed = VectorNormalize( velocity );
	if ( speed ) {
		vec3_t	axis[3];
		float	side;

		speed *= 0.05f;

		AnglesToAxis( legsAngles, axis );
		side = speed * DotProduct( velocity, axis[1] );
		legsAngles[ROLL] -= side;

		side = speed * DotProduct( velocity, axis[0] );
		legsAngles[PITCH] += side;
	}

	//
	clientNum = cent->currentState.clientNum;
	if ( clientNum >= 0 && clientNum < MAX_CLIENTS ) {
		ci = &cgs.clientinfo[ clientNum ];
		if ( ci->fixedlegs ) {
			legsAngles[YAW] = torsoAngles[YAW];
			legsAngles[PITCH] = 0.0f;
			legsAngles[ROLL] = 0.0f;
		}
	}

	// pain twitch
	CG_AddPainTwitch( cent, torsoAngles );

	// pull the angles back out of the hierarchial chain
	AnglesSubtract( headAngles, torsoAngles, headAngles );
	AnglesSubtract( torsoAngles, legsAngles, torsoAngles );
	AnglesToAxis( legsAngles, legs );
	AnglesToAxis( torsoAngles, torso );
	AnglesToAxis( headAngles, head );
}


//==========================================================================

/*
===============
CG_HasteTrail
===============
*/
void CG_HasteTrail( centity_t *cent ) {
	localEntity_t	*smoke;
	vec3_t			origin;
	//int				anim;

	if ( cent->trailTime > cg.time ) {
		return;
	}
	//anim = cent->pe.legs.animationNumber & ~ANIM_TOGGLEBIT;
	//if ( anim != LEGS_RUN && anim != LEGS_BACK ) {
	//	return;
	//}

	cent->trailTime += 100;
	if ( cent->trailTime < cg.time ) {
		cent->trailTime = cg.time;
	}

	VectorCopy( cent->lerpOrigin, origin );
	origin[2] -= 16;

	smoke = CG_SmokePuff( origin, vec3_origin,
				  8,
				  1, 1, 1, 1,
				  500,
				  cg.time,
				  0,
				  0,
				  cgs.media.hastePuffShader );

	// use the optimized local entity add
	smoke->leType = LE_SCALE_FADE;
}

#ifdef MISSIONPACK
/*
===============
CG_BreathPuffs
===============
*/
static void CG_BreathPuffs( centity_t *cent, refEntity_t *head) {
	clientInfo_t *ci;
	vec3_t up, origin;
	int contents;

	ci = &cgs.clientinfo[ cent->currentState.number ];

	if (!cg_enableBreath.integer) {
		return;
	}
	if ( cent->currentState.number == cg.snap->ps.clientNum && !cg.renderingThirdPerson) {
		return;
	}
	if ( cent->currentState.eFlags & EF_DEAD ) {
		return;
	}
	contents = trap_CM_PointContents( head->origin, 0 );
	if ( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) {
		return;
	}
	if ( ci->breathPuffTime > cg.time ) {
		return;
	}

	VectorSet( up, 0, 0, 8 );
	VectorMA(head->origin, 8, head->axis[0], origin);
	VectorMA(origin, -4, head->axis[2], origin);
	CG_SmokePuff( origin, up, 16, 1, 1, 1, 0.66f, 1500, cg.time, cg.time + 400, LEF_PUFF_DONT_SCALE, cgs.media.shotgunSmokePuffShader );
	ci->breathPuffTime = cg.time + 2000;
}

/*
===============
CG_DustTrail
===============
*/
static void CG_DustTrail( centity_t *cent ) {
	int				anim;
	localEntity_t	*dust;
	vec3_t end, vel;
	trace_t tr;

	if (!cg_enableDust.integer)
		return;

	if ( cent->dustTrailTime > cg.time ) {
		return;
	}

	anim = cent->pe.legs.animationNumber & ~ANIM_TOGGLEBIT;
	if ( anim != LEGS_LANDB && anim != LEGS_LAND ) {
		return;
	}

	cent->dustTrailTime += 40;
	if ( cent->dustTrailTime < cg.time ) {
		cent->dustTrailTime = cg.time;
	}

	VectorCopy(cent->currentState.pos.trBase, end);
	end[2] -= 64;
	CG_Trace( &tr, cent->currentState.pos.trBase, NULL, NULL, end, cent->currentState.number, MASK_PLAYERSOLID );

	if ( !(tr.surfaceFlags & SURF_DUST) )
		return;

	VectorCopy( cent->currentState.pos.trBase, end );
	end[2] -= 16;

	VectorSet(vel, 0, 0, -30);
	dust = CG_SmokePuff( end, vel,
				  24,
				  .8f, .8f, 0.7f, 0.33f,
				  500,
				  cg.time,
				  0,
				  0,
				  cgs.media.dustPuffShader );
}

#endif

/*
===============
CG_TrailItem
===============
*/
static void CG_TrailItem( centity_t *cent, qhandle_t hModel ) {
	refEntity_t		ent;
	vec3_t			angles;
	vec3_t			axis[3];

	VectorCopy( cent->lerpAngles, angles );
	angles[PITCH] = 0;
	angles[ROLL] = 0;
	AnglesToAxis( angles, axis );

	memset( &ent, 0, sizeof( ent ) );
	VectorMA( cent->lerpOrigin, -16, axis[0], ent.origin );
	ent.origin[2] += 16;
	angles[YAW] += 90;
	AnglesToAxis( angles, ent.axis );

	ent.hModel = hModel;
	trap_R_AddRefEntityToScene( &ent );
}


/*
===============
CG_PlayerFlag
===============
*/
static void CG_PlayerFlag( centity_t *cent, qhandle_t hSkin, refEntity_t *torso ) {
	clientInfo_t	*ci;
	refEntity_t	pole;
	refEntity_t	flag;
	vec3_t		angles, dir;
	int			legsAnim, flagAnim, updateangles;
	float		angle, d;

	// show the flag pole model
	memset( &pole, 0, sizeof(pole) );
	pole.hModel = cgs.media.flagPoleModel;
	VectorCopy( torso->lightingOrigin, pole.lightingOrigin );
	pole.shadowPlane = torso->shadowPlane;
	pole.renderfx = torso->renderfx;
	CG_PositionEntityOnTag( &pole, torso, torso->hModel, "tag_flag" );
	trap_R_AddRefEntityToScene( &pole );

	// show the flag model
	memset( &flag, 0, sizeof(flag) );
	flag.hModel = cgs.media.flagFlapModel;
	flag.customSkin = hSkin;
	VectorCopy( torso->lightingOrigin, flag.lightingOrigin );
	flag.shadowPlane = torso->shadowPlane;
	flag.renderfx = torso->renderfx;

	VectorClear(angles);

	updateangles = qfalse;
	legsAnim = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;
	if( legsAnim == LEGS_IDLE || legsAnim == LEGS_IDLECR ) {
		flagAnim = FLAG_STAND;
	} else if ( legsAnim == LEGS_WALK || legsAnim == LEGS_WALKCR ) {
		flagAnim = FLAG_STAND;
		updateangles = qtrue;
	} else {
		flagAnim = FLAG_RUN;
		updateangles = qtrue;
	}

	if ( updateangles ) {

		VectorCopy( cent->currentState.pos.trDelta, dir );
		// add gravity
		dir[2] += 100;
		VectorNormalize( dir );
		d = DotProduct(pole.axis[2], dir);
		// if there is anough movement orthogonal to the flag pole
		if (fabs(d) < 0.9) {
			//
			d = DotProduct(pole.axis[0], dir);
			if (d > 1.0f) {
				d = 1.0f;
			}
			else if (d < -1.0f) {
				d = -1.0f;
			}
			angle = acos(d);

			d = DotProduct(pole.axis[1], dir);
			if (d < 0) {
				angles[YAW] = 360 - angle * 180 / M_PI;
			}
			else {
				angles[YAW] = angle * 180 / M_PI;
			}
			if (angles[YAW] < 0)
				angles[YAW] += 360;
			if (angles[YAW] > 360)
				angles[YAW] -= 360;

			//vectoangles( cent->currentState.pos.trDelta, tmpangles );
			//angles[YAW] = tmpangles[YAW] + 45 - cent->pe.torso.yawAngle;
			// change the yaw angle
			CG_SwingAngles( angles[YAW], 25, 90, 0.15f, &cent->pe.flag.yawAngle, &cent->pe.flag.yawing );
		}

		/*
		d = DotProduct(pole.axis[2], dir);
		angle = Q_acos(d);

		d = DotProduct(pole.axis[1], dir);
		if (d < 0) {
			angle = 360 - angle * 180 / M_PI;
		}
		else {
			angle = angle * 180 / M_PI;
		}
		if (angle > 340 && angle < 20) {
			flagAnim = FLAG_RUNUP;
		}
		if (angle > 160 && angle < 200) {
			flagAnim = FLAG_RUNDOWN;
		}
		*/
	}

	// set the yaw angle
	angles[YAW] = cent->pe.flag.yawAngle;
	// lerp the flag animation frames
	ci = &cgs.clientinfo[ cent->currentState.clientNum ];
        CG_RunLerpFrame( ci, &cent->pe.flag, flagAnim, 1,qfalse  );//Xamis, added qfalse for weapon animations
	flag.oldframe = cent->pe.flag.oldFrame;
	flag.frame = cent->pe.flag.frame;
	flag.backlerp = cent->pe.flag.backlerp;

	AnglesToAxis( angles, flag.axis );
	CG_PositionRotatedEntityOnTag( &flag, &pole, pole.hModel, "tag_flag" );

	trap_R_AddRefEntityToScene( &flag );
}


#ifdef MISSIONPACK
/*
===============
CG_PlayerTokens
===============
*/
static void CG_PlayerTokens( centity_t *cent, int renderfx ) {
	int			tokens, i, j;
	float		angle;
	refEntity_t	ent;
	vec3_t		dir, origin;
	skulltrail_t *trail;
	trail = &cg.skulltrails[cent->currentState.number];
	tokens = cent->currentState.generic1;
	if ( !tokens ) {
		trail->numpositions = 0;
		return;
	}

	if ( tokens > MAX_SKULLTRAIL ) {
		tokens = MAX_SKULLTRAIL;
	}

	// add skulls if there are more than last time
	for (i = 0; i < tokens - trail->numpositions; i++) {
		for (j = trail->numpositions; j > 0; j--) {
			VectorCopy(trail->positions[j-1], trail->positions[j]);
		}
		VectorCopy(cent->lerpOrigin, trail->positions[0]);
	}
	trail->numpositions = tokens;

	// move all the skulls along the trail
	VectorCopy(cent->lerpOrigin, origin);
	for (i = 0; i < trail->numpositions; i++) {
		VectorSubtract(trail->positions[i], origin, dir);
		if (VectorNormalize(dir) > 30) {
			VectorMA(origin, 30, dir, trail->positions[i]);
		}
		VectorCopy(trail->positions[i], origin);
	}

	memset( &ent, 0, sizeof( ent ) );
	if( cgs.clientinfo[ cent->currentState.clientNum ].team == TEAM_BLUE ) {
		ent.hModel = cgs.media.redCubeModel;
	} else {
		ent.hModel = cgs.media.blueCubeModel;
	}
	ent.renderfx = renderfx;

	VectorCopy(cent->lerpOrigin, origin);
	for (i = 0; i < trail->numpositions; i++) {
		VectorSubtract(origin, trail->positions[i], ent.axis[0]);
		ent.axis[0][2] = 0;
		VectorNormalize(ent.axis[0]);
		VectorSet(ent.axis[2], 0, 0, 1);
		CrossProduct(ent.axis[0], ent.axis[2], ent.axis[1]);

		VectorCopy(trail->positions[i], ent.origin);
		angle = (((cg.time + 500 * MAX_SKULLTRAIL - 500 * i) / 16) & 255) * (M_PI * 2) / 255;
		ent.origin[2] += sin(angle) * 10;
		trap_R_AddRefEntityToScene( &ent );
		VectorCopy(trail->positions[i], origin);
	}
}
#endif


/*
===============
CG_PlayerPowerups
===============
*/
static void CG_PlayerPowerups( centity_t *cent, refEntity_t *torso ) {
	int		powerups;
	clientInfo_t	*ci;

	powerups = cent->currentState.powerups;
	if ( !powerups ) {
		return;
	}

	ci = &cgs.clientinfo[ cent->currentState.clientNum ];
	// redflag
	if ( powerups & ( 1 << PW_REDFLAG ) ) {
		if (ci->newAnims) {
			CG_PlayerFlag( cent, cgs.media.redFlagFlapSkin, torso );
		}
		else {
			CG_TrailItem( cent, cgs.media.redFlagModel );
		}
		//trap_R_AddLightToScene( cent->lerpOrigin, 200 + (rand()&31), 1.0, 0.2f, 0.2f );   //Xamis disable glow during flag cary
	}

	// blueflag
	if ( powerups & ( 1 << PW_BLUEFLAG ) ) {
		if (ci->newAnims){
			CG_PlayerFlag( cent, cgs.media.blueFlagFlapSkin, torso );
		}
		else {
			CG_TrailItem( cent, cgs.media.blueFlagModel );
		}
		//trap_R_AddLightToScene( cent->lerpOrigin, 200 + (rand()&31), 0.2f, 0.2f, 1.0 );   //Xamis disable glow during flag cary
	}

	// neutralflag
	if ( powerups & ( 1 << PW_NEUTRALFLAG ) ) {
		if (ci->newAnims) {
			CG_PlayerFlag( cent, cgs.media.neutralFlagFlapSkin, torso );
		}
		else {
			CG_TrailItem( cent, cgs.media.neutralFlagModel );
		}
		trap_R_AddLightToScene( cent->lerpOrigin, 200 + (rand()&31), 1.0, 1.0, 1.0 );
	}

	// haste leaves smoke trails
	//if ( powerups & ( 1 << PW_HASTE ) ) {
	//	CG_HasteTrail( cent );
	//}
}


/*
===============
CG_PlayerFloatSprite

Float a sprite over the player's head
===============
*/
static void CG_PlayerFloatSprite( centity_t *cent, qhandle_t shader ) {
	int				rf;
	refEntity_t		ent;

	if ( cent->currentState.number == cg.snap->ps.clientNum && !cg.renderingThirdPerson ) {
		rf = RF_THIRD_PERSON;		// only show in mirrors
	} else {
		rf = 0;
	}

	memset( &ent, 0, sizeof( ent ) );
	VectorCopy( cent->lerpOrigin, ent.origin );
	ent.origin[2] += 48;
	ent.reType = RT_SPRITE;
	ent.customShader = shader;
	ent.radius = 10;
	ent.renderfx = rf;
	ent.shaderRGBA[0] = 255;
	ent.shaderRGBA[1] = 255;
	ent.shaderRGBA[2] = 255;
	ent.shaderRGBA[3] = 255;
	trap_R_AddRefEntityToScene( &ent );
}



/*
===============
CG_PlayerSprites

Float sprites over the player's head
===============
*/
static void CG_PlayerSprites( centity_t *cent ) {
	int		team;

	if ( cent->currentState.eFlags & EF_CONNECTION ) {
		CG_PlayerFloatSprite( cent, cgs.media.connectionShader );
		return;
	}

	if ( cent->currentState.eFlags & EF_TALK ) {
		//CG_PlayerFloatSprite( cent, cgs.media.balloonShader );
		return;
	}

//	if ( cent->currentState.eFlags & EF_AWARD_IMPRESSIVE ) {
//		CG_PlayerFloatSprite( cent, cgs.media.medalImpressive );
//		return;
//	}

	if ( cent->currentState.eFlags & EF_AWARD_EXCELLENT ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalExcellent );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_GAUNTLET ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalGauntlet );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_DEFEND ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalDefend );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_ASSIST ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalAssist );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_CAP ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalCapture );
		return;
	}

	team = cgs.clientinfo[ cent->currentState.clientNum ].team;
	if ( !(cent->currentState.eFlags & EF_DEAD) &&
		cg.snap->ps.persistant[PERS_TEAM] == team &&
		cgs.gametype >= GT_TEAM) {
		if (cg_drawFriend.integer) {
			CG_PlayerFloatSprite( cent, cgs.media.friendShader );
		}
		return;
	}
}

/*
===============
CG_PlayerShadow

Returns the Z component of the surface being shadowed

  should it return a full plane instead of a Z?
===============
*/
#define	SHADOW_DISTANCE		128
static qboolean CG_PlayerShadow( centity_t *cent, float *shadowPlane ) {
	vec3_t		end, mins = {-15, -15, 0}, maxs = {15, 15, 2};
	trace_t		trace;
//	float		alpha;

	*shadowPlane = 0;

	//if ( cg_shadows.integer == 0 ) {
	//	return qfalse;
	//}

	// send a trace down from the player to the ground
	VectorCopy( cent->lerpOrigin, end );
	end[2] -= SHADOW_DISTANCE;

	trap_CM_BoxTrace( &trace, cent->lerpOrigin, end, mins, maxs, 0, MASK_PLAYERSOLID );

	// no shadow if too high
	if ( trace.fraction == 1.0 || trace.startsolid || trace.allsolid ) {
		return qfalse;
	}

	*shadowPlane = trace.endpos[2] + 1;

	//if ( cg_shadows.integer != 1 ) {	// no mark for stencil or projection shadows
		return qtrue;
	//}

	// fade the shadow out with height
//	alpha = 1.0 - trace.fraction;

	// hack / FPE - bogus planes?
	//assert( DotProduct( trace.plane.normal, trace.plane.normal ) != 0.0f )

	// add the mark as a temporary, so it goes directly to the renderer
	// without taking a spot in the cg_marks array
//	CG_ImpactMark( cgs.media.shadowMarkShader, trace.endpos, trace.plane.normal, cent->pe.legs.yawAngle, alpha,alpha,alpha,1, qfalse, 24, qtrue );

//	return qtrue;
}


/*
===============
CG_PlayerSplash

Draw a mark at the water surface
===============
*/
static void CG_PlayerSplash( centity_t *cent ) {
	vec3_t		start, end;
	trace_t		trace;
	int			contents;
	polyVert_t	verts[4];

	if ( !cg_shadows.integer ) {
		return;
	}

	VectorCopy( cent->lerpOrigin, end );
	end[2] -= 24;

	// if the feet aren't in liquid, don't make a mark
	// this won't handle moving water brushes, but they wouldn't draw right anyway...
	contents = trap_CM_PointContents( end, 0 );
	if ( !( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) ) {
		return;
	}

	VectorCopy( cent->lerpOrigin, start );
	start[2] += 32;

	// if the head isn't out of liquid, don't make a mark
	contents = trap_CM_PointContents( start, 0 );
	if ( contents & ( CONTENTS_SOLID | CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) {
		return;
	}

	// trace down to find the surface
	trap_CM_BoxTrace( &trace, start, end, NULL, NULL, 0, ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) );

	if ( trace.fraction == 1.0 ) {
		return;
	}

	// create a mark polygon
	VectorCopy( trace.endpos, verts[0].xyz );
	verts[0].xyz[0] -= 32;
	verts[0].xyz[1] -= 32;
	verts[0].st[0] = 0;
	verts[0].st[1] = 0;
	verts[0].modulate[0] = 255;
	verts[0].modulate[1] = 255;
	verts[0].modulate[2] = 255;
	verts[0].modulate[3] = 255;

	VectorCopy( trace.endpos, verts[1].xyz );
	verts[1].xyz[0] -= 32;
	verts[1].xyz[1] += 32;
	verts[1].st[0] = 0;
	verts[1].st[1] = 1;
	verts[1].modulate[0] = 255;
	verts[1].modulate[1] = 255;
	verts[1].modulate[2] = 255;
	verts[1].modulate[3] = 255;

	VectorCopy( trace.endpos, verts[2].xyz );
	verts[2].xyz[0] += 32;
	verts[2].xyz[1] += 32;
	verts[2].st[0] = 1;
	verts[2].st[1] = 1;
	verts[2].modulate[0] = 255;
	verts[2].modulate[1] = 255;
	verts[2].modulate[2] = 255;
	verts[2].modulate[3] = 255;

	VectorCopy( trace.endpos, verts[3].xyz );
	verts[3].xyz[0] += 32;
	verts[3].xyz[1] -= 32;
	verts[3].st[0] = 1;
	verts[3].st[1] = 0;
	verts[3].modulate[0] = 255;
	verts[3].modulate[1] = 255;
	verts[3].modulate[2] = 255;
	verts[3].modulate[3] = 255;

	trap_R_AddPolyToScene( cgs.media.wakeMarkShader, 4, verts );
}


/*
===============
CG_AddRefEntityWithPowerups

Adds a piece with modifications or duplications for powerups
Also called by CG_Missile for quad rockets, but nobody can tell...
===============
*/
void CG_AddRefEntityWithPowerups( refEntity_t *ent, entityState_t *state, int team ) {

        if( cg.snap->ps.powerups[ PW_NVG ]&& !( cg.ItemToggleState[cg.predictedPlayerState.clientNum] & ( 1 << PW_NVG ))  && !cg.renderingThirdPerson)
    {
        ent->customShader = cgs.media.nvgShader; // TG Shader
        trap_R_AddRefEntityToScene( ent );

        //return; // don't add any other shader
    }

    ent->customShader = 0;
    trap_R_AddRefEntityToScene( ent );

}

/*
=================
CG_LightVerts
=================
*/
int CG_LightVerts( vec3_t normal, int numVerts, polyVert_t *verts )
{
	int				i, j;
	float			incoming;
	vec3_t			ambientLight;
	vec3_t			lightDir;
	vec3_t			directedLight;

	trap_R_LightForPoint( verts[0].xyz, ambientLight, directedLight, lightDir );

	for (i = 0; i < numVerts; i++) {
		incoming = DotProduct (normal, lightDir);
		if ( incoming <= 0 ) {
			verts[i].modulate[0] = ambientLight[0];
			verts[i].modulate[1] = ambientLight[1];
			verts[i].modulate[2] = ambientLight[2];
			verts[i].modulate[3] = 255;
			continue;
		}
		j = ( ambientLight[0] + incoming * directedLight[0] );
		if ( j > 255 ) {
			j = 255;
		}
		verts[i].modulate[0] = j;

		j = ( ambientLight[1] + incoming * directedLight[1] );
		if ( j > 255 ) {
			j = 255;
		}
		verts[i].modulate[1] = j;

		j = ( ambientLight[2] + incoming * directedLight[2] );
		if ( j > 255 ) {
			j = 255;
		}
		verts[i].modulate[2] = j;

		verts[i].modulate[3] = 255;
	}
	return qtrue;
}

/*
===============
CG_Player
===============
*/
void CG_Player( centity_t *cent ) {
	clientInfo_t	*ci;
	refEntity_t		legs;
	refEntity_t		torso;
	refEntity_t		head;
                  refEntity_t                   helmet;
	refEntity_t		nvg;
	refEntity_t		medkit;
                  int                                clientNum;
	int		renderfx;
	qboolean		shadow;
	float		shadowPlane;
	weapon_t        weaponNum;
                  weapon_t        PriweaponNum;
                  weapon_t        SecweaponNum;
                  weapon_t        SidweaponNum;
	weaponInfo_t    *weapon;
                  weaponInfo_t    *weapon1;
                  weaponInfo_t    *weapon2;
                  weaponInfo_t    *weapon3;
        	refEntity_t     PriweaponModel;
                  refEntity_t     SecweaponModel;
                  refEntity_t     SidweaponModel;

                 vec3_t			 angles; 


#ifdef MISSIONPACK
	refEntity_t		skull;
	refEntity_t		powerup;
	int				t;
	float			c;
	float			angle;
	vec3_t			dir; //blud moved this back up here to fix error/warning
#endif

        
        qboolean                vestOn;
        qboolean                helmetOn;
		qboolean				nvgOn;
		qboolean				medkitOn;
        vestOn = helmetOn = nvgOn = medkitOn = qfalse;
        
	weaponNum = cent->currentState.weapon;
        
                  SidweaponNum = CG_GetSidearm();
                  PriweaponNum = CG_GetPrimary();
                  SecweaponNum = CG_GetWorstSecondary();

                  
	CG_RegisterWeapon( weaponNum );
        	CG_RegisterWeapon( PriweaponNum );
                	CG_RegisterWeapon( SecweaponNum );
                  CG_RegisterWeapon( SidweaponNum );
                  
	weapon = &cg_weapons[weaponNum];
 	weapon1 = &cg_weapons[PriweaponNum];
        	weapon2 = &cg_weapons[SecweaponNum];
                	weapon3 = &cg_weapons[SidweaponNum];
  

	// the client number is stored in clientNum.  It can't be derived
	// from the entity number, because a single client may have
	// multiple corpses on the level using the same clientinfo
	clientNum = cent->currentState.clientNum;
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		CG_Error( "Bad clientNum on player entity");
	}
	ci = &cgs.clientinfo[ clientNum ];

	// it is possible to see corpses from disconnected players that may
	// not have valid clientinfo
	if ( !ci->infoValid ) {
		return;
        }


        if ( cg.snap->ps.persistant[PERS_KILLED] !=  ci->deaths ) {
          ci->deaths = cg.snap->ps.persistant[PERS_KILLED];
        }
        if ( cg.snap->ps.persistant[PERS_SCORE] !=  ci->score ) {
          ci->score = cg.snap->ps.persistant[PERS_SCORE];
        }




	// get the player model information
	renderfx = 0;
	if ( cent->currentState.number == cg.snap->ps.clientNum) {
		if (!cg.renderingThirdPerson) {
			renderfx = RF_THIRD_PERSON;			// only draw in mirrors
		} else {
			if (cg_cameraMode.integer) {
				return;
			}
		}
	}


	memset( &legs, 0, sizeof(legs) );
	memset( &torso, 0, sizeof(torso) );
	memset( &head, 0, sizeof(head) );
        memset( &helmet, 0, sizeof(helmet) );
		memset( &nvg, 0, sizeof(nvg) );
		memset( &medkit, 0, sizeof(medkit) );


  //rotate lerpAngles to floor
    VectorCopy( cent->lerpAngles, angles );

  //normalise the pitch
  if( angles[ PITCH ] < -180.0f )
    angles[ PITCH ] += 360.0f;


	// get the rotation information
                  CG_PlayerAngles( cent, angles, legs.axis, torso.axis, head.axis );
//	CG_PlayerAngles( cent, legs.axis, torso.axis, head.axis );
                    

	// get the animation state (after rotation, to allow feet shuffle)
	CG_PlayerAnimation( cent, &legs.oldframe, &legs.frame, &legs.backlerp,
		 &torso.oldframe, &torso.frame, &torso.backlerp );

	// add the talk baloon or disconnect icon
	CG_PlayerSprites( cent );//Need these for Bomb mode!!  --Xamis




	// add the shadow
	shadow = CG_PlayerShadow( cent, &shadowPlane );

	// add a water splash if partially in and out of water
	CG_PlayerSplash( cent );

        if ( 0 ) {  //xamis --was:  cg_shadows.integer == 3 && shadow
		renderfx |= RF_SHADOW_PLANE;
	}
	renderfx |= RF_LIGHTING_ORIGIN;			// use the same origin for all
#ifdef MISSIONPACK
	if( cgs.gametype == GT_BOMB ) {
		CG_PlayerTokens( cent, renderfx );
	}
#endif
	//
	// add the legs
	//
	legs.hModel = ci->legsModel;
	legs.customSkin = ci->legsSkin;

	VectorCopy( cent->lerpOrigin, legs.origin );
	VectorCopy( cent->lerpOrigin, legs.lightingOrigin );
	legs.shadowPlane = shadowPlane;
	legs.renderfx = renderfx;
	VectorCopy (legs.origin, legs.oldorigin);	// don't positionally lerp at all


  //offset on the Z axis if required
 // VectorMA( legs.origin, BG_ClassConfig( class )->zOffset, surfNormal, legs.origin );
  VectorCopy( legs.origin, legs.lightingOrigin );
  VectorCopy( legs.origin, legs.oldorigin ); // don't positionally lerp at all

        VectorScale(legs.axis[0], 1.06f, legs.axis[0]);
        VectorScale(legs.axis[1], 1.06f, legs.axis[1]);
        VectorScale(legs.axis[2], 1.08f, legs.axis[2]);
        
	CG_AddRefEntityWithPowerups( &legs, &cent->currentState, ci->team );

	// if the model failed, allow the default nullmodel to be displayed
	if (!legs.hModel) {
		return;
	}

        if ( cent->currentState.powerups & ( 1 << PW_VEST ) )
          vestOn = qtrue;
	//
	// add the torso
	//
        if ( vestOn){
        torso.hModel = ci->vestModel;
        torso.customSkin = ci->vestSkin;
        }else{
        torso.customSkin = ci->torsoSkin;
	torso.hModel = ci->torsoModel;
        }
	if (!torso.hModel) {
		return;
	}

	//torso.customSkin = ci->torsoSkin;

	VectorCopy( cent->lerpOrigin, torso.lightingOrigin );
        
        VectorScale(torso.axis[0], 1.06f, torso.axis[0]);
        VectorScale(torso.axis[1], 1.06f, torso.axis[1]);
        VectorScale(torso.axis[2], 1.08f, torso.axis[2]);

        
	CG_PositionRotatedEntityOnTag( &torso, &legs, ci->legsModel, "tag_torso");

	torso.shadowPlane = shadowPlane;
	torso.renderfx = renderfx;

	CG_AddRefEntityWithPowerups( &torso, &cent->currentState, ci->team );
        
      

#ifdef MISSIONPACK
	if ( cent->currentState.eFlags & EF_KAMIKAZE ) {

		memset( &skull, 0, sizeof(skull) );

		VectorCopy( cent->lerpOrigin, skull.lightingOrigin );
		skull.shadowPlane = shadowPlane;
		skull.renderfx = renderfx;

		if ( cent->currentState.eFlags & EF_DEAD ) {
			// one skull bobbing above the dead body
			angle = ((cg.time / 7) & 255) * (M_PI * 2) / 255;
			if (angle > M_PI * 2)
				angle -= (float)M_PI * 2;
			dir[0] = sin(angle) * 20;
			dir[1] = cos(angle) * 20;
			angle = ((cg.time / 4) & 255) * (M_PI * 2) / 255;
			dir[2] = 15 + sin(angle) * 8;
			VectorAdd(torso.origin, dir, skull.origin);

			dir[2] = 0;
			VectorCopy(dir, skull.axis[1]);
			VectorNormalize(skull.axis[1]);
			VectorSet(skull.axis[2], 0, 0, 1);
			CrossProduct(skull.axis[1], skull.axis[2], skull.axis[0]);

			skull.hModel = cgs.media.kamikazeHeadModel;
			trap_R_AddRefEntityToScene( &skull );
			skull.hModel = cgs.media.kamikazeHeadTrail;
			trap_R_AddRefEntityToScene( &skull );
		}
		else {
			// three skulls spinning around the player
			angle = ((cg.time / 4) & 255) * (M_PI * 2) / 255;
			dir[0] = cos(angle) * 20;
			dir[1] = sin(angle) * 20;
			dir[2] = cos(angle) * 20;
			VectorAdd(torso.origin, dir, skull.origin);

			angles[0] = sin(angle) * 30;
			angles[1] = (angle * 180 / M_PI) + 90;
			if (angles[1] > 360)
				angles[1] -= 360;
			angles[2] = 0;
			AnglesToAxis( angles, skull.axis );

			/*
			dir[2] = 0;
			VectorInverse(dir);
			VectorCopy(dir, skull.axis[1]);
			VectorNormalize(skull.axis[1]);
			VectorSet(skull.axis[2], 0, 0, 1);
			CrossProduct(skull.axis[1], skull.axis[2], skull.axis[0]);
			*/

			skull.hModel = cgs.media.kamikazeHeadModel;
			trap_R_AddRefEntityToScene( &skull );
			// flip the trail because this skull is spinning in the other direction
			VectorInverse(skull.axis[1]);
			skull.hModel = cgs.media.kamikazeHeadTrail;
			trap_R_AddRefEntityToScene( &skull );

			angle = ((cg.time / 4) & 255) * (M_PI * 2) / 255 + M_PI;
			if (angle > M_PI * 2)
				angle -= (float)M_PI * 2;
			dir[0] = sin(angle) * 20;
			dir[1] = cos(angle) * 20;
			dir[2] = cos(angle) * 20;
			VectorAdd(torso.origin, dir, skull.origin);

			angles[0] = cos(angle - 0.5 * M_PI) * 30;
			angles[1] = 360 - (angle * 180 / M_PI);
			if (angles[1] > 360)
				angles[1] -= 360;
			angles[2] = 0;
			AnglesToAxis( angles, skull.axis );

			/*
			dir[2] = 0;
			VectorCopy(dir, skull.axis[1]);
			VectorNormalize(skull.axis[1]);
			VectorSet(skull.axis[2], 0, 0, 1);
			CrossProduct(skull.axis[1], skull.axis[2], skull.axis[0]);
			*/

			skull.hModel = cgs.media.kamikazeHeadModel;
			trap_R_AddRefEntityToScene( &skull );
			skull.hModel = cgs.media.kamikazeHeadTrail;
			trap_R_AddRefEntityToScene( &skull );

			angle = ((cg.time / 3) & 255) * (M_PI * 2) / 255 + 0.5 * M_PI;
			if (angle > M_PI * 2)
				angle -= (float)M_PI * 2;
			dir[0] = sin(angle) * 20;
			dir[1] = cos(angle) * 20;
			dir[2] = 0;
			VectorAdd(torso.origin, dir, skull.origin);

			VectorCopy(dir, skull.axis[1]);
			VectorNormalize(skull.axis[1]);
			VectorSet(skull.axis[2], 0, 0, 1);
			CrossProduct(skull.axis[1], skull.axis[2], skull.axis[0]);

			skull.hModel = cgs.media.kamikazeHeadModel;
			trap_R_AddRefEntityToScene( &skull );
			skull.hModel = cgs.media.kamikazeHeadTrail;
			trap_R_AddRefEntityToScene( &skull );
		}
	}



		ci->invulnerabilityStartTime = 0;

	t = cg.time - ci->medkitUsageTime;
	if ( ci->medkitUsageTime && t < 500 ) {
		memcpy(&powerup, &torso, sizeof(torso));
		powerup.hModel = cgs.media.medkitUsageModel;
		powerup.customSkin = 0;
		// always draw
		powerup.renderfx &= ~RF_THIRD_PERSON;
		VectorClear(angles);
		AnglesToAxis(angles, powerup.axis);
		VectorCopy(cent->lerpOrigin, powerup.origin);
		powerup.origin[2] += -24 + (float) t * 80 / 500;
		if ( t > 400 ) {
			c = (float) (t - 1000) * 0xff / 100;
			powerup.shaderRGBA[0] = 0xff - c;
			powerup.shaderRGBA[1] = 0xff - c;
			powerup.shaderRGBA[2] = 0xff - c;
			powerup.shaderRGBA[3] = 0xff - c;
		}
		else {
			powerup.shaderRGBA[0] = 0xff;
			powerup.shaderRGBA[1] = 0xff;
			powerup.shaderRGBA[2] = 0xff;
			powerup.shaderRGBA[3] = 0xff;
		}
		trap_R_AddRefEntityToScene( &powerup );
	}
#endif // MISSIONPACK

	//
	// add the head
	//
	head.hModel = ci->headModel;
	if (!head.hModel) {
		return;
	}
        
        VectorScale(head.axis[0], 1.05f, head.axis[0]);
        VectorScale(head.axis[1], 1.05f, head.axis[1]);
        VectorScale(head.axis[2], 1.07f, head.axis[2]);
        
	head.customSkin = ci->headSkin;

	VectorCopy( cent->lerpOrigin, head.lightingOrigin );

	//blud: Just noticed this part below seems backwards... what's up with that?
	if (vestOn) //blud fixing torso
		CG_PositionRotatedEntityOnTag( &head, &torso, ci->torsoModel, "tag_head");
	else
		CG_PositionRotatedEntityOnTag( &head, &torso, ci->vestModel, "tag_head");

	head.shadowPlane = shadowPlane;
	head.renderfx = renderfx;

	CG_AddRefEntityWithPowerups( &head, &cent->currentState, ci->team );



          if ( cent->currentState.powerups & ( 1 << PW_HELMET ) ){ //using ammo for powerup/item storage
            helmetOn = qtrue;
        }
          if ( helmetOn ){

        VectorCopy( cent->lerpOrigin, helmet.lightingOrigin );

        CG_PositionEntityOnTag( &helmet, &head, ci->headModel, "tag_head");
        helmet.hModel = ci->helmetModel;
        helmet.shadowPlane = shadowPlane;
        helmet.renderfx = renderfx;

        CG_AddRefEntityWithPowerups( &helmet, &cent->currentState, ci->team );
        }


	//blud copying the helmet stuff above for nvg
	if ( cent->currentState.powerups & ( 1 << PW_NVG ) ){
		nvgOn = qtrue;
	}
	if ( nvgOn ){

		VectorCopy( cent->lerpOrigin, nvg.lightingOrigin );

		CG_PositionEntityOnTag( &nvg, &head, ci->headModel, "tag_head");
		nvg.hModel = ci->nvgModel;
		nvg.shadowPlane = shadowPlane;
		nvg.renderfx = renderfx;

		CG_AddRefEntityWithPowerups( &nvg, &cent->currentState, ci->team );
                

	}

	
	//blud copying the helmet stuff above for medkit
	if ( cent->currentState.powerups & ( 1 << PW_MEDKIT ) ){
		medkitOn = qtrue;
	}
	if ( medkitOn ){

		VectorCopy( cent->lerpOrigin, medkit.lightingOrigin );

		if (vestOn)
			CG_PositionEntityOnTag( &medkit, &torso, ci->torsoModel, "tag_back");
		else
			CG_PositionEntityOnTag( &medkit, &torso, ci->vestModel, "tag_back");


		medkit.hModel = ci->medkitModel;
		medkit.shadowPlane = shadowPlane;
		medkit.renderfx = renderfx;

		CG_AddRefEntityWithPowerups( &medkit, &cent->currentState, ci->team );
	}




         if( cent->currentState.number == cg.predictedPlayerState.clientNum && cg.snap->ps.stats[STAT_HEALTH] >0 ){
          if ( PriweaponNum != cent->currentState.weapon ){
          	memset( &PriweaponModel, 0, sizeof( PriweaponModel ) );
	VectorCopy( torso.lightingOrigin, PriweaponModel.lightingOrigin );
	PriweaponModel.shadowPlane = torso.shadowPlane;
	PriweaponModel.renderfx = torso.renderfx;
                  PriweaponModel.hModel = weapon1->weaponModel;
                CG_PositionEntityOnTag( &PriweaponModel, &torso, torso.hModel, "tag_primary" );
                CG_AddWeaponWithPowerups( &PriweaponModel, cent->currentState.powerups );
          }
          if ( SecweaponNum != cent->currentState.weapon ){
          	memset( &SecweaponModel, 0, sizeof( SecweaponModel ) );
	VectorCopy( torso.lightingOrigin, SecweaponModel.lightingOrigin );
	SecweaponModel.shadowPlane = torso.shadowPlane;
	SecweaponModel.renderfx = torso.renderfx;
                 SecweaponModel.hModel = weapon2->weaponModel;
                CG_PositionEntityOnTag( &SecweaponModel,  &torso, torso.hModel, "tag_secondar" );
                CG_AddWeaponWithPowerups( &SecweaponModel, cent->currentState.powerups );
          }
          if ( SidweaponNum != cent->currentState.weapon ){
          	memset( &SidweaponModel, 0, sizeof( SidweaponModel ) );
	VectorCopy( legs.lightingOrigin, SidweaponModel.lightingOrigin );
	SidweaponModel.shadowPlane = legs.shadowPlane;
	SidweaponModel.renderfx = legs.renderfx;
                  SidweaponModel.hModel = weapon3->weaponModel;
                CG_PositionEntityOnTag( &SidweaponModel, &legs, legs.hModel, "tag_sidearm" );
                CG_AddWeaponWithPowerups( &SidweaponModel, cent->currentState.powerups );
          }
        }

#ifdef MISSIONPACK
	CG_BreathPuffs(cent, &head);

	CG_DustTrail(cent);
#endif




	//
	// add the gun / barrel / flash
	//
	CG_AddPlayerWeapon( &torso, NULL, cent, ci->team, ci->modelName, ci->skin );

	// add powerups floating behind the player
	CG_PlayerPowerups( cent, &torso );

}


//=====================================================================


/* BLUD NOTE:	I wrote these Get and Set functions below for cleaning up
				the code, since in various places things are done kind of 
				sloppily or repeatedly (half my fault, half q3's fault)
				But I only use CG_GetPlayerModelName so far. Just with 
				only that one I was able to fix the player skin/model
				buggyness that was going on before. But the other 3 will
				useful in future for cleaning up and making the code nicer.
*/

/*
====================
CG_SetPlayerSkinName
by blud
====================
*/
qboolean CG_SetPlayerSkinName( int team, int racered, int raceblue, char *skin ) {
	//well what do you need to know to set the skinName?
	//you need to know 
		//- if it's a team game, we have cgs.gametype for that
			//- what team the user is on, pass in TEAM I guess?
			//- what race* they are on that team, pass in racered and raceblue
		//else its a nonteam game
			//- what is their skin cvar, pass in skin cvar
	//if success
		return qtrue;
	//else
	//	return qfalse;
}


/*
=====================
CG_SetPlayerModelName
by blud
=====================
*/
qboolean CG_SetPlayerModelName( int team, int racered, int raceblue, char *model ) {
	//well what do you need to know to set the modelName?
	//you need to know 
		//- if it's a team game, we have cgs.gametype for that
			//- what team the user is on, pass in TEAM I guess?
			//- what race* they are on that team, pass in racered and raceblue
		//else its a nonteam game
			//- what is their model cvar, pass in model cvar
	//if success
		return qtrue;
	//else
	//	return qfalse;
}


/*
====================
CG_GetPlayerSkinName
by blud
====================
*/
char *CG_GetPlayerSkinName( clientInfo_t *ci ) {
	if (cgs.gametype >= GT_TEAM)
	{
		if (ci->team == TEAM_BLUE || ci->team == TEAM_BLUE_SPECTATOR)
		{
			if (ci->raceblue == 0 || ci->raceblue == 2 )
				return "blue";
			else
				return "blue2";
		}
		else //they are on the red team
		{
			if (ci->racered == 0 || ci->racered == 2 )
				return "red";
			else
				return "red2";
		}
	}
	else //this is a non-team gt
	{
		return ci->skinName;
	}
}


/*
=====================
CG_GetPlayerModelName
by blud
=====================
*/
char *CG_GetPlayerModelName( clientInfo_t *ci ) {
	if (cgs.gametype >= GT_TEAM)
	{
		if (ci->team == TEAM_BLUE || ci->team == TEAM_BLUE_SPECTATOR )
		{
			if (ci->raceblue < 2)
				return "athena";
			else
				return "orion";
		}
		else //they are on the red team
		{
			if (ci->racered < 2)
				return "athena";
			else
				return "orion";
		}
		//I suppose they might be on Spectator team, but then I doubt it matters what model they get
	}
	else //this is a non-team gt
	{
		return ci->modelName;
	}
}


/*
===============
CG_ResetPlayerEntity

A player just came into view or teleported, so reset all animation info
===============
*/
void CG_ResetPlayerEntity( centity_t *cent ) {
	cent->errorTime = -99999;		// guarantee no error decay added
	cent->extrapolated = qfalse;

	CG_ClearLerpFrame( &cgs.clientinfo[ cent->currentState.clientNum ], &cent->pe.legs, cent->currentState.legsAnim );
	CG_ClearLerpFrame( &cgs.clientinfo[ cent->currentState.clientNum ], &cent->pe.torso, cent->currentState.torsoAnim );

	BG_EvaluateTrajectory( &cent->currentState.pos, cg.time, cent->lerpOrigin );
	BG_EvaluateTrajectory( &cent->currentState.apos, cg.time, cent->lerpAngles );

	VectorCopy( cent->lerpOrigin, cent->rawOrigin );
	VectorCopy( cent->lerpAngles, cent->rawAngles );

	memset( &cent->pe.legs, 0, sizeof( cent->pe.legs ) );
	cent->pe.legs.yawAngle = cent->rawAngles[YAW];
	cent->pe.legs.yawing = qfalse;
	cent->pe.legs.pitchAngle = 0;
	cent->pe.legs.pitching = qfalse;

	memset( &cent->pe.torso, 0, sizeof( cent->pe.legs ) );
	cent->pe.torso.yawAngle = cent->rawAngles[YAW];
	cent->pe.torso.yawing = qfalse;
	cent->pe.torso.pitchAngle = cent->rawAngles[PITCH];
	cent->pe.torso.pitching = qfalse;

	if ( cg_debugPosition.integer ) {
		CG_Printf("%i ResetPlayerEntity yaw=%i\n", cent->currentState.number, cent->pe.torso.yawAngle );
	}
}

