#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "objects.h"
#include "global_data.h"
#include "champions.h"
#include "lua_objects.h"
#include "gproto.h"
#include "uproto.h"
#include "timer_events.h"
#include "movebuffer.h"
#include "arch.h"
#include "gamelock.h"
#include "keyboard.h"
#include "mparty.h"

extern const char *CSSAVEFILE[];
extern char *Iname[4];
extern char *Backup_Iname;
extern int debug;

extern const char *Sys_Inventory;

struct inst *oinst[NUM_INST];

extern struct global_data gd;
extern struct graphics_control gfxctl;
extern struct inventory_info gii;

extern lua_State *LUA;
extern FILE *errorlog;

struct clickzone cz[NUM_ZONES];
int lua_cz_n;
struct clickzone *lua_cz;

extern int PPis_here[4];
extern int viewstate;
extern struct dungeon_level *dun;

int forbid_magic(int ppos, int who) {
    int ret;
    onstack("forbid_magic");
    ret = lc_parm_int("sys_forbid_magic", 2, ppos, who);
    RETURN(ret);    
}

void enter_inventory_view(int ch) {
    onstack("enter_inventory_view");
    
    if (gd.gl->lockc[LOCK_INVENTORY])
        VOIDRETURN();
    
    // dead champions have no inventory
    if (gd.champs[gd.party[ch]-1].bar[0] <= 0)
        VOIDRETURN();
        
    gd.who_look = ch;
    *gd.gl_viewstate = VIEWSTATE_INVENTORY;
    gd.viewmode = 1;
    gd.need_cz = gfxctl.do_subrend = 1;
    gd.arrowon = gd.litarrow = 0;
    
    lc_parm_int("sys_inventory_enter", 2, ch, gd.party[ch]);
    
    VOIDRETURN();
}

void exit_inventory_view(void) {
    int was_looked = gd.who_look;
    int c_was_looked = gd.party[was_looked];
    
    onstack("exit_inventory_view");
    
    *gd.gl_viewstate = VIEWSTATE_DUNGEON;
    gd.viewmode = 0;
    gd.tmp_champ = 0;
    gd.need_cz = 1;
    gd.who_look = -1;
    
    gd.softframecnt = 4;
        
    destroy_all_subrenderers();
    
    lc_parm_int("sys_inventory_exit", 2, was_looked, c_was_looked);
    
    VOIDRETURN();
}

void enter_champion_preview(int lua_charid) {
    onstack("enter_champion_preview");
    lc_parm_int("__enter_champion_preview", 1, lua_charid);
    VOIDRETURN();   
}

// This function SHOULD NOT use inventory instmd queueing
void exit_champion_preview(int lua_charid, int taken) {
    onstack("exit_champion_preview");
    lc_parm_int("__exit_champion_preview", 2, lua_charid, taken);  
    VOIDRETURN();    
}

void push_magicrunes(int who) {
    int i;
    
    onstack("push_magicrunes");
    
    for (i=0;i<8;i++) {
        int o_rune = gd.i_spell[who][i];

        lua_pushinteger(LUA, o_rune);
    }

    
    VOIDRETURN();
}

void magic_runeclick(int who, int rune) {
    const char *s_name = "sys_rune_cast";
    int cwho;
    int nm;
    
    onstack("magic_runeclick");
    
    // unlike the convention used in the lua scripts, in this
    // case "who" is a ppos
    cwho = gd.party[who] - 1;
    
    nm = forbid_magic(who, cwho+1);
    if (nm)
        VOIDRETURN();
        
    lua_getglobal(LUA, s_name);
    lua_pushinteger(LUA, who);
    lua_pushinteger(LUA, gd.party[who]);
    lua_pushinteger(LUA, rune);
    push_magicrunes(who);
    
    lc_call_topstack(11, s_name);
    
    VOIDRETURN();
}

void magic_backrune(int ppos) {
    int nr;
        
    onstack("magic_backrune");

    for(nr=7;nr>=0;--nr) {
        if (gd.i_spell[ppos][nr] == 0)
            continue;
            
        gd.i_spell[ppos][nr] = 0;
        break;
    }

    VOIDRETURN();
}

void magic_cast(int who) {
    int i, cv, nm;
    const char *s_name = "sys_spell_cast";
    
    onstack("magic_cast");
        
    if (gd.i_spell[who][0] == 0)
        VOIDRETURN();
        
    nm = forbid_magic(who, gd.party[who]);
    if (nm)
        VOIDRETURN();
    
    luastacksize(12);
    
    lua_getglobal(LUA, s_name);
    
    if (!lua_isfunction(LUA, -1)) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "%s is invalid", s_name);
        VOIDRETURN();
    }
    
    lua_pushinteger(LUA, who);
    lua_pushinteger(LUA, gd.party[who]);

    push_magicrunes(who);

    cv = lc_call_topstack(10, s_name);
    
    if (!cv) {
        memset(gd.i_spell[who], 0, 8);
    }
    
    VOIDRETURN();
}

void global_lua_string_function(lua_State *LUA) {
    onstack("global_lua_string_function");
    if (lua_isfunction(LUA, -1)) {
        VOIDRETURN();
    } else if (lua_isstring(LUA, -1)) {
        const char *dstr = dsbstrdup(lua_tostring(LUA, -1));
        lua_pop(LUA, -1);
        lua_getglobal(LUA, dstr);    
        if (!lua_isfunction(LUA, -1)) {
            char errstr[120];
            snprintf(errstr, sizeof(errstr), "Bad stored method string reference: %s", dstr);
            poop_out(errstr);
        }
        dsbfree(dstr);
    }   
    VOIDRETURN();
}

// someday move this over to the lua side
void click_move_button(int z) {

    switch(z) {
        case 0:
            addmovequeue(PRESS_MOVEKEY, MOVE_TURNLEFT, 0);
        return;
        
        case 1:
            addmovequeue(PRESS_MOVEKEY, MOVE_UP, 0);
        return;
        
        case 2:
            addmovequeue(PRESS_MOVEKEY, MOVE_TURNRIGHT, 0);
        return;
        
        case 3:
            addmovequeue(PRESS_MOVEKEY, MOVE_LEFT, 0);
        return;
        
        case 4:
            addmovequeue(PRESS_MOVEKEY, MOVE_BACK, 0);
        return;
        
        case 5:
            addmovequeue(PRESS_MOVEKEY, MOVE_RIGHT, 0);
        return;   
    }
}

void clear_method(void) {
    onstack("clear_method");

    gd.who_method = 0;
    gd.method_obj = 0;
    
    if (gd.cached_lua_method) {
        struct att_method *meth = gd.cached_lua_method;
        int n;
        for (n=0;n<ATTACK_METHOD_TABLESIZE;++n) {
            if (meth[n].name != NULL) {
                dsbfree(meth[n].name);
                luaL_unref(LUA, LUA_REGISTRYINDEX, meth[n].luafunc);
            }
        }
        dsbfree(gd.cached_lua_method);
        gd.cached_lua_method = NULL;
    }
    
    VOIDRETURN();
}

struct att_method *lua_generate_methods(int args, const char *fname) {
    struct att_method *meth = NULL;
    
    onstack("lua_generate_methods");
    
    if (lua_pcall(LUA, args, 1, 0) != 0) {
        lua_function_error("lua_call.determine_method", lua_tostring(LUA, -1));
        RETURN(NULL);
    }
    
