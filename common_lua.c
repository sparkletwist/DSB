#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <fmod.h>
#include "objects.h"
#include "lua_objects.h"
#include "uproto.h"
#include "lproto.h"
#include "champions.h"
#include "gproto.h"
#include "timer_events.h"
#include "global_data.h"
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
#include "trans.h"

int ZID;
extern int debug;
extern unsigned int i_s_error_c;

extern int ws_tbl_len;
extern struct wallset **ws_tbl;

extern struct global_data gd;
extern struct inventory_info gii;
extern struct translate_table Translate;

extern FILE *errorlog;
extern struct dungeon_level *dun;

extern struct inst *oinst[NUM_INST];

extern struct condition_desc *i_conds;
extern struct condition_desc *p_conds;

extern lua_State *LUA;

extern int grcnt;
extern int viewstate;

extern const char *Lcstr;
extern const char *ESBCNAME;
extern const char *DSB_MasterSoundTable;

extern int lua_cz_n;
extern struct clickzone *lua_cz;

extern const char *DSBMTGC;
extern istack istaparty;
extern int Gmparty_flags;
extern char inst_dirty[];

const char *funcnames[ALL_LUA_FUNCS+1] = {
    "__regobject",
    "__obj_newidxfunc",
    "__log",
    "__flush",
    "__nextinst",
    "__l_tag",
    "__trt_iassign",
    "__trt_purge",
    "__trt_obj_writeback",
    "__activate_gui_zone",
    "__set_internal_gui",
    "__set_internal_shadeinfo",
    // "dsb_insts",
    // "dsb_in_obj",
    "dsb_valid_inst",
    "dsb_game_end",
    "dsb_include_file",
    "dsb_get_bitmap",
    "dsb_clone_bitmap",
    "dsb_get_mask_bitmap",
    "dsb_get_sound",
    "dsb_music",
    "dsb_music_to_background",
    "dsb_get_font",
    "dsb_destroy_bitmap",
    "dsb_new_bitmap",
    "dsb_screen_bitmap",
    "dsb_bitmap_blit",
    "dsb_bitmap_width",
    "dsb_bitmap_height",
    "dsb_bitmap_textout",
    "dsb_text_size",
    "dsb_bitmap_draw",
    "dsb_bitmap_rect",
    "dsb_get_pixel",
    "dsb_set_pixel",
    "dsb_rgb_to_hsv",
    "dsb_hsv_to_rgb",
    "dsb_cache_invalidate",
    "dsb_viewport",
    "dsb_viewport_distort",
    "dsb_make_wallset",
    "dsb_make_wallset_ext",
    "dsb_image2map",
    "dsb_text2map",
    "dsb_party_place",
    "dsb_level_wallset",
    "dsb_level_flags",
    "dsb_level_tint",
    "dsb_level_getinfo",
    "dsb_spawn",
    "dsb_move",
    "dsb_move_moncol",
    "dsb_reposition",
    "dsb_tileptr_exch",
    "dsb_tileptr_rotate",
    "dsb_collide",
    "dsb_delete",
    "dsb_enable",
    "dsb_disable",
    "dsb_toggle",
    "dsb_get_cell",
    "dsb_set_cell",
    "dsb_visited",
    "dsb_forward",
    "dsb_get_condition",
    "dsb_set_condition",
    "dsb_add_condition",
    "dsb_replace_condition",
    "dsb_add_champion",
    "dsb_put_ppos_at_tile",
    "dsb_fetch",
    "dsb_qswap",
    "dsb_swap",
    "dsb_offer_champion",
    "dsb_get_gfxflag",
    "dsb_set_gfxflag",
    "dsb_clear_gfxflag",
    "dsb_toggle_gfxflag",
    "dsb_get_mpartyflag",
    "dsb_set_mpartyflag",
    "dsb_clear_mpartyflag",
    "dsb_toggle_mpartyflag",
    "dsb_get_charge",
    "dsb_set_charge",
    "dsb_get_animtimer",
    "dsb_set_animtimer",
    "dsb_get_crop",
    "dsb_set_crop",
    "dsb_get_crmotion",
    "dsb_set_crmotion",
    "dsb_get_cropmax",
    "dsb_subrenderer_target",
    "dsb_dungeon_view",
    "dsb_bitmap_clear",
    "dsb_objzone",
    "dsb_msgzone",
    "dsb_find_arch",
    "dsb_party_coords",
    "dsb_party_at",
    "dsb_party_contains",
    "dsb_party_viewing",
    "dsb_party_affecting",
    "dsb_party_apush",
    "dsb_party_apop",
    "dsb_hide_mouse",
    "dsb_show_mouse",
    "dsb_mouse_state",
    "dsb_mouse_override",
    "dsb_push_mouse",
    "dsb_pop_mouse",
    "dsb_shoot",
    "dsb_animate",
    "dsb_get_coords",
    "dsb_get_coords_prev",
    "dsb_sound",
    "dsb_3dsound",
    "dsb_stopsound",
    "dsb_checksound",
    "dsb_get_soundvol",
    "dsb_set_soundvol",
    "dsb_get_flystate",
    "dsb_set_flystate",
    "dsb_get_sleepstate",
    "dsb_wakeup",
    "dsb_set_openshot",
    "dsb_get_flydelta",
    "dsb_set_flydelta",
    "dsb_get_flyreps",
    "dsb_set_flyreps",
    "dsb_get_facedir",
    "dsb_set_facedir",
    "dsb_get_hp",
    "dsb_set_hp",
    "dsb_get_maxhp",
    "dsb_set_maxhp",
    "dsb_set_tint",
    "dsb_get_pfacing",
    "dsb_set_pfacing",
    "dsb_get_charname",
    "dsb_get_chartitle",
    "dsb_set_charname",
    "dsb_set_chartitle",
    "dsb_get_load",
    "dsb_get_maxload",
    "dsb_get_food",
    "dsb_set_food",
    "dsb_get_water",
    "dsb_set_water",
    "dsb_get_idle",
    "dsb_set_idle",
    "dsb_get_stat",
    "dsb_set_stat",
    "dsb_get_maxstat",
    "dsb_set_maxstat",
    "dsb_get_injury",
    "dsb_set_injury",
    "dsb_update_inventory_info",
    "dsb_update_system",
    "dsb_current_inventory",
    "dsb_delay_func",
    "dsb_write",
    "dsb_get_bar",
    "dsb_set_bar",
    "dsb_get_maxbar",
    "dsb_set_maxbar",
    "dsb_damage_popup",
    "dsb_ppos_char",
    "dsb_char_ppos",
    "dsb_ppos_tile",
    "dsb_tile_ppos",
    "dsb_get_xp_multiplier",
    "dsb_set_xp_multiplier",
    "dsb_give_xp",
    "dsb_get_xp",
    "dsb_set_xp",
    "dsb_get_temp_xp",
    "dsb_set_temp_xp",
    "dsb_xp_level",
    "dsb_xp_level_nobonus",
    "dsb_get_bonus",
    "dsb_set_bonus",
    "dsb_msg",
    "dsb_msg_chain",
    "dsb_delay_msgs_to",
    "dsb_lock_game",
    "dsb_unlock_game",
    "dsb_get_gamelocks",
    "dsb_tileshift",
    "dsb_rand",
    "dsb_rand_seed",
    "dsb_attack_text",
    "dsb_attack_damage",
    "dsb_export",
    "dsb_replace_methods",
    "dsb_linesplit",
    "dsb_get_gameflag",
    "dsb_set_gameflag",
    "dsb_clear_gameflag",
    "dsb_toggle_gameflag",
    "dsb_ai",
    "dsb_ai_boss",
    "dsb_ai_subordinates",
    "dsb_ai_promote",
    "__get_light_raw",
    "dsb_get_light_total",
    "__set_light_raw",
    "dsb_set_light_totalmax",
    "dsb_get_portraitname",
    "dsb_set_portraitname",
    "dsb_replace_charhand",
    "dsb_replace_inventory",
    "dsb_replace_topimages",
    "dsb_get_exviewinst",
    "dsb_set_exviewinst",
    "dsb_get_leader",
    "dsb_get_caster",
    "dsb_lookup_global",
    "dsb_textformat",
    "dsb_champion_toparty",
    "dsb_champion_fromparty",
    "dsb_party_scanfor",
    "dsb_spawnburst_begin",
    "dsb_spawnburst_end",
    "dsb_fullscreen",
    "dsb_get_alt_wallset",
    "dsb_set_alt_wallset",
    "dsb_override_floor",
    "dsb_override_roof",
    "dsb_wallset_flip_floor",
    "dsb_wallset_flip_roof",
    "dsb_wallset_always_draw_floor",
    "dsb_get_lastmethod",
    "dsb_set_lastmethod",
    "dsb_import_arch",
    "dsb_get_pendingspell",
    "dsb_set_pendingspell",
    "dsb_poll_keyboard",
    "dsb_textconvert_dmstring",
    "dsb_textconvert_numbers",
    "@LASTSTRING"
};

int il_register_object(lua_State *LUA) {
    char *objname = "register";
    
    luaonstack(objname);
    only_when_objparsing(LUA, objname);
    register_object(LUA, 1);
    
    RETURN(0);
}

