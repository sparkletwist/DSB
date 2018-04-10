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
#include "render.h"
#include "champions.h"
#include "gproto.h"
#include "timer_events.h"
#include "global_data.h"
#include "sound.h"
#include "sound_fmod.h"
#include "monster.h"
#include "arch.h"
#include "compile.h"
#include "editor_shared.h"
#include "spawnburst.h"
#include "fullscreen.h"
#include "istack.h"
#include "mparty.h"
#include "viewscale.h"
#include "common_macros.h"
#include "common_lua.h"

int ZID;

unsigned int i_s_error_c = 0;

extern int ws_tbl_len;
extern struct wallset **ws_tbl;

extern struct global_data gd;
extern struct graphics_control gfxctl;
extern struct inventory_info gii;

extern FILE *errorlog;
extern struct dungeon_level *dun;
extern unsigned int cwsa_num;
extern struct wallset *cwsa;
extern struct wallset *haze_base;

extern struct inst *oinst[NUM_INST];

extern struct condition_desc *i_conds;
extern struct condition_desc *p_conds;

extern lua_State *LUA;

extern int grcnt;
extern int viewstate;
extern int debug;

const char *Lcstr = "Lua";
const char *ESBCNAME = "_ESBCNAME";
const char *DSB_MasterSoundTable = "_DSBMasterSoundTable";

extern int lua_cz_n;
extern struct clickzone *lua_cz;

extern const char *DSBMTGC;
extern istack istaparty;
extern int Gmparty_flags;
extern int PPis_here[];

extern char *funcnames[];

int disallow_anitarg(struct animap *an) {
    if (an->numframes > 1) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "Cannot blit onto animated Bitmap");
        return 1;
    }
    
    return 0;
}

void init_lua_soundtable(void) {
    lua_newtable(LUA);
    lua_setglobal(LUA, DSB_MasterSoundTable);
}

void setup_lua_bitmap(lua_State *LUA, struct animap *lua_bitmap) {
    int t;
    
    if (lua_bitmap->flags & AM_VIRTUAL) {
        t = DSB_LUA_VIRTUAL_BITMAP;
    } else {
        t = DSB_LUA_BITMAP;
    }

    lua_newtable(LUA);
    
    lua_pushstring(LUA, "type");
    lua_pushinteger(LUA, t);
    lua_settable(LUA, -3);
        
    lua_pushstring(LUA, "ref");
    lua_pushlightuserdata(LUA, (void*)lua_bitmap);
    lua_settable(LUA, -3);
    
    if (t != DSB_LUA_VIRTUAL_BITMAP) {
        void *npr;
        lua_pushstring(LUA, "ud_ref");
        npr = lua_newuserdata(LUA, sizeof(struct animap*) + sizeof(int));
        memcpy(npr, &lua_bitmap, sizeof(struct animap*));
        memcpy(npr + sizeof(struct animap*), &t, sizeof(int));
        luaL_getmetatable(LUA, DSBMTGC);
        lua_setmetatable(LUA, -2);
        lua_settable(LUA, -3);
    }
    
    if (lua_bitmap->numframes <= 1) {
        if (lua_bitmap->b) {
            lua_bitmap->w = lua_bitmap->b->w;
            lua_bitmap->numframes = 1;
        }
    }
}

void store_in_master_by_id(const char *unique_id, FMOD_SOUND *lua_sound) {
    
    onstack("store_in_master_by_id");
    
    lua_getglobal(LUA, DSB_MasterSoundTable);
    lua_pushstring(LUA, unique_id);
    lua_pushlightuserdata(LUA, (void*)lua_sound);
    lua_settable(LUA, -3);
    lua_pop(LUA, 1);
    
    if (debug)
        fprintf(errorlog, "SOUND LOAD: Stored id %s in MasterSoundTable\n", unique_id);
    
    VOIDRETURN();
}

char *hash_and_cache(lua_State *LUA, FMOD_SOUND *lua_sound, const char *name) {
    static char unique_id[16];
    char *pt = (char*)name;
    int hash1 = 0;
    int hash2 = 0;
    int pos = 0;
    
    onstack("hash_and_cache");
    
    memset(unique_id, 0, 16);
    
    while (*pt != '\0') {
        hash1 = (hash1 + *pt) % 999;
        hash2 = (hash2 + pos + *pt) % 999;
        ++pt;
        ++pos;
    }
    
    snprintf(unique_id, sizeof(unique_id) - 1, "%03d%03d%s",
        hash1, hash2, name);
        
    store_in_master_by_id(unique_id, lua_sound);
    
    RETURN(unique_id);
}

void setup_lua_sound(lua_State *LUA,
    FMOD_SOUND *lua_sound, const char *name)
{
    char *unique_id;
    void *npr;
    int t;
    
    onstack("setup_lua_sound");
    
    unique_id = hash_and_cache(LUA, lua_sound, name);

    lua_newtable(LUA);
    
    t = DSB_LUA_SOUND;
    
    lua_pushstring(LUA, "type");
    lua_pushinteger(LUA, t);
    lua_settable(LUA, -3);
    
    lua_pushstring(LUA, "ref");
    lua_pushlightuserdata(LUA, (void*)lua_sound);
    lua_settable(LUA, -3);
    
    lua_pushstring(LUA, "id");
    lua_pushstring(LUA, unique_id);
    lua_settable(LUA, -3);
    
    lua_pushstring(LUA, "ud_ref");
    npr = lua_newuserdata(LUA, sizeof(FMOD_SOUND*) + sizeof(int));
    memcpy(npr, &lua_sound, sizeof(FMOD_SOUND*));
    memcpy(npr + sizeof(FMOD_SOUND*), &t, sizeof(int));
    luaL_getmetatable(LUA, DSBMTGC);
    lua_setmetatable(LUA, -2);
    lua_settable(LUA, -3);
    
    VOIDRETURN();
}

void setup_lua_font(lua_State *LUA, FONT *lua_font, int use_gc_mt) {
    void *fptr;
    void *npr;
    int t;
    
    lua_newtable(LUA);
    
    if (gd.queued_pointer) {
        t = DSB_LUA_I_FONT;
        fptr = gd.queued_pointer;
        gd.queued_pointer = 0;
    } else {
        t = DSB_LUA_FONT;
        fptr = (void*)lua_font;
    }
    
    lua_pushstring(LUA, "type");
    lua_pushinteger(LUA, t);
    lua_settable(LUA, -3);
    
    lua_pushstring(LUA, "ref");
    lua_pushlightuserdata(LUA, (void*)lua_font);
    lua_settable(LUA, -3);
    
    if (use_gc_mt) {
        lua_pushstring(LUA, "ud_ref");
        npr = lua_newuserdata(LUA, sizeof(void*) + sizeof(int));
        memcpy(npr, &fptr, sizeof(void*));
        memcpy(npr + sizeof(void*), &t, sizeof(int));
        luaL_getmetatable(LUA, DSBMTGC);
        lua_setmetatable(LUA, -2);
        lua_settable(LUA, -3);
    }
}

int expl_game_end(lua_State *LUA) {

    luaonstack(funcnames[DSB_GAME_END]);
    
    gd.stored_rcv = -3;
    
    if (lua_isboolean(LUA, -1) && lua_toboolean(LUA, -1))
        gd.ending_boring = 1;
    else
        gd.ending_boring = 0;
    
    RETURN(0);
}

int expl_include_file(lua_State *LUA) {
    int b = 0;
    const char *file;
    const char *extension;
    const char *includelongname = NULL;
    const char *sfile;
    
    INITVPARMCHECK(2, funcnames[DSB_INCLUDE_FILE]);
    only_when_loading(LUA, funcnames[DSB_INCLUDE_FILE]);
    if (lua_gettop(LUA) == 3) {
        includelongname = luastring(LUA, -1, funcnames[DSB_INCLUDE_FILE], 3);
        b = -1;
    }
    
    file = luastring(LUA, -2 + b, funcnames[DSB_INCLUDE_FILE], 1);
    extension = luastring(LUA, -1 + b, funcnames[DSB_INCLUDE_FILE], 2);
    
    sfile = make_noslash(file);
    
    if (gd.compile)
        add_iname(file, sfile, extension, includelongname);  
        
    RETURN(0);  
}

int expl_get_bitmap(lua_State *LUA) {
    const char *bmpname; 
    const char *bmplongname = NULL;
    BITMAP *lua_bitmap;
    struct animap *my_a_map;
    int b = 0;

    INITVPARMCHECK(1, funcnames[DSB_GET_BITMAP]);
    if (lua_gettop(LUA) == 2) {
        bmplongname = luastring(LUA, -1, funcnames[DSB_GET_BITMAP], 2); 
        b = -1;    
    }
    bmpname = luastring(LUA, -1 + b, funcnames[DSB_GET_BITMAP], 1);
    only_when_load_or_obj(LUA, funcnames[DSB_GET_BITMAP]);
    
    if (gd.file_lod) {
        my_a_map = prepare_lod_animap(bmpname, bmplongname);
        /* DEBUG
        my_a_map->b = create_bitmap(1, 1);
        my_a_map->w = 1;
        my_a_map->numframes = 1;
        // */
    } else {
        lua_bitmap = pcxload(bmpname, bmplongname);
        my_a_map = prepare_animap_from_bmp(lua_bitmap, gd.loaded_tga);
        gd.loaded_tga = 0;
    }
    setup_lua_bitmap(LUA, my_a_map);
    
    //fprintf(errorlog, "Loaded bitmap %d from %s\n", my_a_map, bmpname);
       
    RETURN(1);
}

int expl_clone_bitmap(lua_State *lua) {
    struct animap *my_a_map;
    struct animap *base_a_map;

    INITPARMCHECK(1, funcnames[DSB_CLONE_BITMAP]);
    base_a_map = luabitmap(LUA, -1, funcnames[DSB_CLONE_BITMAP], 1);
    
    lod_use(base_a_map);
    my_a_map = dsbmalloc(sizeof(struct animap));
    memset(my_a_map, 0, sizeof(struct animap));

    my_a_map->flags = base_a_map->flags;
    my_a_map->flags |= AM_VIRTUAL;
    
    // the old way was not actually "safe" with
    // respect to how unions are written up
    if (my_a_map->flags & AM_256COLOR) {
        my_a_map->pal = base_a_map->pal;
    } else {
        my_a_map->vtype = base_a_map->vtype;
    }   
    
    my_a_map->b = base_a_map->b;

    if (base_a_map->cloneof)
        my_a_map->cloneof = base_a_map->cloneof;
    else
        my_a_map->cloneof = base_a_map;
        
    base_a_map->ext_references += 1;
    base_a_map->clone_references += 1;
    
    setup_lua_bitmap(LUA, my_a_map);
    RETURN(1);
}

int expl_get_mask_bitmap(lua_State *LUA) {
    const char *bmpname; 
    const char *bmplongname = NULL;
    BITMAP *lua_bitmap;
    struct animap *my_a_map;
    int b = 0;
    RGB t_pal[256];
    
    INITVPARMCHECK(1, funcnames[DSB_GET_MASK_BITMAP]);  
    if (lua_gettop(LUA) == 2) {
        bmplongname = luastring(LUA, -1, funcnames[DSB_GET_MASK_BITMAP], 2);
        b = -1;  
    }
    bmpname = luastring(LUA, -1 + b, funcnames[DSB_GET_MASK_BITMAP], 1);
    only_when_load_or_obj(LUA, funcnames[DSB_GET_MASK_BITMAP]);
     
    lua_bitmap = pcxload8(bmpname, bmplongname, t_pal);
    gd.loaded_tga = 0;
    
    my_a_map = dsbmalloc(sizeof(struct animap));
    memset(my_a_map, 0, sizeof(struct animap));
    my_a_map->b = lua_bitmap;    
    setup_lua_bitmap(LUA, my_a_map);
    my_a_map->flags = AM_256COLOR;    
    my_a_map->pal = dsbcalloc(1, sizeof(RGB)*256);
    memcpy(my_a_map->pal, t_pal, sizeof(RGB)*256);       
    RETURN(1);
}

int expl_get_sound(lua_State *LUA) {
    const char *sndname; 
    const char *sndlongname = NULL;
    int b = 0;
    FMOD_SOUND *lua_sound;
    
    INITVPARMCHECK(1, funcnames[DSB_GET_SOUND]); 
    if (lua_gettop(LUA) == 2) {
        sndlongname = luastring(LUA, -1, funcnames[DSB_GET_SOUND], 2);
        b = -1;
    }  
    sndname = luastring(LUA, -1 + b, funcnames[DSB_GET_SOUND], 1);    
    only_when_load_or_obj(LUA, funcnames[DSB_GET_SOUND]);

    lua_sound = soundload(sndname, sndlongname, 1, 0, 1);
    setup_lua_sound(LUA, lua_sound, sndname);
    RETURN(1);
}

int expl_music(lua_State *LUA) {
    int sh = 0;
    int loop = 0;
    char unique_id[16];
    const char *musicname; 
    FMOD_SOUND *lua_music;
    int stop_music = 0;
    int b = 0;
    
    INITVPARMCHECK(1, funcnames[DSB_MUSIC]);
    if (lua_gettop(LUA) == 2)
        b = -1;

    if (gd.exestate < STATE_GAMEPLAY) {
        RETURN(0);
    }   
    
    if (lua_isnil(LUA, -1 + b)) {
        stop_music = 1;
    } else if (lua_isnumber(LUA, -1 + b)) {
        int ip = lua_tointeger(LUA, -1 + b);
        if (ip == AI_P_QUERY) {
            if (gd.cur_mus[0].uid) {
                lua_pushstring(LUA, gd.cur_mus[0].filename);
            } else {
                lua_pushnil(LUA);
            }
            RETURN(1);
        }
    } else {    
        musicname = luastring(LUA, -1 + b, funcnames[DSB_MUSIC], 1);    
        if (b == -1 && lua_isboolean(LUA, -1))
            loop = lua_toboolean(LUA, -1);
    }
        
    if (gd.cur_mus[0].uid) {
        stop_sound(gd.cur_mus[0].chan);
        gd.cur_mus[0].preserve = 0;
        current_music_ended(0);
    }
    
    if (stop_music) {
        RETURN(0);
    }
    
    lua_music = do_load_music(musicname, unique_id, 1);
    if (lua_music) {
        sh = play_ssound(lua_music, unique_id, loop, 0);  
        gd.cur_mus[0].chan = sh;
        gd.cur_mus[0].uid = dsbstrdup(unique_id);
        gd.cur_mus[0].filename = dsbstrdup(musicname);
        gd.cur_mus[0].preserve = 0;
        lua_pushinteger(LUA, sh);
    } else
        lua_pushnil(LUA);
    
    RETURN(1);
}

int expl_music_to_background(lua_State *LUA) {
    INITPARMCHECK(0, funcnames[DSB_MUSIC_TO_BACKGROUND]);
    
    if (gd.cur_mus[0].uid) {
        int copied = 0;
        int zi;
        for(zi=1;zi<MUSIC_HANDLES;++zi) {
            if (!gd.cur_mus[zi].uid) {
                memcpy(&(gd.cur_mus[zi]), &(gd.cur_mus[0]),
                    sizeof(struct current_music));
                memset(&(gd.cur_mus[0]), 0, sizeof(struct current_music));
                copied = 1;
                break;
            }
        }  
        if (!copied) {
            gd.lua_nonfatal++;
            DSBLerror(LUA, "%s", "No free music handle");
        }  
    }
    
    RETURN(0);
}

int expl_get_font(lua_State *LUA) {
    const char *fntname; 
    const char *fntlongname = NULL;
    FONT *lua_font;
    int b = 0;
    
    INITVPARMCHECK(1, funcnames[DSB_GET_FONT]);  
    if (lua_gettop(LUA) == 2) {
        fntlongname = luastring(LUA, -1, funcnames[DSB_GET_FONT], 2); 
        b = -1;    
    } 
    fntname = luastring(LUA, -1 + b, funcnames[DSB_GET_FONT], 1);    
    only_when_load_or_obj(LUA, funcnames[DSB_GET_FONT]);
    
    lua_font = fontload(fntname, fntlongname);
    setup_lua_font(LUA, lua_font, 1);       
    RETURN(1);
}

int expl_new_bitmap(lua_State *LUA) {
    int width, height;
    BITMAP *lua_bitmap;
    struct animap *my_a_map;
    
    INITPARMCHECK(2, funcnames[DSB_NEW_BITMAP]);
    
    width = luaint(LUA, -2, funcnames[DSB_NEW_BITMAP], 1);
    height = luaint(LUA, -1, funcnames[DSB_NEW_BITMAP], 2);

    lua_bitmap = create_bitmap(width, height);
    
    my_a_map = dsbmalloc(sizeof(struct animap));
    memset(my_a_map, 0, sizeof(struct animap));
    my_a_map->b = lua_bitmap;    
    setup_lua_bitmap(LUA, my_a_map);
    
    //fprintf(errorlog, "Created bitmap %d size %d %d\n", my_a_map, width, height);
       
    RETURN(1);
}

int expl_desktop_bitmap(lua_State *LUA) {
    struct animap *my_a_map;

    INITPARMCHECK(0, funcnames[DSB_DESKTOP_BITMAP]);
    
    my_a_map = grab_desktop();
    setup_lua_bitmap(LUA, my_a_map);
    
    RETURN(1);   
}

int expl_dungeon_view(lua_State *LUA) {
    int lev, xx, yy, facedir, tlight;

    INITPARMCHECK(5, funcnames[DSB_DUNGEON_VIEW]);

    lev = luaint(LUA, -5, funcnames[DSB_DUNGEON_VIEW], 1);
    xx = luaint(LUA, -4, funcnames[DSB_DUNGEON_VIEW], 2);
    yy = luaint(LUA, -3, funcnames[DSB_DUNGEON_VIEW], 3);
    facedir = luaint(LUA, -2, funcnames[DSB_DUNGEON_VIEW], 4);
    tlight = luaint(LUA, -1, funcnames[DSB_DUNGEON_VIEW], 5);

    validate_coord(funcnames[DSB_DUNGEON_VIEW], lev, xx, yy, facedir);
    facedir = facedir % 4;
 
    if (gfxctl.l_viewport_rtarg == NULL)
        gfxctl.l_viewport_rtarg = create_vanimap(VIEWPORTW, VIEWPORTH);
    
    define_alt_targ(lev, xx, yy, facedir, tlight);
    render_dungeon_view(1, 0);
    destroy_alt_targ();
    
    setup_lua_bitmap(LUA, gfxctl.l_viewport_rtarg);
    RETURN(1);
}

int expl_sub_targ(lua_State *LUA) {
    BITMAP *lua_bitmap;
    struct animap *my_a_map;
    struct animap **subrend_targ;
    
    INITPARMCHECK(0, funcnames[DSB_SUB_TARG]);
    
    if (gfxctl.subrend_targ != NULL) {
        subrend_targ = gfxctl.subrend_targ;
    } else {
        subrend_targ = &(gfxctl.subrend);
    }

    if (*subrend_targ)
        destroy_animap(*subrend_targ);

    lua_bitmap = create_bitmap(246, 146);
    
    my_a_map = dsbcalloc(1, sizeof(struct animap));
    my_a_map->b = lua_bitmap;
    my_a_map->flags = AM_VIRTUAL;

    setup_lua_bitmap(LUA, my_a_map);
    *subrend_targ = my_a_map;
    
    gfxctl.subrend_targ = NULL;
       
    RETURN(1);
}

int expl_destroy_bitmap(lua_State *LUA) {
    static int complaints = 0;
    void *udptr;
    struct animap *lbmp;
    
    INITPARMCHECK(1, funcnames[DSB_DESTROY_BITMAP]);
      
    lbmp = luabitmap(LUA, -1, funcnames[DSB_DESTROY_BITMAP], 1);
    
    if (lbmp->flags & AM_VIRTUAL) {
        if (complaints < 3) {
            ++complaints;
            gd.lua_nonfatal = 1;
            DSBLerror(LUA, "Cannot destroy virtual bitmap");
        }
        RETURN(0);
    }

    purge_from_lod(0, lbmp);
    destroy_animap(lbmp);
    
    lua_pushstring(LUA, "type");
    lua_pushinteger(LUA, 0);
    lua_settable(LUA, -3);

    lua_pushstring(LUA, "ud_ref");
    lua_gettable(LUA, -2);
    udptr = lua_touserdata(LUA, -1);
    memset(udptr, 0, 8);
    lua_pop(LUA, 1);
    
    RETURN(0);
}

int expl_get_pixel(lua_State *LUA) {
    int x, y;
    unsigned int r, g, b;
    unsigned int a;
    unsigned int px;
    struct animap *lbmp;  
    
    INITPARMCHECK(3, funcnames[DSB_GET_PIXEL]);
    
    lbmp = luabitmap(LUA, -3, funcnames[DSB_GET_PIXEL], 1);
    x = luaint(LUA, -2, funcnames[DSB_GET_PIXEL], 2);
    y = luaint(LUA, -1, funcnames[DSB_GET_PIXEL], 3);
    
    lod_use(lbmp);
    if (disallow_anitarg(lbmp))
        RETURN(0);
        
    px = getpixel(lbmp->b, x, y);
    r = getr(px);
    g = getg(px);
    b = getb(px);
    if (lbmp->flags & AM_HASALPHA)
        a = geta(px);
    
    lua_newtable(LUA);
    lua_pushinteger(LUA, 1);
    lua_pushinteger(LUA, r);
    lua_settable(LUA, -3);
    lua_pushinteger(LUA, 2);
    lua_pushinteger(LUA, g);
    lua_settable(LUA, -3);
    lua_pushinteger(LUA, 3);
    lua_pushinteger(LUA, b);
    lua_settable(LUA, -3);
    
    if (lbmp->flags & AM_HASALPHA)
        lua_pushinteger(LUA, a);
    else
        lua_pushnil(LUA);
    
    RETURN(2);  
}

