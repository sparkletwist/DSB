#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "monster.h"
#include "arch.h"
#include "editor_shared.h"
#include "spawnburst.h"

extern struct inst *oinst[NUM_INST];
extern struct global_data gd;
struct burst_obj *bl_sb = NULL;
struct burst_obj *bl_sb_end = NULL;

void execute_spawn_events(void) {
    struct burst_obj *c_sb = bl_sb;
    
    gd.queue_rc = 1;
    while (c_sb != NULL) {
        
        if (oinst[c_sb->inst]) {
            int lev = oinst[c_sb->inst]->level;
            int xx = oinst[c_sb->inst]->x;
            int yy = oinst[c_sb->inst]->y;
            int dir = oinst[c_sb->inst]->tile;
            call_member_func4(c_sb->inst, "on_spawn", lev, xx, yy, dir);
        }
        
        c_sb = c_sb->n;    
    }
    gd.queue_rc = 0;
    flush_instmd_queue_with_max(20000);
    
    
}

void terminate_spawnburst(void) {
    onstack("terminate_spawnburst");
    
    gd.queue_rc = 1;
    while (bl_sb != NULL) {
        struct burst_obj *c_sb = bl_sb;

        bl_sb = bl_sb->n;
        
        if (c_sb->f_type == FT_FLOOR)
            i_to_tile(c_sb->inst, 1);
            
        dsbfree(c_sb);
    }
    gd.queue_rc = 0;
    flush_instmd_queue_with_max(20000);
    
    bl_sb = bl_sb_end = NULL;
    
    gd.last_alloc = 0;
    VOIDRETURN();
}

void check_sb_to_queue(unsigned int inst, int level) {
    onstack("check_sb_to_queue");
    
    if (gd.engine_flags & ENFLAG_SPAWNBURST)
        sb_to_queue(inst, level);

    VOIDRETURN();
}

void sb_to_queue(unsigned int inst, int level) {
    struct burst_obj *n_bl;

    onstack("sb_to_queue");
    
    n_bl = dsbmalloc(sizeof(struct burst_obj));
    if (level >= 0)
        n_bl->f_type = FT_FLOOR;
    else
        n_bl->f_type = FT_NEGATIVELEVEL;
        
    n_bl->inst = inst;
    n_bl->n = NULL;
    
    if (!bl_sb) {
        bl_sb = bl_sb_end = n_bl;
    } else {
        bl_sb_end->n = n_bl;
        bl_sb_end = n_bl;
    }
    
    VOIDRETURN();
}

