#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <signal.h>
#include "objects.h"
#include "global_data.h"
#include "defs.h"
#include "uproto.h"

int debug = 0;
int Gmparty_flags = 0;

// meaningless under ESB
void destroy_alt_targ(void) { return; }
void destroy_viewports(void) { return; }
void destroy_all_subrenderers(void) { return; }
void destroy_console(void) { return; }
void destroy_all_sound_handles(void) { return; }
void destroy_all_timers(void) { return; }
void clear_method(void) { return; }

int to_tile(int lev, int xx, int yy, int dir, int inst, int onoff) {
    return 0;
}
int i_to_tile(unsigned int inst, int onoff) {
    return 0;
}
void sb_to_queue(int inst) { return; }
void check_sb_to_queue(int inst) { return; }
void terminate_spawnburst(void) { return; }
void execute_spawn_events(void) { return; }

int party_ist_peek(int x) { return 0; }

// parameter eaters
void destroy_animap(void *x) { return; }
void psxf(const char *x) { return; }
void add_iname(const char *x, const char *y) { return; }

// no point in bothering with this
void *load_png(char *s, void *v) { return 0; }

// this should never be necessary in edit mode
void instmd_queue(int op, int lev, int xx, int yy, int dir,
    int inst, int data) { return; }
void coord_inst_r_error(const char *s_error, unsigned int inst, int x)
    { return; }
    
int mon_rotate2wide(unsigned int inst, struct inst *pb, int rdir, int b_tele) {
    return;
}
    
void flush_inv_instmd_queue() { return; }
void inv_instmd_queue(int op, int lev, int xx, int yy, int dir,
    int inst, int data) { return; }
void reset_monster_ext(void *x) { return; }

void destroy_all_music_handles(void) { return; }

void activate_gui_zone(char *d, int z, int x, int y, int w, int h, int flags) {
    return;
}
void internal_gui_command(const char cmd, int val) {
    return;
}
void internal_shadeinfo(unsigned int r1, int p1, int p2, int p3, int p4, int p5) {
    return;
}
void purge_from_lod(int mode, struct animap *lani) {
    return;
}
void set_animtimers_unused(int *a, int an) {
    return;
}
void drop_flying_inst(void *p, void *px) {
    return;
}
void flush_all_ist(void) {
    return;
}

// perform a simpler task
void recover_error(char *reason) { program_puke(reason); }
void defer_kill_monster(int m) { recursive_inst_destroy(m, 0); }
void queued_inst_destroy(unsigned int inst) { inst_destroy(inst, 0); }
    
