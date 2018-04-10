#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "gproto.h"
#include "arch.h"
#include "viewscale.h"

//extern FILE *errorlog;

struct animap **get_view(struct obj_arch *p_arch, int view) {

    if (view == V_DVIEW) {
        return p_arch->dview;
    } else if (view == V_INVIEW) {
        return p_arch->inview;
    } else if (view == V_OUTVIEW) {
        return p_arch->outview;
    } else if (view == V_SIDEVIEW) {
        return p_arch->sideview;
    } else if (view == V_EXTVIEW) {
        return p_arch->extraview;
    } else if (view >= V_ATTVIEW) {
        int att_num = (view - V_ATTVIEW);
        
        if (att_num >= p_arch->iv.n)
            return NULL;
        else
            return &(p_arch->iv.v[att_num]);
    }
    
    return NULL;
}

struct animap *iscaler(struct obj_arch *p_arch,
    struct animap *base_map, int sm)
{
    if (p_arch->type == OBJTYPE_DOOR)
        return scaledoor(base_map, sm);
    else
        return scaleimage(base_map, sm);
}

void invalid_cow(struct obj_arch *me, int viewnum, int n) {
    int z;
    struct animap **s;
    
    s = get_view(me, viewnum);
    
    if (s == NULL || s[0] == NULL)
        return;
                
    for (z=0;z<n;z++) {
        conddestroy_dynscaler(s[z]->ad);  
        s[z]->ad = NULL;
    }     
}

void invalidate_scale_cache(struct obj_arch *p_arch) {
     onstack("invalidate_scale_cache");
     
     invalid_cow(p_arch, V_DVIEW, 3);
     invalid_cow(p_arch, V_SIDEVIEW, 3);
     invalid_cow(p_arch, V_OUTVIEW, 3);
     
     if (p_arch->type == OBJTYPE_MONSTER)
        invalid_cow(p_arch, V_ATTVIEW, p_arch->iv.n);   
     else
        invalid_cow(p_arch, V_INVIEW, 3);
     
     VOIDRETURN();     
}

struct animap *do_dynamic_scale(int vsm, struct obj_arch *p_arch,
    int view, int blocker)
{
    int sm;
    struct animap **objview;
    struct animap *scalebase = NULL;
    int force_dynamic = 0;
    int clonefails = 0;
    
    objview = get_view(p_arch, view);
    if (!objview) return NULL;

    // objects have 6 views, and must be dynamic
    if (p_arch->type == OBJTYPE_THING) {
        sm = vsm;
        force_dynamic = 1;
    } else
        sm = (vsm%3);
        
    // multi-attack monsters are always dynamic also
    if (p_arch->type == OBJTYPE_MONSTER && (view >= V_ATTVIEW))
        force_dynamic = 1;

    if (sm == 0 || !force_dynamic) {
        struct animap *amap;
        
        amap = objview[sm];
        lod_use(amap);
        if (sm == 0 || blocker || amap)
            return amap;
    }

    if (!objview[0]) return NULL;
    scalebase = objview[0];
    lod_use(scalebase);
    
    if (!scalebase->ad) {
        scalebase->ad = dsbcalloc(1, sizeof(struct animap_dynscale));
    }
    if (!scalebase->ad->v[sm]) {
        scalebase->ad->v[sm] = iscaler(p_arch, scalebase, sm);        
    }

    return scalebase->ad->v[sm];
}

void DSB_aa_scale_blit(int do_it_anyway, BITMAP *source, BITMAP *dest,
    int s_x, int s_y, int s_w, int s_h,
    int d_x, int d_y, int d_w, int d_h)
{
    if (!do_it_anyway) {
        if (s_w == d_w && s_h == d_h)
            return;
    }
    
    stretch_blit(source, dest, s_x, s_y, s_w, s_h, d_x, d_y, d_w, d_h);   
}
