#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "callstack.h"
#include "uproto.h"
#include "rdefer.h"

struct defdraw *objlist = NULL;
struct defdraw *objlist_end = NULL;
struct defdraw *cldlist = NULL;
struct defdraw *cldlist_end = NULL;

void destroy_ddraw_list(struct defdraw *l) {
    while (l != NULL) {
        struct defdraw *n = l->n;
        dsbfree(l);
        l = n;
    }
}

void purge_defer_list(void) {
    destroy_ddraw_list(objlist);
    objlist = objlist_end = NULL;
    
    destroy_ddraw_list(cldlist);
    cldlist = cldlist_end = NULL;
}


void defer_draw_ptr(struct defdraw **head_ptr,
    struct defdraw **end_ptr, BITMAP *sub_b, 
    int used_sub, int sm, int mx, int my,
    int cf, int flip_it, int alpha,
    struct inst *p_t_inst, int glow)
{
    struct defdraw *n_d;
     
    n_d = dsbmalloc(sizeof(struct defdraw));    
    n_d->bmp = sub_b;
    n_d->destroy = used_sub;
    n_d->sm = sm;
    n_d->tx = mx;
    n_d->ty = my;
    n_d->cf = cf;
    n_d->flip = flip_it;
    n_d->alpha = alpha;
    n_d->p_t_inst = p_t_inst;
    n_d->glow = glow;
    n_d->n = NULL;
    
    if (*head_ptr == NULL)
        *head_ptr = *end_ptr = n_d;
    else {
        (*end_ptr)->n = n_d;
        *end_ptr = n_d;
    }
}


int defer_draw(int dtype, BITMAP *sub_b, int used_sub,
    int sm, int mx, int my, int cf, int flip_it, int alpha,
    struct inst *p_t_inst, int glow)
{
    struct defdraw *n_d;
    
    if (dtype == OBJTYPE_CLOUD) {
        defer_draw_ptr(&cldlist, &cldlist_end, sub_b, used_sub, 
            sm, mx, my, cf, flip_it, alpha, p_t_inst, glow);
    } else {
        defer_draw_ptr(&objlist, &objlist_end, sub_b, used_sub, 
            sm, mx, my, cf, flip_it, alpha, p_t_inst, glow);
    }
    
    return 1;
}

void render_deferred_list(struct defdraw *dlist, BITMAP *scx) {
    struct defdraw *c_d;
      
    c_d = dlist;
    while(c_d != NULL) {    
        render_shaded_object(c_d->sm, scx, c_d->bmp,
            c_d->tx, c_d->ty, c_d->cf, c_d->flip, 0,
            c_d->alpha, c_d->p_t_inst, c_d->glow);
       
        if (c_d->destroy)
            destroy_bitmap(c_d->bmp);
            
        c_d = c_d->n;
    }
}

void render_deferred(BITMAP *scx) {
    
    if (cldlist)
        render_deferred_list(cldlist, scx);
    if (objlist)
        render_deferred_list(objlist, scx);
    
    purge_defer_list(); 
}