int il_nextinst(lua_State *LUA) {
    int cv,ic;
    luaonstack(funcnames[_DSB_NEXTINST]);
    cv = lua_tointeger(LUA, -1);
    
    for(ic=(cv+1);ic<NUM_INST;++ic) {
        if (oinst[ic]) {
            lua_pushinteger(LUA, ic);
            RETURN(1);
        }
    }
    
    lua_pushnil(LUA);
    RETURN(1);   
}

int il_l_tag(lua_State *LUA) {
    const char *str;
    str = luastring(LUA, -1, funcnames[_DSB_L_TAG], 1);
    //ed_tag_l(str);
    return 0;
}

int il_obj_newidxfunc(lua_State *LUA) {

    luastacksize(6);

    // put me in the __ZIDT index
    lua_getglobal(LUA, "__ZIDT");
    lua_pushinteger(LUA, ZID);
    lua_pushvalue(LUA, -4);
    lua_rawset(LUA, -3);
    lua_pop(LUA, 1);

    // actually set the table value
    lua_rawset(LUA, -3);
    
    ZID++;
}

int il_log(lua_State *LUA) {
    const char *s;
    luaonstack(funcnames[_DSB_LOG]);
    s = luastring(LUA, -1, funcnames[_DSB_LOG], 1);
    fprintf(errorlog, "Lua: %s\n", s);
    RETURN(0);
}

int il_flush(lua_State *LUA) {
    luaonstack(funcnames[_DSB_FLUSH]);
    fflush(errorlog);
    RETURN(0);
}

int il_trt_iassign(lua_State *LUA) {
    int i;
    onstack("trt_assign");
    
    luastacksize(8);
    
    if (debug) {
        fprintf(errorlog, "@@@@@ INITIAL INTEGRITY CHECK\n");
        objarchctl(INTEGRITY_CHECK, 0); 
    }
    
    lua_getglobal(LUA, "obj");
    for (i=0;i<Translate.last;++i) {
        
        lua_pushstring(LUA, Translate.e[i].str);
        lua_gettable(LUA, -2);
        
        if (lua_istable(LUA, -1)) {       
            lua_pushstring(LUA, "trnum");
            lua_pushlightuserdata(LUA, (void *)Translate.e[i].id);
            lua_settable(LUA, -3);  
            /*
            fprintf(errorlog, "Wrote trnum %x into obj.%s\n",
                Translate.e[i].id, Translate.e[i].str);
            */
        }
                  
        lua_pop(LUA, 1);    
    }  
    lua_pop(LUA, 1);  
    
    if (debug) {
        fprintf(errorlog, "@@@@@ FINAL INTEGRITY CHECK\n");
        objarchctl(INTEGRITY_CHECK, 0);   
    } 

    RETURN(0);    
}

int il_trt_purge(lua_State *LUA) {
    onstack("trt_purge");
    purge_translation_table(&Translate);
    RETURN(0);      
}

int il_trt_obj_writeback(lua_State *LUA) {
    char *dnameref;
    unsigned int arch_id;
    
    onstack("trt_obj_writeback");
    
    luastacksize(6);
    
    dnameref = dsbstrdup(lua_tostring(LUA, -1));
    lua_getglobal(LUA, "obj");
    lua_pushstring(LUA, dnameref);
    lua_gettable(LUA, -2);
    lua_pushstring(LUA, "regnum");
    lua_gettable(LUA, -2);
    arch_id = (int)lua_touserdata(LUA, -1);
    lua_pop(LUA, 3);
    
    if (Translate.last >= Translate.max)
        translate_enlarge(&Translate);
    Translate.e[Translate.last].str = dnameref;
    Translate.e[Translate.last].id = arch_id;
    Translate.last++;
    
    RETURN(0);      
}

int il_activate_gui_zone(lua_State *LUA) {
    const char *s_fn = funcnames[_DSB_ACTIVATE_GUI];
    const char *bstr;
    char *dstr;
    int zid, x, y, w, h, flags;
    
    INITPARMCHECK(7, s_fn);
    
    bstr = luastring(LUA, -7, s_fn, 1);
    zid = luaint(LUA, -6, s_fn, 2);
    x = luaint(LUA, -5, s_fn, 3);
    y = luaint(LUA, -4, s_fn, 4);
    w = luaint(LUA, -3, s_fn, 5);
    h = luaint(LUA, -2, s_fn, 6);
    flags = luaint(LUA, -1, s_fn, 7);
    
    dstr = dsbstrdup(bstr);
    activate_gui_zone(dstr, zid, x, y, w, h, flags);
    
    RETURN(0);     
}

int il_set_internal_gui(lua_State *LUA) {
    const char *s_fn = funcnames[_DSB_SET_INTERNAL_GUI];
    const char *bstr;
    int val;
    
    INITPARMCHECK(2, s_fn);
    
    bstr = luastring(LUA, -2, s_fn, 1);
    val = luaint(LUA, -1, s_fn, 2);

    internal_gui_command(bstr[0], val);

    RETURN(0);     
}

int il_set_internal_shadeinfo(lua_State *LUA) {
    unsigned int regarch;
    const char *s_fn = funcnames[_DSB_SET_INTERNAL_SHADEINFO];
    int parm[5];
    int p;
    
    INITVPARMCHECK(5, s_fn); 
    
    if (lua_gettop(LUA) == 6) {
        regarch = luaobjarch(LUA, -1, funcnames[_DSB_SET_INTERNAL_SHADEINFO]);   
        lua_pop(LUA, 1);
    } else {
        regarch = 0xFFFFFFFF;
    }    
    
    for(p=0;p<5;++p) {
        if (lua_isnumber(LUA, -5+p)) { 
            parm[p] = lua_tointeger(LUA, -5+p);
        } else
            parm[p] = 0;    
    } 
    
    internal_shadeinfo(regarch, parm[0], parm[1], parm[2], parm[3], parm[4]);    
    
    RETURN(0);
}

int expl_import_arch(lua_State *LUA) {
    const char *root_name = "ROOT_NAME";
    const char *importation_function = "ARCHETYPE_IMPORTED";
    const char *filename;
    const char *objname;
    int is_using_editor = LHFALSE;
    int push_exestate;
    
    INITPARMCHECK(2, funcnames[DSB_IMPORT_ARCH]);
    only_when_objparsing(LUA, funcnames[DSB_IMPORT_ARCH]);
    
    filename = luastring(LUA, -2, funcnames[DSB_IMPORT_ARCH], 1);
    objname = luastring(LUA, -1, funcnames[DSB_IMPORT_ARCH], 2);
    
    push_exestate = gd.exestate;
    gd.exestate = STATE_CUSTOMPARSE;
    
    lua_pushstring(LUA, objname);
    lua_setglobal(LUA, root_name);
    
    stackbackc('(');
    v_onstack(filename);
    addstackc(')');
    
    src_lua_file(fixname(filename), 0);
    
    if (gd.edit_mode)
        is_using_editor = LHTRUE;
    gd.lua_bool_hack = 1;
    lc_parm_int(importation_function, 1, is_using_editor);
    
    lua_pushnil(LUA);
    lua_setglobal(LUA, root_name);
    lua_pushnil(LUA);
    lua_setglobal(LUA, importation_function);
    
    gd.exestate = push_exestate;
    
    RETURN(0);
}

int expl_add_champion(lua_State *LUA) {
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
    
    only_when_dungeon(LUA, funcnames[DSB_ADD_CHAMPION]);
    
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
        memset(&(gd.champs[i_c_champs]), 0,
            sizeof(struct champion) * (gd.num_champs - i_c_champs));  
    }

    targ_champ = &(gd.champs[(i_uid - 1)]);
    memcpy(targ_champ, &newchamp, sizeof(struct champion));
    
    targ_champ->port_name = dsbstrdup(portrait_name);
    
    if (s_pdesg) {
        lua_pushinteger(LUA, i_uid);
        lua_setglobal(LUA, s_pdesg);
    }
   
    targ_champ->condition = dsbcalloc(gd.num_conds, sizeof(int));
    targ_champ->cond_animtimer = dsbcalloc(gd.num_conds, sizeof(int));
    set_animtimers_unused(targ_champ->cond_animtimer, gd.num_conds);

    lua_getglobal(LUA, targ_champ->method_name);
    if (lua_istable(LUA, -1)) {
        targ_champ->method = import_attack_methods(newchamp.name);
    } else if (!lua_isfunction(LUA, -1)) {
        DSBLerror(LUA, "%s: Could not set unarmed attack methods",
            newchamp.name);
    }
    lua_pop(LUA, 1);
    
    targ_champ->portrait = grab_from_gfxtable(targ_champ->port_name);
    
    dsbfree(s_pdesg);
    
    lua_pushinteger(LUA, i_uid);
       
    RETURN(1);
}

