#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "objects.h"
#include "champions.h"
#include "lua_objects.h"
#include "global_data.h"
#include "uproto.h"
#include "timer_events.h"
#include "editor_shared.h"
#include "gamelock.h"

extern struct global_data gd;
extern struct graphics_control gfxctl;
extern lua_State *LUA;

extern struct wallset **ws_tbl;
extern int ws_tbl_len;

extern struct timer_event *te;
extern struct timer_event *a_te;

extern struct inst *oinst[NUM_INST];
struct dungeon_level *dun;
extern unsigned int cwsa_num;
extern struct wallset *cwsa;

extern FILE *errorlog;

void destroy_timer(struct timer_event *cte) {
    onstack("destroy_timer");

    while (cte != NULL) {
        struct timer_event *ncte = cte->n;
        dsbfree(cte);
        cte = ncte;
    }

    VOIDRETURN();
}

void destroy_all_timers(void) {
    onstack("destroy_all_timers");

    destroy_timer(te);
    destroy_timer(a_te);

    te = NULL;
    a_te = NULL;

    VOIDRETURN();
}

void destroy_system_subrenderers(void) {

    onstack("destroy_system_subrenderers");

    if (gfxctl.subrend_targ != NULL)
        VOIDRETURN();

    conddestroy_animap(gfxctl.subrend);
    gfxctl.subrend = NULL;
    gfxctl.do_subrend = 1;

    conddestroy_animap(gfxctl.statspanel);
    gfxctl.statspanel = NULL;

    conddestroy_animap(gfxctl.food_water);
    gfxctl.food_water = NULL;

    conddestroy_animap(gfxctl.objlook);
    gfxctl.objlook = NULL;

    VOIDRETURN();
}

void destroy_all_subrenderers() {

    onstack("destroy_all_subrenderers");

    destroy_system_subrenderers();

    if (gfxctl.i_subrend) {
        destroy_bitmap(gfxctl.i_subrend);
        gfxctl.i_subrend = NULL;
    }

    VOIDRETURN();
}

void destroy_viewports() {
    conddestroy_animap(gfxctl.l_viewport_rtarg);
    gfxctl.l_viewport_rtarg = NULL;

    condfree(gfxctl.floor_override_name);
    gfxctl.floor_override = NULL;
    condfree(gfxctl.roof_override_name);
    gfxctl.roof_override = NULL;
}
