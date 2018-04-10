#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "objects.h"
#include "global_data.h"
#include "timer_events.h"
#include "champions.h"
#include "lua_objects.h"
#include "gproto.h"
#include "uproto.h"
#include "arch.h"
#include "gamelock.h"
#include "istack.h"
#include "mparty.h"
#include "monster.h"
#include "render.h"
#include "viewscale.h"

extern const char *Sys_Inventory;
extern int Gmparty_flags;

struct inst *oinst[NUM_INST];
struct dungeon_level *dun;

extern struct global_data gd;
extern struct inventory_info gii;

extern lua_State *LUA;
extern FILE *errorlog;
extern istack istaparty;
extern int debug;

struct obj_aff *oaq_end = NULL;

int calc_total_light(struct dungeon_level *dd) {
    int i_lightlevel = dd->lightlevel;
    int n;
    
    for (n=0;n<MAX_LIGHTS;++n)
        i_lightlevel += gd.lightlevel[n];
    
    return i_lightlevel;
}

void i_instmd_queue(struct obj_aff **ov_tbl,
    int op, int lev, int xx, int yy, int dir,
    int inst, int data)
{
    struct obj_aff *oa_n;
    
    oa_n = dsbmalloc(sizeof(struct obj_aff));
    oa_n->op = op;
    oa_n->lev = lev;
    oa_n->x = xx;
    oa_n->y = yy;
    oa_n->dir = dir;
    oa_n->inst = inst;
    oa_n->data = data;
    oa_n->n = NULL;
        
    // store state information for handling the queue
    if (op == INSTMD_INST_MOVE || op == INSTMD_INST_DELETE) {
        v_onstack("i_instmd_queue.character");
        if (oinst[inst] && oinst[inst]->level == LOC_CHARACTER) {
            int xl = oinst[inst]->x;
            int yl = oinst[inst]->y;
            gd.champs[xl-1].inv_queue_data[yl] &= ~(IQD_FILLED);
            gd.champs[xl-1].inv_queue_data[yl] |= IQD_VACATED;
        }
        if (lev == LOC_CHARACTER) {
            gd.champs[xx-1].inv_queue_data[yy] &= ~(IQD_VACATED);
            gd.champs[xx-1].inv_queue_data[yy] |= IQD_FILLED;
        } 
        v_upstack();
    }
    
    if (*ov_tbl) {
        oaq_end->n = oa_n;
        oaq_end = oa_n;
    } else
        *ov_tbl = oaq_end = oa_n;
}

void instmd_queue(int op, int lev, int xx, int yy, int dir,
    int inst, int data)
{
    onstack("instmd_queue");
    i_instmd_queue(&(gd.queued_obj_aff), op, lev,
        xx, yy, dir, inst, data);
    VOIDRETURN();
}

void inv_instmd_queue(int op, int lev, int xx, int yy, int dir,
    int inst, int data)
{
    onstack("inv_instmd_queue");
    i_instmd_queue(&(gd.queued_inv_obj_aff), op, lev,
        xx, yy, dir, inst, data);
    VOIDRETURN();
}


void instmd_queue_flush_loop(struct obj_aff **p_oaq, int Queue_Max) {
    int instant_launch = 0;
    int queueruns = 0;
    struct obj_aff *oaq = *p_oaq;
    
    onstack("instmd_queue_flush_loop");
    
    // show an empty queue to any further functions
    // so they start their own queues instead of messing mine
    oaq_end = NULL;
    *p_oaq = NULL;
    
    while(oaq != NULL) {
        struct obj_aff *oaq_it_n = oaq->n;
        
        ++queueruns;
        if (UNLIKELY(queueruns > Queue_Max)) {
            program_puke("Excessive instmd queue");
            VOIDRETURN();
        }
        
        switch(oaq->op) {
            case INSTMD_INST_MOVE:
                if (oinst[oaq->inst]) {
                    struct inst *p_inst = oinst[oaq->inst];
                    
                    if (oaq->lev != p_inst->level ||
                        oaq->x != p_inst->x ||
                        oaq->y != p_inst->y)
                    {
                        if (!in_limbo(oaq->inst))
                            limbo_instance(oaq->inst);

                        if (oaq->lev != LOC_LIMBO) {
                            place_instance(oaq->inst, oaq->lev, oaq->x, oaq->y,
                                oaq->dir, oaq->data);
                        }
                    }
                }
            break;
            
            case INSTMD_RELOCATE:
                if (oinst[oaq->inst]) {
                    if (inst_relocate(oaq->inst, oaq->dir)) {
                        char err[256];
                        snprintf(err, 255, "Bad reposition on inst %d (%d)\n\n%s\n%s",
                            oaq->inst, oaq->dir,
                            "A size 2 monster is likely being repositioned",
                            "where it can't fit.");
                        poop_out(err);
                    }
                }
            break;

            case INSTMD_INST_DELETE:
                if (oinst[oaq->inst]) {
                    inst_destroy(oaq->inst, oaq->data);
                }
            break;
            
            case INSTMD_INST_FLYER:
                if (oinst[oaq->inst]) {
                    struct inst *p_inst = oinst[oaq->inst];
                    if (p_inst->level != LOC_LIMBO)
                        limbo_instance(oaq->inst);
                    exec_flyinst(oaq->inst, p_inst, oaq->dir,
                        oaq->fdist, oaq->dpower, oaq->data, oaq->lev, instant_launch);
                }
            break;
            
            case INSTMD_KILL_MONSTER:
                if (oinst[oaq->inst]) {
                    kill_monster(oaq->inst);
                }
            break;
            
            case INSTMD_PARTY_PLACE:
                exec_party_place(oaq->inst, oaq->lev, oaq->x, oaq->y, oaq->dir);
            break;
            
            case INSTMD_TROTATE:
                tile_actuator_rotate(oaq->lev, oaq->x, oaq->y, oaq->dir, oaq->inst);
            break;
            
            case INSTMD_PTREXCH:
                exchange_pointers(oaq->lev, oaq->x, oaq->y,
                    oaq->lev2, oaq->x2, oaq->y2);
            break;
        }
        
        dsbfree(oaq);
        oaq = oaq_it_n;
    }
    
    VOIDRETURN();
}

