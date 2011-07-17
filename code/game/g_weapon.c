/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2009-2010 Brian Labbie and Dave Richardson.

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
// g_weapon.c
// perform the server side effects of a weapon firing

#include "g_local.h"

static  float   s_quadFactor;
static  vec3_t  forward, right, up;
static  vec3_t  muzzle;

#define NUM_NAILSHOTS 15

/*
================
G_BounceProjectile
================
*/
void G_BounceProjectile( vec3_t start, vec3_t impact, vec3_t dir, vec3_t endout ) {
        vec3_t v, newv;
        float dot;

        VectorSubtract( impact, start, v );
        dot = DotProduct( v, dir );
        VectorMA( v, -2*dot, dir, newv );

        VectorNormalize(newv);
        VectorMA(impact, 8192, newv, endout);
}


/*
======================================================================

GAUNTLET

======================================================================
*/

void Weapon_Gauntlet( gentity_t *ent ) {

}

/*
===============
CheckGauntletAttack
===============
*/
qboolean CheckGauntletAttack( gentity_t *ent ) {
        trace_t         tr;
        vec3_t          end;
        gentity_t       *tent;
        gentity_t       *traceEnt;
        int                     damage;

        // set aiming directions
        AngleVectors (ent->client->ps.viewangles, forward, right, up);

        CalcMuzzlePoint ( ent, forward, right, up, muzzle );

        VectorMA (muzzle, 32, forward, end);

        trap_Trace (&tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT);
        if ( tr.surfaceFlags & SURF_NOIMPACT ) {
                return qfalse;
        }

        traceEnt = &g_entities[ tr.entityNum ];

        // send blood impact
        if ( traceEnt->takedamage && traceEnt->client ) {
                tent = G_TempEntity( tr.endpos, EV_MISSILE_HIT );
                tent->s.otherEntityNum = traceEnt->s.number;
                tent->s.eventParm = DirToByte( tr.plane.normal );
                tent->s.weapon = ent->s.weapon;
        }

        if ( !traceEnt->takedamage) {
                return qfalse;
        }
                s_quadFactor = 1;


        damage = 50 * s_quadFactor;
        G_Damage( traceEnt, ent, ent, forward, tr.endpos,
                damage, 0, MOD_GAUNTLET );

        return qtrue;
}


/*
======================================================================

MACHINEGUN

======================================================================
*/

/*
======================
SnapVectorTowards

Round a vector to integers for more efficient network
transmission, but make sure that it rounds towards a given point
rather than blindly truncating.  This prevents it from truncating
into a wall.
======================
*/
void SnapVectorTowards( vec3_t v, vec3_t to ) {
        int             i;

        for ( i = 0 ; i < 3 ; i++ ) {
                if ( to[i] <= v[i] ) {
                        v[i] = (int)v[i];
                } else {
                        v[i] = (int)v[i] + 1;
                }
        }
}

#ifdef MISSIONPACK
#define CHAINGUN_SPREAD         600
#endif
#define MACHINEGUN_SPREAD       20
#define DEAGLE_SPREAD           10
#define MACHINEGUN_DAMAGE       7
#define M4_DAMAGE       25 //12
#define AK103_DAMAGE    30 //16
#define LR300_DAMAGE   25 //25
#define BERETTA_DAMAGE   20 //25
#define G36_DAMAGE   22 //25
#define DEAGLE_DAMAGE   40 //25
#define UMP45_DAMAGE   40 //25
#define MP5K_DAMAGE   22 //25


void Bullet_Fire (gentity_t *ent, float spread, int damage ) {
        trace_t         tr;
        vec3_t          end;
#ifdef MISSIONPACK
        vec3_t          impactpoint, bouncedir;
#endif
        float           r;
        float           u;
        gentity_t       *tent;
        gentity_t       *traceEnt;
        int                     i, passent;
        spread += BG_CalcSpread(ent->client->ps);
        damage *= s_quadFactor;
       // G_Printf ("spread = %f\n xyspeed = %f", spread, BG_CalcSpread(ent->client->ps) );
        r = random() * M_PI * 2.0f;
        u = sin(r) * crandom() * spread * 16;
        r = cos(r) * crandom() * spread * 16;
        VectorMA (muzzle, 8192*16, forward, end);
        VectorMA (end, r, right, end);
        VectorMA (end, u, up, end);

        passent = ent->s.number;
        for (i = 0; i < 10; i++) {

                trap_Trace (&tr, muzzle, NULL, NULL, end, passent, MASK_SHOT);
                if ( tr.surfaceFlags & SURF_NOIMPACT ) {
                        return;
                }

                traceEnt = &g_entities[ tr.entityNum ];

                // snap the endpos to integers, but nudged towards the line
                SnapVectorTowards( tr.endpos, muzzle );

                // send bullet impact
                if ( traceEnt->takedamage && traceEnt->client ) {
                        tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_FLESH );
                        tent->s.eventParm = traceEnt->s.number;
                        if( LogAccuracyHit( traceEnt, ent ) ) {
                                ent->client->accuracy_hits++;
                        }
                } else {
                        tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_WALL );
                        tent->s.eventParm = DirToByte( tr.plane.normal );
                }
                tent->s.otherEntityNum = ent->s.number;

                if ( traceEnt->takedamage) {
#ifdef MISSIONPACK
                        if ( traceEnt->client && traceEnt->client->invulnerabilityTime > level.time ) {
                                if (G_InvulnerabilityEffect( traceEnt, forward, tr.endpos, impactpoint, bouncedir )) {
                                        G_BounceProjectile( muzzle, impactpoint, bouncedir, end );
                                        VectorCopy( impactpoint, muzzle );
                                        // the player can hit him/herself with the bounced rail
                                        passent = ENTITYNUM_NONE;
                                }
                                else {
                                        VectorCopy( tr.endpos, muzzle );
                                        passent = traceEnt->s.number;
                                }
                                continue;
                        }
                        else {
#endif
                                G_Damage( traceEnt, ent, ent, forward, tr.endpos,
                                        damage, 0, MOD_MACHINEGUN);
#ifdef MISSIONPACK
                        }
#endif
                }
                break;
        }
}



