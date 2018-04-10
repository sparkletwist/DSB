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

#define IMPORT_BMP_PARM(S, V) \
    lua_pushstring(LUA, S);\
    lua_gettable(LUA, stackarg-1);\
    if (lua_isnumber(LUA, -1))\
        lbmp->V = lua_tointeger(LUA, -1);\
    lua_pop(LUA, 1)
    
#define IMPORT_BMP_VAR(S, V) \
    lua_pushstring(LUA, S);\
    lua_gettable(LUA, stackarg-1);\
    if (lua_isnumber(LUA, -1))\
        V = lua_tointeger(LUA, -1);\
    lua_pop(LUA, 1)

lua_State *LUA;

extern FILE *errorlog;
extern struct global_data gd;
extern struct inst *oinst[NUM_INST];

extern int debug;
extern int ZID;

const char *DSBMTGC = "DSBMT.gc_event";

int il_lua_gc(lua_State *LUA) {
    void *lgc;
    void *ptr;
    int type;
    
    onstack("lua_mt.lua_gc");

    // uses ud_ref
    lgc = lua_touserdata(LUA, -1);
    
    memcpy(&ptr, lgc, 4);
    memcpy(&type, (lgc+4), 4);
    
    if (type == DSB_LUA_BITMAP) {
        struct animap *amap = (struct animap*)ptr;
        if (amap->ext_references == 0) {
            purge_from_lod(0, amap);
            destroy_animap(amap);
        }
    } else if (type == DSB_LUA_SOUND) {
        FMOD_Sound_Release((FMOD_SOUND*)ptr);
    } else if (type == DSB_LUA_I_FONT) {
        unload_datafile_object((DATAFILE*)ptr);
    } else if (type == DSB_LUA_FONT) {
        destroy_font((FONT*)ptr);
    }

    /*
    if (debug)
        fprintf(errorlog, "LUA.GC: %d (%d)\n", ptr, type);
    */
    
    RETURN(0);
}

void setup_gc_meta(lua_State *LUA) {
    onstack("lua_mt.setup_gc_meta");
    
    luastacksize(6);
    
    luaL_newmetatable(LUA, DSBMTGC);
    
    lua_pushliteral(LUA, "__index");
    lua_pushvalue(LUA, -2);
    lua_rawset(LUA, -3);

    lua_pushliteral(LUA, "__gc");
    lua_pushcfunction(LUA, il_lua_gc);
    lua_rawset(LUA, -3);
    
    lua_pop(LUA, 1);
    
    VOIDRETURN();
}

void collect_lua_garbage(void) {
    onstack("collect_lua_garbage");
    lua_gc(LUA, LUA_GCCOLLECT, 0);
    VOIDRETURN();
}

void collect_lua_incremental_garbage(int steps) {
    onstack("collect_lua_incremental_garbage");    
    lua_gc(LUA, LUA_GCSTEP, steps);
    VOIDRETURN();
}

unsigned char luacharglobal(const char *gname) {
    unsigned char rval = 0;
    
    lua_getglobal(LUA, gname);
    if (!lua_isnumber(LUA, -1)) {
        unsigned char errstr[64];
        snprintf(errstr, sizeof(errstr), "Required variable %s undefined", gname);
        poop_out(errstr);
    } else {
        int nlev = lua_tointeger(LUA, -1);
        if (nlev < 0 || nlev > 254) {
            unsigned char errstr[64];
            snprintf(errstr, sizeof(errstr), "Bad value of %s", gname);
            poop_out(errstr);
        }
        rval = nlev;
    }
    lua_pop(LUA, 1);
    
    return rval;
}

void lstacktop(void) {
    char *cs_errstr = "Lua stack underflow";
    
    if (lua_gettop(LUA) < 0) {
        recover_error(cs_errstr);
    }
}

void objtype_failure(const char *fname, const char *xstr,
    int paramnum, const char *rtype)
{
    onstack("objtype_failure");
    
    if (xstr) {
        DSBLerror(LUA, "%s%s%s requires %s", fname, ".", xstr, rtype);
    } else
        DSBLerror(LUA, "%s requires %s in param %d", fname, rtype, paramnum);
        
    VOIDRETURN();
}

void bitmap_failure(const char *fname, const char *xstr, int paramnum) {
    objtype_failure(fname, xstr, paramnum, "Bitmap");
}    