void queued_inst_destroy(unsigned int inst) {
    struct inst *p_inst;

    onstack("queued_inst_destroy");
    
    p_inst = oinst[inst];
    if (p_inst == NULL)
        VOIDRETURN();
        
    if (gd.queue_rc && (p_inst->level >= 0 || inst == gd.always_queue_inst))
        instmd_queue(INSTMD_INST_DELETE, 0, 0, 0, 0, inst, 0);
    else if (gd.queue_inv_rc && p_inst->level == LOC_CHARACTER)
        inv_instmd_queue(INSTMD_INST_DELETE, 0, 0, 0, 0, inst, 0);    
    else
        inst_destroy(inst, 0);
        
    VOIDRETURN();
}

void i_flush_instmd_queue(int qmax, int *q_rc, struct obj_aff **q_obj_a) {
    int rep = 0;
    
    onstack("i_flush_instmd_queue");

    while (*q_obj_a) {
        *q_rc += 1;
        instmd_queue_flush_loop(q_obj_a, qmax);
        *q_rc -= 1;
        ++rep;

        if (rep >= 80) {
            recover_error("Excessive trigger queue");
            *q_rc = 0;
            VOIDRETURN();
        }
    }
    
    VOIDRETURN();
}

void flush_instmd_queue_with_max(int qmax) {
    onstack("flush_instmd_queue_with_max");
    i_flush_instmd_queue(qmax, &(gd.queue_rc), &(gd.queued_obj_aff));
    VOIDRETURN();
}

void flush_instmd_queue(void) {
    onstack("flush_instmd_queue");
    i_flush_instmd_queue(1000, &(gd.queue_rc), &(gd.queued_obj_aff));
    VOIDRETURN();
}

void flush_inv_instmd_queue(void) {
    onstack("flush_inv_instmd_queue");
    i_flush_instmd_queue(1000, &(gd.queue_inv_rc), &(gd.queued_inv_obj_aff));
    VOIDRETURN();
}

void coord_inst_r_error(const char *s_error, unsigned int inst, int pos) {
    char s_emsg[256];
    struct inst *p_t = oinst[inst];
    struct obj_arch *p_arch;
    
    if (pos == 255)
        pos = p_t->tile;
        
    p_arch = Arch(p_t->arch);

    snprintf(s_emsg, sizeof(s_emsg),
        "%s %d [%s] @ (%d,%d,%d) %d", s_error,
        inst, p_arch->luaname,
        p_t->level, p_t->x, p_t->y, pos);

    recover_error(s_emsg);
}

/*
int shift_movedir(int from, int to) {

    if (to == (from+1)%4) return to;
    else if (from == (to+1)%4) return (from+2)%4;
    
    return -1;
}
*/

