#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "objects.h"
#include "uproto.h"
#include "lproto.h"
#include "istack.h"
#include "champions.h"
#include "gproto.h"
#include "timer_events.h"
#include "global_data.h"
#include "monster.h"
#include "arch.h"
#include "gamelock.h"

struct buffered_msg {
    unsigned int inst;
    int msgtype;
    int datafield;
    int rc;
    unsigned int id_sender;
    int pstate;
    int xdata;
    struct buffered_msg *n;
};
struct buffered_msg *MSG_BUFFER = NULL;
struct buffered_msg *MSG_BUFFER_LAST = NULL;

struct condition_desc *i_conds = NULL;
struct condition_desc *p_conds = NULL;
extern struct timer_event *te;
extern struct timer_event *a_te;
extern struct timer_event *new_te;

extern struct global_data gd;
extern struct dungeon_level *dun;
extern lua_State *LUA;
extern struct inst *oinst[NUM_INST];

extern istack istaparty;

extern FILE *errorlog;

int grcnt = 0;

// uses 1-based Lua-style references to characters!
void timer_smash(int who) {
    destroy_assoc_timer(who, EVENT_COND_UPDATE, -2);
}

int update_cond(struct timer_event *cte) {
    int rv = 0;
    int luaret;
    struct condition_desc *a_cond;
    
    onstack("update_cond");
    
    if (cte->inst == 9999)
        a_cond = &(p_conds[cte->data]);
    else
        a_cond = &(i_conds[cte->data]);
        
    lua_rawgeti(LUA, LUA_REGISTRYINDEX, a_cond->lua_reg);
    
    // party effect
    if (cte->inst == 9999) {
        int cstr = gd.partycond[cte->data];        
        lua_pushinteger(LUA, cstr);
        luaret = lc_call_topstack(1, "cond_party");        
        if (luaret <= 0) {
            rv = 1;
            luaret = 0;
        }      
        gd.partycond[cte->data] = luaret;
    } else {
        int cstr = gd.champs[cte->inst - 1].condition[cte->data];        
        lua_pushinteger(LUA, cte->inst);
        lua_pushinteger(LUA, cstr);
        luaret = lc_call_topstack(2, "cond");
        if (luaret <= 0) {
            rv = 1;
            luaret = 0;
        }      
        gd.champs[cte->inst - 1].condition[cte->data] = luaret;
    }
    
    RETURN(rv);
}

void do_damage_popup(unsigned int ppos, unsigned int d_amt, unsigned char dtype) {
    
    onstack("do_damage_popup");
    
    if (d_amt > 9990)
        d_amt = 9990;
        
    d_amt = (d_amt/10); 
    
    if (gd.showdam[ppos]) {
        if (dtype == gd.showdam_t[ppos]) {
            if (gd.ini_options & INIOPT_CUDAM) {
                gd.showdam[ppos] += d_amt;
                if (gd.showdam[ppos] > 999)
                    gd.showdam[ppos] = 999;
            } else {
                goto ADD_B_DAMAGE;
            }
            
            goto ADD_B_TTL;
        } else if (dtype < gd.showdam_t[ppos]) {
            goto ADD_B_DAMAGE;
        }
            
    } else
        goto ADD_B_DAMAGE;
        
    VOIDRETURN();
    
    ADD_B_DAMAGE:
    gd.showdam[ppos] = d_amt;
    gd.showdam_t[ppos] = dtype;
    
    ADD_B_TTL:
    gd.showdam_ttl[ppos] = 8;
    VOIDRETURN();               
} 

void calc_flyer_directions(struct inst *p_inst, int *tilechg, int *backtile) {
    *tilechg = 1;
    *backtile = 1;
    
    if (p_inst->tile == (p_inst->facedir+2)%4) {
        *tilechg = 3;
    } else if (p_inst->tile == p_inst->facedir) {
        *tilechg = 3;
        *backtile = 0;
    } else if (p_inst->tile == (p_inst->facedir+1)%4)
        *backtile = 0; 
}

