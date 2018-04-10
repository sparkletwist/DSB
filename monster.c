#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "istack.h"
#include "objects.h"
#include "global_data.h"
#include "timer_events.h"
#include "champions.h"
#include "lua_objects.h"
#include "gproto.h"
#include "uproto.h"
#include "monster.h"
#include "arch.h"

extern struct inst *oinst[NUM_INST];
extern struct global_data gd;
extern struct dungeon_level *dun;

extern FILE *errorlog;
extern lua_State *LUA;

extern istack istaparty;

void spawned_monster(unsigned int inst, struct inst *p_m_inst, int lev) {
    struct obj_arch *p_m_arch;
    struct ai_core *ai_c;
    struct dungeon_level *dd;
    int actrate = 1;
    int srate = 0;
    int fliprate = 0;
    int mhp, mhp_extra;
    int vlev;
    int rn;
        
    onstack("spawned_monster");

    // small random factor so monsters aren't perfectly synchronized
    rn = (DSBtrivialrand() % 6);     

    p_m_arch = Arch(p_m_inst->arch);
    
    actrate = rqueryluaint(p_m_arch, "act_rate");
    fliprate = queryluaint(p_m_arch, "flip_rate");
    srate = queryluaint(p_m_arch, "shift_rate");
    
    if (UNLIKELY(actrate == 0)) actrate = 5000;
    
    p_m_inst->ai = ai_c = dsbcalloc(1, sizeof(struct ai_core));
    
    if (lev < 0) {
        lua_getglobal(LUA, "sys_actual_level");
        vlev = lua_tointeger(LUA, -1);
        lua_pop(LUA, 1);
    } else
        vlev = lev;
    
    mhp = call_member_func(inst, "init_hp", vlev);
    if (!mhp) {
        mhp = rqueryluaint(p_m_arch, "hp");
        
        gd.lua_take_neg_one = 1;
        mhp = lc_parm_int("sys_init_monster_hp", 2, mhp, vlev);
        if (mhp <= 0) mhp = 1;
    }
    ai_c->hp = ai_c->maxhp = mhp;
    
    ai_c->X_lcx = 0;
    ai_c->X_lcy = 0;
    ai_c->action_delay = 0;
    
    ai_c->sface = 0;
    ai_c->controller = add_timer(inst, EVENT_MONSTER, actrate + rn, 
        actrate, srate, fliprate, 0);
    ai_c->controller->investigate = 4;
    
    VOIDRETURN();    
}

void defer_kill_monster(unsigned int inst) {
    onstack("defer_kill_monster");
    
    if (gd.queue_rc) {
        instmd_queue(INSTMD_KILL_MONSTER, 0, 0, 0, 0, inst, 0);
    } else {
        kill_monster(inst);
    }
    
    VOIDRETURN();
}

void kill_monster(unsigned int inst) {
    struct inst *p_inst = oinst[inst];
    
    onstack("kill_monster");
    
    call_member_func(inst, "on_die", -1);
    if (oinst[inst] == p_inst)
        lc_parm_int("sys_kill_monster", 1, inst);
    if (oinst[inst] == p_inst) {
        queued_inst_destroy(inst);
    }
    
    VOIDRETURN();
}

void kill_all_walking_dead_monsters(void) {
    unsigned int i;
    
    onstack("kill_all_walking_dead_monsters");
    
    for(i=1;i<NUM_INST;++i) {
        if (oinst[i] && oinst[i]->ai) {
            if (oinst[i]->ai->ai_flags & AIF_DEAD) {
                defer_kill_monster(i);
            }
        }
    }
    
    VOIDRETURN();   
}

