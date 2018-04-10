#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "gproto.h"
#include "movebuffer.h"
#include "timer_events.h"
#include "arch.h"
#include "gamelock.h"

extern struct global_data gd;

void lockflags_change(unsigned int lockflags, int b_set) {
    unsigned int i;

    onstack("lockflags_change");
    
    for (i=0;i<MAX_LOCKS;++i) {
        unsigned int i_flagcmp = (1 << i);
        if (lockflags & i_flagcmp) {
            if (b_set)
                gd.gl->lockc[i]++;
            else if (!b_set && gd.gl->lockc[i])
                gd.gl->lockc[i]--;
        }
    }

    VOIDRETURN();
}

int lockflags_get(unsigned int bitflag) {
    int res_lockflags = 0;
    int i;
    
    onstack("lockflags_get");

    for (i=0;i<MAX_LOCKS;++i) {
        unsigned int i_flagcmp = (1 << i);
        if (bitflag & i_flagcmp)
            res_lockflags += gd.gl->lockc[i];
    }
    
    RETURN(res_lockflags);
}