    if (lua_istable(LUA, -1)) {
        meth = import_attack_methods(fname);
        gd.cached_lua_method = meth;
    }
    lua_pop(LUA, 1);
    lstacktop();
    
    RETURN(meth);
}

int determine_method(int ppos, int isl) {
    struct att_method *meth = NULL;    
    int invoker_num;
    struct champion *invoker; 
    unsigned int i_obj;
    int i = 0;
    int n_meths = 0;
    
    onstack("determine_method");
    
    invoker_num = gd.party[ppos]-1;
    invoker = &(gd.champs[invoker_num]);
    
    // INV_INVALID expects gd.method_loc to already be valid!
    if (isl != INV_INVALID) {
        i_obj = invoker->inv[isl];
        gd.method_loc = isl;
    } else
        i_obj = gd.method_obj;
    
    memset(gd.c_method, 0, sizeof(struct att_method*) * ATTACK_METHOD_TABLESIZE);
    
    if (i_obj) {
        struct obj_arch *p_arch = Arch(oinst[i_obj]->arch);
        if (p_arch->method)
            meth = p_arch->method;
        else if (p_arch->arch_flags & ARFLAG_METHOD_FUNC) {
            luastacksize(4);
            lua_rawgeti(LUA, LUA_REGISTRYINDEX, p_arch->lua_reg_key);
            lua_pushinteger(LUA, i_obj);
            lua_pushinteger(LUA, invoker_num + 1);
            meth = lua_generate_methods(2, p_arch->luaname);
        }
    } else {
        meth = invoker->method;
        if (!meth) {
            luastacksize(4);
            lua_getglobal(LUA, invoker->method_name);
            lua_pushnil(LUA);
            lua_pushinteger(LUA, invoker_num + 1);
            meth = lua_generate_methods(2, invoker->method_name);
        }
    }
    
    if (meth == NULL)
        RETURN(0);
            
    while (meth[i].name != NULL) {
        struct att_method *curmeth = &(meth[i]);
        int xplevel = determine_mastery(0, invoker_num, curmeth->reqclass, curmeth->reqclass_sub, 1);
        
        if (xplevel >= curmeth->minlev) {
            gd.c_method[n_meths] = curmeth;
            ++n_meths;
        }    
        ++i;       
    }
    
    if (n_meths) {
        gd.method_obj = i_obj;
    } else {
        clear_method();
    }
    
    RETURN(n_meths);
}

void click_method_button(int z, int mloc) {
    int gotmethod = 0;
    int nm;
    
    onstack("click_method_button"); 

    nm = determine_method(z, mloc);
    if (nm) {
        gd.who_method = (z+1);
        gd.num_methods = nm;
        gd.need_cz = 1;
    }  
    
    VOIDRETURN();
}

void selected_method(int z) {
    int rfunc;
    
    onstack("selected_method");
    
    if (z == 0) {
        clear_method();
        gd.num_methods = 0;
        VOIDRETURN();
    }
            
    --z;
    
    stackbackc('(');
    v_onstack(gd.c_method[z]->name);
    addstackc(')');
    
    if (UNLIKELY(gd.who_method < 1)) {
        puke_bad_subscript("gd.who_method", gd.who_method-1);
        VOIDRETURN();
    }
    
    luastacksize(6);
    
    gd.lastmethod[gd.who_method-1][0] = z + 1;
    gd.lastmethod[gd.who_method-1][1] = gd.method_loc;
    gd.lastmethod[gd.who_method-1][2] = gd.method_obj;   
    rfunc = gd.c_method[z]->luafunc;
    
    lua_rawgeti(LUA, LUA_REGISTRYINDEX, rfunc);
    global_lua_string_function(LUA);
    lua_pushstring(LUA, gd.c_method[z]->name);
    lua_pushinteger(LUA, gd.who_method-1);
    lua_pushinteger(LUA, gd.party[gd.who_method-1]);
    if (gd.method_obj)
        lua_pushinteger(LUA, gd.method_obj);
    else
        lua_pushnil(LUA);
        
    clear_method();
    gd.num_methods = 0;     

    lc_call_topstack(4, gd.c_method[z]->name);
    
    VOIDRETURN();
}

void cclear_m_cz(struct clickzone *cp, int n) {
    if (n) {
        memset(cp, 0, sizeof(struct clickzone) * n);
    }
}

void clear_lua_cz(void) {
    if (lua_cz_n) {
        dsbfree(lua_cz);
        lua_cz = NULL;
        lua_cz_n = 0;
    }    
}
    
void new_lua_cz(unsigned int i_zoneflags, struct clickzone **p_cz, int *ct, int zn, int xx, int yy,
    int w, int h, unsigned int cont_inst, unsigned int ext_data)
{
    struct clickzone *i_cz = *p_cz;
      
    while (zn >= *ct) {
        *p_cz = dsbrealloc(i_cz, sizeof(struct clickzone)*((*ct)+8));
        memset((*p_cz)+(*ct), 0, sizeof(struct clickzone)*8);
        
        *ct += 8;
        i_cz = *p_cz;
    }
    
    i_cz[zn].x = xx;
    i_cz[zn].y = yy;
    i_cz[zn].w = w;
    i_cz[zn].h = h;
    i_cz[zn].zoneflags = i_zoneflags;
    
    i_cz[zn].inst = cont_inst;
    i_cz[zn].ext_data = ext_data;
    

}

void new_lua_extcz(unsigned int zoneflags, struct clickzone **p_cz, int *ct, int zn, int xx, int yy,
    int w, int h, unsigned int sysmsg, unsigned int data1, unsigned int data2)
{
    struct clickzone *i_cz;

    new_lua_cz(zoneflags, p_cz, ct, zn, xx, yy, w, h, 0, 0);
    
    i_cz = *p_cz;
    
    i_cz[zn].sys_msg = sysmsg;
    i_cz[zn].sys_data[0] = data1;
    i_cz[zn].sys_data[1] = data2;

}

void deliver_szmsg(int msg, unsigned short parm1, unsigned short parm2) {
    onstack("deliver_szmsg");
    
    if (msg > 0) {
        lc_parm_int("sys_system_message", 3, msg, parm1, parm2);
        VOIDRETURN();    
    }

    switch(msg) {
        case SYS_METHOD_OBJ:
            click_method_button(parm1, parm2);
        break;
        
        case SYS_METHOD_SEL:
            if (gd.who_method && (parm1 == gd.method_obj)) {
                selected_method(parm2);
            } 
        break;
        
        case SYS_METHOD_CLEAR:
            if (parm1 == (gd.who_method - 1)) {
                selected_method(0);
            }
        break;

        case SYS_MAGIC_PPOS:
            if (parm1 < 4)
                gd.whose_spell = parm1;
        break;
        
        case SYS_MAGIC_RUNE:
            // without this check, in the current rune system, two clicks
            // in the same frame make duplicate runes. yes it can happen.
            if (gd.magic_this_frame) {
                if (parm1 < 4)
                    magic_runeclick(parm1, parm2);
                gd.magic_this_frame = 0;
            }
        break;
        
        case SYS_MAGIC_BACK:
            if (gd.magic_this_frame) {
                if (parm1 < 4)
                    magic_backrune(parm1);
                gd.magic_this_frame = 0;
            }
        break;
        
        case SYS_MAGIC_CAST:
            if (gd.magic_this_frame) {
                if (parm1 < 4)
                    magic_cast(parm1);
                gd.magic_this_frame = 0;
            }
        break;

        case SYS_MOVE_ARROW:
            if (parm1 > 0 && parm1 <= 6) {
                --parm1;
                click_move_button(parm1);
            }
        break;
        
        case SYS_OBJ_PUT_AWAY: {
            const char *PUTAWAY = "sys_put_away";
            int ppos = (int)parm1;
                          
            if (ppos >= 0 && PPis_here[ppos]) {
                int l;
                
                if (gd.mouseobj)
                    l = lc_parm_int(PUTAWAY, 2, ppos, gd.mouseobj);
                else
                    l = lc_parm_int(PUTAWAY, 1, ppos);
    
                if (!l) {
                    if (gd.who_look != -1)
                        exit_inventory_view();
                    enter_inventory_view(ppos);
                }
            }            
        } break;
        
        case SYS_LEADER_SET:
            if (parm1 >= 0 && parm1 <= 3)
                change_leader_to(parm1);
        break;
    }

    VOIDRETURN();
}