int expl_put_pixel(lua_State *LUA) {
    int x, y;
    unsigned int px;
    struct animap *lbmp; 
    int b = 0; 
    int a = 0;
    int using_alpha = 0;
    
    INITVPARMCHECK(4, funcnames[DSB_SET_PIXEL]);
    
    if (lua_gettop(LUA) == 5) {
        b = -1;
        a = luaint(LUA, -1, funcnames[DSB_SET_PIXEL], 5);
        using_alpha = 1;
    }
    
    lbmp = luabitmap(LUA, -4 + b, funcnames[DSB_SET_PIXEL], 1);
    x = luaint(LUA, -3 + b, funcnames[DSB_SET_PIXEL], 2);
    y = luaint(LUA, -2 + b, funcnames[DSB_SET_PIXEL], 3);
    px = luargbval(LUA, -1 + b, funcnames[DSB_SET_PIXEL], 4);
    
    lod_use(lbmp);
    if (disallow_anitarg(lbmp))
        RETURN(0);
    
    if (using_alpha) {
        unsigned int rgb = px;
        px = makeacol(getr(rgb), getg(rgb), getb(rgb), a);
        lbmp->flags |= AM_HASALPHA;
    }     

    putpixel(lbmp->b, x, y, px);
    
    RETURN(0);  
}

int expl_rgb_to_hsv(lua_State *LUA) {
    unsigned int rgb;
    float h;
    float s;
    float v;

    INITPARMCHECK(1, funcnames[DSB_RGB_TO_HSV]);
    
    rgb = luargbval(LUA, -1, funcnames[DSB_RGB_TO_HSV], 1);
    
    rgb_to_hsv(getr(rgb), getg(rgb), getb(rgb), &h, &s, &v);

    lua_newtable(LUA);
    lua_pushinteger(LUA, 1);
    lua_pushnumber(LUA, h);
    lua_settable(LUA, -3);
    lua_pushinteger(LUA, 2);
    lua_pushnumber(LUA, s);
    lua_settable(LUA, -3);
    lua_pushinteger(LUA, 3);
    lua_pushnumber(LUA, v);
    lua_settable(LUA, -3);    
    
    RETURN(1);    
}

int expl_hsv_to_rgb(lua_State *LUA) {
    float h = 0.0;
    float s = 0.0;
    float v = 0.0;
    unsigned int r;
    unsigned int g;
    unsigned int b;
    
    INITPARMCHECK(1, funcnames[DSB_HSV_TO_RGB]);
    
    luahsvval(LUA, -1, funcnames[DSB_HSV_TO_RGB], 1, &h, &s, &v);
    
    hsv_to_rgb(h, s, v, &r, &g, &b);
    
    lua_newtable(LUA);
    lua_pushinteger(LUA, 1);
    lua_pushinteger(LUA, r);
    lua_settable(LUA, -3);
    lua_pushinteger(LUA, 2);
    lua_pushinteger(LUA, g);
    lua_settable(LUA, -3);
    lua_pushinteger(LUA, 3);
    lua_pushinteger(LUA, b);
    lua_settable(LUA, -3);
        
    RETURN(1);
}

int expl_bitmap_width(lua_State *LUA) {
    struct animap *lbmp;
    
    INITPARMCHECK(1, funcnames[DSB_BITMAP_WIDTH]);
    
    lbmp = luabitmap(LUA, -1, funcnames[DSB_BITMAP_WIDTH], 1);

    lod_use(lbmp);
    if (disallow_anitarg(lbmp))
        RETURN(0);    

    lua_pushinteger(LUA, lbmp->b->w);
 
    RETURN(1);
}

int expl_bitmap_height(lua_State *LUA) {
    struct animap *lbmp;
    
    INITPARMCHECK(1, funcnames[DSB_BITMAP_HEIGHT]);
    
    lbmp = luabitmap(LUA, -1, funcnames[DSB_BITMAP_HEIGHT], 1);

    lod_use(lbmp);
    if (disallow_anitarg(lbmp))
        RETURN(0);    

    lua_pushinteger(LUA, lbmp->b->h);
 
    RETURN(1);
}

int expl_bitmap_clear(lua_State *LUA) {
    struct animap *lbmp;
    unsigned int rgbv;
    
    INITPARMCHECK(2, funcnames[DSB_BITMAP_CLEAR]);
    
    lbmp = luabitmap(LUA, -2, funcnames[DSB_BITMAP_CLEAR], 1);
    rgbv = luargbval(LUA, -1, funcnames[DSB_BITMAP_CLEAR], 2);
    
    if (disallow_anitarg(lbmp))
        RETURN(0);
    
    clear_to_color(lbmp->b, rgbv);
    lbmp->flags &= ~(AM_HASALPHA);
 
    RETURN(0);
}

int expl_bitmap_blit(lua_State *LUA) {
    struct animap *src_bmp;
    struct animap *dest_bmp;
    int xsrc, ysrc, xdest, ydest, xsiz, ysiz;
    int dxsiz, dysiz;
    int b = 0;
    int scale = 0;
    
    INITRPARMCHECK(8, 10, funcnames[DSB_BITMAP_BLIT]);
    if (lua_gettop(LUA) == 10) {
        b = -2;
        scale = 1;
    }
    
    src_bmp = luabitmap(LUA, -8+b, funcnames[DSB_BITMAP_BLIT], 1);
    dest_bmp = luabitmap(LUA, -7+b, funcnames[DSB_BITMAP_BLIT], 2);
    xsrc = luaint(LUA, -6+b, funcnames[DSB_BITMAP_BLIT], 3);
    ysrc = luaint(LUA, -5+b, funcnames[DSB_BITMAP_BLIT], 4);
    xdest = luaint(LUA, -4+b, funcnames[DSB_BITMAP_BLIT], 5);
    ydest = luaint(LUA, -3+b, funcnames[DSB_BITMAP_BLIT], 6);
    xsiz = luaint(LUA, -2+b, funcnames[DSB_BITMAP_BLIT], 7);
    ysiz = luaint(LUA, -1+b, funcnames[DSB_BITMAP_BLIT], 8);
    if (scale) {
        dxsiz = luaint(LUA, -2, funcnames[DSB_BITMAP_BLIT], 9);
        dysiz = luaint(LUA, -1, funcnames[DSB_BITMAP_BLIT], 10);
    }
    
    lod_use(src_bmp);
    lod_use(dest_bmp);
    
    if (disallow_anitarg(src_bmp))
        RETURN(0);
        
    if (disallow_anitarg(dest_bmp))
        RETURN(0);
    
    if (scale) {
        DSB_aa_scale_blit(1, src_bmp->b, dest_bmp->b, xsrc, ysrc, xsiz, ysiz,
            xdest, ydest, dxsiz, dysiz);
    } else {
        blit(src_bmp->b, dest_bmp->b, xsrc, ysrc, xdest, ydest, xsiz, ysiz);
    }
    
    if (src_bmp->flags & AM_HASALPHA)
        dest_bmp->flags |= AM_HASALPHA;
    
    RETURN(0);
}

int expl_bitmap_draw(lua_State *LUA) {
    struct animap *src_bmp;
    struct animap *dest_bmp;
    struct animap fake_animap;
    int using_destparms = 0;
    BITMAP *draw_sub = NULL;    
    int xsrc, ysrc;
    int b = 0;
    int flip = 0;
    int dmode = 0;
    int xdest, ydest, xsiz, ysiz;
    int alphafix = 0;

    INITRPARMCHECK(5, 9, funcnames[DSB_BITMAP_DRAW]);
    
    if (lua_gettop(LUA) == 9) {
        using_destparms = 1;
        b = -4;
    }

    src_bmp = luabitmap(LUA, -5+b, funcnames[DSB_BITMAP_DRAW], 1);
    dest_bmp = luabitmap(LUA, -4+b, funcnames[DSB_BITMAP_DRAW], 2);
    xsrc = luaint(LUA, -3+b, funcnames[DSB_BITMAP_DRAW], 3);
    ysrc = luaint(LUA, -2+b, funcnames[DSB_BITMAP_DRAW], 4);
    if (using_destparms) {
        xdest = luaint(LUA, -5, funcnames[DSB_BITMAP_DRAW], 5);
        ydest = luaint(LUA, -4, funcnames[DSB_BITMAP_DRAW], 6);
        xsiz = luaint(LUA, -3, funcnames[DSB_BITMAP_DRAW], 7);
        ysiz = luaint(LUA, -2, funcnames[DSB_BITMAP_DRAW], 8);        
    } else {
        xdest = xsrc;
        ydest = ysrc;
    }
    
    lod_use(src_bmp);
    lod_use(dest_bmp);
    
    if (lua_isboolean(LUA, -1)) {
        int fp = lua_toboolean(LUA, -1);
        if (fp)
            flip = -1;
    } else if (lua_isnumber(LUA, -1)) {
        int dval = lua_tointeger(LUA, -1);
        if (dval == 2) flip = 2;
    }
    
    if (disallow_anitarg(dest_bmp))
        RETURN(0);
        
    if (using_destparms) {
        BITMAP *source_b = animap_subframe(src_bmp, 0);
        draw_sub = create_sub_bitmap(source_b, xsrc, ysrc, xsiz, ysiz);
        memset(&fake_animap, 0, sizeof(struct animap));
        fake_animap.numframes = 1;
        fake_animap.b = draw_sub;
        fake_animap.w = draw_sub->w;
        fake_animap.flags = (src_bmp->flags | AM_VIRTUAL);
        src_bmp = &fake_animap;
    }
    
    if (dest_bmp->flags & AM_HASALPHA)
        alphafix = 1;

    draw_gfxicon_af(dest_bmp->b, src_bmp, xdest, ydest, flip, NULL, alphafix);
    
    if (draw_sub)
        destroy_bitmap(draw_sub);

    RETURN(0);
}

int expl_bitmap_rect(lua_State *LUA) {
    struct animap *dest_bmp;
    int x, y, x2, y2;
    int col;
    int solid = 0;
    int mask = 0;
    struct animap *draw_animap = NULL;
    BITMAP *draw_bitmap = gd.simg.no_use;
    
    INITPARMCHECK(7, funcnames[DSB_BITMAP_RECT]);
    
    dest_bmp = luabitmap(LUA, -7, funcnames[DSB_BITMAP_RECT], 1);
    x = luaint(LUA, -6, funcnames[DSB_BITMAP_RECT], 2);
    y = luaint(LUA, -5, funcnames[DSB_BITMAP_RECT], 3);
    x2 = luaint(LUA, -4, funcnames[DSB_BITMAP_RECT], 4);
    y2 = luaint(LUA, -3, funcnames[DSB_BITMAP_RECT], 5);
    
    if (lua_isnumber(LUA, -2) || (lua_isboolean(LUA, -2) && lua_toboolean(LUA, -2))) {
        col = 0;
        mask = 1;        
    } else {
        if (lua_istable(LUA, -2)) {
            lua_pushstring(LUA, "type");
            lua_gettable(LUA, -3);
            if (!lua_isnil(LUA, -1)) {
                lua_pop(LUA, 1);
                draw_animap = luabitmap(LUA, -2, funcnames[DSB_BITMAP_RECT], 6);
                col = 0;
                mask = 1;
            } else {
                lua_pop(LUA, 1);
            }
        }
                
        if (!draw_animap) {
            col = luargbval(LUA, -2, funcnames[DSB_BITMAP_RECT], 6);
        }
    }
    
    lod_use(dest_bmp);
    if (disallow_anitarg(dest_bmp))
        RETURN(0);
    
    if (lua_isboolean(LUA, -1)) {
        int bsolid = lua_toboolean(LUA, -1);
        if (bsolid)
            solid = 1;
    }
    
    if (mask) {
        int dmode = DRAW_MODE_MASKED_PATTERN;
        if (draw_animap) {
            draw_bitmap = animap_subframe(draw_animap, 0);
            dmode = DRAW_MODE_COPY_PATTERN;
        } 
        
        drawing_mode(dmode, draw_bitmap, x, y);
    }
    
    if (solid)
        rectfill(dest_bmp->b, x, y, x2, y2, col);
    else
        rect(dest_bmp->b, x, y, x2, y2, col);
        
    if (mask)
        drawing_mode(DRAW_MODE_SOLID, NULL, 0, 0);

    RETURN(0);
}

int expl_bitmap_textout(lua_State *LUA) {
    struct animap *dest_bmp;
    FONT *dfont;
    const char *ostr;
    int xc, yc;
    int align = 0;
    int fontcolor = 0;
    int lp = 1;
    
    INITPARMCHECK(7, funcnames[DSB_BITMAP_TEXTOUT]);    

    dest_bmp = luabitmap(LUA, -7, funcnames[DSB_BITMAP_TEXTOUT], 1);
    dfont = luafont(LUA, -6, funcnames[DSB_BITMAP_TEXTOUT], 2);
    ostr = luastring(LUA, -5, funcnames[DSB_BITMAP_TEXTOUT], 3);
    xc = luaint(LUA, -4, funcnames[DSB_BITMAP_TEXTOUT], 4);
    yc = luaint(LUA, -3, funcnames[DSB_BITMAP_TEXTOUT], 5);
    align = luaint(LUA, -2, funcnames[DSB_BITMAP_TEXTOUT], 6);
    fontcolor = luargbval(LUA, -1, funcnames[DSB_BITMAP_TEXTOUT], 7);
    
    lod_use(dest_bmp);
    if (disallow_anitarg(dest_bmp))
        RETURN(0);
    
    if (align == 128)
        lp = mline_breaktext(dest_bmp->b, dfont, xc, yc, fontcolor, -1, (char*)ostr,
            gd.t_cpl, gd.t_maxlines, gd.t_ypl, 0);
    else if (align == 4)
        textout_centre_ex(dest_bmp->b, dfont, ostr, xc, yc, fontcolor, -1);
    else if (align == 1)
        textout_right_ex(dest_bmp->b, dfont, ostr, xc, yc, fontcolor, -1);
    else
        textout_ex(dest_bmp->b, dfont, ostr, xc, yc, fontcolor, -1);
    
    lua_pushinteger(LUA, lp);
    RETURN(1);
}

int expl_text_size(lua_State *LUA) {
    FONT *dfont;
    const char *ostr;
    int w = 0;
    int h = 0;

    INITPARMCHECK(2, funcnames[DSB_TEXT_SIZE]);    

    dfont = luafont(LUA, -2, funcnames[DSB_TEXT_SIZE], 1);
    ostr = luastring(LUA, -1, funcnames[DSB_TEXT_SIZE], 2);
    
    w = text_length(dfont, ostr);
    h = text_height(dfont);
    
    lua_pushinteger(LUA, w);
    lua_pushinteger(LUA, h);
    
    RETURN(2);
}

int expl_set_viewport(lua_State *LUA) {
    int yoff = 20;
    int b = 0;
    int xc, yc;
    
    INITVPARMCHECK(2, funcnames[DSB_VIEWPORT]);
    if (lua_gettop(LUA) == 3) {
        b = -1;
    }
    
    xc = luaint(LUA, -2+b, funcnames[DSB_VIEWPORT], 1);
    yc = luaint(LUA, -1+b, funcnames[DSB_VIEWPORT], 2);
    if (b == -1) {
        yoff = luaint(LUA, -1, funcnames[DSB_VIEWPORT], 3);
    }
    
    gd.vxo = xc;
    gd.vyo = yc;
    gd.vyo_off = yoff;
        
    RETURN(0);
}

int expl_set_viewport_distort(lua_State *LUA) {
    int distort_func;
    
    INITPARMCHECK(1, funcnames[DSB_VIEWPORT_DISTORT]);
    gd.distort_func = luaint(LUA, -1, funcnames[DSB_VIEWPORT_DISTORT], 1);
    
    RETURN(0);
}


int expl_make_wallset(lua_State *LUA) {
    struct animap *inbmp[MAX_WALLBMP];
    struct animap *wpatchbmp[MAXWALLPATCH];
    struct animap *sidepatch;
    struct animap *nwindow;
    struct wallset *newset;
    int i; 
    int i_num_parms;
    int no_patches = 0;
    
    for(i=0;i<MAXWALLPATCH;++i) {
        wpatchbmp[i] = NULL;
    }
    
    INITRPARMCHECK(16, 20, funcnames[DSB_MAKE_WALLSET]);

    for(i=0;i<MAXWALLPATCH;++i) {
        wpatchbmp[i] = NULL;
    }
    i_num_parms = lua_gettop(LUA);
    if (i_num_parms == 22) {
        no_patches = 1;
    } 
    
    only_when_loading(LUA, funcnames[DSB_MAKE_WALLSET]);
    
    // use and get rid of the extra parms
    nwindow = luabitmap(LUA, -1, funcnames[DSB_MAKE_WALLSET], i_num_parms);
    if (no_patches) {
        lua_pop(LUA, 1);
    } else {
        sidepatch = luaoptbitmap(LUA, -2, funcnames[DSB_MAKE_WALLSET]);
        for (i=0;i<MAXWALLPATCH;i++)
            wpatchbmp[i] = luaoptbitmap(LUA, -5 + i, funcnames[DSB_MAKE_WALLSET]);
            
        lua_pop(LUA, 5);
    }
        
    for (i=0;i<MAX_WALLBMP;i++)
        inbmp[i] = luabitmap(LUA, (-1*(MAX_WALLBMP-i)), funcnames[DSB_MAKE_WALLSET], 1+i);
        
    newset = dsbcalloc(1, sizeof(struct wallset));
    
    for (i=0;i<MAX_WALLBMP;i++) {
        newset->wall[i] = inbmp[i];
        
        if (newset->wall[i]->numframes <= 0)
            newset->wall[i]->numframes = 1;
            
        if (newset->wall[i])  
            newset->wall[i]->ext_references += 1;
    }
    
    for (i=0;i<MAXWALLPATCH;i++) {
        newset->wpatch[i] = wpatchbmp[i];
        
        if (newset->wpatch[i])
            newset->wpatch[i]->ext_references += 1;        
    }
    
    newset->window = nwindow;
    newset->sidepatch = sidepatch;
    if (nwindow)
        newset->window->ext_references += 1;
    if (sidepatch)
        newset->sidepatch->ext_references += 1;
    
    newset->type = WST_BIGWALLS;
    
    cwsa_num = initialize_wallset(newset);
    
    cwsa = newset;
    if (!haze_base) haze_base = newset;
        
    RETURN(1);
}

int expl_make_wallset_ext(lua_State *LUA) {
    int maxn = MAX_WALLBMP + MAX_XWALLBMP;
    struct animap *inbmp[MAX_WALLBMP + MAX_XWALLBMP];
    struct animap *wpatchbmp[MAXWALLPATCH];
    struct animap *sidepatch = NULL;
    struct animap *nwindow;
    struct wallset *newset;
    int i;
    int no_patches = 0;
    int i_num_parms;
    
    INITRPARMCHECK(22, 26, funcnames[DSB_MAKE_WALLSET_EXT]); 
    
    for(i=0;i<MAXWALLPATCH;++i) {
        wpatchbmp[i] = NULL;
    }
    
    i_num_parms = lua_gettop(LUA);
    if (i_num_parms == 22) {
        no_patches = 1;
    } 
    
    only_when_loading(LUA, funcnames[DSB_MAKE_WALLSET_EXT]);

    // use and get rid of the extra parms
    nwindow = luabitmap(LUA, -1, funcnames[DSB_MAKE_WALLSET_EXT], i_num_parms);
    if (no_patches) {
        lua_pop(LUA, 1);
    } else {
        sidepatch = luaoptbitmap(LUA, -2, funcnames[DSB_MAKE_WALLSET_EXT]);
        for (i=0;i<MAXWALLPATCH;i++)
            wpatchbmp[i] = luaoptbitmap(LUA, -5 + i, funcnames[DSB_MAKE_WALLSET_EXT]);
            
        lua_pop(LUA, 5);
    }

    for (i=0;i<maxn;i++) {
        inbmp[i] = luabitmap(LUA, (-1*(maxn-i)),
            funcnames[DSB_MAKE_WALLSET_EXT], 1+i);
    }

    newset = dsbcalloc(1, sizeof(struct wallset));
    
    for (i=0;i<MAX_WALLBMP;i++) {
        newset->wall[i] = inbmp[i];
        if (newset->wall[i]->numframes <= 0)
            newset->wall[i]->numframes = 1;
          
        if (newset->wall[i])  
            newset->wall[i]->ext_references += 1;
    }
    for (i=0;i<MAX_XWALLBMP;i++) {
        newset->xwall[i] = inbmp[i + MAX_WALLBMP];
        if (newset->xwall[i]->numframes <= 0)
            newset->xwall[i]->numframes = 1;
            
        if (newset->xwall[i])
            newset->xwall[i]->ext_references += 1;
    }

    for (i=0;i<MAXWALLPATCH;i++) {
        newset->wpatch[i] = wpatchbmp[i];
        
        if (newset->wpatch[i])
            newset->wpatch[i]->ext_references += 1;
    }

    newset->window = nwindow;
    newset->sidepatch = sidepatch;
    if (nwindow)
        newset->window->ext_references += 1;
    if (sidepatch)
        newset->sidepatch->ext_references += 1;

    newset->type = WST_LR_SEP;

    cwsa_num = initialize_wallset(newset);

    cwsa = newset;
    if (!haze_base) haze_base = newset;

    RETURN(1);
}