int inst_relocate(unsigned int inst, int destp) {
    struct inst *p_inst;
    struct obj_arch *p_arch;
    
    onstack("inst_relocate");
    
    p_inst = oinst[inst];
    p_arch = Arch(p_inst->arch);
    
    if (p_inst->level < 0) {
        p_inst->tile = destp;
        RETURN(0);
    }
    
    if (p_arch->type == OBJTYPE_MONSTER) {
        struct inst_loc *push = NULL;
        int mdir;
        int srcp;
        struct dungeon_level *dd;
        int itx, ity;
                
        itx = p_inst->x;
        ity = p_inst->y;
        dd = &(dun[p_inst->level]);
        
        if (p_arch->msize == 4) {
            RETURN(0);
        } else if (p_arch->msize == 2 && destp != DIR_CENTER) {
            int i_sdir = check_slave_subtile(p_inst->facedir, destp);

            if (i_sdir < 0) {
                RETURN(1);
            }
        }
        
        // centered objects can only move into a path, which is not
        // a problem. we're only worried about moving OUT of the path of
        // something we'd collide with (because the check happens just
        // before the object is about to move)
        if (p_inst->tile != DIR_CENTER) {
            if (destp == DIR_CENTER) {
                int d0 = p_inst->tile;
                int d1 = (p_inst->tile + 1) % 4;
                int d2 = (p_inst->tile + 2) % 4;
                int d3 = (p_inst->tile + 3) % 4;
                
                // checked to make sense as of DSB 0.68
                push = fast_il_pushcol(push, dd->t[ity][itx].il[d0],
                    itx, ity, d1, d1, inst);
                push = fast_il_pushcol(push, dd->t[ity][itx].il[d0],
                    itx, ity, d3, d2, inst);
                    
            } else {
                int travel_direction = shiftdirto(p_inst->tile, destp);
                push = fast_il_pushcol(push, dd->t[ity][itx].il[p_inst->tile],
                    itx, ity, destp, travel_direction, inst);
            } 
        }   
    }
 
    depointerize(inst);
    p_inst->tile = destp;
    pointerize(inst);

    RETURN(0);
}

int obj_collision(int lev, int x, int y, int tilepos,
    unsigned int no_count, int *col_info)
{
    struct dungeon_level *dd;
    struct inst_loc *tp;
    int monster = 0;
    int rc = 0;
    int ccnt = 0;
    
    onstack("obj_collision");
    
    if (lev < 0) {
        coord_inst_r_error("Invalid collision", no_count, 255);
        RETURN(0);
    }
    
    if (no_count > 0) {
        struct obj_arch *p_arch;
        
        if (oinst[no_count]->openshot)
            RETURN(0);

        p_arch = Arch(oinst[no_count]->arch);
        if (p_arch->type == OBJTYPE_MONSTER)
            monster = 1;
    }

    dd = &(dun[lev]);
    tp = dd->t[y][x].il[tilepos];
    
    if (col_info) {
        ccnt = (*col_info) & 0xFF;
        *col_info = (*col_info) & 0xFFFFFF00;
    }

    while (tp != NULL) {
        struct inst *p_inst = oinst[tp->i];
        struct obj_arch *p_arch;

        p_arch = Arch(p_inst->arch);
        
        if (tp->i != no_count) {
            if (!(p_inst->gfxflags & OF_INACTIVE)) {
                int collided = 0;
                
                if (call_member_func(tp->i, "col", (no_count)?:-1)) {

                    if (!rc || p_arch->type == OBJTYPE_MONSTER)
                        rc = tp->i;
                        
                    collided = 1;                 
                    if (col_info && !tp->slave) ccnt++;
                }
                
                if (monster && 
                    call_member_func(tp->i, "no_monsters", no_count))
                {
                    rc = tp->i;
                    if (col_info)
                        *col_info |= COL_NO_MONS;
                }
                    
                if (col_info && no_count && collided) {                    
                    if (oinst[no_count]->arch != p_inst->arch)
                        *col_info |= COL_DIFF_TYPES;
                    
                    if (p_arch->type == OBJTYPE_DOOR) {
                        *col_info |= COL_HAS_DOOR;
                    } else
                        *col_info |= COL_HAS_OBJ;
                }                    
            }
        }
        
        tp = tp->n;   
    }
 
    if (col_info)
        *col_info |= (ccnt & 0xFF);
        
    RETURN(rc);
}

struct inst_loc *fast_il_genlist(struct inst_loc *PSH,
    struct inst_loc *il, int ddir, int ch)
{
    while(il != NULL) {
        unsigned int i = il->i;
        struct inst *p_inst = oinst[i];
        if (p_inst->flytimer) {
            struct inst_loc *t;
            int hit;

            if (p_inst->openshot & 1)
                hit = 0;
            else if (p_inst->facedir != (ddir+2)%4)
                hit = 0;
            else if (ch == -1)
                hit = 1;
            else
                hit = call_member_func(ch, "col", i);

            if (hit) {
                t = dsbfastalloc(sizeof(struct inst_loc));
                t->i = i;
                t->n = PSH;
                PSH = t;
            }
        }

        il = il->n;
    }

    return PSH;
}

