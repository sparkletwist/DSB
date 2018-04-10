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
#include "common_macros.h"

extern FILE *errorlog;
extern lua_State *LUA;
extern struct global_data gd;
extern struct dungeon_level *dun;
extern struct inst *oinst[NUM_INST];
//extern struct graphics_control gfxctl;
extern struct inventory_info gii;

void validate_xplevel(int what) {
    if (what < 0 || what > MAX_LEVELS)
        DSBLerror(LUA, "Invalid level %d", what);
}

void validate_xpclass(int what) {
    if (what < 0 || what > gd.max_class)
        DSBLerror(LUA, "Invalid class %d", what);
}

void validate_xpsub(int base, int what) {
    if (what < 0 || what > gd.max_subsk[base])
        DSBLerror(LUA, "Invalid subskill %d", what);
}

void validate_coord(char *func, int lev, int xx, int yy, int facedir) {

    if (facedir < 0 || facedir > 4)
        DSBLerror(LUA, "%s: Invalid direction %d", func, facedir);
        
    if (lev < 0 || lev >= gd.dungeon_levels)
        DSBLerror(LUA, "%s: Invalid level %d", func, lev);
        
    if (xx < 0 || xx >= dun[lev].xsiz)
        DSBLerror(LUA, "%s: Bad x location: %d", func, xx);
        
    if (yy < 0 || yy >= dun[lev].ysiz)
        DSBLerror(LUA, "%s: Bad y location: %d", func, yy);

}

void validate_level(int n) {
    if (n < 0 || n >= gd.dungeon_levels)
        DSBLerror(LUA, "Invalid level %d", n);
}

void validate_tilepos(int tp) {
    if (tp < 0 || tp > 3)
        DSBLerror(LUA, "Invalid tileposition %d", tp);
}

void validate_tilepos5(int tp) {
    if (tp < 0 || tp > 4)
        DSBLerror(LUA, "Invalid tileposition %d", tp);
}

void validate_facedir(int tp) {
    if (tp < 0 || tp > 3)
        DSBLerror(LUA, "Invalid facing %d", tp);
}

void validate_instance(unsigned int inst_id) {
    if (inst_id == 0 || inst_id >= NUM_INST || oinst[inst_id] == NULL)
        DSBLerror(LUA, "Invalid instance %d", inst_id);
}

int validate_champion(unsigned int who) {
if (who < 1 || who > gd.num_champs) {
        DSBLerror(LUA, "Invalid character index %d", who);
        return 1;
    }
        
    if (gd.champs[who-1].name == NULL) {
        DSBLerror(LUA, "Invalid character %d", who);
        return 1;
    }
        
    return 0;
}

void validate_stat(int what) {
    if (what < 0 || what > MAX_STATS)
        DSBLerror(LUA, "Invalid stat %d", what);
}

void validate_injloc(int what) {
    if (what < 0 || what > gii.max_injury)
        DSBLerror(LUA, "Invalid injury location %d", what);
}

void validate_barstat(unsigned int what) {
    if (what < 0 || what > MAX_BARS)
        DSBLerror(LUA, "Invalid barstat %d", what);
}
void validate_ppos(int xx) {
    if (xx < 0 || xx > 3)
        DSBLerror(LUA, "Invalid party position %d", xx);
}

void validate_dtype(int xx) {
    if (xx < 0 || xx >= gd.max_dtypes)
        DSBLerror(LUA, "Invalid dmgtype %d", xx);
}

void validate_lightindex(int l) {
    if (l < 0 || l >= MAX_LIGHTS)
        DSBLerror(LUA, "Invalid lightindex %d", l);
}

void validate_special(char *func, int lev, int xx, int yy) {

    int charidx = xx;

    if (lev < LOC_LIMBO)
        DSBLerror(LUA, "%s: Invalid Special parameter %d", func, lev);

    if (lev == LOC_PARTY) {
        validate_ppos(xx);
        charidx = gd.party[xx];
    }

    if (lev == LOC_PARTY || lev == LOC_CHARACTER) {
        if (charidx < 1 || charidx > gd.num_champs)
            DSBLerror(LUA, "%s: Invalid character index %d", func, charidx);

        if (yy < -1 || yy >= gii.max_invslots)
            DSBLerror(LUA, "%s: Invalid inventory index %d", func, yy);
    }

    if (lev == LOC_IN_OBJ)
        validate_instance(xx);
            
}