int expl_level_getinfo(lua_State *LUA) {
    int lev;
    
    INITPARMCHECK(1, funcnames[DSB_LEVEL_GETINFO]);
    
    lev = luaint(LUA, -1, funcnames[DSB_LEVEL_GETINFO], 1);
    validate_level(lev);
    
    luastacksize(7);
    lua_pushinteger(LUA, dun[lev].xsiz);
    lua_pushinteger(LUA, dun[lev].ysiz);
    lua_pushinteger(LUA, dun[lev].lightlevel);
    lua_pushinteger(LUA, dun[lev].xp_multiplier); 
    lua_getglobal(LUA, "__wallset_name_by_ref");
    if (!lua_isnil(LUA, -1)) {
        lua_pushinteger(LUA, dun[lev].wall_num);
        lua_pcall(LUA, 1, 1, 0);
    }
    
    RETURN(5);
}

int expl_move_moncol(lua_State *LUA) {
    unsigned int inst;
    int lev, xx, yy, dir;
    int travel_direction;
    struct inst *p_inst;
    struct inst_loc oneteam;
    int notmoved = 0;
    int isboss;
   
    INITPARMCHECK(7, funcnames[DSB_MOVE_MONCOL]);
    
    STARTNONF();
    inst = luaint(LUA, -7, funcnames[DSB_MOVE_MONCOL], 1);
    lev = luaint(LUA, -6, funcnames[DSB_MOVE_MONCOL], 2);
    xx = luaint(LUA, -5, funcnames[DSB_MOVE_MONCOL], 3);
    yy = luaint(LUA, -4, funcnames[DSB_MOVE_MONCOL], 4);
    dir = luaint(LUA, -3, funcnames[DSB_MOVE_MONCOL], 5);
    travel_direction = luaint(LUA, -2, funcnames[DSB_MOVE_MONCOL], 6);
    ENDNONF();
    
    validate_instance(inst);
    validate_objcoord(funcnames[DSB_MOVE_MONCOL], lev, xx, yy, dir);
    validate_facedir(travel_direction);
    
    if (lua_isboolean(LUA, -1)) {
        isboss = lua_toboolean(LUA, -1);
    } else
        isboss = -1;
        
    p_inst = oinst[inst];
    
    if (gd.watch_inst == inst)
        gd.watch_inst_out_of_position++;
        
    if (lev < 0 || p_inst->level < 0) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "MONCOL moves must take place in dungeon");
        RETURN(0);
    }
    
    if (gd.queue_rc && !gd.monster_move_ok) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "MONCOL moves cannot be deferred by actuators");
        RETURN(0);
    }   
        
    memset(&oneteam, 0, sizeof(struct inst_loc));
    oneteam.i = inst;
  
    mon_extcollision(&oneteam, travel_direction, xx, yy);
    mon_move(inst, lev, xx, yy, dir, isboss, 1);          

    RETURN(0);
}

int expl_tileptr_exch(lua_State *LUA) {
    int srcl, sx, sy;
    int desl, dx, dy;
    
    INITPARMCHECK(6, funcnames[DSB_TILEPTR_EXCH]);
    
    srcl = luaint(LUA, -6, funcnames[DSB_TILEPTR_EXCH], 1);
    sx = luaint(LUA, -5, funcnames[DSB_TILEPTR_EXCH], 2);
    sy = luaint(LUA, -4, funcnames[DSB_TILEPTR_EXCH], 3);
    desl = luaint(LUA, -3, funcnames[DSB_TILEPTR_EXCH], 4);
    dx = luaint(LUA, -2, funcnames[DSB_TILEPTR_EXCH], 5);
    dy = luaint(LUA, -1, funcnames[DSB_TILEPTR_EXCH], 6);
    
    validate_coord(funcnames[DSB_TILEPTR_EXCH], srcl, sx, sy, 0);
    validate_coord(funcnames[DSB_TILEPTR_EXCH], desl, dx, dy, 0);
    
    if (gd.queue_rc)
        instmd_queue(INSTMD_PTREXCH, srcl, sx, sy, desl, dx, dy);
    else
        exchange_pointers(srcl, sx, sy, desl, dx, dy);
    
    RETURN(0);
}

int expl_reposition(lua_State *LUA) {
    unsigned int inst;
    int tilepos;
    struct inst *p_inst;
    struct obj_arch *p_arch;

    INITPARMCHECK(2, funcnames[DSB_REPOSITION]);

    inst = luaint(LUA, -2, funcnames[DSB_REPOSITION], 1);
    tilepos = luaint(LUA, -1, funcnames[DSB_REPOSITION], 2);
    
    validate_instance(inst);
    validate_tilepos5(tilepos);
    
    p_inst = oinst[inst];
    p_arch = Arch(p_inst->arch);
    
    if (gd.queue_rc && p_inst->level >= 0)
        instmd_queue(INSTMD_RELOCATE, p_inst->level, 0, 0, tilepos, inst, 0);
    else {
        int rl = inst_relocate(inst, tilepos);
        if (rl == 1) {
            gd.lua_nonfatal = 1;
            DSBLerror(LUA, "%s: Invalid size2 subtile %d",
                    funcnames[DSB_REPOSITION], tilepos);
        }
    }
    
    RETURN(0);
}

int expl_collide(lua_State *LUA) {
    unsigned int inst;
    struct dungeon_level *dd;
    struct inst_loc *il;
    int lev, xx, yy, dir;
    int ddir;

    INITPARMCHECK(6, funcnames[DSB_COLLIDE]);

    STARTNONF();
    inst = luaint(LUA, -6, funcnames[DSB_COLLIDE], 1);
    lev = luaint(LUA, -5, funcnames[DSB_COLLIDE], 2);
    xx = luaint(LUA, -4, funcnames[DSB_COLLIDE], 3);
    yy = luaint(LUA, -3, funcnames[DSB_COLLIDE], 4);
    dir = luaint(LUA, -2, funcnames[DSB_COLLIDE], 5);
    ddir = luaint(LUA, -1, funcnames[DSB_COLLIDE], 6);
    validate_objcoord(funcnames[DSB_COLLIDE], lev, xx, yy, dir);
    validate_instance(inst);
    validate_facedir(ddir);
    ENDNONF();

    dd = &(dun[lev]);
    il = dd->t[yy][xx].il[dir];
    if (il) {
        struct inst_loc *col_list;
        col_list = fast_il_genlist(NULL, il, ddir, inst);
        inst_loc_to_luatable(LUA, col_list, 1, NULL);
    } else
        lua_pushnil(LUA);
        
    RETURN(1);
}

int expl_get_condition(lua_State *LUA) {
    int targ, cid;
    int cval;
    int maxconds;
    int i_ret_extra = 0;
    int animval = 0;
    
    INITPARMCHECK(2, funcnames[DSB_GET_CONDITION]);
    
    STARTNONF();
    targ = luaint(LUA, -2, funcnames[DSB_GET_CONDITION], 1);
    cid = luaint(LUA, -1, funcnames[DSB_GET_CONDITION], 2);    
    
    if (targ < 0) {
        maxconds = gd.num_pconds;
    } else {
        maxconds = gd.num_conds;
        validate_champion(targ);
    }
    ENDNONF();
    
    if (cid < 1 || cid > maxconds) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "Bad condition number %d", cid);
        RETURN(0);
    }
    
    --cid; 
    if (targ < 0) {
        cval = gd.partycond[cid];
        animval = gd.partycond_animtimer[cid];
    } else {
        cval = gd.champs[targ-1].condition[cid];  
        animval = gd.champs[targ-1].cond_animtimer[cid];  
    }
      
    if (cval == 0)
        lua_pushnil(LUA);
    else {
        lua_pushinteger(LUA, cval);
        if (animval != FRAMECOUNTER_NORUN) {
            i_ret_extra = 1;
            lua_pushinteger(LUA, animval);
        }
    }
    
    RETURN(1+i_ret_extra);
}    
    

int expl_condition(lua_State *LUA) {
    int targ, cid, str;
    struct condition_desc *cond;
    int maxconds;
    int b = 0;
    int update_anim_timer = 0;
    int using_anim_timer = 0;
    int new_anim_timer_v = 0;
    
    INITVPARMCHECK(3, funcnames[DSB_SET_CONDITION]);
    
    if (lua_gettop(LUA) == 4) {
        b = -1;
    }
    
    STARTNONF();
    targ = luaint(LUA, -3 + b, funcnames[DSB_SET_CONDITION], 1);
    cid = luaint(LUA, -2 + b, funcnames[DSB_SET_CONDITION], 2);
    str = luaint(LUA, -1 + b, funcnames[DSB_SET_CONDITION], 3);
    ENDNONF();
    
    if (b == -1) {
        if (lua_isboolean(LUA, -1)) {
            update_anim_timer = 1;
            using_anim_timer = lua_toboolean(LUA, -1);
            new_anim_timer_v = 0;
        } else {
            update_anim_timer = 1;
            using_anim_timer = 1;
            new_anim_timer_v = luaint(LUA, -1, funcnames[DSB_SET_CONDITION], 4);
        }
    }    

    if (targ < 0) {
        maxconds = gd.num_pconds;
        cond = &(p_conds[cid-1]);
    } else {
        maxconds = gd.num_conds;
        cond = &(i_conds[cid-1]);
    }
    
    if (cid < 1 || cid > maxconds) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "Bad condition number %d", cid);
        RETURN(0);
    }
        
    --cid;
    if (targ > 0) {
        int ctimer;
        
        validate_champion(targ);
        ctimer = gd.champs[targ-1].condition[cid]; 
        
        if (cond->baseint) {
            if (ctimer == 0) {
                if (str > 0) {
                    add_timer(targ, EVENT_COND_UPDATE,
                        cond->baseint, cond->baseint, cid, 0, 0);
                }
            } else {
                if (str <= 0) {
                    destroy_assoc_timer(targ, EVENT_COND_UPDATE, cid);
                }
            }
        }
        
        ctimer = str;
        if (ctimer < 0)
            ctimer = 0;

        gd.champs[targ-1].condition[cid] = ctimer;
        
        if (update_anim_timer) {
            if (using_anim_timer)
                gd.champs[targ-1].cond_animtimer[cid] = new_anim_timer_v;
            else
                gd.champs[targ-1].cond_animtimer[cid] = FRAMECOUNTER_NORUN;
        }

        destroy_system_subrenderers();

    } else {
        int ctimer;
        
        if (targ != -1) {
            gd.lua_nonfatal = 1;
            DSBLerror(LUA, "Target for condition must be PARTY (not %d)", targ);
            RETURN(0);
        }
        
        ctimer = gd.partycond[cid];
        
        if (cond->baseint) {
            if (ctimer == 0) {
                if (str > 0) {
                    add_timer(9999, EVENT_COND_UPDATE,
                        cond->baseint, cond->baseint, cid, 0, 0);
                }
            } else {
                if (str <= 0) {
                    destroy_assoc_timer(9999, EVENT_COND_UPDATE, cid);
                }
            }
        }
              
        ctimer = str;
        if (ctimer < 0)
            ctimer = 0;
            
        gd.partycond[cid] = ctimer;
        
        if (update_anim_timer) {
            if (using_anim_timer)
                gd.partycond_animtimer[cid] = new_anim_timer_v;
            else
                gd.partycond_animtimer[cid] = FRAMECOUNTER_NORUN;
        }
    }
        
    RETURN(0);    
}

unsigned short get_condflags(int effect, int haveflags, const char *tstr) {
    unsigned short eflags = 0;

    onstack("get_condflags");

    if (effect == -254) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "Use of condition type BAD_INDIVIDUAL is deprecated. Use flags instead.");
        eflags = COND_MOUTH_RED;
    }
    
    if (haveflags) {
        int i_flags;
        
        lua_getglobal(LUA, tstr);
        
        if (!lua_isnumber(LUA, -1)) {
            gd.lua_nonfatal = 1;
            DSBLerror(LUA, "Invalid flags specified for condition");
            RETURN(eflags);
        }
        
        i_flags = lua_tointeger(LUA, -1);
        
        // only store valid flags
        eflags = (i_flags & 255);
        
        lua_pop(LUA, 1);
    }
    
    RETURN(eflags);
}

int expl_add_condition(lua_State *LUA) {
    struct animap *effectbmp;
    struct animap *charbmp;
    int call_interval;
    int anum, effect;
    unsigned short eflags = 0;
    int tempval = LUA_NOREF;
    int haveflags = 0;
    const char *tmpvar = "_DSBTMP";
    struct condition_desc *conds;
    
    INITVPARMCHECK(5, funcnames[DSB_ADD_CONDITION]);
        
    only_when_loading(LUA, funcnames[DSB_ADD_CONDITION]);
    
    // if we have flags, stash them away rather than messing with it now
    if (lua_gettop(LUA) == 6) {
        haveflags = 1;
        lua_setglobal(LUA, tmpvar);
    }

    effect = luaint(LUA, -5, funcnames[DSB_ADD_CONDITION], 1);
    effectbmp = luaoptbitmap(LUA, -4, funcnames[DSB_ADD_CONDITION]);
    charbmp = luaoptbitmap(LUA, -3, funcnames[DSB_ADD_CONDITION]);
    call_interval = luaint(LUA, -2, funcnames[DSB_ADD_CONDITION], 4);

    // store the called function
    if (call_interval) {
        if (lua_isnil(LUA, -1) || !lua_isfunction(LUA, -1)) {
            DSBLerror(LUA, "%s: Specified function not found",
                funcnames[DSB_ADD_CONDITION]);
            RETURN(0);
        }
        
        if (call_interval < 0) {
            gd.lua_nonfatal = 1;
            DSBLerror(LUA, "%s: Bad call_interval", funcnames[DSB_ADD_CONDITION]);
            RETURN(0);
        }
            
        tempval = luaL_ref(LUA, LUA_REGISTRYINDEX);
    }

    if (effect == -1) { 
        anum = ++gd.num_pconds;
        p_conds = dsbrealloc(p_conds, anum * sizeof(struct condition_desc));
        conds = p_conds;
    } else { 
        anum = ++gd.num_conds;
        i_conds = dsbrealloc(i_conds, anum * sizeof(struct condition_desc));
        conds = i_conds;
    }

    eflags = get_condflags(effect, haveflags, tmpvar);
        
    // store c side
    conds[anum-1].bmp = effectbmp;
    conds[anum-1].pbmp = charbmp;
    conds[anum-1].baseint = call_interval;
    conds[anum-1].flags = eflags;
    
    // store Lua side
    conds[anum-1].lua_reg = tempval;
    
    lua_pushinteger(LUA, anum);
    
    RETURN(1);
}

int expl_replace_condition(lua_State *LUA) {
    struct animap *effectbmp;
    struct animap *charbmp;
    int cid;
    int maxconds;
    int call_interval;
    int effect;
    int tempval = LUA_NOREF;
    unsigned short eflags = 0;
    char *tmpvar = "_DSBTMP";
    int haveflags = 0;
    struct condition_desc *conds;

    INITVPARMCHECK(6, funcnames[DSB_REPLACE_CONDITION]);
    
    luastacksize(9);
        
    // if we have flags, stash them away rather than messing with it now
    if (lua_gettop(LUA) == 7) {
        haveflags = 1;
        lua_setglobal(LUA, tmpvar);
    }

    cid = luaint(LUA, -6, funcnames[DSB_REPLACE_CONDITION], 1);
    effect = luaint(LUA, -5, funcnames[DSB_REPLACE_CONDITION], 2);
    effectbmp = luaoptbitmap(LUA, -4, funcnames[DSB_REPLACE_CONDITION]);
    charbmp = luaoptbitmap(LUA, -3, funcnames[DSB_REPLACE_CONDITION]);
    call_interval = luaint(LUA, -2, funcnames[DSB_REPLACE_CONDITION], 5);
    
    if (effect == -1) {
        maxconds = gd.num_pconds;
        conds = p_conds;
    } else {
        maxconds = gd.num_conds;
        conds = i_conds;
    }
    
    if (debug) {
        v_onstack("lua.dsb_replace_condition.diagnostic");
        fprintf(errorlog, "dsb_replace_condition: Replacing %ld [EBMP old: %x new: %x] [CBMP old: %x new: %x] "
            "[LUA_REF old: %x](noref = %x)\n",
            cid, conds[cid-1].bmp, effectbmp, conds[cid-1].pbmp, charbmp,
            conds[cid-1].lua_reg, LUA_NOREF);
        fflush(errorlog);
        v_upstack();
    }
    
    if (cid < 1 || cid > maxconds) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "Bad condition number %d", cid);
        RETURN(0);
    }
    
    if (call_interval) {
        if (lua_isnil(LUA, -1) || !lua_isfunction(LUA, -1)) {
            DSBLerror(LUA, "%s: Specified function not found",
                funcnames[DSB_REPLACE_CONDITION]);
            RETURN(0);
        }

        if (call_interval < 0) {
            gd.lua_nonfatal = 1;
            DSBLerror(LUA, "%s: Bad call_interval",
                funcnames[DSB_REPLACE_CONDITION]);
            RETURN(0);
        }

        tempval = luaL_ref(LUA, LUA_REGISTRYINDEX);
    }

    // clear old registry entry
    if (conds[cid-1].lua_reg != LUA_NOREF) {
        luaL_unref(LUA, LUA_REGISTRYINDEX, conds[cid-1].lua_reg);
        conds[cid-1].lua_reg = LUA_NOREF;
    }

    
    eflags = get_condflags(effect, haveflags, tmpvar);

    // store c side
    conds[cid-1].bmp = effectbmp;
    conds[cid-1].pbmp = charbmp;
    conds[cid-1].baseint = call_interval;
    conds[cid-1].flags = eflags;
    
    // store Lua side
    conds[cid-1].lua_reg = tempval;

    RETURN(0);
}

int expl_get_portraitname(lua_State *LUA) {
    struct champion *me;
    int who;
    
    INITPARMCHECK(1, funcnames[DSB_GET_PORTRAITNAME]);
    
    who = luaint(LUA, -1, funcnames[DSB_GET_PORTRAITNAME], 1);
    
    validate_champion(who);
    me = &(gd.champs[who-1]);
    lua_pushstring(LUA, me->port_name);
    
    RETURN(1);
}

// formerly expl_replace_portrait
int expl_set_portraitname(lua_State *LUA) {
    struct champion *me;
    char *pname;
    int who;
    INITPARMCHECK(2, funcnames[DSB_SET_PORTRAITNAME]);
    
    who = luaint(LUA, -2, funcnames[DSB_SET_PORTRAITNAME], 1);
    pname = (char *)luastring(LUA, -1, funcnames[DSB_SET_PORTRAITNAME], 2);
    validate_champion(who);
    me = &(gd.champs[who-1]);
    dsbfree(me->port_name);
    me->port_name = dsbstrdup(pname);
    me->portrait = grab_from_gfxtable(me->port_name);
    RETURN(0);
}

int expl_replace_charhand(lua_State *LUA) {
    struct champion *me;
    char *hname;
    int who;
    INITPARMCHECK(2, funcnames[DSB_REPLACE_CHARHAND]);
    
    who = luaint(LUA, -2, funcnames[DSB_REPLACE_CHARHAND], 1);
    hname = (char *)luastring(LUA, -1, funcnames[DSB_REPLACE_CHARHAND], 2);
    validate_champion(who);
    me = &(gd.champs[who-1]);
    dsbfree(me->hand_name);
    me->hand_name = dsbstrdup(hname);
    me->hand = grab_from_gfxtable(me->hand_name);
    RETURN(0);
}

int expl_replace_inventory(lua_State *LUA) {
    struct champion *me;
    char *iname;
    int who;
    INITPARMCHECK(2, funcnames[DSB_REPLACE_INVENTORY]);
    
    who = luaint(LUA, -2, funcnames[DSB_REPLACE_INVENTORY], 1);
    iname = (char *)luastring(LUA, -1, funcnames[DSB_REPLACE_INVENTORY], 2);
    validate_champion(who);
    me = &(gd.champs[who-1]);
    dsbfree(me->invscr_name);
    me->invscr_name = dsbstrdup(iname);
    me->invscr = grab_from_gfxtable(me->invscr_name);
    RETURN(0);
}