void import_lua_bmp_vars(lua_State *LUA, struct animap *lbmp, int stackarg) {
    int xcomp, ycomp;

    luastacksize(4);
    
    IMPORT_BMP_PARM("x_off", xoff);
    IMPORT_BMP_PARM("y_off", yoff);
    IMPORT_BMP_PARM("mid_x_compress", mid_x_compress);
    IMPORT_BMP_PARM("far_x_compress", far_x_compress);
    if (!lbmp->mid_x_compress && !lbmp->far_x_compress) {
        int xcomp = 0;
        IMPORT_BMP_VAR("x_compress", xcomp);
        lbmp->mid_x_compress = xcomp;
        lbmp->far_x_compress = (4*xcomp)/8;
    }
    
    IMPORT_BMP_PARM("mid_y_compress", mid_y_compress);
    IMPORT_BMP_PARM("far_y_compress", far_y_compress);
    if (!lbmp->mid_y_compress && !lbmp->far_y_compress) {
        int ycomp = 0;
        IMPORT_BMP_VAR("y_compress", ycomp);
        lbmp->mid_y_compress = ycomp;
        lbmp->far_y_compress = (4*ycomp)/8;
    }
    
    IMPORT_BMP_PARM("mid_x_tweak", mid_x_tweak);
    IMPORT_BMP_PARM("mid_y_tweak", mid_y_tweak);
    IMPORT_BMP_PARM("far_x_tweak", far_x_tweak);
    IMPORT_BMP_PARM("far_y_tweak", far_y_tweak);
        
    lstacktop();   
}