int expl_replace_method(lua_State *LUA) {
    char *mname;
    int who;
    struct att_method *repmethod;
    
    INITPARMCHECK(2, funcnames[DSB_REPLACE_METHODS]);
    
    who = luaint(LUA, -2, funcnames[DSB_REPLACE_METHODS], 1);
    mname = (char *)luastring(LUA, -1, funcnames[DSB_REPLACE_METHODS], 2);
    validate_champion(who);
    lua_getglobal(LUA, mname);
    
    if (lua_istable(LUA, -1)) {
        repmethod = import_attack_methods(mname);
    } else {
        goto BAD_REPLACE_METHOD;
    }
    
    if (repmethod == NULL) 
        goto BAD_REPLACE_METHOD;
    
    if (gd.champs[who-1].method) {   
        destroy_attack_methods(gd.champs[who-1].method);
        dsbfree(gd.champs[who-1].method_name);
    }
        
    gd.champs[who-1].method = repmethod;
    gd.champs[who-1].method_name = dsbstrdup(mname);
    
    // clear it out if we're displaying it right now 
    if (gd.who_method) {
        int rppos = 0;
        for (rppos=0;rppos<4;rppos++) {
            if (gd.party[rppos] == who) {
                if (gd.who_method == rppos+1) {
                    clear_method();
                }
            }
        }
    }
    
    RETURN(0);

    BAD_REPLACE_METHOD:
    DSBLerror(LUA, "%s: Bad attack method %s", funcnames[DSB_REPLACE_METHODS], mname);
    
    RETURN(0);
}

int expl_spawn(lua_State *LUA) {
    struct obj_arch *p_arch;
    unsigned int objarch;
    unsigned int inst;
    int lev, xx, yy, dir;
    int b_force_instant = 0;
    
    INITVPARMCHECK(5, funcnames[DSB_SPAWN]);
    
    if (lua_gettop(LUA) == 6) {
        const char *s_cid = "DSB_ISPAWN";
        gd.force_id = luaint(LUA, -6, (char*)s_cid, 1); 
        if (gd.force_id == 0)
            b_force_instant = 1;  
    } else
        gd.force_id = 0;
    
    lev = luaint(LUA, -4, funcnames[DSB_SPAWN], 2);
    xx = luaint(LUA, -3, funcnames[DSB_SPAWN], 3);
    yy = luaint(LUA, -2, funcnames[DSB_SPAWN], 4);
    dir = luaint(LUA, -1, funcnames[DSB_SPAWN], 5);
    lua_pop(LUA, 4);
    
    //if (lev > 0) lev += 2;
    
    STARTNONF();
    objarch = luaobjarch(LUA, -1, funcnames[DSB_SPAWN]);
    ENDNONF();
    if (objarch == 0xFFFFFFFF) {
        RETURN(0);
    }
    
    p_arch = Arch(objarch);
    
    // add the param
    stackbackc('(');
    v_onstack(p_arch->luaname);
    addstackc(')');
    
    luastacksize(5);
       
    validate_objcoord(funcnames[DSB_SPAWN], lev, xx, yy, dir);
        
    if (lev == LOC_PARTY) {
        xx = gd.party[xx];
        lev = LOC_CHARACTER;
    }
    
    if (lev == LOC_CHARACTER) {
        
        if (UNLIKELY(xx < 1)) {
            puke_bad_subscript("gd.champs", xx-1);
            gd.force_id = 0;
            RETURN(0);
        }
            
        // don't create into a full spot
        if (gd.champs[xx-1].inv[yy]) {
            lua_pushnil(LUA);
            gd.force_id = 0;
            RETURN(1);
        }
    }
    
    inst = create_instance(objarch, lev, xx, yy, dir, b_force_instant);
    
    lua_pushinteger(LUA, inst);
    RETURN(1);   
}

int expl_fetch(lua_State *LUA) {
    int it = 1;
    int lev, xx, yy, dir;
    int zdir;
    int nr = 1;
    int expar = 0;
    
    INITPARMCHECK(4, funcnames[DSB_FETCH]);
    
    STARTNONF();
    lev = luaint(LUA, -4, funcnames[DSB_FETCH], 1);
    xx = luaint(LUA, -3, funcnames[DSB_FETCH], 2);
    yy = luaint(LUA, -2, funcnames[DSB_FETCH], 3);
    dir = luaint(LUA, -1, funcnames[DSB_FETCH], 4);

    zdir = dir;
    if (lev >= 0 && dir == -1) zdir = 0;
    
    validate_objlevcoord(funcnames[DSB_FETCH], lev, xx, yy, zdir);
    ENDNONF();
    
    if (lev >= 0) {
        if (xx < 0 || xx >= dun[lev].xsiz) {
            PUSHNILDONE:
            lua_pushnil(LUA);
            RETURN(1);
        }

        if (yy < 0 || yy >= dun[lev].ysiz) {
            goto PUSHNILDONE;
        }
    }
    
    if (lev == LOC_PARTY || lev == LOC_CHARACTER) {
        int charidx;
        
        if (lev == LOC_PARTY)
            charidx = gd.party[xx];
        else
            charidx = xx;
            
        stackbackc('(');
        v_onstack("CHAR)");
            
        if (yy == -1) {
            int ins = 0;
            int z;
            
            for(z=0;z<gii.max_invslots;++z) {
                int ot = gd.champs[charidx-1].inv[z];
                objtable(ot, &ins, z+1);
            }
            if (!ins)
                lua_pushnil(LUA);
            else {
                nr = 2;
                expar = z;
            }
                
        } else {
            it = gd.champs[charidx-1].inv[yy];
            rtableval(dir, it);
        }
        
    } else if (lev == LOC_MOUSE_HAND) {
        stackbackc('(');
        v_onstack("MOUSE_HAND)");
        rtableval(dir, gd.mouseobj);
        nr = 2;
        expar = 1;
    } else if (lev == LOC_LIMBO) {
        int ins = 0;
        int i_tval = 1;
        int z;

        for (z=0;z<NUM_INST;++z) {
            struct inst *p_inst = oinst[z];
            if (p_inst && p_inst->level == LOC_LIMBO) {
                objtable(z, &ins, i_tval);
                ++i_tval;
            }
        }
        expar = i_tval - 1;
        nr = 2;
    } else if (lev == LOC_IN_OBJ) {
        struct inst *p_inst;
        int pn;
        
        stackbackc('(');
        v_onstack("IN_OBJ)");
        
        p_inst = oinst[xx];
        pn = p_inst->inside_n;
        
        if (yy == -1) {
            int ins = 0;
            int i_tval = 1;
            int z;
            
            for (z=0;z<pn;++z) {
                int ot = p_inst->inside[z];

                if (ot) {
                    objtable(ot, &ins, i_tval);
                    ++i_tval;
                }
            }
                
            if (!ins)
                lua_pushnil(LUA);
            else {
                nr = 2;
                expar = (i_tval - 1);
            }
                
        } else {
            if (yy >= pn)
                lua_pushnil(LUA);
            else {
                int p = p_inst->inside[yy];
                rtableval(dir, p);
            }
        }
        
    } else if (lev >= 0) {
        struct dungeon_level *dd;
        struct inst_loc *dt_il;
        int cnt = 1;
        int ins = 0;
        int ldir;
        int maxdir = dir;
        int ignoreslaves = 0;
        char debug_sstr[60];
        
        stackbackc('(');
        sprintf(debug_sstr, "LEVEL, %d, %d, %d, %d)", lev, xx, yy, dir);
        v_onstack(debug_sstr);
        
        if (dir == -1) {
            dir = 0;
            maxdir = 4;
            ignoreslaves = 1;
        }
      
        dd = &(dun[lev]);
        
        for(ldir=dir;ldir<=maxdir;ldir++) {
            dt_il = dd->t[yy][xx].il[ldir];
            while (dt_il != NULL) {
                if (!ignoreslaves || !dt_il->slave) {
                    if (dt_il->i) {
                        v_onstack("objtable");
                        objtable(dt_il->i, &ins, cnt);
                        v_upstack();
                        
                        ++cnt;
                    }
                }

                dt_il = dt_il->n;
            }
        }
        
        if (!ins)
            lua_pushnil(LUA);
        else {

            if (UNLIKELY(!lua_istable(LUA, -1))) {
                recover_error("dsb_fetch error");
                lua_pushnil(LUA);
                RETURN(1);
            }
            
            nr = 2;
            expar = cnt - 1;
        }
        
    } else
        lua_pushnil(LUA);
        
    if (nr == 2)
        lua_pushinteger(LUA, expar);
    
    RETURN(nr);
}

int expl_image2map(lua_State *LUA) {
    int lev, lite, xpm;
    const char *dfilename;
    
    INITPARMCHECK(4, funcnames[DSB_IMAGE2MAP]);
    
    only_when_dungeon(LUA, funcnames[DSB_IMAGE2MAP]);
    
    lev = luaint(LUA, -4, funcnames[DSB_IMAGE2MAP], 1);
    dfilename = luastring(LUA, -3, funcnames[DSB_IMAGE2MAP], 2);
    lite = luaint(LUA, -2, funcnames[DSB_IMAGE2MAP], 3);
    xpm = luaint(LUA, -1, funcnames[DSB_IMAGE2MAP], 4);
    
    if (lev < 0)
        DSBLerror(LUA, "%s requires nonnegative level number",
            funcnames[DSB_IMAGE2MAP]);
            
    if (lite < 0)
        DSBLerror(LUA, "%s requires nonnegative light level",
            funcnames[DSB_IMAGE2MAP]);
            
    if (xpm < 0)
        DSBLerror(LUA, "%s requires nonnegative experience multiplier",
            funcnames[DSB_IMAGE2MAP]);

    pixmap2level(lev, dfilename, lite, xpm);
    
    RETURN(0);
}

