#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "lproto.h"
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "compile.h"
#include "arch.h"
#include "trans.h"
#include "editor.h"
#include "editor_hwndptr.h"
#include "editor_gui.h"
#include "editor_menu.h"

#define VL_B_LOAD(FS) \
    if (luaL_loadfile(V_Lua, bfixname(FS))) {\
        char err[500];\
        snprintf(err, sizeof(err), "Error in base code\n%s", lua_tostring(V_Lua, -1));\
        MessageBox(sys_hwnd, err, "Error", MB_ICONSTOP);\
        lua_pop(V_Lua, 1);\
        lua_state_restore(V_Lua);\
        VOIDRETURN();\
    }\
    if (lua_pcall(V_Lua, 0, 0, 0)) {\
        char err[500];\
        snprintf(err, sizeof(err), "Error in base code\n%s", lua_tostring(V_Lua, -1));\
        MessageBox(sys_hwnd, err, "Error", MB_ICONSTOP);\
        lua_pop(V_Lua, 1);\
        lua_state_restore(V_Lua);\
        VOIDRETURN();\
    }
        
const char *NOT_RELOADED = "Object archetypes were not reloaded.";
const char *NOT_IMPORTED = "Imported object was not reloaded.";
const char *VIMPORT = "dsb_import_arch";

extern struct global_data gd;
extern struct editor_global edg;
extern lua_State *LUA;
extern struct inst *oinst[];
extern HWND sys_hwnd;
extern struct translate_table Translate;
extern int ZID;

char stored_directory[MAX_PATH];
int stored_ZID;

void ed_obj_dump_names_into_state(lua_State *VL) {
    int i;
    onstack("ed_obj_dump_names_into_state");
    
    lua_newtable(VL);
    for(i=1;i<NUM_INST;++i) {
        if (oinst[i]) {
            struct inst *p_inst = oinst[i];
            struct obj_arch *p_arch = Arch(p_inst->arch);
            lua_pushinteger(VL, i);
            lua_pushstring(VL, p_arch->luaname);
            lua_settable(VL, -3);
        }
    }
    lua_setglobal(VL, "_ESBTMPSTORETABLE");
    
    VOIDRETURN();    
}

void ed_report_lua_error(lua_State *VL, int derror, const char *fileptr, int z) {
    char sstr[1500];
    const char *lerror = lua_tostring(VL, -1);
    const char *reason_type = NOT_RELOADED;
    if (z)
        reason_type = NOT_IMPORTED;
    snprintf(sstr, sizeof(sstr), "There was an error parsing %s\n\n%s\n\n%s",
        fileptr, lerror, reason_type);
    MessageBox(sys_hwnd, sstr, "Lua Error", MB_ICONEXCLAMATION);
}

int explv_import_arch(lua_State *VL) {
    const char *root_name = "ROOT_NAME";
    const char *filename;
    const char *objname;
    int push_exestate;
    int res, pc;
    
    luaonstack(VIMPORT);
    if (lua_gettop(VL) != 2) {
        DSBLerror(VL, "%s requires 2 parameters", VIMPORT);
        RETURN(0);
    }
    filename = luastring(VL, -2, VIMPORT, 1);
    objname = luastring(VL, -1, VIMPORT, 2);
        
    lua_pushstring(VL, objname);
    lua_setglobal(VL, root_name);
    
    stackbackc('(');
    v_onstack(filename);
    addstackc(')');

    res = luaL_loadfile(VL, fixname(filename));
    if (res) {
        char zfstr[1200];
        const char *parse_error = lua_tostring(VL, -1);
        snprintf(zfstr, sizeof(zfstr), "Failure to import %s\n\n%s\n\n%s",
            filename, parse_error, NOT_IMPORTED);
        MessageBox(sys_hwnd, zfstr, "Error", MB_ICONEXCLAMATION);
        RETURN(0);        
    }
    pc = lua_pcall(VL, 0, 0, 0);
    if (pc) {
        char impdec[1000];
        snprintf(impdec, sizeof(impdec), "imported arch %s", objname);
        ed_report_lua_error(VL, pc, impdec, 1);
        RETURN(0);
    }
    
    lua_pushnil(VL);
    lua_setglobal(VL, root_name);
    
    RETURN(0);
}

void ed_register_virtual_arch_import(lua_State *VL) {
    onstack("ed_register_virtual_arch_import");
    lua_pushcfunction(VL, explv_import_arch);
    lua_setglobal(VL, VIMPORT);
    VOIDRETURN();
}

int ed_arch_verify_reassign(lua_State *VL, int assign) {
    int i; 
    
    onstack("ed_arch_verify_reassign");
    
    lua_getglobal(VL, "_ESBTMPSTORETABLE");
    for(i=1;i<NUM_INST;++i) {
        if (!oinst[i]) continue;
        lua_pushinteger(VL, i);
        lua_gettable(VL, -2);
        if (lua_isstring(VL, -1)) {
            lua_getglobal(VL, "obj");
            lua_pushvalue(VL, -2); 
            lua_gettable(VL, -2);
            if (!lua_istable(VL, -1)) {
                char spt[1200];
                const char *reqstr;
                lua_pop(VL, 2);
                reqstr = lua_tostring(VL, -1);
                snprintf(spt, sizeof(spt), "The arch %s is used in the dungeon, but invalid in the new file.\n\n%s",
                    reqstr, NOT_RELOADED);
                if (assign) {
                    snprintf(spt, sizeof(spt), "The arch %s is used in the dungeon, but invalid in the new file.\n\n%s",
                        reqstr, "Arch table has been corrupted. This almost certainly a bug in ESB.\n");
                    poop_out(spt);
                } else
                    MessageBox(sys_hwnd, spt, "Arch Error", MB_ICONEXCLAMATION);
                lua_pop(VL, 2);
                RETURN(1);
            }
            if (assign) {
                unsigned int ud;
                lua_pushstring(VL, "regnum");
                lua_gettable(VL, -2);
                if (!lua_isuserdata(VL, -1))
                    program_puke("Regnum corruption");
                ud = (unsigned int)lua_touserdata(VL, -1);
                oinst[i]->arch = ud;
                lua_pop(VL, 1);
            }   
            lua_pop(VL, 2);      
        }
        lua_pop(VL, 1);    
    }
    lua_pop(VL, 1);
    
    RETURN(0);
}

