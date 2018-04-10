#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "gproto.h"

// measured in ticks
#define LOD_USE_MARGIN  800
struct lod_ll {
    struct animap *a;
    struct lod_ll *n;
};
struct lod_ll *LOD_LOADED = NULL;
struct lod_ll *FRAME_LOD_PTR = NULL;
struct lod_ll *FRAME_LOD_PTR_P = NULL;

extern FILE *errorlog;
extern struct global_data gd;
extern struct graphics_control gfxctl;

struct animap *prepare_lod_animap(const char *name, const char *longname) {
    struct animap *my_a_map;
    struct lod_data *newlod;
    
    onstack("prepare_lod_animap");

    newlod = dsbmalloc(sizeof(struct lod_data));
    memset(newlod, 0, sizeof(struct lod_data));
    newlod->shortname = dsbstrdup(name);
    newlod->filename = dsbstrdup(longname);
    
    my_a_map = dsbmalloc(sizeof(struct animap));
    memset(my_a_map, 0, sizeof(struct animap));
    my_a_map->lod = newlod;

    RETURN(my_a_map);        
}

void lod_use(struct animap *lani) {
    struct lod_ll *nl;
    BITMAP *lod_bmp;

    if (!lani || !lani->lod) {
        return;
    }
    
    lani->lod->lastuse = gd.lod_timer;
    
    if (lani->lod->d & ALOD_LOADED) {
        return;
    }
    
    // this shouldn't happen any more
    if (lani->b) {
        char errmsg[200];
        sprintf(errmsg, "Bitmap structure is defined on unloaded bitmap!\nShortname: %s\nLongname: %s\nPointer: %x",
            lani->lod->shortname, lani->lod->filename, lani->b);
        poop_out(errmsg);
        return;
    }
    
    lod_bmp = pcxload(lani->lod->shortname, lani->lod->filename);
    lani->lod->d |= ALOD_LOADED;
    if (gd.loaded_tga && !(lani->lod->d & ALOD_SCANNED)) {
        if (bmp_alpha_scan(lod_bmp))
            lani->flags |= AM_HASALPHA;
        lani->lod->d |= ALOD_SCANNED;
    }
    lani->b = lod_bmp;   
    lani->w = lod_bmp->w;
    lani->numframes = 1; 
    gd.loaded_tga = 0; 
    
    if (lani->lod->d & ALOD_ANIMATED) {
        setup_animation(0, lani, NULL, lani->lod->ani_num_frames, lani->lod->ani_framedelay);
    }
    
    nl = dsbmalloc(sizeof(struct lod_ll));
    nl->a = lani;
    nl->n = LOD_LOADED;
    LOD_LOADED = nl;
    
    /*
    fprintf(errorlog, "Lod_use Image: %s (%s) @ %x [%x]\n",
        lani->lod->shortname, lani->lod->filename, lani, lod_bmp); 
    */
}

int expire_lod(struct animap *cl_a) {
    int destroyed;
    
    onstack("expire_lod");
    
    if (!cl_a->b) {
        char errmsg[200];
        sprintf(errmsg, "Requested expiration on null bmp!\nShortname: %s\nLongname: %s\nPointer: %x",
            cl_a->lod->shortname, cl_a->lod->filename, cl_a);
        poop_out(errmsg);      
        RETURN(0);  
    }
    
    /*
    fprintf(errorlog, "Expired Image: %s (%s) @ %x\n",
        cl_a->lod->shortname, cl_a->lod->filename, cl_a); 
    fflush(errorlog);
    */
    
    destroyed = 0;
    if (!(cl_a->lod->d & ALOD_KEEP)) {
        if (!cl_a->cloneof && !cl_a->clone_references) {
            
            if (cl_a->numframes > 1) {
                int i;
                for (i=0;i<cl_a->numframes;++i) {
                    destroy_bitmap(cl_a->c_b[i]);
                }
                dsbfree(cl_a->c_b);
            } else {
                destroy_bitmap(cl_a->b);
            }
                
            conddestroy_dynscaler(cl_a->ad);
            cl_a->b = NULL;
            cl_a->ad = NULL;
            cl_a->lod->d &= ~(ALOD_LOADED);
            destroyed = 1;
        }
    }
    if (!destroyed) {
        cl_a->lod->lastuse = gd.lod_timer;  
    }
    
    RETURN(destroyed);  
}

void frame_lod_use_check(void) {
    int destroyed = 0;
    int count = 0;
    onstack("frame_lod_use_check");
    
    if (FRAME_LOD_PTR == NULL) {
        FRAME_LOD_PTR = LOD_LOADED;
        FRAME_LOD_PTR_P = NULL;
    }
        
    while (FRAME_LOD_PTR && count < 16) {
        struct animap *cl_f_a = FRAME_LOD_PTR->a;
        
        destroyed = 0;
        
        if ((cl_f_a->lod->lastuse + LOD_USE_MARGIN) < gd.lod_timer) {
            destroyed = expire_lod(cl_f_a);
        }        
        
        if (destroyed) {
            if (FRAME_LOD_PTR == LOD_LOADED) {
                FRAME_LOD_PTR = LOD_LOADED->n;
                dsbfree(LOD_LOADED);
                LOD_LOADED = FRAME_LOD_PTR;
            } else {
                FRAME_LOD_PTR_P->n = FRAME_LOD_PTR->n;
                dsbfree(FRAME_LOD_PTR);
                FRAME_LOD_PTR = FRAME_LOD_PTR_P->n;
            }
            count += 2;
        } else {
            FRAME_LOD_PTR_P = FRAME_LOD_PTR;
            FRAME_LOD_PTR = FRAME_LOD_PTR->n;
            ++count;
        }           
    }
    
    VOIDRETURN();    
}

void full_lod_use_check(void) {
    onstack("full_lod_use_check");
    purge_from_lod(1, NULL);
    VOIDRETURN();
}

void purge_from_lod(int mode, struct animap *pptr) {
    struct lod_ll *cur_lod = LOD_LOADED;
    struct lod_ll *cur_lod_p = NULL;
    
    onstack("purge_from_lod");
    
    while (cur_lod) {
        int destroyed = 0;
        struct animap *cl_a = cur_lod->a;
        
        if (mode) {
            if ((cl_a->lod->lastuse + LOD_USE_MARGIN) < gd.lod_timer) {
                destroyed = expire_lod(cl_a);
            }
        } else {
            // we're actually destroying it elsewhere
            // this just ganks it from the list
            if (cl_a == pptr) {
                destroyed = 1;
            }
        }
               
        if (destroyed) {
            if (!cur_lod_p) {
                cur_lod = LOD_LOADED->n;
                dsbfree(LOD_LOADED);
                LOD_LOADED = cur_lod;
            } else {
                cur_lod_p->n = cur_lod->n;
                dsbfree(cur_lod);
                cur_lod = cur_lod_p->n;
            }
        } else {
            cur_lod_p = cur_lod;
            cur_lod = cur_lod->n;
        }
    }
    
    FRAME_LOD_PTR = NULL;
    FRAME_LOD_PTR_P = NULL;
     
    VOIDRETURN();
}

void destroy_lod_list(void) {
    onstack("destroy_lod_list");
    while (LOD_LOADED) {
        struct lod_ll *ll_n = LOD_LOADED->n;
        dsbfree(LOD_LOADED);
        LOD_LOADED = ll_n;
    }
    VOIDRETURN();
}