static void lua_stack_dump(lua_State *L) {
    int i;
    int top = lua_gettop(L);
    
    fprintf(errorlog, "@@@ LUA STACK @@@\n");
    
    for (i = 1; i <= top; i++) {  /* repeat for each level */
        int t = lua_type(L, i);
        switch (t) {
    
            case LUA_TSTRING:  /* strings */
                fprintf(errorlog, "[S:%s]", lua_tostring(L, i));
            break;
            
            case LUA_TBOOLEAN:  /* booleans */
                fprintf(errorlog, "[B:%s]", lua_toboolean(L, i) ? "true" : "false");
            break;
            
            case LUA_TNUMBER:  /* numbers */
                fprintf(errorlog, "[N:%g]", lua_tonumber(L, i));
            break;
            
            default: { /* other values */
                luastacksize(6);
                if (gd.lua_init) {
                    lua_getglobal(L, "__ppcheckserialize");
                    lua_pushvalue(L, i);
                    if (lua_pcall(L, 1, 1, 0))
                        program_puke("Error in Lua error handler");
                    fprintf(errorlog,"[%s]", lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            } break;
    
        }
    }
    fprintf(errorlog, "\n@@@@@@\n");  /* end the listing */
}

void global_stack_dump_request(void) {
    onstack("global_stack_dump_request");
    lua_stack_dump(LUA);
    VOIDRETURN();
}

void bomb_lua_error(const char *lerror) {
    char *myerror;
    
    onstack("bomb_lua_error");
    
    if (lerror == NULL)
        myerror = (char *)lua_tostring(LUA, -1);
    else
        myerror = (char *)lerror;
        
    fprintf(errorlog, "FATAL LUA ERROR: %s\n", myerror);
    lua_stack_dump(LUA);
    fclose(errorlog);
    
    if (gd.lua_error_found) {
        char tmp[800];
        
        v_onstack("snprintf");
        
        snprintf(tmp, sizeof(tmp), "%s\n\n%s\n%s\n%s",
            myerror, "Additional non-fatal Lua error(s) also occurred.",
            "They may or may not be connected with this fatal error.",
            "Please see log.txt for details.");
            
        v_upstack();
        v_onstack("dup");
        myerror = dsbstrdup(tmp);
        v_upstack();
    }
    
    set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
    MessageBox(win_get_window(), myerror, "FATAL LUA ERROR", MB_ICONSTOP);
	DSBallegshutdown();
    exit(1);
    
    VOIDRETURN();
}

void lua_function_error(const char *fname, const char *msg) {
    char errmsg[256];
    snprintf(errmsg, sizeof(errmsg), "Lua Function %s: %s", fname, msg);
    bomb_lua_error(errmsg);
} 

void lua_return_function_error(const char *fname, const char *msg) {
    char errmsg[256];
    snprintf(errmsg, sizeof(errmsg), "Lua Function %s: Function must return %s", fname, msg);
    bomb_lua_error(errmsg);
}       

void initlua(char *filename) {
    
    onstack("initlua");
    
    LUA = lua_open();
    
    if (debug)
        fprintf(errorlog, "LUA: Opening new Lua state %x\n", LUA);
    
    luastacksize(6);
    luaopen_base(LUA);
    luaopen_string(LUA);
    luaopen_table(LUA);
    luaopen_math(LUA);  
    lua_pop(LUA, 5);
    
    lua_register_funcs(LUA);
    
    setup_gc_meta(LUA);
    
    if (luaL_loadfile(LUA, filename) || lua_pcall(LUA, 0, 0, 0))
        bomb_lua_error(NULL);
        
    VOIDRETURN();
}

int src_lua_file(char *s_fx, int opt) {
    int file_fail = 0;
    char *s_fx2 = NULL;
    char *fail_msg = NULL;
    FILE *lfile;
    int r_compiled = 0;

    onstack("src_lua_file");
    stackbackc('(');
    v_onstack(s_fx);
    addstackc(')');
    
    luastacksize(8);
    
    lfile = fopen(s_fx, "r");
    if (lfile == NULL)
        file_fail = 1;
    else {
        fclose(lfile);
        v_onstack("src_lua_file.sourcecode");
        if (luaL_loadfile(LUA, s_fx)) {
            bomb_lua_error(NULL);
            v_upstack();
            v_upstack();
            VOIDRETURN();
        }
        v_upstack();
    }

    if (file_fail) {
        char buf[100];
        int i_zl;

        snprintf(buf, 100, "File not found: %s", s_fx);
        fail_msg = dsbstrdup(buf);
        
        file_fail = 0;
        s_fx2 = dsbstrdup(s_fx);
        s_fx = s_fx2;

        i_zl = strlen(s_fx2);
        if (i_zl > 3) {
            s_fx2[i_zl-2] = 'x';
            s_fx2[i_zl-1] = 'x';
        }
        
        lfile = fopen(s_fx2, "r");
        if (lfile == NULL)
            file_fail = 1;
        else {
            fclose(lfile);
            v_onstack("src_lua_file.compiled");
            if (luaL_loadfile(LUA, s_fx2)) {
                opt = 0;
                file_fail = 1;
            }
            r_compiled = 1;
            v_upstack();
        }
    }

    if (file_fail) {
        if (opt) {
            goto END_SRC_LUA;
        } else {
            bomb_lua_error(fail_msg);
        }
    }


    if (!gd.edit_mode) {
        fprintf(errorlog, "Parsing %s\n", s_fx);
        fflush(errorlog);
    }
    
    v_onstack("src_lua_file.pcall");
    if (lua_pcall(LUA, 0, 0, 0))
        bomb_lua_error(NULL); 
    v_upstack();
        
    END_SRC_LUA:
    lstacktop();
    dsbfree(s_fx2);
    dsbfree(fail_msg);
    RETURN(r_compiled);      
}

const char *luastring(lua_State *LUA, int stackarg, const char *fname, int paramnum) {
    const char *rs;
    
    if (!lua_isstring(LUA, stackarg)) {
        DSBLerror(LUA, "%s requires string in param %d", fname, paramnum);
        return(NULL);
    }
    
    rs = lua_tostring(LUA, stackarg);
        
    return(rs);       
}

void luastacksize(int lsize) {
    
    int schk = lua_checkstack(LUA, lsize);
    if (schk == FALSE)
        program_puke("Lua_checkstack failed");
}

int luaint(lua_State *LUA, int stackarg, const char *fname, int paramnum) {
    int rv;
    
    if (!lua_isnumber(LUA, stackarg)) {
        DSBLerror(LUA, "%s requires int in param %d", fname, paramnum);
        return(0);
    }
    
    rv = lua_tointeger(LUA, stackarg);
        
    return rv;       
}

// this function does not remove the bitmap from the stack
// make sure that the caller either does it or it is not
// required to do so!
struct animap *luabitmap(lua_State *LUA, int stackarg,
    const char *fname, int paramnum)
{
    int tid;
    struct animap *lbmp;
        
    //fprintf(errorlog, "%s: checking\n", fname);
    
    if (!lua_istable(LUA, stackarg)) goto LUA_BMP_ERROR;
    
    //fprintf(errorlog, "%s: is a table\n", fname);
    
    lua_pushstring(LUA, "type");
    lua_gettable(LUA, stackarg-1);
    if (!lua_isnumber(LUA, -1)) {
        lua_pop(LUA, 1);
        return NULL;
    }
    
    //fprintf(errorlog, "%s: has right type\n", fname);
    
    tid = lua_tointeger(LUA, -1);
    lua_pop(LUA, 1);

    if (tid != DSB_LUA_VIRTUAL_BITMAP) {
        if (tid != DSB_LUA_BITMAP) goto LUA_BMP_ERROR;
    }
    
    lua_pushstring(LUA, "ref");
    lua_gettable(LUA, stackarg-1);
    
    lbmp = (struct animap*)lua_touserdata(LUA, -1);
    lua_pop(LUA, 1);
    
    import_lua_bmp_vars(LUA, lbmp, stackarg);
    
    return(lbmp);
    
    LUA_BMP_ERROR:
    bitmap_failure(fname, gd.tmpstr, paramnum);
    gd.tmpstr = NULL;
    return(NULL);
}

FMOD_SOUND *luasound(lua_State *LUA, int stackarg,
    char *id, char *fname, int paramnum)
{
    int tid;
    FMOD_SOUND *lsnd;
    const char *s_id;
    
    onstack("luasound"); 
    if (!lua_istable(LUA, stackarg)) goto LUA_SND_ERROR;

    lua_pushstring(LUA, "type");
    lua_gettable(LUA, stackarg-1);
    if (!lua_isnumber(LUA, -1)) goto LUA_SND_ERROR;    
    tid = lua_tointeger(LUA, -1);
    lua_pop(LUA, 1);
    
    if (tid != DSB_LUA_SOUND) goto LUA_SND_ERROR;    

    lua_pushstring(LUA, "ref");
    lua_gettable(LUA, stackarg-1);    
    lsnd = (FMOD_SOUND *)lua_touserdata(LUA, -1);
    lua_pop(LUA, 1);

    lua_pushstring(LUA, "id");
    lua_gettable(LUA, stackarg-1);
    s_id = lua_tostring(LUA, -1);
    strncpy(id, s_id, 16);
    lua_pop(LUA, 1);
      
    RETURN(lsnd);
    
    LUA_SND_ERROR:
    objtype_failure(fname, gd.tmpstr, paramnum, "Sound");
    gd.tmpstr = NULL;
    RETURN(NULL);
}

FONT *luafont(lua_State *LUA, int stackarg, char *fname, int paramnum) {
    int tid;
    FONT *lfnt;
    
    onstack("luafont"); 
    if (!lua_istable(LUA, stackarg)) goto LUA_FNT_ERROR;
    lua_pushstring(LUA, "type");
    lua_gettable(LUA, stackarg-1);
    if (!lua_isnumber(LUA, -1)) goto LUA_FNT_ERROR;  
    tid = lua_tointeger(LUA, -1);
    lua_pop(LUA, 1);
    if (tid != DSB_LUA_FONT && tid != DSB_LUA_I_FONT) goto LUA_FNT_ERROR;
    lua_pushstring(LUA, "ref");
    lua_gettable(LUA, stackarg-1);    
    lfnt = (FONT *)lua_touserdata(LUA, -1);
    lua_pop(LUA, 1);    
    RETURN(lfnt);
    
    LUA_FNT_ERROR:
    objtype_failure(fname, gd.tmpstr, paramnum, "Font");
    gd.tmpstr = NULL;
    RETURN(NULL);
}

int luawallset(lua_State *LUA, int stackarg, const char *fname) {
    int tid;
    char *xstr;
    int lws;
    
    onstack("luawallset");
    
    if (!lua_istable(LUA, stackarg)) goto LUA_WALLSET_ERROR;
    
    lua_pushstring(LUA, "type");
    lua_gettable(LUA, stackarg-1);
    if (!lua_isnumber(LUA, -1)) goto LUA_WALLSET_ERROR;
    
    tid = lua_tointeger(LUA, -1);
    lua_pop(LUA, 1);
    if (tid != DSB_LUA_WALLSET) goto LUA_WALLSET_ERROR;
    
    lua_pushstring(LUA, "ref");
    lua_gettable(LUA, stackarg-1);
    
    lws = (int)lua_touserdata(LUA, -1);
    lua_pop(LUA, 1);
    RETURN(lws);
    
    LUA_WALLSET_ERROR:
    DSBLerror(LUA, "%s requires Wallset", fname);
    RETURN(0);
}

/* This one is a mess too, but the usual invoking method is
    getluaoptbitmap() which takes care of it                */
struct animap *luaoptbitmap(lua_State *LUA, int stackarg, char *fname) {
    int tid;
    char *xstr;
    struct animap *lbmp;
    
    onstack("luaoptbitmap");
    
    //fprintf(errorlog, "*** Checking table\n");
    
    if (!lua_istable(LUA, stackarg)) {
        RETURN(NULL);
    }
    
    //fprintf(errorlog, "*** is a table\n");
    
    lua_pushstring(LUA, "type");
    lua_gettable(LUA, stackarg-1);
    if (!lua_isnumber(LUA, -1)) {
        lua_pop(LUA, 1);
        RETURN(NULL);
    }
    
    tid = lua_tointeger(LUA, -1);
    lua_pop(LUA, 1);
    
    if (tid != DSB_LUA_VIRTUAL_BITMAP) {
        if (tid != DSB_LUA_BITMAP) goto LUA_OPTBMP_ERROR;
    }
    
    lua_pushstring(LUA, "ref");
    lua_gettable(LUA, stackarg-1);
    
    //fprintf(errorlog, "*** pulling userdata\n");
    
    lbmp = (struct animap*)lua_touserdata(LUA, -1);
    lua_pop(LUA, 1);
    
    import_lua_bmp_vars(LUA, lbmp, stackarg);
    
    RETURN(lbmp);
    
    LUA_OPTBMP_ERROR:
        bitmap_failure(fname, gd.tmpstr, stackarg);
        gd.tmpstr = NULL;
        RETURN(NULL);
}

unsigned int luargbval(lua_State *LUA, int stackarg, const char *fname, int p) {
    unsigned char rgbv[3];
    int i;
 
    // special exception
    if (lua_isnil(LUA, stackarg)) {
        return 0;
    }   
    
    if (!lua_istable(LUA, stackarg))
        goto LUA_RGB_ERROR;
    
    for (i=0;i<3;i++) {
        lua_pushinteger(LUA, i+1);
        lua_gettable(LUA, (stackarg-1));
        
        if (!lua_isnumber(LUA, -1))
            goto LUA_RGB_ERROR;
            
        rgbv[i] = lua_tointeger(LUA, -1);
        lua_pop(LUA, 1);
    }
    
    return(makecol(rgbv[0], rgbv[1], rgbv[2]));
    
    LUA_RGB_ERROR:
    DSBLerror(LUA, "%s requires {R,G,B}", fname);
    return 0;      
}

unsigned int luahsvval(lua_State *LUA, int stackarg, const char *fname, int p,
    float *h, float *s, float *v) 
{
    unsigned char rgbv[3];
    int i;

    onstack("luahsvval");
    
    // special exception
    if (lua_isnil(LUA, stackarg)) {
        RETURN(0);
    }   
    
    if (!lua_istable(LUA, stackarg))
        goto LUA_RGB_ERROR;
    
    for (i=0;i<3;i++) {
        lua_pushinteger(LUA, i+1);
        lua_gettable(LUA, (stackarg-1));
        
        if (!lua_isnumber(LUA, -1))
            goto LUA_RGB_ERROR;
         
        if (i == 1)
            *s = lua_tonumber(LUA, -1);
        else if (i == 2)
            *v = lua_tonumber(LUA, -1);
        else 
            *h = lua_tonumber(LUA, -1);
            
        lua_pop(LUA, 1);
    }
    
    RETURN(1);
    
    LUA_RGB_ERROR:
    DSBLerror(LUA, "%s requires {H,S,V}", fname);
    RETURN(0);      
}

/* This function must be called with -1 stackarg, I forgot why
    but I'm not about to go changing it around now */
unsigned int luaobjarch(lua_State *LUA, int stackarg, const char *fname) {
    unsigned int objarch = 0;
    const char *nameref = "<unknown arch>";
    char *errstr = "unknown error";
    int end_pop = 3;

    onstack("luaobjarch");
    
    //luastacksize(lua_gettop(LUA) + 3);
    
    //fprintf(errorlog, "*** Starting to check object\n");
    
    if (stackarg != -1) {
        recover_error("luaobjarch.stackarg != -1");
        RETURN(0);
    }
    
    // maybe it's a direct object reference not a string
    if (lua_istable(LUA, stackarg)) {
        DSBLerror_nonfatal(LUA, "%s", "Archetypes must be specified as strings");
        v_upstack();
        v_onstack("luaobjarch.table");
        end_pop = 2;        
        goto ALREADY_GOT_OBJECT;
    }
    
    if (!lua_isstring(LUA, stackarg)) {
        errstr = "Invalid string";
        goto LUA_OBJECT_ERROR;
    }
        
    //fprintf(errorlog, "*** object name is a string\n");
        
    nameref = lua_tostring(LUA, stackarg);
    
    lua_getglobal(LUA, "obj");
    lua_pushstring(LUA, nameref);
    lua_gettable(LUA, -2);

    if (!lua_istable(LUA, -1)) {
        errstr = dsbfastalloc(100);
        snprintf(errstr, 100, "\"%s\" is unknown", nameref);
        lua_pop(LUA, 2);
        goto LUA_OBJECT_ERROR;
    }
        
    //fprintf(errorlog, "*** object is a valid table\n");
    
    ALREADY_GOT_OBJECT:
    v_onstack("luaobjarch.regobj_lookup");
    lua_pushstring(LUA, "regobj");
    lua_gettable(LUA, -2);  
    v_upstack();
    
    if (!lua_islightuserdata(LUA, -1)) {
        errstr = dsbfastalloc(100);
        snprintf(errstr, 100, "\"%s\" is invalid (undefined)", nameref);
        lua_pop(LUA, end_pop);
        goto LUA_OBJECT_ERROR;
    }
        
    //fprintf(errorlog, "*** object has regobj\n");
    
    lua_pop(LUA, 1);
    
    lua_pushstring(LUA, "regnum");
    lua_gettable(LUA, -2);  

    if (!lua_islightuserdata(LUA, -1)) {
        errstr = dsbfastalloc(100);
        snprintf(errstr, 100, "\"%s\" is invalid (no index)", nameref);
        lua_pop(LUA, end_pop);
        goto LUA_OBJECT_ERROR;
    }
        
    //fprintf(errorlog, "*** object has regnum\n"); 
    
    objarch = (unsigned int)lua_touserdata(LUA, -1);
    
    // pop the userdata and the tables
    lua_pop(LUA, end_pop);
    
    RETURN(objarch);
    
    LUA_OBJECT_ERROR:
        v_onstack("luaobjarch.obj_error");
        DSBLerror(LUA, "%s requires object arch (%s)", fname, errstr);
        v_upstack();
        RETURN(0xFFFFFFFF);
}

// push "self" and "id" onto the stack and start a call
int member_begin_call(unsigned int inst, const char *func) {
    struct obj_arch *p_arch;
    char *luaname;
    unsigned int archnum;
    char *tmpvar = "_DSBCTMP";
    char *tmpfvar = "_DSBFTMP";
    
    onstack("lua_call.begin");
    stackbackc('(');
    v_onstack(func);
    addstackc(')');
    
    archnum = oinst[inst]->arch;
    p_arch = Arch(archnum);
    luaname = p_arch->luaname;
    
    luastacksize(5);
    
    lua_getglobal(LUA, "obj");
    lua_pushstring(LUA, luaname);
    lua_gettable(LUA, -2);
    lua_setglobal(LUA, tmpvar);
    lua_getglobal(LUA, tmpvar);
    lua_pushstring(LUA, func);
    lua_gettable(LUA, -2);
    
    if (!lua_isfunction(LUA, -1)) {
        lua_pop(LUA, 3);
        RETURN(0);
    }
    
    lua_setglobal(LUA, tmpfvar);
    lua_pop(LUA, 2);
    
    lua_getglobal(LUA, tmpfvar);
    lua_getglobal(LUA, tmpvar);
    lua_pushinteger(LUA, inst);
    
    {
        char comname[256];
        snprintf(comname, sizeof(comname), "%s.%s", luaname, func);
        gd.s_tmpluastr = dsbstrdup(comname);
    }
    
    RETURN(1);
}

void member_finish_call(int addargs, int resv) {

    onstack("lua_call.finish");
    stackbackc('(');
    v_onstack(gd.s_tmpluastr);
    addstackc(')');
    
    if (lua_pcall(LUA, addargs + 2, resv, 0) != 0) {
        lua_function_error(gd.s_tmpluastr, lua_tostring(LUA, -1));
    }

    condfree(gd.s_tmpluastr);
    gd.s_tmpluastr = NULL;
    
    VOIDRETURN();
}

int member_retrieve_stackparms(int count, int *dest) {
    int i;
    int b = count * -1;
    int nils = 0;
    
    onstack("member_retrieve_stackparms");
    
    for (i=0;i<count;++i) {
        int v;
        
        if (lua_isnil(LUA, b)) {
            v = 0;
            ++nils;
        } else if (lua_isboolean(LUA, b))
            v = lua_toboolean(LUA, b);
        else
            v = lua_tointeger(LUA, b);
            
        ++b;
        dest[i] = v;
    }
    lua_pop(LUA, count);
    lstacktop();
    
    RETURN(nils);
}

int lua_call_member(unsigned int inst, const char *funcname,
    int i_parm, int i_parm_2, int i_parm_3, int i_parm_4)
{
    struct obj_arch *p_arch;
    char *luaname;
    unsigned int archnum;
    int nparm = 2;
    int rv;
    char *tmpvar = "_DSBTMP";
    
    // set up the callstack later on for this one--
    onstack("lua_call.i");
    stackbackc('(');
    v_onstack(funcname);
    addstackc(')');
    
    if (!oinst[inst]) {
        RETURN(0);
    }
    
    archnum = oinst[inst]->arch;
    p_arch = Arch(archnum);
    luaname = p_arch->luaname;
    
    lua_getglobal(LUA, "obj");
    lua_pushstring(LUA, luaname);
    lua_gettable(LUA, -2);
    lua_setglobal(LUA, tmpvar);
    lua_getglobal(LUA, tmpvar);
    lua_pushstring(LUA, funcname);
    
    lua_gettable(LUA, -2);
    
    if (!lua_isfunction(LUA, -1)) {
        int rv = 0;
        
        // make a direct eval possible
        if (lua_isnumber(LUA, -1))
            rv = lua_tointeger(LUA, -1);
        else if (lua_isboolean(LUA, -1))
            rv = lua_toboolean(LUA, -1);
            
        lua_pop(LUA, 3);

        // If by some strange coincidence, the function is returning the
        // weird value that says "our answer is on the Lua stack," then
        // the simplest thing to do is actually PUT it on the Lua stack.
        if (rv == CRAZY_UNDEFINED_VALUE) {
            lua_pushinteger(LUA, CRAZY_UNDEFINED_VALUE);
        }
        RETURN(rv);
    }
    
    // push it on from the tmpvar again    
    lua_getglobal(LUA, tmpvar);
           
    lua_pushinteger(LUA, inst);
    
    // push the extra params if exist
    if (i_parm != -1)
        lua_pushinteger(LUA, i_parm);
    else
        lua_pushnil(LUA);
        
    if (i_parm_2 != -1)
        lua_pushinteger(LUA, i_parm_2);
    else
        lua_pushnil(LUA);
        
    if (i_parm_3 != -1)
        lua_pushinteger(LUA, i_parm_3);
    else
        lua_pushnil(LUA);
        
    if (i_parm_4 != -1)
        lua_pushinteger(LUA, i_parm_4);
    else
        lua_pushnil(LUA);

    nparm += 4;
    
    // fix the previous
    upstack();
    // put the whole thing's name on callstack
    v_luaonstack(luaname);
    stackbackc('.');
    v_onstack(funcname);
    
    // call it    
    if (lua_pcall(LUA, nparm, 1, 0) != 0) {
        char comname[256];
        snprintf(comname, sizeof(comname), "%s.%s", luaname, funcname);
        lua_function_error(comname, lua_tostring(LUA, -1));
        RETURN(CRAZY_UNDEFINED_VALUE);
    }
    
    // restore callstack
    RETURN(CRAZY_UNDEFINED_VALUE);
}

char *call_member_func_s(unsigned int inst, const char *funcname, int i_parm) {
    char *d_str;
    int t;
        
    onstack("lua_call.member_func_s");
    
    t = lua_call_member(inst, funcname, i_parm, -1, -1, -1);
    if (t != CRAZY_UNDEFINED_VALUE) {
        DSBLerror(LUA, "Bad stringvar in %s", funcname);
        RETURN(NULL);
    }
        
    if (lua_isstring(LUA, -1)) {
        d_str = dsbstrdup(lua_tostring(LUA, -1));
    } else
        d_str = NULL;
    
    lua_pop(LUA, 3);
    
    RETURN(d_str);
}
    
int call_member_func(unsigned int inst, const char *funcname, int i_parm) {
    int t;
    int rv = 0;
    
    onstack("lua_call.member_func");

    luastacksize(8);
    
    t = lua_call_member(inst, funcname, i_parm, -1, -1, -1);
    
    if (t != CRAZY_UNDEFINED_VALUE)
        RETURN(t);
    
    if (lua_isnil(LUA, -1))
        rv = 0;
    else if (lua_isboolean(LUA, -1))
        rv = lua_toboolean(LUA, -1);
    else
        rv = lua_tointeger(LUA, -1);
    
    // pop the result and the two tables
    lua_pop(LUA, 3);
    lstacktop(); 
    
    // all done
    RETURN(rv);   
}

int call_member_func2(unsigned int inst, const char *funcname, int ip, int ip2) {
    int t;
    int rv = 0;
    
    onstack("lua_call.member_func2");
    
    luastacksize(8);
    
    t = lua_call_member(inst, funcname, ip, ip2, -1, -1);
    
    if (t != CRAZY_UNDEFINED_VALUE)
        RETURN(t);
    
    if (lua_isnil(LUA, -1))
        rv = 0;
    else if (lua_isboolean(LUA, -1))
        rv = lua_toboolean(LUA, -1);
    else
        rv = lua_tointeger(LUA, -1);
    
    // pop the result and the two tables
    lua_pop(LUA, 3);
    lstacktop(); 
    
    // all done
    RETURN(rv);   
}

int call_member_func3(unsigned int inst, const char *funcname, int ip, int ip2, int ip3) {
    int t;
    int rv = 0;

    onstack("lua_call.member_func2");

    luastacksize(8);

    t = lua_call_member(inst, funcname, ip, ip2, ip3, -1);

    if (t != CRAZY_UNDEFINED_VALUE)
        RETURN(t);

    if (lua_isnil(LUA, -1))
        rv = 0;
    else if (lua_isboolean(LUA, -1))
        rv = lua_toboolean(LUA, -1);
    else
        rv = lua_tointeger(LUA, -1);

    // pop the result and the two tables
    lua_pop(LUA, 3);
    lstacktop();

    // all done
    RETURN(rv);
}

int call_member_func4(unsigned int inst, const char *funcname,
    int ip, int ip2, int ip3, int ip4) 
{
    int t;
    int rv = 0;

    onstack("lua_call.member_func4");

    luastacksize(8);

    t = lua_call_member(inst, funcname, ip, ip2, ip3, ip4);

    if (t != CRAZY_UNDEFINED_VALUE)
        RETURN(t);

    if (lua_isnil(LUA, -1))
        rv = 0;
    else if (lua_isboolean(LUA, -1))
        rv = lua_toboolean(LUA, -1);
    else
        rv = lua_tointeger(LUA, -1);

    // pop the result and the two tables
    lua_pop(LUA, 3);
    lstacktop();

    // all done
    RETURN(rv);
}
    
int lc_parm_int(const char *fname, int parms, ...) {
    int rv;
    int schk;
    va_list arg_list;
    
    onstack("lua_call.parm_int");
    stackbackc('(');
    v_onstack(fname);
    addstackc(')');
    
    luastacksize(parms+3);
    
    lua_getglobal(LUA, fname);
    if (!lua_isfunction(LUA, -1)) {
        lua_pop(LUA, 1);
        RETURN(0);
    }
    
    if (parms) {
        int n_parms = parms;
        va_start(arg_list, parms);
        
        while (n_parms) {
            int arg = va_arg(arg_list, int);
            
            if (gd.lua_bool_hack && arg == LHFALSE)
                lua_pushboolean(LUA, 0);
            else if (gd.lua_bool_hack && arg == LHTRUE)
                lua_pushboolean(LUA, 1);
            else if (!gd.lua_take_neg_one && arg == -1)
                lua_pushnil(LUA);
            else   
                lua_pushinteger(LUA, arg);
                
            --n_parms;
        }
        
        va_end(arg_list);
    }
    
    gd.lua_take_neg_one = 0;
    gd.lua_bool_hack = 0;

    upstack();
    v_luaonstack(fname);
    rv = lc_call_topstack(parms, fname);
    
    RETURN(rv);
}
        

int lc_call_topstack(int parms, const char *fname) {
    int rv = 0;
        
    if (lua_pcall(LUA, parms, 1, 0) != 0)
        lua_function_error(fname, lua_tostring(LUA, -1));
             
    if (lua_isnil(LUA, -1)) {
        lua_pop(LUA, 1);
        return 0;
    }
    
    gd.zero_int_result = 0;
    if (!lua_isnumber(LUA, -1)) {
        if (lua_isboolean(LUA, -1))
            rv = lua_toboolean(LUA, -1);
        else {
            if (lua_isnil(LUA, -1))
                rv = 0;
            else
                lua_return_function_error(fname, "int");
        }
    } else {
        rv = lua_tointeger(LUA, -1);
        if (rv == 0)
            gd.zero_int_result = 1;
    }
    
    lua_pop(LUA, 1);
    lstacktop();
    return(rv);
}

void lc_call_topstack_two_results(int parms, const char *fname, int *p1, int *p2) {
    onstack("lc_call_topstack_two_results");
        
    if (lua_pcall(LUA, parms, 2, 0) != 0)
        lua_function_error(fname, lua_tostring(LUA, -1));

    if (lua_isnil(LUA, -2)) *p1 = 0;
    else *p1 = lua_tointeger(LUA, -2);   

    if (lua_isnil(LUA, -1)) *p2 = 0;
    else *p2 = lua_tointeger(LUA, -1);             
        
    lua_pop(LUA, 2);
    lstacktop();
    
    VOIDRETURN();
}

void execute_stacktop_manifest(int bp) {
    int n = 1;
    
    onstack("execute_stacktop_manifest");
    
    while (n < 200) {
        lua_pushinteger(LUA, n);
        lua_gettable(LUA, -2);    
        if (lua_isstring(LUA, -1)) {
            const char *filename = lua_tostring(LUA, -1);
            
            if (bp == 1) {
                src_lua_file(bfixname(filename), 0);
            } else {
                src_lua_file(fixname(filename), 0);
            }
  
        } else {
            lua_pop(LUA, 1);
            VOIDRETURN();
        }
        
        lua_pop(LUA, 1);
        ++n;
    }
    
    VOIDRETURN();
}

void manifest_files(int bp, int compiled_base) {
    onstack("manifest_files");
    
    lua_getglobal(LUA, "lua_manifest");    
    if (!lua_istable(LUA, -1)) {
        lua_pop(LUA, 1);
        VOIDRETURN();
    }
    
    if (!compiled_base) {
        lua_pushstring(LUA, "source_only");
        lua_gettable(LUA, -2);
        if (lua_istable(LUA, -1)) {
            execute_stacktop_manifest(bp);    
        }
        lua_pop(LUA, 1);       
    }
    
    execute_stacktop_manifest(bp);

    lua_pop(LUA, 1);
    lc_parm_int("__clearmanifest", 0);
    VOIDRETURN();        
}

void unreflua(int ref) {
    luaL_unref(LUA, LUA_REGISTRYINDEX, ref);
}


void DSBLerror(lua_State *LUA, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    
    v_onstack("DSBLerror");
    
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    
    if (gd.lua_nonfatal) {
        const char *where_str;
        
        luastacksize(3);
        luaL_where(LUA, 1);
        
        where_str = lua_tostring(LUA, -1);
        fprintf(errorlog, "LUA ERROR: %s%s\n", where_str, buf);
        fflush(errorlog);
        lua_pop(LUA, 1);
        --gd.lua_nonfatal;
        ++gd.lua_error_found;
    } else {
        luaL_error(LUA, "%s", buf);
    }
  
    v_upstack();
}

void DSBLerror_nonfatal(lua_State *LUA, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    const char *where_str;
    
    v_onstack("DSBLerror_nonfatal");
    
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
       
    luastacksize(3);
    luaL_where(LUA, 1);
        
    where_str = lua_tostring(LUA, -1);
    fprintf(errorlog, "LUA ERROR: %s%s\n", where_str, buf);
    fflush(errorlog);
    lua_pop(LUA, 1);
    ++gd.lua_error_found;

    v_upstack();
}
