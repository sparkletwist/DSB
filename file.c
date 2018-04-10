#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <fmod.h>
#include "versioninfo.h"
#include "filedefs.h"
#include "objects.h"
#include "champions.h"
#include "lua_objects.h"
#include "global_data.h"
#include "console.h"
#include "uproto.h"
#include "gproto.h"
#include "monster.h"
#include "sound_fmod.h"
#include "timer_events.h"
#include "arch.h"
#include "gamelock.h"
#include "mparty.h"

#define MINVER_HAVE_TINTSAVED       38
#define MINVER_LONG_CHARGE          41
#define MINVER_SANE_ANIMATION       53
#define MINVER_IND_DELAY            54
#define MINVER_COND_ANIMTIMER       55
#define MINVER_INST_PREV            59
#define MINVER_CHAIN_REPS           60

const char *CSSAVEFILE[] = { "DSBSAVE1.DSB",
    "DSBSAVE2.DSB", "DSBSAVE3.DSB", "DSBSAVE4.DSB" };

extern struct global_data gd;
extern struct graphics_control gfxctl;
extern struct inventory_info gii;
//extern struct translate_table Translate;

extern lua_State *LUA;
extern FILE *errorlog;
extern struct timer_event *te;
extern struct timer_event *a_te;
extern struct timer_event *new_te;
extern struct timer_event *eot_te;
extern struct inst *oinst[NUM_INST];
extern struct dungeon_level *dun;
extern struct exports *ex_l;
extern int debug;

extern struct c_line cons[16];

#define W_CHARS(A, B, C, D) pack_putc(A, pf); \
                            pack_putc(B, pf); \
                            pack_putc(C, pf); \
                            pack_putc(D, pf);

void storestring(PACKFILE *pf, const char *cstr) {
    int sc = 0;
    
    if (cstr == NULL) {
        pack_putc(0, pf);
        return;
    }
    
    while (*cstr != '\0') {
        pack_putc(*cstr, pf);
        ++cstr;
        ++sc;
    }
    pack_putc(0, pf);
}

char *retrstring(PACKFILE *pf) {
    char kk[1024];
    char t = 255;
    int d = 0;
    
    while (t != 0) {
        t = pack_getc(pf);
        kk[d++] = t;
        if (d == 1023) {
            kk[1023] = '\0';
            fprintf(errorlog, "Buffer overflow\nDump:\n%s\n", kk);
            program_puke("Stringbuffer overflow");
        }
    }
    return (dsbstrdup(kk));
}

int arch_from_string(const char *archstr) {
    int gotarch = 0;
    
    onstack("arch_from_string");
    
    luastacksize(8);
    lua_getglobal(LUA, "obj");
    lua_pushstring(LUA, archstr);
    lua_gettable(LUA, -2);
    if (lua_istable(LUA, -1)) {
        lua_pushstring(LUA, "regnum");
        lua_gettable(LUA, -2);
        gotarch = (int)lua_touserdata(LUA, -1);
        lua_pop(LUA, 1);    
    } else {
        char errmsg[128];
        snprintf(errmsg, 127, "Savefile contains unknown arch [%s]", archstr);
        poop_out(errmsg);
    }
    lua_pop(LUA, 2);
    
    RETURN(gotarch);    
}

void read_console_text(PACKFILE *pf, int nl) {
    int i;

    for (i=0;i<nl;i++) {
        int cval;
        rdl(cval);
        if (cval != -2) {
            cons[i].ttl = cval;
            rdl(cons[i].linecol);
            cons[i].txt = retrstring(pf);
        }
    }
}

void write_console_text(PACKFILE *pf, int nl) {
    int i;
    
    for (i=0;i<nl;i++) {
        if (cons[i].txt) {
            wrl(cons[i].ttl);
            wrl(cons[i].linecol);
            storestring(pf, cons[i].txt);
        } else
            wrl(-2);
    }

}

struct timer_event *read_timerdata(PACKFILE *pf) {
    struct timer_event *bte = NULL;
    struct timer_event *cte = NULL;
    unsigned char cx;
    
    onstack("read_timerdata");
    
    rdc(cx);
    if (cx != 255) {
        
        while (1) {
            unsigned char nx;

            if (!bte)
                bte = cte = dsbcalloc(1, sizeof(struct timer_event));
            else {
                cte->n = dsbcalloc(1, sizeof(struct timer_event));
                cte = cte->n;
            }

            rdl(cte->type);
            rdl(cte->inst);
            rdl(cte->time_until);
            rdl(cte->maxtime);
            rdl(cte->data);
            rdl(cte->altdata);
            rdl(cte->sender);

            // DO_FUNCTION timers are saved with their useless data!
            // set delme on load to work around it
            if (cte->type == EVENT_DO_FUNCTION)
                cte->delme = 1;
            else if (cte->type == EVENT_MONSTER) {
                // restore unsaved controller
                struct inst *p_m_inst = oinst[cte->inst];
                p_m_inst->ai->controller = cte;
            }

            rdc(nx);

            if (nx == 255)
                break;
        }
    }
    
    RETURN(bte);
}

void write_timerdata(PACKFILE *pf, struct timer_event *cte) {
    if (UNLIKELY(cte == NULL)) {
        wrc(255);
    } else {
        cleanup_timers(&te);
        wrc(0);
        while (cte != NULL) {
            wrl(cte->type);
            wrl(cte->inst);
            wrl(cte->time_until);
            wrl(cte->maxtime);
            wrl(cte->data);
            wrl(cte->altdata);
            wrl(cte->sender);

            if (!(cte->n)) {
                break;
            } else {
                wrc(0);
                cte = cte->n;
            }
        }
        
        wrc(255);
    }
}

