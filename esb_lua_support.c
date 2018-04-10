#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "callstack.h"
#include "objects.h"
#include "arch.h"
#include "lua_objects.h"
#include "esb_lua_support.h"

extern struct inst *oinst[];
extern lua_State *LUA;

unsigned int wscomp(int lev, int x, int y, int t) {
    return (t | (y << 3) | (x << 12) | (lev << 20));      
}

void esb_set_level_wallset(lua_State *LUA, int lev_on, const char *ws_name) {
    onstack("esb_set_level_wallset");
    
    luastacksize(4);
    lua_getglobal(LUA, "_ESBWALLSET_NAMES");
    lua_pushinteger(LUA, lev_on);
    lua_pushstring(LUA, ws_name);
    lua_settable(LUA, -3);
    lua_pop(LUA, 1);
    
    lua_getglobal(LUA, "_ESB_WALLSETS_USED");
    lua_pushboolean(LUA, 1);
    lua_setfield(LUA, -2, ws_name);
    lua_pop(LUA, 1);
    
    VOIDRETURN();
}

// ESB maxes: 1024 256x256 levels
void esb_set_loc_wallset(lua_State *LUA,
    int lev, int x, int y, int t, const char *ws_name)
{
    unsigned int comb;
    onstack("esb_set_loc_wallset");
    
    comb = wscomp(lev, x, y, t);

    luastacksize(4);
    lua_getglobal(LUA, "_ESBLEVELUSE_WALLSETS");
    lua_pushinteger(LUA, comb);
    lua_pushstring(LUA, ws_name);
    lua_settable(LUA, -3);
    lua_pop(LUA, 1);
    
    lua_getglobal(LUA, "_ESB_WALLSETS_USED");
    lua_pushboolean(LUA, 1);
    lua_setfield(LUA, -2, ws_name);
    lua_pop(LUA, 1);
    
    VOIDRETURN();
}

void esb_clear_used_wallset_table(void) {
    onstack("esb_clear_used_wallset_table");
    
    luastacksize(3);
    lua_newtable(LUA);
    lua_setglobal(LUA, "_ESB_WALLSETS_USED");
    
    VOIDRETURN();    
}

void enable_inst(unsigned int inst, int onoff) {
    struct inst *p_inst;
    
    onstack("enable_inst");
    
    p_inst = oinst[inst];
    
    if (onoff == 0) {
        p_inst->gfxflags |= OF_INACTIVE;
    } else if (onoff > 0) {
        p_inst->gfxflags &= ~OF_INACTIVE;
    }
    
    VOIDRETURN();
}
    

void change_gfxflag(int inst, int gflag, int operation) {
    struct inst *p_inst = oinst[inst];
    if (operation)
        p_inst->gfxflags |= gflag;
    else
        p_inst->gfxflags &= ~gflag;
}

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
     
    depointerize(inst);
    p_inst->tile = destp;
    pointerize(inst);

    RETURN(0);
}