/*
======================================================================

SHOTGUN

======================================================================
*/

// DEFAULT_SHOTGUN_SPREAD and DEFAULT_SHOTGUN_COUNT     are in bg_public.h, because
// client predicts same spreads
#define DEFAULT_SHOTGUN_DAMAGE  20

qboolean ShotgunPellet( vec3_t start, vec3_t end, gentity_t *ent ) {
        trace_t         tr;
        int                     damage, i, passent;
        gentity_t       *traceEnt;
#ifdef MISSIONPACK
        vec3_t          impactpoint, bouncedir;
#endif
        vec3_t          tr_start, tr_end;

        passent = ent->s.number;
        VectorCopy( start, tr_start );
        VectorCopy( end, tr_end );
        for (i = 0; i < 10; i++) {
                trap_Trace (&tr, tr_start, NULL, NULL, tr_end, passent, MASK_SHOT);
                traceEnt = &g_entities[ tr.entityNum ];

                // send bullet impact
                if (  tr.surfaceFlags & SURF_NOIMPACT ) {
                        return qfalse;
                }

                if ( traceEnt->takedamage) {
                        damage = DEFAULT_SHOTGUN_DAMAGE * s_quadFactor;
#ifdef MISSIONPACK
                        if ( traceEnt->client && traceEnt->client->invulnerabilityTime > level.time ) {
                                if (G_InvulnerabilityEffect( traceEnt, forward, tr.endpos, impactpoint, bouncedir )) {
                                        G_BounceProjectile( tr_start, impactpoint, bouncedir, tr_end );
                                        VectorCopy( impactpoint, tr_start );
                                        // the player can hit him/herself with the bounced rail
                                        passent = ENTITYNUM_NONE;
                                }
                                else {
                                        VectorCopy( tr.endpos, tr_start );
                                        passent = traceEnt->s.number;
                                }
                                continue;
                        }
                        else {
                                G_Damage( traceEnt, ent, ent, forward, tr.endpos,
                                        damage, 0, MOD_SHOTGUN);
                                if( LogAccuracyHit( traceEnt, ent ) ) {
                                        return qtrue;
                                }
                        }
#else
                        G_Damage( traceEnt, ent, ent, forward, tr.endpos,       damage, 0, MOD_SHOTGUN);
                                if( LogAccuracyHit( traceEnt, ent ) ) {
                                        return qtrue;
                                }
#endif
                }
                return qfalse;
        }
        return qfalse;
}

// this should match CG_ShotgunPattern
void ShotgunPattern( vec3_t origin, vec3_t origin2, int seed, gentity_t *ent ) {
        int                     i;
        float           r, u;
        vec3_t          end;
        vec3_t          forward, right, up;
        int                     oldScore;
        qboolean        hitClient = qfalse;

        // derive the right and up vectors from the forward vector, because
        // the client won't have any other information
        VectorNormalize2( origin2, forward );
        PerpendicularVector( right, forward );
        CrossProduct( forward, right, up );

        oldScore = ent->client->ps.persistant[PERS_SCORE];

        // generate the "random" spread pattern
        for ( i = 0 ; i < DEFAULT_SHOTGUN_COUNT ; i++ ) {
                r = Q_crandom( &seed ) * DEFAULT_SHOTGUN_SPREAD * 16;
                u = Q_crandom( &seed ) * DEFAULT_SHOTGUN_SPREAD * 16;
                VectorMA( origin, 8192 * 16, forward, end);
                VectorMA (end, r, right, end);
                VectorMA (end, u, up, end);
                if( ShotgunPellet( origin, end, ent ) && !hitClient ) {
                        hitClient = qtrue;
                        ent->client->accuracy_hits++;
                }
        }
}


void weapon_supershotgun_fire (gentity_t *ent) {
        gentity_t               *tent;

        // send shotgun blast
        tent = G_TempEntity( muzzle, EV_SHOTGUN );
        VectorScale( forward, 4096, tent->s.origin2 );
        SnapVector( tent->s.origin2 );
        tent->s.eventParm = rand() & 255;               // seed for spread pattern
        tent->s.otherEntityNum = ent->s.number;

        ShotgunPattern( tent->s.pos.trBase, tent->s.origin2, tent->s.eventParm, ent );
}


/*
======================================================================

GRENADES

======================================================================
*/

void weapon_grenadelauncher_fire (gentity_t *ent) {
        gentity_t       *m;

        // extra vertical velocity
        forward[2] += 0.2f;
        VectorNormalize( forward );

        m = fire_grenade (ent, muzzle, forward);
        m->damage *= s_quadFactor;
        m->splashDamage *= s_quadFactor;

        VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );      // "real" physics
}