void write_lua_data(PACKFILE *pf, int rc) {
    const int stackarg = -1;

    if (rc > 8) {
        onstack("write_lua_data");
        recursion_puke();
        VOIDRETURN();
    }
 
    luastacksize(rc+2);

    if (lua_isboolean(LUA, stackarg)) {
        unsigned char bt = lua_toboolean(LUA, stackarg);
        wrc(D_ISBOOL);
        wrc(bt);
    } else if (lua_isnumber(LUA, stackarg)) {
        int r_i = lua_tointeger(LUA, stackarg);
        wrc(D_ISINT);
        wrl(r_i);
    } else if (lua_isstring(LUA, stackarg)) {
        const char *r_s = lua_tostring(LUA, stackarg);
        wrc(D_ISSTR);
        storestring(pf, r_s);
    } else if (lua_istable(LUA, stackarg)) {
        int i;
        int num_e;
        
        // keeps the stack the way it was
        luastacksize(8);

        lua_getglobal(LUA, "__table_dump");
        lua_pushvalue(LUA, -2);
        lc_call_topstack(1, "storetablevars");

        lua_getglobal(LUA, "_TXVT");

        lua_pushstring(LUA, "_ELE");
        lua_gettable(LUA, -2);
        num_e = lua_tointeger(LUA, -1);
        lua_pop(LUA, 1);
        
        if (!num_e) {
            lua_pop(LUA, 1); // pop _TXVT
            wrc(D_INVALID);
            return;
        }
        
        wrc(D_ISSTBL);

        for(i=0;i<num_e;++i) {

            lua_pushinteger(LUA, i+1);
            lua_gettable(LUA, -2);
            
            if (lua_isnumber(LUA, -1)) {
                int i;
                wrc(D_ISINT);
                i = lua_tointeger(LUA, -1);
                wrl(i);
            } else {
                const char *sxvr;
                sxvr = lua_tostring(LUA, -1);
                wrc(D_ISSTR);
                storestring(pf, sxvr);
            }

            lua_pushvalue(LUA, -3); // copy of initial table
            lua_pushvalue(LUA, -2); // copy of table key
            lua_gettable(LUA, -2);
            
            write_lua_data(pf, rc+1);
            lua_pop(LUA, 3);
        }
        
        wrc(D_ENDSTBL);
        lua_pop(LUA, 1); // pop _TXVT
        
    } else {
        wrc(D_INVALID);
    }
}

int push_lua_filedata(PACKFILE *pf, int rc) {
    static int MAX_RECUR = 8;
    unsigned char ex_t;
    
    if (rc > MAX_RECUR) {
        onstack("push_lua_filedata");
        recursion_puke();
        VOIDRETURN();
    }
    
    luastacksize(rc+2);
    
    rdc(ex_t);
    
    if (ex_t == D_ISINT) {
        int r_i;
        rdl(r_i);
        lua_pushinteger(LUA, r_i);
        
    } else if (ex_t == D_ISSTR) {
        char *s_i;
        s_i = retrstring(pf);
        lua_pushstring(LUA, s_i);
        dsbfree(s_i);
    } else if (ex_t == D_ISBOOL) {
        int bx;
        rdc(bx);
        lua_pushboolean(LUA, bx);
    } else if (ex_t == D_ISSTBL) {
        int tc = 1;
        
        lua_newtable(LUA);
        while (tc < 1000) {
            int r;
            
            r = push_lua_filedata(pf, MAX_RECUR);
            if (r) {
                lua_pop(LUA, 1);
                return 0;
            }

            push_lua_filedata(pf, rc+1);
            lua_settable(LUA, -3);
            ++tc;
        }
        
    } else if (ex_t == D_ENDSTBL) {
        lua_pushnil(LUA);
        return 1;
    } else {
        lua_pushnil(LUA);
    }
    
    return 0;
}

void storeexvars(PACKFILE *pf, const char *exvi, int idx) {
    int i;
    int num_e;
    struct timer_event *cte = te;
    
    luastacksize(16);
    
    lua_getglobal(LUA, "__exvar_dump");
    lua_getglobal(LUA, exvi);
    lua_pushinteger(LUA, idx);
    lc_call_topstack(2, "storeexvars");
    
    lua_getglobal(LUA, "_EXVT");
    lua_pushstring(LUA, "_ELE");
    lua_gettable(LUA, -2);
    num_e = lua_tointeger(LUA, -1);
    lua_pop(LUA, 1);
    
    wrl(num_e);
    if (!num_e)
        goto STORE_FINISHED;
    
    for(i=0;i<num_e;++i) {
        const char *sxvr;

        lua_pushinteger(LUA, i+1);
        lua_gettable(LUA, -2);
        sxvr = lua_tostring(LUA, -1);
        storestring(pf, sxvr);      
        lua_getglobal(LUA, exvi);
        lua_pushinteger(LUA, idx);
        lua_gettable(LUA, -2);
        lua_pushstring(LUA, sxvr);
        lua_gettable(LUA, -2);
        write_lua_data(pf, 0);
        lua_pop(LUA, 4);
    }
    
    STORE_FINISHED:
    lua_pop(LUA, 1);
    
    lstacktop();
}

void retrexvars(PACKFILE *pf, const char *exvi, int idx) {
    int exnum;
    int i;
    
    luastacksize(6);
    
    rdl(exnum);
    if (!exnum) return;
    
    lua_getglobal(LUA, exvi);
    lua_pushinteger(LUA, idx);
    lua_newtable(LUA);
    
    for(i=0;i<exnum;++i) {
        char *exvar_name;
        
        exvar_name = retrstring(pf);
        
        lua_pushstring(LUA, exvar_name);
        push_lua_filedata(pf, 0);
        lua_settable(LUA, -3);
        dsbfree(exvar_name);
    }
    
    lua_settable(LUA, -3);
    lua_pop(LUA, 1);
    
    return;    
}

void store_illist(PACKFILE *pf, struct inst_loc *dt_il) {
    onstack("store_illist");
    
    while (1) {
        wrl(dt_il->i);
        wrc(dt_il->slave);
        wrc(dt_il->chain_reps);
        if (dt_il->n != NULL) {
            wrc(20);
        } else {
            wrc(255);
            break;
        }
            
        dt_il = dt_il->n;
    }
    
    VOIDRETURN();
}

struct inst_loc *retr_illist(PACKFILE *pf, int filever) {
    struct inst_loc *il = NULL;
    struct inst_loc *c_il = NULL;
    int idxnum;
    char slave_val;
    unsigned char ttl_val;
    unsigned char chain_reps;
    unsigned char is_next = 20;
    
    onstack("retr_illist");
    
    while (is_next != 255) {
        struct inst_loc *n_il;
        
        rdl(idxnum);
        rdc(slave_val);
        if (filever >= MINVER_CHAIN_REPS) {
            rdc(chain_reps);
        } else
            chain_reps = 0;
        
        rdc(is_next);
        
        n_il = dsbcalloc(1, sizeof(struct inst_loc));
        n_il->i = idxnum;
        n_il->slave = slave_val;
        n_il->chain_reps = chain_reps;
        
        if (!il)
            il = c_il = n_il;
        else {
            c_il->n = n_il;
            c_il = n_il;
        }
    } 
    
    RETURN(il);
}