int expl_text2map(lua_State *LUA) {
    int lev, lite, xpm;
    int xs, ys;
    const char *dfilename;

    INITPARMCHECK(6, funcnames[DSB_TEXT2MAP]);

    only_when_dungeon(LUA, funcnames[DSB_TEXT2MAP]);

    lev = luaint(LUA, -6, funcnames[DSB_TEXT2MAP], 1);
    xs = luaint(LUA, -5, funcnames[DSB_TEXT2MAP], 2);
    ys = luaint(LUA, -4, funcnames[DSB_TEXT2MAP], 3);
    lite = luaint(LUA, -3, funcnames[DSB_TEXT2MAP], 4);
    xpm = luaint(LUA, -2, funcnames[DSB_TEXT2MAP], 5);
    
    if (!lua_istable(LUA, -1) && !lua_isnil(LUA, -1))
        DSBLerror(LUA, "Level text map must be a table or nil");

    if (lev < 0)
        DSBLerror(LUA, "%s requires nonnegative level number",
            funcnames[DSB_TEXT2MAP]);

    if (lite < 0)
        DSBLerror(LUA, "%s requires nonnegative light level",
            funcnames[DSB_TEXT2MAP]);

    if (xpm < 0)
        DSBLerror(LUA, "%s requires nonnegative experience multiplier",
            funcnames[DSB_TEXT2MAP]);
            
    if (xs < 1)
        DSBLerror(LUA, "X size must be positive");
    else if (ys < 1)
        DSBLerror(LUA, "Y size must be positive");

    textmap2level(LUA, lev, xs, ys, lite, xpm);

    RETURN(0);
}

int expl_party_place(lua_State *LUA) {
    int ap = 0;
    int lev, xx, yy, facedir;
    int b = 0;
    
    INITVPARMCHECK(4, funcnames[DSB_PARTYPLACE]);
    
    if (lua_gettop(LUA) == 5) {
        ap = luaint(LUA, -1, funcnames[DSB_PARTYPLACE], 5);
        b = -1;
        if (ap < 0) ap = 0;
        if (ap > 3) ap = 3;
    } else
        ap = party_ist_peek(gd.a_party);
    
    lev = luaint(LUA, b-4, funcnames[DSB_PARTYPLACE], 1);
    xx = luaint(LUA, b-3, funcnames[DSB_PARTYPLACE], 2);
    yy = luaint(LUA, b-2, funcnames[DSB_PARTYPLACE], 3);
    facedir = luaint(LUA, b-1, funcnames[DSB_PARTYPLACE], 4);
    
    validate_coord(funcnames[DSB_PARTYPLACE], lev, xx, yy, facedir);
    if (facedir == 4) {
        poop_out("You have set a facedir of 4 for the party.\nThis is invalid and may indicate an off-by-one error in your Lua code.");
        RETURN(0);
    }
    
    if (gd.watch_inst == -1)
        gd.watch_inst_out_of_position++;
    
    if (gd.queue_rc) {
        instmd_queue(INSTMD_PARTY_PLACE, lev, xx, yy, facedir, ap, 0);
    } else {
        exec_party_place(ap, lev, xx, yy, facedir);
    }

    RETURN(0);
}

int expl_use_wallset(lua_State *LUA) {
    int ws;
    int lev_on;
    
    INITPARMCHECK(2, funcnames[DSB_USE_WALLSET]);

    lev_on = luaint(LUA, -2, funcnames[DSB_USE_WALLSET], 1);
    ws = luawallset(LUA, -1, funcnames[DSB_USE_WALLSET]);
    
    if (lev_on < 0 || lev_on >= gd.dungeon_levels)
        DSBLerror(LUA, "%s: Invalid level", funcnames[DSB_USE_WALLSET]);
    
    dun[lev_on].wall_num = ws;
 
    RETURN(0);
}

int expl_level_flags(lua_State *LUA) {
    int lev_on;
    unsigned int lev_flags;
    
    INITPARMCHECK(2, funcnames[DSB_LEVEL_FLAGS]);

    lev_on = luaint(LUA, -2, funcnames[DSB_LEVEL_FLAGS], 1);
    lev_flags = luaint(LUA, -1, funcnames[DSB_LEVEL_FLAGS], 2);
    
    if (lev_on < 0 || lev_on >= gd.dungeon_levels)
        DSBLerror(LUA, "%s: Invalid level", funcnames[DSB_LEVEL_FLAGS]);
    
    dun[lev_on].level_flags = lev_flags;
 
    RETURN(0);
}

int expl_level_tint(lua_State *LUA) {
    int b = 0;
    int lev_on;
    int ltint;
    unsigned int ltint_intensity = DSB_DEFAULT_TINT_INTENSITY;

    INITVPARMCHECK(2, funcnames[DSB_LEVEL_TINT]);
    if (lua_gettop(LUA) == 3) {
        ltint_intensity = luaint(LUA, -1, funcnames[DSB_LEVEL_TINT], 3); 
        b = -1;    
    }
    
    lev_on = luaint(LUA, -2 + b, funcnames[DSB_LEVEL_TINT], 1);
    ltint = luargbval(LUA, -1 + b, funcnames[DSB_LEVEL_TINT], 2);
    
    if (lev_on < 0 || lev_on >= gd.dungeon_levels) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "%s: Invalid level", funcnames[DSB_LEVEL_TINT]);
        RETURN(0);
    }

    dun[lev_on].tint = ltint;
    dun[lev_on].tint_intensity = ltint_intensity;

    RETURN(0);
}

int expl_spawnburst_begin(lua_State *LUA) {
    INITVPARMCHECK(0, funcnames[DSB_SPAWNBURST_BEGIN]);
    if (lua_gettop(LUA) == 1 && lua_isnumber(LUA, -1)) {
        gd.last_alloc = lua_tointeger(LUA, -1);
    }   
    gd.engine_flags |= ENFLAG_SPAWNBURST;
    RETURN(0);
}

int expl_spawnburst_end(lua_State *LUA) {
    INITPARMCHECK(0, funcnames[DSB_SPAWNBURST_END]);
    if (gd.engine_flags & ENFLAG_SPAWNBURST) {
        gd.engine_flags &= ~ENFLAG_SPAWNBURST;
        execute_spawn_events();
        terminate_spawnburst();
    } else
        DSBLerror(LUA, "spawnburst ended without beginning");

    RETURN(0);
}

int expl_get_alt_wallset(lua_State *LUA) {
    int lev, xx, yy, dir;
    int ws;
    
    INITPARMCHECK(4, funcnames[DSB_GET_ALT_WALLSET]);
    
    lev = luaint(LUA, -4, funcnames[DSB_GET_ALT_WALLSET], 1);
    xx = luaint(LUA, -3, funcnames[DSB_GET_ALT_WALLSET], 2);
    yy = luaint(LUA, -2, funcnames[DSB_GET_ALT_WALLSET], 3);
    dir = luaint(LUA, -1, funcnames[DSB_GET_ALT_WALLSET], 4);
    validate_coord(funcnames[DSB_GET_ALT_WALLSET], lev, xx, yy, dir);
    
    ws = dun[lev].t[yy][xx].altws[dir];
    if (ws) {
        luastacksize(8);
        lua_getglobal(LUA, "__wallset_name_by_ref");
        if (!lua_isnil(LUA, -1)) {
            lua_pushinteger(LUA, ws - 1);
            lua_pcall(LUA, 1, 1, 0);
        }
    } else
        lua_pushnil(LUA);

    RETURN(1);
}

int expl_set_alt_wallset(lua_State *LUA) {
    int lev, xx, yy, dir;
    int ws;
    
    INITPARMCHECK(5, funcnames[DSB_ALT_WALLSET]);
    
    lev = luaint(LUA, -4, funcnames[DSB_ALT_WALLSET], 2);
    xx = luaint(LUA, -3, funcnames[DSB_ALT_WALLSET], 3);
    yy = luaint(LUA, -2, funcnames[DSB_ALT_WALLSET], 4);
    dir = luaint(LUA, -1, funcnames[DSB_ALT_WALLSET], 5);
    validate_coord(funcnames[DSB_ALT_WALLSET], lev, xx, yy, dir);
    lua_pop(LUA, 4);
    
    if (lua_isnil(LUA, -1)) {
        ws = 0;
    } else {
        ws = luawallset(LUA, -1, funcnames[DSB_ALT_WALLSET]) + 1;
    }
    
    dun[lev].t[yy][xx].altws[dir] = ws;

    RETURN(0);
}

int expl_get_exviewinst(lua_State *LUA) {
    int who;
    int exinst;
    
    INITPARMCHECK(1, funcnames[DSB_GET_EXVIEWINST]);
    
    STARTNONF();
    who = luaint(LUA, -1, funcnames[DSB_SET_EXVIEWINST], 1);
    validate_champion(who);
    ENDNONF();
    
    exinst = gd.champs[who-1].exinst;
    if (exinst == 0) {
        lua_pushnil(LUA);
    } else {
        lua_pushinteger(LUA, exinst);
    }

    RETURN(1);
}