//Xamis
void weapon_smoke_throw (gentity_t *ent) {
  gentity_t       *m;
  vec3_t start;

  VectorCopy(ent->s.pos.trBase, start);

  start[2] += ent->client->ps.viewheight+8.0f;

  forward[2] += 0.1f;

  VectorNormalize( forward );

  m = throw_smoke(ent, start, forward);
        //m->damage *= s_quadFactor;
        //m->splashDamage *= s_quadFactor;
//      VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );      // "real" physics

}


void weapon_grenade_throw (gentity_t *ent) {
        gentity_t       *m;
        vec3_t start;

        VectorCopy(ent->s.pos.trBase, start);

        start[2] += ent->client->ps.viewheight+8.0f;

        forward[2] += 0.1f;

        VectorNormalize( forward );

        m = throw_grenade (ent, start, forward);
        //m->damage *= s_quadFactor;
        //m->splashDamage *= s_quadFactor;
//      VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );      // "real" physics

}


void weapon_grenade_arm( gentity_t *self ){

  gentity_t     *bolt;

  bolt = G_Spawn();
  bolt->classname = "ut_weapon_grenade_he";
  bolt->nextthink = level.time + 3000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
  bolt->s.weapon = WP_HE;
  bolt->s.eFlags = EF_BOUNCE_HALF;
  bolt->r.ownerNum = self->s.number;
  bolt->timestamp = level.time;
  bolt->parent = self;
  bolt->damage = 100;
  bolt->splashDamage = 100;
  bolt->splashRadius = 150;
  bolt->methodOfDeath = MOD_GRENADE;
  bolt->splashMethodOfDeath = MOD_GRENADE_SPLASH;
  bolt->clipmask = MASK_SHOT;
  bolt->target_ent = NULL;
}
/*
=================
weapon_railgun_fire
=================
*/
#define MAX_RAIL_HITS   4
void weapon_railgun_fire (gentity_t *ent) {
        vec3_t          end;
#ifdef MISSIONPACK
        vec3_t impactpoint, bouncedir;
#endif
        trace_t         trace;
        gentity_t       *tent;
        gentity_t       *traceEnt;
        int                     damage;
        int                     i;
        int                     hits;
        int                     unlinked;
        int                     passent;
        gentity_t       *unlinkedEntities[MAX_RAIL_HITS];

        damage = 100 * s_quadFactor;

        VectorMA (muzzle, 8192, forward, end);

        // trace only against the solids, so the railgun will go through people
        unlinked = 0;
        hits = 0;
        passent = ent->s.number;
        do {
                trap_Trace (&trace, muzzle, NULL, NULL, end, passent, MASK_SHOT );
                if ( trace.entityNum >= ENTITYNUM_MAX_NORMAL ) {
                        break;
                }
                traceEnt = &g_entities[ trace.entityNum ];
                if ( traceEnt->takedamage ) {
#ifdef MISSIONPACK
                        if ( traceEnt->client && traceEnt->client->invulnerabilityTime > level.time ) {
                                if ( G_InvulnerabilityEffect( traceEnt, forward, trace.endpos, impactpoint, bouncedir ) ) {
                                        G_BounceProjectile( muzzle, impactpoint, bouncedir, end );
                                        // snap the endpos to integers to save net bandwidth, but nudged towards the line
                                        SnapVectorTowards( trace.endpos, muzzle );
                                        // send railgun beam effect
                                        tent = G_TempEntity( trace.endpos, EV_RAILTRAIL );
                                        // set player number for custom colors on the railtrail
                                        tent->s.clientNum = ent->s.clientNum;
                                        VectorCopy( muzzle, tent->s.origin2 );
                                        // move origin a bit to come closer to the drawn gun muzzle
                                        VectorMA( tent->s.origin2, 4, right, tent->s.origin2 );
                                        VectorMA( tent->s.origin2, -1, up, tent->s.origin2 );
                                        tent->s.eventParm = 255;        // don't make the explosion at the end
                                        //
                                        VectorCopy( impactpoint, muzzle );
                                        // the player can hit him/herself with the bounced rail
                                        passent = ENTITYNUM_NONE;
                                }
                        }
                        else {
                                if( LogAccuracyHit( traceEnt, ent ) ) {
                                        hits++;
                                }
                                G_Damage (traceEnt, ent, ent, forward, trace.endpos, damage, 0, MOD_RAILGUN);
                        }
#else
                                if( LogAccuracyHit( traceEnt, ent ) ) {
                                        hits++;
                                }
                                G_Damage (traceEnt, ent, ent, forward, trace.endpos, damage, 0, MOD_RAILGUN);
#endif
                }
                if ( trace.contents & CONTENTS_SOLID ) {
                        break;          // we hit something solid enough to stop the beam
                }
                // unlink this entity, so the next trace will go past it
                trap_UnlinkEntity( traceEnt );
                unlinkedEntities[unlinked] = traceEnt;
                unlinked++;
        } while ( unlinked < MAX_RAIL_HITS );

        // link back in any entities we unlinked
        for ( i = 0 ; i < unlinked ; i++ ) {
                trap_LinkEntity( unlinkedEntities[i] );
        }

        // the final trace endpos will be the terminal point of the rail trail

        // snap the endpos to integers to save net bandwidth, but nudged towards the line
        SnapVectorTowards( trace.endpos, muzzle );

        // send railgun beam effect
        tent = G_TempEntity( trace.endpos, EV_RAILTRAIL );

        // set player number for custom colors on the railtrail
        tent->s.clientNum = ent->s.clientNum;

        VectorCopy( muzzle, tent->s.origin2 );
        // move origin a bit to come closer to the drawn gun muzzle
        VectorMA( tent->s.origin2, 4, right, tent->s.origin2 );
        VectorMA( tent->s.origin2, -1, up, tent->s.origin2 );

        // no explosion at end if SURF_NOIMPACT, but still make the trail
        if ( trace.surfaceFlags & SURF_NOIMPACT ) {
                tent->s.eventParm = 255;        // don't make the explosion at the end
        } else {
                tent->s.eventParm = DirToByte( trace.plane.normal );
        }
        tent->s.clientNum = ent->s.clientNum;

        // give the shooter a reward sound if they have made two railgun hits in a row
        if ( hits == 0 ) {
                // complete miss
                ent->client->accurateCount = 0;
        } else {
                // check for "impressive" reward sound
                ent->client->accurateCount += hits;
                if ( ent->client->accurateCount >= 2 ) {
                        ent->client->accurateCount -= 2;
                        ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
                        // add the sprite over the player's head
                        ent->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
                        ent->client->ps.eFlags |= EF_AWARD_IMPRESSIVE;
                        ent->client->rewardTime = level.time + REWARD_SPRITE_TIME;
                }
                ent->client->accuracy_hits++;
        }

}