int adv_flyer(unsigned int inst, struct timer_event *cte) {
    
    // makes flyers go into the adjacent square and just sit
    // (leave it set to 0 in normal circumstances)
    const int COLLISION_DEBUG_MODE = 0;
    
    struct inst *p_inst;
    int tilechg = 1;
    int backtile = 1;
    int tx, ty;
    int block = 0;
    int v = 0;
    int newtile;
    int col = -1;
    int pcol = -1;
    int ap = 0;
    int h_ap = 0;
    int is_openshot = 0;
    
    short pow_delta;
    short dam_delta;
    
    onstack("adv_flyer");
    
    p_inst = oinst[inst];
    if (p_inst == NULL)
        RETURN(1);
        
    if (p_inst->gfxflags & OF_INACTIVE) {
        RETURN(0);
    }
    
    tx = p_inst->x;
    ty = p_inst->y;
    newtile = p_inst->tile;
    is_openshot = p_inst->openshot;
    
    pow_delta = cte->data;
    dam_delta = cte->altdata;
    
    // collisions can happen at any power value > 0
    if (p_inst->flytimer > 0) {
        
        calc_flyer_directions(p_inst, &tilechg, &backtile);
        
        if (!COLLISION_DEBUG_MODE) {
            col = obj_collision(p_inst->level, tx, ty, p_inst->tile, inst, NULL);
            if (col)
                goto FLYER_COLLIDE;
    
            for (ap=0;ap<4;++ap) {
                pcol = d_ppos_collision(ap, inst, p_inst);
                if (pcol != -1) {
                    col = -1;
                    h_ap = ap;
                    goto FLYER_COLLIDE;
                }
            }
                
            if (backtile) {
                col = obj_collision(p_inst->level, tx, ty, DIR_CENTER, inst, NULL);
                if (col)
                    goto FLYER_COLLIDE;
            }
        }
        
        p_inst->openshot = 0;
    }
    
    // but only advance if we've got enough power left
    // (but always clear my own square)
    if (is_openshot ||
        (!COLLISION_DEBUG_MODE && (p_inst->flytimer >= pow_delta))) 
    {
        
        newtile = (newtile + tilechg) % 4;
        
        p_inst = oinst[inst];
        if (!p_inst)
            RETURN(1);
        
        if (!backtile) {
            int dx, dy;
            face2delta(p_inst->facedir, &dx, &dy);
            
            tx += dx;
            ty += dy;    
        }
        
        p_inst = oinst[inst];
        if (!p_inst)
            RETURN(1);

        if (member_begin_call(inst, "on_fly")) {
            int nils = 0;
            int ia_dest[5];
            
            luastacksize(10);
            
            lua_pushinteger(LUA, tx);
            lua_pushinteger(LUA, ty);
            lua_pushinteger(LUA, newtile);
            lua_pushinteger(LUA, p_inst->facedir);
            lua_pushinteger(LUA, p_inst->flytimer);

            member_finish_call(5, 5);

            nils = member_retrieve_stackparms(5, ia_dest);
            
            // if we've shifted actual tile, no shortcuts allowed
            if (ia_dest[0] != tx || ia_dest[1] != ty)
                backtile = 0;
                
            if (!nils) {
                tx = ia_dest[0];
                ty = ia_dest[1];
                newtile = ia_dest[2];
                p_inst->facedir = ia_dest[3];
                p_inst->flytimer = ia_dest[4];
                
                if (ia_dest[4] <= 0) {
                    drop_flying_inst(inst, p_inst);
                }
            }
            
            lstacktop();
        }
          
        if (!oinst[inst])
            RETURN(1);
            
         p_inst = oinst[inst];
    
        if (p_inst->level < 0) {
            drop_flying_inst(inst, p_inst);
            RETURN(1);
        }
    }
    
    block = check_for_wall(p_inst->level, tx, ty);
    
    if (!backtile) {
        oinst[inst]->gfxflags &= ~(G_LAUNCHED);
    }
    
    if (!block) {
        if (!backtile) {
            if (!is_openshot)
                i_to_tile(inst, T_FROM_TILE);
        }
            
        p_inst = oinst[inst];
        if (!p_inst)
            RETURN(1);

        if (!COLLISION_DEBUG_MODE && !is_openshot) {
            UDEC(p_inst->flytimer, pow_delta);
            UDEC(p_inst->damagepower, dam_delta);
        }

        // so objects that drop will trigger things properly,
        // give it a small bit of flight again to untrigger
        // things looking for flyers, and THEN let it drop
        if (p_inst->flytimer == 0) {
            // if it's not a backtile, it already got T_FROM_TILE'd above
            if (backtile) {
                p_inst->flytimer = 1;
                if (!is_openshot) {
                    i_to_tile(inst, T_FROM_TILE);
                }
                
                p_inst = oinst[inst];
                if (!p_inst) {
                    RETURN(1);
                }
                
                backtile = 0;
                drop_flying_inst(inst, p_inst);
            }
        }

        depointerize(inst);
        gd.depointerized_inst = inst;     
        place_instance(inst, p_inst->level, tx, ty, newtile, backtile);
        if (!oinst[inst]) {
            RETURN(1);
        }
        
        // warn alert monsters about what's incoming
        if (!COLLISION_DEBUG_MODE && p_inst->flytimer > 0) {
            int nchg, nback;
            int ntx = tx;
            int nty = ty;
            int ntl = newtile;
            
            calc_flyer_directions(p_inst, &nchg, &nback);
            
            ntl = (ntl + nchg) % 4;
            if (!nback) {
                int dx, dy;
                face2delta(p_inst->facedir, &dx, &dy);                
                ntx += dx;
                nty += dy;    
            } 
            
            // don't need to warn squares outside the map
            if (ntx >= 0 && nty >= 0) {          
                lc_parm_int("sys_warning_flying_impact", 5, inst, p_inst->level,
                    ntx, nty, ntl);
                if (nback) {
                    lc_parm_int("sys_warning_flying_impact", 5, inst, p_inst->level,
                        ntx, nty, DIR_CENTER);
                }
            }
        }
        
        goto FLYER_TIMERCHECK;
    }
    
    col = -1;
    
    FLYER_COLLIDE:
    ist_cap_top(&istaparty);
    ist_push(&istaparty, h_ap);
    if (!call_member_func3(inst, "on_impact", col, pcol)) {
        if (oinst[inst])
            lc_parm_int("sys_base_impact", 3, inst, col, pcol);
    }
    ist_pop(&istaparty);
    ist_check_top(&istaparty);
    
    if (!oinst[inst]) {
        RETURN(1);
    } else
        p_inst = oinst[inst];
    
    // remove it as a flyer, add it an object
    // (if it's somewhere valid!)
    if (p_inst->level >= 0) {
        if (p_inst->flytimer) {
            
            if (!is_openshot) {
                i_to_tile(inst, T_FROM_TILE);
            }
            
            p_inst = oinst[inst];
            if (!p_inst)
                RETURN(1);
            drop_flying_inst(inst, p_inst);
            i_to_tile(inst, T_TO_TILE);
            set_fdelta_to_begin_now(p_inst);

            if (!oinst[inst]) {
                RETURN(1);
            } else if (oinst[inst] != p_inst) {
                p_inst = oinst[inst];
            }
        }
    } else {
        drop_flying_inst(inst, p_inst);
    }

    FLYER_TIMERCHECK:
    if (p_inst->flytimer == 0) {
        struct obj_arch *p_arch;

        p_arch = Arch(p_inst->arch);
 
        v = 1;
        p_inst->openshot = 0;
        p_inst->flycontroller = NULL;
        p_inst->gfxflags &= ~(G_LAUNCHED);
   
        lc_parm_int("sys_flyer_drop", 1, inst);
        p_inst = oinst[inst];
        if (p_inst)
            tweak_thing(p_inst);
    }
    
    RETURN(v);
}