void got_lua_clickzone(struct clickzone *lcz, int z, int ppos) {
    char pf[4];
    unsigned int putdown = gd.mouseobj;
    unsigned int pickup = 0;
    unsigned int c_inst;
    unsigned int i_edata;
    struct inst *p_c_inst;
    int ipl, ldr;
    
    onstack("got_lua_clickzone");
    
    // a system message in a zone
    if (lcz[z].zoneflags & ZF_SYSTEM_MSGZONE) {
        int sysmsg = lcz[z].sys_msg;
        deliver_szmsg(sysmsg, lcz[z].sys_data[0], lcz[z].sys_data[1]);
        VOIDRETURN();
    }
    
    // a system object in a zone
    if (lcz[z].zoneflags & ZF_SYSTEM_OBJZONE) {
        inventory_clickzone(z | INVZMASK, ppos, 0);
        VOIDRETURN();        
    }
    
    snprintf(pf, sizeof(pf), "%d", z);
    stackbackc('(');
    v_onstack(pf);
    addstackc(')');
    
    c_inst = lcz[z].inst;
    p_c_inst = oinst[c_inst];

    ipl = gd.party[gd.who_look] - 1;

    // a normal msgzone
    i_edata = lcz[z].ext_data;
    if (i_edata) {
        int ap = party_ist_peek(gd.a_party);
        deliver_msg(c_inst, i_edata, z, gd.mouseobj, ap, 0);
        goto FINISH_SUBREND;
    }
    
    if (z < p_c_inst->inside_n)
        pickup = p_c_inst->inside[z];
        
    if (!putdown && !pickup)
        VOIDRETURN();
    
    if (!putdown || call_member_func2(c_inst, "objzone_check", putdown, z)) {
        
        if (pickup) {
            if (call_member_func(c_inst, "inst_outgoing", pickup))
                VOIDRETURN();
        }
        
        if (putdown) {
            if (call_member_func(c_inst, "inst_incoming", putdown))
                VOIDRETURN();
                
            mouse_obj_drop(putdown);
        }
        
        gd.mouseobj = 0;
        if (pickup) {
            p_c_inst->inside[z] = 0;
            mouse_obj_grab(pickup);
        }
        
        if (putdown) {
            inst_putinside(c_inst, putdown, z);
        }
        
        resize_inside(p_c_inst);
        
        FINISH_SUBREND:
        gd.champs[ipl].load = sum_load(ipl);
        determine_load_color(ipl);
        gfxctl.do_subrend = 1;
    }
    
    VOIDRETURN();
}

// (inventory slot #, character #, what to put down, what to pick up, name of location)
int inv_obj_exch(int z, int ppos, int opc,
    int putdown, int pickup)
{
    const char *invfunc = Sys_Inventory;
    struct champion *me;
    char outname[40];
    int ldr;
    int lpickup, lputdown;
    int let_me;
    
    onstack("inv_obj_exch");
    
    if (UNLIKELY(opc < 0)) {
        puke_bad_subscript("gd.champs", opc);
        RETURN(-1);
    }
    
    // convert these params into something lua can use
    if (!pickup) lpickup = -1;
    else lpickup = pickup;
    
    if (!putdown) lputdown = -1;
    else lputdown = putdown;
    
    gd.queue_inv_rc++;
    gd.lua_bool_hack = 1;
    let_me = lc_parm_int(invfunc, 6, opc+1, z, lpickup, lputdown, LHFALSE, LHFALSE);
    me = &(gd.champs[opc]);
    if (!let_me) {
        me->load = sum_load(opc);
        calculate_maxload(opc+1);
        gd.queue_inv_rc--;
        if (!gd.queue_inv_rc) {
            determine_load_color(opc);
            flush_inv_instmd_queue();
        }
        RETURN(1);
    }

    // protect against vanishing objects
    if (pickup && oinst[pickup] == NULL) {
        pickup = 0;
        lpickup = -1;
    }
    if (putdown && oinst[putdown] == NULL) {
        putdown = 0;
        lputdown = -1;
    }
      
    me->inv[z] = putdown;
    if (putdown) {
        mouse_obj_drop(putdown);
        obj_at(putdown, LOC_CHARACTER, opc+1, z, 0);

        if (z == gd.method_loc && (ppos+1) == gd.who_method)
            clear_method();
    }
    
    gd.mouseobj = 0;
    if (pickup) {
        mouse_obj_grab(pickup);
        
        if (z == gd.method_loc && (ppos+1) == gd.who_method)
            clear_method();
    }
        
    me->load = sum_load(opc);
    calculate_maxload(opc+1);
    
    // if we didn't return 1 up there the queue is still pending
    gd.queue_inv_rc--; // so remove both
    if (!gd.queue_inv_rc) {
        // calls to determine_load_color deleted because it does nothing now
        flush_inv_instmd_queue();
    }    
    
    // after_* events get handled in their own block
    gd.queue_inv_rc++;
    gd.lua_bool_hack = 1;
    lc_parm_int(invfunc, 6, opc+1, z, lpickup, lputdown, LHTRUE, LHTRUE);
    gd.queue_inv_rc--;
    if (!gd.queue_inv_rc) {
        flush_inv_instmd_queue();
    }  
        
    RETURN(0);
}

int on_click_func(unsigned int targ, unsigned int putdown, int x, int y) {
    int rv = 0;

    onstack("on_click_func");

    // a click counts as a trigger, so let's prevent funny business
    ++gd.queue_rc;
        
    if (member_begin_call(targ, "on_click")) {
        if (putdown)
            lua_pushinteger(LUA, putdown);
        else
            lua_pushnil(LUA);
            
        lua_pushinteger(LUA, x);
        lua_pushinteger(LUA, y);

        member_finish_call(3, 1);
        member_retrieve_stackparms(1, &rv);
    }
    --gd.queue_rc;
    if (!gd.queue_rc)
        flush_instmd_queue();
    
    RETURN(rv);
}

int zone_click(int opc, struct champion *me, int z,
    int who_look, int subrend)
{
    unsigned int putdown = gd.mouseobj;
    unsigned int pickup;
    int s = 1;
    
    onstack("zone_click");
    
    pickup = me->inv[z];
    
    s = inv_obj_exch(z, who_look, opc, putdown, pickup);

    // zone 0 always generates the subrenderer
    if (!s && z == 0 && subrend)
        gfxctl.do_subrend = 1;

    RETURN(s);
}

