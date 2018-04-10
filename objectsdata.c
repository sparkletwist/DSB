#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <lauxlib.h>
#include <lualib.h>
#include "lproto.h"
#include "objects.h"
#include "global_data.h"
#include "champions.h"
#include "lua_objects.h"
#include "uproto.h"
#include "timer_events.h"
#include "monster.h"
#include "arch.h"
#include "editor_shared.h"
#include "gproto.h"

extern int CLOCKFREQ;
struct S_arches garch;

struct inst *oinst[NUM_INST];
char inst_dirty[NUM_INST];

extern struct dungeon_level *dun;
extern int debug;
extern FILE *errorlog;
extern struct global_data gd;
extern struct inventory_info gii;

extern int lua_cz_n;
extern struct clickzone *lua_cz;

extern lua_State *LUA;

extern int ZID;

const char *Sys_Inventory = "sys_inventory";

int inside_sum_load(struct inst *p_inst, int rc) {
    int i;
    int loadsum = 0;
    
    if (rc > 50) {
        recursion_puke();
        return 0;
    }
    
    for(i=0;i<p_inst->inside_n;++i) {
        if (p_inst->inside[i]) {
            struct inst *p_i_inst = oinst[p_inst->inside[i]];
            struct obj_arch *p_arch = Arch(p_i_inst->arch);
            
            loadsum += p_arch->mass;
            
            if (p_i_inst->inside_n)
                loadsum += inside_sum_load(p_i_inst, rc+1);
        }      
    }
    
    return loadsum;
}

int sum_load(unsigned int who) {
    int loadsum = 0;
    struct champion *me;
    int z;
    
    onstack("sum_load");
    
    me = &(gd.champs[who]);
    
    // leader carries the mouseobj
    if (gd.leader >= 0 && gd.party[gd.leader] == (who+1)) {
        if (gd.mouseobj) {
            loadsum += ObjMass(gd.mouseobj);
        }
    }
    
    for(z=0;z<gii.max_invslots;z++) {
        if (me->inv[z]) {
            struct inst *p_inst = oinst[me->inv[z]];
            struct obj_arch *p_arch = Arch(p_inst->arch);
            
            loadsum += p_arch->mass;
        
            if (p_inst->inside_n) {
                // do this to avoid puking the stack if the recursion is bad
                onstack("inside_sum_load");
                loadsum += inside_sum_load(p_inst, 0);
                upstack();
            }
        }
    }
    
    RETURN(loadsum);
}

unsigned int ObjMass(unsigned int I) {
    int loadsum;
    struct obj_arch *p_arch;
    
    onstack("ObjMass");
    
    p_arch = Arch(oinst[I]->arch);
    loadsum = p_arch->mass;
    
    if (oinst[I]->inside_n) {
        onstack("inside_sum_load");
        loadsum += inside_sum_load(oinst[I], 0);
        upstack();
    }
    
    RETURN(loadsum);
}

int iidxlookup(int integer_index) {
    int r;
    
    onstack("iidxlookup");
    
    luastacksize(3);
    
    lua_pushinteger(LUA, integer_index);
    lua_gettable(LUA, -2);
    
    if (lua_isnumber(LUA, -1))
        r = lua_tointeger(LUA, -1);
    else
        r= 0;
        
    lua_pop(LUA, 1);
    
    RETURN(r);
}

int boollookup(const char *s_name, int default_value) {
    int r;
    
    onstack("boollookup");
    
    luastacksize(3);
    
    lua_pushstring(LUA, s_name);
    lua_gettable(LUA, -2);
    
    if (lua_isboolean(LUA, -1))
        r = lua_toboolean(LUA, -1);
    else
        r = default_value;
        
    lua_pop(LUA, 1);
    
    RETURN(r);
}

int integerlookup(const char *s_name, int default_value) {
    int r;
    
    onstack("integerlookup");
    
    luastacksize(3);
    
    lua_pushstring(LUA, s_name);
    lua_gettable(LUA, -2);
    
    if (lua_isnumber(LUA, -1))
        r = lua_tointeger(LUA, -1);
    else
        r = default_value;
        
    lua_pop(LUA, 1);
    
    RETURN(r);
}

int tablecheckpresence(const char *s_name) {
    int r;
    
    onstack("tablecheckpresence");
    
    luastacksize(3);
    
    lua_pushstring(LUA, s_name);
    lua_gettable(LUA, -2);
    
    if (lua_istable(LUA, -1))
        r = 1;
    else
        r = 0;
        
    lua_pop(LUA, 1);
    
    RETURN(r);
}

char *queryluaglobalstring(const char *strid) {
    char *s;
    
    onstack("queryluaglobalstring");
    
    luastacksize(3);
    lua_getglobal(LUA, strid);
    if (lua_isstring(LUA, -1)) {
        s = dsbstrdup(lua_tostring(LUA, -1));
    } else {
        s = dsbstrdup("UNDEFINED");
    }
    lua_pop(LUA, 1);
    
    RETURN(s);
}

int queryluapresence(struct obj_arch *p_arch, char *chkstr) {
    int v = 0;
    
    luastacksize(4);
    
    lua_getglobal(LUA, "obj");
    lua_pushstring(LUA, p_arch->luaname);
    lua_gettable(LUA, -2);
    lua_pushstring(LUA, chkstr);
    lua_gettable(LUA, -2);
    v = (lua_isnil(LUA, -1));
    lua_pop(LUA, 3);
    
    return !v;
}

int queryluabool(struct obj_arch *p_arch, char *chkstr) {
    int v = 0;
    
    luastacksize(4);
    
    lua_getglobal(LUA, "obj");
    lua_pushstring(LUA, p_arch->luaname);
    lua_gettable(LUA, -2);
    lua_pushstring(LUA, chkstr);
    lua_gettable(LUA, -2);
    v = (lua_toboolean(LUA, -1));
    lua_pop(LUA, 3);
    
    return v;
}

int queryluaint(struct obj_arch *p_arch, char *chkstr) {
    int v = 0;
    
    luastacksize(4);
    
    lua_getglobal(LUA, "obj");
    lua_pushstring(LUA, p_arch->luaname);
    lua_gettable(LUA, -2);
    lua_pushstring(LUA, chkstr);
    lua_gettable(LUA, -2);
    v = (lua_tointeger(LUA, -1));
    lua_pop(LUA, 3);
    
    return v;
}

int rqueryluaint(struct obj_arch *p_arch, char *chkstr) {
    int v = 0;
    
    luastacksize(4);
    
    lua_getglobal(LUA, "obj");
    lua_pushstring(LUA, p_arch->luaname);
    lua_gettable(LUA, -2);
    lua_pushstring(LUA, chkstr);
    lua_gettable(LUA, -2);
    if (LIKELY(lua_isnumber(LUA, -1))) {
        v = (lua_tointeger(LUA, -1));
    } else {
        v = -1;
        gd.lua_nonfatal = 1;
        DSBLerror(LUA, "%s.%s: Integer expected", p_arch->luaname, chkstr);
    }
    
    lua_pop(LUA, 3);
    
    return v;
}

// this function is the more user friendly interface to luaoptbitmap()
// and it also cleans up the mess that it leaves on the Lua stack
struct animap *getluaoptbitmap(char *xname, char *nam) {
    struct animap *rani;
    
    onstack("getluaoptbitmap");
    
    luastacksize(4);
    
    lua_pushstring(LUA, nam);
    lua_gettable(LUA, -2);   
    gd.tmpstr = nam;
    
    rani = luaoptbitmap(LUA, -1, xname);
    lua_pop(LUA, 1);
    
    gd.tmpstr = NULL;
    
    RETURN(rani);
}

// this one takes numbers
struct animap *getluaoptnbitmap(char *xname, int idx) {
    struct animap *rani;

    onstack("getluaoptnbitmap");
    
    luastacksize(4);

    lua_pushinteger(LUA, idx);
    lua_gettable(LUA, -2);
    gd.tmpstr = "optn";

    rani = luaoptbitmap(LUA, -1, xname);
    lua_pop(LUA, 1);

    gd.tmpstr = NULL;

    RETURN(rani);
}

int ex_exvar(unsigned int inst, const char *vname, int *store) {
    int rv = 0;
    
    luastacksize(4);
    
    lua_getglobal(LUA, "exvar");
    lua_pushinteger(LUA, inst);
    lua_gettable(LUA, -2);
    
    if (!lua_istable(LUA, -1)) {
        lua_pop(LUA, 2);
        return 0;
    }
    
    lua_pushstring(LUA, vname);
    lua_gettable(LUA, -2);
    
    if (lua_isnumber(LUA, -1))
        rv = lua_tointeger(LUA, -1);
    else if (lua_isboolean(LUA, -1)) {
        rv = lua_toboolean(LUA, -1);
        if (rv == 0) {
            lua_pop(LUA, 3);
            return 0;
        }
    } else {
        lua_pop(LUA, 3);
        return 0;
    }
        
    lua_pop(LUA, 3);
    if (store)
        *store = rv;    
    return 1;
}

char *ex_grab_table_string(const char *vname, const char *altname) {
    char *rv = NULL;

    if (!lua_istable(LUA, -1)) {
        lua_pop(LUA, 2);
        return NULL;
    }
    
    lua_pushstring(LUA, vname);
    lua_gettable(LUA, -2);
    
    if (lua_isstring(LUA, -1))
        rv = dsbstrdup(lua_tostring(LUA, -1));
    else if (altname && 
        lua_isboolean(LUA, -1) && lua_toboolean(LUA, -1)) 
    {
        lua_pop(LUA, 1);
        lua_pushstring(LUA, altname);
        lua_gettable(LUA, -2);
        if (lua_isstring(LUA, -1))
            rv = dsbstrdup(lua_tostring(LUA, -1));
    }
        
    lua_pop(LUA, 3);    
    return rv;
}