int shpack(unsigned short s_v1, unsigned short s_v2) {
    int l_sv2 = s_v2;
    l_sv2 = (l_sv2 << 16);
    l_sv2 |= s_v1;
    return l_sv2;    
}

void shunpack(unsigned int packed,
    unsigned short *s_v1, unsigned short *s_v2)
{
    *s_v1 = (unsigned short)(packed & 0xFFFF);
    packed = (packed & 0xFFFF0000) >> 16;
    *s_v2 = (unsigned short)(packed);
}

void load_error(const char *s) {
    char vstr[60];

    snprintf(vstr, sizeof(vstr), "%s is not a valid DSB file", s);
    poop_out(vstr);
}

char *get_save_name(const char *filename, PACKFILE *pf) {
    char *gamename;
    int v;
    int filever;
    int xvars_c4;
    
    onstack("get_save_name");
    
    Mrdl(v);
    if (v == F_PACK_MAGIC) {
        pack_fclose(pf);
        pf = pack_fopen(fixname(filename), F_READ_PACKED);
        Mrdl(v);
    } else if (v == F_NOPACK_MAGIC) {
        Mrdl(v);
    }

    if (v != KEY_STRING)
        RETURN(NULL);

    rdl(filever);
    rdl(xvars_c4);

    gamename = retrstring(pf);
    
    pack_fclose(pf);
    
    RETURN(gamename);
}

int check_for_savefile(const char *filename, char **fidpt, char *writeinto) {
    PACKFILE *pf;
    
    onstack("check_for_savefile");
    
    pf = pack_fopen(fixname(filename), F_READ);
    if (pf == NULL)
        RETURN(0);
    
    if (writeinto != NULL) {
        char *s_svname = get_save_name(filename, pf);
        sprintf(writeinto, "%s", s_svname);
        dsbfree(s_svname);
    } else if (fidpt != NULL) {
        *fidpt = get_save_name(filename, pf);
    } else {
        pack_fclose(pf);
    }
    
    RETURN(1);
}

void write_override_info(PACKFILE *pf) {
    unsigned char override_store = 0;
    
    onstack("write_override_info");
    
    if (gfxctl.floor_override)
        override_store |= OV_FLOOR;

    if (gfxctl.roof_override)
        override_store |= OV_ROOF;
        
    if (gfxctl.floor_alt_override)
        override_store |= OVST_ALTFLOOR;

    if (gfxctl.roof_alt_override)
        override_store |= OVST_ALTROOF;

    wrc(override_store);
    wrc(gfxctl.override_flip);

    if (gfxctl.floor_override)
        storestring(pf, gfxctl.floor_override_name);
        
    if (gfxctl.floor_alt_override)
        storestring(pf, gfxctl.floor_alt_override_name);

    if (gfxctl.roof_override)
        storestring(pf, gfxctl.roof_override_name);
        
    if (gfxctl.roof_alt_override)
        storestring(pf, gfxctl.roof_alt_override_name);
        
    VOIDRETURN();
}

void read_override_info(PACKFILE *pf) {
    unsigned char override_store;
    unsigned char cx;
    
    onstack("read_override_info");
    
    rdc(override_store);
    rdc(gfxctl.override_flip);

    if (override_store & OV_FLOOR) {
        gfxctl.floor_override_name = retrstring(pf);
        gfxctl.floor_override = grab_from_gfxtable(gfxctl.floor_override_name);
    }
    
    if (override_store & OVST_ALTFLOOR) {
        gfxctl.floor_alt_override_name = retrstring(pf);
        gfxctl.floor_alt_override = grab_from_gfxtable(gfxctl.floor_alt_override_name);
    }

    if (override_store & OV_ROOF) {
        gfxctl.roof_override_name = retrstring(pf);
        gfxctl.roof_override = grab_from_gfxtable(gfxctl.roof_override_name);
    }
    
    if (override_store & OVST_ALTROOF) {
        gfxctl.roof_alt_override_name = retrstring(pf);
        gfxctl.roof_alt_override = grab_from_gfxtable(gfxctl.roof_alt_override_name);
    }
    
    VOIDRETURN();
}

void write_current_music_info(PACKFILE *pf) {
    unsigned int mbits = 0;
    int i;
    onstack("write_current_music_info");

    for(i=0;i<MUSIC_HANDLES;++i) {
        if (gd.cur_mus[i].uid) {
            mbits |= (1 << i);
        }
    } 
    wrl(mbits);
    
    if (mbits > 0) { 
        for(i=0;i<MUSIC_HANDLES;++i) {
            if (gd.cur_mus[i].uid) {
                wrl(gd.cur_mus[i].chan);
                storestring(pf, gd.cur_mus[i].uid);
                storestring(pf, gd.cur_mus[i].filename);                  
            }
        } 
    }    
      
    VOIDRETURN();       
}

void read_current_music_info(PACKFILE *pf) {
    unsigned int mbits = 0;
    
    onstack("read_current_music_info");
    
    rdl(mbits);
    if (mbits > 0) {
        int i;
                
        for(i=0;i<MUSIC_HANDLES;++i) {
            int i_bit = (1 << i);
            if (mbits & i_bit) {
                FMOD_SOUND *music_ptr;
                char *music_filename;
                char checkid[16];
                      
                rdl(gd.cur_mus[i].chan);
                gd.cur_mus[i].uid = retrstring(pf);
                music_filename = retrstring(pf);
                music_ptr = do_load_music(music_filename, checkid, 0);
                gd.cur_mus[i].filename = music_filename;
                gd.cur_mus[i].preserve = 1; // because it's frozen!
        
                if (strncmp(checkid, gd.cur_mus[i].uid, 15) != 0) {
                    fprintf(errorlog, "MUSIC: Hash[%d] %s on channel %d != saved hash %s\n",
                        i, checkid, gd.cur_mus[i].chan, gd.cur_mus[i].uid);
                            
                    poop_out("Music file load error\n\nSee log.txt for details");
                } else {
                    fprintf(errorlog, "Loaded music [%d] %s on channel %d (from hash %s)\n",
                        i, checkid, gd.cur_mus[i].chan, gd.cur_mus[i].uid);                    
                }
            }
        } 
    }
     
    VOIDRETURN();       
}