void character_taken(int lua_charid, int ppos, const char *s_message) {
    int ap;
    char *textmsg;
    int store_party_member;
    int real_level;
    
    lc_parm_int(s_message, 2, gd.who_look, lua_charid);
    
    inv_pickup_all(lua_charid);
    gd.tmp_champ = 0;
    inv_putdown_all(lua_charid);
    
    exit_inventory_view();
    exit_champion_preview(lua_charid, 1);
    
    ap = party_ist_peek(gd.a_party);
    
    real_level = gd.p_lev[ap];
    store_party_member = gd.party[ppos];
    gd.party[ppos] = 0;
    party_composition_changed(ap, PCC_MOVEOFF, real_level);
    gd.party[ppos] = store_party_member;
    
    if (gd.leader == -1)
        gd.leader = 0;
    party_composition_changed(ap, PCC_MOVEON, real_level);
    
    lc_parm_int("__TAKEFUNC", 0);
}

void inventory_clickzone(int z, int who_look, int subrend) {
    char pf[40];
    int opc;
    struct champion *me;
    
    onstack("inventory_clickzone");
    snprintf(pf, sizeof(pf), "%s%d, %d",
        (z & INVZMASK) ? "MASK:" : "z",
        (z & 0x0FFFFFFF), who_look);
    stackbackc('(');
    v_onstack(pf);
    addstackc(')');
        
    opc = (gd.party[who_look]-1);
    me = &(gd.champs[opc]);

    if (z & INVZMASK) {
        int rz = (z & 0x0FFFFFFF);
        zone_click(opc, me, rz, who_look, subrend);

        VOIDRETURN();
    }
    
    if (z == ZONE_EYE) {
        if (!gd.mouse_mode) {

            destroy_system_subrenderers();
            
            if (gd.mouseobj) {
                int game_mode = 0;
                int lookobj = gd.mouseobj;
                int simlookobj;
                
                if (viewstate == VIEWSTATE_CHAMPION)
                    game_mode = 1;
                simlookobj = call_member_func2(gd.mouseobj, "on_look", opc+1, game_mode);
                if (simlookobj)
                    lookobj = simlookobj;

                if (gd.mouseobj) {
                    gd.mouse_mode = MM_EYELOOK + lookobj;
                }

            } else
                gd.mouse_mode = MM_EYELOOK;
        }
            
        VOIDRETURN();    
    }
    
    if (z == ZONE_MOUTH) {

        if (gd.mouseobj)
            lc_parm_int("sys_click_mouth", 2, opc+1, gd.mouseobj);
            //call_member_func(gd.mouseobj, "on_consume", opc+1);
        else {
            if (!gd.mouse_mode) {
                gd.mouse_mode = MM_MOUTHLOOK;
            }
        }
        
        VOIDRETURN();
    }
            
    if (viewstate == VIEWSTATE_CHAMPION) {
        if (!gd.mouseobj) {
            int position = gd.offering_ppos;
            
            if (z == ZONE_RES) {       
                gd.offering_ppos = -1;
                character_taken(opc+1, position, "sys_character_resurrected");
                      
            } else if (z == ZONE_REI) {

                if (gd.gui_mode == GUI_NAME_ENTRY) {
                    gd.offering_ppos = -1;
                    character_taken(opc+1, position, "sys_character_reincarnated");
                    gd.gui_mode = 0;
                } else {
                    destroy_bitmap(gfxctl.i_subrend);
                    gfxctl.i_subrend = pcxload("REI_NAMEENTRY", NULL);
                    gd.gui_mode = GUI_NAME_ENTRY;
                    gd.curs_pos = 0;
                    memset(me->name, 0, 8);
                    memset(me->lastname, 0, 20);
                    dsb_clear_keybuf();
                }
            }
            
            if (z == ZONE_CANCEL || z == ZONE_EXITBOX) {
                int i = gd.who_look;
                exit_inventory_view();
                exit_champion_preview(opc+1, 0);
                gd.offering_ppos = -1;
                gd.party[i] = 0;
                remove_from_pos(gd.a_party, i+1);
            }
        }
        
        VOIDRETURN();
    }
    
    if (z == ZONE_ZZZ)
        go_to_sleep();
    
    if (z == ZONE_EXITBOX)
        exit_inventory_view();
        
    if (z == ZONE_DISK) {
        if (lc_parm_int("sys_forbid_save", 0)) {
            VOIDRETURN();
        }

        gd.gui_mode = GUI_WANT_SAVE;
        *gd.gl_viewstate = VIEWSTATE_GUI;
        freeze_sound_channels();
    }
           
    VOIDRETURN();
}

void got_gui_clickzone(int z) {

    onstack("got_gui_clickzone");
    
    if (gd.gui_down_button)
        VOIDRETURN();
    
    if (gd.gui_mode == GUI_WANT_SAVE) {
        switch(z) {
            case 1:
                interface_click();
                gd.gui_button_wait = 10;
                gd.gui_down_button = z;
                gd.gui_next_mode = 0;
                break;
                
            case 4:
                interface_click();
                gd.gui_button_wait = 10;
                gd.gui_down_button = z;
                gd.gui_next_mode = GUI_PICK_FILE;
                break;

            case 5:
                interface_click();
                gd.gui_button_wait = 10;
                gd.gui_down_button = z;
                gd.gui_next_mode = 255;
                break;
        }
        
    } else if (gd.gui_mode == GUI_PICK_FILE) {
        interface_click();
        gd.gui_button_wait = 10;
        gd.gui_down_button = z;
        gd.gui_next_mode = GUI_ENTER_SAVENAME;
        gd.active_szone = pick_inamei_zone(z);
        gd.active_czone = z;
        gd.curs_pos = 0;
        dsbfree(Backup_Iname);
        if (Iname[gd.active_szone]) {
            Backup_Iname = dsbstrdup(Iname[gd.active_szone]);
            dsbfree(Iname[gd.active_szone]);
        }
        Iname[gd.active_szone] = dsbcalloc(16, 1);
        if (check_for_savefile(CSSAVEFILE[gd.active_szone], NULL, Iname[gd.active_szone]))
            gd.curs_pos = strlen(Iname[gd.active_szone]);
        dsb_clear_keybuf();
    }
    
    
    VOIDRETURN();
}

void finish_savefile_save(void) {
    int a = gd.active_szone;
    char *sv_f;
    
    onstack("finish_savefile_save");
    
    sv_f = pick_savefile_zone(gd.active_czone);
    save_savefile(sv_f, Iname[a]);
    dsbfree(gd.reload_option);
    gd.reload_option = sv_f;
    
    dsbfree(Backup_Iname);
    Backup_Iname = NULL;
    
    lc_parm_int("sys_game_save", 0);
    
    unfreeze_sound_channels();
    gd.gui_mode = 0;
    *gd.gl_viewstate = VIEWSTATE_INVENTORY;
    
    VOIDRETURN();
}

int change_leader_to(int nleader) {
    int m_o;
    
    onstack("change_leader_to");
    
    m_o = gd.mouseobj;
     
    if (nleader == gd.offering_ppos) {
        RETURN(0);
    }
    
    if (PPis_here[nleader]) {
        // pass the mouse hand object between leaders
        if (m_o) mouse_obj_drop(gd.mouseobj);
        gd.leader = nleader;
        if (m_o) mouse_obj_grab(m_o);
        RETURN(1);
    } else {
        // cannot remotely pass, or pass in inventory
        if (!m_o && (gd.who_look == -1)) {
            int nparty = mparty_containing(nleader);
            gd.leader = nleader;
            change_partyview(nparty);
            RETURN(1);
        }
    }
    RETURN(0);
}