char *exs_exvar(unsigned int inst, char *vname) {

    luastacksize(4);
    
    lua_getglobal(LUA, "exvar");
    lua_pushinteger(LUA, inst);
    lua_gettable(LUA, -2);
    
    return (ex_grab_table_string(vname, NULL));
}

// only used by ESB
char *exs_alt_exvar(unsigned int inst, const char *vname, const char *altvname) {

    luastacksize(5);
    
    lua_getglobal(LUA, "exvar");
    lua_pushinteger(LUA, inst);
    lua_gettable(LUA, -2);
    
    return (ex_grab_table_string(vname, altvname));
}

char *exs_archvar(unsigned int archnum, char *vname) { 
    struct obj_arch *p_arch;
    
    luastacksize(4);
    
    p_arch = Arch(archnum);
       
    lua_getglobal(LUA, "obj");
    lua_pushstring(LUA, p_arch->luaname);
    lua_gettable(LUA, -2);
    
    return (ex_grab_table_string(vname, NULL));
}

void apply_rhack(struct obj_arch *o_ptr, char *rhackname, int flag) {

    luastacksize(2);
    
    lua_pushstring(LUA, rhackname);
    lua_gettable(LUA, -2);
    
    if (lua_isboolean(LUA, -1)) {
        int bv = lua_toboolean(LUA, -1);
        if (bv) o_ptr->rhack |= flag;
    }
    
    lua_pop(LUA, 1);  
}

void apply_rhack_f(struct obj_arch *o_ptr, char *rhackname, int flag) {

    luastacksize(2);
    
    lua_pushstring(LUA, rhackname);
    lua_gettable(LUA, -2);
    
    if (lua_isfunction(LUA, -1)) {
        o_ptr->rhack |= flag;
    }
    
    lua_pop(LUA, 1);  
}

void apply_archflag(struct obj_arch *o_ptr, char *aflagname, int flag) {

    luastacksize(2);
    
    lua_pushstring(LUA, aflagname);
    lua_gettable(LUA, -2);
    
    if (lua_isboolean(LUA, -1)) {
        int bv = lua_toboolean(LUA, -1);
        if (bv) o_ptr->arch_flags |= flag;
    }
    
    lua_pop(LUA, 1);  
}


void destroy_attack_methods(struct att_method *meth) {
    int n;
    struct att_method *op = meth;
    
    onstack("destroy_attack_methods");
    
    for(n=0;n<ATTACK_METHOD_TABLESIZE;++n) {
        if (op[n].name != NULL) {
            luaL_unref(LUA, LUA_REGISTRYINDEX, op[n].luafunc);    
            dsbfree(op[n].name);
        }
    }
               
    DESTROY_STRUCTURE:
    dsbfree(meth);
    
    VOIDRETURN();
}

void import_monattack_views(char *nam, struct mon_att_view *dv) {
    int n;
    const char *aname = "attack";
    struct animap *tviews[32] = { 0 };
    
    onstack("import_monattack_views");
    
    tviews[0] = getluaoptbitmap(nam, (char*)aname);
    if (tviews[0] != NULL) {
        dv->n = 1;
        
        dv->v = dsbcalloc(1, sizeof(struct animap*));
        dv->v[0] = tviews[0];
        
        VOIDRETURN();
    }
    
    lua_pushstring(LUA, aname);
    lua_gettable(LUA, -2);
    if (lua_istable(LUA, -1)) {
        int i;
        for (i=0;i<32;++i) {
            tviews[i] = getluaoptnbitmap(nam, i + 1);
            if (tviews[i] == NULL) {
                if (i == 0) {
                    DSBLerror(LUA, "No attack bitmaps defined at all!");
                    VOIDRETURN();
                }
                break;
            }
        }
        
        dv->n = i;
        dv->v = dsbcalloc(i, sizeof(struct animap*));
        
        for(n=0;n<dv->n;++n) {
            dv->v[n] = tviews[n];
        }
    }
    lua_pop(LUA, 1);
    
    VOIDRETURN();
}

struct att_method *import_attack_methods(const char *obj_name) {
    struct att_method op[ATTACK_METHOD_TABLESIZE];
    struct att_method *ret;
    int n;
    int s_ret = 0;
    
    onstack("import_attack_methods");
    stackbackc('(');
    v_onstack(obj_name);
    addstackc(')');
    
    memset(op, 0, sizeof(struct att_method) * ATTACK_METHOD_TABLESIZE);
    
    luastacksize(5);
    
    for (n=1;n<=16;++n) {
        char *mname;
        int tempval;
        int reqclass = 0;
        int reqclass_sub = 0;
        
        lua_pushinteger(LUA, n);
        lua_gettable(LUA, -2);
        if (!lua_istable(LUA, -1)) {
            lua_pop(LUA, 1);
            goto METHODS_GOT;
        }
            
        lua_pushinteger(LUA, 1);
        lua_gettable(LUA, -2);
        if (lua_isstring(LUA, -1))
            mname = dsbstrdup(lua_tostring(LUA, -1));
        else {
            DSBLerror(LUA, "%s: Invalid attack method name", obj_name);
            lua_pop(LUA, 2);
            RETURN(NULL);
        }           
        lua_pop(LUA, 1);
        op[s_ret].name = mname;
        
        lua_pushinteger(LUA, 2);
        lua_gettable(LUA, -2);
        tempval = luaint(LUA, -1, obj_name, 2);
        validate_xplevel(tempval);
        op[s_ret].minlev = tempval;
        lua_pop(LUA, 1);
        
        lua_pushinteger(LUA, 3);
        lua_gettable(LUA, -2);
        if (lua_istable(LUA, -1)) {
            reqclass = iidxlookup(1);
            reqclass_sub = iidxlookup(2);
        } else {
            reqclass = luaint(LUA, -1, obj_name, 3);
        }
        validate_xpclass(reqclass);
        op[s_ret].reqclass = reqclass;
        op[s_ret].reqclass_sub = reqclass_sub;
        lua_pop(LUA, 1);
        
        lua_pushinteger(LUA, 4);
        lua_gettable(LUA, -2);
        if (!lua_isfunction(LUA, -1) && !lua_isstring(LUA, -1)) { 
            DSBLerror(LUA, "%s.%s: Invalid attack method function",
                obj_name, mname);
                
            lua_pop(LUA, 2);
            RETURN(NULL);
        }
        tempval = luaL_ref(LUA, LUA_REGISTRYINDEX); 
        op[s_ret].luafunc = tempval;
        
        // the luaL_ref removes function at [4],
        // so this pops the original table
        lua_pop(LUA, 1);
        
        ++s_ret;
    }
    
    METHODS_GOT:
    if (s_ret) {
        ret = dsbcalloc(ATTACK_METHOD_TABLESIZE, sizeof(struct att_method));
        memcpy(ret, op, s_ret*sizeof(struct att_method));       
    } else
        ret = NULL;
        
    lstacktop();
    
    RETURN(ret);
}

void init_exvars(void) {
    lua_newtable(LUA);
    lua_setglobal(LUA, "exvar");

    lua_newtable(LUA);
    lua_setglobal(LUA, "ch_exvar");
}

void init_all_objects(void) {
    onstack("init_all_objects");
    
    garch.ac[AC_BASE] = NULL;
    
    memset(oinst, 0, sizeof(struct inst*)*NUM_INST);
    //fix_inst_int();
    
    lua_cz_n = 0;
    lua_cz = NULL;
    
    VOIDRETURN();   
}

int in_limbo(unsigned int inst) {
    
    onstack("in_limbo");

    if (oinst[inst] && oinst[inst]->level == LOC_LIMBO) {
        RETURN(1);
    } else {
        RETURN(0);
    } 
          
}

void obj_at(int inst, int lev, int xx, int yy, int dir) {         
    onstack("obj_at");
    
    oinst[inst]->prev.x = oinst[inst]->x;
    oinst[inst]->prev.y = oinst[inst]->y;
    oinst[inst]->prev.level = oinst[inst]->level;
    oinst[inst]->prev.tile = oinst[inst]->tile;

    oinst[inst]->x = xx;
    oinst[inst]->y = yy;
    oinst[inst]->level = lev;
    oinst[inst]->tile = dir;
    
    VOIDRETURN();
}

int setup_colortable_convert(void) {
    onstack("setup_colortable_convert");
    
    lua_getglobal(LUA, "colorconvert");
    
    if (lua_istable(LUA, -1)) {
        RETURN(1);
    } else {
        lua_pop(LUA, 1);
        RETURN(0);
    }
}

