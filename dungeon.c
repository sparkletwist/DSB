#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "objects.h"
#include "champions.h"
#include "lua_objects.h"
#include "global_data.h"
#include "uproto.h"
#include "timer_events.h"
#include "editor_shared.h"
#include "gamelock.h"
#include "istack.h"

extern struct global_data gd;
extern lua_State *LUA;

extern int ws_tbl_len;

extern struct timer_event *te;
extern struct timer_event *a_te;

extern struct inst *oinst[NUM_INST];
struct dungeon_level *dun;
extern unsigned int cwsa_num;
extern struct wallset *cwsa;
extern int Gmparty_flags;

extern FILE *errorlog;

void import_shading_info(void) {
    lc_parm_int("__import_shading_info", 0);
}

void party_enter_level(int ap, int lev) {
    onstack("party_enter_level");
    
    // visited, so now it's active
    dun[lev].level_flags &= ~(DLF_START_INACTIVE);
    
    lc_parm_int("sys_enter_level", 1, lev);
    
    if (ap == gd.a_party) {
        party_viewto_level(lev);
    }
    
    VOIDRETURN();
}

void party_viewto_level(int lev) {
    onstack("party_viewto_level");

    lc_parm_int("sys_viewto_level", 1, lev);
    import_shading_info();

    VOIDRETURN();
}

int check_for_wall(int lev, int x, int y) {
    struct dungeon_level *dd;
    dd = &(dun[lev]);
    if (x < 0 || x >= dd->xsiz)
        return -1;
    if (y < 0 || y >= dd->ysiz)
        return -1;
    return (dd->t[y][x].w & 1);
}

/*
int check_for_mons_wall(int lev, int x, int y) {
    int wv;
    struct dungeon_level *dd;
    int scratch;
    
    dd = &(dun[lev]);
    if (x < 0 || x >= dd->xsiz)
        return -1;
    if (y < 0 || y >= dd->ysiz)
        return -1;
        
    wv = dd->t[y][x].w;
    scratch = dd->t[y][x].SCRATCH;
    // a fake wall the party has gone through-- disbelieved!
    if (!(wv & 1) && (wv & 2) && (scratch & 4))
        return 0;
      
    return (wv & ~(2));
}

int check_for_app_wall(int lev, int x, int y) {
    struct dungeon_level *dd;
    int scratch4;
    
    dd = &(dun[lev]);
    if (x < 0 || x >= dd->xsiz)
        return -1;
    if (y < 0 || y >= dd->ysiz)
        return -1;
        
    scratch4 = ((dd->t[y][x].SCRATCH) & 4);
    return ((dd->t[y][x].w & 1) | scratch4);
}
*/

void textmap2level(lua_State *LUA, int lev,
    int xs, int ys, int lite, int xpm)
{
	int i, xx, yy;
	struct dungeon_level *dd;

    onstack("textmap2level");
    
    if (!gd.edit_mode && ws_tbl_len == 0) {
        DSBLerror(LUA, "Cannot create level %d with no wallsets defined", lev);
        VOIDRETURN();
    }
    
    if (lev+1 > gd.dungeon_levels) {
        int sizdif = ((lev+1) - gd.dungeon_levels);

        dun = dsbrealloc(dun, (sizeof(struct dungeon_level) * (lev+1)));
        memset(&(dun[gd.dungeon_levels]), 0,
            sizeof(struct dungeon_level) * sizdif);

        gd.dungeon_levels = (lev+1);
        luastacksize(3);
        lua_pushinteger(LUA, lev);
        lua_setglobal(LUA, "ESB_LAST_LEVEL");

    } else {
        if (dun[lev].xsiz > 0) {
            DSBLerror(LUA, "Level %d already exists", lev);
        }
    }

    dd = &(dun[lev]);
    dd->tint_intensity = DSB_DEFAULT_TINT_INTENSITY;
    
    dd->t = dsbcalloc(ys, sizeof(struct dtile*));
    for(i=0;i<ys;++i)
        dd->t[i] = dsbcalloc(xs, sizeof(struct dtile));

    dd->xsiz = xs;
    dd->ysiz = ys;
    dd->tint = 0;
    
    if (lua_istable(LUA, -1)) {
        for (yy=0;yy<ys;++yy) {
            const char *cl;
    
            lua_pushinteger(LUA, yy+1);
            lua_gettable(LUA, -2);
            if (!lua_isstring(LUA, -1))
                DSBLerror(LUA, "Invalid table row %d", yy+1);
            cl = lua_tostring(LUA, -1);
            
            for(xx=0;xx<xs;++xx) {
                char c = cl[xx];
                
                if (c == '\0')
                    DSBLerror(LUA, "Invalid table column %d", xx);
    
                if (c == '1')
                    dd->t[yy][xx].w = 0;
                else
                    dd->t[yy][xx].w = 1;
            }
            
            lua_pop(LUA, 1);
        }
    }

    dd->lightlevel = lite;
    dd->xp_multiplier = xpm;
    dd->wall_num = cwsa_num;
    
    VOIDRETURN();
}