/*
======================================================================

GRAPPLING HOOK

======================================================================
*/

void Weapon_GrapplingHook_Fire (gentity_t *ent)
{
        if (!ent->client->fireHeld && !ent->client->hook)
                //fire_grapple (ent, muzzle, forward);
                //blud commenting out call to fire_grapple() because
                //fire_grapple() function is now commented out because
                //it was throwing a warning I didn't want to see.



        ent->client->fireHeld = qtrue;
}

void Weapon_HookFree (gentity_t *ent)
{
        ent->parent->client->hook = NULL;
//      ent->parent->client->ps.pm_flags &= ~PMF_GRAPPLE_PULL;
        G_FreeEntity( ent );
}

void Weapon_HookThink (gentity_t *ent)
{
        if (ent->enemy) {
                vec3_t v, oldorigin;

                VectorCopy(ent->r.currentOrigin, oldorigin);
                v[0] = ent->enemy->r.currentOrigin[0] + (ent->enemy->r.mins[0] + ent->enemy->r.maxs[0]) * 0.5;
                v[1] = ent->enemy->r.currentOrigin[1] + (ent->enemy->r.mins[1] + ent->enemy->r.maxs[1]) * 0.5;
                v[2] = ent->enemy->r.currentOrigin[2] + (ent->enemy->r.mins[2] + ent->enemy->r.maxs[2]) * 0.5;
                SnapVectorTowards( v, oldorigin );      // save net bandwidth

                G_SetOrigin( ent, v );
        }

        VectorCopy( ent->r.currentOrigin, ent->parent->client->ps.grapplePoint);
}

/*
======================================================================

LIGHTNING GUN

======================================================================
*/

void Weapon_LightningFire( gentity_t *ent ) {
        trace_t         tr;
        vec3_t          end;
#ifdef MISSIONPACK
        vec3_t impactpoint, bouncedir;
#endif
        gentity_t       *traceEnt, *tent;
        int                     damage, i, passent;

        damage = 8 * s_quadFactor;

        passent = ent->s.number;
        for (i = 0; i < 10; i++) {
                VectorMA( muzzle, LIGHTNING_RANGE, forward, end );

                trap_Trace( &tr, muzzle, NULL, NULL, end, passent, MASK_SHOT );

#ifdef MISSIONPACK
                // if not the first trace (the lightning bounced of an invulnerability sphere)
                if (i) {
                        // add bounced off lightning bolt temp entity
                        // the first lightning bolt is a cgame only visual
                        //
                        tent = G_TempEntity( muzzle, EV_LIGHTNINGBOLT );
                        VectorCopy( tr.endpos, end );
                        SnapVector( end );
                        VectorCopy( end, tent->s.origin2 );
                }
#endif
                if ( tr.entityNum == ENTITYNUM_NONE ) {
                        return;
                }

                traceEnt = &g_entities[ tr.entityNum ];

                if ( traceEnt->takedamage) {
#ifdef MISSIONPACK
                        if ( traceEnt->client && traceEnt->client->invulnerabilityTime > level.time ) {
                                if (G_InvulnerabilityEffect( traceEnt, forward, tr.endpos, impactpoint, bouncedir )) {
                                        G_BounceProjectile( muzzle, impactpoint, bouncedir, end );
                                        VectorCopy( impactpoint, muzzle );
                                        VectorSubtract( end, impactpoint, forward );
                                        VectorNormalize(forward);
                                        // the player can hit him/herself with the bounced lightning
                                        passent = ENTITYNUM_NONE;
                                }
                                else {
                                        VectorCopy( tr.endpos, muzzle );
                                        passent = traceEnt->s.number;
                                }
                                continue;
                        }
                        else {
                                G_Damage( traceEnt, ent, ent, forward, tr.endpos,
                                        damage, 0, MOD_LIGHTNING);
                        }
#else
                                G_Damage( traceEnt, ent, ent, forward, tr.endpos,
                                        damage, 0, MOD_LIGHTNING);
#endif
                }

                if ( traceEnt->takedamage && traceEnt->client ) {
                        tent = G_TempEntity( tr.endpos, EV_MISSILE_HIT );
                        tent->s.otherEntityNum = traceEnt->s.number;
                        tent->s.eventParm = DirToByte( tr.plane.normal );
                        tent->s.weapon = ent->s.weapon;
                        if( LogAccuracyHit( traceEnt, ent ) ) {
                                ent->client->accuracy_hits++;
                        }
                } else if ( !( tr.surfaceFlags & SURF_NOIMPACT ) ) {
                        tent = G_TempEntity( tr.endpos, EV_MISSILE_MISS );
                        tent->s.eventParm = DirToByte( tr.plane.normal );
                }

                break;
        }
}



