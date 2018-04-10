#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "global_data.h"
#include "champions.h"
#include "uproto.h"
#include "gproto.h"

extern struct global_data gd;
extern struct inst *oinst[NUM_INST];
extern struct dungeon_level *dun;
extern struct inventory_info gii;

extern FILE *errorlog;

unsigned int *level_table;

int determine_mastery(int rawsublevel, int who, int maincl, int subcl, int usebonus) {
    int lv = 0;
    int ac;
    int xp;

    onstack("determine_mastery");
    /*
    {
        char x[300];
        stackbackc('(');
        sprintf(x, "%d,%d,%d,%d,%d)", rawsublevel, who, maincl, subcl, usebonus);
        v_onstack(x);
    }
    */

    xp = gd.champs[who].xp[maincl];
    if (usebonus)
        xp += gd.champs[who].xp_temp[maincl][0];
        
    v_onstack("determine_mastery.sub_table");
    if (subcl) {
        if (rawsublevel) {
            xp = gd.champs[who].sub_xp[maincl][subcl-1] * gd.max_subsk[maincl];
        } else {
            xp += gd.champs[who].sub_xp[maincl][subcl-1];
    
            if (usebonus)
                xp += gd.champs[who].xp_temp[maincl][subcl];
    
            xp = xp/2;
        }
    }
    v_upstack();

    for(ac=0;ac<gd.max_lvl;ac++) {
        if (xp >= level_table[ac])
            ++lv;
        else
            break;
    }
    
    if (usebonus) {
        lv += gd.champs[who].lev_bonus[maincl][0];
        if (subcl)
            lv += gd.champs[who].lev_bonus[maincl][subcl-1];
    }

    if (!rawsublevel) {
        if (lv < 0)
            lv = 0;
        
        if (lv > gd.max_lvl)
            lv = gd.max_lvl;
    }

    RETURN(lv);
}

void destroy_champion_xp_memory(struct champion *me) {
    int i;

    onstack("destroy_champion_xp_memory");

    for(i=0;i<gd.max_class;++i) {
        dsbfree(me->sub_xp[i]);
        dsbfree(me->xp_temp[i]);
        dsbfree(me->lev_bonus[i]);
    }
    dsbfree(me->sub_xp);
    dsbfree(me->xp_temp);
    dsbfree(me->lev_bonus);
    dsbfree(me->xp);

    VOIDRETURN();
}

void destroy_champion_invslots(struct champion *me) {
    dsbfree(me->inv);
    dsbfree(me->inv_queue_data);
    dsbfree(me->injury);
}

// number index is based on LUA VALUES
void calculate_maxload(int lcidx) {
    gd.champs[lcidx-1].maxload = lc_parm_int("sys_calc_maxload", 1, lcidx);
}

void mouse_obj_grab(unsigned int pickup) {
    onstack("mouse_obj_grab");
    
    gd.mouseobj = pickup;
    oinst[pickup]->gfxflags &= ~(G_UNMOVED);
    obj_at(pickup, LOC_MOUSE_HAND, 0, 0, 0);
        
    if (gd.leader >= 0) {
        int ipl = gd.party[gd.leader]-1;
        gd.champs[ipl].load += ObjMass(pickup);  
    }
    
    VOIDRETURN();     
}

void mouse_obj_drop(unsigned int putdown) {
    
    onstack("mouse_obj_drop");
    
    if (gd.mouseobj != putdown) {
        recover_error("Bad mouse_obj_drop");
        VOIDRETURN();
    }

    gd.mouseobj = 0;
    if (gd.mouse_mode >= MM_EYELOOK) {
        gd.mouse_mode = MM_EYELOOK;
    }
    
    oinst[putdown]->gfxflags &= ~(G_UNMOVED);
    obj_at(putdown, LOC_LIMBO, 0, 0, 0);
    
    if (gd.leader >= 0) {
        int ipl = gd.party[gd.leader]-1;

        gd.champs[ipl].load -= ObjMass(putdown);
        determine_load_color(ipl);    
    }   
    
    VOIDRETURN();   
}

// Set up the champion's dynamically allocated xp variables
void allocate_champion_xp_memory(struct champion *me) {
    int i;

    onstack("allocate_champion_xp_memory");

    me->xp = dsbcalloc(gd.max_class, sizeof(int));
    me->sub_xp = dsbcalloc(gd.max_class, sizeof(int*));
    me->xp_temp = dsbcalloc(gd.max_class, sizeof(int*));
    me->lev_bonus = dsbcalloc(gd.max_class, sizeof(int*));
    
    for(i=0;i<gd.max_class;++i) {
        me->sub_xp[i] = dsbcalloc(gd.max_subsk[i], sizeof(int));
        me->xp_temp[i] = dsbcalloc(gd.max_subsk[i] + 1, sizeof(int));
        me->lev_bonus[i] = dsbcalloc(gd.max_subsk[i]+ 1, sizeof(int));
    }
    
    VOIDRETURN();
}

void allocate_champion_invslots(struct champion *me) {
    me->inv = dsbcalloc(gii.max_invslots, sizeof(int));
    me->inv_queue_data = dsbcalloc(gii.max_invslots, sizeof(int));
    me->injury = dsbcalloc(gii.max_injury, sizeof(int));
}

int levelxp(int level) {

    if (UNLIKELY(level < 0 || level > gd.max_lvl)) {
        puke_bad_subscript("level_table", level);
        return 0;
    }
        
    if (level == 0)
        return 0;
        
    return (level_table[level-1]);    
}