struct inst_loc *fast_il_pushcol(struct inst_loc *PSH,
    struct inst_loc *il, int tx, int ty, int tl, int ddir, int ch)
{
    int n = 0;
    unsigned int collision_entity[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    
    while(il != NULL) {
        unsigned int i = il->i;
        struct inst *p_inst = oinst[i];
        if (p_inst->flytimer) {
            struct inst_loc *t;
            int hit;
            
            // as of DSB 0.68 openshot projectiles now get pushcollided!
            // this should not break anything because the party is blocked
            // from walking into its own projectiles, and monsters are
            // delayed after launching a projectile to also avoid it. this
            // means a pushcollide with an openshot projectile should not
            // even really happen...
            
            if (p_inst->facedir != (ddir+2)%4)
                hit = 0;
            else if (ch == -1)
                hit = 1;
            else
                hit = call_member_func(ch, "col", i);

            if (hit) {
                t = dsbfastalloc(sizeof(struct inst_loc));
                t->i = i;
                t->n = PSH;
                PSH = t;
                
                collision_entity[n] = i;
                n++;
                if (n == 8) {
                    break;
                }
            }
        }

        il = il->n;
    }
    
    // formerly we modified the same list that we were iterating over
    // ... this didn't actually break anything but probably was bad.
    for(n=0;n<8;n++) {
        if (collision_entity[n]) {
            unsigned int i = collision_entity[n];
            struct inst *p_inst = oinst[i];
            
            depointerize(i);
            p_inst->x = tx;
            p_inst->y = ty;
            p_inst->tile = tl;
            pointerize(i);
        } else {
            break;
        }
    }

    return PSH;
}

void remove_openshot_on_pushdest(struct dungeon_level *dd, int tx, int ty, int tl) {
    struct inst_loc *il;
    
    onstack("remove_openshot_on_pushdest");

    il = dd->t[ty][tx].il[tl];
    
    while(il != NULL) {
        unsigned int i = il->i;
        struct inst *p_inst = oinst[i];
        
        if (p_inst->openshot) {
            p_inst->openshot = 0;
        }
        
        il = il->n;
    }    

    VOIDRETURN();
}

void member_func_inside(unsigned int islot, const char *func,
    int param, int n, int recurs)
{
    int ic;
    struct inst *p_i_inst;
    
    if (recurs > 60) {
        recursion_puke();
        return;
    }
           
    p_i_inst = oinst[islot];     
    for (ic=0;ic<p_i_inst->inside_n;++ic) {
        unsigned int ii = p_i_inst->inside[ic];
        if (ii) {
            struct inst *p_inst;
            call_member_func2(ii, func, param, n); 
            p_inst = oinst[ii];
            if (p_inst && p_inst->inside_n)
                member_func_inside(ii, func, param, n, (recurs+1));
        }
    }
}

unsigned int scanfor_all_inside(unsigned int islot, int n,
    int arch, int recurs)
{
    int ic;
    struct inst *p_i_inst;

    if (recurs > 30) {
        recursion_puke();
        return 0;
    }

    p_i_inst = oinst[islot];
    for (ic=0;ic<p_i_inst->inside_n;++ic) {
        unsigned int ii = p_i_inst->inside[ic];
        if (ii) {
            struct inst *p_inst;
            p_inst = oinst[ii];
            if (p_inst->arch == arch)
                return ii;
                
            if (p_inst && p_inst->inside_n)
                scanfor_all_inside(ii, n, arch, (recurs+1));
        }
    }
    
    return 0;
}

unsigned int scanfor_all_owned(int scan_ppos, int arch) {
    int n;
    
    onstack("scanfor_all_owned");

    if (gd.mouseobj) {
        struct inst *p_inst = oinst[gd.mouseobj];
        if (arch == p_inst->arch)
            RETURN(gd.mouseobj);
    }

    for(n=0;n<4;n++) {
        if (scan_ppos != -1 && scan_ppos != n)
            continue;
        
        if (gd.party[n] && IsNotDead(n)) {
            int ic;

            for (ic=0;ic<gii.max_invslots;++ic) {
                unsigned int islot = gd.champs[gd.party[n]-1].inv[ic];

                if (islot) {
                    struct inst *p_inst;
                    p_inst = oinst[islot];

                    if (p_inst->arch == arch)
                        RETURN(islot);
                        
                    if (p_inst && p_inst->inside_n) {
                        unsigned int r;
                        
                        v_onstack("scanfor_all_inside");
                        r = scanfor_all_inside(islot, n, arch, 0);
                        v_upstack();
                        
                        if (r)
                            RETURN(r);
                    }
                }
            }
        }
    }
    
    RETURN(0);
}

void member_func_all_owned(const char *func, int param) {
    int n;
    
    onstack("member_func_all_owned");
    
    for(n=0;n<4;n++) {
        if (gd.party[n] && IsNotDead(n)) {
            int ic;
            
            for (ic=0;ic<gii.max_invslots;++ic) {
                unsigned int islot = gd.champs[gd.party[n]-1].inv[ic];
                
                if (islot) {
                    struct inst *p_inst;
                    
                    call_member_func2(islot, func, param, n);                    
                    p_inst = oinst[islot];                    
                    if (p_inst && p_inst->inside_n) {
                        v_onstack("member_func_inside");
                        member_func_inside(islot, func, param, n, 0);
                        v_upstack();
                    }
                        
                }    
            }                 
        }
    }
    
    if (gd.mouseobj)
        call_member_func2(gd.mouseobj, func, param, gd.leader);

    VOIDRETURN();
}  

void change_facing(int ap, int olddir, int newdir, int s) {
    int actual_dir = newdir;
    
    if (s) {
        p_to_tile(ap, T_OFF_TURN, olddir);
        p_to_tile(ap, T_ON_TURN, newdir);
    }
    
    // use the party's actual direction in case it got tweaked
    member_func_all_owned("on_turn", gd.p_face[ap]);
    
    // this must be called AFTER the object on_turns
    lc_parm_int("sys_party_turn", 1, gd.p_face[ap]);
}

int i_to_tile(unsigned int inst, int onoff) {
    int rv;
    struct inst *p_inst;
    
    onstack("i_to_tile");
    
    p_inst = oinst[inst];
    
    // don't drop instances that aren't in a level
    if (onoff && p_inst->level < 0)
        RETURN(0);
    
    rv = to_tile(p_inst->level, p_inst->x, p_inst->y, p_inst->tile, inst, onoff);
    RETURN(rv);    
}

int p_to_tile(int ap, int onoff, int v) {
    int rv;
    int faceval;
    
    if (onoff == T_ON_TURN || onoff == T_OFF_TURN)
        faceval = v;
    else
        faceval = gd.p_face[ap];
    
    onstack("p_to_tile");
    ist_push(&istaparty, ap);
    rv = to_tile(gd.p_lev[ap], gd.p_x[ap], gd.p_y[ap], faceval, v, onoff);
    ist_pop(&istaparty);
    RETURN(rv);
}

struct inst_loc *scan_d_trigger(struct inst_loc *il_event_list,
    struct inst_loc *tp, int inst)
{
    struct inst_loc *il_el_end = NULL;
    