//======================================================================


/*
===============
LogAccuracyHit
===============
*/
qboolean LogAccuracyHit( gentity_t *target, gentity_t *attacker ) {
        if( !target->takedamage ) {
                return qfalse;
        }

        if ( target == attacker ) {
                return qfalse;
        }

        if( !target->client ) {
                return qfalse;
        }

        if( !attacker->client ) {
                return qfalse;
        }

        if( target->client->ps.stats[STAT_HEALTH] <= 0 ) {
                return qfalse;
        }

        if ( OnSameTeam( target, attacker ) ) {
                return qfalse;
        }

        return qtrue;
}


/*
===============
CalcMuzzlePoint

set muzzle location relative to pivoting eye
===============
*/
void CalcMuzzlePoint ( gentity_t *ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint ) {
        VectorCopy( ent->s.pos.trBase, muzzlePoint );
        muzzlePoint[2] += ent->client->ps.viewheight;
        VectorMA( muzzlePoint, 14, forward, muzzlePoint );
        // snap to integer coordinates for more efficient network bandwidth usage
        SnapVector( muzzlePoint );
}

/*
===============
CalcMuzzlePointOrigin

set muzzle location relative to pivoting eye
===============
*/
void CalcMuzzlePointOrigin ( gentity_t *ent, vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint ) {
        VectorCopy( ent->s.pos.trBase, muzzlePoint );
        muzzlePoint[2] += ent->client->ps.viewheight;
        VectorMA( muzzlePoint, 14, forward, muzzlePoint );
        // snap to integer coordinates for more efficient network bandwidth usage
        SnapVector( muzzlePoint );
}

void Set_Mode(gentity_t *ent){
//  G_Printf ("Set_Mode called\n");
  switch (weapmodes_save.string[ent->client->ps.weapon] ){


    case '0':
  bg_weaponlist[ ent->client->ps.weapon ].weapMode[ent->client->ps.clientNum]
      = ent->client->ps.stats[STAT_MODE]
      = ent->client->weaponMode[ ent->client->ps.weapon ]
      = 0;
      ent->client->ps.pm_flags &= ~PMF_SINGLE_MODE;
  break;
    case '1':
      bg_weaponlist[ ent->client->ps.weapon ].weapMode[ent->client->ps.clientNum]
          = ent->client->ps.stats[STAT_MODE]
          = ent->client->weaponMode[ ent->client->ps.weapon ]
          = 1;
      ent->client->ps.pm_flags |= PMF_SINGLE_MODE;
      if ( ent->client->ps.weapon == WP_MP5K || ent->client->ps.weapon == WP_UMP45 )
        Change_Mode(ent);
      break;
    case '2':
      bg_weaponlist[ ent->client->ps.weapon ].weapMode[ent->client->ps.clientNum]
          = ent->client->ps.stats[STAT_MODE]
          = ent->client->weaponMode[ ent->client->ps.weapon ]
          = 2;
      ent->client->ps.pm_flags &= ~PMF_SINGLE_MODE;
      break;
  }
}
void Change_Mode(gentity_t *ent){
//  int i;
 ent->client->weaponModeChar = weapmodes_save.string;

 //G_Printf( "weaponModeChar: %s\n", ent->client->weaponModeChar);




 ent->client->weaponMode[ ent->client->ps.weapon ]++;
 if ( ent->client->ps.weapon == WP_MP5K || ent->client->ps.weapon == WP_UMP45 ){
   if (ent->client->weaponMode[ ent->client->ps.weapon ] == 1 )
     ent->client->weaponMode[ ent->client->ps.weapon ]++;
 }
 if ( ent->client->weaponMode[ ent->client->ps.weapon ] > 2 || ent->client->weaponMode[ ent->client->ps.weapon ] >2 )
   ent->client->weaponMode[ ent->client->ps.weapon ] =0;


 bg_weaponlist[ ent->client->ps.weapon ].weapMode[ent->client->ps.clientNum] = ent->client->ps.stats[STAT_MODE]= ent->client->weaponMode[ ent->client->ps.weapon ];
 // G_Printf( "WeaponMode set to: %i\n", ent->client->weaponMode[ ent->client->ps.weapon ]);


 switch (ent->client->weaponMode[ ent->client->ps.weapon ]){


   case 0:
        weapmodes_save.string[ent->client->ps.weapon]= '0';
        ent->client->ps.pm_flags &= ~PMF_SINGLE_MODE;
        break;
   case 1:
     weapmodes_save.string[ent->client->ps.weapon]= '1';
     ent->client->ps.pm_flags |= PMF_SINGLE_MODE;
     break;
    case 2:
      weapmodes_save.string[ent->client->ps.weapon]= '2';
      ent->client->ps.pm_flags &= ~PMF_SINGLE_MODE;
      break;
    default:
      weapmodes_save.string[ent->client->ps.weapon]= '2';

      trap_Cvar_Set( "weapmodes_save", ent->client->weaponModeChar);
  //    return;
  }
 // G_Printf( "weapmodes_save.string: %s\n", weapmodes_save.string );
  //for ( i=15; i>0; i--){
   // G_Printf( "weapmodes_save.string[%i]: %c\n", i, weapmodes_save.string[i]);

  //}

//  G_Printf( "weaponmodes_save set to: %c\n", weapmodes_save.string[ent->client->ps.weapon]);
  //if (  ent->client->weaponMode[ ent->client->ps.weapon ] != 1 )
  //  ent->parent->client->ps.pm_flags &= ~PMF_SINGLE_SHOT;


  if ( ent->client->weaponMode[ ent->client->ps.weapon ] == 2)
    ent->client->weaponMode[ ent->client->ps.weapon ] =-1;


}
/*
==================
  Cmd_Reload
==================
*/
void Cmd_Reload( gentity_t *ent )       {


  int amt;
  int ammotoadd;

  amt = RoundCount(ent->client->ps.weapon);
  ammotoadd = amt;

  if (BG_Grenade(ent->client->ps.weapon))
    return;

  //if (ent->client->ps.ammo[ent->client->ps.weapon] == 0 || ent->client->ps.weapon == WP_KNIFE ) return;
  if ( bg_weaponlist[ent->client->ps.weapon].numClips[ent->client->ps.clientNum] == 0 || ent->client->ps.weapon == WP_KNIFE ) return;
  if (ent->client->ps.weapon == WP_SPAS && bg_weaponlist[ent->client->ps.weapon].rounds[ent->client->ps.clientNum] > 7 ){
    return;
  }
  if (ent->client->ps.weapon == WP_SPAS && bg_weaponlist[ent->client->ps.weapon].rounds[ent->client->ps.clientNum] <= 7 ){
    if (ent->client->ps.weaponTime >0 ) return;
    ent->client->ps.weaponstate = WEAPON_RELOADING_START;
    return;
  }

  ent->client->ps.weaponstate = WEAPON_RELOADING_START;
  if (bg_weaponlist[ent->client->ps.weapon].rounds[ent->client->ps.clientNum] > 0)  {
    ammotoadd -= bg_weaponlist[ent->client->ps.weapon].rounds[ent->client->ps.clientNum];
  }

  //if (ent->client->ps.ammo[ent->client->ps.weapon] > 0)   {
    if ( bg_weaponlist[ent->client->ps.weapon].numClips[ent->client->ps.clientNum] > 0)   {
    bg_weaponlist[ent->client->ps.weapon].numClips[ent->client->ps.clientNum]--;
    //ent->client->ps.ammo[ent->client->ps.weapon]--;
  }

 // ent->client->ps.ammo[ent->client->ps.weapon]+= ammotoadd;
  bg_weaponlist[ent->client->ps.weapon].rounds[ent->client->ps.clientNum] += ammotoadd;
 // ent->client->clipammo[ent->client->ps.weapon]+= ammotoadd;


}