int load_savefile(const char *filename) {
    int opv = (MAJOR_VERSION*100) + MINOR_VERSION;
    int filever = 0;
    char c;
    unsigned char cx;
    unsigned char file_spec;
    int i;
    int v;
    int mxfood;
    int mxinst;
    int sysarch;
    int l_locks = MAX_LOCKS;
    int tmpval;
    unsigned int ext_capabs;
    PACKFILE *pf;
    char *gamename;
    int con_lines;
    
    onstack("load_savefile");
    
    // trash everything first
    destroy_dungeon();
    
    pf = pack_fopen(fixname(filename), F_READ);
    if (pf == NULL)
        RETURN(0);
        
    Mrdl(v);
    if (v == F_PACK_MAGIC) {
        pack_fclose(pf);
        pf = pack_fopen(fixname(filename), F_READ_PACKED);
        Mrdl(v);
    } else if (v == F_NOPACK_MAGIC) {
        Mrdl(v);
    }
    
    if (v != KEY_STRING)
        load_error(filename);
    
    rdl(filever);
    if (filever > opv)
        poop_out("You need a newer DSB version.");
    
    rdc(c);
    if (c != 0)
        load_error(filename);
        
    rdc(file_spec);
    
    rdc(cx);
    // CURRENT_BETA was set to 28 for a couple of release versions. Oops. Just do it this way...
    if (cx != 0 && cx != 28) {
        if (cx != CURRENT_BETA) {
            poop_out("Version id mismatch!\nThis is probably caused by trying to use a file\ncompiled by a beta version with a\n different beta or a release version.");
        }
    }
    
    rdc(cx);
    if (cx < BACKCOMPAT_REVISION) {
        if (cx == 64)
            poop_out("This file will only run on DSB 0.36.");
        else if (cx == 65)
            // current version
            ;
    }
    
    gamename = retrstring(pf);
    dsbfree(gamename);
    rdl(ext_capabs);
        
    rdc(gd.a_tickclock);
    rdc(cx);
    rdc(cx);
    rdc(cx);
    
    rdl(gd.leader);
    rdl(gd.num_champs);
    
    rdl(gd.a_party);
    read_mparty_flags(pf);
    for(i=0;i<4;++i) {
        rdl(gd.p_lev[i]);
        rdl(gd.p_x[i]);
        rdl(gd.p_y[i]);
        rdl(gd.p_face[i]);
    }
    
    rdl(gd.gameplay_flags);
    
    for(i=0;i<l_locks;++i)
        rdc(gd.gl->lockc[i]);
    
    rdc(gd.t_ypl);
    rdc(gd.t_cpl);
    rdc(gd.t_maxlines);
    rdc(cx);
    
    rdc(gd.max_total_light);
    rdc(cx);
    rdc(gd.run_everywhere_count);
    rdc(gd.distort_func);
    
    if (ext_capabs & DXC_VARIABLECONSOLE) {
        rdc(con_lines);
        rdc(cx);
        rdc(cx);
        rdc(cx);
    } else {
        con_lines = 4;
    }
    read_console_text(pf, con_lines);
    
    read_override_info(pf);
    
    for(i=0;i<4;++i) {
        int v;
        unsigned short chk;
        
        rdl(v);
        shunpack(v, &(gd.lastmethod[i][0]), &(gd.lastmethod[i][1]));
        
        rdl(v);
        shunpack(v, &(gd.lastmethod[i][2]), &chk);
        
        if (chk != 0) {
            poop_out("Stored attack method data corrupt");
        }
    }
    
    rdc(cx);
    if (cx != gd.max_lvl)
        poop_out("XP Level Mismatch");
        
    rdc(cx);
    if (cx != gd.max_class)
        poop_out("XP Class Mismatch");
        
    rdc(gd.max_UNUSED);
    rdc(gd.max_nstat);

    
    for(i=0;i<MAX_LIGHTS;++i) {
        rdl(gd.lightlevel[i]);
    }
    
    for(i=0;i<4;++i) {
        rdc(cx);
        gd.g_facing[i] = cx;
    }
    
    rdl(i);
    if (i != gd.num_conds) {
        poop_out("Condition Data Mismatch");
    }
    
    rdl(i);
    if (i != gd.num_pconds) {
        poop_out("Party Condition Data Mismatch");
    }
    
    if (gd.num_pconds) {

        gd.partycond = dsbcalloc(gd.num_pconds, sizeof(int));
        gd.partycond_animtimer = dsbcalloc(gd.num_pconds, sizeof(int));
        
        for(i=0;i<gd.num_pconds;++i) {
            rdl(gd.partycond[i]);
        }
        
        v_onstack("load_savefile.pcond_animtimer");
        if (filever >= MINVER_COND_ANIMTIMER) {
            for(i=0;i<gd.num_pconds;++i) {
                rdl(gd.partycond_animtimer[i]);
            }
        } else {
            set_animtimers_unused(gd.partycond_animtimer, gd.num_pconds);
        }
        v_upstack();
        
        
    } else {
        gd.partycond = NULL;
        gd.partycond_animtimer = NULL;
    }
    
    rdl(v);
    rdl(mxfood);
    
    rdl(sysarch);
    
    rdl(tmpval);
    if (tmpval != gii.max_invslots) {
        poop_out("Inventory Slot Data Mismatch");
    }
    
    rdl(tmpval);
    if (tmpval != gii.max_injury) {
        poop_out("Injury Slot Data Mismatch");
    }
    
    gd.champs = dsbcalloc(gd.num_champs, sizeof(struct champion));
    
    v_onstack("load_savefile.load_champion");
    for(i=0;i<gd.num_champs;++i) {
        struct champion *me = &(gd.champs[i]);
        int j;
        char c;
        char *tmp;
        unsigned char cap;
        
        tmp = retrstring(pf);
        snprintf(me->name, 8, "%s", tmp);
        dsbfree(tmp);
        
        tmp = retrstring(pf);
        snprintf(me->lastname, 20, "%s", tmp);
        dsbfree(tmp);
        
        me->port_name = retrstring(pf);
        me->method_name = retrstring(pf);           
        
        rdc(me->custom_v);
        rdc(cap);
        if (cap & EMPTY_CHAMPION) {
            dsbfree(me->port_name);
            dsbfree(me->method_name);
            me->port_name = NULL;
            me->method_name = NULL;
            continue;
        }
        
        me->portrait = grab_from_gfxtable(me->port_name);

        lua_getglobal(LUA, me->method_name);
        if (lua_istable(LUA, -1)) {
            me->method = import_attack_methods(me->name);
        }
        lua_pop(LUA, 1);
        
        // no longer used
        if (me->custom_v & _CUSV_DEPRECATED_1) {
            int unused_param;
            rdl(unused_param);
        }
        
        if (cap & CUST_HAND) {
            me->hand_name = retrstring(pf);
            me->hand = grab_from_gfxtable(me->hand_name);
        }
        
        if (cap & CUST_INVSCR) {
            me->invscr_name = retrstring(pf);
            me->invscr = grab_from_gfxtable(me->invscr_name);
        }
        
        if (cap & CUST_TOP_HANDS) {
            me->top_hands_name = retrstring(pf);
            me->topscr_override[TOP_HANDS] = grab_from_gfxtable(me->top_hands_name);
        }
        
        if (cap & CUST_TOP_PORT) {
            me->top_port_name = retrstring(pf);
            me->topscr_override[TOP_PORT] = grab_from_gfxtable(me->top_port_name);
        }
        
        if (cap & CUST_TOP_DEAD) {
            me->top_dead_name = retrstring(pf);
            me->topscr_override[TOP_DEAD] = grab_from_gfxtable(me->top_dead_name);
        }
                
        for (j=0;j<MAX_BARS;++j) {
            rdl(me->bar[j]);
            rdl(me->maxbar[j]);
        }
        
        for (j=0;j<MAX_STATS;++j) {
            rdl(me->stat[j]);
            rdl(me->maxstat[j]);
        }
        
        me->condition = dsbcalloc(gd.num_conds, sizeof(int));
        me->cond_animtimer = dsbcalloc(gd.num_conds, sizeof(int));
        
        v_onstack("load_savefile.cond_animtimer");
        for(j=0;j<gd.num_conds;++j) {
            int z;
            
            rdl(me->condition[j]);
            
            if (filever >= MINVER_COND_ANIMTIMER)
                rdl(me->cond_animtimer[j]);
            else
                me->cond_animtimer[j] = FRAMECOUNTER_NORUN;
        }
        v_upstack();
        
        allocate_champion_xp_memory(me);
        allocate_champion_invslots(me);
        
        for (j=0;j<gd.max_class;++j) {
            int q;
            for (q=0;q<5;++q) {
                rdl(me->lev_bonus[j][q]);
                rdl(me->xp_temp[j][q]);
            }
            
            for(q=0;q<4;++q)
                rdl(me->sub_xp[j][q]);
                
            rdl(me->xp[j]);
        }
        
        for (j=0;j<gii.max_injury;++j) {
            rdl(me->injury[j]);
        }
        
        for (j=0;j<gii.max_invslots;++j) {
            rdl(me->inv[j]);
        }
                
        rdl(me->load);
        rdl(me->maxload);
        rdl(me->food);
        rdl(me->water);
        rdl(me->exinst);
        
        retrexvars(pf, "ch_exvar", i+1);
        
        rdc(c);
        rdc(c);
        rdc(c);
        if (c != 127)
            poop_out("Character Data Corrupt");
        rdc(c);
    }
    v_upstack();
    
    rdl(gd.who_method); 
    rdl(gd.method_obj);
    rdl(gd.method_loc);
    
    rdl(v);
    for (i=0;i<4;++i) {
        int j;
        
        rdl(gd.party[i]);
        rdl(gd.idle_t[i]);
        
        for (j=0;j<8;++j) {
            rdc(gd.i_spell[i][j]);
        }
    }
    rdl(gd.whose_spell);
    for (i=0;i<2;++i) {
        int j;
        for (j=0;j<2;++j) {
            int t;
            for (t=0;t<4;++t) {
                rdc(gd.guypos[i][j][t]);
            }
        }
    }
    
    rdl(v);
    rdl(gd.mouseobj);
    rdl(gd.move_lock);
    rdl(gd.updateclock);
    rdl(gd.tickclock);
    rdl(gd.framecounter);
    rdl(v);

    read_current_music_info(pf);
    read_all_sounddata(pf);
           
    rdl(v);
       
    rdl(gd.dungeon_levels);
    rdl(v);
    
    dun = dsbcalloc(gd.dungeon_levels, sizeof(struct dungeon_level));

    for(i=0;i<gd.dungeon_levels;++i) {
        int j, yy, xx;
        struct dungeon_level *dd = &(dun[i]);
        
        rdl(dd->xsiz); 
        rdl(dd->ysiz);
        rdl(dd->lightlevel);
        rdl(dd->xp_multiplier);
        rdl(dd->wall_num);
        rdl(dd->tint);
        
        if (ext_capabs & DXC_TINTVALUE)
            rdl(dd->tint_intensity);
        else
            dd->tint_intensity = DSB_DEFAULT_TINT_INTENSITY;
            
        if (ext_capabs & DXC_DUNGEON_FLAGS)
            rdl(dd->level_flags);
        else
            dd->level_flags = 0;
        
        dd->t = dsbcalloc(dd->ysiz, sizeof(struct dtile*));
        for(j=0;j<dd->ysiz;++j)
            dd->t[j] = dsbcalloc(dd->xsiz, sizeof(struct dtile));
        
        for(yy=0;yy<dd->ysiz;++yy) {
            for (xx=0;xx<dd->xsiz;++xx) {
                unsigned char wv;
                unsigned char ws;
                int dv;
                
                rdc(wv);

                dd->t[yy][xx].w = (wv & 7);
                
                for (dv=0;dv<MAX_DIR;++dv) {
                    unsigned char ws;
                    rdc(ws);
                    dd->t[yy][xx].altws[dv] = ws;
                }
                
                for(dv=0;dv<5;++dv) {
                    if ((8 << dv) & wv) {
                        struct inst_loc *dt_il;
                        
                        dt_il = retr_illist(pf, filever);
                        dd->t[yy][xx].il[dv] = dt_il;    
                    }
                }   
            }
        }     
    }
    
    rdl(v);
    if (v != 0) {
        poop_out("Dungeon Map Corrupt");
    }
    
    rdl(mxinst);
    if (mxinst > NUM_INST) {
        poop_out("Too many insts stored");
    }
    
    for(i=1;i<mxinst;++i) {
        unsigned char old_framedelta = 0;
        unsigned short old_freezeframe = 0;
        unsigned short unused_value;
        char *astr;
        struct inst *p_inst;
        unsigned char extdata = 0;
        int inum;
        
        rdl(inum);
        if ((unsigned int)inum == 0xFFFFFFFF)
            break;
            
        if (inum > i)
            i = inum;
        
        oinst[i] = dsbcalloc(1, sizeof(struct inst));
        p_inst = oinst[i];
        
        p_inst->CHECKVALUE = DSB_CHECKVALUE;
        
        astr = retrstring(pf);
        p_inst->arch = arch_from_string(astr);
        dsbfree(astr);
                    
        rdl(p_inst->level);
        rdl(p_inst->x);
        rdl(p_inst->y);
        
        rdl(inum);
        p_inst->tile = inum & 0xFF;
        p_inst->facedir = (inum & 0x0000FF00) >> 8;
        
        if (filever >= MINVER_INST_PREV) {
            rdl(p_inst->prev.level);
            rdl(p_inst->prev.x);
            rdl(p_inst->prev.y);
            rdl(p_inst->prev.tile);
        } else {
            // won't match anything
            p_inst->prev.level = -300;
            p_inst->prev.x = 0;
            p_inst->prev.y = 0;
            p_inst->prev.tile = 0;
        }
        
        rdl(p_inst->x_tweak);
        rdl(p_inst->y_tweak);
        
        rdl(inum);
        shunpack(inum, &(p_inst->flytimer), &(p_inst->max_flytimer));
        p_inst->flycontroller = NULL;
        
        rdl(inum);
        shunpack(inum, &(p_inst->crop), &(p_inst->gfxflags));

        if (filever < MINVER_SANE_ANIMATION) {
            rdl(inum);
            shunpack(inum, &(old_freezeframe), &(unused_value));
        }
        
        if (filever >= MINVER_HAVE_TINTSAVED) {
            rdl(p_inst->tint);
            rdl(p_inst->tint_intensity);
        }
        
        if (filever >= MINVER_LONG_CHARGE) {
            rdl(p_inst->charge);
        }
        
        rdc(old_framedelta);
        if (filever < MINVER_LONG_CHARGE) {
            rdc(p_inst->charge);
        } else {
            rdc(p_inst->Unused_char);
        }
        rdc(p_inst->openshot);
        rdc(p_inst->inside_n);
        
        if (filever >= MINVER_SANE_ANIMATION)
            rdl(p_inst->frame);
        else {
            p_inst->frame = old_freezeframe + old_framedelta;
        }
                
        if (p_inst->inside_n) {
            int ic;
            p_inst->inside = dsbmalloc(p_inst->inside_n * sizeof(unsigned int));      
            for(ic=0;ic<p_inst->inside_n;++ic) {
                rdl(p_inst->inside[ic]);
            }
        }
        rdl(p_inst->uplink);
            
        rdl(inum);    
        shunpack(inum, &(p_inst->damagepower), &(p_inst->i_var));
                
        retrexvars(pf, "exvar", i);
        
        rdc(extdata);
        
        //fprintf(errorlog, "INST_EXDATA: %d has %d\n", i, extdata);
            
        if (extdata & EXD_CHAINTARG)    
            p_inst->chaintarg = retr_illist(pf, filever);
         
        // controller is not saved! it has to be restored
        // when the timer events queue is loaded in.   
        if (extdata & EXD_AI_CORE) {
            unsigned char zero;
            struct ai_core *ai_c;
            
            ai_c = dsbcalloc(1, sizeof(struct ai_core));
            p_inst->ai = ai_c;
            
            rdl(ai_c->state);         
            rdl(ai_c->boss);
            rdl(ai_c->hp);
            rdl(ai_c->maxhp);
            rdl(ai_c->fear);
            
            rdl(inum);
            if (filever < MINVER_IND_DELAY) { 
                shunpack(inum, &(ai_c->X_lcx), &(ai_c->X_lcy));
                ai_c->action_delay = 0;
            } else {
                short unused_var;
                ai_c->X_lcx = ai_c->X_lcy = 0;
                shunpack(inum, &(ai_c->action_delay), &(unused_var));
            }
                
            rdl(ai_c->ai_flags);
            
            rdc(zero);
            rdc(zero);
            rdc(zero);
            rdc(ai_c->attack_anim);
            
            rdc(ai_c->sface);
            rdc(ai_c->no_move);
            rdc(ai_c->see_party);
            rdc(ai_c->wallbelief);
            
            rdl(inum);
            if (inum != 0) {
                poop_out("Corrupt AI Information");
            }
        }
    }
    //fix_inst_int();
    
    rdl(v);
    if (v != 0) {
        poop_out("Inst Table Corrupt");
    }
    
    te = read_timerdata(pf);
    a_te = read_timerdata(pf);
    
    rdc(cx);
    if (cx != 255) {
        while (1) {
            char *gvar_name;
            gvar_name =  retrstring(pf);
            push_lua_filedata(pf, 0);
            lua_setglobal(LUA, gvar_name);
            dsbfree(gvar_name);
            rdc(cx);
            if (cx == 255)
                break;  
        }
    }
     
    rdl(v);
    rdl(v);
    if (v != 0x58585858)
        poop_out("Integrity check failed");
    
    pack_fclose(pf);
    
    party_viewto_level(gd.p_lev[gd.a_party]);
    set_3d_soundcoords();
    monster_telefrag(gd.p_lev[gd.a_party],
        gd.p_x[gd.a_party], gd.p_y[gd.a_party]);
    
    // restore the attack method
    if (gd.who_method) {
        int nm;
        nm = determine_method(gd.who_method-1, INV_INVALID);
        gd.num_methods = nm;
        gd.need_cz = 1;
    }
    
    lc_parm_int("__lua_backward_compat_fixes", 2, filever, opv);
    
    RETURN(1);
}

