#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "objects.h"
#include "uproto.h"
#include "lproto.h"
#include "champions.h"
#include "gproto.h"
#include "timer_events.h"
#include "global_data.h"
#include "arch.h"

extern struct condition_desc *i_conds;
extern struct condition_desc *p_conds;
struct timer_event *te;
struct timer_event *a_te;
struct timer_event *new_te;
struct timer_event *eot_te;

struct timer_event *add_timer(int inst, int type, int first_time, int do_time,
    int datafield, int altdata, unsigned int id_sender)
{
    return (add_timer_ex(inst, type, first_time, do_time,
        datafield, altdata, id_sender, 0, 0));    
}

struct timer_event *add_timer_ex(int inst, int type, int first_time, int do_time,
    int datafield, int altdata, unsigned int id_sender, int ap, int xd)
{
    struct timer_event **root_te;
    struct timer_event *n_te;
    
    onstack("add_timer_ex");
    
    n_te = dsbcalloc(1, sizeof(struct timer_event));
    n_te->type = type;
    n_te->inst = inst;
    
    // set the timer to the max interval and start counting down
    n_te->maxtime = do_time;
    n_te->time_until = first_time;
    
    n_te->data = datafield;
    n_te->altdata = altdata;
    n_te->pstate = ap;
    n_te->xdata = xd;

    n_te->sender = id_sender;
    
    root_te = &new_te;
    if (first_time == DELAY_EOT_TIMER) {
        root_te = &eot_te;
        n_te->time_until = 1;
    }
    
    if (*root_te == NULL) {
        *root_te = n_te;
    } else {
        struct timer_event *it_te = *root_te;
        while (it_te->n != NULL)
            it_te = it_te->n;
        it_te->n = n_te;
    }
    
    RETURN(n_te);
}

void delay_all_timers_in(struct timer_event *bte, int inst, int type, int delay) {
    struct timer_event *cte;
    
    onstack("delay_all_timers_in");
    
    cte = bte;
    while (cte != NULL) {
        if (cte->inst == inst && cte->type == type) {
            cte->time_until += delay;
        } 
        cte = cte->n;  
    }
    
    VOIDRETURN(); 
}

void delay_all_timers(int inst, int type, int delay) {
    delay_all_timers_in(te, inst, type, delay);
    delay_all_timers_in(new_te, inst, type, delay);
    delay_all_timers_in(a_te, inst, type, delay);
}

int destroy_assoc_timer_in(struct timer_event *bte,
    unsigned int inst, int etype, int data) 
{
    struct timer_event *cte;
    
    onstack("destroy_assoc_timer_in");
    cte = bte;
    while (cte != NULL) {
        
        if (cte->inst == inst && cte->type == etype) {
            if (data < 0 || cte->data == data) {
                cte->delme = 1;
                
                if (data != -2)
                    RETURN(1);
            }
        }
            
        cte = cte->n;    
    }
    
    if (data == -2) {
        RETURN(1);
    } 
    
    RETURN(0);
}

void destroy_assoc_timer(unsigned int inst, int etype, int data) {
    int success = 0;
    onstack("destroy_assoc_timer");
    success += destroy_assoc_timer_in(te, inst, etype, data);
    success += destroy_assoc_timer_in(new_te, inst, etype, data);
    if (data != -2 && !success) {
        char uhoh[128];
        
        snprintf(uhoh, sizeof(uhoh), 
            "Destroying invalid timer %d (type %d)", inst, etype);
        
        program_puke(uhoh);
    }   
    VOIDRETURN();
}

void cleanup_timers(struct timer_event **p_root_te) {
    struct timer_event *cte = *p_root_te;
    struct timer_event *pte = NULL;
    
    while (cte != NULL) {   
        if (cte->delme) {
            struct timer_event *optr = cte;
            
            if (pte == NULL) {
                *p_root_te = cte->n;
                cte = *p_root_te;
            } else
                cte = pte->n = cte->n;
            
            dsbfree(optr);
        } else {
            pte = cte;
            cte = cte->n;
        }   
    }
}