int expl_set_exviewinst(lua_State *LUA) {
    struct inst *p_inst;
    unsigned int exviewinst;
    unsigned int nexviewinst;
    int who;
    
    INITPARMCHECK(2, funcnames[DSB_SET_EXVIEWINST]);

    STARTNONF();
    who = luaint(LUA, -2, funcnames[DSB_SET_EXVIEWINST], 1);
    nexviewinst = luaint(LUA, -1, funcnames[DSB_SET_EXVIEWINST], 2);
    validate_champion(who);
    validate_instance(nexviewinst);
    ENDNONF();
    
    p_inst = oinst[nexviewinst];
    if (p_inst->level == LOC_PARCONSPC) {
        gd.lua_nonfatal++;
        DSBLerror(LUA, "inst %d is already used as exview", nexviewinst);
        RETURN(0);
    } else if (p_inst->level != LOC_LIMBO) {
        gd.lua_nonfatal++;
        DSBLerror(LUA, "inst %d is not in LIMBO", nexviewinst);
        RETURN(0);
    }
    
    exviewinst = gd.champs[who-1].exinst;
    if (exviewinst) {
        if (oinst[exviewinst])
            oinst[exviewinst]->level = LOC_LIMBO;
    }
    gd.champs[who-1].exinst = nexviewinst;
    p_inst->level = LOC_PARCONSPC;

    RETURN(0);
}

int expl_get_hp(lua_State *LUA) {
    unsigned int m;
    struct inst *p_inst;
    
    INITPARMCHECK(1, funcnames[DSB_GET_HP]);
    m = luaint(LUA, -1, funcnames[DSB_GET_HP], 1);
    validate_instance(m);
    p_inst = oinst[m];
    if (p_inst->ai)
        lua_pushinteger(LUA, p_inst->ai->hp);
    else
        lua_pushnil(LUA);
        
    RETURN(1);
}

int expl_set_hp(lua_State *LUA) {
    unsigned int m;
    int hpv;
    struct inst *p_inst;
    
    INITPARMCHECK(2, funcnames[DSB_SET_HP]);
    m = luaint(LUA, -2, funcnames[DSB_SET_HP], 1);
    hpv = luaint(LUA, -1, funcnames[DSB_SET_HP], 2);
    
    // fail quietly on invalid instance because it probably
    // just got killed by the monster deletion code or something
    if (m > 0 && !oinst[m] && inst_dirty[m]) {
        RETURN(0);
    }
    validate_instance(m);
    p_inst = oinst[m];
    
    if (p_inst->ai) {

        if (hpv > 0)
            p_inst->ai->hp = hpv;
            
        else {
            p_inst->ai->hp = 0;
            p_inst->ai->ai_flags |= AIF_DEAD;
            if (!gd.queue_monster_deaths) {
                defer_kill_monster(m);
            }
        }
    }
        
    RETURN(0);
}

int expl_get_maxhp(lua_State *LUA) {
    unsigned int m;
    struct inst *p_inst;
    
    INITPARMCHECK(1, funcnames[DSB_GET_MAXHP]);
    m = luaint(LUA, -1, funcnames[DSB_GET_MAXHP], 1);
    validate_instance(m);
    p_inst = oinst[m];
    if (p_inst->ai)
        lua_pushinteger(LUA, p_inst->ai->maxhp);
    else
        lua_pushnil(LUA);
        
    RETURN(1);
}

int expl_set_maxhp(lua_State *LUA) {
    unsigned int m;
    int hpv;
    struct inst *p_inst;
    
    INITPARMCHECK(2, funcnames[DSB_SET_MAXHP]);
    m = luaint(LUA, -2, funcnames[DSB_SET_MAXHP], 1);
    hpv = luaint(LUA, -1, funcnames[DSB_SET_MAXHP], 2);
    
    if (m > 0 && !oinst[m] && inst_dirty[m]) {
        RETURN(0);
    }    
    validate_instance(m);
    p_inst = oinst[m];
        
    if (p_inst->ai) {
        if (hpv > 0) {
            p_inst->ai->maxhp = hpv;
            p_inst->ai->d_hp = 1;
        } else {
            p_inst->ai->hp = 0;
            p_inst->ai->maxhp = 0;
            defer_kill_monster(m);
        }
    }
        
    RETURN(0);
}

int expl_enable(lua_State *LUA) {
    unsigned int inst;
    
    INITPARMCHECK(1, funcnames[DSB_ENABLE]);
    STARTNONF();
    inst = luaint(LUA, -1, funcnames[DSB_ENABLE], 1);  
    validate_instance(inst);
    ENDNONF();
    
    if (oinst[inst]->gfxflags & OF_INACTIVE) {
        if (gd.exestate == STATE_DUNGEONLOAD)
            recursive_enable_inst(inst, 1);
        else
            enable_inst(inst, 1);
    }

    RETURN(0);   
}

int expl_disable(lua_State *LUA) {
    unsigned int inst;
    
    INITPARMCHECK(1, funcnames[DSB_DISABLE]);
    STARTNONF();
    inst = luaint(LUA, -1, funcnames[DSB_DISABLE], 1);
    validate_instance(inst);
    ENDNONF();
    
    if (!(oinst[inst]->gfxflags & OF_INACTIVE)) {
        if (gd.exestate == STATE_DUNGEONLOAD)
            recursive_enable_inst(inst, 0);
        else
            enable_inst(inst, 0);
    }

    RETURN(0);
}

int expl_toggle(lua_State *LUA) {
    unsigned int inst;
    int i;
    
    INITPARMCHECK(1, funcnames[DSB_TOGGLE]);
    STARTNONF();
    inst = luaint(LUA, -1, funcnames[DSB_TOGGLE], 1);
    validate_instance(inst);
    ENDNONF();
    
    i = !!(oinst[inst]->gfxflags & OF_INACTIVE);
    
    if (gd.exestate == STATE_DUNGEONLOAD)
        recursive_enable_inst(inst, i);
    else
        enable_inst(inst, i);

    RETURN(0);   
}

int expl_msg_chain(lua_State *LUA) {
    unsigned int base_id;
    unsigned int fwd_to;
    unsigned int chain_reps = 0;
    
    INITVPARMCHECK(2, funcnames[DSB_MSG_CHAIN]);
    
    STARTNONF();
    if (gd.in_handler) {
        DSBLerror(LUA, "Adding chains not allowed in a msg_handler");
        RETURN(0);
    }
    ENDNONF();
    
    // forget it. you're doing something stupid.
    if (gd.edit_mode) {
        RETURN(0);
    }
    
    if (lua_gettop(LUA) == 3) {
        chain_reps = luaint(LUA, -1, funcnames[DSB_MSG_CHAIN], 3);
        lua_pop(LUA, 1);
        if (chain_reps > 255)
            chain_reps = 255;
    }
    base_id = luaint(LUA, -2, funcnames[DSB_MSG_CHAIN], 1);
    fwd_to = luaint(LUA, -1, funcnames[DSB_MSG_CHAIN], 2);
    
    validate_instance(base_id);
    validate_instance(fwd_to);
    
    if (oinst[fwd_to]->uplink) {
        DSBLerror(LUA, "Chained inst %d already has uplink", fwd_to);
        RETURN(0);
    }
    
    msg_chain_to(base_id, fwd_to, chain_reps);
    
    RETURN(0);
}

int expl_set_gameflag(lua_State *LUA) {
    unsigned int gflag;
    
    INITPARMCHECK(1, funcnames[DSB_SET_GAMEFLAG]);
    STARTNONF();
    gflag = luaint(LUA, -1, funcnames[DSB_SET_GAMEFLAG], 1); 
    ENDNONF();
    
    gd.gameplay_flags |= gflag;
 
    RETURN(0);   
}

int expl_clear_gameflag(lua_State *LUA) {
    unsigned int gflag;
    
    INITPARMCHECK(1, funcnames[DSB_CLEAR_GAMEFLAG]);
    STARTNONF();
    gflag = luaint(LUA, -1, funcnames[DSB_CLEAR_GAMEFLAG], 1); 
    ENDNONF();
    
    gd.gameplay_flags &= ~gflag;
 
    RETURN(0);   
}

int expl_toggle_gameflag(lua_State *LUA) {
    unsigned int gflag;
    
    INITPARMCHECK(1, funcnames[DSB_TOGGLE_GAMEFLAG]);
    STARTNONF();
    gflag = luaint(LUA, -1, funcnames[DSB_TOGGLE_GAMEFLAG], 1);
    ENDNONF();
   
    gd.gameplay_flags ^= gflag;
    
    RETURN(0);   
}

int expl_set_mflag(lua_State *LUA) {
    unsigned int gflag;

    INITPARMCHECK(1, funcnames[DSB_SET_MFLAG]);
    STARTNONF();
    gflag = luaint(LUA, -1, funcnames[DSB_SET_MFLAG], 1);
    ENDNONF();

    Gmparty_flags |= gflag;

    RETURN(0);
}

int expl_clear_mflag(lua_State *LUA) {
    unsigned int gflag;

    INITPARMCHECK(1, funcnames[DSB_CLEAR_MFLAG]);
    STARTNONF();
    gflag = luaint(LUA, -1, funcnames[DSB_CLEAR_MFLAG], 1);
    ENDNONF();

    Gmparty_flags &= ~gflag;
    
    if (gflag & MPF_ENABLED)
        gd.a_party = 0;

    RETURN(0);
}

int expl_toggle_mflag(lua_State *LUA) {
    unsigned int gflag;

    INITPARMCHECK(1, funcnames[DSB_TOGGLE_MFLAG]);
    STARTNONF();
    gflag = luaint(LUA, -1, funcnames[DSB_TOGGLE_MFLAG], 1);
    ENDNONF();

    Gmparty_flags ^= gflag;
    
    if (!GMP_ENABLED())
        gd.a_party = 0;

    RETURN(0);
}