struct inst_loc *monster_groupup(unsigned int l_inst, struct inst *leader,
    unsigned int *icount, int take_all)
{
    struct inst_loc *team = NULL; 
    struct inst_loc *boss;
    struct obj_arch *p_l_arch;
    int n, lev, xx, yy;
    int totalgroup = 0;
    
    onstack("monster_groupup");
    
    if (leader->tile == DIR_CENTER)
        goto BOSS_ONLY;
    
    lev = leader->level;
    xx = leader->x;
    yy = leader->y;
    
    p_l_arch = Arch(leader->arch);
    
    for(n=0;n<4;++n) {
        struct inst_loc *dt_il = dun[lev].t[yy][xx].il[n];
        
        // don't try to shove around more than 2 size 2's
        // (only 1 because the boss is automatically added)
        if (!take_all) {
            if (p_l_arch->msize == 2 && totalgroup == 1){
                break;
            }
        }
        
        // otherwise we could form a group without the boss in it
        if (!take_all && n == leader->tile)
            continue;
        
        while (dt_il != NULL) {
            if (dt_il->i != l_inst) {
                struct inst *p_inst = oinst[dt_il->i];
                if (p_inst->ai && !dt_il->slave) {
                    int valid_group = 0;
                    
                    if (p_inst->arch == leader->arch)
                        valid_group = 1;
                    else if (p_l_arch->grouptype > 0) {
                        struct obj_arch *p_arch = Arch(p_inst->arch);
                        if (p_arch->grouptype == p_l_arch->grouptype) {
                            valid_group = 1;
                        }
                    }
                    
                    if (p_inst->ai->ai_flags & AIF_UNGROUPED) {
                        if (gd.ungrouphack)
                            p_inst->ai->ai_flags &= ~(AIF_UNGROUPED);
                        else
                            valid_group = 0;
                    }
                    
                    if (valid_group) {
                        struct inst_loc *n_l;                   
                        n_l = dsbfastalloc(sizeof(struct inst_loc));
                        memcpy(n_l, dt_il, sizeof(struct inst_loc));
                        n_l->n = team;
                        team = n_l;
                        ++totalgroup;
                        
                        if (icount) (*icount)++;
                        
                        // only take one monster per subtile
                        // in movement actions
                        if (!take_all)
                            break;
                    }
                }
            }
            
            dt_il = dt_il->n;   
        }
    }
    
    BOSS_ONLY:
    boss = dsbfastalloc(sizeof(struct inst_loc));
    boss->i = l_inst;
    boss->slave = boss->link_unusedv2 = boss->unusedv3 = boss->unusedv4 = 0;
    boss->n = team;
    if (icount) (*icount)++;
    
    RETURN(boss);
}

// checks if the monster will collide
int moncheck(unsigned int inst, int lev, int tx, int ty, int walls) {
    int cc;
    int check;
    int hit = 0;
    int info = 0;
    int u = 0;
    
    for (u=0;u<4;++u) {
        if (lev == gd.p_lev[u] &&
            tx == gd.p_x[u] &&
            ty == gd.p_y[u])
        {
            return -255;
        }
    }
    
    check = check_for_wall(lev, tx, ty);
    
    if (check)
        return -1;
    
    for (cc=0;cc<5;++cc) {
        int t_hit;

        t_hit = obj_collision(lev, tx, ty, cc, inst, &info);
        
        if (t_hit)
            hit = 1;    
    }
        
    if (hit)
        return info;
    
    return 0;
}

// prevents flying projectiles from going through a moving monster
void mon_extcollision(struct inst_loc *team, int dir, int tx, int ty) {
    struct dungeon_level *dd;
    struct inst_loc *il;
    struct inst_loc *PUSHCOL = NULL;
    int vf = (dir+1)%4;
    struct inst *p_inst;

    onstack("mon_extcollision");
    
    p_inst = oinst[team->i];
    dd = &(dun[p_inst->level]);

    if (p_inst->tile == DIR_CENTER) {
        int cx = p_inst->x;
        int cy = p_inst->y;
        
        il = dd->t[cy][cx].il[dir];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, dir, dir, team->i);

        il = dd->t[ty][tx].il[(dir+3)%4];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, dir, dir, team->i);
        
        il = dd->t[cy][cx].il[vf];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, vf, dir, team->i);
        
        il = dd->t[ty][tx].il[(dir+2)%4];
        PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, vf, dir, team->i);

        remove_openshot_on_pushdest(dd, tx, ty, dir);
        remove_openshot_on_pushdest(dd, tx, ty, vf);
        
    } else {
        unsigned int i_vpid[4] = { 0 };
        int cx = p_inst->x;
        int cy = p_inst->y;

        while (team != NULL) {
            struct inst *p_t_inst = oinst[team->i];
            int i_tpos = p_t_inst->tile;

            if (i_tpos < 4)
                i_vpid[i_tpos] = team->i;
                
            team = team->n;
        }
        
        if (i_vpid[dir]) {
            int i_col = i_vpid[dir];
            
            il = dd->t[cy][cx].il[dir];
            PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, dir, dir, i_col);

            il = dd->t[ty][tx].il[(dir+3)%4];
            PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, dir, dir, i_col);
            
            remove_openshot_on_pushdest(dd, tx, ty, dir);

        } else if (i_vpid[(dir+3)%4]) {
            int bf = (dir+3)%4;
            int i_col = i_vpid[bf];

            il = dd->t[cy][cx].il[bf];
            PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, bf, dir, i_col);

            il = dd->t[cy][cx].il[dir];
            PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, bf, dir, i_col);
            
            remove_openshot_on_pushdest(dd, tx, ty, bf);
        }

        // vf = (dir+1)%4
        if (i_vpid[vf]) {
            int i_col = i_vpid[vf];
            
            il = dd->t[cy][cx].il[vf];
            PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, vf, dir, i_col);

            il = dd->t[ty][tx].il[(dir+2)%4];
            PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, vf, dir, i_col);
            
            remove_openshot_on_pushdest(dd, tx, ty, vf);

        } else if (i_vpid[(dir+2)%4]) {
            int bf = (dir+2)%4;
            int i_col = i_vpid[bf];

            il = dd->t[cy][cx].il[bf];
            PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, bf, dir, i_col);

            il = dd->t[cy][cx].il[vf];
            PUSHCOL = fast_il_pushcol(PUSHCOL, il, tx, ty, bf, dir, i_col);
            
            remove_openshot_on_pushdest(dd, tx, ty, bf);
        }
    }
    
    VOIDRETURN();
}