void control_clickzone(int z, int mx, int my) {
    char pf[4];
    
    onstack("control_clickzone");
    
    snprintf(pf, sizeof(pf), "%d", z);
    stackbackc('(');
    v_onstack(pf);
    addstackc(')');
    
    // namebar -- change leader
    // NO LONGER USED
    if (z >= 8 && z <= 11) {
        //change_leader_to(nleader);
    }
    
    if (*gd.gl_viewstate == VIEWSTATE_CHAMPION)
        VOIDRETURN();
        
    // a ppos zone
    if (z >= 0 && z <= 3) {
        int snr = z;    
        if (gfxctl.ppos_r[snr].flags & SRF_ACTIVE) {
            int lzr;  
            lzr = scan_clickzones(gfxctl.ppos_r[snr].cz, mx, my, gfxctl.ppos_r[snr].cz_n);           
            if (lzr != -1) {
                got_lua_clickzone(gfxctl.ppos_r[snr].cz, lzr, snr);
            }
        }               
    }
    
    // someone's right hand
    // NO LONGER USED
    if (z >= 4 && z <= 7) {
        //inventory_clickzone(gii.rslot[z-4] | INVZMASK, z-4, 0);
    }
    
    // inventory bars (screen 52-55)
    // NO LONGER USED
    if (z >= 12 && z <= 15) {
        //
    }
    
    // zones 16-33 are open (screen zones 56-73)

    // lua renderer controlled zones are 34-41 (screen = 74-81) 
    if (z >= 34 && z < 34+NUM_SR) {
        int zzid;
        int snr = z - 34;
    
        if (gfxctl.SR[snr].flags & SRF_ACTIVE) {
            int lzr;
            
            lzr = scan_clickzones(gfxctl.SR[snr].cz, mx, my, gfxctl.SR[snr].cz_n);
            
            if (debug) {
                fprintf(errorlog, "[LZ] Zone click at (%d, %d) zone %d has %d subzones, scan returned %d\n", mx, my,
                    snr, gfxctl.SR[snr].cz_n, lzr); 
            }
            
            if (lzr != -1) {
                got_lua_clickzone(gfxctl.SR[snr].cz, lzr, gd.leader);
            }
        }
    }
        
    VOIDRETURN();
}

void mouse_obj_throw(int side) {
    unsigned int t_obj;
    
    onstack("mouse_obj_throw");
    
    t_obj = gd.mouseobj;
    
    if (UNLIKELY(gd.party[gd.leader] < 1)) {
        puke_bad_subscript("gd.party", gd.party[gd.leader]);
        VOIDRETURN();
    }
    
    // call event and protect against crash
    if (call_member_func2(t_obj, "on_throw", -1, gd.party[gd.leader]))
        VOIDRETURN();
    if (!oinst[t_obj])
        VOIDRETURN();
    if (gd.leader == -1)
        VOIDRETURN();
        
    lc_parm_int("sys_mouse_throw", 4, t_obj, gd.leader, 
        gd.party[gd.leader], side);
    
    VOIDRETURN();
}

void objinst_click(int z, int abs_x, int abs_y) {
    int putdown = gd.mouseobj;
    int pickup = 0;
    unsigned int targ_inst = 0;
    int cz11;
    int wallclick = 0;
    int wall_drop = 0;
    int monsterclick = 0;
    int rx, ry;
    int dz_rel;
    
    onstack("objinst_click");
    
    // calculate relative click
    rx = abs_x - cz[z].x;
    ry = abs_y - cz[z].y;
    
    if (z >= 38) {
        if (cz[z].inst) {           
            int type_id = (cz[z].inst & 7);
            if (type_id & 4)
                type_id = 4;
            else if (type_id & 2)
                type_id = 2;
            else
                type_id = 1;
            
            lc_parm_int("sys_wall_knock", 1, type_id);
            
        } else {
            if (putdown)
                mouse_obj_throw(z - 38);
        }
                    
        VOIDRETURN();
    }
    
    // a click with an object transfers to the underlying dropzone
    if (putdown)
        if (z < 10) z += 10;

    // deal with dropzones and wallitems differently
    if (z == 1 || z == 11) {
        cz11 = cz[11].inst;
        wallclick = 1;
        wall_drop = 1;
    } else if (z >= 19 && z < 38) {
        cz11 = cz[z].inst;
        wallclick = 1;
    }

    // wall click object
    if (wallclick) {
        struct obj_arch *p_arch;
        
        // check here to make sure there is even an inst
        // (something might have yoinked it out from under us)
        if (oinst[cz11] == NULL)    
            goto ZONERETURN;
        
        p_arch = Arch(oinst[cz11]->arch);
        
        if (gd.leader == -1) {  
            if (!queryluabool(p_arch, "no_party_clickable"))
                VOIDRETURN();
        }
        
        if (gd.gl->lockc[LOCK_ACTUATORS])
            goto ZONERETURN;
        
        if (!(p_arch->arch_flags & ARFLAG_DZ_EXCHONLY)) {
            // one-item mode means everything else on the wall gets a click too
            int one_item = !!(gd.gameplay_flags & GP_ONE_WALLITEM);
        
            // queue up any destroyed opbys. it's a lot less messy this way...
            if (one_item) {
                gd.always_queue_inst = putdown;
                gd.queue_rc++;
            }
            
            on_click_func(cz11, putdown, rx, ry);
                     
            if (one_item) {
                struct inst *p_inst = oinst[cz11];
                struct inst_loc *c_dt_il = dun[p_inst->level].t[p_inst->y][p_inst->x].il[p_inst->tile];
                
                while (c_dt_il != NULL) {
                    int oid = c_dt_il->i;
                    if (oid != cz11) {
                        // my object seems to have disappeared
                        if (putdown && !oinst[putdown]) {
                            putdown = 0;
                        }
                            
                        if (!(oinst[oid]->gfxflags & OF_INACTIVE)) {
                            on_click_func(oid, putdown, rx, ry);
                        }
                    }
                    c_dt_il = c_dt_il->n;
                }
                
                gd.queue_rc--;
                gd.always_queue_inst = 0;
                if (!gd.queue_rc)
                    flush_instmd_queue();
            }
        }
        
        // nothing happens if the object is gone
        if (oinst[cz11] == NULL)    
            goto ZONERETURN;
    }
    
        
    // this is as far as the ghost goes
    if (gd.leader == -1)
        VOIDRETURN();
        
    if (z >= 19) {
        VOIDRETURN();
        
    } else if (z >= 16 && z <= 18) {
        int targ = cz[z].inst;

        monsterclick = on_click_func(targ, putdown, rx, ry);
        
        if (!monsterclick) {
            if (putdown && gd.click38 != -1) {
                mouse_obj_throw(gd.click38 - 38);
                gd.click38 = -1;
                goto ZONERETURN;
            }
        }

    } else if (z > 10) {
        int dx, dy;
        struct obj_arch *down_arch;
        
        // drop zone clicks
        if (!putdown)
            goto ZONERETURN;
            
        down_arch = Arch(oinst[putdown]->arch);
            
        face2delta(gd.p_face[gd.a_party], &dx, &dy);
        
        // wall drop zone
        if (z == 11) {
            struct obj_arch *p_arch = Arch(oinst[cz11]->arch);
            int take = p_arch->arch_flags & ARFLAG_DROP_ZONE;
            int rej;
            
            if (!take)
                VOIDRETURN();
                
            // no putting fireballs in alcoves (?!)
            if (down_arch->arch_flags & ARFLAG_FLY_ONLY)
                VOIDRETURN();

            rej = call_member_func(putdown, "on_drop", -1);
            if (rej)
                VOIDRETURN();
                
            rej = call_member_func(cz11, "on_zone_drop", putdown);
            if (rej)
                VOIDRETURN();

            if (p_arch->arch_flags & ARFLAG_DZ_EXCHONLY) {
                on_click_func(cz11, putdown, rx, ry);
            }
            
            if (oinst[putdown]) {
                int ap = gd.a_party;
                
                p_to_tile(gd.a_party, T_LOCATION_DROP, putdown);
                if (oinst[putdown]) {
                    mouse_obj_drop(putdown);
                    place_instance(putdown, gd.p_lev[ap],
                        gd.p_x[ap]+dx, gd.p_y[ap]+dy,
                        (gd.p_face[ap]+2)%4, 0);
                }
            }

        // floor drop zone
        } else {
            int ap = gd.a_party;
            int rej;

            int lx = gd.p_x[ap] + ((z>13)? dx : 0);
            int ly = gd.p_y[ap] + ((z>13)? dy : 0);
            int lf = (gd.p_face[ap] + (z-12)) % 4;
            
            rej = call_member_func(putdown, "on_drop", -1);
            if (rej)
                VOIDRETURN();
            
            p_to_tile(gd.a_party, T_LOCATION_DROP, putdown);
            if (oinst[putdown]) {
                mouse_obj_drop(putdown);
                place_instance(putdown, gd.p_lev[ap], lx, ly, lf, 0);
            }
            
            if (oinst[putdown]) {
                if (down_arch->arch_flags & ARFLAG_FLY_ONLY) {
                    if (!call_member_func(putdown, "on_impact", -1)) {
                        if (oinst[putdown]) {
                            inst_destroy(putdown, 0);
                            VOIDRETURN();
                        }
                    }
                }
            }
        }

    } else {
        // pickup zone clicks
        int v = 0;
        struct obj_arch *p_arch;
        
        pickup = cz[z].inst;
        v = on_click_func(pickup, 0, rx, ry);
        
        if (v || oinst[pickup] == NULL)
            goto ZONERETURN;
            
        p_arch = Arch(oinst[pickup]->arch);
        if (p_arch->type != OBJTYPE_THING) {
            goto ZONERETURN;
        }
            
        if (wall_drop) {
            struct obj_arch *p_w_arch = Arch(oinst[cz11]->arch);
            if (p_w_arch->arch_flags & ARFLAG_DZ_EXCHONLY) {
                on_click_func(cz11, pickup, rx, ry);
            }
        }
        
        p_to_tile(gd.a_party, T_LOCATION_PICKUP, pickup);
        if (oinst[pickup]) {
            limbo_instance(pickup);
            mouse_obj_grab(pickup);
        }
        
        // put the screen into softmode to render the alpha blended
        // icon of a newly picked-upped object
        if (oinst[pickup]) {

            p_arch = Arch(oinst[pickup]->arch);
        
            if (p_arch->icon && p_arch->icon->flags & AM_HASALPHA)
                gd.softframecnt = 2;
            else if (p_arch->alt_icon &&
                p_arch->alt_icon->flags & AM_HASALPHA)
            {
                gd.softframecnt = 2;
            }
        }
    }
    
    ZONERETURN:
    gd.need_cz = 1;   
    VOIDRETURN();
}

