#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <fmod.h>
#include "objects.h"
#include "lua_objects.h"
#include "esb_lua_support.h"
#include "uproto.h"
#include "lproto.h"
#include "champions.h"
#include "gproto.h"
#include "timer_events.h"
#include "global_data.h"
#include "editor_hwndptr.h"
#include "editor_gui.h"
#include "sound.h"
#include "sound_fmod.h"
#include "monster.h"
#include "arch.h"
#include "editor_shared.h"
#include "spawnburst.h"
#include "fullscreen.h"
#include "istack.h"
#include "mparty.h"
#include "viewscale.h"
#include "common_macros.h"
#include "common_lua.h"

extern HWND sys_hwnd;

int il_esb_register_object(lua_State *LUA);
int expl_esb_add_champion(lua_State *LUA);
int expl_esb_use_wallset(lua_State *LUA);
int expl_esb_alt_wallset(lua_State *LUA);
int expl_esb_champion_toparty(lua_State *LUA);
int il_esb_l_tag(lua_State *LUA);

extern const char *funcnames[];
extern struct global_data gd;
extern struct editor_global edg;

lua_State *LUA;

int esbl_nilreturner(lua_State *LUA) {
    lua_pushnil(LUA);
    return 1;
}

int esbl_ntablereturner(lua_State *LUA) {
    lua_newtable(LUA);
    return 1;
}

int esbl_makecol(lua_State *LUA) {
    if (lua_istable(LUA, -1)) {
        int r, g, b;
        lua_pushinteger(LUA, 1);
        lua_gettable(LUA, -2);
        r = lua_tointeger(LUA, -1);
        lua_pop(LUA, 1);
        
        lua_pushinteger(LUA, 2);
        lua_gettable(LUA, -2);
        g = lua_tointeger(LUA, -1);
        lua_pop(LUA, 1);
        
        lua_pushinteger(LUA, 3);
        lua_gettable(LUA, -2);
        b = lua_tointeger(LUA, -1);
        lua_pop(LUA, 1);
        
        lua_pushinteger(LUA, makecol(r, g, b));
    } else {
        lua_pushinteger(LUA, 0);
    }
    return 1;
}

int esbl_bitflag(lua_State *LUA) {
    int v, f;
    v = luaint(LUA, -2, "esb_bitflag", 1);
    f = luaint(LUA, -1, "esb_bitflag", 2);
    if (v & f)
        lua_pushboolean(LUA, 1);
    else
        lua_pushboolean(LUA, 0);
    return 1;    
}

int esbl_information(lua_State *LUA) {
    const char *s;
    luaonstack("esb_information");
    
    s = luastring(LUA, -1, "esb_information", 1);
    MessageBox(sys_hwnd, s, "Editor Information", MB_ICONINFORMATION);

    RETURN(0);    
}

int expl_esb_replace_method(lua_State *LUA) {
    char *mname;
    int who;
    struct att_method *repmethod;
    
    INITPARMCHECK(2, funcnames[DSB_REPLACE_METHODS]);
    
    who = luaint(LUA, -2, funcnames[DSB_REPLACE_METHODS], 1);
    mname = (char *)luastring(LUA, -1, funcnames[DSB_REPLACE_METHODS], 2);
    validate_champion(who);

    gd.champs[who-1].method_name = dsbstrdup(mname);
    
    RETURN(0);
}