unsigned char colortable_convert(BITMAP *m_map, int lev,
    int xx, int yy, unsigned char t)
{
    unsigned char rv;
    const char *s_cl_name;
    unsigned int objarch;
    const char *s_FNAME = "colortable_convert";
    
    onstack(s_FNAME);
    
    //fprintf(errorlog, "*** Grabbing table for %d at %d %d\n", t, xx, yy);
    
    lua_pushinteger(LUA, t);
    lua_gettable(LUA, -2);
    
    //fprintf(errorlog, "Got possible table\n");
    
    if (!lua_istable(LUA, -1)) {
        lua_pop(LUA, 1);
        RETURN(0);
    }
    
    //fprintf(errorlog, "Color %d has a table\n", t);
    
    lua_pushinteger(LUA, 1);
    lua_gettable(LUA, -2);
    rv = lua_tointeger(LUA, -1);
    lua_pop(LUA, 1);

    lua_pushinteger(LUA, 2);
    lua_gettable(LUA, -2);
    s_cl_name = luastring(LUA, -1, s_FNAME, 2);
    if (*s_cl_name == '_') {
        int b_grab = 1;
        int i_n = 3;
        char *s_tmpname = dsbstrdup(s_cl_name);
        char *s_usefuncname = s_tmpname + 1;
        
        lua_getglobal(LUA, s_usefuncname);
        if (!lua_isfunction(LUA, -1)) {
            poop_out("invalid_function");
            lua_pop(LUA, 3);
            dsbfree(s_tmpname);
            RETURN((rv != 0));
        }
        
        lua_pushinteger(LUA, lev);
        lua_pushinteger(LUA, xx);
        lua_pushinteger(LUA, yy);
        
        while (b_grab) {
            lua_pushinteger(LUA, i_n);
            lua_gettable(LUA, -1*i_n - 4);
            if (lua_isnil(LUA, -1)) {
                b_grab = 0;
                lua_pop(LUA, 1);
            } else
                ++i_n;
        }
        
        lc_call_topstack(i_n, s_FNAME);

        dsbfree(s_tmpname);
        
    } else {
        int dir;
        
        objarch = luaobjarch(LUA, -1, s_FNAME);
        
        lua_pushinteger(LUA, 3);
        lua_gettable(LUA, -3);
        dir = lua_tointeger(LUA, -1);
        lua_pop(LUA, 1);
        
        if (dir == -1) {
            int v;
            
            dir = 0;
            v = getpixel(m_map, xx+1, yy);
            if (v == 255) dir = 1;
            else {
                v = getpixel(m_map, xx, yy+1);
                if (v == 255) dir = 2;
                else {
                    v = getpixel(m_map, xx-1, yy);
                    if (v == 255) dir = 3;
                }
            }
            
        } else if (dir < 0 && dir > 4)
            dir = 0;
        
        create_instance(objarch, lev, xx, yy, dir, 0);
    }
    lua_pop(LUA, 2);

    RETURN((rv != 0));
}

void finish_colortable_convert(void) {
    onstack("finish_colortable_convert");
    
    lua_pop(LUA, 1);
    
    VOIDRETURN();
}

int objarchctl(int req, int ac) {
    static int maxobj[ARCH_CLASSES] = { 0 };
    static int curobj[ARCH_CLASSES] = { 0 };
    
    onstack("objarchctl");
    stackbackc('(');
    
    if (req == NEW_ARCH) {
        int i_pnum = 0;
        
        onstack("NEW)");
        
        do {    
            if (curobj[ac] >= maxobj[ac]) {
                int oldmax = maxobj[ac];
                int u;
                
                maxobj[ac] += 16;
                garch.ac[ac] = dsbrealloc(garch.ac[ac],
                    maxobj[ac] * sizeof(struct obj_arch));
                         
                for (u=oldmax;u<maxobj[ac];++u) {
                    garch.ac[ac][u].type = OBJTYPE_INVALID;
                }
                curobj[ac] = oldmax;
            }
            
            i_pnum = curobj[ac];
            curobj[ac] += 1;
            
        } while (garch.ac[ac][i_pnum].type != OBJTYPE_INVALID);
        
        RETURN(i_pnum + (ac << 24));
    }
    
    if (req == QUERY_ARCH) {

        onstack("QUERY)");
        
        if (ac < 0) {
            int c;
            int t = 0;
            for (c=0;c<ARCH_CLASSES;++c) {
                t += curobj[c];
            }
            RETURN(t);
        } else {
            RETURN(curobj[ac]);
        }
    }
        
    if (req == CLEAR_ALL_ARCH) {
        int an = 0;
        
        onstack("CLEAR_ALL)");
        
        for (an=0;an<ARCH_CLASSES;++an) {
            curobj[an] = 0;
            maxobj[an] = 0;
            free(garch.ac[an]);
            garch.ac[an] = NULL;
        }
    }
    
    if (req == INTEGRITY_CHECK) {
        int cnum;
        
        onstack("INTEGRITY_CHECK)");
        
        fprintf(errorlog, "**** ARCH INTEGRITY CHECK ****\n");
        for(cnum=0;cnum<ARCH_CLASSES;++cnum) {
            fprintf(errorlog, "ARCH: %d @addr %x\n",
                cnum, garch.ac[cnum]);
        }
        fprintf(errorlog, "******\n");
        
        RETURN(0);
    }
    
    RETURN(-1);
}

void init_inst(unsigned int inum) {
    oinst[inum] = dsbcalloc(1, sizeof(struct inst));
    //fix_inst_int();
}

unsigned int next_object_inst(unsigned int archnum, int last_alloc) {
    unsigned int onum = 1;
    int a_onum;
    int failed = 0;
    struct obj_arch *p_new_arch;
    
    onstack("next_object_inst");
    
    if (!gd.edit_mode)
        onum = last_alloc + 1;
        
    // things that can be opbys can't be < 20 to avoid weirdness with
    // triggers wanting custom data fields getting default messages
    if (onum < 20) {
        p_new_arch = Arch(archnum);
        if (p_new_arch->type >= OBJTYPE_MONSTER) {
            onum = 20;
        }
    }
    
    while (oinst[onum] != NULL || inst_dirty[onum]) {
        ++onum;
        if (onum == NUM_INST) {
            onum = 1;
            program_puke("No free insts");
            RETURN(0);
        }
    }
    
    a_onum = onum;
    init_inst(a_onum);
    RETURN(a_onum);
}

void purge_dirty_list(void) {
    onstack("purge_dirty_list");
    memset(inst_dirty, 0, sizeof(char) * NUM_INST);
    VOIDRETURN();
}

/*
void add_to_hash(int nhash, int id) {
    struct obj_hash_entry *my_hash;
    
    onstack("add_to_hash");
    
    my_hash = dsbmalloc(sizeof(struct obj_hash_entry));
    my_hash->o_n = id;
    my_hash->n = ohash[nhash];
    ohash[nhash] = my_hash;
    
    VOIDRETURN();
}

void destroy_hash(void) {
    int i_c;
    
    onstack("destroy_hash");
    
    for(i_c=0;i_c<HASHMAX;++i_c) {
        struct obj_hash_entry *h_cur;
        
        h_cur = ohash[i_c];
        while (h_cur != NULL) {
            struct obj_hash_entry *hcn = h_cur->n;
            dsbfree(h_cur);
            h_cur = hcn;
        }
        
        ohash[i_c] = NULL;
    }
    
    VOIDRETURN();
}

int make_hash(char *objnam) {
    unsigned int hval = 0;
    
    onstack("make_hash");

    if (objnam == NULL)
        RETURN(0);

    stackbackc('(');
    v_onstack(objnam);
    addstackc(')');
    
    while (*objnam != '\0') {
        hval += *objnam;
        ++objnam;
    }
    
    hval = (hval % HASHMAX);
    RETURN(hval);
}

int find_hash_arch(int t_obj_hash, char *objname) {
    struct obj_hash_entry *myoh;

    onstack("find_hash_arch");
    
    myoh = ohash[t_obj_hash];
    
    while (myoh != NULL) {
        struct obj_arch *p_arch = Arch(myoh->o_n);
        if (!strcmp(objname, p_arch->luaname))
            RETURN(myoh->o_n);
            
        myoh = myoh->n;
    }
    
    RETURN(-1);    
}
*/

/*
void object_file_error(char *iv, char *errstr, char *objname) {
    char buf[384];
            
    snprintf(buf, sizeof(buf), "Invalid %s %s for object %s", iv, errstr, objname);
    fprintf(errorlog, "ERROR: %s\n", buf);
    fclose(errorlog);
    MessageBox(win_get_window(), buf, "Error", MB_ICONSTOP);
    DSBallegshutdown();
    exit(1);
}
*/

void tweak_thing(struct inst *o_i) {
    o_i->x_tweak = 2*(DSBtrivialrand()%6 - DSBtrivialrand()%6);
    o_i->y_tweak = DSBtrivialrand()%5;
}