int monster_telefrag(int lev, int xx, int yy) {
    int sdir;
    
    onstack("monster_telefrag");
    
    for(sdir=0;sdir<=4;++sdir) {
        struct inst_loc *il = dun[lev].t[yy][xx].il[sdir];
        while (il != NULL) {
            struct inst_loc *n = il->n;
            struct inst *p_inst = oinst[il->i];
            struct obj_arch *p_arch = Arch(p_inst->arch);
            
            if (p_arch->type == OBJTYPE_MONSTER)
                kill_monster(il->i);
            
            il = n;
        }  
    }
    
    VOIDRETURN();
}

int mon_rotate2wide(unsigned int inst, struct inst *pb, int rdir, int b_tele, int b_instant_turn) {
    struct inst_loc *teamx;
    struct inst *p_boss;
    struct obj_arch *p_boss_arch;
    int s_rdir = rdir+1;
    int other;
    int setdir = 0;
    
    onstack("mon_rotate2wide");
    
    // hack a "group" for a call from a teleporter
    // pb is NULL when used from a teleporter!
    if (b_tele) {
        teamx = dsbfastalloc(sizeof(struct inst_loc));
        teamx->i = inst;
        teamx->n = NULL;
    } else {
        teamx = monster_groupup(inst, pb, NULL, 0); 
    }
       
    p_boss = oinst[teamx->i];
    p_boss_arch = Arch(p_boss->arch);

    if (!b_tele && !(p_boss_arch->arch_flags & ARFLAG_INSTTURN) &&
        !b_instant_turn && (rdir+2)%4 == p_boss->facedir) 
    {
        setdir = 1;
        p_boss->ai->sface = s_rdir;
        rdir += 1 + 2*(DSBtrivialrand()%2);
        rdir %= 4;
    }

    depointerize(teamx->i);
    if (p_boss->facedir == p_boss->tile) {
        p_boss->facedir = p_boss->tile = rdir;
        other = (p_boss->tile+1)%4;
    } else {
        p_boss->facedir = rdir;
        p_boss->tile = (rdir+1)%4;
        other = (p_boss->tile+3)%4;
    }
    pointerize(teamx->i);
    
    if (teamx->n) {
        unsigned int oth_num = teamx->n->i;
        struct inst *p_other = oinst[oth_num];
        depointerize(oth_num);
        p_other->facedir = rdir;
        p_other->tile = other;
        pointerize(oth_num);

        if (setdir)
            p_other->ai->sface = s_rdir;
        
        // safety check
        teamx = teamx->n->n;
        while (teamx != NULL) {
            kill_monster(teamx->i);
            teamx = teamx->n;
        }   
    }
    
    RETURN(1);
}

void mon_rotateteam(int rmethod, struct inst_loc *team, int rdir, int frozenalso, int b_instant_turn) {
    struct inst *p_boss;
    struct obj_arch *p_boss_arch;
    int rf = -1;
    
    onstack("mon_rotateteam");
    
    p_boss = oinst[team->i];
    p_boss_arch = Arch(p_boss->arch);
    
    if (rmethod & ROTATE_2PRE) {
        if (p_boss->tile != DIR_CENTER &&
            (p_boss_arch->rhack & RHACK_2WIDE))
        {
            mon_rotate2wide(team->i, p_boss, rdir, 0, b_instant_turn);
            VOIDRETURN();
        }
    }
    
    if (rmethod & ROTATE_POST) {    
        while (team != NULL) {
            struct inst *pm = oinst[team->i];
            
            if (frozenalso || !(pm->gfxflags & G_FREEZE)) {
                if (!(p_boss_arch->arch_flags & ARFLAG_INSTTURN) &&
                    !b_instant_turn && 
                    ((rdir+2)%4 == pm->facedir)) 
                {
                    pm->ai->sface = rdir+1;
                    if (rf == -1) rf = 1 + 2*(DSBtrivialrand()%2);
                    pm->facedir = (pm->facedir+rf)%4;
                } else {
                    pm->facedir = rdir;
                    pm->ai->sface = 0;
                }
            }
            
            team = team->n;
        }
    }
    
    VOIDRETURN();    
}