/*
===============
FireWeapon
===============
*/
void FireWeapon( gentity_t *ent ) {


  if ( bg_weaponlist[ent->client->ps.weapon].rounds[ent->client->ps.clientNum] != -1
       || ent->s.weapon != WP_KNIFE
       ||!(BG_Grenade(ent->client->ps.weapon)) ) {
    bg_weaponlist[ent->client->ps.weapon].rounds[ent->client->ps.clientNum]--;
  }
  if (BG_Grenade(ent->client->ps.weapon))
        bg_weaponlist[ent->client->ps.weapon].numClips[ent->client->ps.clientNum]--;

  if ( (BG_Grenade(ent->client->ps.weapon))
        && bg_weaponlist[ent->client->ps.weapon].numClips[ent->client->ps.clientNum] <= 0){


    //bg_inventory.sort[ent->client->ps.clientNum][NADE] = WP_NONE;

        }
        s_quadFactor = 1;

        // track shots taken for accuracy tracking.  Grapple is not a weapon and gauntet is just not tracked
        if(  ent->s.weapon != WP_KNIFE ) {

        }

        // set aiming directions
        AngleVectors (ent->client->ps.viewangles, forward, right, up);

        CalcMuzzlePointOrigin ( ent, ent->client->oldOrigin, forward, right, up, muzzle );

        // fire the specific weapon
        switch( ent->s.weapon ) {
        case WP_KNIFE:
                //Weapon_Gauntlet( ent );
                break;
                case WP_SPAS:
                        weapon_supershotgun_fire( ent );
        case WP_M4:
                        Bullet_Fire( ent, MACHINEGUN_SPREAD, M4_DAMAGE );
                break;
                case WP_MP5K:
                        Bullet_Fire( ent, MACHINEGUN_SPREAD, MP5K_DAMAGE );
                        break;
                case WP_UMP45:
                  Bullet_Fire( ent, MACHINEGUN_SPREAD, UMP45_DAMAGE );
                        break;
                case WP_PSG1:
                        Bullet_Fire( ent, MACHINEGUN_SPREAD, MACHINEGUN_DAMAGE );
                        break;
                case WP_SR8:
                        Bullet_Fire( ent, MACHINEGUN_SPREAD, MACHINEGUN_DAMAGE );
                        break;
                case WP_G36:
                        Bullet_Fire( ent, MACHINEGUN_SPREAD, G36_DAMAGE );
                        break;
                case WP_LR300:
                  Bullet_Fire( ent, MACHINEGUN_SPREAD, LR300_DAMAGE );
                        break;
                case WP_AK103:
                        Bullet_Fire( ent, MACHINEGUN_SPREAD, AK103_DAMAGE );
                        break;
                case WP_NEGEV:
                        Bullet_Fire( ent, MACHINEGUN_SPREAD, MACHINEGUN_DAMAGE );
                        break;
                case WP_DEAGLE:
                        Bullet_Fire( ent, DEAGLE_SPREAD, DEAGLE_DAMAGE );
                        break;
                case WP_BERETTA:
                        Bullet_Fire( ent, MACHINEGUN_SPREAD, BERETTA_DAMAGE );
                        break;
        case WP_HK69:
                        weapon_grenadelauncher_fire( ent );
                        break;
                case WP_HE:
                        //weapon_grenade_arm( ent );
                        weapon_grenade_throw( ent );
                        break;
                case WP_SMOKE:
                        weapon_smoke_throw( ent );
                break;
        default:
// FIXME                G_Error( "Bad ent->s.weapon" );
                break;
        }
}


