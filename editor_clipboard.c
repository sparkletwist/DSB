#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "compile.h"
#include "arch.h"
#include "monster.h"
#include "trans.h"
#include "E_res.h"
#include "editor.h"
#include "editor_hwndptr.h"
#include "editor_gui.h"
#include "editor_shared.h"
#include "editor_menu.h"
#include "editor_clipboard.h"
#include "integration.h"
#include "integration_esb.h"

extern HWND sys_hwnd;
extern lua_State *LUA;
extern struct editor_global edg;
extern struct global_data gd;

extern struct inst *oinst[NUM_INST];
extern struct dungeon_level *dun;
extern struct translate_table Translate;

int duplicate_instance(int from_id) {
    int to_id;
    struct inst *p_from;
    struct inst *p_to;
    
    onstack("duplicate_instance");
    
    p_from = oinst[from_id];
    to_id = next_object_inst(0, p_from->arch, 0);
    p_to = oinst[to_id];
    
    memcpy(p_to, p_from, sizeof(struct inst));
    // ignore chained targets
    p_to->chaintarg = NULL;
    // copy AI core
    if (p_from->ai) {
        p_to->ai = dsbmalloc(sizeof(struct ai_core));
        memcpy(p_to->ai, p_from->ai, sizeof(struct ai_core));
    }
    // copy insides
    if (p_from->inside_n > 0) {
        int inum;
        p_to->inside = dsbmalloc(sizeof(int) * p_from->inside_n);
        for (inum=0;inum<p_from->inside_n;++inum) {
            p_to->inside[inum] = duplicate_instance(p_from->inside[inum]);
        }
    }
    note_exvar_duplication(from_id, to_id);
    
    RETURN(to_id);    
}

struct inst_loc *create_inst_loc_duplicate(struct inst_loc *in_cp_from) {
    struct inst_loc *cp_to = NULL;
    struct inst_loc *cp_to_end = NULL;
    struct inst_loc *cp_from = in_cp_from;
    
    onstack("create_inst_loc_duplicate");    
    while (cp_from) {
        struct inst_loc *ct = dsbmalloc(sizeof(struct inst_loc));
        if (cp_to) {
            cp_to_end->n = ct; 
        } else {
            cp_to = ct;
        }
        cp_to_end = ct;
        
        memcpy(ct, cp_from, sizeof(struct inst_loc));
        
        if (cp_from->slave) {
            poop_out("Attemped to use a slave pointer in edit mode?\nThis is probably caused by a bug in ESB.");
        }
        
        ct->n = NULL;
        ct->i = duplicate_instance(cp_from->i);
        
        cp_from = cp_from->n;    
    }
    
    RETURN(cp_to);
}

void begin_inst_exvar_duplication(void) {
    onstack("begin_inst_exvar_duplication");
    lc_parm_int("__ed_copy_begin", 0);    
    VOIDRETURN();
}

void fix_copied_inst_exvar_references(void) {
    onstack("fix_copied_inst_exvar_references");
    lc_parm_int("__ed_copy_finish", 0);
    VOIDRETURN();    
}

void note_exvar_duplication(int from_id, int to_id) {
    onstack("note_exvar_duplication");
    lc_parm_int("__ed_copy_notify", 2, from_id, to_id);    
    VOIDRETURN();
}

void change_inside_location(int in_what, int integration) {
    struct inst *p_in_inst;
    int i;
    
    onstack("change_inside_location");
    p_in_inst = oinst[in_what];
    for (i=0;i<p_in_inst->inside_n;++i) {
        int iobj = p_in_inst->inside[i];
        oinst[iobj]->x = in_what;
        
        if (integration) {
            struct obj_arch *p_obj_arch = Arch(oinst[iobj]->arch);
            const char *arch_name = p_obj_arch->luaname;
            
            finalize_integration_obj_add(iobj, arch_name, LOC_IN_OBJ, in_what, i, 0);
            integration_inst_update(iobj);            
        }
        
        if (oinst[iobj]->inside_n > 0) {
            change_inside_location(iobj, integration);
        }
    }
    VOIDRETURN();    
}

void list_is_cut(struct inst_loc *in_list) {
    struct inst_loc *dt = in_list;
    
    onstack("list_is_cut");
    while (dt) {
        struct inst *p_inst = oinst[dt->i];
        p_inst->esb_iscut = 1;        
        dt = dt->n;    
    }
    
    VOIDRETURN();     
}

