#include <math.h>
#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "global_data.h"
#include "defs.h"
#include "uproto.h"
#include "arch.h"
#include "editor_shared.h"
#include "timer_events.h"
#include "monster.h"

const char *OBJECT_FILE_NAME = "objects.lua";
extern struct global_data gd;
extern FILE *errorlog;
extern struct S_arches garch;
//extern struct translate_table Translate;

extern struct inst *oinst[NUM_INST];
extern struct dungeon_level *dun;

extern int ZID;
extern int debug;

struct obj_arch *Arch(unsigned int i_num) {
    struct obj_arch *r_arch;
    unsigned int cnum;
    unsigned int anum;

    anum = (i_num & 0x00FFFFFF);
    cnum = (i_num & 0xFF000000) >> 24;

    r_arch = &(garch.ac[cnum][anum]);

    return r_arch;
}

void register_lua_objects(void) {
    lc_parm_int("__register_objects", 1, ZID);
}

void initialize_objects(void) {
    // inst-related stuff
    init_all_objects();
    init_exvars();
    
    // this sets up the metatable for the archs
    ZID = 1;
    lc_parm_int("__obj_mt", 0);
}
 
void load_system_objects(int loadparm) {       
    onstack("load_system_objects");
    
    if (loadparm & LOAD_OBJ_SYSTEM) {
        src_lua_file(bfixname(OBJECT_FILE_NAME), 0);
        gd.arch_select = AC_BASE;
        /*
        register_lua_objects();
        gd.num_sysarch = objarchctl(QUERY_ARCH, -1);
        fprintf(errorlog, "OBJ: %d system archs\n", gd.num_sysarch);
        */
    }
    
    if (loadparm & LOAD_OBJ_USER) {
        if (!gd.uprefix) {
            src_lua_file(fixname(OBJECT_FILE_NAME), 1);
        }
    }
    
    if (loadparm & LOAD_OBJ_REGISTER) {
        register_lua_objects();
        fprintf(errorlog, "OBJ: %d total archs\n", objarchctl(QUERY_ARCH, -1));
    }
    
    VOIDRETURN();
}

char *fixname(const char *file_name) {
    static char fixed_name[MAX_PATH];

    onstack("fixname");
    
    snprintf(fixed_name, sizeof(fixed_name),
        "%s/%s", gd.dprefix, file_name);

    RETURN(fixed_name);
}

char *bfixname(const char *file_name) {
    static char fixed_name[MAX_PATH];

    onstack("bfixname");

    snprintf(fixed_name, sizeof(fixed_name),
        "%s/%s", gd.bprefix, file_name);

    RETURN(fixed_name);
}

void purge_all_arch(void) {
    int i_ac;
    
    onstack("purge_all_arch");
    
    for(i_ac=0;i_ac<ARCH_CLASSES;++i_ac) {
        int i_arch = objarchctl(QUERY_ARCH, i_ac);
        int cn;
        for (cn=0;cn<i_arch;++cn) {
            struct obj_arch *p_a = &(garch.ac[i_ac][cn]);
            if (p_a != NULL) {
                condfree(p_a->luaname);
                condfree(p_a->dname);
                condfree(p_a->method);
                condfree(p_a->shade);
                
                if (p_a->lua_reg_key)
                    unreflua(p_a->lua_reg_key);
            }
        }
    }
    
    objarchctl(CLEAR_ALL_ARCH, -1);

    VOIDRETURN();
}

int inst_moveup(unsigned int inst) {
    struct inst *p_inst;
    struct dungeon_level *dd;
    struct inst_loc *dt_il = NULL;
    struct inst_loc *dt_prev = NULL;
    struct inst_loc *dt_next = NULL;
    int tx, ty, tl, il;
    
    onstack("inst_moveup");
    
    p_inst = oinst[inst];
    tl = p_inst->level;
    tx = p_inst->x;
    ty = p_inst->y;
    il = p_inst->tile;
    
    dd = &(dun[tl]);
    dt_il = dd->t[ty][tx].il[il];
    
    if (UNLIKELY(dt_il == NULL))
        RETURN(0);
        
    if (dt_il->n == NULL || dt_il->i == inst)
        RETURN(0);
        
    while (dt_il->n->i != inst) {
        dt_prev = dt_il;
        dt_il = dt_il->n;
    }
    
    dt_next = dt_il->n->n;
    dt_il->n->n = dt_il;
    
    if (dt_prev) dt_prev->n = dt_il->n;
    else dd->t[ty][tx].il[il] = dt_il->n;
    
    dt_il->n = dt_next;
    
    RETURN(1);
}

int inst_movedown(unsigned int inst) {
    struct inst *p_inst;
    struct dungeon_level *dd;
    struct inst_loc *dt_il = NULL;
    struct inst_loc *dt_prev = NULL;
    struct inst_loc *dt_next = NULL;
    int tx, ty, tl, il;

    onstack("inst_movedown");

    p_inst = oinst[inst];
    tl = p_inst->level;
    tx = p_inst->x;
    ty = p_inst->y;
    il = p_inst->tile;

    dd = &(dun[tl]);
    dt_il = dd->t[ty][tx].il[il];

    if (UNLIKELY(dt_il == NULL))
        RETURN(0);

    while (dt_il->i != inst) {
        dt_prev = dt_il;
        dt_il = dt_il->n;
    }
    
    if (dt_il->n == NULL)
        RETURN(0);

    dt_next = dt_il->n;
    dt_il->n = dt_il->n->n;
    
    if (dt_prev) dt_prev->n = dt_next;
    else dd->t[ty][tx].il[il] = dt_next;
    
    dt_next->n = dt_il;

    RETURN(1);
}

int check_slave_subtile(int facedir, int tile) {
    int ccw;
    
    onstack("check_slave_subtile");
    
    if (facedir == tile)
        ccw = 0;
    else if (facedir == (tile+3)%4)
        ccw = 1;
    else {
        RETURN(-2);
    }
    
    RETURN(ccw);
}

void destroyed_monster(struct inst *p_m_inst) {
    onstack("destroyed_monster");
    
    // A call to destroy_assoc_timer isn't needed, this works the same
    if (p_m_inst->ai->controller)
        p_m_inst->ai->controller->delme = 1;
    
    // destroy ai
    dsbfree(p_m_inst->ai);
    p_m_inst->ai = NULL;
    
    VOIDRETURN();
}

void thick_line(BITMAP *bmp, float x, float y, float x_, float y_,
    float thickness, int color)
{
    float dx = x - x_;
    float dy = y - y_;
    float d = sqrtf(dx * dx + dy * dy);
    if (!d)
        return;
 
    int v[4 * 2];
 
    /* left up */
    v[0] = x - thickness * dy / d;
    v[1] = y + thickness * dx / d;
    /* right up */
    v[2] = x + thickness * dy / d;
    v[3] = y - thickness * dx / d;
    /* right down */
    v[4] = x_ + thickness * dy / d;
    v[5] = y_ - thickness * dx / d;
    /* left down */
    v[6] = x_ - thickness * dy / d;
    v[7] = y_ + thickness * dx / d;
 
    polygon(bmp, 4, v, color);
}

// straight out of CSBwin
int decode_character_value(const char *buf, short num) {
    int result = 0;
    
    onstack("decode_character_value");
  
    do {
        result <<= 4;
        if ( (*buf < 'A') || (*buf > 'P') ) {
            RETURN(-1);
        }
    
        result += *(buf++) - 'A';
        num--;
    } while (num!=0);
  
    RETURN(result);
}