#ifdef MISSIONPACK

/*
===============
KamikazeRadiusDamage
===============
*/
static void KamikazeRadiusDamage( vec3_t origin, gentity_t *attacker, float damage, float radius ) {
        float           dist;
        gentity_t       *ent;
        int                     entityList[MAX_GENTITIES];
        int                     numListedEntities;
        vec3_t          mins, maxs;
        vec3_t          v;
        vec3_t          dir;
        int                     i, e;

        if ( radius < 1 ) {
                radius = 1;
        }

        for ( i = 0 ; i < 3 ; i++ ) {
                mins[i] = origin[i] - radius;
                maxs[i] = origin[i] + radius;
        }

        numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

        for ( e = 0 ; e < numListedEntities ; e++ ) {
                ent = &g_entities[entityList[ e ]];

                if (!ent->takedamage) {
                        continue;
                }

                // dont hit things we have already hit
                if( ent->kamikazeTime > level.time ) {
                        continue;
                }

                // find the distance from the edge of the bounding box
                for ( i = 0 ; i < 3 ; i++ ) {
                        if ( origin[i] < ent->r.absmin[i] ) {
                                v[i] = ent->r.absmin[i] - origin[i];
                        } else if ( origin[i] > ent->r.absmax[i] ) {
                                v[i] = origin[i] - ent->r.absmax[i];
                        } else {
                                v[i] = 0;
                        }
                }

                dist = VectorLength( v );
                if ( dist >= radius ) {
                        continue;
                }

//              if( CanDamage (ent, origin) ) {
                        VectorSubtract (ent->r.currentOrigin, origin, dir);
                        // push the center of mass higher than the origin so players
                        // get knocked into the air more
                        dir[2] += 24;
                        G_Damage( ent, NULL, attacker, dir, origin, damage, DAMAGE_RADIUS|DAMAGE_NO_TEAM_PROTECTION, MOD_KAMIKAZE );
                        ent->kamikazeTime = level.time + 3000;
//              }
        }
}

/*
===============
KamikazeShockWave
===============
*/
static void KamikazeShockWave( vec3_t origin, gentity_t *attacker, float damage, float push, float radius ) {
        float           dist;
        gentity_t       *ent;
        int                     entityList[MAX_GENTITIES];
        int                     numListedEntities;
        vec3_t          mins, maxs;
        vec3_t          v;
        vec3_t          dir;
        int                     i, e;

        if ( radius < 1 )
                radius = 1;

        for ( i = 0 ; i < 3 ; i++ ) {
                mins[i] = origin[i] - radius;
                maxs[i] = origin[i] + radius;
        }

        numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

        for ( e = 0 ; e < numListedEntities ; e++ ) {
                ent = &g_entities[entityList[ e ]];

                // dont hit things we have already hit
                if( ent->kamikazeShockTime > level.time ) {
                        continue;
                }

                // find the distance from the edge of the bounding box
                for ( i = 0 ; i < 3 ; i++ ) {
                        if ( origin[i] < ent->r.absmin[i] ) {
                                v[i] = ent->r.absmin[i] - origin[i];
                        } else if ( origin[i] > ent->r.absmax[i] ) {
                                v[i] = origin[i] - ent->r.absmax[i];
                        } else {
                                v[i] = 0;
                        }
                }

                dist = VectorLength( v );
                if ( dist >= radius ) {
                        continue;
                }

//              if( CanDamage (ent, origin) ) {
                        VectorSubtract (ent->r.currentOrigin, origin, dir);
                        dir[2] += 24;
                        G_Damage( ent, NULL, attacker, dir, origin, damage, DAMAGE_RADIUS|DAMAGE_NO_TEAM_PROTECTION, MOD_KAMIKAZE );
                        //
                        dir[2] = 0;
                        VectorNormalize(dir);
                        if ( ent->client ) {
                                ent->client->ps.velocity[0] = dir[0] * push;
                                ent->client->ps.velocity[1] = dir[1] * push;
                                ent->client->ps.velocity[2] = 100;
                        }
                        ent->kamikazeShockTime = level.time + 3000;
//              }
        }
}