int expl_replace_topimages(lua_State *LUA) {
    struct champion *me;
    int who;
    int i;
    
    INITPARMCHECK(4, funcnames[DSB_REPLACE_TOPIMAGES]);
    
    who = luaint(LUA, -4, funcnames[DSB_REPLACE_TOPIMAGES], 1);
    validate_champion(who);
    me = &(gd.champs[who-1]);
    
    for (i=0;i<MAX_TOP_SCR;++i) {
        const char *bmp_name;
        char *storestring;
        int b_parm = -1 * (MAX_TOP_SCR - i);
        
        if (lua_isnil(LUA, b_parm))
            continue;
            
        if (lua_isnumber(LUA, b_parm))
            storestring = NULL;
        else {          
            bmp_name = luastring(LUA, b_parm, funcnames[DSB_REPLACE_TOPIMAGES], i + 2);
            storestring = dsbstrdup(bmp_name);
        }
        
        switch (i) {
            case (TOP_HANDS): {
                dsbfree(me->top_hands_name);
                me->top_hands_name = storestring; 
            } break;
            
            case (TOP_PORT): {
                dsbfree(me->top_port_name);
                me->top_port_name = storestring;
            } break;
            
            case (TOP_DEAD): {
                dsbfree(me->top_dead_name);
                me->top_dead_name = storestring;
            } break;
        }
        
        if (storestring)
            me->topscr_override[i] = grab_from_gfxtable(storestring);
        else
            me->topscr_override[i] = NULL;
    }
    
    RETURN(0);
}

int expl_swap(lua_State *LUA) {
    unsigned int inst;
    unsigned int targ_arch;
    
    INITPARMCHECK(2, funcnames[DSB_SWAP]);
    STARTNONF();
    inst = luaint(LUA, -2, funcnames[DSB_SWAP], 1);
    targ_arch = luaobjarch(LUA, -1, funcnames[DSB_SWAP]);   
    validate_instance(inst);
    ENDNONF();
    
    regular_swap(inst, targ_arch);
    lua_pushinteger(LUA, inst);
    RETURN(1);
}

int expl_get_gameflag(lua_State *LUA) {
    unsigned int gflag;
    
    INITPARMCHECK(1, funcnames[DSB_GET_GAMEFLAG]);
    STARTNONF();
    gflag = luaint(LUA, -1, funcnames[DSB_GET_GAMEFLAG], 1); 
    ENDNONF();
    
    if (gd.gameplay_flags & gflag)
        lua_pushinteger(LUA, 1);
    else
        lua_pushnil(LUA);
 
    RETURN(1);   
}

int expl_get_mflag(lua_State *LUA) {
    unsigned int gflag;

    INITPARMCHECK(1, funcnames[DSB_GET_MFLAG]);
    STARTNONF();
    gflag = luaint(LUA, -1, funcnames[DSB_GET_MFLAG], 1);
    ENDNONF();

    if (Gmparty_flags & gflag)
        lua_pushinteger(LUA, 1);
    else
        lua_pushnil(LUA);

    RETURN(1);
}

int expl_champion_toparty(lua_State *LUA) {
    int ppos;
    int who_add;
    int ap;
    int real_level;

    INITPARMCHECK(2, funcnames[DSB_CHAMPION_TOPARTY]);
    
    STARTNONF();
    ppos = luaint(LUA, -2, funcnames[DSB_CHAMPION_TOPARTY], 1);
    who_add = luaint(LUA, -1, funcnames[DSB_CHAMPION_TOPARTY], 2);
    validate_ppos(ppos);
    validate_champion(who_add);
    ENDNONF();
    
    if (gd.party[ppos]) {
        RETURN(0);
    }
    
    ap = party_ist_peek(gd.a_party);
    
    inv_pickup_all(who_add);
    gd.party[ppos] = who_add;
    inv_putdown_all(who_add);
    
    gd.party[ppos] = 0;
    real_level = gd.p_lev[ap];
    party_composition_changed(ap, PCC_MOVEOFF, real_level);
    put_into_pos(ap, ppos+1);
    gd.party[ppos] = who_add;
    party_composition_changed(ap, PCC_MOVEON, real_level);
    
    if (gd.leader == -1)
        gd.leader = ppos;

    gd.idle_t[ppos] = 0;
        
    lua_pushboolean(LUA, 1);
    RETURN(1);
}

int expl_champion_fromparty(lua_State *LUA) {
    int who_out;
    int ppos;

    INITPARMCHECK(1, funcnames[DSB_CHAMPION_FROMPARTY]);

    STARTNONF();
    ppos = luaint(LUA, -1, funcnames[DSB_CHAMPION_FROMPARTY], 1);
    validate_ppos(ppos);
    ENDNONF();

    if (!gd.party[ppos]) {
        RETURN(0);
    }
    
    if (viewstate == VIEWSTATE_INVENTORY) {
        if (gd.who_look == ppos)
            exit_inventory_view();
    }
    
    if (gd.who_method == (ppos+1))
        clear_method();
        
    who_out = gd.party[ppos];
    inv_pickup_all(who_out);

    gd.idle_t[ppos] = 0;
    gd.party[ppos] = 0;
    
    // only try to unppos a living character
    if (gd.champs[who_out-1].bar[BAR_HEALTH] > 0) {
        remove_from_all_pos(ppos+1);
    }

    if (gd.whose_spell == ppos) {
        int n = 0;
        for(n=0;n<4;++n) {
            if (gd.party[n] && IsNotDead(n)) {
                gd.whose_spell = n;
            }
        }
    }

    if (gd.leader == ppos) {
        int ic = 0;
        gd.leader = -1;
        for (ic=0;ic<4;++ic) {
            if (gd.party[ic] && IsNotDead(ic)) {
                gd.leader = ic;
                break;
            }
        }
    }
    
    inv_putdown_all(who_out);
    
    if (gd.leader == -1) {
        if (gd.gameplay_flags & GP_END_EMPTY)
            gd.everyone_dead = 65;
    }

    lua_pushboolean(LUA, 1);
    RETURN(1);
}


int expl_offer_champion(lua_State *LUA) {
    int who_offer, offermode;
    int freeparty;
    int added = 0;
    
    INITPARMCHECK(3, funcnames[DSB_OFFER_CHAMPION]);
    
    STARTNONF();
    who_offer = luaint(LUA, -3, funcnames[DSB_OFFER_CHAMPION], 1);
    offermode = luaint(LUA, -2, funcnames[DSB_OFFER_CHAMPION], 2);
    ENDNONF();
    
    // if we're already offered one, don't even try
    if (viewstate == VIEWSTATE_CHAMPION) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "Already offering champion");
        RETURN(0);
    }
        
    if (lua_isfunction(LUA, -1))
        lua_setglobal(LUA, "__TAKEFUNC");
    else {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "%s: Invalid take function", funcnames[DSB_OFFER_CHAMPION]);
        RETURN(0);
    }
    
    gd.lua_nonfatal = 1;
    if (validate_champion(who_offer)) {
        RETURN(0);
    }
    
    if (offermode < 1 || offermode > 3) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "%s: Bad mode %d", funcnames[DSB_OFFER_CHAMPION], offermode);
        RETURN(0);
    }
    
    gd.lua_nonfatal = 0;
    
    // temporarily add member to the party for viewing    
    freeparty = 0;
    while (freeparty < 4 && !added) {
        if (!gd.party[freeparty]) {
            gd.party[freeparty] = who_offer;
            added = 1+freeparty;
            break;
        }
        ++freeparty;
    }
    
    gd.champs[who_offer-1].load = sum_load(who_offer-1);    
    calculate_maxload(who_offer);
    determine_load_color(who_offer-1);
    
    if (!added)
        RETURN(0);
    
    if (offermode == 1)
        gfxctl.i_subrend = pcxload("OPTION_RES", NULL);
    else if (offermode == 2)
        gfxctl.i_subrend = pcxload("OPTION_REI", NULL);
    else if (offermode == 3)
        gfxctl.i_subrend = pcxload("OPTION_RES_REI", NULL);
        
    gd.tmp_champ = added;
    put_into_pos(gd.a_party, added);
    
    enter_inventory_view(added - 1);
    enter_champion_preview(who_offer);
    
    gd.offering_ppos = freeparty;
    gd.viewmode = offermode;
    viewstate = VIEWSTATE_CHAMPION;
    gd.arrowon = gd.litarrow = 0;
    
    RETURN(0);  
}

int expl_objzone(lua_State *LUA) {
    static int complaints = 0;
    struct animap *sbmp;
    unsigned int zn, xx, yy;
    int inst;
    struct inst *p_inst;
    int cx, cy;
    int icnt;
    int csr_found;
    struct clickzone **lcz = NULL;
    int *lczn = NULL;
    
    INITPARMCHECK(5, funcnames[DSB_OBJZONE]);
    
    sbmp = luabitmap(LUA, -5, funcnames[DSB_OBJZONE], 1);
    inst = luaint(LUA, -4, funcnames[DSB_OBJZONE], 2);
    zn = luaint(LUA, -3, funcnames[DSB_OBJZONE], 3);
    xx = luaint(LUA, -2, funcnames[DSB_OBJZONE], 4);
    yy = luaint(LUA, -1, funcnames[DSB_OBJZONE], 5);
    
    csr_found = find_targeted_csr(sbmp, xx, yy, &lcz, &lczn, &cx, &cy);
    
    if (!csr_found) {
        lcz = &lua_cz;
        lczn = &lua_cz_n;  
        cx = xx + gd.vxo + gii.srx;
        cy = yy + gd.vyo + gd.vyo_off + gii.sry;      
        if (sbmp != gfxctl.subrend) {
            if (complaints < 4) {
                complaints++;
                gd.lua_nonfatal = 1;
                DSBLerror(LUA, "%s: Invalid objzone target", funcnames[DSB_OBJZONE]);
            }
            RETURN(0);
        }
    }
    
    // don't draw on animations
    if (sbmp->numframes > 1) {
        RETURN(0);
    }
    
    if (inst < 0) {
        int csr = csr_found-1;
        int ppos;
        
        ppos = find_associated_csr_ppos(sbmp, csr, csr_found);
        
        if (zn < 0)
            zn = find_valid_czn(*lcz, *lczn);
                     
        if (PPis_here[ppos]) {
            new_lua_cz(ZF_SYSTEM_OBJZONE, lcz, lczn, zn, cx, cy, 32, 32, 0, 0);
        }
        
        draw_i_pl_ex(1, sbmp->b, gd.party[ppos]-1, xx, yy, zn, 0);
        if (gd.who_method == (ppos+1) && gd.method_loc == zn) {
            draw_gfxicon(sbmp->b, gii.sel_box, xx, yy, 0, NULL);
        }
        
    } else {   
        validate_instance(inst);
        p_inst = oinst[inst];
        
        if (zn < 0)
            zn = find_valid_czn(*lcz, *lczn);
        
        new_lua_cz(0, lcz, lczn, zn, cx, cy, 32, 32, inst, 0);
    
        if (zn < p_inst->inside_n) {
            unsigned int i_n = p_inst->inside[zn];
            
            if (i_n) {
                draw_objicon(sbmp->b, i_n, xx, yy, 0);
            }
        }
    }  
    
    
    RETURN(0);    
}

int expl_msgzone(lua_State *LUA) {
    static int complaints = 0;
    int b;
    struct clickzone **lcz = NULL;
    int *lczn = NULL;
    struct animap *sbmp;
    int zn;
    unsigned int xx, yy;
    int inst;
    int i_sysmsg = 0;
    int i_objmsg;
    unsigned int i_objmsg_ex = 0;
    int cx, cy;
    int w, h;
    int icnt;
    int found;

    INITRPARMCHECK(8, 10, funcnames[DSB_MSGZONE]);

    if (lua_gettop(LUA) == 10) {
        b = -2;
    } else {
        b = 0;
    }

    sbmp = luabitmap(LUA, b-8, funcnames[DSB_MSGZONE], 1);
    inst = luaint(LUA, b-7, funcnames[DSB_MSGZONE], 2);
    zn = luaint(LUA, b-6, funcnames[DSB_MSGZONE], 3);
    xx = luaint(LUA, b-5, funcnames[DSB_MSGZONE], 4);
    yy = luaint(LUA, b-4, funcnames[DSB_MSGZONE], 5);
    w = luaint(LUA, b-3, funcnames[DSB_MSGZONE], 6);
    h = luaint(LUA, b-2, funcnames[DSB_MSGZONE], 7);
    i_objmsg = luaint(LUA, b-1, funcnames[DSB_MSGZONE], 8);
    
    if (b == -2) {
        // targeted inst must be SYSTEM
        if (inst != -1) {
            if (complaints < 4) {
                complaints++;
                gd.lua_nonfatal = 1;
                DSBLerror(LUA, "Extended msgzone target must be SYSTEM");
            }
            RETURN(0);
        }
        
        i_sysmsg = i_objmsg;
        i_objmsg = luaint(LUA, -2, funcnames[DSB_MSGZONE], 9);
        i_objmsg_ex = luaint(LUA, -1, funcnames[DSB_MSGZONE], 10);
    } else if (inst == -1) {
        i_sysmsg = i_objmsg;
        i_objmsg = 0;
    }

    found = find_targeted_csr(sbmp, xx, yy, &lcz, &lczn, &cx, &cy);
      
    if (!found) {
        if (sbmp == gfxctl.subrend) {
            lcz = &lua_cz;
            lczn = &lua_cz_n;
            cx = xx + gd.vxo + gii.srx;
            cy = yy + gd.vyo + gd.vyo_off + gii.sry;
        } else {
            if (complaints < 4) {
                complaints++;
                gd.lua_nonfatal = 1;
                DSBLerror(LUA, "%s: Invalid msgzone target", funcnames[DSB_MSGZONE]);
            }
            RETURN(0);
        }
    }
    
    if (zn < 0) {
        zn = find_valid_czn(*lcz, *lczn);   
    }

    if (inst == -1) {
        new_lua_extcz(ZF_SYSTEM_MSGZONE, lcz, lczn, zn, cx, cy, w, h,
            i_sysmsg, i_objmsg, i_objmsg_ex);
    } else {
        validate_instance(inst);
        if (i_objmsg != 0) {
            new_lua_cz(0, lcz, lczn, zn, cx, cy, w, h, inst, i_objmsg);
        }
    }

    RETURN(0);
}

int expl_party_coords(lua_State *LUA) {
    int ap;
    
    INITRPARMCHECK(0, 1, funcnames[DSB_PARTY_COORDS]);
    if (lua_gettop(LUA) == 1) {
        ap = luaint(LUA, -1, funcnames[DSB_PARTY_COORDS], 1);
        if (ap < 0) ap = 0;
        if (ap > 3) ap = 3;
    } else {
        ap = party_ist_peek(gd.a_party);
    }
    
    lua_pushinteger(LUA, gd.p_lev[ap]);
    lua_pushinteger(LUA, gd.p_x[ap]);
    lua_pushinteger(LUA, gd.p_y[ap]);
    lua_pushinteger(LUA, gd.p_face[ap]);
    
    RETURN(4);    
}

int expl_party_at(lua_State *LUA) {
    int nap;
    int lev, xx, yy;
    
    INITPARMCHECK(3, funcnames[DSB_PARTY_AT]);

    lev = luaint(LUA, -3, funcnames[DSB_PARTY_AT], 1);
    xx = luaint(LUA, -2, funcnames[DSB_PARTY_AT], 2);
    yy = luaint(LUA, -1, funcnames[DSB_PARTY_AT], 3);
    
    validate_coord(funcnames[DSB_PARTY_AT], lev, xx, yy, 0);
    for (nap=0;nap<4;++nap) {
        if (gd.p_lev[nap] == lev) {
            if (gd.p_x[nap] == xx && gd.p_y[nap] == yy) {
                lua_pushinteger(LUA, nap);
                RETURN(1);
                break;
            }
        }
    }

    lua_pushnil(LUA);
    RETURN(1);
}

int expl_party_contains(lua_State *LUA) {
    int ppos;
    int ap;

    INITPARMCHECK(1, funcnames[DSB_PARTY_CONTAINS]);
    ppos = luaint(LUA, -1, funcnames[DSB_PARTY_CONTAINS], 1);
    validate_ppos(ppos);
    
    if (!GMP_ENABLED()) {
        lua_pushinteger(LUA, 0);
        RETURN(1);
    }
    
    ap = mparty_containing(ppos);
    if (ap < 0)
        lua_pushnil(LUA);
    else
        lua_pushinteger(LUA, ap);
        
    RETURN(1);
}

int expl_party_viewing(lua_State *LUA) {
    INITPARMCHECK(0, funcnames[DSB_PARTY_VIEWING]);
    lua_pushinteger(LUA, gd.a_party);
    RETURN(1);
}

int expl_party_affecting(lua_State *LUA) {
    int ap;
    
    INITPARMCHECK(0, funcnames[DSB_PARTY_AFFECTING]);
    ap = party_ist_peek(gd.a_party);
    lua_pushinteger(LUA, ap);
    RETURN(1);
}

int expl_party_apush(lua_State *LUA) {
    int ap;
    
    INITPARMCHECK(1, funcnames[DSB_PARTY_APUSH]);
    
    STARTNONF();
    ap = luaint(LUA, -1, funcnames[DSB_PARTY_APUSH], 1);
    ENDNONF();
    
    if (ap < 0) ap = 0;
    if (ap > 3) ap = 3;
    
    ist_push(&istaparty, ap);
    
    RETURN(0);
}

int expl_party_apop(lua_State *LUA) {
    int iv;
    
    INITPARMCHECK(0, funcnames[DSB_PARTY_APOP]);
    
    iv = ist_pop(&istaparty);
    
    if (iv == -99) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "Party stack underflow");
        RETURN(0);
    }
    
    lua_pushinteger(LUA, iv);
    RETURN(1);
}

int expl_push_mouse(lua_State *LUA) {
    unsigned int inst;

    INITPARMCHECK(1, funcnames[DSB_PUSH_MOUSE]);
    STARTNONF();
    inst = luaint(LUA, -1, funcnames[DSB_PUSH_MOUSE], 1);
    validate_instance(inst);
    ENDNONF();

    if (!gd.mouseobj) {

        if (oinst[inst]->level != LOC_LIMBO)
            limbo_instance(inst);
            
        mouse_obj_grab(inst);
    }

    RETURN(0);
}


int expl_pop_mouse(lua_State *LUA) {
    
    INITPARMCHECK(0, funcnames[DSB_POP_MOUSE]);

    if (gd.mouseobj) {
        lua_pushinteger(LUA, gd.mouseobj);
        mouse_obj_drop(gd.mouseobj);
    } else
        lua_pushnil(LUA);
        
    RETURN(1);
}

int expl_shoot(lua_State *LUA) {
    unsigned int inst;
    int lev, xx, yy, travdir, tilepos;
    unsigned short throwpower;
    unsigned short damagepower;
    short delta;
    short ddelta;
    int b = 0;
    
    INITVPARMCHECK(9, funcnames[DSB_SHOOT]);
    
    if (lua_gettop(LUA) == 9) {
        b = 0;
    } else {
        b = -1;
    }
    
    inst = luaint(LUA, b-9, funcnames[DSB_SHOOT], 1);
    lev = luaint(LUA, b-8, funcnames[DSB_SHOOT], 2);
    xx = luaint(LUA, b-7, funcnames[DSB_SHOOT], 3);
    yy = luaint(LUA, b-6, funcnames[DSB_SHOOT], 4);
    travdir = luaint(LUA, b-5, funcnames[DSB_SHOOT], 5);
    tilepos = luaint(LUA, b-4, funcnames[DSB_SHOOT], 6);
    throwpower = luaint(LUA, b-3, funcnames[DSB_SHOOT], 7);
    damagepower = luaint(LUA, b-2, funcnames[DSB_SHOOT], 8);
    delta = luaint(LUA, b-1, funcnames[DSB_SHOOT], 9);
    
    if (b == -1)
        ddelta = luaint(LUA, -1, funcnames[DSB_SHOOT], 10);
    else
        ddelta = delta;
        
    validate_coord(funcnames[DSB_SHOOT], lev, xx, yy, travdir);
    
    if (tilepos < 0 || tilepos > 3) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "%s: Bad starting tile position", funcnames[DSB_SHOOT]);
        RETURN(0);
    }
        
    launch_object(inst, lev, xx, yy, travdir, tilepos, 
        throwpower, damagepower, delta, ddelta);
    
    RETURN(0);
}

int expl_animate(lua_State *LUA) {
    int table_animate = 0;
    static int complaints = 0;
    struct animap *lbmp = NULL;
    struct animap *b_table[256];
    struct animap *ret_bmp = NULL;
    int num_frames;
    int framedelay;
    
    INITPARMCHECK(3, funcnames[DSB_ANIMATE]);
    
    num_frames = luaint(LUA, -2, funcnames[DSB_ANIMATE], 2);
    framedelay = luaint(LUA, -1, funcnames[DSB_ANIMATE], 3);
    lua_pop(LUA, 2);
    
    memset(&b_table, 0, sizeof(struct animap*) * 256);
    
    // detect if the first parameter is a table not a bitmap
    if (lua_istable(LUA, -1)) {
        int n;
        struct animap *checkmap = NULL;
        luastacksize(6);
        for(n=1;n<256;n++) {
            lua_pushinteger(LUA, n);
            lua_gettable(LUA, -2);
            checkmap = luaoptbitmap(LUA, -1, funcnames[DSB_ANIMATE]);
            lua_pop(LUA, 1);
            if (!checkmap)
                break;
            lod_use(checkmap);
            b_table[n-1] = checkmap;
            if (n >= 2)
                table_animate = n;
        }
    }
    
    if (!table_animate) {
        lbmp = luabitmap(LUA, -1, funcnames[DSB_ANIMATE], 1); 
        lod_use(lbmp); 
    }
    
    if (framedelay < 1)
        framedelay = 1;
    else if (framedelay > 250)
        framedelay = 250;
    
    if (!table_animate && lbmp && (lbmp->flags & AM_VIRTUAL)) {
        if (complaints < 5) {
            gd.lua_nonfatal = 1;
            DSBLerror(LUA, "%s: Cannot animate Virtual Bitmap", funcnames[DSB_ANIMATE]);
        }
        ++complaints;
        RETURN(0);
    }
    
    ret_bmp = setup_animation(table_animate, lbmp, b_table, num_frames, framedelay);
    if (!table_animate && lbmp->lod) {
        lbmp->lod->d |= ALOD_ANIMATED;
        lbmp->lod->ani_num_frames = num_frames;
        lbmp->lod->ani_framedelay = framedelay;
    }
    
    if (ret_bmp)
        setup_lua_bitmap(LUA, ret_bmp);
    
    RETURN(1);
}