void paste_list_at_location(struct inst_loc *in_list, int lev, int x, int y, int t, int integration) {
    struct inst_loc *dt = in_list;
    
    onstack("paste_list_at_location");
    while (dt) {
        struct inst *p_inst = oinst[dt->i];
        p_inst->level = lev;
        p_inst->x = x;
        p_inst->y = y;
        p_inst->tile = t;
        
        p_inst->esb_iscut = 0;
        
        if (!integration) {
            if (p_inst->inside_n > 0) {
                change_inside_location(dt->i, 0);
            }
        }
        
        dt = dt->n;    
    }
    if (lev >= 0) {
        struct inst_loc *targ_lt = dun[lev].t[y][x].il[t];
        
        if (integration) {
            struct inst_loc *integ_list = in_list;
            while (integ_list) {
                v_onstack("paste_list_at_location.integration");
                struct obj_arch *p_obj_arch = Arch(oinst[integ_list->i]->arch);
                const char *arch_name = p_obj_arch->luaname;
                
                finalize_integration_obj_add(integ_list->i, arch_name, lev, x, y, t);
                integration_inst_update(integ_list->i);
                integ_list = integ_list->n;
                v_upstack();
            }
        }
        
        if (!targ_lt) {
            dun[lev].t[y][x].il[t] = in_list;
        } else {
            while (targ_lt->n) {
                targ_lt = targ_lt->n;
            }
            targ_lt->n = in_list;
        }
    }
    
    // change_inside_location has to be called after the object has already
    // been created for DSB to be able to create objects in it; all this
    // does for ESB is make the above loop less efficient so it's fine
    if (integration) {
        dt = in_list;
        while (dt) {
            struct inst *p_inst = oinst[dt->i]; 
            
            if (p_inst->inside_n > 0) {
                change_inside_location(dt->i, 1);
            }
            
            dt = dt->n;  
        }
    }
    
    VOIDRETURN();
}

void ed_clipboard_command(int cmd) {
    int integration = 0;
    
    onstack("ed_clipboard_command");
    
    if (!edg.cp) {
        VOIDRETURN();
    }
    
    if (dsb_is_running()) {
        integration = 1;
    }
    
    if (cmd == ECCM_CUT || cmd == ECCM_COPY) {
        struct dungeon_level *dd = &(dun[edg.cp->level]);
        struct inst_loc *targl[MAX_DIR];
        int i;
        int found = 0;
        
        destroy_edit_clipboard();
        v_onstack("ed_clipboard_command.malloc_and_set");
        edg.cb = dsbmalloc(sizeof(cut_paste_cb));
        memset(edg.cb, 0, sizeof(cut_paste_cb));
        v_upstack();
        
        v_onstack("ed_clipboard_command.store_il_table");
        for (i=0;i<MAX_DIR;++i) {
            targl[i] = dd->t[edg.cp->y1][edg.cp->x1].il[i];
            if (targl[i]) found++;
        }
        v_upstack();
        
        if (!found) {
            destroy_edit_clipboard();
            VOIDRETURN();
        }
        
        if (cmd == ECCM_CUT) {
            v_onstack("ed_clipboard_command.cut");
            for (i=0;i<MAX_DIR;++i) {
                edg.cb->t[i] = targl[i];
                list_is_cut(targl[i]);
                dd->t[edg.cp->y1][edg.cp->x1].il[i] = NULL;
                
                if (integration && (targl[i] != NULL)) {
                    v_onstack("ed_clipboard_command.cut.integration");
                    struct inst_loc *il = targl[i];
                    while (il) {
                        unsigned int i_inst = il->i;
                        struct obj_arch *p_arch = Arch(oinst[i_inst]->arch);
                        integration_inst_destroy(i_inst, p_arch);
                        il = il->n;
                    }
                    v_upstack();
                }
            }
            v_upstack();
        } else {
            v_onstack("ed_clipboard_command.copy");
            edg.cb->dup = 1;
            begin_inst_exvar_duplication();
            for (i=0;i<MAX_DIR;++i) {
                edg.cb->t[i] = create_inst_loc_duplicate(targl[i]);
            }
            fix_copied_inst_exvar_references();
            v_upstack();
        }   
             
    } else {
        struct inst_loc *newl[MAX_DIR];
        int i;
        
        if (!edg.cb) {
            VOIDRETURN();
        }
        
        v_onstack("ed_clipboard_command.paste");
        
        begin_inst_exvar_duplication();
        for (i=0;i<MAX_DIR;++i) {
            newl[i] = create_inst_loc_duplicate(edg.cb->t[i]);
        }
        fix_copied_inst_exvar_references();
        
        edg.cb->dup = 1;
        for (i=0;i<MAX_DIR;++i) {
            paste_list_at_location(edg.cb->t[i],
                edg.cp->level, edg.cp->x1, edg.cp->y1, i, integration);
        
            edg.cb->t[i] = newl[i];
        }
        
        v_upstack();
    }
    
    force_redraw(edg.subwin, 1);
    VOIDRETURN();        
}

void clipboard_mark(int tl, int tx, int ty) {
    onstack("clipboard_mark");
    deselect_edit_clipboard();
    edg.cp = dsbmalloc(sizeof(cut_paste_sel));
    edg.cp->level = tl;
    edg.cp->x1 = tx;
    edg.cp->y1 = ty;
    VOIDRETURN();   
}

// mostly unused
void deselect_edit_clipboard(void) {
    onstack("deselect_edit_clipboard"); 
    dsbfree(edg.cp);
    edg.cp = NULL;
    VOIDRETURN();   
}

void destroy_edit_clipboard(void) {
    onstack("destroy_edit_clipboard"); 
    if (edg.cb) {
        int i;
        for (i=0;i<MAX_DIR;++i) {
            destroy_inst_loc_list(edg.cb->t[i], 0, 1);
        }
        dsbfree(edg.cb);
    }
    edg.cb = NULL;
    VOIDRETURN();   
}