int monvalidateboss(struct inst *p_m_inst, int bossnum) {
    
    struct inst *boss_i = oinst[bossnum];
        
    if (!boss_i || !boss_i->ai)
        bossnum = 0;
    else if ((p_m_inst != boss_i) && (boss_i->gfxflags & G_FREEZE))
        // frozen monsters can't be the group boss
        // but solos declare themselves boss so we don't
        // want to interfere in that or other things will go wrong
        bossnum = 0;
    else if (boss_i->arch != p_m_inst->arch) {
        struct obj_arch *b_arch = Arch(boss_i->arch);
        if (!b_arch->grouptype)
            bossnum = 0;
        else {
            struct obj_arch *p_arch = Arch(p_m_inst->arch);
            if (b_arch->grouptype != p_arch->grouptype)
                bossnum = 0;
        }
    } else if (boss_i->level != p_m_inst->level)
        bossnum = 0;
    else if (boss_i->x != p_m_inst->x)
        bossnum = 0;
    else if (boss_i->y != p_m_inst->y)
        bossnum = 0;
        
    else if (UNLIKELY(boss_i->ai->boss != bossnum)) {
        int sbossnum = boss_i->ai->boss;
        struct inst *p_inst;

        fprintf(errorlog, "AI: Boss %d thinks %d is boss!\n", bossnum, sbossnum);
        
        if (sbossnum) {
            p_inst = oinst[sbossnum];
            if (!p_inst || !p_inst->ai)
                fprintf(errorlog, "AI: %d does not exist\n", sbossnum);
            else
                fprintf(errorlog, "AI: %d thinks %d is boss\n", sbossnum, p_inst->ai->boss);
        }

        poop_out("Monsters can't agree who is boss.\nThis is more than likely a bug in DSB.\n\nSee log.txt for details");
        return 0;
    }
        
    return bossnum;
}

void mon_move(unsigned int inst, int lev, int x, int y, int ndir, int isboss, int frozenalso) {
    struct inst *p_m_inst;
    int sdir;
    int isbossv = (isboss)? 1 : -1;
    int no_event = (isboss < 0);
    
    onstack("mon_move");
    
    p_m_inst = oinst[inst];
    
    // frozen monsters don't move unless specifically instructed
    if (!frozenalso && (p_m_inst->gfxflags & G_FREEZE)) {
        VOIDRETURN();
    }
    
    p_m_inst->ai->ai_flags &= 0xFFFF0000;
    
    sdir = p_m_inst->tile;
    if (ndir < 0)
        ndir = sdir;
        
    depointerize(inst);
    i_to_tile(inst, T_FROM_TILE);
    
    p_m_inst = oinst[inst];
    if (!p_m_inst)
        VOIDRETURN();
    
    gd.depointerized_inst = inst;
    
    p_m_inst->gfxflags &= ~(G_UNMOVED);
    
    place_instance(inst, lev, x, y, ndir, 0);
    // got the rug pulled out from under me
    if (!oinst[inst]) {
        if (gd.depointerized_inst == inst)
            gd.depointerized_inst = 0;
        VOIDRETURN();
    }
    
    if (!no_event) {
        call_member_func(inst, "on_move", isbossv);   
    } 

    VOIDRETURN();
} 