int expl_get_flystate(lua_State *LUA) {
    unsigned int inst;
    struct inst *ptr_inst;
    
    INITPARMCHECK(1, funcnames[DSB_GET_FLYSTATE]);
    
    inst = luaint(LUA, -1, funcnames[DSB_GET_FLYSTATE], 1);
    validate_instance(inst);
    
    ptr_inst = oinst[inst];
    
    if (ptr_inst->flytimer) {
        lua_pushinteger(LUA, ptr_inst->flytimer);
        lua_pushinteger(LUA, ptr_inst->facedir);
        lua_pushinteger(LUA, ptr_inst->damagepower);
        RETURN(3);
    }
    
    lua_pushnil(LUA);
    RETURN(1);
}

int expl_set_flystate(lua_State *LUA) {
    unsigned int inst;
    int ftimer;
    struct inst *ptr_inst;
    
    INITPARMCHECK(2, funcnames[DSB_SET_FLYSTATE]);
    
    inst = luaint(LUA, -2, funcnames[DSB_SET_FLYSTATE], 1);
    ftimer = luaint(LUA, -1, funcnames[DSB_SET_FLYSTATE], 2);
    validate_instance(inst);
    
    ptr_inst = oinst[inst];
    if (ftimer < 0) ftimer = 0;
    
    if (ptr_inst->flytimer) {
        ptr_inst->flytimer = ftimer;
        if (ftimer == 0) {
            drop_flying_inst(inst, ptr_inst);
            destroy_assoc_timer(inst, EVENT_MOVE_FLYER, -1);    
        }
    } else if (ftimer != 0) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "%s: Must dsb_shoot non-flying object", funcnames[DSB_SET_FLYSTATE]);                
    }

    RETURN(0);
}


int expl_set_openshot(lua_State *LUA) {
    int b = 0;
    unsigned int forbid;
    int forbid_timer = 0;
    unsigned int inst;
    struct inst *ptr_inst;
    int shotmode = OPENSHOT_GENERIC_OBJECT;
    
    INITVPARMCHECK(1, funcnames[DSB_SET_OPENSHOT]);
    
    if (lua_gettop(LUA) == 2) {
        forbid = luaint(LUA, -1, funcnames[DSB_SET_OPENSHOT], 2);
        forbid_timer = 4;
        b = -1;
        shotmode = OPENSHOT_PARTY_OBJECT;
    }
    inst = luaint(LUA, -1 + b, funcnames[DSB_SET_OPENSHOT], 1);
    validate_instance(inst);
    
    ptr_inst = oinst[inst];
    ptr_inst->openshot = shotmode;
    
    if (forbid_timer) {
        gd.forbid_move_timer = forbid_timer;
        gd.forbid_move_dir = forbid;
    }
    
    RETURN(0);
}

int expl_get_flydelta(lua_State *LUA) {
    unsigned int inst;
    struct inst *ptr_inst;
    unsigned int flyreps;
    int rv = 0;
    int rv2 = 0;
    
    INITPARMCHECK(1, funcnames[DSB_GET_FLYDELTA]);
    
    inst = luaint(LUA, -1, funcnames[DSB_GET_FLYDELTA], 1);
    validate_instance(inst);
    
    ptr_inst = oinst[inst];
    if (ptr_inst->flytimer) {
        set_inst_flycontroller(inst, ptr_inst);
        rv = ptr_inst->flycontroller->data;
        rv2 = ptr_inst->flycontroller->altdata;
    } else {
        lua_pushnil(LUA);
        lua_pushnil(LUA);
        RETURN(2);
    }
    
    lua_pushinteger(LUA, rv);
    lua_pushinteger(LUA, rv2);
    RETURN(2);
}

int expl_set_flydelta(lua_State *LUA) {
    unsigned int inst;
    struct inst *ptr_inst;
    unsigned int flyreps;
    int b = 0;
    int ndelta = 0;
    int ndamdelta = 0;
    
    INITVPARMCHECK(2, funcnames[DSB_SET_FLYDELTA]);
    if (lua_gettop(LUA) == 3)
        b = -1;
    
    inst = luaint(LUA, -2 + b, funcnames[DSB_SET_FLYDELTA], 1);
    ndelta = luaint(LUA, -1 + b, funcnames[DSB_SET_FLYDELTA], 2);
    if (b == -1)
        ndamdelta = luaint(LUA, -1, funcnames[DSB_SET_FLYDELTA], 3);
         
    validate_instance(inst);
    
    ptr_inst = oinst[inst];
    if (ptr_inst->flytimer) {
        set_inst_flycontroller(inst, ptr_inst);
        ptr_inst->flycontroller->data = ndelta;
        if (b == -1)
            ptr_inst->flycontroller->altdata = ndamdelta;    
    }
    
    RETURN(0);
}

int expl_get_flyreps(lua_State *LUA) {
    unsigned int inst;
    struct inst *ptr_inst;
    unsigned int flyreps;
    int rv = 0;
    
    INITPARMCHECK(1, funcnames[DSB_GET_FLYREPS]);
    
    inst = luaint(LUA, -1, funcnames[DSB_GET_FLYREPS], 1);
    validate_instance(inst);
    
    ptr_inst = oinst[inst];
    if (ptr_inst->flytimer) {
        set_inst_flycontroller(inst, ptr_inst);
        rv = ptr_inst->flycontroller->flyreps;
    } else {
        lua_pushnil(LUA);
        RETURN(1);
    }
    
    lua_pushinteger(LUA, rv);
    RETURN(1);
}

int expl_set_flyreps(lua_State *LUA) {
    unsigned int inst;
    struct inst *ptr_inst;
    unsigned int flyreps;
    
    INITPARMCHECK(2, funcnames[DSB_SET_FLYREPS]);
    
    inst = luaint(LUA, -2, funcnames[DSB_SET_FLYREPS], 1);
    flyreps = luaint(LUA, -1, funcnames[DSB_SET_FLYREPS], 2);
    validate_instance(inst);
    
    ptr_inst = oinst[inst];
    if (ptr_inst->flytimer) {
        set_inst_flycontroller(inst, ptr_inst);
        ptr_inst->flycontroller->flyreps = flyreps;
    }
    
    RETURN(0);
}

int expl_get_cropmax(lua_State *LUA) {
    unsigned int inst;
    struct inst *p_inst;
    struct obj_arch *p_arch;
    
    INITPARMCHECK(1, funcnames[DSB_GET_CROPMAX]);
    inst = luaint(LUA, -1, funcnames[DSB_GET_CROPMAX], 1);
    validate_instance(inst);
    p_inst = oinst[inst];
    p_arch = Arch(p_inst->arch);
    
    lua_pushinteger(LUA, p_arch->cropmax);
    RETURN(1);
}

int expl_3dsound(lua_State *LUA) {
    FMOD_SOUND *fs;
    int lev, xx, yy;
    int sh;
    int b;
    int loop = 0;
    char id[16];
    
    INITVPARMCHECK(4, funcnames[DSB_3DSOUND]);
    
    if (lua_gettop(LUA) == 4) {
        b = 0;
    } else {
        b = -1;
    }
    
    if (lua_isnil(LUA, b-4)) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "Sound is nil");
        RETURN(0);
    }   
    
    fs = luasound(LUA, b-4, id, funcnames[DSB_3DSOUND], 1);
    lev = luaint(LUA, b-3, funcnames[DSB_3DSOUND], 2);
    xx = luaint(LUA, b-2, funcnames[DSB_3DSOUND], 3);
    yy = luaint(LUA, b-1, funcnames[DSB_3DSOUND], 4);
    
    if (b == -1) {
        if (lua_isboolean(LUA, b)) {
            loop = lua_toboolean(LUA, b);
        }
    }
    
    if (gd.exestate < STATE_GAMEPLAY) {
        RETURN(0);
    }
    
    sh = play_3dsound(fs, id, lev, xx, yy, loop);
    
    lua_pushinteger(LUA, sh);
    
    RETURN(1);
}

int expl_sound(lua_State *LUA) {
    FMOD_SOUND *fs;
    int lev, xx, yy;
    int sh;
    int b;
    int loop = 0;
    char id[16];
    
    INITVPARMCHECK(1, funcnames[DSB_SOUND]);

    if (lua_gettop(LUA) == 1) {
        b = 0;
    } else {
        b = -1;
    }
    
    if (lua_isnil(LUA, b-1)) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "Sound is nil");
        RETURN(0);
    }  
    
    if (b == -1) {
        if (lua_isboolean(LUA, b)) {
            loop = lua_toboolean(LUA, b);
        }
    }
    
    if (gd.exestate < STATE_GAMEPLAY) {
        RETURN(0);
    }
    
    fs = luasound(LUA, b-1, id, funcnames[DSB_SOUND], 1);
    
    sh = play_ssound(fs, id, loop, 1);
    
    lua_pushinteger(LUA, sh);
    
    RETURN(1);
}

int expl_stopsound(lua_State *LUA) {
    int i;
    unsigned int shand;
    
    INITPARMCHECK(1, funcnames[DSB_STOPSOUND]);
    
    shand = luaint(LUA, -1, funcnames[DSB_STOPSOUND], 1);
    if (shand < 0)
        RETURN(0);
        
    stop_sound(shand);
    for(i=0;i<MUSIC_HANDLES;++i) {
        if (gd.cur_mus[i].uid && (shand == gd.cur_mus[i].chan)) {
            gd.cur_mus[i].preserve = 0;
            current_music_ended(i);
        }
    }
    
    RETURN(0);
}

int expl_checksound(lua_State *LUA) {
    unsigned int shand;
    int sval;
    
    INITPARMCHECK(1, funcnames[DSB_CHECKSOUND]);

    shand = luaint(LUA, -1, funcnames[DSB_CHECKSOUND], 1);
    if (shand < 0)
        RETURN(0);
        
    sval = check_sound(shand);
    
    lua_pushboolean(LUA, sval);
    RETURN(1);
}

int expl_get_soundvol(lua_State *LUA) {
    unsigned int shand;
    int vol;

    INITPARMCHECK(1, funcnames[DSB_GET_SOUNDVOL]);

    shand = luaint(LUA, -1, funcnames[DSB_GET_SOUNDVOL], 1);

    if (shand < 0)
        RETURN(0);
        
    vol = get_sound_vol(shand);
    
    if (vol < 0)
        lua_pushnil(LUA);
    else
        lua_pushinteger(LUA, vol);
        
    RETURN(1);
}

int expl_set_soundvol(lua_State *LUA) {
    unsigned int shand;
    unsigned int vol;

    INITPARMCHECK(2, funcnames[DSB_SET_SOUNDVOL]);

    shand = luaint(LUA, -2, funcnames[DSB_SET_SOUNDVOL], 1);
    vol = luaint(LUA, -1, funcnames[DSB_SET_SOUNDVOL], 2);

    if (shand < 0)
        RETURN(0);
        
    if (vol > 100) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "%s: Bad sound volume", funcnames[DSB_SET_SOUNDVOL]);
    }
    
    set_sound_vol(shand, vol);
    
    RETURN(0);
}

int expl_get_stat(lua_State *LUA) {
    unsigned int who;
    unsigned int which;
    
    INITPARMCHECK(2, funcnames[DSB_GET_STAT]);

    who = luaint(LUA, -2, funcnames[DSB_GET_STAT], 1);
    which = luaint(LUA, -1, funcnames[DSB_GET_STAT], 2);
    
    validate_champion(who);
    validate_stat(which);
    
    lua_pushinteger(LUA, gd.champs[who-1].stat[which]);
    
    RETURN(1);
}

int expl_set_stat(lua_State *LUA) {
    unsigned int who;
    unsigned int which;
    int sval;
    
    INITPARMCHECK(3, funcnames[DSB_SET_STAT]);

    who = luaint(LUA, -3, funcnames[DSB_SET_STAT], 1);
    which = luaint(LUA, -2, funcnames[DSB_SET_STAT], 2);
    sval = luaint(LUA, -1, funcnames[DSB_SET_STAT], 3);
    if (sval < gii.stat_minimum) sval = gii.stat_minimum;
    if (sval > gii.stat_maximum) sval = gii.stat_maximum; 
    validate_champion(who);
    validate_stat(which);
    gd.champs[who-1].stat[which] = sval;
    calculate_maxload(who);
    determine_load_color(who-1);  
    
    destroy_system_subrenderers();

    RETURN(0);
}

int expl_get_maxstat(lua_State *LUA) {
    unsigned int who;
    unsigned int which;
    
    INITPARMCHECK(2, funcnames[DSB_GET_MAXSTAT]);

    who = luaint(LUA, -2, funcnames[DSB_GET_MAXSTAT], 1);
    which = luaint(LUA, -1, funcnames[DSB_GET_MAXSTAT], 2);
    
    validate_champion(who);
    validate_stat(which);
    
    lua_pushinteger(LUA, gd.champs[who-1].maxstat[which]);
    
    RETURN(1);
}

int expl_set_maxstat(lua_State *LUA) {
    unsigned int who;
    unsigned int which;
    int sval;
    
    INITPARMCHECK(3, funcnames[DSB_SET_MAXSTAT]);

    who = luaint(LUA, -3, funcnames[DSB_SET_MAXSTAT], 1);
    which = luaint(LUA, -2, funcnames[DSB_SET_MAXSTAT], 2);
    sval = luaint(LUA, -1, funcnames[DSB_SET_MAXSTAT], 3);
    if (sval < gii.stat_minimum) sval = gii.stat_minimum;
    if (sval > gii.stat_maximum) sval = gii.stat_maximum;    
    validate_champion(who);
    validate_stat(which);
    gd.champs[who-1].maxstat[which] = sval;
    calculate_maxload(who);
    determine_load_color(who-1);  
    
    destroy_system_subrenderers();
    
    RETURN(0);
}

int expl_get_load(lua_State *LUA) {
    unsigned int who;    
    INITPARMCHECK(1, funcnames[DSB_GET_LOAD]);
    who = luaint(LUA, -1, funcnames[DSB_GET_LOAD], 1);
    validate_champion(who);    
    lua_pushinteger(LUA, gd.champs[who-1].load);  
    RETURN(1);
}

int expl_get_maxload(lua_State *LUA) {
    unsigned int who;    
    INITPARMCHECK(1, funcnames[DSB_GET_MAXLOAD]);
    who = luaint(LUA, -1, funcnames[DSB_GET_MAXLOAD], 1);
    validate_champion(who);    
    lua_pushinteger(LUA, gd.champs[who-1].maxload);  
    RETURN(1);
}

int expl_get_food(lua_State *LUA) {
    unsigned int who;
    
    INITPARMCHECK(1, funcnames[DSB_GET_FOOD]);

    who = luaint(LUA, -1, funcnames[DSB_GET_FOOD], 1);
    validate_champion(who);
    
    lua_pushinteger(LUA, gd.champs[who-1].food);
    
    RETURN(1);
}

int expl_get_water(lua_State *LUA) {
    unsigned int who;
    
    INITPARMCHECK(1, funcnames[DSB_GET_WATER]);

    who = luaint(LUA, -1, funcnames[DSB_GET_WATER], 1);
    validate_champion(who);
    
    lua_pushinteger(LUA, gd.champs[who-1].water);
    
    RETURN(1);
}

int expl_get_idle(lua_State *LUA) {
    unsigned int who;
    
    INITPARMCHECK(1, funcnames[DSB_GET_IDLE]);

    who = luaint(LUA, -1, funcnames[DSB_GET_IDLE], 1);
    validate_ppos(who);
    
    lua_pushinteger(LUA, gd.idle_t[who]);
    
    RETURN(1);
}

int expl_get_pfacing(lua_State *LUA) {
    unsigned int who;
    
    INITPARMCHECK(1, funcnames[DSB_GET_PFACING]);

    who = luaint(LUA, -1, funcnames[DSB_GET_PFACING], 1);
    validate_ppos(who);
    
    lua_pushinteger(LUA, gd.g_facing[who]);
    
    RETURN(1);
}

int expl_set_food(lua_State *LUA) {
    unsigned int who;
    int fv;
    
    INITPARMCHECK(2, funcnames[DSB_SET_FOOD]);

    STARTNONF();
    who = luaint(LUA, -2, funcnames[DSB_SET_FOOD], 1);
    fv = luaint(LUA, -1, funcnames[DSB_SET_FOOD], 2);
    validate_champion(who);
    ENDNONF();
    
    if (fv < 0) fv = 0;
    if (fv > MAX_FOOD) fv = MAX_FOOD;
    
    gd.champs[who-1].food = fv;
    calculate_maxload(who);
    determine_load_color(who-1);  
    
    destroy_system_subrenderers();
    
    RETURN(0);
}

int expl_set_water(lua_State *LUA) {
    unsigned int who;
    int wv;
    
    INITPARMCHECK(2, funcnames[DSB_SET_WATER]);

    STARTNONF();
    who = luaint(LUA, -2, funcnames[DSB_SET_WATER], 1);
    wv = luaint(LUA, -1, funcnames[DSB_SET_WATER], 2);
    validate_champion(who);
    ENDNONF();

    if (wv < 0) wv = 0;
    if (wv > MAX_FOOD) wv = MAX_FOOD;
    
    gd.champs[who-1].water = wv;
    calculate_maxload(who);
    determine_load_color(who-1);  
    
    destroy_system_subrenderers();
    
    RETURN(0);
}

int expl_set_idle(lua_State *LUA) {
    unsigned int who;
    int t;
    
    INITPARMCHECK(2, funcnames[DSB_SET_IDLE]);

    who = luaint(LUA, -2, funcnames[DSB_SET_IDLE], 1);
    t = luaint(LUA, -1, funcnames[DSB_SET_IDLE], 2);
    
    validate_ppos(who);
    if (t < 0 || t > 2000) t = 0;
    
    gd.idle_t[who] = t;
    
    RETURN(0);
}

int expl_set_pfacing(lua_State *LUA) {
    unsigned int who;
    unsigned int t;
    
    INITPARMCHECK(2, funcnames[DSB_SET_PFACING]);

    who = luaint(LUA, -2, funcnames[DSB_SET_PFACING], 1);
    t = luaint(LUA, -1, funcnames[DSB_SET_PFACING], 2);
    
    validate_ppos(who);
    if (t > 3) t = 0;
    
    gd.g_facing[who] = t;
    
    RETURN(0);
}

int expl_update_inventory_info(lua_State *LUA) {
    INITPARMCHECK(0, funcnames[DSB_UPDATE_II]);
    update_all_inventory_info();
    RETURN(0);
}

int expl_update_system(lua_State *LUA) {
    INITPARMCHECK(0, funcnames[DSB_UPDATE_SYSTEM]);
    update_all_system();
    RETURN(0);
}

int expl_delay_func(lua_State *LUA) {
    int dtime, regentry;
    
    INITPARMCHECK(2, funcnames[DSB_DELAY_FUNC]);
    dtime = luaint(LUA, -2, funcnames[DSB_DELAY_FUNC], 1);
    
    if (dtime) {
        int repeat_dtime = dtime; 
        if (dtime == DELAY_EOT_TIMER) {
            repeat_dtime = 0;
        }
        regentry = luaL_ref(LUA, LUA_REGISTRYINDEX);   
        add_timer(-1, EVENT_DO_FUNCTION, dtime, repeat_dtime, regentry, 0, 0);
    } else
        lc_call_topstack(0, "immed"); 
    
    RETURN(0);    
}

int expl_get_champname(lua_State *LUA) {
    int who;
    
    INITPARMCHECK(1, funcnames[DSB_GET_CHARNAME]);
    
    who = luaint(LUA, -1, funcnames[DSB_GET_CHARNAME], 1);
    validate_champion(who);
    
    lua_pushstring(LUA, gd.champs[who-1].name);   
    
    RETURN(1);
}

int expl_set_champname(lua_State *LUA) {
    int who;
    const char *cstr;
    
    INITPARMCHECK(2, funcnames[DSB_SET_CHARNAME]);
    
    who = luaint(LUA, -2, funcnames[DSB_SET_CHARNAME], 1);
    cstr = luastring(LUA, -1, funcnames[DSB_SET_CHARNAME], 2); 
    validate_champion(who);
    
    snprintf(gd.champs[who-1].name, 8, "%s", cstr);   
    
    RETURN(0);
}