void pixmap2level(int lev, const char *dfilename, int lite, int xpm) {
	int i, xx, yy;
    BITMAP *m_map;
	struct dungeon_level *dd;
	int cc = 0;
	
	onstack("pixmap2level");
	
    if (!gd.edit_mode && ws_tbl_len == 0) {
        DSBLerror(LUA, "Cannot create level %s with no wallsets defined", dfilename);
        VOIDRETURN();
    }

	set_color_conversion(COLORCONV_NONE);    	
    m_map = load_bitmap(fixname(dfilename), NULL);
    if (m_map == NULL) {
        DSBLerror(LUA, "Invalid bitmap %s", dfilename);
        VOIDRETURN();
    }
    
    if (bitmap_color_depth(m_map) != 8) {
        DSBLerror(LUA, "%s is not a 256 color bitmap", dfilename);
        VOIDRETURN();
    }
     
    if (lev+1 > gd.dungeon_levels) {
        int sizdif = ((lev+1) - gd.dungeon_levels);
        
        dun = dsbrealloc(dun, (sizeof(struct dungeon_level) * (lev+1)));
        memset(&(dun[gd.dungeon_levels]), 0,
            sizeof(struct dungeon_level) * sizdif);
            
        gd.dungeon_levels = (lev+1);
        luastacksize(3);
        lua_pushinteger(LUA, lev);
        lua_setglobal(LUA, "ESB_LAST_LEVEL");
        
    } else {

        if (dun[lev].xsiz > 0) {
            DSBLerror(LUA, "Level %d already exists", lev);
        }
        
    }
     
    dd = &(dun[lev]);
    dd->tint_intensity = DSB_DEFAULT_TINT_INTENSITY;
    
    dd->t = dsbcalloc(m_map->h, sizeof(struct dtile*));
    for(i=0;i<m_map->h;++i)
        dd->t[i] = dsbcalloc(m_map->w, sizeof(struct dtile));
        
    dd->xsiz = m_map->w;
    dd->ysiz = m_map->h;
    dd->tint = 0;
    
    cc = setup_colortable_convert();
      
    // at some point i could replace this with hyper optimized code
    // that i myself scarcely understand
    for (yy=0;yy<m_map->h;++yy) {
        for(xx=0;xx<m_map->w;++xx) {
            unsigned char t;
            t = 0;
            t = _getpixel(m_map, xx, yy);
            
            if (t == 255)
                t = 1;
            else if (cc && t > 1)
                t = colortable_convert(m_map, lev, xx, yy, t);
            
            if (!t)
                dd->t[yy][xx].w = 1;
            else
                dd->t[yy][xx].w = 0;
        }
    }
    
    if (cc)
        finish_colortable_convert(); 
    
    dd->lightlevel = lite;
    dd->xp_multiplier = xpm;
    dd->wall_num = cwsa_num;
    
    destroy_bitmap(m_map);
	set_color_conversion(COLORCONV_KEEP_ALPHA);
	
	VOIDRETURN();
}