// called by qswap to update the animations
// also resets the attack frame (to avoid oob crashes)
void reset_monster_ext(struct inst *p_inst) {
    struct ai_core *ai_c;
    struct timer_event *cte;
    struct obj_arch *p_arch;
    
    onstack("reset_monster_ext");

    ai_c = p_inst->ai;
    cte = ai_c->controller;
    p_arch = Arch(p_inst->arch);
    
    ai_c->attack_anim = 0;
    
    cte->shift = queryluaint(p_arch, "shift_rate");
    cte->flip = queryluaint(p_arch, "flip_rate");
    cte->investigate = 4;
    
    VOIDRETURN();
}

void do_monster_incidentals(struct timer_event *cte) {
    struct inst *p_m_inst;
    
    p_m_inst = oinst[cte->inst];
    
    if (UNLIKELY(!p_m_inst))
        return;
    
    // special monsters shift and flip so party members appear to
    if (p_m_inst->level != LOC_PARCONSPC) {
        if (p_m_inst->level != gd.p_lev[gd.a_party])
            return;
    }
        
    onstack("do_monster_incidentals");
    
    if (cte->shift) {
        --cte->shift;
        if (cte->shift == 0) {
            struct obj_arch *m_arch = Arch(p_m_inst->arch);
            tweak_thing(p_m_inst);
            cte->shift = rqueryluaint(m_arch, "shift_rate");
            if (cte->shift > 2)
                cte->shift += DSBtrivialrand()%(cte->shift);
            call_member_func(cte->inst, "on_monster_shift", -1);
        }       
    }
    
    if (cte->flip) {
        --cte->flip;
        if (cte->flip == 0) {
            struct obj_arch *m_arch = Arch(p_m_inst->arch);
            
            oinst[cte->inst]->gfxflags ^= G_FLIP;
            cte->flip = rqueryluaint(m_arch, "flip_rate");
            if (cte->flip > 2)
                cte->flip += DSBtrivialrand()%(cte->flip);
            call_member_func(cte->inst, "on_monster_flip", -1);
        }
    }
    
    // special monsters won't investigate (it'll just crash)
    if (p_m_inst->level != LOC_PARCONSPC) {
        if (cte->investigate) {
            --cte->investigate;
            if (cte->investigate == 0) {
                cte->investigate = 2;
                // the boss is the only one who investigates
                // and only when a turn isn't pending
                if (p_m_inst->ai->boss == cte->inst) {
                    if (cte->time_until > 3) {
                        monster_investigates(cte->inst);
                    } else {
                        cte->investigate = 1;
                    }
                }
            }
        }
    }
    
    VOIDRETURN();    
}