int expl_get_champtitle(lua_State *LUA) {
    int who;
    
    INITPARMCHECK(1, funcnames[DSB_GET_CHARTITLE]);
    
    who = luaint(LUA, -1, funcnames[DSB_GET_CHARTITLE], 1);
    validate_champion(who);
    
    lua_pushstring(LUA, gd.champs[who-1].lastname);   
    
    RETURN(1);
}

int expl_set_champtitle(lua_State *LUA) {
    int who;
    const char *cstr;
    
    INITPARMCHECK(2, funcnames[DSB_SET_CHARTITLE]);
    
    who = luaint(LUA, -2, funcnames[DSB_SET_CHARTITLE], 1);
    cstr = luastring(LUA, -1, funcnames[DSB_SET_CHARTITLE], 2); 
    validate_champion(who);
    
    snprintf(gd.champs[who-1].lastname, 20, "%s", cstr);   
    
    RETURN(0);
}

int expl_write(lua_State *LUA) {
    const char *textmsg;
    unsigned int in_color;
    struct animap *in_bmp;
    int b = 0;
    int i_ttl = 50;
    
    INITVPARMCHECK(2, funcnames[DSB_WRITE]);
    if (lua_gettop(LUA) == 3) {
        b = -1;
        i_ttl = luaint(LUA, -1, funcnames[DSB_WRITE], 3);
        if (i_ttl < 1) i_ttl = 1;
        if (i_ttl > 32700) i_ttl = 32700; 
    }
    
    in_bmp = luaoptbitmap(LUA, -2 + b, funcnames[DSB_WRITE]);
    if (in_bmp) {
        in_color = makecol(255, 255, 255);
    } else {
        in_color = luargbval(LUA, -2 + b, funcnames[DSB_WRITE], 1);
    }
    textmsg = luastring(LUA, -1 + b, funcnames[DSB_WRITE], 2);
    
    pushtxt_cons_bmp(textmsg, in_color, in_bmp, i_ttl);
    RETURN(0);    
}

int expl_damage_popup(lua_State *LUA) {
    unsigned int ppos, dtype, damount;
    
    INITPARMCHECK(3, funcnames[DSB_DAMAGE_POPUP]);
    
    ppos = luaint(LUA, -3, funcnames[DSB_DAMAGE_POPUP], 1);
    dtype = luaint(LUA, -2, funcnames[DSB_DAMAGE_POPUP], 2);
    damount = luaint(LUA, -1, funcnames[DSB_DAMAGE_POPUP], 3);
    
    validate_ppos(ppos);
    validate_dtype(dtype);
    
    if (gd.party[ppos])
        do_damage_popup(ppos, damount, dtype);
    
    RETURN(0);
}

int expl_get_bar(lua_State *LUA) {
    unsigned int who;
    int which;
    
    INITPARMCHECK(2, funcnames[DSB_GET_BAR]);
    who = luaint(LUA, -2, funcnames[DSB_GET_BAR], 1);
    which = luaint(LUA, -1, funcnames[DSB_GET_BAR], 2);
    validate_champion(who);
    validate_barstat(which);  
    lua_pushinteger(LUA, gd.champs[who-1].bar[which]);
    
    RETURN(1);
}

int expl_get_maxbar(lua_State *LUA) {
    unsigned int who;
    int which;
    
    INITPARMCHECK(2, funcnames[DSB_GET_BAR]);
    who = luaint(LUA, -2, funcnames[DSB_GET_BAR], 1);
    which = luaint(LUA, -1, funcnames[DSB_GET_BAR], 2);
    validate_champion(who);
    validate_barstat(which);  
    lua_pushinteger(LUA, gd.champs[who-1].maxbar[which]);
    
    RETURN(1);
}

int expl_get_injury(lua_State *LUA) {
    unsigned int who;
    int which;
    int il;
    
    INITPARMCHECK(2, funcnames[DSB_GET_INJURY]);
    who = luaint(LUA, -2, funcnames[DSB_GET_INJURY], 1);
    which = luaint(LUA, -1, funcnames[DSB_GET_INJURY], 2);
    validate_champion(who);
    validate_injloc(which);  
    
    il = gd.champs[who-1].injury[which];
    
    if (il)
        lua_pushinteger(LUA, il);
    else
        lua_pushnil(LUA);
    
    RETURN(1);
}

int expl_set_bar(lua_State *LUA) {
    unsigned int who;
    int which;
    int nval;
    
    INITPARMCHECK(3, funcnames[DSB_SET_BAR]);

    who = luaint(LUA, -3, funcnames[DSB_SET_BAR], 1);
    which = luaint(LUA, -2, funcnames[DSB_SET_BAR], 2);
    nval = luaint(LUA, -1, funcnames[DSB_SET_BAR], 3);
    if (nval < 0) nval = 0;
    if (nval > 9990) nval = 9990;
    validate_champion(who);
    validate_barstat(which);
    
    setbarval(who, which, nval);
    calculate_maxload(who);
    determine_load_color(who-1);  
    
    RETURN(0);
}

int expl_set_maxbar(lua_State *LUA) {
    unsigned int who;
    int which;
    int nval;
    
    INITPARMCHECK(3, funcnames[DSB_SET_MAXBAR]);

    who = luaint(LUA, -3, funcnames[DSB_SET_MAXBAR], 1);
    which = luaint(LUA, -2, funcnames[DSB_SET_MAXBAR], 2);
    nval = luaint(LUA, -1, funcnames[DSB_SET_MAXBAR], 3);
    if (nval < 0) nval = 0;
    if (nval > 9990) nval = 9990;
    validate_champion(who);
    validate_barstat(which);   
    gd.champs[who-1].maxbar[which] = nval;
    calculate_maxload(who);
    determine_load_color(who-1);  
    RETURN(0);
}

int expl_set_injury(lua_State *LUA) {
    unsigned int who;
    int which;
    int val;
    
    INITPARMCHECK(3, funcnames[DSB_SET_INJURY]);
    who = luaint(LUA, -3, funcnames[DSB_SET_INJURY], 1);
    which = luaint(LUA, -2, funcnames[DSB_SET_INJURY], 2);
    val = luaint(LUA, -1, funcnames[DSB_SET_INJURY], 3);
    validate_champion(who);
    validate_injloc(which);  
    if (val < 0) {
        DSBLerror(LUA, "Bad injury value %d", val);
        RETURN(0);
    }
    
    if (val > 100) val = 100;
    
    gd.champs[who-1].injury[which] = val;
    calculate_maxload(who);
    RETURN(0);
}

int expl_give_xp(lua_State *LUA) {
    unsigned int who;
    int which, sub;
    int amt;
    
    INITPARMCHECK(4, funcnames[DSB_GIVE_XP]);

    who = luaint(LUA, -4, funcnames[DSB_GIVE_XP], 1);
    which = luaint(LUA, -3, funcnames[DSB_GIVE_XP], 2);
    sub = luaint(LUA, -2, funcnames[DSB_GIVE_XP], 3);
    amt = luaint(LUA, -1, funcnames[DSB_GIVE_XP], 4);

    validate_champion(who);
    validate_xpclass(which);
    validate_xpsub(which, sub);
    if (amt < 0)
        DSBLerror(LUA, "Invalid xp value %d", amt);
    
    give_xp(who-1, which, sub, amt);
    
    RETURN(0); 
}

int expl_put_ppos_to_relpos(lua_State *LUA) {
    int ppos;
    int dest_pos;
    int doswap;
    int gx, gy;
    int ap;
    int ox, oy;
    int outgoing_stor_ppos;
    int b_was_found;
    int i_stor_ppos;
    // dsb_put_ppos_at_tile
    const char *sname = funcnames[DSB_PUT_PPOS_TO_RELPOS];
    
    INITVPARMCHECK(3, sname); 
    ppos = luaint(LUA, -3, sname, 1);
    dest_pos = luaint(LUA, -2, sname, 2);
    doswap = lua_toboolean(LUA, -1);
    
    validate_ppos(ppos);
    validate_tilepos(dest_pos);
    
    ap = party_ist_peek(gd.a_party);
    
    gx = !!(dest_pos % 3);
    gy = (dest_pos > 1);
    
    if (gd.guypos[gy][gx][ap] && !doswap) {
        gd.lua_nonfatal++;
        DSBLerror(LUA, "Position %d is full and swap is false", dest_pos);      
        RETURN(0);
    }
    
    i_stor_ppos = ppos + 1;
    
    b_was_found = find_remove_from_pos_tolerant(ap, i_stor_ppos, &ox, &oy);
    if (!b_was_found) {
        gd.lua_nonfatal++;
        DSBLerror(LUA, "Ppos %d not found", ppos);
        RETURN(0);
    }
    
    outgoing_stor_ppos = gd.guypos[gy][gx][ap];
    gd.guypos[gy][gx][ap] = i_stor_ppos;
    gd.guypos[oy][ox][ap] = outgoing_stor_ppos;
    
    exec_lua_rearrange(i_stor_ppos, ox, oy, gx, gy);
    ppos_reassignment(ap, outgoing_stor_ppos, i_stor_ppos, ox, oy, gx, gy);
    
    RETURN(0); 
}

int expl_tile_ppos(lua_State *LUA) {
    int abstilepos;
    int relpos;
    int whothere;
    int b = 0;
    int ap;
    
    INITVPARMCHECK(1, funcnames[DSB_TILE_PPOS]);
    if (lua_gettop(LUA) == 2) {
        ap = luaint(LUA, -1, funcnames[DSB_PARTYPLACE], 5);
        b = -1;
        if (ap < 0) ap = 0;
        if (ap > 3) ap = 3;
    } else
        ap = party_ist_peek(gd.a_party);
    abstilepos = luaint(LUA, b-1, funcnames[DSB_TILE_PPOS], 1);
    
    validate_tilepos(abstilepos);
    
    relpos = ((abstilepos+4) - gd.p_face[ap]) % 4;
    whothere = gd.guypos[relpos/2][(relpos%3 > 0)][ap];
    
    if (whothere)
        lua_pushinteger(LUA, (whothere-1));
    else
        lua_pushnil(LUA);
    
    RETURN(1);    
}

int expl_ppos_tile(lua_State *LUA) {
    int ppos;
    int x, y, vp;
    int relpos, abspos;
    int b = 0;
    int ap;
    
    INITRPARMCHECK(1, 2, funcnames[DSB_PPOS_TILE]);
    if (lua_gettop(LUA) == 2) {
        ap = luaint(LUA, -1, funcnames[DSB_PPOS_TILE], 2);
        b = -1;
        if (ap < 0) ap = 0;
        if (ap > 3) ap = 3;
    } else
        ap = party_ist_peek(gd.a_party);
    
    ppos = luaint(LUA, b-1, funcnames[DSB_PPOS_TILE], 1);
    
    validate_ppos(ppos);
    ++ppos;

    for (y=0;y<2;y++) {
        for (x=0;x<2;x++) {
            if (ppos == gd.guypos[y][x][ap]) {
                goto BREAKLOOP;
            }
        }
    }
    BREAKLOOP:
    relpos = 3*y + x;
    if (relpos == 4) relpos = 2;
    
    // (3*2 + 2) == nothing found
    if (relpos == 8) {
        lua_pushnil(LUA);
        RETURN(1);
    }
    
    abspos = (relpos + gd.p_face[ap]) % 4;
    lua_pushinteger(LUA, abspos);
    
    RETURN(1);    
}  

int expl_ppos_char(lua_State *LUA) {
    int ppos;
    int b = 0;
    int ap;
    int s_ap;
    
    INITRPARMCHECK(1, 2, funcnames[DSB_PPOS_CHAR]);
    if (lua_gettop(LUA) == 2) {
        ap = luaint(LUA, -1, funcnames[DSB_PPOS_CHAR], 2);
        b = -1;
        if (ap < -1) ap = 0;
        if (ap > 3) ap = 3;
    } else
        ap = party_ist_peek(gd.a_party);
        
    ppos = luaint(LUA, b-1, funcnames[DSB_PPOS_CHAR], 1);

    validate_ppos(ppos);
    if (GMP_ENABLED()) {
        s_ap = mparty_containing(ppos);
    } else {
        ap = 0;
        s_ap = ap;
    }

    if (ap != -1 && s_ap != ap)
        lua_pushnil(LUA);
    else if (gd.party[ppos])
        lua_pushinteger(LUA, gd.party[ppos]);
    else
        lua_pushnil(LUA);
         
    RETURN(1);    
}

int expl_char_ppos(lua_State *LUA) {
    int who;
    int cn;
    
    INITPARMCHECK(1, funcnames[DSB_CHAR_PPOS]);
    who = luaint(LUA, -1, funcnames[DSB_CHAR_PPOS], 1);
    
    validate_champion(who);
    
    for(cn=0;cn<4;++cn) {
        if (gd.party[cn] == who) {
            if (who != gd.tmp_champ) {
                lua_pushinteger(LUA, cn);
                RETURN(1);
            }
        }
    }
    
    lua_pushnil(LUA);
    RETURN(1);    
}     

int expl_xp_level(lua_State *LUA) {
    int who;
    int cltype;
    int subskill;
    int level;
    
    INITPARMCHECK(3, funcnames[DSB_XP_LEVEL]);
    who = luaint(LUA, -3, funcnames[DSB_XP_LEVEL], 1);
    cltype = luaint(LUA, -2, funcnames[DSB_XP_LEVEL], 2);
    if (lua_isnil(LUA, -1))
        subskill = 0;
    else
        subskill = luaint(LUA, -1, funcnames[DSB_XP_LEVEL], 3);
    
    validate_champion(who);
    validate_xpclass(cltype);
    validate_xpsub(cltype, subskill);
    
    level = determine_mastery(0, who-1, cltype, subskill, 1);

    lua_pushinteger(LUA, level);
    RETURN(1);   
}

int expl_xp_level_nb(lua_State *LUA) {
    int who;
    int cltype;
    int subskill;
    int level;
    
    INITPARMCHECK(3, funcnames[DSB_XP_LEVEL_NB]);
    who = luaint(LUA, -3, funcnames[DSB_XP_LEVEL_NB], 1);
    cltype = luaint(LUA, -2, funcnames[DSB_XP_LEVEL_NB], 2);
    if (lua_isnil(LUA, -1))
        subskill = 0;
    else
        subskill = luaint(LUA, -1, funcnames[DSB_XP_LEVEL_NB], 3);
    
    validate_champion(who);
    validate_xpclass(cltype);
    validate_xpsub(cltype, subskill);
    
    level = determine_mastery(0, who-1, cltype, subskill, 0);

    lua_pushinteger(LUA, level);
    RETURN(1);   
}

int expl_get_bonus(lua_State *LUA) {
    int who;
    int cltype;
    int subskill;
    int level;
    int bonus;
    
    INITPARMCHECK(3, funcnames[DSB_GET_BONUS]);
    who = luaint(LUA, -3, funcnames[DSB_GET_BONUS], 1);
    cltype = luaint(LUA, -2, funcnames[DSB_GET_BONUS], 2);
    subskill = luaint(LUA, -1, funcnames[DSB_GET_BONUS], 3);
    
    validate_champion(who);
    validate_xpclass(cltype);
    validate_xpsub(cltype, subskill);
    bonus = gd.champs[who-1].lev_bonus[cltype][subskill];  
    
    lua_pushinteger(LUA, bonus);
    RETURN(1);   
}

int expl_set_bonus(lua_State *LUA) {
    int who;
    int cltype;
    int subskill;
    int level;
    int bonusval;
    
    INITPARMCHECK(4, funcnames[DSB_SET_BONUS]);
    who = luaint(LUA, -4, funcnames[DSB_SET_BONUS], 1);
    cltype = luaint(LUA, -3, funcnames[DSB_SET_BONUS], 2);
    subskill = luaint(LUA, -2, funcnames[DSB_SET_BONUS], 3);
    bonusval = luaint(LUA, -1, funcnames[DSB_SET_BONUS], 4);
    
    validate_champion(who);
    validate_xpclass(cltype);
    validate_xpsub(cltype, subskill);
    if (bonusval < (-1 * gd.max_lvl) || bonusval > gd.max_lvl)
        DSBLerror(LUA, "Bad xp bonus %d", bonusval);

    gd.champs[who-1].lev_bonus[cltype][subskill] = bonusval;    

    RETURN(0);   
}

int expl_msg(lua_State *LUA) {
    int delay;
    int args;
    unsigned int target;
    unsigned int msgtype;
    int payload = 0;
    int b_spushed = 0;
    int id_sender = 0;
    int o = 0;
    int ap = party_ist_peek(gd.a_party);
    int xdata = 0;
    
    int debugblock = 0;
    
    luaonstack(funcnames[DSB_MSG]);
    
    args = lua_gettop(LUA);
    
    /*
    if (!grcnt) {
        v_luaonstack(funcnames[DSB_MSG]);
        b_spushed = 1;
    }
    */
    
    if (args == 5) {
        id_sender = luaint(LUA, -1, funcnames[DSB_MSG], 5);
        if (id_sender < 0)
            id_sender = 0;
        o = 1;
    } else {
        if (args != 4)
            varparmerror(4, 5, funcnames[DSB_MSG]);
    }
    
    STARTNONF();
    delay = luaint(LUA, -4 - o, funcnames[DSB_MSG], 1);
    target = luaint(LUA, -3 - o, funcnames[DSB_MSG], 2);
    msgtype = luaint(LUA, -2 - o, funcnames[DSB_MSG], 3);
    if (lua_istable(LUA, -1 - o)) {
        // shove the payload into the exvar of a data container
        // and then pass along the id of the container.  
        // (not yet implemented)  
    } else {
        payload = luaint(LUA, -1 - o, funcnames[DSB_MSG], 4);
    }
    // system target
    if (target != -1) {
        validate_instance(target);
    }
    ENDNONF();
    
    if (delay) {
        if (delay < 0 && delay != DELAY_EOT_TIMER) { delay = 1; }
        
        add_timer_ex(target, EVENT_SEND_MESSAGE, delay,
            0, msgtype, payload, id_sender, ap, xdata);
    } else {
        deliver_msg(target, msgtype, payload, id_sender, ap, xdata);
    }
    
    /*
    if (b_spushed)
        upstack();
    */
    
    RETURN(0);
    
    //return 0;
}

int expl_delay_msgs_to(lua_State *LUA) {
    unsigned int inst;
    unsigned int delay;
    
    INITPARMCHECK(2, funcnames[DSB_DELAY_MSGS_TO]);

    inst = luaint(LUA, -2, funcnames[DSB_DELAY_MSGS_TO], 1);
    delay = luaint(LUA, -1, funcnames[DSB_DELAY_MSGS_TO], 2);
    
    validate_instance(inst);
    
    if (delay > 100000) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "bad delay value");
        delay = 100000;
    }
    
    delay_all_timers(inst, EVENT_SEND_MESSAGE, delay);
    
    RETURN(0);        
}

int expl_hide_mouse(lua_State *LUA) {
    int override_mode = HIDEMOUSEMODE_HIDE;
    
    INITVPARMCHECK(0, funcnames[DSB_HIDE_MOUSE]);
    
    if (lua_gettop(LUA) == 1) {
        if (lua_toboolean(LUA, -1)) {
            override_mode = HIDEMOUSEMODE_HIDE_OVERRIDE;
            gd.mouse_hcx = mouse_x;
            gd.mouse_hcy = mouse_y;        
        }
    }
    
    gd.mouse_hidden = override_mode;
    RETURN(0);    
}

int expl_show_mouse(lua_State *LUA) {
    INITPARMCHECK(0, funcnames[DSB_SHOW_MOUSE]);
    gd.mouse_hidden = HIDEMOUSEMODE_SHOW;
    RETURN(0);    
}

int expl_mouse_override(lua_State *LUA) {
    
    luaonstack(funcnames[DSB_MOUSE_OVERRIDE]);
    if (lua_gettop(LUA) == 0)
        clear_mouse_override();
    else {
        struct animap *mbmp;
        
        if (lua_gettop(LUA) != 1)
            varparmerror(0, 1, funcnames[DSB_MOUSE_OVERRIDE]);
            
        if (lua_isnil(LUA, -1) || lua_isboolean(LUA, -1)) {
            mbmp = NULL;
        } else {
            mbmp = luabitmap(LUA, -1, funcnames[DSB_MOUSE_OVERRIDE], 1);
        }
        
        clear_mouse_override();
        set_mouse_override(mbmp);
    }
    
    RETURN(0);  
}

int expl_mouse_state(lua_State *LUA) {
    INITPARMCHECK(0, funcnames[DSB_MOUSE_STATE]);
    
    luastacksize(4);
    lua_pushinteger(LUA, mouse_x);
    lua_pushinteger(LUA, mouse_y);
    lua_pushboolean(LUA, !!(mouse_b & 1));
    lua_pushboolean(LUA, !!(mouse_b & 2));
    
    RETURN(4);  
}

int expl_lock_game(lua_State *LUA) {
    unsigned int lockflags;
    luaonstack(funcnames[DSB_LOCK_GAME]);
    if (lua_gettop(LUA) == 0)
        lockflags = 0xFFFFFFFF;
    else {
        if (lua_gettop(LUA) != 1)
            varparmerror(0, 1, funcnames[DSB_LOCK_GAME]);

        lockflags = luaint(LUA, -1, funcnames[DSB_LOCK_GAME], 1);
    }
    
    lockflags_change(lockflags, 1);
    RETURN(0);
}