static const struct luaL_reg ESB_Funcs [] = {
    { "dsb_valid_inst", expl_valid_inst },
    { "dsb_import_arch", expl_import_arch },
    { "dsb_add_champion", expl_esb_add_champion },
    { "dsb_champion_toparty", expl_esb_champion_toparty },
    { "dsb_spawn", expl_spawn },
    { "dsb_fetch", expl_fetch },
    { "dsb_level_tint", expl_level_tint },
    { "dsb_level_flags", expl_level_flags },
    { "dsb_image2map", expl_image2map },
    { "dsb_text2map", expl_text2map },
    { "dsb_level_wallset", expl_esb_use_wallset },
    { "dsb_party_place", expl_party_place },
    { "dsb_spawnburst_begin", expl_spawnburst_begin },
    { "dsb_spawnburst_end", expl_spawnburst_end },
    { "dsb_set_alt_wallset", expl_esb_alt_wallset },
    { "dsb_get_exviewinst", expl_get_exviewinst },
    { "dsb_set_exviewinst", expl_set_exviewinst },
    { "dsb_get_hp", expl_get_hp },
    { "dsb_set_hp", expl_set_hp },
    { "dsb_get_maxhp", expl_get_maxhp },
    { "dsb_set_maxhp", expl_set_maxhp },
    { "dsb_get_animtimer", expl_get_animtimer },
    { "dsb_set_animtimer", expl_set_animtimer },
    { "dsb_enable", expl_enable },
    { "dsb_disable", expl_disable },
    { "dsb_toggle", expl_toggle },
    { "dsb_msg_chain", expl_msg_chain },
    { "dsb_set_gameflag", expl_set_gameflag },
    { "dsb_clear_gameflag", expl_clear_gameflag },
    { "dsb_toggle_gameflag", expl_toggle_gameflag },
    { "dsb_set_mpartyflag", expl_set_mflag },
    { "dsb_clear_mpartyflag", expl_clear_mflag },
    { "dsb_toggle_mpartyflag", expl_toggle_mflag },
    { "dsb_get_gfxflag", expl_get_gfxflag },
    { "dsb_set_gfxflag", expl_set_gfxflag },
    { "dsb_clear_gfxflag", expl_clear_gfxflag },
    { "dsb_toggle_gfxflag", expl_toggle_gfxflag },
    { "dsb_get_xp_multiplier", expl_get_xp_multiplier },
    { "dsb_rand", expl_rand },
    { "dsb_rand_seed", expl_rand_seed },
    { "dsb_find_arch", expl_find_arch },
    { "dsb_set_charge", expl_set_charge },
    { "dsb_get_coords", expl_get_coords },
    { "dsb_move", expl_move },
    { "dsb_tileptr_rotate", expl_tileptr_rotate },
    { "dsb_qswap", expl_qswap },
    { "dsb_get_cell", expl_get_cell },
    { "dsb_set_cell", expl_set_cell },
    { "dsb_get_facedir", expl_get_facedir },
    { "dsb_set_facedir", expl_set_facedir },
    { "dsb_get_bitmap", esbl_ntablereturner },
    { "dsb_get_font", esbl_ntablereturner },
    { "dsb_animate", esbl_nilreturner },
    { "dsb_get_mask_bitmap", esbl_ntablereturner },
    { "dsb_clone_bitmap", esbl_ntablereturner },
    { "dsb_get_sound", esbl_ntablereturner },
    { "dsb_make_wallset", esbl_nilreturner },
    { "dsb_make_wallset_ext", esbl_nilreturner },
    { "dsb_add_condition", esbl_ntablereturner },
    { "dsb_replace_condition", esbl_ntablereturner },
    { "dsb_export", esbl_nilreturner },
    { "dsb_delay_func", esbl_nilreturner },
    { "dsb_get_crop", expl_get_crop },
    { "dsb_set_crop", expl_set_crop },
    { "dsb_delete", expl_delete },
    { "dsb_replace_methods", expl_esb_replace_method },
    { "dsb_textconvert_dmstring", expl_dmtextconvert },
    { "dsb_textconvert_numbers", expl_numbertextconvert },
    { "dsb_forward", expl_forward },
    { "dsb_tileshift", expl_tileshift },
    { "dsb_linesplit", expl_linesplit },
    { "dsb_get_xp", expl_get_xp },
    { "dsb_set_xp", expl_set_xp },
    { "__trt_iassign", il_trt_iassign },
    { "__trt_purge", il_trt_purge },
    { "__trt_obj_writeback", il_trt_obj_writeback },
    { "__regobject", il_esb_register_object },
    { "__obj_newidxfunc", il_obj_newidxfunc },
    { "__log", il_log },
    { "__flush", il_flush },
    { "__nextinst", il_nextinst },
    { "__l_tag", il_esb_l_tag },
    { "esb_makecol", esbl_makecol },
    { "esb_bitflag", esbl_bitflag },
    { "esb_information", esbl_information },
    { NULL, NULL }
};

int il_esb_l_tag(lua_State *LUA) {
    const char *str;
    str = luastring(LUA, -1, funcnames[_DSB_L_TAG], 1);
    ed_tag_l(str);
    return 0;
}

int il_esb_register_object(lua_State *LUA) {
    char *objname = "register";
    
    luaonstack(objname);
    only_when_objparsing(LUA, objname);
    register_object(LUA, 0);
    
    RETURN(0);
}

int expl_esb_use_wallset(lua_State *LUA) {
    const char *ws_name;
    int lev_on;
    
    INITPARMCHECK(2, funcnames[DSB_USE_WALLSET]);

    lev_on = luaint(LUA, -2, funcnames[DSB_USE_WALLSET], 1);
    ws_name = luastring(LUA, -1, funcnames[DSB_USE_WALLSET], 2);
    
    if (lev_on < 0 || lev_on >= gd.dungeon_levels)
        DSBLerror(LUA, "%s: Invalid level", funcnames[DSB_USE_WALLSET]);
        
    esb_set_level_wallset(LUA, lev_on, ws_name);
        
    RETURN(0);
}