void destroy_champion(int t) {
    onstack("destroy_champion");
    
    // already invalidated
    if (gd.champs[t].method_name == NULL)
        VOIDRETURN();
        
    if (gd.champs[t].exinst) {
        int vinst = gd.champs[t].exinst;
        oinst[vinst]->level = LOC_LIMBO;
    }
    
    if (gd.champs[t].method)
        destroy_attack_methods(gd.champs[t].method);
        
    destroy_lua_exvars("ch_exvar", t+1);
    
    condfree(gd.champs[t].condition);
    condfree(gd.champs[t].cond_animtimer);
    
    condfree(gd.champs[t].port_name);
    condfree(gd.champs[t].hand_name);
    condfree(gd.champs[t].invscr_name);
    
    condfree(gd.champs[t].top_hands_name);
    condfree(gd.champs[t].top_port_name);
    condfree(gd.champs[t].top_dead_name);
    
    destroy_champion_xp_memory(&(gd.champs[t]));
    destroy_champion_invslots(&(gd.champs[t]));
    
    condfree(gd.champs[t].method_name);
    gd.champs[t].method_name = NULL;
    
    VOIDRETURN();
}

void destroy_dungeon_level(struct dungeon_level *dd) {
    int i;
    
    onstack("destroy_dungeon_level");
    
    for(i=0;i<dd->ysiz;++i) {
        int x;
        for (x=0;x<dd->xsiz;++x) {
            int dir;
            for (dir=0;dir<MAX_DIR;++dir) {
                if (dd->t[i][x].il[dir])
                    destroy_inst_loc_list(dd->t[i][x].il[dir], 0, 0);
            }
        }
        
        dsbfree(dd->t[i]);
    }
    
    dsbfree(dd->t);
        
    VOIDRETURN();    
}

void destroy_all_dungeon_levels() {
    int l;
    
    onstack("destroy_all_dungeon_levels");

    for (l=0;l<gd.dungeon_levels;++l) {
        struct dungeon_level *dd;
        dd = &(dun[l]);        
        destroy_dungeon_level(dd);
    }

    dsbfree(dun);
    dun = NULL;
    
    gd.dungeon_levels = 0;
    lua_pushnil(LUA);
    lua_setglobal(LUA, "ESB_LAST_LEVEL");
    
    VOIDRETURN();
}

void destroy_dungeon(void) {
    int t;
    
    onstack("destroy_dungeon");  
    
    for(t=0;t<gd.num_champs;++t)
        destroy_champion(t);
    for (t=0;t<4;++t)
        gd.party[t] = 0;
    gd.leader = -1;
    gd.offering_ppos = -1;
    
    flush_all_ist();

    v_onstack("destroy_dungeon.champions");
    dsbfree(gd.champs);
    gd.champs = NULL;
    v_upstack();
    
    v_onstack("destroy_dungeon.attack_methods");
    dsbfree(gd.attack_msg);
    clear_method();
    v_upstack();
    
    v_onstack("destroy_dungeon.global_data");
    gd.num_champs = 0;
    gd.attack_msg_ttl = 0;
    gd.num_methods = 0;
    gd.everyone_dead = 0;
    gd.mouse_mode = gd.mouse_guy = 0;
    gd.mouseobj = 0;
    gd.move_lock = 0;
    gd.forbid_move_timer = 0;
    gd.lua_error_found = 0;
    gd.gameplay_flags = 0;
    Gmparty_flags = 0;
    if (gd.gl)
        memset(gd.gl, 0, sizeof(struct gamelock));
    v_upstack();
    
    v_onstack("destroy_dungeon.partycond");
    condfree(gd.partycond);
    condfree(gd.partycond_animtimer);
    gd.partycond = NULL;
    gd.partycond_animtimer = NULL;
    v_upstack();
    
    // clear all runes
    v_onstack("destroy_dungeon.runes");
    memset(gd.i_spell, 0, 32);
    v_upstack();
    
    v_onstack("destroy_dungeon.graphics");
    destroy_alt_targ();
    destroy_viewports();
    
    destroy_all_subrenderers();
    destroy_console();
    destroy_all_sound_handles();
    destroy_all_music_handles();
    v_upstack();
    
    v_onstack("destroy_dungeon.inst_table");
    for(t=0;t<NUM_INST;++t) {
        if (oinst[t]) {
            inst_destroy(t, 2);
        }   
    }
    gd.last_alloc = 0;
    memset(oinst, 0, sizeof(struct inst*)*NUM_INST);
    v_upstack();

    destroy_all_timers();
    destroy_all_dungeon_levels();
    
    VOIDRETURN();
}