void register_object(lua_State *LUA, int graphical) {
    int regnum;
    char *nameref;
    char *typestr;
    char *badtype = "<unknown>";
    int typenum = OBJTYPE_INVALID;
    struct obj_arch *o_ptr;
    struct animap *icon_image;
    int trnum_used = 0;
    unsigned int trnum_value;
    int defmax;
    
    onstack("register_object");
    
    nameref = dsbstrdup(lua_tostring(LUA, -1));
    lua_pop(LUA, 1);
    
    luastacksize(6);
    lua_getglobal(LUA, "obj");
    lua_pushstring(LUA, nameref);
    lua_gettable(LUA, -2);
    
    if (!lua_istable(LUA, -1)) {
        char objerr[120];
        sprintf(objerr, "Bad object entry obj.%s", nameref);
        fprintf(errorlog, "OBJ ERROR: %s\n", objerr); 
        DSBLerror(LUA, objerr);
        VOIDRETURN();
    }
       
    lua_pushstring(LUA, "type");
    lua_gettable(LUA, -2);
    if (!lua_isstring(LUA, -1)) {
        fprintf(errorlog, "OBJ ERROR: Bad object type obj.%s\n", nameref); 
        typestr = dsbstrdup("UNDEFINED");
    } else {
        typestr = dsbstrdup(lua_tostring(LUA, -1));
    }
    lua_pop(LUA, 1);
    
    if (*typestr == 'T' && !strcmp(typestr, "THING"))
        typenum = OBJTYPE_THING;
    
    if (*typestr == 'D' && !strcmp(typestr, "DOOR"))
        typenum = OBJTYPE_DOOR;
        
    if (*typestr == 'W' && !strcmp(typestr, "WALLITEM"))
        typenum = OBJTYPE_WALLITEM;
        
    if (*typestr == 'H' && !strcmp(typestr, "HAZE"))
        typenum = OBJTYPE_HAZE;

    if (*typestr == 'C' && !strcmp(typestr, "CLOUD"))
        typenum = OBJTYPE_CLOUD; 
        
    if (*typestr == 'M' && !strcmp(typestr, "MONSTER"))
        typenum = OBJTYPE_MONSTER;
        
    if (*typestr == 'F') {
            if (!strcmp(typestr, "FLOORFLAT"))
                typenum = OBJTYPE_FLOORITEM;
                
            if (!strcmp(typestr, "FLOORUPRIGHT"))
                typenum = OBJTYPE_UPRIGHT;
    }
    
    if (*typestr == 'U' && !strcmp(typestr, "UNDEFINED"))
        typenum = OBJTYPE_UNDEFINED;
        
    if (typenum == OBJTYPE_INVALID) {
        typenum = OBJTYPE_UNDEFINED;
    }
    
    lua_pushstring(LUA, "trnum");
    lua_gettable(LUA, -2); 
    if (lua_islightuserdata(LUA, -1)) {
        trnum_used = 1;
        trnum_value = (unsigned int)lua_touserdata(LUA, -1); 
    }
    lua_pop(LUA, 1);

    lua_pushstring(LUA, "regnum");
    lua_gettable(LUA, -2);  
    
    stackbackc(':');
    v_onstack("check_regnum");
    if (!lua_islightuserdata(LUA, -1)) {
        
        if (!trnum_used) {
            int archtypenum = gd.arch_select;
            struct obj_arch *p_arch;
            
            if (typenum < OBJTYPE_UNDEFINED)
                archtypenum = gd.arch_select + typenum;
                
            regnum = objarchctl(NEW_ARCH, archtypenum);
            p_arch = Arch(regnum);
            
            if (debug) {
                fprintf(errorlog, "ARCH: Registered %s id %x (class %d num %d) @%x\n",
                    nameref, regnum, (regnum & 0xFF000000) >> 24,
                    regnum & 0x00FFFFFF, p_arch);
            }
            
            memset(p_arch, 0, sizeof(struct obj_arch));
            o_ptr = p_arch;
            o_ptr->type = typenum;
            goto NEW_OBJECT_DEC;
        } else {
            regnum = trnum_value;
            o_ptr = Arch(regnum);
            
            if (debug) {
                int xcl = (regnum & 0xFF000000) >> 24;
                fprintf(errorlog, "ARCH: Replaced %s id %x (class %d num %d) @%x\n",
                    nameref, regnum, xcl, regnum & 0x00FFFFFF, o_ptr);
            }
        }

    } else {
        regnum = (int)lua_touserdata(LUA, -1);
        o_ptr = Arch(regnum);
    }
    
    if (o_ptr->type != typenum)
        DSBLerror(LUA, "%s: Redeclarations cannot change type [%d to %d]",
            nameref, o_ptr->type, typenum);
    
    NEW_OBJECT_DEC:
    lua_pop(LUA, 1);
    
    if (graphical) {
        if (typenum == OBJTYPE_THING) {
            lua_pushstring(LUA, "icon");
            lua_gettable(LUA, -2);
            gd.tmpstr = "icon";
            icon_image = luabitmap(LUA, -1, nameref, 0);
            lua_pop(LUA, 1);
        } else
            icon_image = getluaoptbitmap(nameref, "icon");
    } else
        icon_image = NULL;

    if (o_ptr->luaname) dsbfree(o_ptr->luaname);
    o_ptr->luaname = nameref;
    o_ptr->icon = icon_image;
    
    // store the system name inside the object
    lua_pushstring(LUA, "ARCH_NAME");
    lua_pushstring(LUA, o_ptr->luaname);
    lua_settable(LUA, -3);
    
    lua_pushstring(LUA, "renderer_hack");
    lua_gettable(LUA, -2);
    o_ptr->rhack = 0;
    if (lua_isstring(LUA, -1)) {
        const char *r_hack = lua_tostring(LUA, -1);
        
        if (*r_hack == 'D') {
            if (!strcmp(r_hack, "DOORFRAME")) {
                o_ptr->rhack = RHACK_DOORFRAME;
            } else if (!strcmp(r_hack, "DOORBUTTON")) {
                o_ptr->rhack = RHACK_DOORBUTTON;
                o_ptr->rhack |= RHACK_DOORFRAME;
            }
        }

        if (*r_hack == 'F' && !strcmp(r_hack, "FAKEWALL"))
            o_ptr->rhack = RHACK_FAKEWALL;
            
        if (*r_hack == 'H' && !strcmp(r_hack, "HAZE"))
            o_ptr->rhack = RHACK_BLUEHAZE;
            
        if (*r_hack == 'I' && !strcmp(r_hack, "INVISIBLEWALL"))
            o_ptr->rhack = RHACK_INVISWALL;
            
        if (*r_hack == 'L') {
            if (!strcmp(r_hack, "LEFT"))
                o_ptr->dtype = DTYPE_LEFT;
            else if (!strcmp(r_hack, "LEFTRIGHT"))
                o_ptr->dtype = DTYPE_LEFTRIGHT;
        }
        
        if (*r_hack == 'M') {
            if (!strcmp(r_hack, "MIRROR")) {
                o_ptr->rhack = RHACK_MIRROR;
            } else if (!strcmp(r_hack, "MAGICDOOR")) {
                o_ptr->dtype = DTYPE_MAGICDOOR;
            }
        }
        
        if (*r_hack == 'N' && !strcmp(r_hack, "NORTHWEST"))
            o_ptr->dtype = DTYPE_NORTHWEST;
        
        if (*r_hack == 'P' && !strcmp(r_hack, "POWERSCALE"))
            o_ptr->rhack = RHACK_POWERSCALE;
            
        if (*r_hack == 'R' && !strcmp(r_hack, "RIGHT"))
            o_ptr->dtype = DTYPE_RIGHT;
            
        if (*r_hack == 'S') {
            if (!strcmp(r_hack, "STAIRS"))
                o_ptr->rhack = RHACK_STAIRS;
            else if (!strcmp(r_hack, "SOUTHEAST"))
                o_ptr->dtype = DTYPE_SOUTHEAST;
        }
            
        if (typenum == OBJTYPE_WALLITEM) {
            if (*r_hack == 'W' && !strcmp(r_hack, "WRITING"))
                o_ptr->rhack = RHACK_WRITING;
        }   
        if (typenum == OBJTYPE_MONSTER) {
            if (*r_hack == '2' && !strcmp(r_hack, "2WIDE"))
                o_ptr->rhack = RHACK_2WIDE;
        }        
    }
    lua_pop(LUA, 1);
    
    o_ptr->arch_flags = 0;
    apply_rhack(o_ptr, "only_defined_gfx", RHACK_ONLY_DEFINED);
    apply_rhack(o_ptr, "no_shade", RHACK_NOSHADE);
    
    if (typenum == OBJTYPE_WALLITEM) {
        apply_rhack(o_ptr, "wall_patch", RHACK_WALLPATCH);
        apply_archflag(o_ptr, "drop_zone", ARFLAG_DROP_ZONE);
        apply_archflag(o_ptr, "ignore_empty_clicks", ARFLAG_DZ_EXCHONLY);
        
        if (o_ptr->rhack & RHACK_WRITING) {
            o_ptr->s_parm1 = integerlookup("top_offset", 8);
            o_ptr->s_parm2 = integerlookup("mid_offset", 8);
        }
        
    }
        
    if (typenum == OBJTYPE_UPRIGHT)
        apply_rhack(o_ptr, "clickable", RHACK_CLICKABLE);
        
    if (typenum == OBJTYPE_MONSTER) {
        apply_archflag(o_ptr, "immobile", ARFLAG_IMMOBILE);
        apply_archflag(o_ptr, "no_stopclicks", ARFLAG_NOCZCLOBBER);
        apply_archflag(o_ptr, "instant_turn", ARFLAG_INSTTURN);
    }
      
    if (typenum == OBJTYPE_THING)
        apply_archflag(o_ptr, "flying_only", ARFLAG_FLY_ONLY);
        
    if (typenum == OBJTYPE_DOOR) {
        apply_archflag(o_ptr, "smooth", ARFLAG_SMOOTH);
    }
        
    apply_rhack_f(o_ptr, "bitmap_tweaker", RHACK_DYNCUT);
    apply_archflag(o_ptr, "draw_contents", ARFLAG_DRAW_CONTENTS);
    apply_archflag(o_ptr, "no_bump", ARFLAG_NO_BUMP);
    //apply_archflag(o_ptr, "priority", ARFLAG_PRIORITY);
    
    lua_pushstring(LUA, "namechanger");
    lua_gettable(LUA, -2);
    if (lua_isfunction(LUA, -1)) {
        o_ptr->rhack |= RHACK_GOT_NAMECHANGER;
    }
    lua_pop(LUA, 1);
        
    lua_pushstring(LUA, "name");
    lua_gettable(LUA, -2);
    if (lua_isstring(LUA, -1)) {
        o_ptr->dname = dsbstrdup(lua_tostring(LUA, -1));
    }
    lua_pop(LUA, 1);
    
    if (typenum == OBJTYPE_MONSTER) {
        unsigned int sz = integerlookup("size", 1);
        if (sz < 1 || sz > 4) sz = 1;
        o_ptr->msize = sz;
        if (sz == 2)
            o_ptr->rhack |= RHACK_2WIDE;
    } else
        o_ptr->mass = integerlookup("mass", 0);
        
    defmax = 32;
    if (o_ptr->arch_flags & ARFLAG_SMOOTH)
        defmax = (CLOCKFREQ*4);
    o_ptr->cropmax = integerlookup("cropmax", defmax);
    
    lua_pushstring(LUA, "viewangle");
    lua_gettable(LUA, -2);
    if (lua_isnumber(LUA, -1)) {
        int shiftval = (lua_tointeger(LUA, -1)) % 4;
        o_ptr->viewangle = (1 << shiftval) | 128;
    } else if (lua_istable(LUA, -1)) {
        unsigned char va = 128;
        int i;
        luastacksize(3);
        for (i=0;i<4;++i) {
            int is_valid;
            lua_pushinteger(LUA, i + 1);
            lua_gettable(LUA, -2);
            if (lua_toboolean(LUA, -1)) {
                va |= (1 << i);
            }
            lua_pop(LUA, 1);
        }
        o_ptr->viewangle = va;   
    }
    lua_pop(LUA, 1); 
    
    if (typenum == OBJTYPE_THING) {
        lua_pushstring(LUA, "def_charge");
        lua_gettable(LUA, -2);
        if (lua_isnumber(LUA, -1)) {
            unsigned int z;
            z = lua_tointeger(LUA, -1);
            o_ptr->def_charge = z;
        } else
            o_ptr->def_charge = 0;
        lua_pop(LUA, 1);   
        
        o_ptr->base_flyreps = integerlookup("flyreps", 1);
    }
    
    if (typenum == OBJTYPE_CLOUD) {
        o_ptr->sizetweak = integerlookup("sizetweak", 0);
        o_ptr->ytweak = integerlookup("y_tweak", 0);
    }
    
    /* AFTER THIS POINT, ESB DOES NOT IMPORT ANYTHING */
    if (!graphical)
        goto SKIP_LOAD_GRAPHICS;
   
    if (typenum == OBJTYPE_THING) {
        lua_pushstring(LUA, "methods");
        lua_gettable(LUA, -2);
        if (lua_istable(LUA, -1)) {
            o_ptr->method = import_attack_methods(nameref);
            lua_pop(LUA, 1);
        } else if (lua_isfunction(LUA, -1)) {
            o_ptr->method = NULL;
            o_ptr->arch_flags |= ARFLAG_METHOD_FUNC;
            o_ptr->lua_reg_key = luaL_ref(LUA, LUA_REGISTRYINDEX);
        } else
            lua_pop(LUA, 1);
    }
    
    if (o_ptr->rhack & RHACK_WRITING) {
    
        lua_pushstring(LUA, "font");
        lua_gettable(LUA, -2);
        gd.tmpstr = "font";
        o_ptr->w_font = luafont(LUA, -1, nameref, 0);
        lua_pop(LUA, 1);
    } else
        o_ptr->alt_icon = getluaoptbitmap(nameref, "alt_icon");
    
    if (typenum == OBJTYPE_DOOR) {
        o_ptr->dview[0] = getluaoptbitmap(nameref, "front");
        o_ptr->sideview[0] = getluaoptbitmap(nameref, "side");
        
        if (o_ptr->dview[0]) {
            o_ptr->inview[0] = getluaoptbitmap(nameref, "deco");
            o_ptr->outview[0] = getluaoptbitmap(nameref, "bash_mask");
        }  
    }
    
    if (typenum == OBJTYPE_HAZE || typenum == OBJTYPE_CLOUD) {
        o_ptr->dview[0] = getluaoptbitmap(nameref, "dungeon");
        o_ptr->inview[0] = getluaoptbitmap(nameref, "inside_weak");
        o_ptr->inview[1] = getluaoptbitmap(nameref, "inside_med");
        o_ptr->inview[2] = getluaoptbitmap(nameref, "inside_strong");
    }
    
    if (typenum == OBJTYPE_THING) {
        o_ptr->dview[0] = getluaoptbitmap(nameref, "dungeon");        
        if (o_ptr->dview[0]) {
            o_ptr->inview[0] = getluaoptbitmap(nameref, "flying_toward");
            o_ptr->outview[0] = getluaoptbitmap(nameref, "flying_away");
            o_ptr->sideview[0] = getluaoptbitmap(nameref, "flying_side");   
        }
    }
    
    if (typenum == OBJTYPE_WALLITEM) {
        o_ptr->dview[0] = getluaoptbitmap(nameref, "front");
        o_ptr->sideview[0] = getluaoptbitmap(nameref, "side");
        o_ptr->outview[0] = getluaoptbitmap(nameref, "other_side");
        
        if (o_ptr->outview[0] && !o_ptr->sideview[0])
            DSBLerror(LUA, "%s: Cannot have other_side without side", nameref);

        o_ptr->inview[0] = getluaoptbitmap(nameref, "near_side");
        o_ptr->inview[1] = getluaoptbitmap(nameref, "near_other_side");
            
        if (o_ptr->rhack & RHACK_MIRROR) {
            o_ptr->inview[2] = getluaoptbitmap(nameref, "mirror_inside");
            if (!o_ptr->inview[2]) {
                DSBLerror(LUA, "Mirror must have mirror_inside defined");
            }
        }
    }
    
    if (typenum == OBJTYPE_FLOORITEM || typenum == OBJTYPE_UPRIGHT) {
        o_ptr->dview[0] = getluaoptbitmap(nameref, "front");
        o_ptr->dview[1] = getluaoptbitmap(nameref, "front_med");
        o_ptr->dview[2] = getluaoptbitmap(nameref, "front_far");
        o_ptr->sideview[0] = getluaoptbitmap(nameref, "side");
        o_ptr->sideview[1] = getluaoptbitmap(nameref, "side_med");
        o_ptr->sideview[2] = getluaoptbitmap(nameref, "side_far");
        o_ptr->inview[0] = getluaoptbitmap(nameref, "same_square");
        o_ptr->inview[1] = getluaoptbitmap(nameref, "near_side");
        
        if (o_ptr->rhack & RHACK_STAIRS) {
            o_ptr->outview[0] = getluaoptbitmap(nameref, "xside");
            o_ptr->outview[1] = getluaoptbitmap(nameref, "xside_med");
            o_ptr->outview[2] = getluaoptbitmap(nameref, "xside_far");
        }
    }
    
    if (typenum == OBJTYPE_MONSTER) {
        o_ptr->dview[0] = getluaoptbitmap(nameref, "front");
        o_ptr->sideview[0] = getluaoptbitmap(nameref, "side");
        o_ptr->outview[0] = getluaoptbitmap(nameref, "back");
        o_ptr->extraview[0] = getluaoptbitmap(nameref, "other_side");
        
        if (o_ptr->extraview[0] && !o_ptr->sideview[0])
            DSBLerror(LUA, "%s: Cannot have other_side without side", nameref);
        
        import_monattack_views(nameref, &(o_ptr->iv));
        
        if (o_ptr->sideview[0] == o_ptr->dview[0] ||
            o_ptr->sideview[0] == o_ptr->outview[0])
        {
            o_ptr->side_flip_ok = 1;
        }
        
        o_ptr->grouptype = integerlookup("group_type", 0);  
    }
        
    SKIP_LOAD_GRAPHICS:
    
    // use lightuserdata so the lua side can't modify this
    lua_pushstring(LUA, "regobj");
    lua_pushlightuserdata(LUA, (void *)1);
    lua_settable(LUA, -3);
    
    lua_pushstring(LUA, "regnum");
    lua_pushlightuserdata(LUA, (void *)regnum);
    lua_settable(LUA, -3);
    
    // import a custom shading table
    if (graphical && tablecheckpresence("shading_info")) {
        luastacksize(4);
        lua_getglobal(LUA, "__import_arch_shading_info");
        lua_pushstring(LUA, o_ptr->luaname);
        lc_call_topstack(1, "shading_info");
        lstacktop();
    }
    
    dsbfree(typestr);
    lua_pop(LUA, 2);
    lstacktop();
    VOIDRETURN();
}