int scan_clickzones(struct clickzone *cz_a, int xx, int yy, int max) {
    int i;
    
    for(i=0;i<max;++i) {
        if (cz_a[i].w) {
            if (xx > cz_a[i].x && yy > cz_a[i].y) {
                if (xx < cz_a[i].x+cz_a[i].w && yy < cz_a[i].y+cz_a[i].h) {
                            
                    return i;   
                           
                }
            }
        }
    }
    
    return -1;
}

int scan_inventory_click(int xx, int yy) {
    int i;
    int szx = 32;
    int szy = 32;
    
    for (i=0;i<gii.max_invslots;++i) {
        if (xx > gii.inv[i].x && yy > gii.inv[i].y) {
            if (xx < gii.inv[i].x+szx && yy < gii.inv[i].y+szy) {

                return i;

            }
        }
    }
    
    return -1;
}

int b_scan_clickzones(int base, struct clickzone *cz_a, int xx, int yy, int max) {
    int i;

    for(i=base;i<max;++i) {
        if (cz_a[i].w) {
            if (xx > cz_a[i].x && yy > cz_a[i].y) {
                if (xx < cz_a[i].x+cz_a[i].w && yy < cz_a[i].y+cz_a[i].h) {

                    return i;

                }
            }
        }
    }

    return -1;
}