int expl_unlock_game(lua_State *LUA) {
    unsigned int lockflags;
    luaonstack(funcnames[DSB_LOCK_GAME]);
    if (lua_gettop(LUA) == 0)
        lockflags = 0xFFFFFFFF;
    else {
        if (lua_gettop(LUA) != 1)
            varparmerror(0, 1, funcnames[DSB_LOCK_GAME]);

        lockflags = luaint(LUA, -1, funcnames[DSB_LOCK_GAME], 1);
    }

    lockflags_change(lockflags, 0);
    
    // deliver queued messages when unlocking
    deliver_message_buffer();
    
    RETURN(0);
}

int expl_get_gamelocks(lua_State *LUA) {
    unsigned int lockflags;
    int lockval;
    
    INITPARMCHECK(1, funcnames[DSB_GET_GAMELOCKS]);
    lockflags = luaint(LUA, -1, funcnames[DSB_GET_GAMELOCKS], 1);
    
    lockval = lockflags_get(lockflags);
    
    if (lockval) {
        lua_pushinteger(LUA, lockval);
    } else
        lua_pushnil(LUA);
        
    RETURN(1);
}

int expl_visited(lua_State *LUA) {
    int lev, xx, yy;
    
    INITPARMCHECK(3, funcnames[DSB_VISITED]);

    lev = luaint(LUA, -3, funcnames[DSB_VISITED], 1);
    xx = luaint(LUA, -2, funcnames[DSB_VISITED], 2);
    yy = luaint(LUA, -1, funcnames[DSB_VISITED], 3);

    validate_level(lev);
    if (xx < 0 || xx >= dun[lev].xsiz) {
        PUSH_NIL_DONE:
        lua_pushnil(LUA);
        RETURN(1);
    }

    if (yy < 0 || yy >= dun[lev].ysiz) {
        goto PUSH_NIL_DONE;
    }
    
    if (dun[lev].t[yy][xx].w & 2)
        lua_pushboolean(LUA, 1);
    else
        lua_pushboolean(LUA, 0);

    RETURN(1);
}

int expl_attack_text(lua_State *LUA) {
    const char *at;
    char *s_at;
    
    INITPARMCHECK(1, funcnames[DSB_ATTACK_TEXT]);
    at = luastring(LUA, -1, funcnames[DSB_ATTACK_TEXT], 1);
    s_at = dsbstrdup(at);
    if (gd.attack_msg)
        dsbfree(gd.attack_msg);
    gd.attack_msg = s_at;
    gd.attack_msg_ttl = 3;    
    RETURN(0);
}

int expl_attack_damage(lua_State *LUA) {
    int dmg;
    
    INITPARMCHECK(1, funcnames[DSB_ATTACK_DAMAGE]);
    dmg = luaint(LUA, -1, funcnames[DSB_ATTACK_DAMAGE], 1);
    if (dmg < 1 || dmg > 999) 
        DSBLerror(LUA, "Bad dmg value %d", dmg);
        
    if (gd.attack_msg) {
        dsbfree(gd.attack_msg);
        gd.attack_msg = NULL;
    }
         
    gd.attack_dmg_msg = dmg;
    gd.attack_msg_ttl = 2;
    RETURN(0);
}

int expl_export(lua_State *LUA) {
    const char *vname;
    char *s_vname;   
    INITPARMCHECK(1, funcnames[DSB_EXPORT]);
    only_when_loading(LUA, funcnames[DSB_EXPORT]);
    vname = luastring(LUA, -1, funcnames[DSB_EXPORT], 1);
    s_vname = dsbstrdup(vname);
    add_export_gvar(s_vname);
    RETURN(0);
}

int expl_get_sleepstate(lua_State *LUA) {
    INITPARMCHECK(0, funcnames[DSB_GET_SLEEPSTATE]);
    if (viewstate == VIEWSTATE_SLEEPING)
        lua_pushinteger(LUA, 1);
    else
        lua_pushnil(LUA);   
    RETURN(1);
}

int expl_wakeup(lua_State *LUA) {
    INITPARMCHECK(0, funcnames[DSB_WAKEUP]);
    if (viewstate == VIEWSTATE_SLEEPING)
        wake_up();
        
    RETURN(0);
}
int expl_set_tint(lua_State *LUA) {
    unsigned int m;
    unsigned int tintv;
    struct inst *p_inst;
    int b = 0;
    int intensity = 127;

    INITVPARMCHECK(2, funcnames[DSB_SET_TINT]);
    if (lua_gettop(LUA) == 3) {
        int i_error = 0;
        b = -1;
        intensity = luaint(LUA, -1, funcnames[DSB_SET_TINT], 3);
        if (intensity < 0) {
            i_error = intensity;
            intensity = 0;
        }
        if (intensity > 255) {
            i_error = intensity;
            intensity = 255;
        }
        if (i_error) {
            gd.lua_nonfatal = 1;
            DSBLerror(LUA, "Bad intensity value %d", i_error);
        }
    }
    m = luaint(LUA, -2 + b, funcnames[DSB_SET_TINT], 1);
    tintv = luargbval(LUA, -1 + b, funcnames[DSB_SET_TINT], 2);
    
    if (tintv == 0 && b == 0)
        intensity = 0;
    
    validate_instance(m);
    p_inst = oinst[m];
    p_inst->tint = tintv;
    p_inst->tint_intensity = intensity;

    RETURN(0);
}

int expl_set_xp_multiplier(lua_State *LUA) {
    int lev;
    int xpm;
    INITPARMCHECK(2, funcnames[DSB_SET_XPM]);
    lev = luaint(LUA, -2, funcnames[DSB_SET_XPM], 1);
    xpm = luaint(LUA, -1, funcnames[DSB_SET_XPM], 2);
    validate_level(lev);
    if (xpm < 0 || xpm > 999)
        DSBLerror(LUA, "Invalid xp_multiplier %d", xpm);
    dun[lev].xp_multiplier = xpm;
    RETURN(0);
}

int expl_get_temp_xp(lua_State *LUA) {
    unsigned int who;
    int which, sub;
    
    INITPARMCHECK(3, funcnames[DSB_GET_TEMP_XP]);
    who = luaint(LUA, -3, funcnames[DSB_GET_TEMP_XP], 1);
    which = luaint(LUA, -2, funcnames[DSB_GET_TEMP_XP], 2);
    sub = luaint(LUA, -1, funcnames[DSB_GET_TEMP_XP], 3);
    validate_champion(who);
    validate_xpclass(which);
    validate_xpsub(which, sub);
    
    lua_pushinteger(LUA, gd.champs[who-1].xp_temp[which][sub]);
    
    RETURN(1);         
}

int expl_set_temp_xp(lua_State *LUA) {
    unsigned int who;
    int which, sub;
    int amt;
    
    INITPARMCHECK(4, funcnames[DSB_SET_TEMP_XP]);
    who = luaint(LUA, -4, funcnames[DSB_SET_TEMP_XP], 1);
    which = luaint(LUA, -3, funcnames[DSB_SET_TEMP_XP], 2);
    sub = luaint(LUA, -2, funcnames[DSB_SET_TEMP_XP], 3);
    amt = luaint(LUA, -1, funcnames[DSB_SET_TEMP_XP], 4);
    validate_champion(who);
    validate_xpclass(which);
    validate_xpsub(which, sub);
    if (amt < 0)
        DSBLerror(LUA, "Invalid temp xp value %d", amt);
    
    gd.champs[who-1].xp_temp[which][sub] = amt;
    
    RETURN(0);     
}

int expl_ai(lua_State *LUA) {
    unsigned int inst;
    unsigned int ai_msg;
    int parm, s;
    int b;
    int lparm = -5999;
    
    INITVPARMCHECK(3, funcnames[DSB_AI]);
    
    if (lua_gettop(LUA) == 4) {
        b = -1;
    } else {
        b = 0;
    }
    
    STARTNONF();
    inst = luaint(LUA, b-3, funcnames[DSB_AI], 1);
    ai_msg = luaint(LUA, b-2, funcnames[DSB_AI], 2);
    parm = luaint(LUA, b-1, funcnames[DSB_AI], 3);
    if (b == -1) {
        lparm = luaint(LUA, -1, funcnames[DSB_AI], 4);
    }
    validate_instance(inst);
    ENDNONF();
    
    if (!oinst[inst]->ai) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "ai msg sent to non-monster inst %d", inst);
        RETURN(0);
    }
    
    s = ai_msg_handler(inst, ai_msg, parm, lparm);
    
    if (gd.lua_return_done) {
        int rv = gd.lua_return_done;
        gd.lua_return_done = 0;
        RETURN(rv);
    }
    
    if (gd.lua_bool_return) {
        gd.lua_bool_return = 0;
        lua_pushboolean(LUA, s);
    } else {
        if (s == 0 && !gd.zero_int_result)
            lua_pushnil(LUA);
        else {
            gd.zero_int_result = 0;
            lua_pushinteger(LUA, s);
        }
    }
        
    RETURN(1);
}

int expl_ai_boss(lua_State *LUA) {
    int boss;
    unsigned int inst;
    int no_validation = 0;
    
    INITVPARMCHECK(1, funcnames[DSB_AI_BOSS]);
    if (lua_gettop(LUA) == 2) {
        no_validation = lua_toboolean(LUA, -1);
        lua_pop(LUA, 1);
    }
    
    inst = luaint(LUA, -1, funcnames[DSB_AI_BOSS], 1);
    validate_instance(inst);
    
    if (!oinst[inst]->ai) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "Instance %d is not a monster", inst);
        lua_pushnil(LUA);
        RETURN(1);
    }
    boss = oinst[inst]->ai->boss;

    if (!no_validation) {
        // make sure it's not a bad boss
        if (boss)
            boss = monvalidateboss(oinst[inst], boss);
    
        // no boss? fine, i'll be the boss
        if (boss == 0) {
            int num = 0;
            struct inst_loc *team = monster_groupup(inst, oinst[inst], &num, 1);
    
            while (team != NULL) {
                oinst[team->i]->ai->boss = inst;
                team = team->n;
            }
    
            boss = inst;
        }
    }
    
    lua_pushinteger(LUA, boss);
        
    RETURN(1);
}

int expl_ai_subordinates(lua_State *LUA) {
    int boss;
    unsigned int inst;
    struct inst_loc *team;
    int frozenalso = 0;
    int num = 0;
    int b = 0;

    INITVPARMCHECK(1, funcnames[DSB_AI_SUBORDINATES]);
    
    if (lua_gettop(LUA) == 2) {
        frozenalso = lua_toboolean(LUA, -1);
        b = -1;
    }
    
    STARTNONF();
    inst = luaint(LUA, -1 + b, funcnames[DSB_AI_SUBORDINATES], 1);
    validate_instance(inst);
    ENDNONF();
    
    // get rid of this parameter
    lua_pop(LUA, 1);
    
    if (!oinst[inst]->ai) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "Instance %d is not a monster", inst);
        lua_pushnil(LUA);
        lua_pushnil(LUA);
        RETURN(2);
    }
    
    boss = oinst[inst]->ai->boss;
    if (boss != inst) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "Instance %d is not the group boss (%d is)", inst, boss);
        inst = boss;
    }

    team = monster_groupup(inst, oinst[inst], &num, 0);

    inst_loc_to_luatable(LUA, team, frozenalso, &num);
    lua_setglobal(LUA, "__gt_LUA_GTABLE");
    
    lua_pushinteger(LUA, num);
    lua_getglobal(LUA, "__gt_LUA_GTABLE");
    
    RETURN(2);
}

int expl_ai_promote(lua_State *LUA) {
    int boss;
    unsigned int inst;
    struct inst_loc *team;
    struct timer_event *con;
    int num = 0;
    int ungroup = 0;

    INITPARMCHECK(1, funcnames[DSB_AI_PROMOTE]);

    STARTNONF();
    inst = luaint(LUA, -1, funcnames[DSB_AI_PROMOTE], 1);
    validate_instance(inst);
    ENDNONF();

    // get rid of this parameter
    lua_pop(LUA, 1);

    if (!oinst[inst]->ai) {
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "Instance %d is not a monster", inst);
        RETURN(0);
    }

    if (oinst[inst]->ai->ai_flags & AIF_UNGROUPED) {
        ungroup = 1;
        gd.ungrouphack++;
    }
    team = monster_groupup(inst, oinst[inst], &num, 1);
    if (ungroup)
        gd.ungrouphack--;
    
    con = oinst[inst]->ai->controller;
    con->time_until = con->maxtime;
    
    while (team != NULL) {
        oinst[team->i]->ai->boss = inst;
        team = team->n;
    }

    RETURN(0);
}

int expl_get_light(lua_State *LUA) {
    int i_l;
    INITPARMCHECK(1, funcnames[DSB_GET_LIGHT]);

    STARTNONF();
    i_l = luaint(LUA, -1, funcnames[DSB_GET_LIGHT], 1);
    validate_lightindex(i_l);
    ENDNONF();
    
    lua_pushinteger(LUA, gd.lightlevel[i_l]);
    RETURN(1);
}

int expl_get_light_total(lua_State *LUA) {
    int cparty;    
    INITPARMCHECK(0, funcnames[DSB_GET_LIGHT_TOTAL]);
    
    cparty = party_ist_peek(gd.a_party);
    if (gd.p_lev[cparty] < 0)
        lua_pushinteger(LUA, 0);
    else {
        struct dungeon_level *dd;
        dd = &(dun[gd.p_lev[cparty]]);
        lua_pushinteger(LUA, calc_total_light(dd));
    }
    RETURN(1);
}

int expl_set_light(lua_State *LUA) {
    int i_l, ll;
    INITPARMCHECK(2, funcnames[DSB_SET_LIGHT]);
    
    STARTNONF();
    i_l = luaint(LUA, -2, funcnames[DSB_SET_LIGHT], 1);
    ll = luaint(LUA, -1, funcnames[DSB_SET_LIGHT], 2);
    validate_lightindex(i_l);
    ENDNONF();
    
    gd.lightlevel[i_l] = ll;
    RETURN(0);
}

int expl_set_light_totalmax(lua_State *LUA) {
    int lmax;
    INITPARMCHECK(1, funcnames[DSB_SET_LIGHT_TOTALMAX]);
    
    STARTNONF();
    if (lua_isnil(LUA, -1))
        lmax = 255;
    else
        lmax = luaint(LUA, -1, funcnames[DSB_SET_LIGHT_TOTALMAX], 2);
    ENDNONF();
    
    if (lmax < 0) lmax = 0;
    if (lmax > 255) lmax = 255;
    
    gd.max_total_light = lmax;
    RETURN(0);
}

int expl_get_leader(lua_State *LUA) {
    INITPARMCHECK(0, funcnames[DSB_GET_LEADER]);
    if (gd.leader < 0)
        lua_pushnil(LUA);
    else
        lua_pushinteger(LUA, gd.leader);
    RETURN(1);
}

int expl_get_caster(lua_State *LUA) {
    INITPARMCHECK(0, funcnames[DSB_GET_CASTER]);
    if (gd.leader < 0)
        lua_pushnil(LUA);
    else
        lua_pushinteger(LUA, gd.whose_spell);
    RETURN(1);
}


int expl_lookup_global(lua_State *LUA) {
    const char *s;
    
    INITPARMCHECK(1, funcnames[DSB_LOOKUP_GLOBAL]);
    s = luastring(LUA, -1, funcnames[DSB_LOOKUP_GLOBAL], 1);

    lua_getglobal(LUA, s);
    RETURN(1);
}

int expl_cur_inv(lua_State *LUA) {

    INITPARMCHECK(0, funcnames[DSB_CUR_INV]);
    
    if (viewstate == VIEWSTATE_INVENTORY ||
        viewstate == VIEWSTATE_CHAMPION)
    {
        lua_pushinteger(LUA, gd.who_look);
    } else
        lua_pushnil(LUA);
        
    RETURN(1);
}

int expl_textformat(lua_State *LUA) {
    int cpl, ypl, maxlines;
    
    INITPARMCHECK(3, funcnames[DSB_TEXTFORMAT]);
    
    cpl = luaint(LUA, -3, funcnames[DSB_TEXTFORMAT], 1);
    ypl = luaint(LUA, -2, funcnames[DSB_TEXTFORMAT], 2);
    maxlines = luaint(LUA, -1, funcnames[DSB_TEXTFORMAT], 3);
    
    if (cpl < 2)
        DSBLerror(LUA, "Bad chars per line %d", cpl);

    if (cpl > 255)
        cpl = 255;
        
    if (ypl < 1 || ypl > 255)
        DSBLerror(LUA, "Bad Y-offset %d", ypl);
        
    if (maxlines < 1 || maxlines > 255)
        DSBLerror(LUA, "Bad MaxLines %d", maxlines);
    
    gd.t_ypl = ypl;
    gd.t_cpl = cpl;
    gd.t_maxlines = maxlines;
    
    RETURN(0);
}

int expl_party_scanfor(lua_State *LUA) {
    unsigned int arch;
    unsigned int i_inst;
    int ppos;
    
    INITVPARMCHECK(1, funcnames[DSB_PARTY_SCANFOR]);
    
    if (lua_gettop(LUA) == 2) {
        ppos = luaint(LUA, -1, funcnames[DSB_PARTY_SCANFOR], 2);
        validate_ppos(ppos);
        lua_pop(LUA, 1);
    } else
        ppos = -1;
    arch = luaobjarch(LUA, -1, funcnames[DSB_PARTY_SCANFOR]);
    i_inst = scanfor_all_owned(ppos, arch);
    
    if (i_inst) lua_pushinteger(LUA, i_inst);
    else lua_pushnil(LUA);
        
    RETURN(1);
}

// dsb_fullscreen(bitmap/drawfunc, clickfunc, updatefunc, draw_ptr, [fade mode]);
int expl_fullscreen(lua_State *LUA) {
    unsigned int push_framecounter;
    int rv;
    struct fullscreen *fsd;

    INITVPARMCHECK(4, funcnames[DSB_FULLSCREEN]);

    fsd = dsbcalloc(1, sizeof(struct fullscreen));
    
    fsd->fs_fademode = 0;
    if (lua_gettop(LUA) == 5) {
        if (lua_istable(LUA, -1)) {
            int bkg_draw;
            
            fsd->fs_fademode = boollookup("fade", 0);
            
            bkg_draw = boollookup("game_draw", 0);
            if (bkg_draw)
                fsd->fs_bflags |= FS_B_DRAW_GAME;
            
            lua_pop(LUA, 1);    
        } else {
            fsd->fs_fademode = lua_toboolean(LUA, -1);
            lua_pop(LUA, 1); 
        }
    }

    if (lua_isboolean(LUA, -1))
        fsd->fs_mousemode = !!(lua_toboolean(LUA, -1));
    else if (lua_isnil(LUA, -1))
        fsd->fs_mousemode = FS_NOMOUSE;
    else {
        fsd->mouse = luabitmap(LUA, -1, funcnames[DSB_FULLSCREEN], 4);
        fsd->fs_mousemode = FS_MOUSECUSTOM;
    }
    lua_pop(LUA, 1);
    
    if (lua_isfunction(LUA, -1))
        fsd->lr_update = luaL_ref(LUA, LUA_REGISTRYINDEX);
    else {
        fsd->lr_update = LUA_NOREF;
        lua_pop(LUA, 1);
    }
        
    if (lua_isfunction(LUA, -1))
        fsd->lr_on_click = luaL_ref(LUA, LUA_REGISTRYINDEX);
    else {
        fsd->lr_on_click = LUA_NOREF;
        lua_pop(LUA, 1);
    }
    
    fsd->lr_bkg_draw = LUA_NOREF;
    if (lua_isfunction(LUA, -1)) {
        fsd->lr_bkg_draw = luaL_ref(LUA, LUA_REGISTRYINDEX);
        fsd->bkg = NULL;
    } else if (lua_isnil(LUA, -1)) {
        fsd->bkg = NULL;
    } else {
        fsd->bkg = luabitmap(LUA, -1, funcnames[DSB_FULLSCREEN], 1);
    }
    
    fsd->fs_scrbuf = NULL;

    upstack();
    
    push_framecounter = gd.framecounter;
    gd.framecounter = 0; 
    rv = fullscreen_mode(fsd);
    while (mouse_b) Sleep(10);
    gd.framecounter = push_framecounter;
    
    luaL_unref(LUA, LUA_REGISTRYINDEX, fsd->lr_update);
    luaL_unref(LUA, LUA_REGISTRYINDEX, fsd->lr_on_click);
    luaL_unref(LUA, LUA_REGISTRYINDEX, fsd->lr_bkg_draw);
    dsbfree(fsd);
    if (rv) {
        lua_pushinteger(LUA, rv);
        return 1;
    } else
        return 0;
}