void depointerize_one(unsigned int inst, struct inst *p_i, 
    int lev, int xx, int yy, int dir, int expected_slave)
{    
    onstack("depointerize_one");
        
    if (LIKELY(lev >= 0)) {
        struct dtile *dt;
        struct inst_loc *c_dt_il;
        struct inst_loc *p_dt_il;
        
        dt = &(dun[lev].t[yy][xx]);
        
        c_dt_il = dt->il[dir];
        p_dt_il = NULL;
        
        while (c_dt_il != NULL) {            
            
            if (c_dt_il->i == inst) {
                
                if (c_dt_il->slave != expected_slave) {
                    char slaveerror[200];
                    sprintf(slaveerror, "Tile slave mismatch [is %d, wanted %d]", c_dt_il->slave, expected_slave);
                    coord_inst_r_error(slaveerror, c_dt_il->i, 255);
                    VOIDRETURN();    
                }
                               
                if (p_dt_il == NULL)
                    dt->il[dir] = c_dt_il->n;
                else
                    p_dt_il->n = c_dt_il->n;
                
                dsbfree(c_dt_il);
                VOIDRETURN();
                
            } else {
                p_dt_il = c_dt_il;
                c_dt_il = c_dt_il->n;
            }    
        }
        
        // this is all for error reporting only
        // a successful depointerize returned above
        {
            int plev, px, py, ptile;
            int f_plev, f_px, f_py, f_ptile;
            char errmsg[200];
            char err2[200];
            int pointerfound = 0;
            
            v_onstack("depointerize_one.error");
            
            for(plev=0;plev<gd.dungeon_levels;++plev) {
                for(py=0;py<dun[plev].ysiz;++py) {
                    for(px=0;px<dun[plev].xsiz;++px) {
                        for(ptile=0;ptile<4;++ptile) {
                            struct inst_loc *i_l = dun[plev].t[py][px].il[ptile];
                            while (i_l) {
                                if (i_l->i == inst) {
                                    if (i_l->slave == expected_slave) {
                                        pointerfound = 1;
                                        f_plev = plev;
                                        f_px = px;
                                        f_py = py;
                                        f_ptile = ptile;
                                    }
                                     
                                    fprintf(errorlog, "DEBUG: POINTER %d @ (%d, %d, %d, %d)[dir: %d, slave: %d]\n",
                                        inst, plev, px, py, ptile, oinst[inst]->facedir, i_l->slave);    
                                    
                                    //goto BREAK_FOUR_LOOPS;
                                }
                                i_l = i_l->n;
                            }
                        }
                    }
                }
            }
            BREAK_FOUR_LOOPS:
            
            if (pointerfound) {
                snprintf(err2, 200, "pointer @ (%d, %d, %d, %d)", f_plev, f_px, f_py, f_ptile);
            } else {
                sprintf(err2, "valid pointer nonexistent");
            }
            
            snprintf(errmsg, 200, "%s target %d not found at (%d, %d, %d, %d) [%s]",
                expected_slave ? "depointerize[slave]" : "depointerize",
                inst, lev, xx, yy, dir, err2);
            v_upstack();
            
            recover_error(errmsg);
        }
    }    
    
    VOIDRETURN();
}