int expl_set_gfxflag(lua_State *LUA) {
    unsigned int inst, gflag;
    
    INITPARMCHECK(2, funcnames[DSB_SET_GFXFLAG]);
    STARTNONF();
    inst = luaint(LUA, -2, funcnames[DSB_SET_GFXFLAG], 1);
    gflag = luaint(LUA, -1, funcnames[DSB_SET_GFXFLAG], 2);  
    validate_instance(inst);
    ENDNONF();
    
    if (!(oinst[inst]->gfxflags & gflag))
        change_gfxflag(inst, gflag, 1);
 
    RETURN(0);   
}

int expl_clear_gfxflag(lua_State *LUA) {
    unsigned int inst, gflag;
    
    INITPARMCHECK(2, funcnames[DSB_CLEAR_GFXFLAG]);
    STARTNONF();
    inst = luaint(LUA, -2, funcnames[DSB_CLEAR_GFXFLAG], 1);
    gflag = luaint(LUA, -1, funcnames[DSB_CLEAR_GFXFLAG], 2);
    validate_instance(inst);
    ENDNONF();
    
    if (oinst[inst]->gfxflags & gflag)
        change_gfxflag(inst, gflag, 0);
 
    RETURN(0);   
}

int expl_toggle_gfxflag(lua_State *LUA) {
    unsigned int inst, gflag;
    
    INITPARMCHECK(2, funcnames[DSB_SET_GFXFLAG]);
    STARTNONF();
    inst = luaint(LUA, -2, funcnames[DSB_SET_GFXFLAG], 1);
    gflag = luaint(LUA, -1, funcnames[DSB_SET_GFXFLAG], 2);
    validate_instance(inst);
    ENDNONF();
    
    if (oinst[inst]->gfxflags & gflag) {
        change_gfxflag(inst, gflag, 0);
    } else {        
        change_gfxflag(inst, gflag, 1);
    }
    
    RETURN(0);   
}

int expl_rand(lua_State *LUA) {
    int min, max;
    int ret;
    
    INITPARMCHECK(2, funcnames[DSB_RAND]);
    min = luaint(LUA, -2, funcnames[DSB_RAND], 1);
    max = luaint(LUA, -1, funcnames[DSB_RAND], 2);
    if (UNLIKELY(min > max))
        DSBLerror(LUA, "Bad bounds for %s", funcnames[DSB_RAND]);
    
    if (UNLIKELY(min == max))
        ret = min;
    else
        ret = DSBprand(min, max);
    lua_pushinteger(LUA, ret);  
    RETURN(1);
}

int expl_rand_seed(lua_State *LUA) {
    unsigned int random_seed;
    
    INITPARMCHECK(1, funcnames[DSB_RAND_SEED]);
    random_seed = luaint(LUA, -1, funcnames[DSB_RAND], 1);
    gd.session_seed = random_seed;
    srand_to_session_seed();
    RETURN(0);
}

int expl_get_xp_multiplier(lua_State *LUA) {
    int lev;
    INITPARMCHECK(1, funcnames[DSB_GET_XPM]);
    lev = luaint(LUA, -1, funcnames[DSB_GET_XPM], 1);
    validate_level(lev);
    lua_pushinteger(LUA, dun[lev].xp_multiplier);
    RETURN(1);
}

int expl_get_charge(lua_State *LUA) {
    unsigned int inst;
    struct inst *p_inst;
    
    INITPARMCHECK(1, funcnames[DSB_GET_CHARGE]);
    inst = luaint(LUA, -1, funcnames[DSB_GET_CHARGE], 1);
    validate_instance(inst);
    p_inst = oinst[inst];
    
    lua_pushinteger(LUA, p_inst->charge);
    RETURN(1);
}

int expl_set_charge(lua_State *LUA) {
    unsigned int inst;
    int chval;
    struct inst *p_inst;
    
    INITPARMCHECK(2, funcnames[DSB_SET_CHARGE]);
    inst = luaint(LUA, -2, funcnames[DSB_SET_CHARGE], 1);    
    chval = luaint(LUA, -1, funcnames[DSB_SET_CHARGE], 2);
    validate_instance(inst);
    
    if (chval < 0 || chval > 65535) {
        DSBLerror(LUA, "Bad charge value %d", chval);
    } else {
        p_inst = oinst[inst];
        p_inst->charge = chval;
    }
    
    RETURN(0);
}

int expl_get_animtimer(lua_State *LUA) {
    unsigned int inst;
    struct inst *p_inst;
    
    INITPARMCHECK(1, funcnames[DSB_GET_ANIMTIMER]);
    inst = luaint(LUA, -1, funcnames[DSB_GET_ANIMTIMER], 1);
    validate_instance(inst);
    p_inst = oinst[inst];
    
    lua_pushinteger(LUA, p_inst->frame);
    RETURN(1);
}

int expl_set_animtimer(lua_State *LUA) {
    unsigned int inst;
    unsigned int chval;
    struct inst *p_inst;
    
    INITPARMCHECK(2, funcnames[DSB_SET_ANIMTIMER]);
    inst = luaint(LUA, -2, funcnames[DSB_SET_ANIMTIMER], 1);    
    chval = luaint(LUA, -1, funcnames[DSB_SET_ANIMTIMER], 2);
    validate_instance(inst);
    
    if (chval >= FRAMECOUNTER_MAX) {
        DSBLerror(LUA, "Bad anim timer value %d", chval);
    } else {
        p_inst = oinst[inst];
        p_inst->frame = chval;
    }
    
    RETURN(0);
}


int expl_find_arch(lua_State *LUA) {
    struct obj_arch *p_arch;
    unsigned int inst;
    int table_and_quit = 0;

    INITPARMCHECK(1, funcnames[DSB_FIND_ARCH]);
    
    if (lua_isnil(LUA, -1)) {
        table_and_quit = 1;
    } else {
        inst = luaint(LUA, -1, funcnames[DSB_FIND_ARCH], 1);
        if (inst == 0 || inst >= NUM_INST || oinst[inst] == NULL)
            table_and_quit = 1;
    }
    
    if (table_and_quit) {
        lua_newtable(LUA);
        RETURN(1);
    }
    
    lua_getglobal(LUA, "obj");
    
    p_arch = Arch(oinst[inst]->arch);
    lua_pushstring(LUA, p_arch->luaname);

    lua_gettable(LUA, -2);  
    
    RETURN(1);
}

int expl_get_coords(lua_State *LUA) {
    unsigned int inst;    
    struct inst *ptr_inst;
    
    INITPARMCHECK(1, funcnames[DSB_GET_COORDS]);
    
    inst = luaint(LUA, -1, funcnames[DSB_GET_COORDS], 1);
    
    validate_instance(inst);
    ptr_inst = oinst[inst];
        
    lua_pushinteger(LUA, ptr_inst->level);
    lua_pushinteger(LUA, ptr_inst->x);
    lua_pushinteger(LUA, ptr_inst->y);
    lua_pushinteger(LUA, ptr_inst->tile);
    
    RETURN(4);
}

int expl_get_coords_prev(lua_State *LUA) {
    unsigned int inst;    
    struct inst *ptr_inst;
    
    INITPARMCHECK(1, funcnames[DSB_GET_COORDS_PREV]);
    
    inst = luaint(LUA, -1, funcnames[DSB_GET_COORDS_PREV], 1);
    
    validate_instance(inst);
    ptr_inst = oinst[inst];
        
    lua_pushinteger(LUA, ptr_inst->prev.level);
    lua_pushinteger(LUA, ptr_inst->prev.x);
    lua_pushinteger(LUA, ptr_inst->prev.y);
    lua_pushinteger(LUA, ptr_inst->prev.tile);
    
    RETURN(4);
}


int expl_move(lua_State *LUA) {
    unsigned int inst;
    int lev, xx, yy, dir;
    struct inst *p_inst;
    int notmoved = 0;
    int oldlev = 0;
   
    INITPARMCHECK(5, funcnames[DSB_MOVE]);
    
    STARTNONF();
    inst = luaint(LUA, -5, funcnames[DSB_MOVE], 1);
    lev = luaint(LUA, -4, funcnames[DSB_MOVE], 2);
    xx = luaint(LUA, -3, funcnames[DSB_MOVE], 3);
    yy = luaint(LUA, -2, funcnames[DSB_MOVE], 4);
    dir = luaint(LUA, -1, funcnames[DSB_MOVE], 5);
    ENDNONF();
    
    validate_instance(inst);
    validate_objcoord(funcnames[DSB_MOVE], lev, xx, yy, dir);
        
    p_inst = oinst[inst];
        
    if (lev == LOC_PARTY) {
        xx = gd.party[xx];
        lev = LOC_CHARACTER;
    }
    
    if (lev == LOC_CHARACTER) {
        struct champion *me = &(gd.champs[xx-1]);
        // don't move into a full spot
        if (me->inv[yy] && !(me->inv_queue_data[yy] & IQD_VACATED)) {
            RETURN(1);
        } else if (me->inv_queue_data[yy] & IQD_FILLED) {
            RETURN(1);
        }
    }
    
    oldlev = p_inst->level;
    
    notmoved = (p_inst->level == lev && p_inst->y == yy && p_inst->x == xx);
    p_inst->gfxflags &= ~(G_UNMOVED);
    
    if (gd.watch_inst == inst)
        gd.watch_inst_out_of_position++;
    
    if (gd.queue_rc && ((inst == gd.always_queue_inst) || (oldlev >= 0 || lev >= 0))) {
        if (notmoved)
            instmd_queue(INSTMD_RELOCATE, lev, 0, 0, dir, inst, 0);
        else
            instmd_queue(INSTMD_INST_MOVE, lev, xx, yy, dir, inst, 0);
            
    } else if (gd.queue_inv_rc && 
        (p_inst->level == LOC_CHARACTER || lev == LOC_CHARACTER))
    {
        inv_instmd_queue(INSTMD_INST_MOVE, lev, xx, yy, dir, inst, 0);
        
    } else {
        if (notmoved)
            inst_relocate(inst, dir);
        else {

            if (p_inst->level == LOC_PARCONSPC) {
                gd.lua_nonfatal = 1;
                DSBLerror(LUA, "%s: Cannot move exview inst", funcnames[DSB_MOVE]);
                RETURN(0);
            }
            
            if (!in_limbo(inst))
                limbo_instance(inst);

            if (lev != LOC_LIMBO)
                place_instance(inst, lev, xx, yy, dir, 0);
        }
    }

    RETURN(0);
}

