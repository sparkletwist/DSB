#include <stdio.h>
#include <stdarg.h>
#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <fmod.h>
#include "uproto.h"
#include "objects.h"
#include "lua_objects.h"
#include "global_data.h"
#include "arch.h"
#include "sound.h"
#include "sound_fmod.h"
#include "editor_shared.h"

lua_State *LUA;

extern FILE *errorlog;
extern struct global_data gd;
extern struct inst *oinst[NUM_INST];

extern int debug;
extern int ZID;

const char *Lua_FontHandle = "sys_font";

void init_lua_font_handle(void) {
    // add a Lua handle to the system font
    gd.queued_pointer = (void*)font;
    setup_lua_font(LUA, font, 0);
    lua_setglobal(LUA, Lua_FontHandle);
}

void initialize_lua(void) {
    int cb;
    
    initlua(bfixname("lua.dat"));
    init_lua_soundtable();
    init_lua_font_handle();
    gd.lua_init = 1;
    fprintf(errorlog, "LUA: Lua initialized\n");
    
    luastacksize(4);
    gd.file_lod = 0;
    
    psxf("GETTING PATH CONFIGURATION");
    
    src_lua_file(fixname("lod.cfg"), 1);
    src_lua_file(fixname("graphics.cfg"), 1);
    src_lua_file(fixname("sound.cfg"), 1);
    
    lua_getglobal(LUA, "LOAD_ON_DEMAND");
    if (lua_isboolean(LUA, -1) && lua_toboolean(LUA, -1)) {
        if (gd.compile) {
            int mbr = MessageBox(win_get_window(), "Compile = 1 is set but load on demand is enabled.\nDisable LOD and proceed with compilation?",
                "Compliation Notice", MB_ICONINFORMATION | MB_YESNO);
            if (mbr == 6) {
                gd.compile = 1;
                gd.file_lod = 0;
            } else {
                gd.compile = 0;
                gd.file_lod = 1;
            }
        } else
            gd.file_lod = 1;
        
        if (gd.file_lod) {
            fprintf(errorlog, "LOD: Attempting to load resources on demand\n");
        }
    }
    lua_pop(LUA, 1);
    
    lua_getglobal(LUA, "graphics_paths");
    if (lua_istable(LUA, -1)) {
        gd.gfxpathtable = 1;
    }
    lua_pop(LUA, 1);
    
    lua_getglobal(LUA, "sound_paths");
    if (lua_istable(LUA, -1)) {
        gd.sndpathtable = 1;
    }
    lua_pop(LUA, 1);
    
    psxf("LOADING SYSTEM PRESTARTUP");
    
    lua_pushstring(LUA, "dungeon.lua");
    lua_setglobal(LUA, "DUNGEON_FILENAME");
    lua_pushstring(LUA, "dungeon.dsb");
    lua_setglobal(LUA, "COMPILED_DUNGEON_FILENAME");
    
    cb = src_lua_file(bfixname("prestartup.lua"), 1);
    manifest_files(1, cb);
    if (!gd.uprefix) {
        psxf("LOADING CUSTOM PRESTARTUP");
        cb = src_lua_file(fixname("prestartup.lua"), 1);
        manifest_files(0, cb);
    } 
    
    psxf("LOADING SYSTEM SCRIPTS");   
    
    cb = src_lua_file(bfixname("global.lua"), 0);
    manifest_files(1, cb);
    cb = src_lua_file(bfixname("startup.lua"), 0);
    manifest_files(1, cb);

    if (!gd.uprefix) {
        int compiled_base;
        psxf("LOADING CUSTOM SCRIPTS");
        cb = src_lua_file(fixname("startup.lua"), 1);
        manifest_files(0, cb);
    }
}

void reget_lua_font_handle(void) {
    onstack("reget_lua_font_handle");

    lua_getglobal(LUA, Lua_FontHandle);
    font = (FONT*)luafont(LUA, -1, "font_handle", 0);
    lua_pop(LUA, 1);

    VOIDRETURN();
}
