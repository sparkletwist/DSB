#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "lproto.h"
#include "uproto.h"
#include "objects.h"
#include "champions.h"
#include "global_data.h"

extern unsigned int *level_table;
extern struct global_data gd;
extern struct graphics_control gfxctl;

extern lua_State *LUA;

int getidx(int n) {
    int rv = 0;
    
    lua_pushinteger(LUA, n);
    lua_gettable(LUA, -2);
    rv = lua_tointeger(LUA, -1);
    lua_pop(LUA, 1);
    
    return rv;
}

struct animap *grabsysbmp(char *name) {
    struct animap *rv = NULL;
    
    lua_pushstring(LUA, name);
    lua_gettable(LUA, -2);   
    rv = luabitmap(LUA, -1, name, 0);
    lua_pop(LUA, 1);
    
    return rv;
}

int grabsysstruct(int coords_only, struct iidesc *i, const char *sname, int idx) {

    onstack("grabsysstruct");
    
    luastacksize(5);
    
    if (sname)
        lua_pushstring(LUA, sname);
    else
        lua_pushinteger(LUA, idx);
    lua_gettable(LUA, -2);
    
    if (!lua_istable(LUA, -1)) {
        lua_pop(LUA, 1);
        RETURN(0);
    }
 
    lua_pushstring(LUA, "x");
    lua_gettable(LUA, -2);
    i->x = lua_tointeger(LUA, -1);
    lua_pop(LUA, 1);
    
    lua_pushstring(LUA, "y");
    lua_gettable(LUA, -2);
    i->y = lua_tointeger(LUA, -1);
    lua_pop(LUA, 1);
    
    if (!coords_only) {
        lua_pushstring(LUA, "icon");
        lua_gettable(LUA, -2);
        if (!lua_isnil(LUA, -1)) {
            i->img = luabitmap(LUA, -1, "icon", 0);
        } else {
            i->img = NULL;
        }
              
        lua_pop(LUA, 1);
          
        if (sname != NULL)
            lua_pushstring(LUA, "look_icon");
        else
            lua_pushstring(LUA, "hurt_icon");
        lua_gettable(LUA, -2);
        if (!lua_isnil(LUA, -1)) {
            // not actually looking for "icon_ex" in the table
            // the name is specified above
            i->img_ex = luabitmap(LUA, -1, "icon_ex", 0); 
        } else
            i->img_ex = NULL;
        lua_pop(LUA, 1);
    }
     
    lua_pop(LUA, 1);
    lstacktop();
    
    RETURN(1);
}

struct animap *grab_from_gfxtable(const char *nam) {
    int tid;
    struct animap *lbmp;
    
    onstack("grab_from_gfxtable");
    
    lua_getglobal(LUA, "gfx");
    lua_pushstring(LUA, nam);
    lua_gettable(LUA, -2);
    if (!lua_istable(LUA, -1)) goto TABLE_BMP_ERROR;
    lua_pushstring(LUA, "type");
    lua_gettable(LUA, -2);
    if (!lua_isnumber(LUA, -1)) goto TABLE_BMP_ERROR;
    tid = lua_tointeger(LUA, -1);
    lua_pop(LUA, 1);
    if (tid != DSB_LUA_BITMAP) goto TABLE_BMP_ERROR;

    lua_pushstring(LUA, "ref");
    lua_gettable(LUA, -2);
    lbmp = (struct animap*)lua_touserdata(LUA, -1);
    lua_pop(LUA, 3);

    RETURN(lbmp);

    TABLE_BMP_ERROR:
    {
        char oopsmsg[64];
        snprintf(oopsmsg, sizeof(oopsmsg), "Invalid graphic %s%s", "gfx.", nam);
        poop_out(oopsmsg);
    }
    RETURN(NULL);
}

void importcolortable(char *tname, unsigned int *where, int num, struct animap **bmp_ptr) {
    int i;
    
    onstack("importcolortable");
    
    lua_getglobal(LUA, tname);
    
    if (!lua_istable(LUA, -1)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Cannot find %s", tname);
        poop_out(buf);
        VOIDRETURN();
    }
    
    for (i=1;i<=num;++i) {
        int bmp_got = 0;
        int r, g, b;
        int j;
        
        lua_pushinteger(LUA, i);
        lua_gettable(LUA, -2);
        
        if (bmp_ptr) {
            struct animap *loaded = luaoptbitmap(LUA, -1, tname);           
            bmp_ptr[i-1] = loaded;
            if (loaded) {
                where[i-1] = 0;
                bmp_got = 1;    
            }
        }
        
        if (!bmp_got) {    
            r = getidx(1);
            g = getidx(2);
            b = getidx(3);     
            where[i-1] = makecol(r, g, b);
        }
        
        lua_pop(LUA, 1);
    }
    lua_pop(LUA, 1);
    
    VOIDRETURN();   
}

