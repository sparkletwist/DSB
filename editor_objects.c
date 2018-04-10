#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "objects.h"
#include "monster.h"
#include "champions.h"
#include "global_data.h"
#include "uproto.h"
#include "compile.h"
#include "arch.h"
#include "editor_hwndptr.h"
#include "editor_gui.h"
#include "editor_menu.h"
#include "editor_shared.h"

struct inst *oinst[NUM_INST];
extern const char *OBJECT_FILE_NAME;
extern struct global_data gd;

int ZID;

void setup_editor_paths(const char *fname) {
    char *s_bname;
    char *s_c_bname;
    char *s_slashptr = NULL;
    char sep = '/';
    int ctr = 0;

    onstack("setup_editor_paths");
    
    s_bname = dsbstrdup(fname);
    s_c_bname = s_bname;

    while (*s_c_bname != '\0') {
        ++ctr;

        if (*s_c_bname == '/' || *s_c_bname == '\\') {
            s_slashptr = s_c_bname;
            sep = *s_slashptr;
        }

        ++s_c_bname;
    }
    if (s_slashptr != NULL)
        *s_slashptr = '\0';

    condfree(gd.dprefix);
    gd.dprefix = dsbstrdup(s_bname);
    dsbfree(s_bname);
    
    VOIDRETURN();
}

void spawned_monster(unsigned int inst, struct inst *p_m_inst, int l) {
    onstack("ESB_spawned_monster");    
    p_m_inst->ai = dsbcalloc(1, sizeof(struct ai_core));    
    
    VOIDRETURN();
}

void exec_party_place(int ap, int lev, int xx, int yy, int facedir) {
    gd.p_lev[ap] = lev;
    gd.p_x[ap] = xx;
    gd.p_y[ap] = yy;
    gd.p_face[ap] = facedir;
}

void ed_move_tile_position(int inst, int zmt) {
    struct inst *p_inst = oinst[inst];
    struct obj_arch *p_arch = Arch(p_inst->arch);
    depointerize(inst);
    p_inst->tile = zmt;
    
    // auto-rotate fatsos so they don't break everything
    if (p_arch->type == OBJTYPE_MONSTER &&
        p_arch->msize == 2 && zmt != 4)
    {
        if (get_slave_subtile(p_inst) < 0)
            p_inst->facedir = zmt;
    }
    
    pointerize(inst);    
} 

int ed_calculate_mon_hp(struct inst *p_inst) {
    struct obj_arch *p_arch = Arch(p_inst->arch);
    int rhp = 1;
    
    onstack("ed_calculate_mon_hp");
    
    if (p_inst->level >= 0) {
        int base = rqueryluaint(p_arch, "hp");
        rhp = lc_parm_int("calc_monster_initial_hp", 2, base, p_inst->level);
    }
    
    RETURN(rhp);
}
    

