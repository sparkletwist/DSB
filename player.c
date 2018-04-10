#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "global_data.h"
#include "champions.h"
#include "uproto.h"
#include "gproto.h"
#include "monster.h"
#include "istack.h"
#include "mparty.h"

extern struct global_data gd;
extern struct inventory_info gii;
extern struct inst *oinst[NUM_INST];
extern struct dungeon_level *dun;

extern istack istaparty;
extern FILE *errorlog;
extern int Gmparty_flags;

unsigned int *level_table;

void party_update(void) {
    int tval;
    int n;
    
    lc_parm_int("sys_tick", 1, gd.updateclock);
    
    for (n=0;n<4;n++) {
        if (gd.idle_t[n])
            --gd.idle_t[n];
    }
        
    gd.updateclock++;
    if (gd.updateclock == 50) {
        gd.updateclock = 0;
        gd.lua_bool_hack = 1;
        lc_parm_int("sys_update", 1, LHTRUE);
        
    } else if (*gd.gl_viewstate == VIEWSTATE_SLEEPING) {
        if (gd.updateclock == 12 || gd.updateclock == 25 ||
            gd.updateclock == 38)
        {
            gd.lua_bool_hack = 1;
            lc_parm_int("sys_update", 1, LHFALSE);   
        }     
    }
}

void party_moveto(int ap, int lev, int xx, int yy, int lk, int pcc_type) {
    int oldlev, oldx, oldy, oldface;
    int force_move = 0;
    int iface;
    
    // PCC_ACTUALMOVE being 0, so PCC_ACTUALMOVE sets actual_move to 1
    int actual_move = !pcc_type;
    
    onstack("party_moveto");
    
    if (gd.p_lev[ap] < 0)
        force_move = 1;
    
    oldlev = gd.p_lev[ap];    
    oldx = gd.p_x[ap];
    oldy = gd.p_y[ap];
    gd.p_x[ap] = xx;
    gd.p_y[ap] = yy;
    gd.p_lev[ap] = lev;
    set_3d_soundcoords();    

    if (pcc_type == PCC_MOVEOFF || (actual_move && (!lk && !force_move))) {
        to_tile(oldlev, oldx, oldy, gd.p_face[ap], -1, T_FROM_TILE);      
        /*
        if (pcc_type == PCC_MOVEOFF)
            allegro_message("PCC_MOVEOFF: moving off tile %d %d %d (%d)", lev, xx, yy, gd.party[0]);
        else
            allegro_message("actual move: moving off tile %d %d %d (%d)", lev, xx, yy, gd.party[0]); 
        */   
    }
    
    if (actual_move) {        
        dun[lev].t[yy][xx].w |= 2;
        
        // no collision check here, if we get to this point,
        // the parties ARE colliding, so we might as well
        // merge them rather than risk weirdness
        iface = gd.p_face[gd.a_party];
        mparty_merge(ap);
    }
    
    if (pcc_type == PCC_MOVEON || (actual_move && !lk)) {
        ist_cap_top(&istaparty);
        ist_push(&istaparty, ap);   
        to_tile(lev, xx, yy, gd.p_face[ap], -1, T_TO_TILE);
        ist_pop(&istaparty);
        ist_check_top(&istaparty);
        /*
        if (pcc_type == PCC_MOVEON)
            allegro_message("PCC_MOVEON: moving on tile %d %d %d (%d)", lev, xx, yy, gd.party[0]);
        else
            allegro_message("actual move: moving on tile %d %d %d (%d)", lev, xx, yy, gd.party[0]);    
        */
    }   
    
    if (actual_move) {
        monster_telefrag(lev, xx, yy);
        
        if (ap != gd.a_party)
            mparty_integrity_check(ap, iface);
        else
            mparty_who_is_here();
    }
    
    VOIDRETURN();
}

void exec_party_place(int ap, int lev, int xx, int yy, int facedir) {
    int lk;

    onstack("exec_party_place");

    if (!gd.dungeon_loading) {
        if (lev != gd.p_lev[ap]) {
            party_enter_level(ap, lev);
        }
    }

    lk = (gd.p_lev[ap] == lev && gd.p_x[ap] == xx && gd.p_y[ap] == yy);
    
    party_moveto(ap, lev, xx, yy, lk, PCC_ACTUALMOVE);

    if (facedir != gd.p_face[ap]) {
        int oldface = gd.p_face[ap];

        gd.p_face[ap] = facedir;
        // this should be 0
        // otherwise teleporting onto the tile would cause a double-trigger
        // (and trigger tile on_turn events, which is... strange)
        change_facing(ap, oldface, facedir, 0);
    }

    VOIDRETURN();
}