void invalid_slave_pointer(int inst, int sf, const char *s_dirp) {
    char s_errstr[1000];
    struct inst *p_inst = oinst[inst];
    
    sprintf(s_errstr, "Invalid slave pointer to %s (inst %ld [%d, %d, %d, %d (%d)] - got %ld)",
       s_dirp,
       inst, p_inst->level, p_inst->x, p_inst->y, p_inst->tile, p_inst->facedir, sf);
    program_puke(s_errstr); 
}

void depointerize_slave(unsigned int inst, struct inst *p_i,
    int lev, int xx, int yy)
{
    struct obj_arch *p_a;
    
    onstack("depointerize_slave");
    
    // the editor doesn't need/care about slave pointers
    if (gd.edit_mode)
        VOIDRETURN();
        
    p_a = Arch(p_i->arch);
    
    if (p_i->tile != DIR_CENTER && p_a->rhack & RHACK_2WIDE) {
        int sf = get_slave_subtile(p_i);
        
        if (UNLIKELY(sf < 0)) {
            invalid_slave_pointer(inst, sf, "depointerize");
            VOIDRETURN();
        }
            
        depointerize_one(inst, p_i, lev, xx, yy, sf, 1);    
    }
     
    VOIDRETURN();    
}

void depointerize(unsigned int inst) {
    struct inst *p_i;
    int xx, yy, lev, dir;
    
    onstack("depointerize");
    
    p_i = oinst[inst];
    xx = p_i->x;
    yy = p_i->y;
    lev = p_i->level;
    dir = p_i->tile;
    
    if (UNLIKELY(lev < 0)) {
        program_puke("Depointerized limbo instance");
        VOIDRETURN();
    }
    
    depointerize_slave(inst, p_i, lev, xx, yy);
    depointerize_one(inst, p_i, lev, xx, yy, dir, 0);
    
    VOIDRETURN();
}

void pointerize_one(unsigned int inst, int lev,
    int xx, int yy, int dir, int slave) 
{
    struct dtile *dt;
    struct inst_loc *c_dt_il;
    struct inst_loc *n_dt_il;
    
    onstack("pointerize_one");
    
    dt = &(dun[lev].t[yy][xx]);
        
    n_dt_il = dsbcalloc(1, sizeof(struct inst_loc));
    c_dt_il = dt->il[dir];
        
    if (c_dt_il == NULL)
        dt->il[dir] = n_dt_il;
    else {
        while (c_dt_il->n != NULL)
            c_dt_il = c_dt_il->n;
            
        c_dt_il->n = n_dt_il;
    }
    
    n_dt_il->i = inst;
    n_dt_il->slave = slave;
    n_dt_il->n = NULL;
    
    VOIDRETURN();    
}

void pointerize_slave(unsigned int inst,
    int lev, int xx, int yy)
{
    struct obj_arch *p_a;
    struct inst *p_i;
    
    onstack("pointerize_slave");
    
    // the editor doesn't need/care about slave pointers
    if (gd.edit_mode)
        VOIDRETURN();
    
    p_i = oinst[inst];
    p_a = Arch(p_i->arch);
    
    if (p_i->tile != DIR_CENTER && p_a->rhack & RHACK_2WIDE) {
        int sf = get_slave_subtile(p_i);
        
        if (UNLIKELY(sf < 0)) {
            invalid_slave_pointer(inst, sf, "pointerize");
            VOIDRETURN();
        }

        pointerize_one(inst, lev, xx, yy, sf, 1);    
    }
     
    VOIDRETURN();
}

void pointerize(unsigned int inst) {
    struct inst *p_i;
    int xx, yy, lev, dir;
    
    onstack("pointerize");
    
    p_i = oinst[inst];
    xx = p_i->x;
    yy = p_i->y;
    lev = p_i->level;
    dir = p_i->tile;
        
    pointerize_one(inst, lev, xx, yy, dir, 0);
    pointerize_slave(inst, lev, xx, yy);
    
    VOIDRETURN();
}

void place_instance(unsigned int inst, int lev, int xx, int yy, int dir, int pt) {
    int newspawn = 0;
    int put = 0;
    int sb_queued = 0;
    
    onstack("place_instance");
    
    // i have no idea what this originally was supposed to be
    if (pt & 0x100)
        newspawn = 1;       
    pt &= 0xFF;
        
    if (gd.depointerized_inst) {
        if (LIKELY(gd.depointerized_inst == inst))
            gd.depointerized_inst = 0;
        else {
            coord_inst_r_error("Invalid depointerized_inst", inst, dir);
            VOIDRETURN();
        }
    
    } else {
        if (UNLIKELY(!in_limbo(inst))) {
            coord_inst_r_error("Nonlimbo place_instance on", inst, dir);
            VOIDRETURN();
        }
    }
    
    if (lev >= 0) {
        struct inst *ptr_inst;
        struct obj_arch *ptr_arch;
        int old_lev = oinst[inst]->level;
        
        obj_at(inst, lev, xx, yy, dir); 
        put = 1;    
        
        ptr_inst = oinst[inst];
        ptr_arch = Arch(ptr_inst->arch);
        
        if (ptr_arch->type == OBJTYPE_MONSTER) {
            if (ptr_arch->msize == 2 && dir != DIR_CENTER) {
                int i_sdir;
                i_sdir = check_slave_subtile(ptr_inst->facedir, dir);
                if (i_sdir < 0)
                    ptr_inst->facedir = dir;
            }
        }
        
        ptr_inst->gfxflags &= ~(G_UNMOVED);
        pointerize_one(inst, lev, xx, yy, dir, 0);
        pointerize_slave(inst, lev, xx, yy);
        process_queued_after_from(inst);
        
        if (ptr_arch->type == OBJTYPE_MONSTER) {
            if (old_lev != lev) {
                lc_parm_int("sys_monster_enter_level", 2, inst, lev);
            }
            if (!oinst[inst]) {
                VOIDRETURN();
            }
        }
        
        if (!pt) {
            if (newspawn && (gd.engine_flags & ENFLAG_SPAWNBURST))
                sb_to_queue(inst, lev);
            else {
                // "openshot" objects do not interact with the environment
                if (!ptr_inst->openshot) {    
                    to_tile(lev, xx, yy, dir, inst, T_TO_TILE);
                }
            }
            
            // even if we're not in sb mode, the check has been made
            sb_queued = 1;
        }
    } else {
        struct inst *ptr_inst;
        ptr_inst = oinst[inst];
            
        if (ptr_inst->flytimer) {
            drop_flying_inst(inst, ptr_inst);
            destroy_assoc_timer(inst, EVENT_MOVE_FLYER, -1); 
        }
    }
    
    if (lev == LOC_MOUSE_HAND) {
        if (gd.mouseobj)
            mouse_obj_drop(gd.mouseobj);
            
        mouse_obj_grab(inst);
        check_sb_to_queue(inst, LOC_MOUSE_HAND);
        sb_queued = 1;
    }
    
    if (lev == LOC_CHARACTER) {
        const char *invfunc = Sys_Inventory;

        if (UNLIKELY(xx < 1)) {
            puke_bad_subscript("gd.champs", xx-1);
            VOIDRETURN();
        }
        
        gd.queue_inv_rc++;
        gd.lua_bool_hack = 1;
        lc_parm_int(invfunc, 6, xx, yy, -1, inst, LHFALSE, LHTRUE);
        
        gd.champs[xx-1].inv[yy] = inst;
        gd.champs[xx-1].inv_queue_data[yy] = 0;
        obj_at(inst, LOC_CHARACTER, xx, yy, 0);
        gd.champs[xx-1].load += ObjMass(inst);
        oinst[inst]->gfxflags &= ~(G_UNMOVED);
        put = 1;
               
        // after_* has to run after!
        // so flush the queue once first
        gd.queue_inv_rc--;
        if (!gd.queue_inv_rc)
            flush_inv_instmd_queue();
                    
        // try this again
        gd.queue_inv_rc++;
        process_queued_after_from(inst);
        gd.lua_bool_hack = 1;
        lc_parm_int(invfunc, 6, xx, yy, -1, inst, LHTRUE, LHTRUE);   
        gd.queue_inv_rc--;
        if (!gd.queue_inv_rc)
            flush_inv_instmd_queue();   
        
        // compute load at the very end  
        calculate_maxload(xx);
        
        check_sb_to_queue(inst, LOC_CHARACTER);
        sb_queued = 1;
    }
    
    if (lev == LOC_IN_OBJ) {
        inst_putinside(xx, inst, yy);     
        put = 1;
        
        check_sb_to_queue(inst, LOC_IN_OBJ);
        sb_queued = 1;
    }
    
    // fallthrough for any < 0 levels without special handlers
    if (!sb_queued) {
        check_sb_to_queue(inst, LOC_LIMBO);
    }
    
    if (!put) {
        obj_at(inst, lev, xx, yy, dir);
        process_queued_after_from(inst);
    }
    
    VOIDRETURN();   
}