void import_core_colors(void) {

    importcolortable("player_colors", gd.playercol, 4, gd.playerbitmap);
    importcolortable("name_draw_colors", gd.name_draw_col, 4, NULL);

    lua_getglobal(LUA, "system_color");
    if (!lua_istable(LUA, -1))
        poop_out("Cannot find system_color Lua table");
    else {
        int r, g, b;

        r = getidx(1);
        g = getidx(2);
        b = getidx(3);

        gd.sys_col = makecol(r, g, b);
    }
    lua_pop(LUA, 1);
}

void import_core_strings(void) {
    int i;

    lua_getglobal(LUA, "barnames");
    if (!lua_istable(LUA, -1))
        poop_out("Cannot find barnames Lua table");

    for(i=0;i<3;++i) {
        const char *vstr;

        lua_pushinteger(LUA, (i+1));
        lua_gettable(LUA, -2);

        vstr = lua_tostring(LUA, -1);
        condfree(gd.barname[i]);
        if (vstr != NULL)
            gd.barname[i] = dsbstrdup(vstr);
        else
            gd.barname[i] = dsbstrdup("BAR");

        lua_pop(LUA, 1);
    }
    lua_pop(LUA, 1);
}

void destroy_dynalloc_constants(void) {
    int i;
    
    onstack("destroy_dynalloc_constants");
    
    dsbfree(gd.max_subsk);
    
    gd.max_class = 0;
    gd.max_subsk = NULL;
    
    VOIDRETURN();
}

void import_lua_constants(void) {
    int i;
    int lastz = 0;
    
    onstack("import_lua_constants");
    
    gd.max_class = luacharglobal("xp_classes");
    gd.max_subsk = dsbcalloc(gd.max_class, sizeof(char));

    gd.t_cpl = 18;
    gd.t_maxlines = 4;
    gd.t_ypl = 14;
    
    import_core_colors();
    
    update_all_inventory_info();
  
    import_core_strings();
    
    gd.max_lvl = luacharglobal("xp_levels");
    
    level_table = dsbcalloc(gd.max_lvl, sizeof(unsigned int));
  
    lua_getglobal(LUA, "xp_subskills");
    if (!lua_istable(LUA, -1))
        poop_out("Cannot find xp_subskills");
        
    for (i=1;i<=gd.max_class;i++) {
        int usk = getidx(i);

        if (usk <= 0) usk = 1;
        else if (usk > 254) usk = 254;
        
        gd.max_subsk[i-1] = usk;
    }
    lua_pop(LUA, 1);
    
    lua_getglobal(LUA, "xp_levelamounts");
    if (!lua_istable(LUA, -1))
        poop_out("Cannot find xp_levelamounts Lua table");
    for(i=0;i<gd.max_lvl;++i) {
        int z;
        
        lua_pushinteger(LUA, (i+1));
        lua_gettable(LUA, -2);
        z = lua_tointeger(LUA, -1);
        if (z > lastz) {
            level_table[i] = z;
            lastz = z;
        } else
            poop_out("xp_levelamounts: Bad value");
            
        lua_pop(LUA, 1);
    }
    lua_pop(LUA, 1);
    
    gd.max_nstat = 6;
    
    VOIDRETURN();
}

void import_interface_constants(void) {
    onstack("import_interface_constants");
    
    luastacksize(8);  
    lc_parm_int("__gui_importation", 0);
    
    VOIDRETURN();
}

void flush_guy_icons(void) {
    int ip;
    onstack("flush_guy_icons");
    for (ip=0;ip<4;++ip) {
        if (gd.simg.guy[ip] != NULL) {
            destroy_bitmap(gd.simg.guy[ip]);
            gd.simg.guy[ip] = NULL;
        }
    }
    VOIDRETURN();
}

void update_all_system(void) {
    import_core_strings();
    import_core_colors();
    flush_guy_icons();
}
