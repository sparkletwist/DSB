#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include "callstack.h"
#include "istack.h"
#include "uproto.h"

extern FILE *errorlog;
istack istaparty;
istack istaparty_top;

void ist_error_dump(istack *ist) {
    if (errorlog) {
        fprintf(errorlog, "@@@ IST ERROR @@@\n@@@ ist %x istack_cap_top %x index %ld\n"
            "@@@ system stacks %x %x\n", ist, ist->istack_cap_top, ist->top,
            istaparty, istaparty_top);
        fflush(errorlog);
    }
}

void ist_init(istack *ist) {
    onstack("ist_init");
    ist->top = 0;
    ist->max = 0;
    ist->e = NULL;
    VOIDRETURN();
}

void ist_push(istack *ist, int ne) {
    if (ist->top == ist->max) {
        ist->e = dsbrealloc(ist->e, (ist->max + 8) * sizeof(int));
        ist->max += 8;
    }
    ist->e[ist->top] = ne;
    ist->top++;
}

int ist_pop(istack *ist) {
    int pv = 0;
    
    if (ist->top == 0) {
        return(-99);
    } else if (ist->top < 0) {
        ist_error_dump(ist);
        program_puke("IST popped negative index");
    }

    ist->top--;
    pv = ist->e[ist->top];

    return(pv);
}

int ist_opeek(istack *ist, int idefv) {  
    if (ist->top > 0) {
        return(ist->e[ist->top - 1]);
    }
    
    return(idefv);
}

void ist_cap_top(istack *ist) {
    if (ist->istack_cap_top) {
        ist_push(ist->istack_cap_top, ist->top);
        if (ist->top < 0) {
            ist_error_dump(ist);
            program_puke("IST pushed garbage topindex");
        }
    }
}

void ist_check_top(istack *ist) {
    if (ist->istack_cap_top) {
        int execs = 0;
        int topval = ist_pop(ist->istack_cap_top);
        if (UNLIKELY(topval == -99)) {
            program_puke("IST Internal Error");
        }
        
        while (topval < ist->top) {
            int last_return = ist_pop(ist);
            execs++;
            if (execs > 100) {
                char errstr[100];
                fprintf(errorlog, "@@@ IST ERROR @@@\n@@@ ist %x check_top error!\n---- Topval is %ld - IST top is %ld - "
                    "last return was %ld - loop ran %ld times\n",
                    ist, topval, ist->top, last_return, execs); 
                program_puke("IST topindex loop error");
                break;
            }
        }
    }
}

void ist_cleanup(istack *ist) {
    if (ist->max > (ist->top + 10)) {
        ist->e = dsbrealloc(ist->e, (ist->top + 8) * sizeof(int));
        ist->max = ist->top + 8;
    }
}

void init_all_ist(void) {
    ist_init(&istaparty);
    ist_init(&istaparty_top);
    istaparty.istack_cap_top = &istaparty_top;
}

void flush_all_ist(void) {   
    while (ist_pop(&istaparty) != -99);
    while (ist_pop(&istaparty_top) != -99);   
}