int do_timer_event(struct timer_event *cte) {
    
    if (cte->type == EVENT_MOVE_FLYER) {
        int t = cte->flyreps;
        while (t > 0 && (adv_flyer(cte->inst, cte) == 0))
            t--;
            
        return (!!t);
    }
        
    else if (cte->type == EVENT_MONSTER)
        return (monster_acts(cte->inst));
        
    else if (cte->type == EVENT_COND_UPDATE)
        return (update_cond(cte));
        
    else if (cte->type == EVENT_DO_FUNCTION) {
        lua_rawgeti(LUA, LUA_REGISTRYINDEX, cte->data);
        lc_call_topstack(0, "do_timer_event");
        luaL_unref(LUA, LUA_REGISTRYINDEX, cte->data);
        return 1;
    }
    
    else if (cte->type == EVENT_SEND_MESSAGE) {
        deliver_msg(cte->inst, cte->data, cte->altdata, cte->sender,
            cte->pstate, cte->xdata);
        return 1;
    }
    
    return 0;
}


void set_inst_flycontroller(int inst, struct inst *p_inst) {
    struct timer_event *cte;
    
    onstack("set_inst_flycontroller"); 
    
    if (p_inst->flycontroller != NULL) {
        VOIDRETURN();
    }
    
    cte = te;
    while (cte != NULL) {
        if (cte->inst == inst && 
            cte->type == EVENT_MOVE_FLYER) 
        {
            p_inst->flycontroller = cte;
            VOIDRETURN();
        }
    }
    
    VOIDRETURN();   
}

void drop_flying_inst(int inst, struct inst *p_inst) {
    p_inst->flytimer = 0;
    p_inst->flycontroller = NULL;
    p_inst->gfxflags &= ~(G_LAUNCHED);
}

void append_new_queue_te(struct timer_event *ote, struct timer_event *nte) {
    onstack("append_new_queue_te");
    
    // well, this is sort of stupid.
    // ... but maybe someday we'll support other options.
    if (ote != te) {
        program_puke("Invalid append_new_queue_te");
        VOIDRETURN();      
    }
        
    if (!nte)
        VOIDRETURN();
        
    if (!ote) {
        te = nte;
    } else {
        while (ote->n != NULL)
            ote = ote->n;
        ote->n = nte;
    }   
    new_te = NULL;
    
    VOIDRETURN();
}