int lmon_dirmove(int inst, int dir, int fdir) {
    int dx, dy, tx, ty;
    int lev;
    struct inst *p_m;
    struct obj_arch *p_m_arch;
    struct ai_core *ai_c;
    int r_v = 0;
    int isblocked;
    int curface;
    
    char dirparams[200];
    
    onstack("lmon_dirmove");
    
    v_onstack("lmon_dirmove.params");
    sprintf(dirparams, "%d, %d, %d)", inst, dir, fdir); 
    v_upstack();
    stackbackc('(');
    v_onstack(dirparams);

    p_m = oinst[inst];
    ai_c = p_m->ai;
    p_m_arch = Arch(p_m->arch);
    
    if (fdir < 0)
        fdir = dir;

    if (p_m->level < 0) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "Bad AI_MOVE on inst %d (level = %d)", inst, p_m->level);
        RETURN(0);
    }
    
    if (dir < 0) {
        if (fdir >= 0) {
            tx = p_m->x;
            ty = p_m->y;
            lev = p_m->level;
            isblocked = 0;
        } else {
            RETURN(0);
        }
    } else {
        if (p_m_arch->arch_flags & ARFLAG_IMMOBILE)
            RETURN(0);
            
        if (ai_c->no_move)
            RETURN(0);
            
        face2delta(dir, &dx, &dy);
        tx = dx + p_m->x;
        ty = dy + p_m->y;
        lev = p_m->level;
        isblocked = moncheck(inst, lev, tx, ty, ai_c->wallbelief);
        if (isblocked) {
            RETURN(0);
        }
    }
    curface = p_m->facedir;
    
    luastacksize(8);

    if (member_begin_call(inst, "on_want_move")) {
        int ia_targs[4];

        lua_pushinteger(LUA, lev);
        lua_pushinteger(LUA, tx);
        lua_pushinteger(LUA, ty);
        lua_pushinteger(LUA, fdir);
        
        if (ai_c->fear)
            lua_pushinteger(LUA, MOVE_FEAR);
        else {
            if (ai_c->ai_flags & AIF_PARTY_SEE)
                lua_pushinteger(LUA, MOVE_PARTY);
            else
                lua_pushinteger(LUA, MOVE_NORMAL);
        }
        lua_pushnil(LUA); // 6 parameters
        
        member_finish_call(6, 4);

        member_retrieve_stackparms(4, ia_targs);
        lev = ia_targs[0];
        tx = ia_targs[1];
        ty = ia_targs[2];
        fdir = ia_targs[3];
        
        isblocked = CRAZY_UNDEFINED_VALUE;
    }
    
    if (lev < 0 || (lev == p_m->level && tx == p_m->x && ty == p_m->y)) {
        struct inst_loc *team;
        int num = 0;

        team = monster_groupup(inst, p_m, &num, 0);
        mon_rotateteam(ROTATE_FULL, team, fdir, 0, 0);

        RETURN(1);
        
    } else if (isblocked == CRAZY_UNDEFINED_VALUE) {
        isblocked = moncheck(inst, lev, tx, ty, ai_c->wallbelief);
    }
    
    if (p_m_arch->arch_flags & ARFLAG_IMMOBILE)
        RETURN(0);

    if (!isblocked) {
        struct inst_loc *team;
        int num = 0;
        int move_handled = 0;
        struct inst_loc *dsq;

        team = monster_groupup(inst, p_m, &num, 0);
    
        mon_rotateteam(ROTATE_2PRE, team, fdir, 0, 1);
        mon_extcollision(team, dir, tx, ty);
        mon_rotateteam(ROTATE_POST, team, fdir, 0, 1);
        
        // some squares "grab" the monsters using an entirely different process
        dsq = dun[p_m->level].t[ty][tx].il[DIR_CENTER];
        ++gd.queue_rc;
        ++gd.monster_move_ok;
        while (dsq != NULL) {
            move_handled = call_member_func2(dsq->i, "on_monster_move_into", inst, dir);
            if (move_handled) {
                break;
            }
            dsq = dsq->n;
        }
        --gd.monster_move_ok;
        --gd.queue_rc;
        if (!gd.queue_rc)
            flush_instmd_queue();        

        if (!oinst[inst]) {
            RETURN(0);
        } else {
            // the boss has changed. update it!
            if (oinst[inst]->ai->boss > 0) {
                inst = oinst[inst]->ai->boss;
            }
        }
        
        if (!move_handled) {
            int lev = p_m->level; // in case someone moves the boss
            unsigned int tag = ((inst << 16) | (ty << 8) | ty);
            while (team != NULL) {
                //oinst[team->i]->ai->groupid = tag;
                mon_move(team->i, lev, tx, ty, -1, (team->i == inst), 0);
                team = team->n;
            }
        }

        r_v = 1;
    }

    RETURN(r_v);
}

