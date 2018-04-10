#include <stdio.h>
#include <stdarg.h>
#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "uproto.h"
#include "objects.h"
#include "lua_objects.h"
#include "global_data.h"
#include "champions.h"

#define TABLE_STORE_INT(I) \
    lua_pushinteger(LUA, cur_idx);\
    lua_pushinteger(LUA, I);\
    lua_settable(LUA, -3);\
    cur_idx++
    
#define TABLE_STORE_DESTROY_STRING(S) \
    lua_pushinteger(LUA, cur_idx);\
    lua_pushstring(LUA, S);\
    lua_settable(LUA, -3);\
    cur_idx++;\
    dsbfree(S)

struct exports *ex_l = NULL;

extern struct global_data gd;
extern lua_State *LUA;

struct export_information {
    int active;
    
    char *port_name;
    char *name;
    char *lastname;
    
    int bar[MAX_BARS];
    int stat[MAX_STATS];
    
    int xp[8];
    int subxp[8][8];
    
    char *unarmed;
};
struct export_information Exported[4];

int is_game_exportable(void) {
    onstack("is_game_exportable");
    
    if (gd.leader == -1) {
        RETURN(0);
    }
    
    luastacksize(2);
    lua_getglobal(LUA, "TARGET_FILENAME");
    if (lua_isstring(LUA, -1)) {
        lua_pop(LUA, 1);
        RETURN(1);
    }
    
    RETURN(0);    
}

void store_exported_character_data(void) {
    int pp;
    
    onstack("store_exported_character_data");
    
    for(pp=0;pp<4;++pp) {
        
        Exported[pp].active = 0;
        
        if (gd.party[pp]) {
            int j;
            int who = gd.party[pp]-1;
            struct champion *me = &(gd.champs[who]);
            
            // prevent stat boosting items from carrying over
            inv_pickup_all(who+1);
            
            Exported[pp].active = 1;
            Exported[pp].port_name = dsbstrdup(me->port_name);
            Exported[pp].name = dsbstrdup(me->name);
            Exported[pp].lastname = dsbstrdup(me->lastname);

            for(j=0;j<MAX_BARS;++j) {
                Exported[pp].bar[j] = me->maxbar[j];
            }
                
            for(j=0;j<MAX_STATS;++j) {
                Exported[pp].stat[j] = me->maxstat[j];
            }
                
            for(j=0;j<gd.max_class;++j) {
                int sub;
                Exported[pp].xp[j] = me->xp[j];
                for(sub=0;sub<gd.max_subsk[j];sub++) {
                    Exported[pp].subxp[j][sub] = me->sub_xp[j][sub];
                }
            }

            Exported[pp].unarmed = dsbstrdup(me->method_name);
        }    
    }
    
    VOIDRETURN();    
}

void retrieve_exported_character_data(void) {
    int pp;
    onstack("retrieve_exported_character_data");
    
    luastacksize(6);
    lua_getglobal(LUA, "__rebuild_exported_characters");
    if (lua_isnil(LUA, -1)) {
        poop_out("Exported character rebuilder not found");
        VOIDRETURN();
    }
    
    for(pp=0;pp<4;++pp) {
        if (Exported[pp].active) {
            int j;
            struct export_information *x = &(Exported[pp]);
            int cur_idx = 1;
            
            lua_newtable(LUA);
            
            TABLE_STORE_DESTROY_STRING(x->port_name);
            TABLE_STORE_DESTROY_STRING(x->name);
            TABLE_STORE_DESTROY_STRING(x->lastname);
            
            for(j=0;j<MAX_BARS;++j) {
                TABLE_STORE_INT(x->bar[j]);
            }
                
            for(j=0;j<MAX_STATS;++j) {
                TABLE_STORE_INT(x->stat[j]);
            }
            
            for(j=0;j<gd.max_class;++j) {
                TABLE_STORE_INT(0);
            }            
            
            TABLE_STORE_DESTROY_STRING(x->unarmed);
            
        } else {
            lua_pushnil(LUA);
        }
    }
    lc_call_topstack(4, "rebuild_exported_characters");

    // unpack xp
    lua_getglobal(LUA, "__NEW_CHAMPIONS");
    for(pp=0;pp<4;++pp) {
        if (Exported[pp].active) {
            int j;
            int who;  
            struct champion *me;
            
            lua_pushinteger(LUA, pp+1);
            lua_gettable(LUA, -2);
            who = lua_tointeger(LUA, -1) - 1;
            lua_pop(LUA, 1);
            
            me = &(gd.champs[who]);
            for(j=0;j<gd.max_class;++j) {
                int sub;
                me->xp[j] = Exported[pp].xp[j];
                for(sub=0;sub<gd.max_subsk[j];sub++) {
                    me->sub_xp[j][sub] = Exported[pp].subxp[j][sub];
                }
            }            
        }
    }
    lua_pop(LUA, 1);
    
    VOIDRETURN();
}

void load_export_target(void) {
    int i,j,pp;

    onstack("load_export_target");
    
    gd.exestate = STATE_DUNGEONLOAD; 
    destroy_dungeon();
    load_dungeon("TARGET_FILENAME", "COMPILED_TARGET_FILENAME");
    for(pp=0;pp<4;++pp) {
        for(i=0;i<2;i++) {
            for(j=0;j<2;j++) {
                gd.guypos[i][j][pp] = 0;
            }
        }
    }
    if (gd.queue_rc != 0)
        program_puke("instmd queue during exported load");
        
    retrieve_exported_character_data();  
    
    gd.exestate = STATE_GAMEPLAY;
    lc_parm_int("sys_game_export", 0);
    show_load_ready(1);
    
    VOIDRETURN();    
}

void add_export_gvar(char *s_vname) {
    struct exports *n_x;
    
    onstack("add_export_gvar");
    
    n_x = dsbmalloc(sizeof(struct exports));
    n_x->s = s_vname;
    n_x->n = ex_l;
    ex_l = n_x; 
    
    lua_getglobal(LUA, s_vname);
    if (lua_isnil(LUA, -1)) {
        lua_pushinteger(LUA, 0);
        lua_setglobal(LUA, s_vname);
    }
    lua_pop(LUA, 1);    
    
    VOIDRETURN();
}