int expl_qswap(lua_State *LUA) {
    unsigned int inst;
    unsigned int targ_arch;
    
    INITPARMCHECK(2, funcnames[DSB_QSWAP]);
    
    STARTNONF();
    inst = luaint(LUA, -2, funcnames[DSB_QSWAP], 1);
    targ_arch = luaobjarch(LUA, -1, funcnames[DSB_QSWAP]);   
    validate_instance(inst);
    ENDNONF();
    
    oinst[inst]->arch = targ_arch;
    
    if (oinst[inst]->ai) {
        reset_monster_ext(oinst[inst]);
    }
    
    lua_pushinteger(LUA, inst);

    RETURN(1);
}

int expl_get_gfxflag(lua_State *LUA) {
    unsigned int inst, gflag;
    
    INITPARMCHECK(2, funcnames[DSB_GET_GFXFLAG]);
    STARTNONF();
    inst = luaint(LUA, -2, funcnames[DSB_GET_GFXFLAG], 1);
    gflag = luaint(LUA, -1, funcnames[DSB_GET_GFXFLAG], 2);  
    validate_instance(inst);
    ENDNONF();
    
    if (oinst[inst]->gfxflags & gflag)
        lua_pushinteger(LUA, 1);
    else
        lua_pushnil(LUA);
 
    RETURN(1);   
}

int expl_get_cell(lua_State *LUA) {
    int lev, xx, yy;
    int dir = 0;
    int wall;
    
    INITPARMCHECK(3, funcnames[DSB_GET_CELL]);

    lev = luaint(LUA, -3, funcnames[DSB_GET_CELL], 1);
    xx = luaint(LUA, -2, funcnames[DSB_GET_CELL], 2);
    yy = luaint(LUA, -1, funcnames[DSB_GET_CELL], 3);
    
    validate_level(lev);
    if (xx < 0 || xx >= dun[lev].xsiz) {
        PUSH_TRUE_DONE:
        lua_pushboolean(LUA, 1);
        RETURN(1);
    }

    if (yy < 0 || yy >= dun[lev].ysiz) {
        goto PUSH_TRUE_DONE;
    }
    
    wall = (dun[lev].t[yy][xx].w & 1);

    if (wall)
        lua_pushboolean(LUA, 1);
    else
        lua_pushboolean(LUA, 0);
    
    RETURN(1);    
}

int expl_set_cell(lua_State *LUA) {
    int lev, xx, yy;
    int dir = 0;
    int wall;
    unsigned int cellstate;

    INITPARMCHECK(4, funcnames[DSB_SET_CELL]);

    lev = luaint(LUA, -4, funcnames[DSB_SET_CELL], 1);
    xx = luaint(LUA, -3, funcnames[DSB_SET_CELL], 2);
    yy = luaint(LUA, -2, funcnames[DSB_SET_CELL], 3);

    // accept either Lua true/false or integer 1/0 as input
    if (lua_isboolean(LUA, -1) && !lua_isnil(LUA, -1))
        cellstate = lua_toboolean(LUA, -1);
    else
        cellstate = luaint(LUA, -1, funcnames[DSB_SET_CELL], 4);

    validate_coord(funcnames[DSB_SET_CELL], lev, xx, yy, dir);

    if (cellstate)
        dun[lev].t[yy][xx].w |= 1;
    else
        dun[lev].t[yy][xx].w &= ~(1);

    RETURN(0);
}

int expl_get_facedir(lua_State *LUA) {
    unsigned int inst;
    struct inst *ptr_inst;
    
    INITPARMCHECK(1, funcnames[DSB_GET_FACEDIR]);
    
    inst = luaint(LUA, -1, funcnames[DSB_GET_FACEDIR], 1);
    validate_instance(inst);
    
    ptr_inst = oinst[inst];
    lua_pushinteger(LUA, ptr_inst->facedir);
    RETURN(1);
}

int expl_set_facedir(lua_State *LUA) {
    unsigned int inst;
    struct inst *ptr_inst;
    struct obj_arch *ptr_arch;
    int facedir;
    
    // is this ever something useful?
    // we can add an optional param if it is...
    int throw_size2_error = 0;
    
    INITPARMCHECK(2, funcnames[DSB_SET_FACEDIR]); 
    
    inst = luaint(LUA, -2, funcnames[DSB_SET_FACEDIR], 1);
    facedir = luaint(LUA, -1, funcnames[DSB_SET_FACEDIR], 2);
    validate_instance(inst);
    validate_facedir(facedir);
    
    ptr_inst = oinst[inst];
    ptr_arch = Arch(ptr_inst->arch);
    
    // trying to rotate double width monsters is trickier
    if (ptr_arch->type == OBJTYPE_MONSTER &&
        ptr_arch->msize == 2 && ptr_inst->level >=0 &&
        ptr_inst->tile != DIR_CENTER)
    {
        int i_sdir = check_slave_subtile(facedir, ptr_inst->tile);
        
        // the monster is being spun into an unsupported position, so
        // let's instead move its subtile unless we're explicitly told
        // to throw an error
        if (i_sdir < 0) {
            if (throw_size2_error || gd.edit_mode) {
                gd.lua_nonfatal = 1;
                DSBLerror(LUA, "Invalid slave tile[%d] (inst %d pos %d dir %d targdir %d)",
                    i_sdir, inst, ptr_inst->tile, ptr_inst->facedir, facedir);
                goto SKIP_IT_ALL;
            } else {
                mon_rotate2wide(inst, NULL, facedir, 1, 0);
                ptr_inst->ai->sface = 0;
                goto SKIP_IT_ALL;
            }
        }
            
        depointerize(inst);
        ptr_inst->facedir = facedir;
        pointerize(inst);
        
        ptr_inst->ai->sface = 0;
        
    } else {
        ptr_inst->facedir = facedir;
        if (ptr_inst->ai)
            ptr_inst->ai->sface = 0;
    }
    
    SKIP_IT_ALL:
    RETURN(0);
}

int expl_valid_inst(lua_State *LUA) {
    unsigned int inst;
    
    INITPARMCHECK(1, funcnames[DSB_VALID_INST]); 
    
    inst = luaint(LUA, -1, funcnames[DSB_VALID_INST], 1); 
    if (inst < 1 || inst > NUM_INST)
        lua_pushnil(LUA);
    else {
        if (oinst[inst])
            lua_pushboolean(LUA, 1);
        else
            lua_pushboolean(LUA, 0);
    }
 
    RETURN(1);       
}

int expl_get_crop(lua_State *LUA) {
    unsigned int inst;
    struct inst *p_inst;
    
    INITPARMCHECK(1, funcnames[DSB_GET_CROP]);
    inst = luaint(LUA, -1, funcnames[DSB_GET_CROP], 1);
    validate_instance(inst);
    p_inst = oinst[inst];
    
    lua_pushinteger(LUA, p_inst->crop);
    RETURN(1);
}

int expl_get_crmotion(lua_State *LUA) {
    unsigned int inst;
    struct inst *p_inst;
    
    INITPARMCHECK(1, funcnames[DSB_GET_CRMOTION]);
    inst = luaint(LUA, -1, funcnames[DSB_GET_CRMOTION], 1);
    validate_instance(inst);
    p_inst = oinst[inst];
    
    lua_pushinteger(LUA, p_inst->crmotion);
    RETURN(1);
}

int expl_set_crop(lua_State *LUA) {
    unsigned int inst;
    int crval;
    struct inst *p_inst;
    struct obj_arch *p_arch;
    int b = 0;
  
    INITVPARMCHECK(2, funcnames[DSB_SET_CROP]);
    
    if (lua_gettop(LUA) == 3)
        b = -1;
        
    inst = luaint(LUA, -2+b, funcnames[DSB_SET_CROP], 1);
    crval = luaint(LUA, -1+b, funcnames[DSB_SET_CROP], 2);
    validate_instance(inst);
    if (b == -1) {
        //old_cr_m_val = luaint(LUA, b, funcnames[DSB_SET_CROP], 3);
        // now this is ignored
    }
    
    p_inst = oinst[inst];
    p_arch = Arch(p_inst->arch);

    if (crval < 0 || crval > p_arch->cropmax) {
        DSBLerror(LUA, "Bad crop value %d", crval);
    } else {
        p_inst->crop = crval;
    }
    
    RETURN(0);
}