void got_mkeypress(int type, int key) {
    onstack("got_mkeypress");
    
    if (type == PRESS_MAGICKEY) {
        if (gfxctl.SR[SR_MAGIC].flags & SRF_ACTIVE) {
            if (gfxctl.SR[SR_MAGIC].cz && gfxctl.SR[SR_MAGIC].cz[key].w) {
                got_lua_clickzone(gfxctl.SR[SR_MAGIC].cz, key, gd.whose_spell);
            }
        }
    } else if (type == PRESS_METHODKEY) {
        if (gfxctl.SR[SR_METHODS].flags & SRF_ACTIVE) {
            if (gfxctl.SR[SR_METHODS].cz && gfxctl.SR[SR_METHODS].cz[key].w) {
                got_lua_clickzone(gfxctl.SR[SR_METHODS].cz, key, (gd.who_method-1));
            }
        }
    } else if (type == PRESS_INTERFACEKEY) {
        if (key == KD_LEADER_CYCLE || key == KD_LEADER_CYCLE_BACK) {
            if (gd.leader != -1) {
                int cleader = gd.leader;
                int delta = 1;
                if (key == KD_LEADER_CYCLE_BACK)
                    delta = 3;
                do {
                    cleader = (cleader + delta) % 4;
                    if (gd.party[cleader]) {
                        if (change_leader_to(cleader))
                            break;
                    }
                } while (cleader != gd.leader);
            }
        } else if (key == KD_ROWSWAP || key == KD_SIDESWAP) {
            if (gd.leader != -1) {
                int ap = gd.a_party;
                int swap, ox, oy;
                int nx, ny;
                int mg;
                gd.mouse_guy = gd.leader + 1;
                mg = gd.mouse_guy;
                find_remove_from_pos(ap, gd.mouse_guy, &ox, &oy);
                if (key == KD_ROWSWAP) {
                    ny = !oy;
                    nx = ox;
                } else {
                    ny = oy;
                    nx = !ox;
                }
                swap = gd.guypos[ny][nx][ap];
                gd.guypos[oy][ox][ap] = swap;
                gd.guypos[ny][nx][ap] = gd.mouse_guy;
                gd.mouse_guy = 0;
                exec_lua_rearrange(mg, ox, oy, nx, ny);
                ppos_reassignment(ap, swap, mg, ox, oy, nx, ny);
            }  
        } else if (key == KD_GOSLEEP || key == KD_QUICKSAVE) {
            if (gd.leader != -1) {
                int WL = gd.who_look;
                if (WL == -1)
                    enter_inventory_view(gd.leader);
                if (key == KD_QUICKSAVE) {
                    if (lc_parm_int("sys_forbid_save", 0)) {
                        console_system_message("SAVING IS CURRENTLY NOT ALLOWED.", makecol(255, 255, 255));
                    } else {
                        int chosen_file = -1;
                        int failed = 0;
                        char iname[80];
                        if (!gd.reload_option) {
                            int i_sv;
                            for(i_sv=0;i_sv<5;++i_sv) {
                                if (i_sv == 4) {
                                    failed = 1;
                                    console_system_message("ALL SAVE SLOTS ARE FULL. PLEASE SAVE MANUALLY.", makecol(255, 255, 255));
                                    break;
                                }
                                if (!check_for_savefile(CSSAVEFILE[i_sv], NULL, iname)) {
                                    gd.reload_option = dsbstrdup(CSSAVEFILE[i_sv]);
                                    chosen_file = i_sv;
                                    break;
                                }
                            }
                        }
                        if (!failed) {
                            if (!check_for_savefile(gd.reload_option, NULL, iname)) {
                                sprintf(iname, "QUICKSAVE");
                                Iname[chosen_file] = dsbstrdup(iname);
                            }
                            freeze_sound_channels();
                            save_savefile(gd.reload_option, iname);                        
                            lc_parm_int("sys_game_save", 0);
                            sprintf(iname, "GAME SAVED TO %s%s", gd.reload_option, ".");
                            console_system_message(iname, makecol(255, 255, 255));  
                            unfreeze_sound_channels();                          
                        }
                    }
                } else if (key == KD_GOSLEEP) {
                    int fs = lc_parm_int("sys_forbid_sleep", 0);
                    if (fs) {
                        console_system_message("SLEEPING IS CURRENTLY NOT ALLOWED.", makecol(255, 255, 255));
                    } else {
                        if (*gd.gl_viewstate == VIEWSTATE_INVENTORY) {
                            exit_inventory_view();
                            WL = 0;
                        }        
                        *gd.gl_viewstate = VIEWSTATE_SLEEPING;
                        lc_parm_int("sys_sleep", 0);
                    }
                }    
                               
                if (WL == -1)
                    exit_inventory_view();                
            }    
        }   
    } else if (type == PRESS_INVKEY) {
        int oppos = -1;
        if (key == 4 || key == 5) {
            int diff = 1;
            if (key == 5)
                diff = 3;
                
            if (gd.who_look >= 0) {
                int tries = 0;
                int next = gd.who_look;
                while (tries < 4) {
                    next = (next + diff) % 4;
                    tries++;
                    if (next != gd.who_look &&
                        gd.party[next] && PPis_here[next])
                    {
                        oppos = next;
                        break;
                    }
                }
            } else
                oppos = gd.leader;
        } else
            oppos = key;
            
        if (oppos >= 0) {
           if (viewstate <= VIEWSTATE_INVENTORY) {
                if (gd.party[oppos] && PPis_here[oppos]) {
                    int WL = gd.who_look;
                    if (WL != -1)
                        exit_inventory_view();
                    if (oppos != WL)
                        enter_inventory_view(oppos);
                }  
            }
        }    
    }
    
    VOIDRETURN();    
}

// the params for this function are guyicons style STORED ppos 
// (0 = blank, 1 = ppos 0, etc.)
void exec_lua_rearrange(int mg, int ox, int oy, int nx, int ny) {
    int oldp, newp;
    
    onstack("exec_lua_rearrange");
            
    oldp = 3*oy + ox;
    newp = 3*ny + nx;
    if (oldp == 4) oldp = 2;
    if (newp == 4) newp = 2;
    
    lc_parm_int("sys_party_rearrange", 3, mg - 1, oldp, newp);  
        
    VOIDRETURN();
}

// the params for this function are guyicons style STORED ppos 
// (0 = blank, 1 = ppos 0, etc.)
void ppos_reassignment(int ap, int oppos, int nppos, int ox, int oy, int nx, int ny) {
    int oldp, newp;
    int swap;
    char runeswap[8];
    
    onstack("ppos_reassignment");

    /*
    // this code mostly works
    // but it does bad things on the inventory screen
    // maybe eventually i'll fix it?
    //
    oldp = (oy*2) + (ox%2);
    newp = (ny*2) + (nx%2);
    
    if (gd.guypos[oy][ox][ap])
        gd.guypos[oy][ox][ap] = oldp + 1;
      
    if (gd.guypos[ny][nx][ap])
        gd.guypos[ny][nx][ap] = newp + 1;
        
    swap = gd.party[oldp];
    gd.party[oldp] = gd.party[newp];
    gd.party[newp] = swap;
    
    swap = gd.idle_t[oldp];
    gd.idle_t[oldp] = gd.idle_t[newp];
    gd.idle_t[newp] = swap;
    
    memcpy(runeswap, gd.i_spell[oldp], 8);
    memcpy(gd.i_spell[oldp], gd.i_spell[newp], 8);
    memcpy(gd.i_spell[newp], runeswap, 8);
    
    if (gd.leader == oldp)
        gd.leader = newp; 
    else if (gd.leader == newp)
        gd.leader = oldp;
        
    if (gd.whose_spell == oldp)
        gd.whose_spell = newp;
    else if (gd.whose_spell == newp)
        gd.whose_spell = oldp;
    */
    
    VOIDRETURN();
}