int monster_move(struct inst_loc *team, int dist, int perc, int bp) {
    struct inst *p_m_boss;
    int r_v;
    int i;

    onstack("monster_move");
    
    i = team->i;
    p_m_boss = oinst[i];

    // resolve init sface
    if (p_m_boss->ai->sface) {
        v_onstack("monster_move.resolve_sface");
        mon_rotateteam(ROTATE_FULL, team, p_m_boss->ai->sface - 1, 1, 1);
        p_m_boss->ai->sface = 0;
        p_m_boss->ai->controller->time_until = 1;
        v_upstack();
        RETURN(-1);
    }

    ist_push(&istaparty, bp);
    ist_cap_top(&istaparty);
    if (dist) {
        r_v = lc_parm_int("sys_ai_far", 1, i);
    } else {
        r_v = lc_parm_int("sys_ai_near", 3, i, perc);
    }
    ist_check_top(&istaparty);
    ist_pop(&istaparty);
    
    // if i self destruct or swap or something
    p_m_boss = oinst[i];
    if (!p_m_boss) {
        RETURN(r_v);
    }

    // set up resolution for next time
    if (p_m_boss->ai->sface) {
        int interval = p_m_boss->ai->controller->time_until;
        p_m_boss->ai->controller->time_until = lc_parm_int("sys_calc_shortinterval", 1, interval);
    }

    RETURN(i);
}

int ai_msg_handler(unsigned int inst, int ai_msg, int parm, int lparm) {
    static int ai_handler = 0;
    int r_v = 0;
    int zr;
    int i_time;
    struct inst *pm;
    struct ai_core *ai_c;
    struct timer_event *con;

    onstack("ai_msg_handler");
    if (ai_handler) {
        stackbackc('_');
        v_onstack("r");
    }

    pm = oinst[inst];
    ai_c = pm->ai;
    con = ai_c->controller;

    // deliberately make this part of the function non re-entrant
    // so subsequent calls won't cause infinite recursion
    if (!ai_handler) {
        ai_handler = 1;

        zr = call_member_func(inst, "ai_message", ai_msg);
        if (zr)
            RETURN(zr);

        ai_handler = 0;
    }

    switch(ai_msg) {
        case (AI_HAZARD):
            if (parm < 0) {
                r_v = !!(ai_c->ai_flags & AIF_HAZARD);
                gd.lua_bool_return = 1;
            } else {
                ai_c->ai_flags |= AIF_HAZARD;
                i_time = con->maxtime/2;
                if (i_time < 6) i_time = 6;
                if (i_time > 14) i_time = 14;
                if (!(ai_c->ai_flags & AIF_NOFASTMOVE) && con->time_until > i_time)
                {
                    con->time_until = i_time;
                    ai_c->ai_flags |= AIF_NOFASTMOVE;
                }
            }
            break;
            
        case (AI_WALL_BELIEF):
            if (parm < 0) {
                r_v = ai_c->wallbelief;
                gd.zero_int_result = 1;
            } else {
                ai_c->wallbelief = parm;
            }
            break;
            
        case (AI_TIMER):
            if (parm < 0) {
                r_v = con->time_until;
                gd.zero_int_result = 1;
            } else {
                con->time_until = parm;
            }
            break;

        case (AI_DELAY_TIMER):
            if (parm >= 0) {
                con->time_until += parm;
            }
            break;
            
        case (AI_DELAY_ACTION):
            if (parm == AI_P_QUERY) {
                r_v = ai_c->action_delay;
                gd.zero_int_result = 1;
            } else {
                int ad = ai_c->action_delay + parm;
                if (ad < 0)
                    ad = 0;
                ai_c->action_delay = ad;
            }
             
            break;
            
        case (AI_DELAY_EVERYTHING):
            if (parm > 0) {
                ai_c->action_delay += parm;
                if (con->shift)
                    con->shift += (parm + 1);
                if (con->flip)
                    con->flip += (parm + 1);
            }
            break;

        case (AI_STUN):
            if (parm < 0) {
                r_v = ai_c->no_move;
            } else if (!ai_c->no_move) {
                ai_c->no_move = parm;
                if (ai_c->no_move > 15)
                    ai_c->no_move = 15;
            }

            break;

        case (AI_FEAR):
            if (parm == AI_P_QUERY) {
                r_v = ai_c->fear;
            } else {
                if (!ai_c->fear && parm > 0) {
                    if (ai_c->controller->time_until > 3)
                        ai_c->controller->time_until = 3;
                    ++parm;
                }

                if (parm > 0) {
                    ai_c->fear += parm;
                    if (ai_c->fear > 23)
                        ai_c->fear = 23;
                } else if (parm < 0) {
                    int afear = ai_c->fear;
                    afear += parm;
                    if (afear < 0) afear = 0;
                    ai_c->fear = afear;
                }
                    
            }
            break;
            
        case (AI_SEE_PARTY):
            if (parm < 0) {
                r_v = ai_c->see_party;
            } else {
                if (parm > 10) parm = 10;
                ai_c->see_party = parm;
                ai_c->ai_flags |= AIF_PARTY_SEE;
            }
            break;
            
        case (AI_UNGROUP):
            ai_c->boss = inst;
            ai_c->ai_flags |= AIF_UNGROUPED;
            ai_c->controller->time_until += 2;
            break;
            
        case (AI_TARGET):
            gd.lua_nonfatal = 1;
            DSBLerror(LUA, "Use of AI_TARGET is no longer supported. Use add_monster_target(id, ttl, x, y)");
            break;

        case (AI_MOVE_NOW):
            if (parm < 0) {
                r_v = !!(ai_c->ai_flags & AIF_MOVE_NOW);
                gd.lua_bool_return = 1;
            } else {
                int store_delay = ai_c->action_delay;
                ai_c->action_delay = 0;
                ai_c->ai_flags |= AIF_MOVE_NOW;
                ai_c->ai_flags &= ~AIF_NOFASTMOVE;               
                ai_c->controller->time_until = 0;
                monster_acts(inst);
                ai_c->controller->time_until = ai_c->controller->maxtime;
                ai_c->ai_flags |= AIF_NOFASTMOVE;
                ai_c->action_delay = store_delay;
            }
            break;
            
            
        case (
        AI_TURN):
            if (parm < 0 || parm > 3) {
                gd.lua_nonfatal = 1;
                DSBLerror(LUA, "Invalid AI_TURN dir for inst %d", inst);
                RETURN(0);
            }
            
            r_v = lmon_dirmove(inst, -1, parm);
                        
            gd.lua_bool_return = 1;
            break;

        case (AI_MOVE):
            if (parm < 0 || parm > 3) {
                gd.lua_nonfatal = 1;
                DSBLerror(LUA, "Invalid AI_MOVE dir for inst %d", inst);
                RETURN(0);
            }

            if (lparm > 3) {
                gd.lua_nonfatal = 1;
                DSBLerror(LUA, "Invalid AI_MOVE facedir for inst %d", inst);
                RETURN(0);
            }
            
            if (oinst[inst] && !(oinst[inst]->gfxflags & G_FREEZE)) {
                r_v = lmon_dirmove(inst, parm, lparm);
                gd.lua_bool_return = 1;
            }
            break;
            
        case (AI_ATTACK_BMP):
            if (parm < 0) {
                r_v = ai_c->attack_anim + 1;
                gd.zero_int_result = 1;
            } else {
                struct obj_arch *p_arch;
                p_arch = Arch(pm->arch);
                
                if (parm > 0)
                    --parm;
                
                if (parm < p_arch->iv.n)
                    ai_c->attack_anim = parm;
                else {
                    gd.lua_nonfatal = 1;
                    DSBLerror(LUA, "Invalid ATTACK_BMP %d for inst %d", parm+1, inst);
                    RETURN(0);
                }
            }
            break;
    }

    RETURN(r_v);
}