int findppos(int charid) {
    int i;

    for(i=0;i<4;i++) {
        if (gd.party[i] == charid)
            return i;
    }
    
    return -1;
    
}

void give_xp(unsigned int cnum, unsigned int wclass, 
    unsigned int subc, unsigned int xpamt)
{   
    onstack("give_xp");
    
    if (UNLIKELY(cnum < 0)) {
        puke_bad_subscript("gd.champs", cnum);
        VOIDRETURN();
    } else if (UNLIKELY(wclass < 0)) {
        puke_bad_subscript("xp", cnum);
        VOIDRETURN();
    }
    
    gd.champs[cnum].xp[wclass] += xpamt;
    if (subc)
        gd.champs[cnum].sub_xp[wclass][subc-1] += xpamt;
    
    VOIDRETURN();
}

int d_ppos_collision(int ap, unsigned int inst, struct inst *p_inst) {
    int abstilepos, relpos, whothere;
    
    onstack("d_ppos_collision");
    
    if (p_inst == NULL)
        p_inst = oinst[inst];
                
    if (gd.p_lev[ap] >= 0 && gd.p_lev[ap] == p_inst->level) {
        if (gd.p_x[ap] == p_inst->x && gd.p_y[ap] == p_inst->y) {
            int hv;
            
            if ((p_inst->openshot & 1))
                RETURN(-1);

            abstilepos = p_inst->tile;
            relpos = ((abstilepos+4) - gd.p_face[ap]) % 4;
            whothere = gd.guypos[relpos/2][(relpos%3 > 0)][ap];

            if (whothere) {
                hv = lc_parm_int("sys_party_col", 3, inst, whothere-1, ap);
                if (hv)
                    RETURN(whothere-1);
            }
        
            RETURN(-1);
        }
    }
    
    RETURN(-1);
}

void dead_character(unsigned int who) {
    int m_o;
    int ppos;
    int n;
    int inp;
    int dest;
    int mdrop = gd.mouseobj;
    int mddrop = -1;
    int dead_party;
    
    onstack("dead_character");
    
    ppos = findppos(who);
    dead_party = mparty_containing(ppos);
    
    if (UNLIKELY(ppos == -1)) {
        recover_error("Killed invalid character");
        VOIDRETURN();
    }
    
    if (*gd.gl_viewstate == VIEWSTATE_INVENTORY) {
        if (gd.who_look == ppos)
            exit_inventory_view();
    }
    
    if (gd.who_method == (ppos+1))
        clear_method();
       
    // find a new spellcaster   
    if (gd.whose_spell == ppos) {
        for(n=0;n<4;++n) {
            if (n == ppos) continue;
            if (gd.party[n] && IsNotDead(n)) {
                gd.whose_spell = n;
                break;
            }
        }        
    }

    // find a new leader if necessary
    if (gd.leader == ppos) {
        gd.leader = -1;
        for(n=0;n<4;++n) {
            if (n == ppos) continue;
            if (gd.party[n] && IsNotDead(n) &&
                mparty_containing(n) == gd.a_party)
            {
                gd.leader = n;
                mdrop = -1;
                break;
            }
        }
    
        // only bother searching when there are multi-parties
        if (GMP_ENABLED()) {
            // nobody left in the active party
            if (gd.leader == -1) {
                for(n=0;n<4;++n) {
                    if (n == ppos) continue;
                    if (gd.party[n] && IsNotDead(n))
                    {
                        gd.leader = n;
                        if (gd.whose_spell == ppos)
                            gd.whose_spell = n;
                        change_partyview(mparty_containing(n));
                        if (gd.mouseobj)
                            mdrop = gd.mouseobj;
                        break;
                    }
                }
            }
        }
    }
    
    m_o = gd.mouseobj;
    if (m_o) mouse_obj_drop(gd.mouseobj);
    ist_push(&istaparty, dead_party);
    if (mdrop > 0) mddrop = mdrop;
    lc_parm_int("sys_character_die", 3, ppos, who, mddrop);
    ist_pop(&istaparty);
    if (mdrop > -1) m_o = 0;

    memset(gd.champs[who-1].condition, 0, sizeof(int)*gd.num_conds);
    timer_smash(who);

    inp = remove_from_all_pos(ppos+1);
    if (gd.mouse_guy == inp+1) {
        gd.mouse_guy = 0;
    }

    if (gd.leader == -1) {
        int everyones_still_dead = 1;
        int li;
               
        li = lc_parm_int("sys_everyone_dead", 1, who);
        
        for(n=0;n<4;++n) {
            if (gd.party[n] && IsNotDead(n)) {
                everyones_still_dead = 0;
                gd.leader = n;
                gd.whose_spell = n;
                gd.a_party = mparty_containing(n);
                break;
            }
        }
        
        if (everyones_still_dead) {
            if (li > 0) {
                gd.everyone_dead = 5+li;
            }
            clear_mouse_override();
            VOIDRETURN();
        }
    }
        
    dest = mparty_destroy_empty(inp);
    mparty_who_is_here();
    if (m_o) mouse_obj_grab(m_o);

    VOIDRETURN();
}