void got_mouseclick(int b, int xx, int yy, int viewstate) {
    int zr = -1;
    int gzx, gzy;
    int guyzone = 0;
    int ap = gd.a_party;
        
    onstack("got_mouseclick");
    
    // clicking on the guys in the corner
    if ((mouse_x > gfxctl.guy_x) && (mouse_x < gfxctl.guy_x + 80)
        && (mouse_y > gfxctl.guy_y && mouse_y < gfxctl.guy_y + 60)) 
    {
        gzx = (mouse_x-gfxctl.guy_x)/40;
        gzy = (mouse_y-gfxctl.guy_y)/30;
        
        if (gzx > 1) gzx = 1;
        if (gzy > 1) gzy = 1;
        guyzone = 1;
    }
    
    if (viewstate >= VIEWSTATE_FROZEN)
        guyzone = 0;
        
    // middle click 
    if (b == 2) {
        if (viewstate == VIEWSTATE_DUNGEON) {
            int mouse_obj = gd.mouseobj;
            
            if (!mouse_obj) {
                mouse_obj = -1;
            }\
            
            lc_parm_int("sys_put_away_click", 1, mouse_obj); 
        }
            
        VOIDRETURN();   
    }
    
    // right clicks
    if (b) {
        int ppos;
        int scanned = 0;
                
        if (guyzone && !gd.mouse_guy) {
            int dirmod = 0;
            int i_actguy = gd.guypos[gzy][gzx][ap];
            if (i_actguy) {
                --i_actguy;
                
                if (((mouse_x - gfxctl.guy_x)/20) % 2 == 0)
                    dirmod = 2;
                    
                gd.g_facing[i_actguy] += (1 + dirmod);
                gd.g_facing[i_actguy] %= 4;
            }

            VOIDRETURN();
        }
        
        if (viewstate == VIEWSTATE_GUI) {
            if (gd.gui_mode == GUI_PICK_FILE ||
                gd.gui_mode == GUI_ENTER_SAVENAME)
            {
                if (gd.gui_mode == GUI_ENTER_SAVENAME) {
                    Iname[gd.active_szone] = Backup_Iname;
                    Backup_Iname = NULL;
                }
                
                interface_click();
                gd.gui_button_wait = 1;
                gd.gui_next_mode = 0;
                
            }
            VOIDRETURN();    
        }
        
        if (*gd.gl_viewstate <= VIEWSTATE_INVENTORY) {
            for(ppos=0;ppos<4;++ppos) {
                if (gfxctl.ppos_r[ppos].flags & SRF_ACTIVE) {
                    int iscanx = gfxctl.ppos_r[ppos].rtx;
                    int iscany = gfxctl.ppos_r[ppos].rty;
                    int sx = gfxctl.ppos_r[ppos].sizex;
                    int sy = gfxctl.ppos_r[ppos].sizey;

                    if (xx >= iscanx && yy >= iscany) {
                        if ((xx <= (iscanx + sx)) && (yy <= (iscany + sy))) {
                            int sel_c = ppos;
                            if (gd.party[sel_c] && PPis_here[sel_c]) {
                                int WL = gd.who_look;
                                if (WL != -1)
                                    exit_inventory_view();
                                if (sel_c != WL)
                                    enter_inventory_view(sel_c);
                                scanned = 1;
                                break;
                            }   
                            scanned = 1;
                        }
                    }
                }
            }
        }
        
        if (!scanned) {
            if (*gd.gl_viewstate == VIEWSTATE_INVENTORY)
                exit_inventory_view();
            else if (*gd.gl_viewstate == VIEWSTATE_DUNGEON) {
                if (gd.leader != -1)
                    enter_inventory_view(gd.leader);
            }
        }            
        
        /*
        } else {
            if (*gd.gl_viewstate <= VIEWSTATE_INVENTORY) {
                if (xx < 552) {
                    int sel_c = (xx/138);
                    
                    if (gd.party[sel_c] && PPis_here[sel_c]) {
                        int WL = gd.who_look;
                        if (WL != -1)
                            exit_inventory_view();
                        if (sel_c != WL)
                            enter_inventory_view(xx/138);
                    }  
                }
            }
        }
        */
        
        VOIDRETURN();    
    }
    
    if (guyzone) {
        if (!gd.mouse_guy) {
            gd.mouse_guy = gd.guypos[gzy][gzx][ap];
            if (gd.mouse_guy)
                gd.g_facing[gd.mouse_guy - 1] = 0;
        } else {
            int swap, ox, oy;
            int mg = gd.mouse_guy;
            find_remove_from_pos(ap, gd.mouse_guy, &ox, &oy);
            swap = gd.guypos[gzy][gzx][ap];
            gd.guypos[oy][ox][ap] = swap;
            gd.guypos[gzy][gzx][ap] = gd.mouse_guy;
            gd.mouse_guy = 0;
            exec_lua_rearrange(mg, ox, oy, gzx, gzy);
            ppos_reassignment(ap, swap, mg, ox, oy, gzx, gzy);
            
        }
      
        VOIDRETURN();
    }
        
    if (viewstate == VIEWSTATE_DUNGEON) {
        gd.click38 = b_scan_clickzones(38, cz, xx, yy, 40);
        zr = scan_clickzones(cz, xx, yy, 18);
        if (zr != -1)
            objinst_click(zr, xx, yy);
        else {
            int noclick = 1;
            int lv_zr = 18;
            
            while (lv_zr < 38) {
                lv_zr = b_scan_clickzones(lv_zr, cz, xx, yy, 38);
                if (lv_zr == -1)
                    break;
                noclick = 0;
                objinst_click(lv_zr, xx, yy);
                ++lv_zr;
            }
            
            if (noclick) {
                zr = gd.click38;
                gd.click38 = -1;
                if (zr != -1)
                    objinst_click(zr, xx, yy);
            }
        }
        
    } else if (viewstate == VIEWSTATE_INVENTORY ||
        viewstate == VIEWSTATE_CHAMPION)
    {
        if (lua_cz_n) {
            zr = scan_clickzones(lua_cz, xx, yy, lua_cz_n);
            if (zr != -1)
                got_lua_clickzone(lua_cz, zr, gd.who_look);
        }
        
        if (zr == -1) {
            zr = scan_inventory_click(xx - gd.vxo, yy - (gd.vyo + gd.vyo_off));
            if (zr != -1) {
                inventory_clickzone(zr | INVZMASK, gd.who_look, 1);
            } else {
                zr = scan_clickzones(cz, xx, yy, 40);
                if (zr != -1)
                    inventory_clickzone(zr, gd.who_look, 1);
            }
        }
    
    } else if (viewstate == VIEWSTATE_GUI) {
        zr = scan_clickzones(cz, xx, yy, 10);
        if (zr != -1)
            got_gui_clickzone(zr);
            
        VOIDRETURN();
    }
    
    zr = scan_clickzones(cz+40, xx, yy, 40);
    if (zr != -1)
        control_clickzone(zr, xx, yy);
    
    VOIDRETURN();
}

int pick_inamei_zone(int z) {
    switch(z) {
        case 3: return 1;
        case 4: return 2;
        case 5: return 3;
        default: return 0;
    }
}

char *pick_savefile_zone(int z) {
    int i = pick_inamei_zone(z);
    return(dsbstrdup(CSSAVEFILE[i]));
}

void DSBkeyboard_name_input(void) {
    int i_fullkey = 0;
    unsigned char c;
    unsigned int code;
    int send_flag = 0;
    struct champion *me;
    int who_look;
    
    if (!keypressed())
        return;
        
    onstack("DSBkeyboard_name_input");
        
    i_fullkey = readkey();

    c = (i_fullkey & 0xFF);
	code = (i_fullkey & 0xFF00) >> 8;
	
	who_look = gd.who_look;
	me = &(gd.champs[gd.party[who_look] - 1]);

	if (code == KEY_BACKSPACE) {

		if (gd.curs_pos > 0) {
            int pl;
            
			--gd.curs_pos;
            pl = gd.curs_pos;
            
            if (pl < 7)
                me->name[pl] = 0;
            else
                me->lastname[pl-7] = 0;
		}

	} else if (code == KEY_ENTER) {

		if (gd.curs_pos < 7)
            gd.curs_pos = 7;

	} else if (c >= 32 && c <= 122) {
        int pl = gd.curs_pos;
        
        // toupper
        if (c >= 97) c -= 32;

        if (pl < 7) {
            me->name[pl] = c;
            gd.curs_pos++;
        } else if (pl < 26) {
            me->lastname[pl-7] = c;
            gd.curs_pos++;
        }
	}
	
	dsb_clear_keybuf();
	
	VOIDRETURN();
}

void DSBkeyboard_gamesave_input(void) {
    int i_fullkey = 0;
    unsigned char c;
    unsigned int code;
    int z = gd.active_szone;

    if (!keypressed())
        return;

    onstack("DSBkeyboard_gamesave_input");

    i_fullkey = readkey();

    c = (i_fullkey & 0xFF);
	code = (i_fullkey & 0xFF00) >> 8;
	
	if (code == KEY_BACKSPACE) {

		if (gd.curs_pos > 0) {
            int pl;

			--gd.curs_pos;
            pl = gd.curs_pos;

            Iname[z][pl] = 0;
		}

	} else if (code == KEY_ENTER) {

        finish_savefile_save();

	} else if (c >= 32 && c <= 122) {
        int pl = gd.curs_pos;

        if (gd.curs_pos >= 12) {
            VOIDRETURN();
        }

        // toupper
        if (c >= 97) c -= 32;

        Iname[z][pl] = c;
        gd.curs_pos++;

	}

	dsb_clear_keybuf();

	VOIDRETURN();
}