void lua_state_restore(lua_State *VL) {
    onstack("lua_state_restore");
    ZID = stored_ZID;
    lua_close(VL);
    SetCurrentDirectory(stored_directory);
    VOIDRETURN();
}

void ed_reload_all_arch(void) {
    char fstr[120];
    int res;
    int pc;
    lua_State *V_Lua;
    int stored_ZID;
    FILE *ftester;
    int push_exestate;
    onstack("ed_reload_all_arch");
    
    GetCurrentDirectory(MAX_PATH, stored_directory);
    SetCurrentDirectory(edg.curdir);
    stored_ZID = ZID;
    V_Lua = lua_open(); 
    if (!V_Lua || lua_checkstack(V_Lua, 8) == FALSE) {
        sprintf(fstr, "Secondary Lua stack failure [%x]", V_Lua);
        MessageBox(sys_hwnd, fstr, "Error", MB_ICONSTOP);
        if (V_Lua) lua_close(V_Lua);
        VOIDRETURN();
    }
    lua_checkstack(V_Lua, 12);
    luaopen_base(V_Lua);
    luaopen_string(V_Lua);
    luaopen_table(V_Lua);
    luaopen_math(V_Lua);
    lua_register_funcs(V_Lua);
    ed_register_virtual_arch_import(V_Lua);
    VL_B_LOAD("lua.dat");
    VL_B_LOAD("util.lua");

    // hack to get around another hack!
    // (old code relies on the existence of esb_typecheck to know
    // that it is in ESB and not try to call some DSB-only code)
    lua_pushboolean(V_Lua, 1);
    lua_setglobal(V_Lua, "esb_typecheck");

    ZID = 1;
    lua_getglobal(V_Lua, "__obj_mt");
    lua_pcall(V_Lua, 0, 0, 0);
    lua_getglobal(V_Lua, "__editor_metatables");
    lua_pcall(V_Lua, 0, 0, 0);
    
    VL_B_LOAD("global.lua");
    VL_B_LOAD("inventory_info.lua");
    
    ed_obj_dump_names_into_state(V_Lua);
    
    res = luaL_loadfile(V_Lua, bfixname("objects.lua"));
    if (res) {
        sprintf(fstr, "Failure to load base objects.lua [%x]", V_Lua);
        MessageBox(sys_hwnd, fstr, "Error", MB_ICONSTOP);
        lua_state_restore(V_Lua);
        VOIDRETURN();
    }
    pc = lua_pcall(V_Lua, 0, 0, 0);
    if (pc) {
        ed_report_lua_error(V_Lua, pc, "base/objects.lua", 0);
        ZID = stored_ZID;
        lua_close(V_Lua);
        VOIDRETURN();
    }
    
    ftester = fopen(fixname("objects.lua"), "r");
    if (ftester) {
        fclose(ftester);   
        res = luaL_loadfile(V_Lua, fixname("objects.lua"));
        if (res) {
            char zfstr[1200];
            const char *parse_error = lua_tostring(V_Lua, -1);
            snprintf(zfstr, sizeof(zfstr), "Failure to load objects.lua\n\n%s\n\n%s",
                parse_error, NOT_RELOADED);
            MessageBox(sys_hwnd, zfstr, "Error", MB_ICONEXCLAMATION);
            lua_pop(V_Lua, 1);
            lua_state_restore(V_Lua);
            VOIDRETURN();        
        }
        pc = lua_pcall(V_Lua, 0, 0, 0);
        if (pc) {
            ed_report_lua_error(V_Lua, pc, "objects.lua", 0);
            lua_state_restore(V_Lua);
            VOIDRETURN();
        } 
    }

    if (ed_arch_verify_reassign(V_Lua, 0)) {
        lua_state_restore(V_Lua);
        VOIDRETURN();
    }     
    lua_close(V_Lua);
    
    //now we do it for real
    ed_obj_dump_names_into_state(LUA);
    push_exestate = gd.exestate;
    gd.exestate = STATE_OBJPARSE;
    purge_all_arch();
    purge_translation_table(&Translate);
    ed_purge_treeview(edg.treeview);
    ZID = 1;
    lc_parm_int("__obj_mt", 0);
    
    /* Originally it loaded objects BEFORE parsing startup.lua. I think this is safe */
    src_lua_file(fixname("startup.lua"), 1);
    load_system_objects(LOAD_OBJ_ALL);  
    
    ed_arch_verify_reassign(LUA, 1);
    ed_build_tree(edg.treeview, 0, NULL);
    
    SetD(DRM_DRAWDUN, NULL, NULL);

    lua_pushnil(LUA);
    lua_setglobal(LUA, "_ESBTMPSTORETABLE");  
    gd.exestate = push_exestate;
    deselect_edit_clipboard();
    destroy_edit_clipboard();  
     
    SetCurrentDirectory(stored_directory);
    MessageBox(sys_hwnd, "Object archetypes reloaded from Lua sources.", "Success", MB_ICONINFORMATION);
    VOIDRETURN();
}