void save_savefile(const char *filename, const char *gamename) {
    PACKFILE *pf;
    int i;
    int iskip;
    struct exports *cex = ex_l;
    const char *s_cfilename;
    unsigned char override_store = 0;
    const unsigned int ext_capabs = DSB_CURRENT_EXTENDED_CAPABS;
    
    onstack("save_savefile");
    
    luastacksize(8);
    
    s_cfilename = fixname(filename);
	pf = pack_fopen(s_cfilename, F_WRITE_PACKED);
	if (pf == NULL) {
        gd.lua_error_found++;
        fprintf(errorlog, "FILE ERROR: Unable to open %s for writing\n", s_cfilename);
		VOIDRETURN();
    }

    // header stuff (and hooks for backcompatibility)
    Mwrl(KEY_STRING);
    wrl((MAJOR_VERSION*10) + MINOR_VERSION);
    W_CHARS(0, CURRENT_FILESPEC, CURRENT_BETA, BACKCOMPAT_REVISION);

    storestring(pf, gamename);

    wrl(ext_capabs);
    
    wrc(gd.a_tickclock);
    wrc(0);
    wrc(0);
    wrc(0);
    
    wrl(gd.leader);
    wrl(gd.num_champs);
    
    wrl(gd.a_party);
    write_mparty_flags(pf);
    for(i=0;i<4;++i) {
        wrl(gd.p_lev[i]);
        wrl(gd.p_x[i]);
        wrl(gd.p_y[i]);
        wrl(gd.p_face[i]);
    }
    wrl(gd.gameplay_flags);
    
    for(i=0;i<MAX_LOCKS;++i)
        wrc(gd.gl->lockc[i]);

    wrc(gd.t_ypl);
    wrc(gd.t_cpl);
    wrc(gd.t_maxlines);
    wrc(0);
    
    wrc(gd.max_total_light);
    wrc(0);
    wrc(gd.run_everywhere_count);
    wrc(gd.distort_func);
    
    wrc(gfxctl.console_lines);
    wrc(0);
    wrc(0);
    wrc(0);
    
    write_console_text(pf, gfxctl.console_lines);
    
    write_override_info(pf);

    for(i=0;i<4;++i) {
        wrl(shpack(gd.lastmethod[i][0], gd.lastmethod[i][1]));
        wrl(shpack(gd.lastmethod[i][2], 0));
    }
    
    wrc(gd.max_lvl);
    wrc(gd.max_class);
    wrc(gd.max_UNUSED);
    wrc(gd.max_nstat);

    for(i=0;i<MAX_LIGHTS;++i) {
        wrl(gd.lightlevel[i]);
    }
    
    for(i=0;i<4;++i) {
        unsigned char cx = gd.g_facing[i];
        wrc(cx);
    }
    
    wrl(gd.num_conds);
    wrl(gd.num_pconds);
    
    for(i=0;i<gd.num_pconds;++i) {
        wrl(gd.partycond[i]);
    }
    
    for(i=0;i<gd.num_pconds;++i) {
        wrl(gd.partycond_animtimer[i]);
    }
  
    W_CHARS(MAX_BARS, MAX_STATS, MAX_CLASSES, MAX_LEVELS);
    wrl(MAX_FOOD);

    wrl(gd.num_sysarch);
    
    wrl(gii.max_invslots);
    wrl(gii.max_injury);
      
    v_onstack("save_savefile.save_champion");
    for(i=0;i<gd.num_champs;++i) {
        struct champion *me = &(gd.champs[i]);
        int j;
        unsigned char cap = 0;
        
        storestring(pf, me->name);
        storestring(pf, me->lastname);
        storestring(pf, me->port_name);     // storing name only, must restore
        storestring(pf, me->method_name);   // storing name only, must restore
        
        if (!me->port_name) {
            wrc(0);
            wrc(EMPTY_CHAMPION);
            continue;
        }
                   
        if (me->hand) cap |= CUST_HAND;
        if (me->invscr) cap |= CUST_INVSCR;
        
        if (me->top_hands_name) cap |= CUST_TOP_HANDS;
        if (me->top_port_name) cap |= CUST_TOP_PORT;
        if (me->top_dead_name) cap |= CUST_TOP_DEAD;
        
        wrc(me->custom_v);
        wrc(cap);
    
        if (me->custom_v & _CUSV_DEPRECATED_1) {
            wrl(0);
        }

        if (me->hand)
            storestring(pf, me->hand_name);

        if (me->invscr)
            storestring(pf, me->invscr_name);
            
        if (me->top_hands_name)
            storestring(pf, me->top_hands_name);
            
        if (me->top_port_name)
            storestring(pf, me->top_port_name);
            
        if (me->top_dead_name)
            storestring(pf, me->top_dead_name);
        
        for (j=0;j<MAX_BARS;++j) {
            wrl(me->bar[j]);
            wrl(me->maxbar[j]);
        }
        
        for (j=0;j<MAX_STATS;++j) {
            wrl(me->stat[j]);
            wrl(me->maxstat[j]);
        }
        
        for(j=0;j<gd.num_conds;++j) {
            wrl(me->condition[j]);
            wrl(me->cond_animtimer[j]);
        }
        
        for (j=0;j<gd.max_class;++j) {
            int q;
            for (q=0;q<5;++q) {
                wrl(me->lev_bonus[j][q]);
                wrl(me->xp_temp[j][q]);
            }
            
            for(q=0;q<4;++q)
                wrl(me->sub_xp[j][q]);
                
            wrl(me->xp[j]);
        }
        
        for (j=0;j<gii.max_injury;++j) {
            wrl(me->injury[j]);
        }
        
        for (j=0;j<gii.max_invslots;++j) {
            wrl(me->inv[j]);
        }
                
        wrl(me->load);
        wrl(me->maxload);
        wrl(me->food);
        wrl(me->water);
        wrl(me->exinst);
        
        storeexvars(pf, "ch_exvar", i+1);

        W_CHARS(0, 0, 127, 128);
    }
    v_upstack();
    // usable with determine_method(arg-1)
    // to reconstruct attack method selected
    wrl(gd.who_method);
    wrl(gd.method_obj);
    wrl(gd.method_loc);
    
    W_CHARS(0, 0, 0, 0);
    for (i=0;i<4;++i) {
        int j;
        
        wrl(gd.party[i]);
        wrl(gd.idle_t[i]);
        
        for (j=0;j<8;++j) {
            wrc(gd.i_spell[i][j]);
        }
    }
    wrl(gd.whose_spell);    
    for (i=0;i<2;++i) {
        int j;
        for (j=0;j<2;++j) {
            int t;
            for (t=0;t<4;++t) {
                wrc(gd.guypos[i][j][t]);
            }
        }
    }
    
    wrl(0);
    wrl(gd.mouseobj);
    wrl(gd.move_lock);
    wrl(gd.updateclock);
    wrl(gd.tickclock);
    wrl(gd.framecounter);
    wrl(0); // don't change this 0!
    
    write_current_music_info(pf);
    write_all_sounddata(pf);
    
    W_CHARS(255, 255, 255, 255);   
    wrl(gd.dungeon_levels);
    W_CHARS(0, 0, 0, 0);
    for(i=0;i<gd.dungeon_levels;++i) {
        int yy, xx;
        struct dungeon_level *dd = &(dun[i]);
        
        wrl(dd->xsiz); 
        wrl(dd->ysiz);
        wrl(dd->lightlevel);
        wrl(dd->xp_multiplier);
        wrl(dd->wall_num);
        wrl(dd->tint);
        wrl(dd->tint_intensity);
        wrl(dd->level_flags);
        
        // store the tile data
        // bit 1 = solid wall (1)
        // bit 2 = visited (2)
        // bit 3 = illusionary wall (4)
        // bits 4,5,6,7,8 = are there obj pointers in this direction?
        for(yy=0;yy<dd->ysiz;++yy) {
            for (xx=0;xx<dd->xsiz;++xx) {
                unsigned char wv = dd->t[yy][xx].w;
                int dv;
                
                for(dv=0;dv<5;++dv) {
                    if (dd->t[yy][xx].il[dv]) {
                        wv |= (8 << dv);
                    }
                }
                wrc(wv); 
                
                for(dv=0;dv<MAX_DIR;++dv) {
                    unsigned char ws;
                    ws = dd->t[yy][xx].altws[dv];
                    wrc(ws);
                }
                
                for (dv=0;dv<5;++dv) {
                    struct inst_loc *dt_il = dd->t[yy][xx].il[dv];
                    if (!dt_il) continue;
                    store_illist(pf, dt_il);
                }    
            }
        }     
    }
    
    W_CHARS(0, 0, 0, 0);
    wrl(NUM_INST);
    iskip = 0;
    
    for(i=1;i<NUM_INST;++i) {
        struct inst *p_inst = oinst[i];
        struct obj_arch *p_arch;
        int ic;
        unsigned char extdata = 0;
        
        if (!p_inst) {
            iskip = 1;
            continue;
        }
        
        p_arch = Arch(p_inst->arch);
        
        wrl(i);    
        storestring(pf, p_arch->luaname);
        wrl(p_inst->level);
        wrl(p_inst->x);
        wrl(p_inst->y);
        wrl(p_inst->tile | (p_inst->facedir << 8));
        
        wrl(p_inst->prev.level);
        wrl(p_inst->prev.x);
        wrl(p_inst->prev.y);
        wrl(p_inst->prev.tile);        
        
        wrl(p_inst->x_tweak);
        wrl(p_inst->y_tweak);
        
        wrl(shpack(p_inst->flytimer, p_inst->max_flytimer));
        wrl(shpack(p_inst->crop, p_inst->gfxflags));
        //wrl(shpack(0, p_inst->XXX1));
        
        // as of 0.38 tint saved
        wrl(p_inst->tint);
        wrl(p_inst->tint_intensity);
        
        // as of 0.41 charge is a long
        wrl(p_inst->charge);
        
        W_CHARS(0, p_inst->Unused_char,
            p_inst->openshot, p_inst->inside_n);
            
        wrl(p_inst->frame);
            
        for(ic=0;ic<p_inst->inside_n;++ic) {
            wrl(p_inst->inside[ic]);
        }
          
        wrl(p_inst->uplink);  
        wrl(shpack(p_inst->damagepower, p_inst->i_var));
        
        storeexvars(pf, "exvar", i);
        
        if (p_inst->chaintarg)
            extdata |= EXD_CHAINTARG;
        if (p_inst->ai)
            extdata |= EXD_AI_CORE;
            
        wrc(extdata);
           
        if (p_inst->chaintarg)    
            store_illist(pf, p_inst->chaintarg);
           
        if (p_inst->ai) {
            struct ai_core *ai_c = p_inst->ai;
            
            wrl(ai_c->state);
            wrl(ai_c->boss);
            wrl(ai_c->hp);
            wrl(ai_c->maxhp);
            wrl(ai_c->fear);
            wrl(shpack(ai_c->action_delay, 0));
            
            wrl(ai_c->ai_flags);
            
            wrc(0);
            wrc(0);
            wrc(0);
            wrc(ai_c->attack_anim);
            
            wrc(ai_c->sface);
            wrc(ai_c->no_move);
            wrc(ai_c->see_party);
            wrc(ai_c->wallbelief);
            
            W_CHARS(0,0,0,0);
        }
    }
    
    /* INTEGRITY */
    W_CHARS(255, 255, 255, 255);
    wrl(0);
    /* ********* */
    
    append_new_queue_te(te, new_te);
    append_new_queue_te(te, eot_te);
    write_timerdata(pf, te);
    write_timerdata(pf, a_te);
    
    if (UNLIKELY(cex == NULL)) {
        wrc(255);
    } else {
        wrc(0);
        while(1) {
            storestring(pf, cex->s);
            lua_getglobal(LUA, cex->s);
            write_lua_data(pf, 0);
            lua_pop(LUA, 1);
            if (cex->n) {
                wrc(0);
                cex = cex->n;
            } else {
                wrc(255);
                break;
            }
        }
    }
    
    W_CHARS(0,0,0,0);
    W_CHARS(0x58,0x58,0x58,0x58);
    
    pack_fclose(pf);
     
    VOIDRETURN();
}