void validate_objcoord(char *func, int lev, int xx, int yy, int facedir) {
    
    onstack("validate_objcoord");

    if (lev < 0)
        validate_special(func, lev, xx, yy);
    else
        validate_coord(func, lev, xx, yy, facedir);
        
    VOIDRETURN();
}

void validate_objlevcoord(char *func, int lev, int xx, int yy, int facedir) {

    onstack("validate_objlevcoord");

    if (lev < 0)
        validate_special(func, lev, xx, yy);
    else {
        validate_level(lev);
        if (facedir < 0 || facedir > 4)
            DSBLerror(LUA, "%s: Invalid direction %d", func, facedir);
    }

    VOIDRETURN();
}

void parmerror(int pr, const char *fn) {
    DSBLerror(LUA, "%s requires %d parameter%s (not %d)",
        fn, pr, (pr==1)?" ":"s ", lua_gettop(LUA));
}

void varparmerror(int pr, int pr2, const char *fn) {
    DSBLerror(LUA, "%s requires %d to %d parameters", fn, pr, pr2);
}

void varorparmerror(int pr, int pr2, const char *fn) {
    DSBLerror(LUA, "%s requires %d or %d parameters", fn, pr, pr2);
}

int C_only_when_loading(lua_State *LUA, char *fname) {
    onstack("only_when_loading");
    
    if (gd.exestate == STATE_LOADING)
        RETURN(0);
    
    gd.lua_nonfatal++;
    DSBLerror(LUA, "%s can only be used in startup scripts", fname);
    RETURN(1);
}

int C_only_when_load_or_obj(lua_State *LUA, char *fname) {
    onstack("only_when_load_or_obj");

    if (gd.exestate == STATE_LOADING)
        RETURN(0);
        
    if (gd.exestate == STATE_CUSTOMPARSE)
        RETURN(0);

    gd.lua_nonfatal++;
    DSBLerror(LUA, "%s can only be used in initialization files", fname);
    RETURN(1);
}

int C_only_when_objparsing(lua_State *LUA, char *fname) {
    onstack("only_when_objparsing");

    if (gd.exestate == STATE_OBJPARSE)
        RETURN(0);

    gd.lua_nonfatal++;
    DSBLerror(LUA, "%s can only be used in objects files", fname);
    RETURN(1);
}

int C_only_when_dungeon(lua_State *LUA, char *fname) {
    onstack("only_when_dungeon");

    if (gd.exestate == STATE_DUNGEONLOAD)
        RETURN(0);

    gd.lua_nonfatal++;
    DSBLerror(LUA, "%s can only be used in dungeon definitions", fname);
    RETURN(1);
}

void int_on_table(int idx, int val) {
    lua_pushinteger(LUA, idx);
    lua_pushinteger(LUA, val);
    lua_settable(LUA, -3);  
}

void rtableval(int dir, int it) {     
    if (it) {
        if (dir) {
            lua_newtable(LUA);
            int_on_table(1, it);
        } else
            lua_pushinteger(LUA, it);
    } else
        lua_pushnil(LUA); 
}

void objtable(int ot, int *ins, int z) {
    if (ot) {
        if (!(*ins)) {
            lua_newtable(LUA);
            *ins = 1;
        }
                    
        int_on_table(z, ot);
    }
}

void recursive_enable_inst(unsigned int inst, int onoff) {
    enable_inst(inst, onoff); 
    if (oinst[inst]->chaintarg) {
        struct inst_loc *ct = oinst[inst]->chaintarg;
        while (ct) {
            recursive_enable_inst(ct->i, onoff);
            ct = ct->n;
        }    
    }
}

