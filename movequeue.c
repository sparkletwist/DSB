#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "gproto.h"
#include "movebuffer.h"

CRITICAL_SECTION Queue_CS;
int instant_click;

unsigned int CurrentKey;
int KeyQueue[KEYQUEUE_SIZE];
int KeyQueuePos = 0;

struct movequeue *mq;
struct movequeue *mq_end;

void dsb_clear_immed_key_queue(void) {
    EnterCriticalSection(&Queue_CS);
    memset(KeyQueue, 0, KEYQUEUE_SIZE*sizeof(int)); 
    KeyQueuePos = 0;   
    CurrentKey = 0;
    LeaveCriticalSection(&Queue_CS);    
}

void dsb_shift_immed_key_queue(void) {
    int i;
    EnterCriticalSection(&Queue_CS);
    CurrentKey = KeyQueue[0]; 
    if (KeyQueuePos > 0) {
        for(i=1;i<(KeyQueuePos+1);++i) {
            KeyQueue[i-1] = KeyQueue[i];
        }
        KeyQueuePos--;  
    }
    LeaveCriticalSection(&Queue_CS);    
}

void dsb_clear_keybuf(void) {
    onstack("dsb_clear_keybuf");
    clear_keybuf();
    dsb_clear_immed_key_queue();
    VOIDRETURN();
}

void DSBcallback_kb(int scancode) {
    EnterCriticalSection(&Queue_CS);
    if (!(scancode & 0x80) && (KeyQueuePos < (KEYQUEUE_SIZE-1))) {
        KeyQueue[KeyQueuePos++] = scancode;   
    }
    LeaveCriticalSection(&Queue_CS);
}

void DSBcallback_mouse(int flags) {
    int icl = 0;
    
    if (flags & MOUSE_FLAG_LEFT_DOWN)
        icl |= 1;

    if (flags & MOUSE_FLAG_RIGHT_DOWN) 
        icl |= 2;
        
    if (flags & MOUSE_FLAG_MIDDLE_DOWN)
        icl |= 4;
    
    if (icl) {
        instant_click |= icl;     
        
        EnterCriticalSection(&Queue_CS);   
        KeyQueue[KeyQueuePos++] = 0x80000000 | (mouse_x << 4) | (mouse_y << 18) | icl;
        LeaveCriticalSection(&Queue_CS);
    }
    
}

void initmovequeue(void) {
    mq = mq_end = NULL;
    
    keyboard_lowlevel_callback = DSBcallback_kb;
    mouse_callback = DSBcallback_mouse;
}

void destroymovequeue(void) {

    while (mq) {
        struct movequeue *mqn = mq->n;
        dsbfree(mq);
        mq = mqn;
    }
    
    mq = mq_end = NULL;
}

void addmovequeue(int t, int m, int y) {
    onstack("addmovequeue");
    
    if (!mq) {
        mq = dsbmalloc(sizeof(struct movequeue));
        mq_end = mq;
    } else {
        mq_end->n = dsbmalloc(sizeof(struct movequeue));
        mq_end = mq_end->n;
    }
    
    mq_end->type = t;
    mq_end->x = m;
    mq_end->y = y;
    mq_end->n = NULL;
        
    VOIDRETURN();    
}

void addmovequeuefront(int t, int m, int y) {
    struct movequeue *n_mq;
    
    onstack("addmovequeuefront");
    
    n_mq = dsbmalloc(sizeof(struct movequeue));    
    n_mq->type = t;
    n_mq->x = m;
    n_mq->y = y;
    n_mq->n = mq;
    mq = n_mq;
        
    VOIDRETURN();    
}

int popmovequeue(int *t, int *m, int *y) {
    struct movequeue *t_mq;
    
    if (!mq) return 0;
    
    onstack("popmovequeue");
    t_mq = mq;
    *t = mq->type;
    *m = mq->x;
    *y = mq->y;
    mq = mq->n;
    dsbfree(t_mq);
    RETURN(1);
}