int is_monster_distant(unsigned int inst, int *p_perc, int *p_best_party) {
    struct inst *p_m_inst;
    int total_light;
    int sq_dist;
    int perc, sq_perc, r_dist; 
    int mylev;
    int i;
    int lx, ly;
    int best_party = 0;
    int distant_monster = 1;
    
    onstack("is_monster_distant");
    
    p_m_inst = oinst[inst]; 
    mylev = p_m_inst->level;
    if (mylev < 0) {
        RETURN(1);
    }
    
    total_light = calc_total_light(&(dun[mylev]));
    if (total_light > 100)
        total_light = 100;
    else if (total_light < 0)
        total_light = 0;
    perc = lc_parm_int("sys_calc_sight", 2, inst, total_light);
    
    lx = p_m_inst->x;
    ly = p_m_inst->y;

    if (perc > 8) perc = 8;
    if (perc < 1) perc = 1;
    sq_perc = (perc * perc);
    
    sq_dist = 500;
    for (i=0;i<4;++i) {
        int n_sq_dist;
        if (gd.p_lev[i] != mylev) continue;
        n_sq_dist = (lx - gd.p_x[i]) * (lx - gd.p_x[i]);
        n_sq_dist += (ly - gd.p_y[i]) * (ly - gd.p_y[i]);
        
        if (n_sq_dist < sq_dist) {
            sq_dist = n_sq_dist;
            best_party = i;
        }
    }

    if (sq_perc < 8)
        r_dist = 8;
    else
        r_dist = sq_perc;
        
    distant_monster = (sq_dist > r_dist); 
    
    if (p_perc)
        *p_perc = perc;
    if (p_best_party)
        *p_best_party = best_party;
    RETURN(distant_monster);
}