int expl_set_crmotion(lua_State *LUA) {
    unsigned int inst;
    int crmval;
    struct inst *p_inst;

    INITPARMCHECK(2, funcnames[DSB_SET_CRMOTION]);
    
    inst = luaint(LUA, -2, funcnames[DSB_SET_CRMOTION], 1);
    crmval = luaint(LUA, -1, funcnames[DSB_SET_CRMOTION], 2);
    validate_instance(inst);
        
    if (crmval < -32 || crmval > 32) {
        DSBLerror(LUA, "Bad crop motion value %d", crmval);
    } else {
        p_inst = oinst[inst];
        p_inst->crmotion = crmval;
    }
    
    RETURN(0);
}

int expl_delete(lua_State *LUA) {
    unsigned int inst;

    INITPARMCHECK(1, funcnames[DSB_DELETE]);
    
    STARTNONF();
    inst = luaint(LUA, -1, funcnames[DSB_DELETE], 1); 
    validate_instance(inst);
    ENDNONF();
    
    queued_inst_destroy(inst);
    
    RETURN(0);   
}

int expl_dmtextconvert(lua_State *LUA) {
    const char *str;
    int idx;
    int tidx;
    char mbuffer[5000];
    
    INITPARMCHECK(1, funcnames[DSB_DMTEXTCONVERT]);   
    
    STARTNONF();
    str = luastring(LUA, -1, funcnames[DSB_DMTEXTCONVERT], 1);
    ENDNONF();
    
    idx = 0;
    tidx = 0;
    memset(mbuffer, 0, sizeof(mbuffer));
    while (str[idx] != '\0') {
        char c = str[idx];
        
        if (c == ']')
            c = '/';
        
        if (c == '_') {
            char n = str[++idx];
            
            if (n == '\0') break;
            
            if (n == 'C') {
                mbuffer[tidx++] = 'T';
                mbuffer[tidx++] = 'H';
                mbuffer[tidx++] = 'E';
            } else if (n == 'D') {
                mbuffer[tidx++] = 'Y';
                mbuffer[tidx++] = 'O';
                mbuffer[tidx++] = 'U';
            }
            c = ' ';
        }
        
        mbuffer[tidx++] = c;
        idx++;
    }
    lua_pop(LUA, 1);
    lua_pushstring(LUA, mbuffer);
    RETURN(1);
}

int expl_numbertextconvert(lua_State *LUA) {
    const char *str;
    const char *curstr;
    int convlen;
    int numconv;
    int i;
    int *results;
    
    INITPARMCHECK(3, funcnames[DSB_NUMBERTEXTCONVERT]);
    
    str = luastring(LUA, -3, funcnames[DSB_NUMBERTEXTCONVERT], 1);
    numconv = luaint(LUA, -2, funcnames[DSB_NUMBERTEXTCONVERT], 2);
    convlen = luaint(LUA, -1, funcnames[DSB_NUMBERTEXTCONVERT], 3);
    
    results = dsbcalloc(numconv, sizeof(int));
    curstr = str;
    for(i=0;i<numconv;++i) {
        results[i] = decode_character_value(curstr, convlen);
        curstr = curstr + convlen;
    }
    
    luastacksize(4);
    lua_newtable(LUA);
    for(i=0;i<numconv;++i) {
        lua_pushinteger(LUA, i+1);
        lua_pushinteger(LUA, results[i]);
        lua_settable(LUA, -3);
    } 
    dsbfree(results);
    RETURN(1);
}

int expl_forward(lua_State *LUA) {
    int facedir;
    int dx = 0;
    int dy = 0;
       
    INITPARMCHECK(1, funcnames[DSB_FORWARD]);
    facedir = luaint(LUA, -1, funcnames[DSB_FORWARD], 1);
    
    validate_facedir(facedir);
    face2delta(facedir, &dx, &dy);
    
    lua_pushinteger(LUA, dx);
    lua_pushinteger(LUA, dy);
    RETURN(2);
}

int expl_tileshift(lua_State *LUA) {
    int in_tile;
    int shift_by;
    int ret = 0;
    
    INITPARMCHECK(2, funcnames[DSB_TILESHIFT]);
    in_tile = luaint(LUA, -2, funcnames[DSB_TILESHIFT], 1);
    shift_by = luaint(LUA, -1, funcnames[DSB_TILESHIFT], 2);
    validate_facedir(in_tile);
    validate_facedir(shift_by);    
    ret = tileshift(in_tile, shift_by, NULL);
    lua_pushinteger(LUA, ret);
    RETURN(1);
}

int expl_linesplit(lua_State *LUA) {
    const char *splitline;
    const char *splitter;
    char *d_sl;
    char *p_sl;
    char *b_sl;
    int init = 0;
    int cnt = 0;
    char sc;
    
    INITPARMCHECK(2, funcnames[DSB_LINESPLIT]);
    splitline = luastring(LUA, -2, funcnames[DSB_LINESPLIT], 1);
    splitter = luastring(LUA, -1, funcnames[DSB_LINESPLIT], 2);
    sc = *splitter;
    
    d_sl = dsbstrdup(splitline);
    p_sl = b_sl = d_sl;
    lua_pop(LUA, 1);
    
    while (*p_sl != '\0') {
        if (*p_sl == sc) {
            *p_sl = '\0';
            if (!init) {
                lua_newtable(LUA);
                init = 1;
            }
            ++cnt;
            lua_pushinteger(LUA, cnt);
            lua_pushstring(LUA, b_sl);    
            lua_settable(LUA, -3);
            b_sl = p_sl+1;
        }
        ++p_sl;        
    }
        
    if (!init) {
        if (d_sl == '\0') {
            lua_pushnil(LUA);
            lua_pushnil(LUA);
            RETURN(2);
        } else
            lua_newtable(LUA);
    }    
    ++cnt;
    lua_pushinteger(LUA, cnt);
    lua_pushstring(LUA, b_sl);
    lua_settable(LUA, -3);
    lua_pushinteger(LUA, cnt);
    
    dsbfree(d_sl);
    
    RETURN(2);
}

int expl_get_xp(lua_State *LUA) {
    unsigned int cnum;
    int wclass, subc;
    int val;

    INITPARMCHECK(3, funcnames[DSB_GET_XP]);

    STARTNONF();
    cnum = luaint(LUA, -3, funcnames[DSB_GET_XP], 1);
    wclass = luaint(LUA, -2, funcnames[DSB_GET_XP], 2);
    subc = luaint(LUA, -1, funcnames[DSB_GET_XP], 3);

    validate_champion(cnum);
    validate_xpclass(wclass);
    validate_xpsub(wclass, subc);
    ENDNONF();

    --cnum;
    if (subc)
        val = gd.champs[cnum].sub_xp[wclass][subc-1];
    else
       val = gd.champs[cnum].xp[wclass];
       
    lua_pushinteger(LUA, val);

    RETURN(1);
}

int expl_set_xp(lua_State *LUA) {
    unsigned int cnum;
    int wclass, subc;
    int val;

    INITPARMCHECK(4, funcnames[DSB_SET_XP]);

    STARTNONF();
    cnum = luaint(LUA, -4, funcnames[DSB_SET_XP], 1);
    wclass = luaint(LUA, -3, funcnames[DSB_SET_XP], 2);
    subc = luaint(LUA, -2, funcnames[DSB_SET_XP], 3);
    val = luaint(LUA, -1, funcnames[DSB_SET_XP], 4);

    validate_champion(cnum);
    validate_xpclass(wclass);
    validate_xpsub(wclass, subc);
    if (val < 0)
        DSBLerror(LUA, "Invalid xp value %d", val);
        
    ENDNONF();
        
    --cnum;
    if (subc)
        gd.champs[cnum].sub_xp[wclass][subc-1] = val;
    else
       gd.champs[cnum].xp[wclass] = val;
       
    RETURN(0);
}

int expl_tileptr_rotate(lua_State *LUA) {
    int lev, x, y, pos;
    int b = 0;
    int inst = 0;

    INITVPARMCHECK(4, funcnames[DSB_TILEPTR_ROTATE]);
    
    if (lua_gettop(LUA) == 5) {
        STARTNONF();
        inst = luaint(LUA, -1, funcnames[DSB_TILEPTR_ROTATE], 5);
        ENDNONF();
        
        validate_instance(inst);
        b = -1;
    }
    
    STARTNONF();
    lev = luaint(LUA, -4+b, funcnames[DSB_TILEPTR_ROTATE], 1);
    x = luaint(LUA, -3+b, funcnames[DSB_TILEPTR_ROTATE], 2);
    y = luaint(LUA, -2+b, funcnames[DSB_TILEPTR_ROTATE], 3);
    pos = luaint(LUA, -1+b, funcnames[DSB_TILEPTR_ROTATE], 4);
    ENDNONF();
    
    validate_coord(funcnames[DSB_TILEPTR_ROTATE], lev, x, y, pos);
   
    if (gd.queue_rc)
        instmd_queue(INSTMD_TROTATE, lev, x, y, pos, inst, 0);
    else
        tile_actuator_rotate(lev, x, y, pos, inst);
    
    RETURN(0);
}