void run_timers(struct timer_event *cte, struct timer_event **p_root_te,
    int flags) 
{
    struct timer_event *pte;
    int clean_timers = 0;
    
    onstack("run_timers");
    
    gd.queue_monster_deaths++;
    
    while (cte != NULL) {
        
        if (cte->type != EVENT_DO_FUNCTION) {
            int larrayval = -1;
            
            if (gd.gl->lockc[LOCK_ALL_TIMERS]) {
                goto SKIP_TIMER_LOOP;
            }

            if (cte->type == EVENT_MOVE_FLYER)
                larrayval = LOCK_FLYERS;
            else if (cte->type == EVENT_COND_UPDATE)
                larrayval = LOCK_CONDITIONS;
            else if (cte->type == EVENT_SEND_MESSAGE)
                larrayval = LOCK_MESSAGES;
                
            if (larrayval > -1) {
                if (gd.gl->lockc[larrayval])
                    goto SKIP_TIMER_LOOP;
            }
        }
        
        if (cte->delme)
            clean_timers = 1;
        else {

            if (cte->type == EVENT_MONSTER) {
                int monster_go = 1;
                
                v_onstack("run_timers.inactive_check");
                if (oinst[cte->inst]->level >= 0) {
                    int monlev = oinst[cte->inst]->level;
                    if (dun[monlev].level_flags & DLF_START_INACTIVE) {
                        v_upstack();
                        goto SKIP_TIMER_LOOP;
                    }
                }
                v_upstack();
                
                if (monster_go && (flags & TE_DECMODE)) {
                    cte->time_until--;
                    do_monster_incidentals(cte);
                }
            } else if (flags & TE_DECMODE)
                cte->time_until--;
            
            if ((flags & TE_RUNMODE) && !cte->time_until) {
                cte->time_until = cte->maxtime;
                cte->delme = do_timer_event(cte);
                if (cte->delme)
                    clean_timers = 1;    
            }
        }
        
        SKIP_TIMER_LOOP:
        cte = cte->n;
    }
    gd.queue_monster_deaths--;
    if (!gd.queue_monster_deaths)
        kill_all_walking_dead_monsters();
    
    if (clean_timers)
        cleanup_timers(p_root_te);
        
    VOIDRETURN();
}

void init_t_events(void) {
    
    te = NULL;
    new_te = NULL;
    a_te = NULL;
    
}

void buffer_message_locked(unsigned int inst, int msgtype, int datafield,
    int rc, unsigned int id_sender, int pstate, int xdata)
{
    struct buffered_msg *b;
    
    onstack("buffer_message_locked");
    
    b = dsbmalloc(sizeof(struct buffered_msg));
    b->inst = inst;
    b->msgtype = msgtype;
    b->datafield = datafield;
    b->rc = rc;
    b->id_sender = id_sender;
    b->pstate = pstate;
    b->xdata = xdata;
    b->n = NULL; 
    
    //fprintf(errorlog, "*** DEBUG *** queued msgtype %d\n", msgtype); 
          
    if (MSG_BUFFER_LAST) {
        MSG_BUFFER_LAST->n = b;
        MSG_BUFFER_LAST = b;
    } else {
        MSG_BUFFER = MSG_BUFFER_LAST = b;
    }
        
    VOIDRETURN();
}

void i_deliver_msg(int primary, unsigned int inst, int msgtype, int datafield,
    int rc, unsigned int id_sender, int pstate, int xdata)
{
    struct inst *p_inst;
    struct obj_arch *p_arch;
    const char *s_mh_n = "msg_handler";
    int n_stack = 0;
    int b_stack_function = 0;
    int c_pstate = party_ist_peek(gd.a_party);
    int pushed = 0;

    if (rc > 50) {
        recursion_puke();
        return;
    }
    
    if (oinst[inst] == NULL)
        return;   
        
    if (gd.gl->lockc[LOCK_MESSAGES]) {
        buffer_message_locked(inst, msgtype, datafield, rc, id_sender, pstate, xdata);
        return;
    }
    
    p_inst = oinst[inst];
    p_arch = Arch(p_inst->arch);
    
    gd.in_handler = 1;
    
    if (c_pstate != pstate) {
        ist_cap_top(&istaparty);
        ist_push(&istaparty, pstate);
        pushed = 1;
    }
       
    deliver_chained_msgs(oinst[inst], msgtype,
        datafield, id_sender, rc, pstate, xdata);
    
    // someone destroyed the thing!
    if (UNLIKELY(!oinst[inst])) {
        lstacktop();
        if (pushed) {
            ist_pop(&istaparty);
            ist_check_top(&istaparty);
        }
        return;
    }
    
    luastacksize(8);
    lua_getglobal(LUA, "__main_msg_processor");
    lua_pushinteger(LUA, inst);
    lua_pushstring(LUA, p_arch->luaname);
    lua_pushinteger(LUA, msgtype);    
    lua_pushinteger(LUA, datafield);
    if (id_sender > 0)
        lua_pushinteger(LUA, id_sender);
    else
        lua_pushnil(LUA);
    
    grcnt = (rc+1);
    if (lua_pcall(LUA, 5, 0, 0) != 0) {
        char comname[256];
        snprintf(comname, sizeof(comname), "%s.%s[%d]", 
            p_arch->luaname, s_mh_n, msgtype);
            
        lua_function_error(comname, lua_tostring(LUA, -1));
        return;
    }
    grcnt = 0;
        
    if (pushed) {
        ist_pop(&istaparty);
        v_onstack("i_deliver_msg.check"); // DEBUG
        ist_check_top(&istaparty);
        v_upstack(); // DEBUG
    }

    gd.in_handler = 0;

    lstacktop();
    
    return;
}