int expl_get_lastmethod(lua_State *LUA) {
    int ppos;
    
    INITPARMCHECK(1, funcnames[DSB_GET_LASTMETHOD]);
    
    STARTNONF();
    ppos = luaint(LUA, -1, funcnames[DSB_GET_LASTMETHOD], 1);
    validate_ppos(ppos);
    ENDNONF();

    if (gd.lastmethod[ppos][0]) {
        lua_pushinteger(LUA, gd.lastmethod[ppos][0]);
        lua_pushinteger(LUA, gd.lastmethod[ppos][1]);
        
        if (gd.lastmethod[ppos][2])
            lua_pushinteger(LUA, gd.lastmethod[ppos][2]);
        else
            lua_pushnil(LUA);
            
        RETURN(3);
    } else {
        lua_pushnil(LUA);
        RETURN(1);
    }
}

int expl_set_lastmethod(lua_State *LUA) {
    int b = 0;
    int method_num;
    int location;
    int what = 0;
    int no_item = 0;
    int ppos;
    
    INITVPARMCHECK(3, funcnames[DSB_SET_LASTMETHOD]);
    if (lua_gettop(LUA) == 4) {
        b = -1;
    } else {
        no_item = 1;
    }
    
    STARTNONF();
    ppos = luaint(LUA, -3+b, funcnames[DSB_SET_LASTMETHOD], 1);
    validate_ppos(ppos);
    method_num = luaint(LUA, -2+b, funcnames[DSB_SET_LASTMETHOD], 2);
    location = luaint(LUA, -1+b, funcnames[DSB_SET_LASTMETHOD], 3);
    if (!no_item) {
        what = luaint(LUA, -1, funcnames[DSB_SET_LASTMETHOD], 4);
        validate_instance(what);
    }
    ENDNONF();
    
    gd.lastmethod[ppos][0] = method_num;
    gd.lastmethod[ppos][1] = location;
    gd.lastmethod[ppos][2] = what;

    RETURN(0);
}

int expl_get_pendingspell(lua_State *LUA) {
    int ppos, n;
    int b = 0;
    int tbl = 0;
    int elem = 8;

    INITVPARMCHECK(1, funcnames[DSB_GET_PENDINGSPELL]);
    STARTNONF();
    if (lua_gettop(LUA) == 2) {
        tbl = lua_toboolean(LUA, -1);
        b = -1;
        if (tbl)
            elem = 1;
    }
    ppos = luaint(LUA, -1+b, funcnames[DSB_GET_PENDINGSPELL], 1);
    validate_ppos(ppos);
    ENDNONF();
    
    luastacksize(8);
    
    if (tbl)
        lua_newtable(LUA);
    
    for(n=0;n<8;++n) {
        if (tbl)
            lua_pushinteger(LUA, n+1);
            
        lua_pushinteger(LUA, gd.i_spell[ppos][n]);
        
        if (tbl)
            lua_settable(LUA, -3);
    }

    RETURN(elem);
}

int expl_set_pendingspell(lua_State *LUA) {
    int pr[8] = { 0 };
    int nrs;

    luaonstack(funcnames[DSB_SET_PENDINGSPELL]);

    nrs = lua_gettop(LUA) - 1;
    if (nrs > 8)
        nrs = 8;

    if (nrs > 0) {
        int rn;
        int ppos;

        STARTNONF();
        ppos = luaint(LUA, -1*(nrs+1), funcnames[DSB_SET_PENDINGSPELL], 1);
        validate_ppos(ppos);
        ENDNONF();
        
        memset(gd.i_spell[ppos], 0, 8);
        
        for (rn=0;rn<nrs;rn++) {
            unsigned int ln = luaint(LUA, -1*(nrs-rn),
                funcnames[DSB_SET_PENDINGSPELL], 2+rn);
                
            gd.i_spell[ppos][rn] = ln;
        }
    }

    RETURN(0);
}

int expl_wallset_override_floor(lua_State *LUA) {
    const char *myfunc = funcnames[DSB_WALLSET_OVERRIDE_FLOOR];
    
    INITVPARMCHECK(1, myfunc);
    
    if (lua_isnil(LUA, -1)) {
        condfree(gfxctl.floor_override_name);
        gfxctl.floor_override = NULL;
        condfree(gfxctl.floor_alt_override_name);
        gfxctl.floor_alt_override = NULL;
    } else {
        int p;
        const char *s_wname;
        
        if (lua_gettop(LUA) != 2) {
            gd.lua_nonfatal = 1;
            parmerror(2, myfunc);
            RETURN(0);
        }
    
        s_wname = luastring(LUA, -2, myfunc, 1);

        gfxctl.floor_override = grab_from_gfxtable(s_wname);
        gfxctl.floor_override_name = dsbstrdup(s_wname);
        
        if (lua_isboolean(LUA, -1)) {
            p = lua_toboolean(LUA, -1);
            if (p)
                gfxctl.override_flip |= OV_FLOOR;
            else
                gfxctl.override_flip &= ~(OV_FLOOR);
                
            condfree(gfxctl.floor_alt_override_name);
            gfxctl.floor_alt_override = NULL;
            
        } else {
            gfxctl.override_flip &= ~(OV_FLOOR);
            s_wname = luastring(LUA, -1, myfunc, 2);
            gfxctl.floor_alt_override = grab_from_gfxtable(s_wname);
            gfxctl.floor_alt_override_name = dsbstrdup(s_wname);
        }
    }

    RETURN(0);
}

int expl_wallset_override_roof(lua_State *LUA) {
    const char *myfunc = funcnames[DSB_WALLSET_OVERRIDE_ROOF];

    INITVPARMCHECK(1, myfunc);
    
    if (lua_isnil(LUA, -1)) {
        condfree(gfxctl.roof_override_name);
        gfxctl.roof_override = NULL;
        condfree(gfxctl.roof_alt_override_name);
        gfxctl.roof_alt_override = NULL;
    } else {
        int p;
        const char *s_wname;
        
        if (lua_gettop(LUA) != 2) {
            gd.lua_nonfatal = 1;
            parmerror(2, myfunc);
            RETURN(0);
        }

        s_wname = luastring(LUA, -2, myfunc, 1);

        gfxctl.roof_override = grab_from_gfxtable(s_wname);
        gfxctl.roof_override_name = dsbstrdup(s_wname);
        
        if (lua_isboolean(LUA, -1)) {
            p = lua_toboolean(LUA, -1);
            if (p)
                gfxctl.override_flip |= OV_ROOF;
            else
                gfxctl.override_flip &= ~(OV_ROOF);
                
            condfree(gfxctl.roof_alt_override_name);
            gfxctl.roof_alt_override = NULL;
        
        } else {
            gfxctl.override_flip &= ~(OV_ROOF);
            s_wname = luastring(LUA, -1, myfunc, 2);
            gfxctl.roof_alt_override = grab_from_gfxtable(s_wname);
            gfxctl.roof_alt_override_name = dsbstrdup(s_wname);
        }
    }

    RETURN(0);
}

int expl_wallset_flip_floor(lua_State *LUA) {
    const char *myfunc = funcnames[DSB_WALLSET_FLIP_FLOOR];
    int ws;
    struct wallset *cws;

    INITPARMCHECK(2, myfunc);
    
    ws = luawallset(LUA, -2, myfunc);
    cws = ws_tbl[ws];
    
    if (lua_isboolean(LUA, -1)) {
        int flip = lua_toboolean(LUA, -1);
        cws->alt_floor = NULL;
        if (flip)
            cws->flipflags |= OV_FLOOR;
        else
            cws->flipflags &= ~(OV_FLOOR);
    } else {
        struct animap *amap;
        amap = luabitmap(LUA, -1, myfunc, 2);
        cws->alt_floor = amap;
    }
    
    lstacktop();
    RETURN(0);
}

int expl_wallset_flip_roof(lua_State *LUA) {
    const char *myfunc = funcnames[DSB_WALLSET_FLIP_ROOF];
    int ws;
    struct wallset *cws;

    INITPARMCHECK(2, myfunc);

    ws = luawallset(LUA, -2, myfunc);
    cws = ws_tbl[ws];

    if (lua_isboolean(LUA, -1)) {
        int flip = lua_toboolean(LUA, -1);
        cws->alt_roof = NULL;
        if (flip)
            cws->flipflags |= OV_ROOF;
        else
            cws->flipflags &= ~(OV_ROOF);
    } else {
        struct animap *amap;
        amap = luabitmap(LUA, -1, myfunc, 2);
        cws->alt_roof = amap;
    }

    lstacktop();
    RETURN(0);
}

int expl_wallset_always_draw_floor(lua_State *LUA) {
    const char *myfunc = funcnames[DSB_WALLSET_ALWAYS_DRAW_FLOOR];
    int ws;
    struct wallset *cws;
    unsigned int nflags = 0;
    int set = 0;

    INITPARMCHECK(2, myfunc);

    ws = luawallset(LUA, -2, myfunc);
    cws = ws_tbl[ws];
    nflags = cws->flags;

    if (lua_isboolean(LUA, -1)) {
        if (lua_toboolean(LUA, -1)) {
            set = 1;
        }
    }

    if (set) {
        nflags |= WSF_ALWAYS_DRAW;
    } else {
        nflags &= ~(WSF_ALWAYS_DRAW);
    }
    
    cws->flags = nflags;

    lstacktop();
    RETURN(0);    
    
}

int expl_cache_invalidate(lua_State *LUA) {
    const char *myfunc = funcnames[DSB_CACHE_INVALIDATE];
    unsigned int objarch;
    struct obj_arch *p_arch;
    
    INITPARMCHECK(2, myfunc);
    
    objarch = luaobjarch(LUA, -1, myfunc);
    if (objarch == 0xFFFFFFFF) {
        lstacktop();
        RETURN(0);
    }   
    p_arch = Arch(objarch);
    
    invalidate_scale_cache(p_arch);
    
    lstacktop();
    RETURN(0);
}

#define SET_IF_KEY_PUSHED(V, S) \
    if (key[V]) {\
        lua_pushstring(LUA, S);\
        lua_pushboolean(LUA, 1);\
        lua_settable(LUA, -3);\
    }

int expl_poll_keyboard(lua_State *LUA) {
    const char *myfunc = funcnames[DSB_POLL_KEYBOARD];

    INITPARMCHECK(0, myfunc);
   
    if (keypressed()) {
        int i_fullkey = readkey();
        int c = (i_fullkey & 0xFF);
        int code = (i_fullkey & 0xFF00) >> 8;
        char keystring[2] = { 0, 0 };
        
        keystring[0] = c;
        
        lua_newtable(LUA);
        lua_pushstring(LUA, "key");
        lua_pushstring(LUA, keystring);
        lua_settable(LUA, -3);
        lua_pushstring(LUA, "keycode");
        lua_pushinteger(LUA, c);
        lua_settable(LUA, -3);
        lua_pushstring(LUA, "uppercode");
        lua_pushinteger(LUA, code);
        lua_settable(LUA, -3);
        
        SET_IF_KEY_PUSHED(KEY_ALT, "alt");
        SET_IF_KEY_PUSHED(KEY_ALTGR, "alt");
        SET_IF_KEY_PUSHED(KEY_LCONTROL, "ctrl");
        SET_IF_KEY_PUSHED(KEY_RCONTROL, "ctrl");
        SET_IF_KEY_PUSHED(KEY_LSHIFT, "shift");
        SET_IF_KEY_PUSHED(KEY_RSHIFT, "shift");
        SET_IF_KEY_PUSHED(KEY_BACKSPACE, "backspace");
        SET_IF_KEY_PUSHED(KEY_ESC, "esc");
    } else
        lua_pushnil(LUA);
        
    lstacktop();
    RETURN(1);
}

void lua_register_funcs(lua_State *LUA) {
    int Counter = 0;
        
    onstack("lua_register_funcs");

    NEWL(il_register_object);
    NEWL(il_obj_newidxfunc);
    NEWL(il_log);
    NEWL(il_flush);
    NEWL(il_nextinst);
    NEWL(il_l_tag);
    NEWL(il_trt_iassign);
    NEWL(il_trt_purge);
    NEWL(il_trt_obj_writeback);
    NEWL(il_activate_gui_zone);
    NEWL(il_set_internal_gui);
    NEWL(il_set_internal_shadeinfo);
    NEWL(expl_valid_inst);
    NEWL(expl_game_end);
    NEWL(expl_include_file);
    NEWL(expl_get_bitmap);
    NEWL(expl_clone_bitmap);
    NEWL(expl_get_mask_bitmap);
    NEWL(expl_get_sound);
    NEWL(expl_music);
    NEWL(expl_music_to_background);
    NEWL(expl_get_font);
    NEWL(expl_destroy_bitmap);
    NEWL(expl_new_bitmap);
    NEWL(expl_desktop_bitmap);
    NEWL(expl_bitmap_blit);
    NEWL(expl_bitmap_width);
    NEWL(expl_bitmap_height);
    NEWL(expl_bitmap_textout);
    NEWL(expl_text_size);
    NEWL(expl_bitmap_draw);
    NEWL(expl_bitmap_rect);
    NEWL(expl_get_pixel);
    NEWL(expl_put_pixel);
    NEWL(expl_rgb_to_hsv);
    NEWL(expl_hsv_to_rgb);
    NEWL(expl_cache_invalidate);
    NEWL(expl_set_viewport);
    NEWL(expl_set_viewport_distort);
    NEWL(expl_make_wallset);
    NEWL(expl_make_wallset_ext);
    NEWL(expl_image2map);
    NEWL(expl_text2map);
    NEWL(expl_party_place);
    NEWL(expl_use_wallset);
    NEWL(expl_level_flags);
    NEWL(expl_level_tint);
    NEWL(expl_level_getinfo);
    NEWL(expl_spawn);
    NEWL(expl_move);
    NEWL(expl_move_moncol);
    NEWL(expl_reposition);
    NEWL(expl_tileptr_exch);
    NEWL(expl_tileptr_rotate);
    NEWL(expl_collide);
    NEWL(expl_delete);
    NEWL(expl_enable);
    NEWL(expl_disable);
    NEWL(expl_toggle);
    NEWL(expl_get_cell);
    NEWL(expl_set_cell);
    NEWL(expl_visited);
    NEWL(expl_forward);
    NEWL(expl_get_condition);
    NEWL(expl_condition);
    NEWL(expl_add_condition);
    NEWL(expl_replace_condition);
    NEWL(expl_add_champion);
    NEWL(expl_put_ppos_to_relpos);
    NEWL(expl_fetch);
    NEWL(expl_qswap);
    NEWL(expl_swap);
    NEWL(expl_offer_champion);
    NEWL(expl_get_gfxflag);
    NEWL(expl_set_gfxflag);
    NEWL(expl_clear_gfxflag);
    NEWL(expl_toggle_gfxflag);
    NEWL(expl_get_mflag);
    NEWL(expl_set_mflag);
    NEWL(expl_clear_mflag);
    NEWL(expl_toggle_mflag);
    NEWL(expl_get_charge);
    NEWL(expl_set_charge);
    NEWL(expl_get_animtimer);
    NEWL(expl_set_animtimer);
    NEWL(expl_get_crop);
    NEWL(expl_set_crop);
    NEWL(expl_get_crmotion);
    NEWL(expl_set_crmotion);
    NEWL(expl_get_cropmax);
    NEWL(expl_sub_targ);
    NEWL(expl_dungeon_view);
    NEWL(expl_bitmap_clear);
    NEWL(expl_objzone);
    NEWL(expl_msgzone);
    NEWL(expl_find_arch);
    NEWL(expl_party_coords);
    NEWL(expl_party_at);
    NEWL(expl_party_contains);
    NEWL(expl_party_viewing);
    NEWL(expl_party_affecting);
    NEWL(expl_party_apush);
    NEWL(expl_party_apop);
    NEWL(expl_hide_mouse);
    NEWL(expl_show_mouse);
    NEWL(expl_mouse_state);
    NEWL(expl_mouse_override);
    NEWL(expl_push_mouse);
    NEWL(expl_pop_mouse);
    NEWL(expl_shoot);
    NEWL(expl_animate);
    NEWL(expl_get_coords);
    NEWL(expl_get_coords_prev);
    NEWL(expl_sound);
    NEWL(expl_3dsound);
    NEWL(expl_stopsound);
    NEWL(expl_checksound);
    NEWL(expl_get_soundvol);
    NEWL(expl_set_soundvol);
    NEWL(expl_get_flystate);
    NEWL(expl_set_flystate);
    NEWL(expl_get_sleepstate);
    NEWL(expl_wakeup);
    NEWL(expl_set_openshot);
    NEWL(expl_get_flydelta);
    NEWL(expl_set_flydelta);
    NEWL(expl_get_flyreps);
    NEWL(expl_set_flyreps);
    NEWL(expl_get_facedir);
    NEWL(expl_set_facedir);
    NEWL(expl_get_hp);
    NEWL(expl_set_hp);
    NEWL(expl_get_maxhp);
    NEWL(expl_set_maxhp);
    NEWL(expl_set_tint);
    NEWL(expl_get_pfacing);
    NEWL(expl_set_pfacing);
    NEWL(expl_get_champname);
    NEWL(expl_get_champtitle);
    NEWL(expl_set_champname);
    NEWL(expl_set_champtitle);
    NEWL(expl_get_load);
    NEWL(expl_get_maxload);
    NEWL(expl_get_food);
    NEWL(expl_set_food);
    NEWL(expl_get_water);
    NEWL(expl_set_water);
    NEWL(expl_get_idle);
    NEWL(expl_set_idle);
    NEWL(expl_get_stat);
    NEWL(expl_set_stat);
    NEWL(expl_get_maxstat);
    NEWL(expl_set_maxstat);
    NEWL(expl_get_injury);
    NEWL(expl_set_injury);
    NEWL(expl_update_inventory_info);
    NEWL(expl_update_system);
    NEWL(expl_cur_inv);
    NEWL(expl_delay_func);
    NEWL(expl_write);
    NEWL(expl_get_bar);
    NEWL(expl_set_bar);
    NEWL(expl_get_maxbar);
    NEWL(expl_set_maxbar);
    NEWL(expl_damage_popup);
    NEWL(expl_ppos_char);
    NEWL(expl_char_ppos);
    NEWL(expl_ppos_tile);
    NEWL(expl_tile_ppos);
    NEWL(expl_get_xp_multiplier);
    NEWL(expl_set_xp_multiplier);
    NEWL(expl_give_xp); 
    NEWL(expl_get_xp);
    NEWL(expl_set_xp);
    NEWL(expl_get_temp_xp);
    NEWL(expl_set_temp_xp);
    NEWL(expl_xp_level);
    NEWL(expl_xp_level_nb);
    NEWL(expl_get_bonus);
    NEWL(expl_set_bonus);
    NEWL(expl_msg);
    NEWL(expl_msg_chain);
    NEWL(expl_delay_msgs_to);
    NEWL(expl_lock_game);
    NEWL(expl_unlock_game);
    NEWL(expl_get_gamelocks);
    NEWL(expl_tileshift); 
    NEWL(expl_rand);
    NEWL(expl_rand_seed);
    NEWL(expl_attack_text);
    NEWL(expl_attack_damage);
    NEWL(expl_export);
    NEWL(expl_replace_method);
    NEWL(expl_linesplit);
    NEWL(expl_get_gameflag);
    NEWL(expl_set_gameflag);
    NEWL(expl_clear_gameflag);
    NEWL(expl_toggle_gameflag);
    NEWL(expl_ai);
    NEWL(expl_ai_boss);
    NEWL(expl_ai_subordinates);
    NEWL(expl_ai_promote);
    NEWL(expl_get_light);
    NEWL(expl_get_light_total);
    NEWL(expl_set_light);
    NEWL(expl_set_light_totalmax);
    NEWL(expl_get_portraitname);
    NEWL(expl_set_portraitname);
    NEWL(expl_replace_charhand);
    NEWL(expl_replace_inventory);
    NEWL(expl_replace_topimages);
    NEWL(expl_get_exviewinst);
    NEWL(expl_set_exviewinst);
    NEWL(expl_get_leader);
    NEWL(expl_get_caster);
    NEWL(expl_lookup_global);
    NEWL(expl_textformat);
    NEWL(expl_champion_toparty);
    NEWL(expl_champion_fromparty);
    NEWL(expl_party_scanfor);
    NEWL(expl_spawnburst_begin);
    NEWL(expl_spawnburst_end);
    NEWL(expl_fullscreen);
    NEWL(expl_get_alt_wallset);
    NEWL(expl_set_alt_wallset);
    NEWL(expl_wallset_override_floor);
    NEWL(expl_wallset_override_roof);
    NEWL(expl_wallset_flip_floor);
    NEWL(expl_wallset_flip_roof);
    NEWL(expl_wallset_always_draw_floor);
    NEWL(expl_get_lastmethod);
    NEWL(expl_set_lastmethod);
    NEWL(expl_import_arch);
    NEWL(expl_get_pendingspell);
    NEWL(expl_set_pendingspell);
    NEWL(expl_poll_keyboard);
    NEWL(expl_dmtextconvert);
    NEWL(expl_numbertextconvert);
    
    if (Counter != ALL_LUA_FUNCS) {
        int numstrings = 0;
        char errmsg[200];
        while (funcnames[numstrings][0] != '@')
            ++numstrings;
            
        sprintf(errmsg, "Bad function count!\n\n%d defines, %d strings, %d functions",
            ALL_LUA_FUNCS, numstrings, Counter);
        poop_out(errmsg);
    }
                                     
    VOIDRETURN();
}