    onstack("scan_d_trigger");
    
    if (il_event_list != NULL) {
        struct inst_loc *il_sl = il_event_list;
        while (il_sl != NULL) {
            il_el_end = il_sl;
            il_sl = il_sl->n;
        }
    }
    
    while (tp != NULL) {
        if (tp->i != inst && oinst[tp->i]) {
            if (!(oinst[tp->i]->gfxflags & OF_INACTIVE)) {
                struct inst_loc *il_new_el;

                il_new_el = dsbcalloc(1, sizeof(struct inst_loc));
                il_new_el->i = tp->i;
                il_new_el->n = NULL;

                if (il_event_list == NULL) {
                    il_event_list = il_el_end = il_new_el;
                } else {
                    il_el_end->n = il_new_el;
                    il_el_end = il_new_el;
                }
            }
        }
        tp = tp->n;
    }
    
    RETURN (il_event_list);
}

int to_tile(int lev, int xx, int yy, int dir, int inst, int onoff) {
    static int queue_rc = 0;
    int extparm;
    struct dungeon_level *dd;
    struct inst_loc *il_event_list = NULL;
    const char *cmdstr;
    int rv = 0;
    int no_party = 0;
    int d_handle = 0;
    int priority;
    int destroy_only = 0;
    
    onstack("to_tile");
    
    if (gd.gl->lockc[LOCK_ACTUATORS]) {
        RETURN(0);
    }
    
    if (!gd.party[0] && !gd.party[1] && !gd.party[2] && !gd.party[3])
        no_party = 1;
    
    // "inst" isn't really an instance in these cases
    extparm = inst;
    if (onoff >= T_TRY_MOVE) inst = -1;
    
    if (onoff == T_FROM_TILE) cmdstr = "off_trigger";
    else if (onoff == T_TO_TILE) cmdstr = "on_trigger";
    else if (onoff == T_TRY_MOVE) cmdstr = "on_try_move";
    else if (onoff == T_ON_TURN) cmdstr = "on_turn";
    else if (onoff == T_OFF_TURN) cmdstr = "off_turn";
    else if (onoff == T_LOCATION_PICKUP) cmdstr = "on_location_pickup";
    else if (onoff == T_LOCATION_DROP) cmdstr = "on_location_drop";
    
    stackbackc('(');
    v_onstack(cmdstr);
    addstackc(')');
    
    dd = &(dun[lev]);
    
    ++gd.queue_rc;
    
    // now calloc'd instead of fastalloc'd... so they need to be freed
    il_event_list = scan_d_trigger(NULL, dd->t[yy][xx].il[DIR_CENTER], inst);
    if (dir != DIR_CENTER) {
        il_event_list = scan_d_trigger(il_event_list,
            dd->t[yy][xx].il[dir], inst);
    }
     
     // this used to actually do something.
     // I'll leave it here in case I need to do something again
    for(priority=0;priority>=0;--priority) {
        struct inst_loc *il_el = il_event_list;
        int priority_actions = 0;
         
        while (il_el != NULL) {
            int forget_it = 0;
            struct inst_loc *il_el_n = il_el->n;
            unsigned int i_cinst = il_el->i;
                  
            // someone destroyed the inst out from under me?
            if (!destroy_only && 
                (inst == -1 || (inst > 0 && oinst[inst]))) 
            {
                struct obj_arch *p_arch = Arch(oinst[i_cinst]->arch);
                //int cpri = !!(p_arch->arch_flags & ARFLAG_PRIORITY);
                
                if (priority == 0 /* always true */) {
                    if (inst == -1 && no_party) {
                        int tr = queryluabool(p_arch, "no_party_triggerable");
                        if (!tr)
                            forget_it = 1;
                    }
       
                    // someone destroyed the inst out from under me here?
                    if (inst > 0 && !oinst[inst])
                        forget_it = 1;
                    
                    if (!forget_it) {   
                        rv = call_member_func(i_cinst, cmdstr, extparm);
                        priority_actions += priority;
                    }
                }
            }    
                        
            if (priority == 0) {
                dsbfree(il_el);
            }
            il_el = il_el_n;
        }
        
        /*
        if (priority_actions) {    
            if (gd.watch_inst_out_of_position) {
                destroy_only = 1;  
                gd.watch_inst_out_of_position = 0;
            }
        }
        */
    }
    
    gd.watch_inst = 0;
    --gd.queue_rc;
    if (!gd.queue_rc)
        flush_instmd_queue();
    
    RETURN(rv);
}

void enable_inst(unsigned int inst, int onoff) {
    struct dungeon_level *dd;
    struct inst_loc *tp;
    struct obj_arch *p_arch;
    char *cmdstr;
    struct inst *p_inst;
    int xx, yy;
    int d;
    int i;
    
    onstack("enable_inst");
    
    p_inst = oinst[inst];
    
    if (onoff == 0) {
        cmdstr = "off_trigger";
        p_inst->gfxflags |= OF_INACTIVE;
    } else if (onoff > 0) {
        cmdstr = "on_trigger";
        p_inst->gfxflags &= ~OF_INACTIVE;
    } else {
        char d[64];
        snprintf(d, sizeof(d), "p_inst.onoff = %d ?", onoff);
        recover_error(d);
        VOIDRETURN();
    }
    
    // spawnbursted objects don't require all that stuff
    // it'll happen later on
    if (gd.engine_flags & ENFLAG_SPAWNBURST) {
        VOIDRETURN();
    }
    
    stackbackc('(');
    v_onstack(cmdstr);
    addstackc(')');
    
    if (p_inst->level < 0)
        VOIDRETURN();
        
    dd = &(dun[p_inst->level]);
    yy = p_inst->y;
    xx = p_inst->x;
    
    p_arch = Arch(p_inst->arch);
    addstackc('-');
    stackbackc('>');
    v_onstack(p_arch->luaname);

    // party activation
    for (i=0;i<4;++i) {
        if (gd.p_lev[i] < 0) continue;

        if (gd.p_lev[i] == p_inst->level) {
            if (xx == gd.p_x[i] && yy == gd.p_y[i]) {
                ist_push(&istaparty, i);
                call_member_func(inst, cmdstr, -1);
                ist_pop(&istaparty);
                break;
            }
        }
    }

    gd.queue_rc++;

    for (d=0;d<MAX_DIR;d++) {
        tp = dd->t[yy][xx].il[d];
        while (tp != NULL) {
            if (tp->i && tp->i != inst) {
                if (oinst[tp->i]) {
                    struct inst *p_t_inst = oinst[tp->i];
                    int rv;
                    if (!(p_t_inst->gfxflags & OF_INACTIVE)) {
                        if (p_inst->tile == DIR_CENTER || p_inst->tile == p_t_inst->tile)
                            rv = call_member_func(inst, cmdstr, tp->i);
                    }
                }
            }
            tp = tp->n;
        }
    }
    
    gd.queue_rc--;
    if (!gd.queue_rc)
        flush_instmd_queue();
    
    VOIDRETURN();
}

int party_objblock(int ap, int lev, int x, int y) {
    int cc;
    
    onstack("party_objblock");
    
    if (GMP_ENABLED() && (Gmparty_flags & MPF_COLLIDE)) {
        int i = 0;
        for (i=0;i<4;++i) {
            if (ap == i)
                continue;
                
            if (gd.p_lev[i] == lev) {
                if (gd.p_x[i] == x && gd.p_y[i] == y) {
                    RETURN(-1);
                }
            }
        }
    }
    
    for (cc=0;cc<5;++cc) {
        int hit = 0;
        hit = obj_collision(lev, x, y, cc, 0, NULL);
        if (hit) {
            if (oinst[hit]) {
                struct obj_arch *p_arch;

                p_arch = Arch(oinst[hit]->arch);
                
                if (p_arch->arch_flags & ARFLAG_NO_BUMP) {
                    RETURN(-1*hit);
                } else {
                    RETURN(hit);
                }
                
            } else {
                RETURN(hit);
            }
        }
    }
    
    RETURN(0);
}

void exec_flyinst(unsigned int inst, struct inst *p_inst,
    int travdir, int flydist, int damagepower, short delta, short ddelta, int instant)
{
    onstack("exec_flyinst");
    
    struct obj_arch *p_arch;
    unsigned int flyreps;
    int v=1;
    
    p_arch = Arch(p_inst->arch);
    flyreps = p_arch->base_flyreps;
    if (flyreps < 1)
        flyreps = 1;

    p_inst->facedir = travdir;
    p_inst->flytimer = p_inst->max_flytimer = flydist;
    p_inst->damagepower = damagepower;
    p_inst->x_tweak = p_inst->y_tweak = 0;
    p_inst->flycontroller = NULL;
    p_inst->gfxflags |= G_LAUNCHED;

    if (gd.tickclock && !instant)
        v++;

    // v is no longer used in the timer info
    add_timer(inst, EVENT_MOVE_FLYER, 1, 1, delta, ddelta, flyreps);
    
    set_fdelta_to_begin_now(p_inst);
    
    VOIDRETURN();
}

void launch_object(int inst, int lev, int x, int y, 
    int travdir, int tilepos, unsigned short flydist, 
    unsigned short damagepower, short delta, short ddelta)
{
    int instant_launch = 0;
    struct inst *p_inst;
    
    onstack("launch_object");
    
    p_inst = oinst[inst];
    
    if (gd.queue_rc) {
        instmd_queue(INSTMD_INST_FLYER, ddelta, flydist, damagepower,
            travdir, inst, delta);
        instmd_queue(INSTMD_INST_MOVE, lev, x, y, tilepos, inst, 0);
    } else {
        if (p_inst->level != LOC_LIMBO)
            limbo_instance(inst);
            
        exec_flyinst(inst, p_inst, travdir, flydist, damagepower,
            delta, ddelta, instant_launch);
        place_instance(inst, lev, x, y, tilepos, 0);
    }
    
    VOIDRETURN();
}

void inv_pickup_all(int li_who) {
    const char *invfunc = Sys_Inventory;
    int who = li_who - 1;
    int ic;
    
    onstack("inv_pickup_all");
  
    gd.queue_inv_rc++;  

    for (ic=0;ic<gii.max_invslots;++ic) {
        unsigned int islot = gd.champs[who].inv[ic];

        if (islot) {
            struct inst *p_inst = oinst[islot];
            int xx = p_inst->x;
            int yy = p_inst->y;
            
            gd.lua_bool_hack = 1;
            lc_parm_int(invfunc, 6, xx, yy, islot, -1, LHFALSE, LHTRUE);
            
            // re-assert after the move
            p_inst = oinst[islot];
            
            gd.lua_bool_hack = 1;
            lc_parm_int(invfunc, 6, xx, yy, islot, -1, LHTRUE, LHTRUE);
        }
    }    
    gd.champs[who].load = sum_load(who);
    calculate_maxload(li_who);
    determine_load_color(who);
    
    gd.queue_inv_rc--;
    if (!gd.queue_inv_rc)
        flush_inv_instmd_queue();
        
    VOIDRETURN();
}

void inv_putdown_all(int li_who) {
    const char *invfunc = Sys_Inventory;
    int who = li_who - 1;
    int ic;
    
    onstack("inv_putdown_all");
    
    gd.queue_inv_rc++;
    for (ic=0;ic<gii.max_invslots;++ic) {
        unsigned int islot = gd.champs[who].inv[ic];

        if (islot) {
            struct inst *p_inst = oinst[islot];
            int xx = p_inst->x;
            int yy = p_inst->y;
            
            gd.lua_bool_hack = 1;
            lc_parm_int(invfunc, 6, xx, yy, -1, islot, LHFALSE, LHTRUE);
            
            p_inst = oinst[islot];

            gd.lua_bool_hack = 1;
            lc_parm_int(invfunc, 6, xx, yy, -1, islot, LHTRUE, LHTRUE);
        }
    }  
    gd.champs[li_who-1].load = sum_load(who);
    calculate_maxload(li_who);
    determine_load_color(who);
    
    gd.queue_inv_rc--;
    if (!gd.queue_inv_rc)
        flush_inv_instmd_queue();
        
    VOIDRETURN();
}


void change_gfxflag(int inst, int gflag, int operation) {
    struct inst *p_inst;
    struct obj_arch *p_arch;
    
    onstack("change_gfxflag");
    
    p_inst = oinst[inst];
    if (!p_inst) {
        VOIDRETURN();
    }
    
    p_arch = Arch(p_inst->arch);
        
    // reset the monster's animation
    if (gflag & G_ATTACKING) {
        set_fdelta_to_begin_now(p_inst);
    }
    
    if (operation)
        p_inst->gfxflags |= gflag;
    else
        p_inst->gfxflags &= ~gflag;
 
    VOIDRETURN();   
}

void exchange_pointers(int srcl, int sx, int sy, int desl, int dx, int dy) {
    int i;
    struct dungeon_level *s_d;
    struct dungeon_level *d_d;
    
    onstack("exchange_pointers");
    
    s_d = &(dun[srcl]);
    d_d = &(dun[desl]);
    
    for (i=0;i<MAX_DIR;++i) {
        struct inst_loc *swap_il;
        struct inst_loc *travl;
        swap_il = s_d->t[sy][sx].il[i];
        s_d->t[sy][sx].il[i] = d_d->t[dy][dx].il[i];
        d_d->t[dy][dx].il[i] = swap_il;
        
        travl = s_d->t[sy][sx].il[i];
        while (travl) {
            struct inst *p_inst = oinst[travl->i];
            p_inst->x = sx;
            p_inst->y = sy;
            p_inst->level = srcl;
            travl = travl->n;
        }
        
        travl = d_d->t[dy][dx].il[i];
        while (travl) {
            struct inst *p_inst = oinst[travl->i];
            p_inst->x = dx;
            p_inst->y = dy;
            p_inst->level = desl;
            travl = travl->n;
        }
    }
    
    VOIDRETURN();
}

// this code used to be a whole lot more of a mess
void set_fdelta_to_begin_now(struct inst *p_obj_inst) {
    p_obj_inst->frame = 0;
}

void adv_inst_and_cond_framecounters(void) {
    int z;
    int w;
    
    onstack("adv_inst_and_cond_framecounters");
    
    for (z=0;z<NUM_INST;++z) {
        if (oinst[z]) {
            struct inst *p_inst = oinst[z];
            if (!(p_inst->gfxflags & G_FREEZE)) {
                p_inst->frame++;
                if (p_inst->frame >= FRAMECOUNTER_MAX)
                    p_inst->frame = 0;
            }
       
            if (p_inst->crmotion) {
                int cropmax = Arch(p_inst->arch)->cropmax;
            
                p_inst->crop += p_inst->crmotion;
                
                if (p_inst->crop < 0) {
                    p_inst->crop = 0;
                    p_inst->crmotion = 0;
                } else if (p_inst->crop > cropmax) {
                    p_inst->crop = cropmax;
                    p_inst->crmotion = 0;
                }    
            }            
        }
    } 
    
    for (z=0;z<gd.num_pconds;++z) {
        if (gd.partycond_animtimer[z] != FRAMECOUNTER_NORUN) {
            gd.partycond_animtimer[z]++;
            if (gd.partycond_animtimer[z] >= FRAMECOUNTER_MAX) {
                gd.partycond_animtimer[z] = 0;
            }
        }
    }
    
    for (w=0;w<gd.num_champs;++w) {
        struct champion *me = &(gd.champs[w]);
        // only iterate over alloc'd champions
        if (me->port_name) {
            for (z=0;z<gd.num_conds;++z) {
                if (me->condition[z] > 0) {
                    if (me->cond_animtimer[z] != FRAMECOUNTER_NORUN) {
                        me->cond_animtimer[z]++;
                        if (me->cond_animtimer[z] >= FRAMECOUNTER_MAX) {
                            me->cond_animtimer[z] = 0;
                        }
                    }
                }
            }
        }
    }
    
    VOIDRETURN();       
}

void adv_crmotion_with_missing(int missing_ticks) {
    int z;
    
    onstack("adv_crmotion_with_missing");
    
    for (z=0;z<NUM_INST;++z) {
        if (oinst[z]) {
            struct inst *p_inst = oinst[z];
            if (p_inst->crmotion) {
                int cropmax = Arch(p_inst->arch)->cropmax;
                
                p_inst->crop += (p_inst->crmotion * missing_ticks);

                if (p_inst->crop < 0) p_inst->crop = 0;
                else if (p_inst->crop > cropmax) p_inst->crop = cropmax;
            }            
        }
    } 
    
    VOIDRETURN();
}