int expl_esb_alt_wallset(lua_State *LUA) {
    int lev, xx, yy, dir;
    int ws;
    const char *ws_name;
    
    INITPARMCHECK(5, funcnames[DSB_ALT_WALLSET]);
    
    lev = luaint(LUA, -4, funcnames[DSB_ALT_WALLSET], 2);
    xx = luaint(LUA, -3, funcnames[DSB_ALT_WALLSET], 3);
    yy = luaint(LUA, -2, funcnames[DSB_ALT_WALLSET], 4);
    dir = luaint(LUA, -1, funcnames[DSB_ALT_WALLSET], 5);
    lua_pop(LUA, 4);
    
    if (lua_isnil(LUA, -1)) {
        ws = 0;
        ws_name = NULL;
    } else {
        ws = 1;
        ws_name = luastring(LUA, -1, funcnames[DSB_ALT_WALLSET], 1);
    }

    esb_set_loc_wallset(LUA, lev, xx, yy, dir, ws_name);

    RETURN(0);
}

int expl_esb_add_champion(lua_State *LUA) {
    int i;
    int r_parm;
    struct champion newchamp;
    struct champion *targ_champ;
    const char *name;
    const char *lastname;
    const char *portrait_name;
    int i_uid = -255;
    int i_c_champs = 0;
    char *s_pdesg = NULL;
    // changed from hardcoded value of 4
    int rparms = 13 + gd.max_class;
    
    luaonstack(funcnames[DSB_ADD_CHAMPION]);
    
    if (lua_gettop(LUA) == (rparms + 2)) {
        char *s_cid = "DSB__C";
        const char *s_t;

        i_uid = luaint(LUA, -1*(rparms+2), s_cid, 1);

        s_t = luastring(LUA, -1*(rparms+1), s_cid, 1);
        s_pdesg = dsbstrdup(s_t);

    } else {
        
        if(lua_gettop(LUA) != rparms)
            parmerror(rparms, funcnames[DSB_ADD_CHAMPION]);
    }
      
    memset(&newchamp, 0, sizeof(struct champion));
    
    portrait_name = luastring(LUA, -1*rparms, funcnames[DSB_ADD_CHAMPION], 1);
        
    name = luastring(LUA, -1*(rparms-1), funcnames[DSB_ADD_CHAMPION], 2);
    lastname = luastring(LUA, -1*(rparms-2), funcnames[DSB_ADD_CHAMPION], 3);
    strncpy(newchamp.name, name, 8);
    newchamp.name[7] = '\0';
    strncpy(newchamp.lastname, lastname, 20);
    newchamp.lastname[19] = '\0'; 
    
    for(i=0;i<3;++i) {
        r_parm = luaint(LUA, -1*((10+gd.max_class) - i),
            funcnames[DSB_ADD_CHAMPION], 4+i);

        if (r_parm < 0 || r_parm > 9990)
            DSBLerror(LUA, "%s: Bad val %d in param %d",
                newchamp.name, r_parm, 4+i);
        else {
            newchamp.bar[i] = r_parm;
            newchamp.maxbar[i] = r_parm;
        }
    } 
    
    for(i=0;i<7;++i) {
        r_parm = luaint(LUA, -1*((7+gd.max_class) - i),
            funcnames[DSB_ADD_CHAMPION], 7+i);
            
        if (r_parm < 0 || r_parm > 2559)
            DSBLerror(LUA, "%s: Bad stat %d in param %d",
                newchamp.name, r_parm, 7+i);
        else {
            newchamp.stat[i] = r_parm;
            newchamp.maxstat[i] = r_parm;
        }
    } 
    // start luck slightly lower than max
    newchamp.stat[STAT_LUCK] *= 5;
    newchamp.stat[STAT_LUCK] /= 6;
    
    allocate_champion_xp_memory(&newchamp);
    allocate_champion_invslots(&newchamp);
    
    for(i=0;i<4;++i) {
        int parmnum = -1*(gd.max_class - i);
        
        if (lua_istable(LUA, parmnum)) {
            int sub;
            
            lua_pushvalue(LUA, parmnum);
            for(sub=0;sub<(gd.max_subsk[i]+1);++sub) {
                int tval;
                
                lua_pushinteger(LUA, sub+1);
                lua_gettable(LUA, -2);
                tval = lua_tointeger(LUA, -1);
                lua_pop(LUA, 1);

                if (tval < 0 || tval > gd.max_lvl) {
                    DSBLerror(LUA, "%s: Bad xp level %d in param %d[%d]",
                        newchamp.name, r_parm, 14+i, sub+1); 
                } else {
                    int xp = levelxp(tval);
                    
                    if (sub == 0)
                        newchamp.xp[i] = xp;  
                    else
                        newchamp.sub_xp[i][sub-1] = (xp / gd.max_subsk[i]);
                }
            }
            lua_pop(LUA, 1);
            
        } else {
            r_parm = luaint(LUA, parmnum,
                funcnames[DSB_ADD_CHAMPION], 14+i);
    
            if (r_parm < 0 || r_parm > gd.max_lvl)
                DSBLerror(LUA, "%s: Bad xp level %d in param %d",
                    newchamp.name, r_parm, 14+i);
            else {
                int tmp;
                int xp = levelxp(r_parm);
                
                newchamp.xp[i] = xp;
                
                // allocate subskills like CSB does
                for(tmp=0;tmp<4;++tmp)
                    newchamp.sub_xp[i][tmp] = xp / gd.max_subsk[i];
            }
        }
    }
    
    newchamp.method_name = dsbstrdup("unarmed_methods");
    
    newchamp.food = 2524;
    newchamp.water = 2524;
    
    i_c_champs = gd.num_champs;
    if (i_uid < 0) {
        gd.num_champs++;
        i_uid = gd.num_champs;
    } else {
        if (i_uid > gd.num_champs)
            gd.num_champs = i_uid;
    }
    
    if (gd.num_champs > i_c_champs) {
        gd.champs = dsbrealloc(gd.champs, gd.num_champs * sizeof(struct champion));
        edg.c_ext = dsbrealloc(edg.c_ext, gd.num_champs * sizeof(struct eextchar));
        
        memset(&(gd.champs[i_c_champs]), 0,
            sizeof(struct champion) * (gd.num_champs - i_c_champs));  
        memset(&(edg.c_ext[i_c_champs]), 0,
            sizeof(struct eextchar) * (gd.num_champs - i_c_champs)); 
    }

    targ_champ = &(gd.champs[(i_uid - 1)]);
    memcpy(targ_champ, &newchamp, sizeof(struct champion));
    
    targ_champ->port_name = dsbstrdup(portrait_name);
    
    // no designation given, create one
    if (!s_pdesg) {
        int n = 0;
        char unspaced_name[12];
        char tmpstr[90];
        snprintf(unspaced_name, sizeof(unspaced_name), "%s", name);
        while (unspaced_name[n] != '\0') {
            if (unspaced_name[n] == ' ')
                unspaced_name[n] = '_';
            ++n;
        }
        snprintf(tmpstr, sizeof(tmpstr), "%s_%02d", unspaced_name, i_uid);
        s_pdesg = dsbstrdup(tmpstr);
    }
    lua_pushinteger(LUA, i_uid);
    lua_setglobal(LUA, s_pdesg);
    edg.c_ext[i_uid - 1].desg = dsbstrdup(s_pdesg);
    
    // for sconvert
    lua_getglobal(LUA, "reverse_champion_table");
    lua_pushinteger(LUA, i_uid);
    lua_pushstring(LUA, s_pdesg);
    lua_settable(LUA, -3);
    lua_pop(LUA, 1);
   
    targ_champ->condition = dsbcalloc(gd.num_conds, sizeof(int));
    
    /*lua_getglobal(LUA, targ_champ->method_name);
    if (lua_istable(LUA, -1)) {
        targ_champ->method = import_attack_methods(newchamp.name);
    } else if (!lua_isfunction(LUA, -1)) {
        DSBLerror(LUA, "%s: Could not set unarmed attack methods",
            newchamp.name);
    }
    lua_pop(LUA, 1);
    
    targ_champ->portrait = grab_from_gfxtable(targ_champ->port_name);
    */
    dsbfree(s_pdesg);
    
    lua_pushinteger(LUA, i_uid);
       
    RETURN(1);
}

int expl_esb_champion_toparty(lua_State *LUA) {
    int ppos;
    int who_add;
    int ap;

    INITPARMCHECK(2, funcnames[DSB_CHAMPION_TOPARTY]);
    
    ppos = luaint(LUA, -2, funcnames[DSB_CHAMPION_TOPARTY], 1);
    who_add = luaint(LUA, -1, funcnames[DSB_CHAMPION_TOPARTY], 2);
    validate_ppos(ppos);
    validate_champion(who_add);
    
    if (gd.party[ppos]) {
        RETURN(0);
    }
    
    gd.party[ppos] = who_add;
        
    lua_pushboolean(LUA, 1);
    RETURN(1);
}


void lua_register_funcs(lua_State *LUA) {
    int i = 0;
 
    onstack("ESB_lua_register_funcs");
    
    while (ESB_Funcs[i].name != NULL) {
        lua_pushcfunction(LUA, ESB_Funcs[i].func);
        lua_setglobal(LUA, ESB_Funcs[i].name);
        i++;
    }
    
    VOIDRETURN();
}