void resize_inside(struct inst *in_inst) {
    int n;
    int max_i = -1;
    
    onstack("resize_inside");
    
    for (n=0;n<in_inst->inside_n;++n) {
        if (in_inst->inside[n])
            max_i = n;
    }
    
    ++max_i;
    
    if (max_i == 0) {
        dsbfree(in_inst->inside);
        in_inst->inside = NULL;
    } else 
        in_inst->inside = dsbrealloc(in_inst->inside, sizeof(unsigned int)*max_i);
        
    in_inst->inside_n = max_i;
    
    VOIDRETURN();    
}

void limbo_instance(unsigned int inst) {
    struct inst *p_inst;
    const char *invfunc = Sys_Inventory;
    
    onstack("limbo_instance");
    
    p_inst = oinst[inst];
    
    if (p_inst->level >= 0) {
        p_inst->gfxflags &= ~(G_UNMOVED);
        depointerize(inst);
        i_to_tile(inst, T_FROM_TILE);
    } else if (p_inst->level == LOC_CHARACTER) {
        int pix = p_inst->x;
        
        if (UNLIKELY(pix < 1)) {
            puke_bad_subscript("gd.champs", pix-1);
            VOIDRETURN();
        }
        
        gd.queue_inv_rc++;
        
        gd.lua_bool_hack = 1;
        lc_parm_int(invfunc, 6, pix, p_inst->y, inst, -1, LHFALSE, LHTRUE);
        
        gd.champs[pix-1].inv[p_inst->y] = 0;
        gd.champs[pix-1].inv_queue_data[p_inst->y] = 0;
        gd.champs[pix-1].load -= ObjMass(inst);
        p_inst->gfxflags &= ~(G_UNMOVED);

        if (p_inst->y == gd.method_obj) {
            clear_method();
        }
        
        calculate_maxload(pix);
        
        gd.queue_inv_rc--;
        if (!gd.queue_inv_rc)
            flush_inv_instmd_queue();
            
        // if we have an after_from_*, the expected response is
        // probably going to be to process it whenever the object
        // gets where it's going. unfortunately, in here,
        // we don't know when that's actually going to happen. 
        // queue it up and hope for the best!
        if (!p_inst->after_from_q) {
            p_inst->after_from_q = dsbmalloc(sizeof(struct obj_after_from_q));
            p_inst->after_from_q->lev_parm = pix;
            p_inst->after_from_q->x_parm = p_inst->y;
            p_inst->after_from_q->y_parm = inst;
            p_inst->after_from_q->dir_parm = -1;
            p_inst->after_from_q->lh1_parm = LHTRUE;
            p_inst->after_from_q->lh2_parm = LHTRUE;
        }
                
    } else if (p_inst->level == LOC_IN_OBJ) {
        struct inst *p_c_inst = oinst[p_inst->x];
        
        p_inst->gfxflags &= ~(G_UNMOVED);
        p_c_inst->inside[p_inst->y] = 0;
        
        if (!gd.noresizehack)
            resize_inside(p_c_inst);

    } else if (p_inst->level = LOC_MOUSE_HAND) {
        mouse_obj_drop(inst);
    }
    
    obj_at(inst, LOC_LIMBO, 0, 0, 0);
    
    VOIDRETURN();
}

void r_inst_putinside(unsigned int inwhat, unsigned int put_in, int z, int rc) {
    struct inst *p_c_inst;
    unsigned int clobberobj = 0;
    
    if (rc > 32) {
        recursion_puke();
        return;
    }
     
    p_c_inst = oinst[inwhat];
    
    // find a slot that fits
    if (z < 0) {
        z = 0;
        while (z < p_c_inst->inside_n) {
            if (!p_c_inst->inside[z]) break;
            ++z;
        }
    }
    
    if (z >= p_c_inst->inside_n) {
        int sizdif = ((z+1) - p_c_inst->inside_n);
            
        p_c_inst->inside = dsbrealloc(p_c_inst->inside, sizeof(unsigned int) * (z+1));
        memset(p_c_inst->inside + p_c_inst->inside_n, 0, sizeof(unsigned int) * sizdif);
        p_c_inst->inside_n = (z+1);   
    }
    
    if (p_c_inst->inside[z])
        clobberobj = p_c_inst->inside[z];
        
    p_c_inst->inside[z] = put_in;
    obj_at(put_in, LOC_IN_OBJ, inwhat, z, 0);
    
    // this could be ugly
    if (clobberobj) {
        r_inst_putinside(inwhat, clobberobj, (z+1), (rc+1));
    }
}

void inst_putinside(unsigned int inwhat, unsigned int put_in, int z) {
    onstack("inst_putinside");
    r_inst_putinside(inwhat, put_in, z, 0);
    process_queued_after_from(put_in);
    VOIDRETURN();
}

void destroy_lua_exvars(char *exvi, unsigned int inst) {
    onstack("destroy_lua_exvars");
    
    lua_getglobal(LUA, exvi);
    lua_pushinteger(LUA, inst);
    lua_pushnil(LUA);
    lua_settable(LUA, -3);
    lua_pop(LUA, 1);
    
    VOIDRETURN();    
}

void destroy_inst_loc_list(struct inst_loc *il, int unmark_uplinks, int destroy_insts) {
    onstack("destroy_inst_loc_list");
    
    while (il != NULL) {
        struct inst_loc *il_n = il->n;
        
        if (unmark_uplinks && oinst[il->i]) {
            oinst[il->i]->uplink = 0;
        }
        
        // used only by the editor
        if (destroy_insts) {
            inst_destroy(il->i, 1);
        }
        
        dsbfree(il);
        il = il_n;
    }
    
    VOIDRETURN();    
}

void purge_inside(int n, unsigned int *in) {
    int i;
    
    onstack("purge_inside");
    
    gd.noresizehack = 1;
    for(i=0;i<n;++i) {
        if (in[i]) {
            limbo_instance(in[i]);
        }
    }
    
    dsbfree(in);
    gd.noresizehack = 0;
    
    VOIDRETURN();    
}

// d_i_p = delete in place level
// -- 0 do everything; normal delete
// -- >= 1 don't depointerize pointerized insts
// -- 2 don't bother clearing out containers
//    (i.e., i'm just destroying all insts sequentually) 
// -- 4 don't free my table identifier (used by regular_swap)
void inst_destroy(unsigned int inst, int d_i_p) {
    struct inst *p_inst;
    struct obj_arch *p_arch;
    int otype;
    
    onstack("inst_destroy");
    
    p_inst = oinst[inst];
    p_arch = Arch(p_inst->arch);
    otype = p_arch->type;
    
    if (otype == OBJTYPE_MONSTER)
        destroyed_monster(p_inst);
        
    if (otype == OBJTYPE_THING) {
        if (p_inst->flytimer) {
            destroy_assoc_timer(inst, EVENT_MOVE_FLYER, -1);     
        }
    }
    
    // prevents messages from being misdirected
    destroy_assoc_timer(inst, EVENT_SEND_MESSAGE, -2);
    
    if (!d_i_p && p_inst->level != LOC_LIMBO)
        limbo_instance(inst);

    destroy_lua_exvars("exvar", inst);
    
    // queued after_from? handle it... or just get rid of it.
    if (!d_i_p) {
        process_queued_after_from(inst);
    } else {
        dsbfree(p_inst->after_from_q);
        p_inst->after_from_q = NULL;
    }
    
    if (p_inst->inside_n) {
        if (d_i_p == 2)
            dsbfree(p_inst->inside);
        else
            purge_inside(p_inst->inside_n, p_inst->inside);
    }
    
    // destroys the list, but not the associated objects
    // flushouts don't bother to tell the uplink (it'll get
    // deleted too anyway, everything else voids it
    destroy_inst_loc_list(p_inst->chaintarg, (d_i_p != 2), 0);
    
    // same thing. only tell the uplink on a non-clearout
    if (p_inst->uplink && (d_i_p != 2)) {
        msg_chain_delink(p_inst->uplink, inst);
    }
    
    if (gd.who_method && gd.method_obj == inst) {
        clear_method();
    }
    
    if (d_i_p != 4)
        dsbfree(p_inst);
        
    inst_dirty[inst] = 1;
    oinst[inst] = NULL;
    //fix_inst_int();
    lstacktop();
    
    VOIDRETURN();    
}

void inst_add_to_table(unsigned int inst,
    unsigned int objarch, int lev, int tdir)
{
    struct inst *o_i;
    struct animap *amap;
    struct obj_arch *p_arch;
    int otype;
    int cf;
    
    onstack("inst_add_to_table");

    o_i = oinst[inst];
    o_i->arch = objarch;
    p_arch = Arch(objarch);
    otype = p_arch->type;
    
    o_i->level = LOC_LIMBO;
    o_i->charge = p_arch->def_charge;
    o_i->CHECKVALUE = DSB_CHECKVALUE;
    
    amap = p_arch->dview[0];
    o_i->frame = 0;
        
    if (otype == OBJTYPE_THING)
        tweak_thing(o_i);
        
    if (otype == OBJTYPE_MONSTER) {
        spawned_monster(inst, o_i, lev);
        
        // normalize direction
        if (lev >= 0 && p_arch->msize == 2) {
            if (tdir != DIR_CENTER) {
                int i_check = check_slave_subtile(tdir, o_i->tile);
                if (i_check < 0) {
                    o_i->facedir = tdir;
                }
            }
        }
    }
    
    if (!gd.edit_mode) {
        call_member_func(inst, "on_init", lev);
    }
             
    VOIDRETURN();
}