void revive_character(unsigned int who) {
    int n;
    int ppos;
    
    onstack("revive_character");
    
    ppos = findppos(who);
    put_into_pos(gd.a_party, ppos+1);
    
    for(n=0;n<gii.max_injury;++n) {
        gd.champs[who-1].injury[n] = 0;
    }
    
    VOIDRETURN();
}

void setbarval(unsigned int who, int which, int nval) {
    int oldval;
    
    onstack("setbarval");
    
    if (UNLIKELY(who < 1)) {
        puke_bad_subscript("gd.champs", who-1);
        VOIDRETURN();
    }
    
    oldval = gd.champs[who-1].bar[which];
    gd.champs[who-1].bar[which] = nval;
    
    if (which == BAR_HEALTH) {
        if (oldval > 0 && nval <= 0)
            dead_character(who);
        else if (oldval <= 0 && nval > 0)
            revive_character(who);
    }
        
    VOIDRETURN();
}

void go_to_sleep(void) {
    int fs;
    
    fs = lc_parm_int("sys_forbid_sleep", 0);
    if (fs)
        return;
        
    if (*gd.gl_viewstate == VIEWSTATE_INVENTORY)
        exit_inventory_view();
        
    *gd.gl_viewstate = VIEWSTATE_SLEEPING;
    lc_parm_int("sys_sleep", 0);
}

void wake_up(void) {
    gd.need_cz = 1;
    *gd.gl_viewstate = VIEWSTATE_DUNGEON;
    lc_parm_int("sys_wake_up", 0);
}

// necessary to avoid trigger errors when the party changes
// while standing on top of a trigger
void party_composition_changed(int ap, int pcc_type, int real_level) {
    char debugstr[200];
    int lev, xx, yy, facedir;
    
    onstack("party_composition_changed");
    stackbackc('(');
    sprintf(debugstr, "%s, %d, %d, %d", (gd.queue_rc)? "Q" : "X",
        ap, pcc_type, real_level);
    v_onstack(debugstr);
    addstackc(')');
      
    if (real_level >= 0) {
        // if it's getting queued, then this will mess everything up
        if (!gd.queue_rc) {
            lev = real_level;   
            xx = gd.p_x[ap];
            yy = gd.p_y[ap];
            facedir = gd.p_face[ap];   
            party_moveto(ap, lev, xx, yy, 0, pcc_type);
        }
    }
       
    VOIDRETURN();
}

struct inst_loc *party_extcollision_forward(int ap, int oface, int tx, int ty) {
    struct dungeon_level *dd;
    struct inst_loc *il;
    struct inst_loc *PUSHCOL = NULL;
    int oface1 = (oface+1)%4;
    int oface2 = (oface+2)%4;
    int oface3 = (oface+3)%4;
    int py = gd.p_y[ap];
    int px = gd.p_x[ap];
    
    onstack("party_extcollision_forward");
    
    dd = &(dun[gd.p_lev[ap]]);
    
    if (gd.guypos[0][0][ap]) {
        il = dd->t[py][px].il[oface];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface, oface, -1);
        
        il = dd->t[ty][tx].il[oface3];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface, oface, -1);
        
    } else if (gd.guypos[1][0][ap]) {
        il = dd->t[py][px].il[oface3];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface3, oface, -1);
        
        il = dd->t[py][px].il[oface];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface3, oface, -1);
    }

    if (gd.guypos[0][1][ap]) {
        il = dd->t[py][px].il[oface1];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface1, oface, -1);

        il = dd->t[ty][tx].il[oface2];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface1, oface, -1);
        
    } else if (gd.guypos[1][1][ap]) {      
        il = dd->t[py][px].il[oface2];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface2, oface, -1);

        il = dd->t[py][px].il[oface1];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface2, oface, -1);
    }

    RETURN(PUSHCOL);
}