/*
===============
KamikazeDamage
===============
*/
static void KamikazeDamage( gentity_t *self ) {
        int i;
        float t;
        gentity_t *ent;
        vec3_t newangles;

        self->count += 100;

        if (self->count >= KAMI_SHOCKWAVE_STARTTIME) {
                // shockwave push back
                t = self->count - KAMI_SHOCKWAVE_STARTTIME;
                KamikazeShockWave(self->s.pos.trBase, self->activator, 25, 400, (int) (float) t * KAMI_SHOCKWAVE_MAXRADIUS / (KAMI_SHOCKWAVE_ENDTIME - KAMI_SHOCKWAVE_STARTTIME) );
        }
        //
        if (self->count >= KAMI_EXPLODE_STARTTIME) {
                // do our damage
                t = self->count - KAMI_EXPLODE_STARTTIME;
                KamikazeRadiusDamage( self->s.pos.trBase, self->activator, 400, (int) (float) t * KAMI_BOOMSPHERE_MAXRADIUS / (KAMI_IMPLODE_STARTTIME - KAMI_EXPLODE_STARTTIME) );
        }

        // either cycle or kill self
        if( self->count >= KAMI_SHOCKWAVE_ENDTIME ) {
                G_FreeEntity( self );
                return;
        }
        self->nextthink = level.time + 100;

        // add earth quake effect
        newangles[0] = crandom() * 2;
        newangles[1] = crandom() * 2;
        newangles[2] = 0;
        for (i = 0; i < MAX_CLIENTS; i++)
        {
                ent = &g_entities[i];
                if (!ent->inuse)
                        continue;
                if (!ent->client)
                        continue;

                if (ent->client->ps.groundEntityNum != ENTITYNUM_NONE) {
                        ent->client->ps.velocity[0] += crandom() * 120;
                        ent->client->ps.velocity[1] += crandom() * 120;
                        ent->client->ps.velocity[2] = 30 + random() * 25;
                }

                ent->client->ps.delta_angles[0] += ANGLE2SHORT(newangles[0] - self->movedir[0]);
                ent->client->ps.delta_angles[1] += ANGLE2SHORT(newangles[1] - self->movedir[1]);
                ent->client->ps.delta_angles[2] += ANGLE2SHORT(newangles[2] - self->movedir[2]);
        }
        VectorCopy(newangles, self->movedir);
}

/*
===============
G_StartKamikaze
===============
*/
void G_StartKamikaze( gentity_t *ent ) {
        gentity_t       *explosion;
        gentity_t       *te;
        vec3_t          snapped;

        // start up the explosion logic
        explosion = G_Spawn();

        explosion->s.eType = ET_EVENTS + EV_KAMIKAZE;
        explosion->eventTime = level.time;

        if ( ent->client ) {
                VectorCopy( ent->s.pos.trBase, snapped );
        }
        else {
                VectorCopy( ent->activator->s.pos.trBase, snapped );
        }
        SnapVector( snapped );          // save network bandwidth
        G_SetOrigin( explosion, snapped );

        explosion->classname = "kamikaze";
        explosion->s.pos.trType = TR_STATIONARY;

        explosion->kamikazeTime = level.time;

        explosion->think = KamikazeDamage;
        explosion->nextthink = level.time + 100;
        explosion->count = 0;
        VectorClear(explosion->movedir);

        trap_LinkEntity( explosion );

        if (ent->client) {
                //
                explosion->activator = ent;
                //
                ent->s.eFlags &= ~EF_KAMIKAZE;
                // nuke the guy that used it
                G_Damage( ent, ent, ent, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_KAMIKAZE );
        }
        else {
                if ( !strcmp(ent->activator->classname, "bodyque") ) {
                        explosion->activator = &g_entities[ent->activator->r.ownerNum];
                }
                else {
                        explosion->activator = ent->activator;
                }
        }

        // play global sound at all clients
        te = G_TempEntity(snapped, EV_GLOBAL_TEAM_SOUND );
        te->r.svFlags |= SVF_BROADCAST;
        te->s.eventParm = GTS_KAMIKAZE;
}
#endif



/*
============
 
Laser Sight 
 * Xamis
============
*/

void Laser_Gen( gentity_t *ent, int type )      {
        gentity_t       *las;
        int oldtype;

        if ( ent->client->lasersight) {
                  oldtype = ent->client->lasersight->s.eventParm;
                  G_FreeEntity( ent->client->lasersight );
                  ent->client->lasersight = NULL;
                  if (oldtype == type)
                          return;
        }

        las = G_Spawn();

        las->nextthink = level.time;
        las->think = Laser_Think;
        las->r.ownerNum = ent->s.number;
        las->parent = ent;
        las->s.eType = ET_LASER;

        //Lets tell it if flashlight or laser

                las->s.eventParm = 1; //tells CG that it is a laser sight
                las->classname = "lasersight";

        ent->client->lasersight = las;
}

void Laser_Think( gentity_t *self )     {
        vec3_t          end, start, forward, up;
        trace_t         tr;

        //If Player Dies, You Die -> now thanks to Camouflage!
        if (self->parent->client->ps.pm_type == PM_DEAD)  {
                G_FreeEntity(self);
                return;
        }

        //Set Aiming Directions
        AngleVectors(self->parent->client->ps.viewangles, forward, right, up);
        CalcMuzzlePoint(self->parent, forward, right, up, start);
        VectorMA (start, 8192, forward, end);

        //Trace Position
        trap_Trace (&tr, start, NULL, NULL, end, self->parent->s.number, MASK_SHOT );

        //Did you not hit anything?
        if (tr.surfaceFlags & SURF_NOIMPACT || tr.surfaceFlags & SURF_SKY)      {
                self->nextthink = level.time;
                trap_UnlinkEntity(self);
                return;
        }

        //Move you forward to keep you visible
        if (tr.fraction != 1)   VectorMA(tr.endpos,-4,forward,tr.endpos);

        //Set Your position
        VectorCopy( tr.endpos, self->r.currentOrigin );
        VectorCopy( tr.endpos, self->s.pos.trBase );

        vectoangles(tr.plane.normal, self->s.angles);

        trap_LinkEntity(self);

        //Prep next move
        self->nextthink = level.time;
}