void monster_investigates(unsigned int inst) {
    int perc = 0;
    
    onstack("monster_investigates");
    if (!is_monster_distant(inst, &perc, NULL)) {
        lc_parm_int("sys_ai_investigate", 2, inst, perc);
    }   
    VOIDRETURN();
}

int monster_acts(unsigned int inst) {
    struct inst *p_m_inst;
    struct ai_core *ai_c;
    struct inst_loc *team = NULL;
    int mylev;
    int pl;
    int i;
    int psamelev = 0;
    int best_party = 0;
    int membs = 0;
    int perc;
    int distant_monster;

    onstack("monster_acts");

    p_m_inst = oinst[inst];
    if (UNLIKELY(!p_m_inst)) {
        recover_error("Invalid monster in timer table");
        RETURN(0);
    } else if (UNLIKELY(!p_m_inst->ai)) {
        coord_inst_r_error("Zombie monster in timer table", inst, 255);
        RETURN(0);
    }
    
    if (UNLIKELY(p_m_inst->CHECKVALUE != DSB_CHECKVALUE)) {
        coord_inst_r_error("Check value overwritten", inst, p_m_inst->CHECKVALUE);
        RETURN(0);
    }
    
    ai_c = p_m_inst->ai;
    
    // decrease the internal time delay
    if (ai_c->action_delay > 0) {
        ai_c->action_delay -= 1;
        ai_c->controller->time_until = 1;
        RETURN(0);
    }
        
    if (ai_c->state < 0)
        RETURN(0);        

    mylev = p_m_inst->level;
    if (mylev < 0)
        RETURN(0);

    // make sure i'm still associated with a valid boss
    if (ai_c->boss > 0)
        ai_c->boss = monvalidateboss(p_m_inst, ai_c->boss);

    // i'm either new or my old boss was invalid...
    // i need to either make myself a boss, or find a boss
    if (!ai_c->boss) {
        int foundboss = 0;
        struct inst_loc *heremon;

        team = monster_groupup(inst, p_m_inst, &membs, 1);

        heremon = team;
        while (heremon != NULL) {
            struct inst *p_hm = oinst[heremon->i];
            if (heremon->i != inst && p_hm->ai->boss == heremon->i) {
                foundboss = heremon->i;
                break;
            }
            heremon = heremon->n;
        }
        if (foundboss) {
            foundboss = monvalidateboss(p_m_inst, foundboss);
            ai_c->boss = foundboss;
        }

        // nobody's a good boss, so i'll appoint myself
        if (!foundboss) {
            heremon = team;
            while (heremon != NULL) {
                struct inst *p_m = oinst[heremon->i];
                p_m->ai->boss = inst;
                heremon = heremon->n;
            }
        }
    }
    
    // this shouldn't happen
    if (UNLIKELY(ai_c->boss == 0)) {
        coord_inst_r_error("Rogue monster tried to move", inst, 255);
        RETURN(0);
    }

    // i'm not the boss, so wait for the boss's turn
    if (ai_c->boss != inst)
        RETURN(0);

    if (!team)
        team = monster_groupup(inst, p_m_inst, &membs, 0);
     
    // remove any delays   
    if (ai_c->no_move > 0) {
        ai_c->no_move--;
    }

    pl = -99;
    psamelev = 0;
    for (i=0;i<4;++i) {
        if (gd.p_lev[i] < 0) continue;
        if (mylev == gd.p_lev[i]) {
            ++psamelev;
            pl = gd.p_lev[i];
        } else {
            // keep a near level
            if (!psamelev) {
                if (!(mylev == (pl+1) || mylev == (pl-1)))
                    pl = gd.p_lev[i];
            }
        }
    }
    
    if (!psamelev) {
        if ((gd.run_everywhere_count == 0) || mylev == (pl+1) || mylev == (pl-1)) {
            if (ai_c->see_party) ai_c->see_party--;
            monster_move(team, 1, 0, 0);
        } else
            ai_c->see_party = 0;

        RETURN(!oinst[team->i]);
    }
    
    distant_monster = is_monster_distant(inst, &perc, &best_party);
        
    if (ai_c->see_party)
        ai_c->see_party--;

    if (distant_monster) {
        monster_move(team, 1, 0, 0);
        RETURN(!oinst[team->i]);
    }

    monster_move(team, 0, perc, best_party);

    RETURN(0);
}