struct inst_loc *party_extcollision_back(int ap, int oface, int tx, int ty) {
    struct dungeon_level *dd;
    struct inst_loc *il;
    struct inst_loc *PUSHCOL = NULL;
    int oface1 = (oface+1)%4;
    int oface2 = (oface+2)%4;
    int oface3 = (oface+3)%4;
    int py = gd.p_y[ap];
    int px = gd.p_x[ap];
    
    onstack("party_extcollision_back");
    
    dd = &(dun[gd.p_lev[ap]]);
    
    if (gd.guypos[1][0][ap]) {
        il = dd->t[py][px].il[oface1];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface1, oface, -1);
        
        il = dd->t[ty][tx].il[oface2];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface1, oface, -1);
        
    } else if (gd.guypos[0][0][ap]) {
        il = dd->t[py][px].il[oface2];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface2, oface, -1);
        
        il = dd->t[py][px].il[oface1];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface2, oface, -1);
    }

    if (gd.guypos[1][1][ap]) {
        il = dd->t[py][px].il[oface];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface, oface, -1);

        il = dd->t[ty][tx].il[oface3];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface, oface, -1);
        
    } else if (gd.guypos[0][1][ap]) {       
        il = dd->t[py][px].il[oface3];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface3, oface, -1);

        il = dd->t[py][px].il[oface];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface3, oface, -1);
    }

    RETURN(PUSHCOL);
}

struct inst_loc *party_extcollision_right(int ap, int oface, int tx, int ty) {
    struct dungeon_level *dd;
    struct inst_loc *il;
    struct inst_loc *PUSHCOL = NULL;
    int oface1 = (oface+1)%4;
    int oface2 = (oface+2)%4;
    int oface3 = (oface+3)%4;
    int py = gd.p_y[ap];
    int px = gd.p_x[ap];
    
    onstack("party_extcollision_right");
    
    dd = &(dun[gd.p_lev[ap]]);
    
    if (gd.guypos[0][1][ap]) {
        il = dd->t[py][px].il[oface];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface, oface, -1);
        
        il = dd->t[ty][tx].il[oface3];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface, oface, -1);
        
    } else if (gd.guypos[0][0][ap]) {
        il = dd->t[py][px].il[oface3];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface3, oface, -1);
        
        il = dd->t[py][px].il[oface];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface3, oface, -1);
    }

    if (gd.guypos[1][1][ap]) {
        il = dd->t[py][px].il[oface1];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface1, oface, -1);

        il = dd->t[ty][tx].il[oface2];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface1, oface, -1);
        
    } else if (gd.guypos[1][0][ap]) {      
        il = dd->t[py][px].il[oface2];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface2, oface, -1);

        il = dd->t[py][px].il[oface1];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface2, oface, -1);
    }

    RETURN(PUSHCOL);
}

struct inst_loc *party_extcollision_left(int ap, int oface, int tx, int ty) {
    struct dungeon_level *dd;
    struct inst_loc *il;
    struct inst_loc *PUSHCOL = NULL;
    int oface1 = (oface+1)%4;
    int oface2 = (oface+2)%4;
    int oface3 = (oface+3)%4;
    int py = gd.p_y[ap];
    int px = gd.p_x[ap];
    
    onstack("party_extcollision_left");
    
    dd = &(dun[gd.p_lev[ap]]);
    
    if (gd.guypos[0][0][ap]) {
        il = dd->t[py][px].il[oface1];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface1, oface, -1);
        
        il = dd->t[ty][tx].il[oface2];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface1, oface, -1);
        
    } else if (gd.guypos[0][1][ap]) {
        il = dd->t[py][px].il[oface2];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface2, oface, -1);
        
        il = dd->t[py][px].il[oface1];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface2, oface, -1);
    }

    if (gd.guypos[1][0][ap]) {
        il = dd->t[py][px].il[oface];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface, oface, -1);

        il = dd->t[ty][tx].il[oface3];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface, oface, -1);
        
    } else if (gd.guypos[1][1][ap]) {       
        il = dd->t[py][px].il[oface3];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface3, oface, -1);

        il = dd->t[py][px].il[oface];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, oface3, oface, -1);
    }

    RETURN(PUSHCOL);
}

