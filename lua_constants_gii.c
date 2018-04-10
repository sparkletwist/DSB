#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "arch.h"
#include "lproto.h"
#include "uproto.h"
#include "objects.h"
#include "champions.h"
#include "global_data.h"

#define TABLE_INTEGER(G, S, D) \
    lua_pushstring(LUA, S);\
    lua_gettable(LUA, -2);\
    if (lua_isnumber(LUA, -1))\
        G = lua_tointeger(LUA, -1);\
    else\
        G = D;\
    lua_pop(LUA, 1)

extern unsigned int *level_table;
extern struct global_data gd;
extern struct inventory_info gii;

extern lua_State *LUA;

void update_all_inventory_info(void) {
    int n;
    int max_invslots = 0;
    int max_injury = 0;
    int b_injbreak = 0;
    struct iidesc tii[256];
    struct iidesc tmpi;
    char *s_i = "inventory_info";

    onstack("update_all_inventory_info");

    luastacksize(4);

    lua_getglobal(LUA, s_i);
    if (!lua_istable(LUA, -1))
        poop_out("Cannot find inventory_info Lua table");
     
    gii.main_background = getluaoptbitmap(s_i, "main_background");   
    gii.inventory_background = getluaoptbitmap(s_i, "background");
    
    gii.topscr[TOP_HANDS] = getluaoptbitmap(s_i, "top_hands");
    gii.topscr[TOP_PORT] = getluaoptbitmap(s_i, "top_port");
    gii.topscr[TOP_DEAD] = getluaoptbitmap(s_i, "top_dead");
    
    gii.sel_box = getluaoptbitmap(s_i, "selection_box");
    gii.boost_box = getluaoptbitmap(s_i, "boost_box");
    gii.hurt_box = getluaoptbitmap(s_i, "hurt_box");
        
    gd.tmpstr = s_i;
    grabsysstruct(0, &(gii.eye), "eye", 0);
    grabsysstruct(0, &(gii.mouth), "mouth", 0);
    grabsysstruct(0, &(gii.save), "save", 0);
    grabsysstruct(0, &(gii.sleep), "sleep", 0);
    grabsysstruct(0, &(gii.exit), "exitbox", 0);
    
    grabsysstruct(1, &tmpi, "subrenderer", 0);
    gii.srx = tmpi.x;
    gii.sry = tmpi.y;
    
    TABLE_INTEGER(gii.stat_minimum, "stat_minimum", 0);
    TABLE_INTEGER(gii.stat_maximum, "stat_maximum", 9990);
    
    for (n=0;n<256;++n) {
        int r = grabsysstruct(0, &(tii[n]), NULL, n);
        if (!r) break;
        ++max_invslots;

        if (!b_injbreak && tii[n].img_ex)
            ++max_injury;
        else
            ++b_injbreak;
    }

    if (UNLIKELY(!max_invslots)) {
        poop_out("No inventory slots defined");
        VOIDRETURN();
    }

    if (gii.filled) {
        if (max_invslots != gii.max_invslots) {
            poop_out("Cannot resize inventory table");
            VOIDRETURN();
        }
    } else {
        gii.inv = dsbmalloc(sizeof(struct iidesc) * max_invslots);
        gii.filled = 1;
    }

    memcpy(gii.inv, tii, sizeof(struct iidesc) * max_invslots);

    gii.max_invslots = max_invslots;
    gii.max_injury = max_injury;

    memset(gii.lslot, 0, sizeof(int) * 4);
    memset(gii.rslot, 0, sizeof(int) * 4);
    lua_pushstring(LUA, "top_row");
    lua_gettable(LUA, -2);
    if (lua_istable(LUA, -1)) {
        int t;
        for (t=0;t<4;++t) {
            lua_pushinteger(LUA, t+1);
            lua_gettable(LUA, -2);
            if (lua_istable(LUA, -1)) {
                gii.lslot[t] = getidx(1);
                gii.rslot[t] = getidx(2);
            }
            lua_pop(LUA, 1);
        }
    }
    lua_pop(LUA, 1);

    gd.tmpstr = NULL;
    lua_pop(LUA, 1);

    VOIDRETURN();
}