void load_dungeon(const char *raw_string, const char *compile_string) {
    const char *compiled_dungeon_filename;
    const char *raw_dungeon_filename;
    const char *loadstr;

    onstack("load_dungeon");

    if (gd.compile) loadstr = "compile";
    else loadstr = "load";
    fprintf(errorlog, "DUNGEON: Attempting to %s\n", loadstr);
    fflush(errorlog);
    
    luastacksize(2);
    lua_getglobal(LUA, compile_string);
    compiled_dungeon_filename = lua_tostring(LUA, -1);

    if (gd.compile || !load_savefile(compiled_dungeon_filename)) {
        char *dungeon_name;
        
        // allocate this, it's normally in the savefile and
        // load_savefile destroys it
        if (gd.num_pconds) {
            gd.partycond = dsbcalloc(gd.num_pconds, sizeof(int));
            gd.partycond_animtimer = dsbcalloc(gd.num_pconds, sizeof(int));
            set_animtimers_unused(gd.partycond_animtimer, gd.num_pconds);
        } else {
            gd.partycond = NULL;
            gd.partycond_animtimer = NULL;
        }
        
        lua_getglobal(LUA, raw_string);
        raw_dungeon_filename = dsbstrdup(lua_tostring(LUA, -1));
        lua_pop(LUA, 1);        
        gd.dungeon_loading = 1;
        src_lua_file(fixname(raw_dungeon_filename), 0);
        gd.dungeon_loading = 0;
        dsbfree(raw_dungeon_filename);

        if (gd.engine_flags & ENFLAG_SPAWNBURST) {
            poop_out("Error: spawnburst begun but not ended");
        }

        if (gd.compile) {
            dungeon_name = queryluaglobalstring("dungeon_name");
            save_savefile(compiled_dungeon_filename, dungeon_name);
            dsbfree(dungeon_name);
        }
    }
    lua_pop(LUA, 1);
    fflush(errorlog);

    VOIDRETURN();
}