void regular_swap(unsigned int inst, unsigned int objarch) {
    struct inst *p_inst;
    struct inst *p_old_inst;
    int lev, xx, yy, tdir;
    
    onstack("regular_swap");
    
    p_inst = oinst[inst];    
    lev = p_inst->level;
    xx = p_inst->x;
    yy = p_inst->y;
    tdir = p_inst->tile;
    
    p_old_inst = p_inst;
    inst_destroy(inst, 4);
    
    init_inst(inst);
    inst_add_to_table(inst, objarch, lev, tdir);
    
    dsbfree(p_old_inst);
    
    p_inst = oinst[inst];
    p_inst->level = lev;
    p_inst->x = xx;
    p_inst->y = yy;
    p_inst->tile = tdir;

    if (p_inst->level == LOC_CHARACTER) {
        int px = p_inst->x-1;
        gd.champs[px].load = sum_load(px);
        determine_load_color(px);
    } else if (p_inst->level == LOC_MOUSE_HAND) {
        int lc = gd.party[gd.leader] - 1;
        gd.champs[lc].load = sum_load(lc);
        determine_load_color(lc);
    }
    
    VOIDRETURN();
}

unsigned int create_instance(unsigned int objarch, 
    int lev, int xx, int yy, int dir, int b_force_instant)
{ 
    unsigned int inst;
    
    onstack("create_instance");

    if (gd.force_id) {
        if (UNLIKELY(oinst[gd.force_id])) {
            struct inst *p_p_inst = oinst[gd.force_id];
            struct obj_arch *ex_arch = Arch(p_p_inst->arch);
            struct obj_arch *new_arch = Arch(objarch);
            char buf[400];
            snprintf(buf, sizeof(buf),
                "Inst %d already allocated\n\nNew: %s[%x] @ (%d, %d, %d, %d)\nPrev: %s[%x] @ (%d, %d, %d, %d)\n\n"
                    "This is likely due to a bug in whatever editor/automatic generation tool you used.",
                    gd.force_id,
                    ex_arch->luaname, ex_arch, lev, xx, yy, dir,
                    new_arch->luaname, new_arch, p_p_inst->level, p_p_inst->x, p_p_inst->y, p_p_inst->tile);
            
            poop_out(buf);
            VOIDRETURN();
        }

        inst = gd.force_id;
        init_inst(inst);
        
    } else
        inst = next_object_inst(objarch, gd.last_alloc);
        
    gd.force_id = 0;
    inst_add_to_table(inst, objarch, lev, dir);
    if (!gd.edit_mode) {
        if (debug) {
            fprintf(errorlog, "on_spawn[%ld][%x] at %d %d %d %d\n",
                inst, objarch, lev, xx, yy, dir);
        }
        
        if (!(gd.engine_flags & ENFLAG_SPAWNBURST)) {
            call_member_func4(inst, "on_spawn", lev, xx, yy, dir);
        }
    }
    
    if (oinst[inst]) {
        if (!b_force_instant && gd.queue_rc && lev >= 0) {
            instmd_queue(INSTMD_INST_MOVE, lev, xx, yy, dir, inst, 0x100);
        } else
            place_instance(inst, lev, xx, yy, dir, 0x100);
    }
    RETURN(inst);
}

void inst_loc_to_luatable(lua_State *LUA, struct inst_loc *tteam, int frozenalso, int *adjp) {
    int idx = 1;
    
    onstack("inst_loc_to_luatable");
    
    luastacksize(4);
    
    if (tteam == NULL) {
        if (adjp)
            *adjp = 0;
            
        lua_pushnil(LUA);
        VOIDRETURN();
    }

    lua_newtable(LUA);
    while (tteam != NULL) {
        struct inst *p_inst = oinst[tteam->i];
 
        if (p_inst) {  
            if (frozenalso || !(p_inst->gfxflags & G_FREEZE)) { 
                lua_pushinteger(LUA, idx++);
        
                lua_newtable(LUA);
        
                lua_pushstring(LUA, "id");
                lua_pushinteger(LUA, tteam->i);
                lua_settable(LUA, -3);
        
                lua_pushstring(LUA, "tile");
                lua_pushinteger(LUA, p_inst->tile);
                lua_settable(LUA, -3);
        
                // set the new structure into the table
                lua_settable(LUA, -3);
            }
        }

        tteam = tteam->n;
    }
    
    // nothing got pushed
    if (idx == 1) {
        lua_pop(LUA, 1);
        lua_pushnil(LUA);
    }
    
    if (adjp) {
        *adjp = (idx - 1);
    }
    
    VOIDRETURN();
}

int get_slave_subtile(struct inst *p_inst) {
    int ccw;

    onstack("get_slave_subtile");

    if (UNLIKELY(p_inst->tile == DIR_CENTER)) {
        recover_error("Req slave of CENTER");
        RETURN(-1);
    }

    if (p_inst->facedir == p_inst->tile)
        ccw = 0;
    else if (p_inst->facedir == (p_inst->tile+3)%4)
        ccw = 1;
    else if (p_inst->facedir < 0) {
        RETURN(-3);
    } else {
        RETURN(-2);
    }

    if (ccw) {
        RETURN((p_inst->tile+1)%4);
    } else {
        RETURN((p_inst->tile+3)%4);
    }
}

void msg_chain_to(unsigned int base_id, unsigned int fwd_to, unsigned int chain_reps) {
    struct inst *p_inst;
    struct inst_loc *nct;
    
    onstack("msg_chain_to");
    
    p_inst = oinst[base_id];
    
    nct = dsbcalloc(1, sizeof(struct inst_loc));
    nct->i = fwd_to;
    nct->chain_reps = chain_reps;
    nct->n = p_inst->chaintarg;
    p_inst->chaintarg = nct;

    VOIDRETURN();
}

void msg_chain_delink(unsigned int base_id, unsigned int fwd_to) {
    struct inst *p_inst;
    
    onstack("msg_chain_delink");
    
    p_inst = oinst[base_id];
    if (p_inst && p_inst->chaintarg) {
        struct inst_loc *ct = p_inst->chaintarg;
        while (ct) {
            if (ct->i == fwd_to) {
                ct->delink = 1;
            }
            ct = ct->n;
        }        
    }
    
    recover_error("Delink command from non-chained inst");
    VOIDRETURN();
}

// index is based on INTERNAL values
// -- NOT USED ANYMORE. THIS IS ALL LUA-BASED NOW.
void determine_load_color(int who) {
    struct champion *me;
    
    return;
}
    /*
    onstack("determine_load_color");
    
    lua_getglobal(LUA, "sys_calc_loadcolor");
    if (!lua_isfunction(LUA, -1)) {
        lua_pop(LUA, 1);
        VOIDRETURN();
    }
    
    me = &(gd.champs[who]);
    lua_pushinteger(LUA, me->load);
    lua_pushinteger(LUA, me->maxload);
    
    if (lua_pcall(LUA, 2, 1, 0) != 0) {
        lua_function_error("sys_calc_loadcolor", lua_tostring(LUA, -1));
    }
    
    if (lua_isnil(LUA, -1)) {
        me->custom_load_color_value = 0;
        me->custom_v &= ~(CUSV_LOAD_COLOR);
    } else {
        int rgbv;        
        int nf = gd.lua_nonfatal; 
        gd.lua_nonfatal = 1;
        rgbv = luargbval(LUA, -1, "sys_calc_loadcolor return", 1);
        nf = gd.lua_nonfatal;
        
        me->custom_load_color_value = rgbv;
        me->custom_v |= CUSV_LOAD_COLOR;        
    }
    
    lua_pop(LUA, 1);
    
    VOIDRETURN();  
}*/

void tile_actuator_rotate(int lev, int x, int y, int pos, unsigned int want_inst) {
    struct dungeon_level *dd;
    struct inst_loc *i_l = NULL;
    struct inst_loc *tile_l = NULL;
    struct inst_loc *last_valid = NULL;
    int wanttype = OBJTYPE_WALLITEM;
    int alttype = OBJTYPE_UPRIGHT;
    
    onstack("tile_actuator_rotate");

    dd = &(dun[lev]);
    
    if (pos != DIR_CENTER)
        wanttype = OBJTYPE_WALLITEM;
    else
        wanttype = OBJTYPE_FLOORITEM;
    
    tile_l = dd->t[y][x].il[pos];
    i_l = tile_l;
    while (i_l) {
        int id = i_l->i;
        struct obj_arch *arch = Arch(oinst[id]->arch);
        if (arch->type == wanttype || arch->type == alttype) {
            if (want_inst == 0 || want_inst == id) {
                last_valid = i_l;
            }
        }
        i_l = i_l->n;
    }
    
    if (last_valid) {
        struct inst_loc *old_lvn = NULL;
        struct inst_loc *append_to;
        
        append_to = last_valid;
        old_lvn = last_valid->n;
        i_l = tile_l;
        while (i_l) {
            if (i_l != last_valid) {
                append_to->n = i_l;
                append_to = append_to->n;
                i_l = i_l->n;
            } else {
                i_l = old_lvn;
            }
        } 
        
        dd->t[y][x].il[pos] = last_valid;
        append_to->n = NULL;
    }
    
    VOIDRETURN(); 
}

void process_queued_after_from(unsigned int inst) {
    struct inst *p_inst;
    struct obj_after_from_q *ofq;
    const char *invfunc = Sys_Inventory;
    
    if (!oinst[inst]) return;
    onstack("process_queued_after_from");
    p_inst = oinst[inst];
    if (!p_inst->after_from_q) { VOIDRETURN(); }
    
    ofq = p_inst->after_from_q;

    gd.queue_inv_rc++;
    gd.lua_bool_hack = 1;
    lc_parm_int(invfunc, 6, ofq->lev_parm,
        ofq->x_parm, ofq->y_parm, ofq->dir_parm, ofq->lh1_parm, ofq->lh2_parm);
    gd.queue_inv_rc--;
    if (!gd.queue_inv_rc)
        flush_inv_instmd_queue();
    
    dsbfree(p_inst->after_from_q);
    p_inst->after_from_q = NULL;
    VOIDRETURN();   
}

