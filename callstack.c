#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <signal.h>
#include "objects.h"
#include "global_data.h"
#include "defs.h"
#include "uproto.h"

int c_STACKLEVEL = 0;
char vstack[4096];
char *csptr;
extern int debug;

extern struct global_data gd;
extern FILE *errorlog;

/*
static struct inst *INTEGRITY[NUM_INST];
extern struct inst *oinst[NUM_INST];

void fix_inst_int(void) {
    memcpy(INTEGRITY, oinst, sizeof(struct inst*)*NUM_INST);
}

// For debugging only-- it can get slow!
void check_inst_int(void) {
    int z;
    
    for (z=0;z<NUM_INST;++z) {
        if (INTEGRITY[z] != oinst[z]) {
            recover_error("Inst list integrity corruption");
        }
    }
}
*/

void CHECKOUT(int sl) {
    if (sl + 1 != c_STACKLEVEL) {
        program_puke("Callstack Corruption");
    }
}


int cstacklev() {
    return c_STACKLEVEL;
}

void init_stack() {
    memset(vstack, 0, sizeof(vstack));
    csptr = vstack;
}

void v_onstack(const char *on_str) {
    
    ++c_STACKLEVEL;
    while (*on_str != '\0') {
        *csptr = *on_str;
        ++csptr;
        ++on_str;
    }
    
    *csptr = '\0';
    csptr++;
    
    //check_inst_int();
    
    if (LIKELY((csptr-vstack) < 3900)) return;
    
    program_puke("Stack Overflow");
}

void stackbackc(char c) {
    *(csptr-1) = c;
    --c_STACKLEVEL;
}

void addstackc(char c) {
    *(csptr-1) = c;
    *csptr = '\0';
    ++csptr;
}

void v_luaonstack(const char *on_str) {
    *csptr = 'l'; ++csptr;
    *csptr = 'u'; ++csptr;
    *csptr = 'a'; ++csptr;
    *csptr = '.'; ++csptr;
    v_onstack(on_str);
}
    
void v_upstack() {

    csptr -= 2;
    
    while(*csptr != '\0')
        --csptr;
        
    ++csptr;
    
    --c_STACKLEVEL;
    //check_inst_int();
}

void sigcrash(int sig) {
    signal(sig, NULL);
    
    if (sig == SIGSEGV)
        program_puke("Segmentation Fault");
        
    else if (sig == SIGILL)
        program_puke("Illegal Instruction");
        
    else if (sig == SIGFPE)
        program_puke("Divide by zero");
}

void really_bad_situation(int sig) {
        exit(sig);
}

void program_puke(char *reason) {
    char ohno[8192];
    char *iptr = vstack;
    
    signal(SIGSEGV, really_bad_situation);
    
    set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
    
    --csptr;

    while(iptr < csptr) {
        if (*iptr == '\0') *iptr = '\n';
        ++iptr;
    }
    
    fprintf(errorlog, "PROGRAM CRASH!\nLocation: %ld %ld %ld\nReason: %s\nStack Dump:\n%s",
        gd.p_lev[0], gd.p_x[0], gd.p_y[0], reason, vstack);
    fclose(errorlog);
    
    snprintf(ohno, sizeof(ohno), "Fatal Error: %s\nStack Trace:\n%s\n\n%s\n%s",
        reason, vstack, "This information is also recorded in LOG.TXT.",
        "It would be handy to send along if you're making a bug report.");

    MessageBox(win_get_window(), ohno, "CRASH :(", MB_ICONSTOP);
    DSBallegshutdown();
    exit(2);
}

void recursion_puke(void) {
    recover_error("Runaway recursion");
}

void puke_bad_subscript(char *var, int badsub) {
    char uhoh[256];
    
    snprintf(uhoh, sizeof(uhoh), "Bad subscript %s[%d]", var, badsub);
    recover_error(uhoh);
}