void deliver_msg(int inst, int msgtype,
    int datafield, unsigned int id_sender,
    int p_state, int xdata)
{
    int b_spushed = 0;
    
    // a SYSTEM target message
    if (inst < 0) {
        onstack("deliver_msg(SYSTEM)");     
        deliver_szmsg(msgtype, datafield, id_sender);
        VOIDRETURN();
    }

    if (!grcnt) {
        struct inst *p_inst;
        struct obj_arch *p_arch;
        
        // stack +1
        v_onstack("deliver_msg");
        p_inst = oinst[inst];
        p_arch = Arch(p_inst->arch);
        stackbackc('(');
        v_onstack(p_arch->luaname);
        addstackc(')');
        
        ist_cap_top(&istaparty);
        b_spushed = 1;
    }
    
    i_deliver_msg(1, inst, msgtype, datafield, grcnt, id_sender, p_state, xdata);

    if (b_spushed) {
        // stack back to 0
        v_upstack();
        
        // add something briefly
        v_onstack("deliver_msg.check_top");
        ist_check_top(&istaparty);
        v_upstack();
    }
}

void deliver_chained_msgs(struct inst *p_inst, int msgtype, int datafield,
    unsigned int sender, int rc, int p_state, int xdata)
{
    struct inst_loc *ct;
    struct inst_loc *pct;
    
    onstack("deliver_chained_msgs");
    
    ct = p_inst->chaintarg;
    pct = NULL;
    
    while (ct != NULL) {
        int delme = 0;
        
        // clean out the list when traversing it, rather than creating
        // ALL KINDS OF HORRIBLE PROBLEMS if someone tries to delink
        // in the midst of a chained msg (like if you try to chain
        // a destroy message, for example)
        if (!ct->delink && oinst[ct->i]) { 
            i_deliver_msg(0, ct->i, msgtype, datafield, (rc+1),
                sender, p_state, xdata);
            if (ct->delink) delme = 1;
        } else
            delme = 1;
            
        if (!delme && (ct->chain_reps > 0)) {
            ct->chain_reps--;
            if (ct->chain_reps == 0) {
                oinst[ct->i]->uplink = 0;
                ct->delink = 1;
                delme = 1;
            }
        }
            
        if (delme) {
            
            if (pct == NULL) {
                struct inst_loc *x = ct;
                p_inst->chaintarg = ct = ct->n;
                dsbfree(x);
            } else {
                pct->n = ct->n;
                dsbfree(ct);
                ct = pct->n;
            }
            
        } else {
            pct = ct;
            ct = ct->n;
        }
    }
    
    VOIDRETURN();
}

// messages sent during a locked game are now buffered instead of discarded
void deliver_message_buffer(void) {
    onstack ("deliver_message_buffer");
    
    if (gd.gl->lockc[LOCK_MESSAGES]) {
        VOIDRETURN();
    }

    while (MSG_BUFFER) {
        struct buffered_msg *b = MSG_BUFFER;
        
        //fprintf(errorlog, "*** DEBUG *** delivering queued message %d\n", b->msgtype);
        i_deliver_msg(1, b->inst, b->msgtype, b->datafield,
            b->rc, b->id_sender, b->pstate, b->xdata);
            
        if (MSG_BUFFER == MSG_BUFFER_LAST) {
            MSG_BUFFER = MSG_BUFFER_LAST = NULL;
        } else {
            MSG_BUFFER = MSG_BUFFER->n;
        }
        dsbfree(b);
        
        // quit out if someone locked it as a result of a msg handler
        if (gd.gl->lockc[LOCK_MESSAGES]) {
            VOIDRETURN();
        }
    }
    
    VOIDRETURN();
}

void set_animtimers_unused(int *cond, int num) {
    int i;
    
    onstack("set_animtimers_unused");
    
    for(i=0;i<num;++i) {
        cond[i] = FRAMECOUNTER_NORUN;
    }
    
    VOIDRETURN();
}
