#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "render.h"
#include "champions.h"
#include "global_data.h"
#include "uproto.h"
#include "gproto.h"
#include "rdefer.h"
#include "timer_events.h"
#include "arch.h"
#include "monster.h"
#include "viewscale.h"

extern BITMAP *FORCE_RENDER_TARGET;
extern int (*dsbgetpixel)(BITMAP *, int, int);
extern void (*dsbputpixel)(BITMAP *, int, int, int);

// miscoffsets
#define WD2Y 74

// monster offsets for the renderer
#define D1X0 190
#define D1X1 258
#define D1X2 266
#define D1X3 182
#define D1YF 165
#define D1YC 182
#define LD1X0 2
#define LD1X1 92
#define LD1X2 70
#define LD1X3 -32
#define RD1X0 (448 - LD1X1)
#define RD1X1 (448 - LD1X0)
#define RD1X2 (448 - LD1X3)
#define RD1X3 (448 - LD1X2)

#define D2X0 196
#define D2X1 250
#define D2X2 258
#define D2X3 190
#define D2YF 140
#define D2YC 148
#define LD2X0 66
#define LD2X1 130
#define LD2X3 44
#define LD2X2 120
#define RD2X0 (448 - LD2X1)
#define RD2X1 (448 - LD2X0)
#define RD2X2 (448 - LD2X3)
#define RD2X3 (448 - LD2X2)

#define DrawWall(Z) ((((Z->w) & 1) && (!(Z->SCRATCH & 2))) || (Z->SCRATCH & 4))

//only use if altws exists!
#define AlwaysDraw(Z)   (ws_tbl[Z->altws[DIR_CENTER] - 1]->flags & WSF_ALWAYS_DRAW) 

//#define M2(Z)   (Z & ~(2))

#define ChooseSet(A,W) cwsa = choosewallset(A,W)

#define draw_i_pl(S,I,L) draw_i_pl_ex(softmode, S,I,0,0,L, 0)

#define OUTSOFTMODE() {if (softmode) used_softmode = 1; \
    else {upstack();release_bitmap(scx);return(-1);}}
    
#define rendfloor(S,D,F,X,Y,M,T) \
{if(draw_floorstuff(softmode,S,D,F,X,Y,M,T,0)) OUTSOFTMODE();}

#define rendfloor_cloudsonly(S,D,F,X,Y,M,T) \
{if(draw_floorstuff_cloudsonly(softmode,S,D,F,X,Y,M,T,0)) OUTSOFTMODE();}

#define rendczfloor(S,D,F,X,Y,M,T,Z) \
{if(draw_floorstuff(softmode,S,D,F,X,Y,M,T,Z)) OUTSOFTMODE();}

#define rendhaze(S,D,X,Y,W,H,B,F,Z) {if(draw_haze(softmode,cwsa,S,D,X,Y,W,H,B,F,Z)) OUTSOFTMODE();}

#define rendmonster(S,D,F,X,Y,M,T) \
{if(draw_monster(softmode,S,D,F,X,Y,M,T,0,0,0,0)) OUTSOFTMODE();}

#define rendczmonster(S,D,F,X,Y,M,T,Z,Q) \
{if(draw_monster(softmode,S,D,F,X,Y,M,T,Z,Q,0,0)) OUTSOFTMODE();}

#define rendssmonster(S,D,F,X,Y,M,T,R) \
{if(draw_monster(softmode,S,D,F,X,Y,M,T,0,0,R,0)) OUTSOFTMODE();}

#define renddecals(S,D,X,Y,M,T) \
{if(draw_floordecals(softmode,S,D,X,Y,M,T)) OUTSOFTMODE();}

#define rendwall(S,D,F,X,Y,M,T) \
{if(draw_wallstuff(softmode,S,D,F,X,Y,M,T,0)) OUTSOFTMODE();}

#define rendczwall(S,D,F,X,Y,M,T,Z) \
{if(draw_wallstuff(softmode,S,D,F,X,Y,M,T,Z)) OUTSOFTMODE();}
   
#define ablit(S,M,X,Y,F,O) {if(wall_animap_blit(softmode,cwsa,S,M,X,Y,F,O,0,0,0)) OUTSOFTMODE();}
#define cropblit(S,M,X,Y,F,O,C,D,E) {if(wall_animap_blit(softmode,cwsa,S,M,X,Y,F,O,C,D,E)) OUTSOFTMODE();}

#define OSIDE(x) ((x+448) - cwsa->wall[awall]->w)
#define O_HSIDE(x) ((x+448) - haze_base->wall[awall]->w)

#define DRAW_ITERCEIL(X,Y) \
if (b_iter) { \
    it_ct = &(copy_tile[Y][X]);\
    if (it_ct->altws[DIR_CENTER] && (!DrawWall(it_ct) || AlwaysDraw(it_ct))) { \
        trapblit(1, scx, bx, by, it_ct, X, Y, oddeven, 0, floor_bitmap, roof_bitmap);\
    }\
}

#define DO_SM_POSITION_HACKS() \
    if (sm == 1) {\
        if (tflags & SUBWIDTH)\
            mx -= amap->mid_x_tweak;\
        else\
            mx += amap->mid_x_tweak;\
        my += amap->mid_y_tweak;\
    }\
    if (sm == 2) {\
        if (tflags & SUBWIDTH)\
            mx -= amap->far_x_tweak;\
        else\
            mx += amap->far_x_tweak;\
        my += amap->far_y_tweak;\
    }
    
#define SETUP_COMPO() \
    if (compobase)\
        drawbase = compobase;\
    else\
        drawbase = base;\
    \
    newcompo = create_bitmap_ex(32, drawbase->w, drawbase->h);\
    if (alpha)\
        clear_to_color(newcompo, 0);\
    else\
        clear_to_color(newcompo, makecol(255, 0, 255));

#define SUBWIDTH        0x0001
#define LEFT_SIDE       0x0004
#define RIGHT_SIDE      0x0008
#define SIDE_VIEW       0x0010
#define ZERO_RANGE      0x0020
#define ONE_RANGE       0x0040
#define TOP_ZONE        0x0100
#define BOTTOM_ZONE     0x0200

#define FLIPH 1
#define TWO_DIRECTIONS 2
#define BACK_VIEW 8
#define COERCED_VIEW 128

#define WALLIDX_OVERRIDE    0xFFFFFFFF

static int G_mask_xsub;
static int G_mask_ysub;
static int G_Fdir;
extern int CLOCKFREQ;

struct graphics_control gfxctl;

extern struct dungeon_level *dun;

extern struct inst *oinst[NUM_INST];
extern struct clickzone cz[NUM_ZONES];

extern int lua_cz_n;
extern struct clickzone *lua_cz;

extern struct global_data gd;
extern struct inventory_info gii;

extern BITMAP *scr_pages[3];
extern BITMAP *soft_buffer;

extern FILE *errorlog;

struct wallset *haze_base;
struct wallset *cwsa;
unsigned int cwsa_num;
struct wallset *base_active_set;
unsigned int act_tintr;
unsigned int act_tintg;
unsigned int act_tintb;
unsigned int act_tint_intensity;

extern struct condition_desc *i_conds;
extern struct condition_desc *p_conds;

extern struct wallset **ws_tbl;

static struct animap *Override_Bmp;

int MONSTERXBASE;

// for renderer hacks
int frontsquarehack;
int sideobshack;
int Psquarehack;
static unsigned int g_i_cztrk;

// ppos hack
extern int PPis_here[4];

int valid_viewangle(int cur_face_dir, unsigned char viewangle) {
    unsigned char dirbyte = (1 << ((cur_face_dir + 2) % 4));   
      
    if (viewangle & dirbyte)
        return 1;
        
    return 0;
}

int calcymodf(int ymodf, int sm, int vsm) {
    if (sm != vsm)
        ymodf -= 42;

    if (vsm == 1)
        ymodf = (ymodf*3)/5;
    else if (vsm == 4)
        ymodf = (ymodf*27)/40;
    else if (sm == 2)
        ymodf = (ymodf*2)/5;
        
    return ymodf;
}

struct wallset *choosewallset(unsigned char wsa[], int odir) {

    if (wsa[odir])
        return ws_tbl[wsa[odir] - 1];
    else if (wsa[DIR_CENTER])
        return ws_tbl[wsa[DIR_CENTER] - 1];
    else
        return base_active_set;
}

// slower but sophisticated version
unsigned long fancy_darkdun(unsigned long src, unsigned long dst, unsigned long n) 
{
    static unsigned long r, g, b;
    
    r = getr(dst);
    g = getg(dst);
    b = getb(dst);
    
    if (r > n) r -= n; else r = 0;
    if (g > n) g -= n; else g = 0; 
    if (b > n) b -= n; else b = 0;
         
    return makecol(r, g, b); 
}

void block_glowmask(int glow, BITMAP *db, int tx, int ty, int hasalpha) {
    int powerpink = makecol(255, 0, 255);
    int ix, iy;
    int gmx, gmy;
    
    if (!glow && gd.glowmask == NULL)
        return;
    if (db == NULL)
        return;
        
    if (gd.glowmask == NULL)
        gd.glowmask = dsbcalloc(VIEWPORTW * VIEWPORTH, sizeof(int));
        
    gmx = G_mask_xsub;
    gmy = G_mask_ysub;
    
    for (iy=0;iy<db->h;iy++) {
        if (iy+ty-gmy < 0) continue;
        if (iy+ty-gmy >= VIEWPORTH) break;
        for (ix=0;ix<db->w;ix++) {
            int px;
            int px_a = 255;
                        
            if (ix+tx-gmx < 0) continue;
            if (ix+tx-gmx >= VIEWPORTW) break;

            px = dsbgetpixel(db, ix, iy);
            if (hasalpha) {
                px_a = geta(px);
            }
                
            if (px != powerpink && px_a != 0) {
                int targ_val = (tx+ix - gmx) + (ty+iy - gmy)*VIEWPORTW;
                
                if (glow) {
                    int gtv = gd.glowmask[targ_val] + px_a;
                    if (gtv > 255) gtv = 255;
                    gd.glowmask[targ_val] = gtv;
                } else {
                    int gtv = gd.glowmask[targ_val] - px_a;
                    if (gtv < 0) gtv = 0;
                    gd.glowmask[targ_val] = gtv;                    
                }
            }
        }
    } 
}

void make_cz(int cznum, int mx, int my, int w, int h, int inum) {
    cz[cznum].x = mx;
    cz[cznum].y = my;
    cz[cznum].w = w;
    cz[cznum].h = h;
    cz[cznum].inst = inum;
}                                           

void create_composite(BITMAP **sx, int *us) {
    if (*us != 2) {
        BITMAP *sub_b = *sx;
        BITMAP *compo;
        compo = create_bitmap(sub_b->w, sub_b->h);
        blit(sub_b, compo, 0, 0, 0, 0, sub_b->w, sub_b->h);
        if (*us)
            destroy_bitmap(sub_b);
        *sx = compo;                
        *us = 2;
    }
}

int render_shaded_object(int sm, BITMAP *scx, BITMAP *sub_b,
    int mx, int my, int tf, int flip, int tws, int alpha,
    struct inst *t_inst, int useglow)
{
    int used_sub = 0;
    int sx, sy;
    BITMAP *ybmp;
    int tr = act_tintr;
    int tg = act_tintg;
    int tb = act_tintb;
    int dtf;
    
    if (useglow) {
        tf = 0;
        tws = 0;
    }
    
    // screen x and y may be different from where we're
    // actually drawing (if sub bmps are used)
    sx = mx;
    sy = my;
    
    if (alpha) {
        mx = 0;
        my = 0;
        ybmp = create_bitmap_ex(32, sub_b->w, sub_b->h);
    } else
        ybmp = scx;
        
    if (t_inst && t_inst->tint && t_inst->tint_intensity) {
        tr = getr(t_inst->tint);
        tg = getg(t_inst->tint);
        tb = getb(t_inst->tint);
        tws = t_inst->tint_intensity;
        tf = 0;      
    }

    // use scratchsheet to blit flipped sprite
    if (flip) {
        BITMAP *base_b = sub_b;
        
        sub_b = create_bitmap(base_b->w, base_b->h);
        clear_to_color(sub_b, makecol(255, 0, 255));
        draw_sprite_h_flip(sub_b, base_b, 0, 0);        
        used_sub = 1;
    }  
    
    //setup dtf
    if (tf > 0) {
        dtf = tf * 2;
        if (dtf+tws > 255) {
            dtf = (255-tws);
        }
        if (tf+tws > 255) {
            tf = 255-tws;
        }
    } else
        dtf = 0;
    
    if (sm == 1 && (tf+tws) > 0) {
        set_trans_blender(tr, tg, tb, 255);
        draw_lit_sprite(ybmp, sub_b, mx, my, tf+tws);
        if (alpha)
            copy_alpha_channel(sub_b, ybmp);
        else
            block_glowmask(useglow, sub_b, sx, sy, alpha);
    } else if (sm == 2 && (dtf+tws) > 0) {
        set_trans_blender(tr, tg, tb, 255);
        draw_lit_sprite(ybmp, sub_b, mx, my, dtf+tws);
        if (alpha)
            copy_alpha_channel(sub_b, ybmp);
        else
            block_glowmask(useglow, sub_b, sx, sy, alpha);
    } else {
        if (tws) {
            set_trans_blender(tr, tg, tb, 255);
            draw_lit_sprite(ybmp, sub_b, mx, my, tws);
            if (alpha)
                copy_alpha_channel(sub_b, ybmp);
            else
                block_glowmask(useglow, sub_b, sx, sy, alpha);
        } else {    
            if (alpha) {
                destroy_bitmap(ybmp);
                ybmp = scx;
                set_alpha_blender();
                draw_trans_sprite(scx, sub_b, sx, sy);
                block_glowmask(useglow, sub_b, sx, sy, alpha);
            } else {
                draw_sprite(scx, sub_b, mx, my);
                block_glowmask(useglow, sub_b, sx, sy, alpha);
            }
        }
    }
    
    if (ybmp != scx) {
        set_alpha_blender();
        draw_trans_sprite(scx, ybmp, sx, sy);
        block_glowmask(useglow, ybmp, sx, sy, alpha);
        destroy_bitmap(ybmp);
    }

    if (used_sub)
        destroy_bitmap(sub_b);
        
    return(0);
}

void long_tweak_tweaks(int sm, int *p_xtweak, int *p_ytweak) {
    int xtweak = *p_xtweak;
    int ytweak = *p_ytweak;

    if (sm == 1) {
        xtweak = (xtweak*25)/36;
        ytweak = (ytweak*13)/18;
    } else if (sm == 2) {
        xtweak = (xtweak*29)/64;
        ytweak = (ytweak*31)/64;
    }

    *p_xtweak = xtweak;
    *p_ytweak = ytweak;
}

void tweak_tweaks(int sm, int *p_xtweak, int *p_ytweak) {
    int xtweak = *p_xtweak;
    int ytweak = *p_ytweak;
    
    if (sm == 1) {
        xtweak = (xtweak*25)/36;
        ytweak = (ytweak*12)/18;
    } else if (sm == 2) {
        xtweak = (xtweak*29)/64;
        ytweak = (ytweak*31)/64;
    }
    
    *p_xtweak = xtweak;
    *p_ytweak = ytweak;
}

int draw_gfxicon(BITMAP *scx, struct animap *ani, int mx, int my, int lit,
    struct inst *p_inst)
{;
    return(draw_gfxicon_af(scx, ani, mx, my, lit, p_inst, 0));
}

int draw_gfxicon_af(BITMAP *scx, struct animap *ani, int mx, int my, int lit,
    struct inst *p_inst, int alphafix) 
{
    BITMAP *s_bmp;
    int des_sub = 0;
    int rv = 0;
    
    if (!ani) {
        return(0);
    }
    
    lod_use(ani);
    if (ani->numframes > 1) {
        if (p_inst)
            s_bmp = fz_animap_subframe(ani, p_inst);
        else
            s_bmp = animap_subframe(ani, 0);
    } else
        s_bmp = ani->b;
        
    mx += ani->xoff;
    my += ani->yoff;
    
    if (ani->flags & AM_256COLOR)
        select_palette((RGB*)ani->pal);
    
    if (lit > 0) {
        int skip_rest = 0;
        BITMAP *d_s_bmp = s_bmp;
        BITMAP *ybmp = NULL;
        
        set_trans_blender(0, 0, 0, 255);
            
        if (lit == 2) {
            if (ani->flags & AM_HASALPHA) {
                ybmp = create_bitmap_ex(32, s_bmp->w, s_bmp->h);
                clear_to_color(ybmp, 0);
                copy_alpha_channel(s_bmp, ybmp);
                d_s_bmp = ybmp;
                set_dsb_fixed_alpha_blender(alphafix);
                draw_trans_sprite(scx, ybmp, mx, my);
            } else {
                draw_lit_sprite(scx, d_s_bmp, mx, my, 255);
            }
            
            skip_rest = 1;   
        } else {
            if (ani->flags & AM_HASALPHA) {
                ybmp = create_bitmap_ex(32, s_bmp->w, s_bmp->h);
                draw_lit_sprite(ybmp, s_bmp, 0, 0, 192);
                copy_alpha_channel(s_bmp, ybmp);
                d_s_bmp = ybmp;
                set_dsb_fixed_alpha_blender(alphafix);
                draw_trans_sprite(scx, ybmp, mx+4, my+4);
            } else {
                draw_lit_sprite(scx, d_s_bmp, mx+4, my+4, 192);
            }  
        }
        
        if (ybmp)
            destroy_bitmap(ybmp);
            
        if (skip_rest) {
            goto FINISH_GFXICON; 
        }
    }
    
    if (lit == -1) {
        if (ani->flags & AM_HASALPHA) {
            BITMAP *ybmp;

            ybmp = create_bitmap_ex(32, s_bmp->w, s_bmp->h);
            clear_to_color(ybmp, 0);
            draw_sprite_h_flip(ybmp, s_bmp, 0, 0);
            set_dsb_fixed_alpha_blender(alphafix);
            draw_trans_sprite(scx, ybmp, mx, my);
            destroy_bitmap(ybmp);
            rv = 1;
        } else
            draw_sprite_h_flip(scx, s_bmp, mx, my);
    } else {
        if (ani->flags & AM_HASALPHA) {
            
            set_dsb_fixed_alpha_blender(alphafix);            
            draw_trans_sprite(scx, s_bmp, mx, my);
            rv = 1;
        } else
            draw_sprite(scx, s_bmp, mx, my);
    }
    
    FINISH_GFXICON:
    if (des_sub)
        destroy_bitmap(s_bmp);
        
    return(rv);
}

int draw_objicon(BITMAP *scx, unsigned int inst, int mx, int my, int lit) {
    struct inst *p_inst = oinst[inst];
    struct obj_arch *p_arch = Arch(p_inst->arch);
    struct animap *an;
    int r = 0;
     
    if ((p_inst->gfxflags & G_ALT_ICON) && p_arch->alt_icon) {
        an = p_arch->alt_icon;
    } else
        an = p_arch->icon;
        
    r = draw_gfxicon(scx, an, mx, my, lit, p_inst);
    
    return(r);
}

int draw_a_portrait(int softmode, BITMAP *scx, int champnum, 
    struct obj_arch *p_obj_arch, struct inst *p_obj_inst,
    int mx, int my) 
{
    struct animap *in;
    BITMAP *who_inside;
    int ix, iy, v_x, v_y;
    int des_sub = 0;
    int need_soft = 0;
     
    lod_use(p_obj_arch->inview[2]);
    if (p_obj_arch->inview[2]->numframes > 1)
        return(0);

    ix = p_obj_arch->inview[2]->w;
    iy = p_obj_arch->inview[2]->b->h;
    v_x = p_obj_arch->inview[2]->xoff;
    v_y = p_obj_arch->inview[2]->yoff;
                         
    in = gd.champs[champnum-1].portrait;
    if (in == NULL)
        return(0);
        
    lod_use(in);
    if (in->flags & AM_HASALPHA) {
        need_soft = 1;
        if (!softmode)
            return(1);
    }
    
    who_inside = animap_subframe(in, 0);
    
    if (need_soft) {
        BITMAP *ybmp;
        ybmp = create_bitmap(ix, iy);
        blit(who_inside, ybmp, 0, 0, 0, 0, ix, iy);
        set_alpha_blender();
        draw_trans_sprite(scx, ybmp, mx+v_x, my+v_y);
        destroy_bitmap(ybmp);
    } else
        masked_blit(who_inside, scx, 0, 0, mx+v_x, my+v_y, ix, iy);
    
    return(need_soft);
}

int leader_color_type(int cnum, int here) {
    int cl;

    if (gd.leader == cnum)
        cl = 3;
    else if (IsNotDead(cnum)) {
        if (here)
            cl = 2;
        else
            cl = 1;
    } else
        cl = 0;
        
    return cl;
}

/* -- NO LONGER USED
// always done with a released bitmap, so it must be acquired to draw a bitmap
// (not really that important now that software rendering is almost always done)
void draw_champbars(BITMAP *scx, int cnum, int rchamp, int scale_f) {
    int ibar = 0;
    
    onstack("draw_champbars");
    
    for (ibar=0;ibar<3;++ibar) {
        int hmod = 0;
        int s = gd.champs[rchamp].bar[ibar];
        int mx = gd.champs[rchamp].maxbar[ibar];
        
        if (s == 0 && (s <= mx))
            continue;
            
        // mana that is less than 10 (1 unit) shows up as 0
        if (ibar >= 2 && s < 10)
            continue;
            
        if (mx == 0 && s > mx)
            mx = s;
        
        hmod = 26 - ((26*(s/10)) / (mx/10));
        
        // always show a little even if you're low
        if (s > 0 && hmod > 24)
            hmod = 24;
            
        // keep the bar from going over the roof
        if (hmod < 0)
            hmod = 0;
        
        if (hmod<=24) {
            int dest_x = scale_f*cnum+92+14*ibar + gfxctl.port_x;
            if (gd.playerbitmap[cnum]) {
                BITMAP *bbmp;
                lod_use(gd.playerbitmap[cnum]);
                bbmp = animap_subframe(gd.playerbitmap[cnum], 0);
                acquire_bitmap(scx);
                blit(bbmp, scx, 0, hmod*2, dest_x, 0 + 4+(hmod*2), 8, (bbmp->h-(hmod*2)));
                release_bitmap(scx);
            } else {
                rectfill(scx, dest_x, 0 + 4+(hmod*2), dest_x + 7, 0+ 53, gd.playercol[cnum]);
            }
        }    
    }
    
    VOIDRETURN();
}
*/

/* -- no longer needed
void draw_champname(BITMAP *scx, int cnum, int rchamp, int scale_f, int here) {
    int cl;
    
    onstack("draw_champname");
    
    cl = gd.name_draw_col[leader_color_type(cnum, here)];
    
    textout_ex(scx, font, gd.champs[rchamp].name,
        gfxctl.port_x + (scale_f*cnum)+2, gfxctl.port_y + 2, cl, -1);
    
    VOIDRETURN();
}
*/

int draw_i_pl_ex(int softmode, BITMAP *scx, int pcidx, int mx, int my,
    int o, int makezone)
{
    struct champion *me;
    int islot;
    int rv = 0;
    int lx, ly;
    
    onstack("draw_i_pl_ex");
    
    me = &(gd.champs[pcidx]);
    islot = me->inv[o];
    
    
    if (mx && my) {
        lx = mx;
        ly = my;
    } else {
        lx = gd.vxo + gii.inv[o].x;
        ly = gd.vyo + gd.vyo_off + gii.inv[o].y;
    }
     
    if (!lx && !ly) {
        // this zone has nothing in it
        RETURN(softmode);
    }
    
    if (o < gii.max_injury && me->injury[o]) {
        lod_use(gii.hurt_box);
        draw_gfxicon(scx, gii.hurt_box, lx, ly, 0, NULL);
    }
    
    if (islot) {
        rv = draw_objicon(scx, islot, lx, ly, 0);
    } else {
        struct animap *img = gii.inv[o].img;
        struct animap *inj_img = gii.inv[o].img_ex;
        
        if (img == NULL) goto MAKETHECZ;
        
        if (inj_img != NULL && me->injury[o]) {
            rv = draw_gfxicon(scx, inj_img, lx, ly, 0, 0);
        } else 
            rv = draw_gfxicon(scx, img, lx, ly, 0, 0);
    }
    
    MAKETHECZ:
    if (makezone)
        make_cz(makezone, lx, ly, 32, 32, islot);
    
    RETURN(rv);
}

void emptyzone(int cznum, int mod, int cx, int cy, int w, int h) {
    
    if (!cznum || cz[cznum+mod].w)
        return;
        
    make_cz(cznum+mod, cx-(w/2), cy-h, w, h, 0);            
}
/*
void draw_damage(BITMAP *scx, int pn, int is_port) {
    struct animap *a_damage;
    BITMAP *b_damage;
    BITMAP *bp_damage;
    struct animap img_part;
    int damount, dtype;
    int wt = makecol(255, 255, 255);
    int dtn;
    int ipn;
    int iw;
    int xm, ym;
    
    onstack("draw_damage");
    
    damount = gd.showdam[pn];
    dtype = gd.showdam_t[pn];
    
    if (is_port) {
        a_damage = gd.simg.full_dmg;
        dtn = 58;
        ipn = 14;
        iw = 64;
        xm = gfxctl.port_x + 48;
        ym = gfxctl.port_y + 24; 
    } else {
        a_damage = gd.simg.bar_dmg;
        // vertical offset by damage type (and height)
        dtn = 14;
        // offset after player number offset   
        ipn = 0;
        // width of bitmap
        iw = 86;
        // text pritn locations
        xm = gfxctl.port_x + 44;
        ym = gfxctl.port_y + 2;
    }
    
    b_damage = animap_subframe(a_damage, 0);
    bp_damage = create_sub_bitmap(b_damage, 0, dtype*dtn, iw, dtn);    
    memset(&img_part, 0, sizeof(struct animap));
    img_part.numframes = 1;
    img_part.b = bp_damage;
    img_part.w = bp_damage->w;
    img_part.flags = a_damage->flags;
    draw_gfxicon(scx, &img_part, (138*pn)+ipn, 20, 0, NULL);
    destroy_bitmap(bp_damage);
    
    textprintf_centre_ex(scx, font, 138*pn+xm, ym, wt, -1, "%d", damount);

    VOIDRETURN();
}
*/

// render overlays to champion panel
void draw_individual_conditions(BITMAP *scx, int cn, int a_cn) {
    struct inst fake_obj;
    struct inst *p_fake_obj;
    int cc;
    
    onstack("draw_individual_conditions");
    
    for(cc=0;cc<gd.num_conds;++cc) {
        if (gd.champs[(a_cn-1)].condition[cc] && i_conds[cc].pbmp) {
            int animtimer = gd.champs[(a_cn-1)].cond_animtimer[cc];
            
            p_fake_obj = NULL;
            if (animtimer != FRAMECOUNTER_NORUN) {
                memset(&fake_obj, 0, sizeof(struct inst));
                fake_obj.frame = animtimer;
                
                p_fake_obj = &fake_obj;
            }
                
            draw_gfxicon(scx, i_conds[cc].pbmp, 0, 0, 0, p_fake_obj);
        }
    }
    
    VOIDRETURN();
}

// render viewport overlays
void draw_party_conditions(BITMAP *scx) {
    struct inst fake_obj;
    struct inst *p_fake_obj;
    int cc;
    
    onstack("draw_party_conditions");
    
    if (*gd.gl_viewstate == VIEWSTATE_DUNGEON) {
        for(cc=0;cc<gd.num_pconds;++cc) {
            if (gd.partycond[cc] && p_conds[cc].pbmp) {
                int animtimer = gd.partycond_animtimer[cc];
                int rx = gd.vxo;
                int ry = gd.vyo + gd.vyo_off;
                
                p_fake_obj = NULL;
                if (animtimer != FRAMECOUNTER_NORUN) {
                    memset(&fake_obj, 0, sizeof(struct inst));
                    fake_obj.frame = animtimer;
                    
                    p_fake_obj = &fake_obj;
                }
                
                draw_gfxicon(scx, p_conds[cc].pbmp, rx, ry, 0, p_fake_obj);
            }
        }
    }
    
    VOIDRETURN();
}

int top_part_draw(BITMAP *b_t, BITMAP *scx, int top_alpha, int cn) {
    int rv = 0;
    onstack("top_part_draw");
    if (b_t) {
        if (top_alpha) {
            BITMAP *ybmp;
            ybmp = create_sub_bitmap(b_t, 0, 0, 134, 58);
            set_alpha_blender();
            draw_trans_sprite(scx, ybmp, gfxctl.port_x + 138*cn, gfxctl.port_y); 
            destroy_bitmap(ybmp);
            rv = 1;
        } else {
            masked_blit(b_t, scx, 0, 0, gfxctl.port_x + 138*cn, gfxctl.port_y, 134, 58);    
        }
    }    
    RETURN(rv);
} 

int draw_champions(int softmode, BITMAP *scx, int show_port) {
    int cn = 0;
    int rv = 0;
    int cur_x, cur_y;
    
    onstack("draw_champions");
    
    // determine who is in -this- party
    mparty_who_is_here();
    
    acquire_bitmap(scx);
    
    for (cn=0;cn<4;cn++) {
        sys_rend *cp = &(gfxctl.ppos_r[cn]);
        cp->flags = 0;
        cclear_m_cz(gfxctl.ppos_r[cn].cz, gfxctl.ppos_r[cn].cz_n);
        if (gd.party[cn]) {
            int extrax = 0;
            int extray = 0;
            struct animap *a_c_topscr;
            BITMAP *b_c_topscr;
            unsigned int a_cn = gd.party[cn];
            struct champion *me = &(gd.champs[(a_cn-1)]);
            int cstate;
            
            if (show_port == a_cn) {
                cstate = TOP_PORT;
                rv = 1;
            } else if (gd.champs[(a_cn-1)].bar[0] > 0)
                cstate = TOP_HANDS;
            else
                cstate = TOP_DEAD;
                
            a_c_topscr = gii.topscr[cstate];
            if (me->topscr_override[cstate])
                a_c_topscr = me->topscr_override[cstate];                    
            lod_use(a_c_topscr);
            b_c_topscr = animap_subframe(a_c_topscr, 0);
            
            cp->flags |= SRF_ACTIVE; 
            cp->sizex = b_c_topscr->w;
            cp->sizey = b_c_topscr->h;
            if (!cp->rtarg ||
                cp->sizex != cp->rtarg->b->w ||
                cp->sizey != cp->rtarg->b->h)
            {
                if (cp->rtarg)
                    destroy_animap(cp->rtarg);
                    
                cp->rtarg = dsbmalloc(sizeof(struct animap));
                memset(cp->rtarg, 0, sizeof(struct animap));
                               
                cp->rtarg->b = create_bitmap(cp->sizex, cp->sizey);
                cp->rtarg->w = cp->sizex;
                cp->rtarg->flags = a_c_topscr->flags | AM_VIRTUAL;             
            }
            blit(b_c_topscr, cp->rtarg->b, 0, 0, 0, 0, cp->sizex, cp->sizey);
            if (cp->rtarg->flags & AM_HASALPHA)
                rv = 1;
            if (rv && !softmode)
                goto FRAME_DROPPER;           
            
            call_lua_top_renderer(cp->rtarg, cstate, cn, a_cn, PPis_here[cn], me->port_name, &extrax, &extray);
            cur_x = gfxctl.port_x + extrax;
            cur_y = gfxctl.port_y + extray;
            cp->rtx = cur_x;
            cp->rty = cur_y;
                                   
            draw_individual_conditions(cp->rtarg->b, cn, a_cn);
            
            if (gd.showdam_ttl[cn])
                call_lua_damage_renderer(cp->rtarg, cstate, cn, a_cn);

            if (cp->rtarg->flags & AM_HASALPHA) {
                set_alpha_blender();
                draw_trans_sprite(scx, cp->rtarg->b, cur_x, cur_y);
            } else {                
                draw_sprite(scx, cp->rtarg->b, cur_x, cur_y);
            }
            make_cz(40+cn, cur_x, cur_y, cp->sizex, cp->sizey, cn);
        }
    }  
    
    ////////////        
    /* THIS HAS ALL BEEN MOVED TO LUA CODE
    ////////////
            for(i_t=0;i_t<MAX_TOP_SCR;i_t++) {
                struct animap *a_c_topscr;
                
                b_topscr[i_t] = NULL;
                top_alpha[i_t] = 0; 
                
                a_c_topscr = gii.topscr[i_t];
                if (me->topscr_override[i_t])
                    a_c_topscr = me->topscr_override[i_t];                    
                lod_use(a_c_topscr);
                    
                if (a_c_topscr) {
                    top_alpha[i_t] = !!(a_c_topscr->flags & AM_HASALPHA);
                    b_topscr[i_t] = animap_subframe(a_c_topscr, 0);
                }  
            }
            
            if (show_port == a_cn) {
                struct animap *i_port;
                BITMAP *port;

                // no mode check because portraits are always drawn
                // on a mode 1 screen
                rv = 1;
                top_part_draw(b_topscr[TOP_PORT], scx, top_alpha[TOP_PORT], cn);
                
                i_port = me->portrait;
                lod_use(i_port);
                port = animap_subframe(i_port, 0);

                if (i_port->flags & AM_HASALPHA) {
                    BITMAP *ybmp;
                    ybmp = create_sub_bitmap(port, 0, 0, 64, 58);
                    set_alpha_blender();
                    draw_trans_sprite(scx, ybmp, gfxctl.port_x + (138*cn)+14, gfxctl.port_y);
                    destroy_bitmap(ybmp);
                } else
                    masked_blit(port, scx, 0, 0, gfxctl.port_x + (138*cn)+14, gfxctl.port_y, 64, 58);
                
                release_bitmap(scx);
                draw_champbars(scx, cn, (a_cn-1), 138);
                acquire_bitmap(scx);
                
                draw_conditions(scx, cn, a_cn);
                
                if (gd.showdam[cn])
                    draw_damage(scx, cn, 1);
                    
                make_cz(48+cn, gfxctl.port_x + 138*cn, gfxctl.port_y, 85, 58, cn);
                    
            } else {
                if (gd.champs[(a_cn-1)].bar[0] > 0) {
                    int r2v = 0;
                    int here = PPis_here[cn];
                    
                    rv = top_part_draw(b_topscr[TOP_HANDS], scx, top_alpha[TOP_HANDS], cn);
                    if (rv && !softmode)
                        goto FRAME_DROPPER;
                    
                    release_bitmap(scx);
                    draw_champbars(scx, cn, (a_cn-1), 138);
                    acquire_bitmap(scx);
                
                    // draw left hand with injury, selection etc.
                    rv = draw_i_pl_ex(softmode, scx, (a_cn-1),
                        gfxctl.port_x + 8+138*cn, gfxctl.port_y + 20, gii.lslot[cn], here*(40+cn));                       
                    if (rv && !softmode)
                        goto FRAME_DROPPER;                       
                    if (gd.who_method == (cn+1) && gd.method_loc == gii.lslot[cn])
                        draw_gfxicon(scx, gd.simg.sel_box, gfxctl.port_x + 6+138*cn, gfxctl.port_y + 18, 0, NULL);
                     
                    // draw right hand with injury, selection etc.   
                    r2v = draw_i_pl_ex(softmode, scx, (a_cn-1),
                        gfxctl.port_x + 48+138*cn, gfxctl.port_y + 20, gii.rslot[cn], here*(44+cn));                       
                    if (r2v) {
                        rv = 1;
                        if (!softmode)
                            goto FRAME_DROPPER;
                    }                      
                    if (gd.who_method == (cn+1) && gd.method_loc == gii.rslot[cn])
                        draw_gfxicon(scx, gd.simg.sel_box, gfxctl.port_x + 46+138*cn, gfxctl.port_y + 18, 0, NULL);
                      
                    make_cz(48+cn, gfxctl.port_x + 138*cn, gfxctl.port_y, 85, 13, cn);
                    make_cz(52+cn, gfxctl.port_x + 138*cn+88, gfxctl.port_y, 45, 57, cn);
                
                } else {
                    rv = top_part_draw(b_topscr[TOP_DEAD], scx, top_alpha[TOP_DEAD], cn);
                    if (rv && !softmode)
                        goto FRAME_DROPPER;
                }
                    
                draw_individual_conditions(scx, cn, a_cn);
                    
                if (gd.showdam[cn])
                    draw_damage(scx, cn, 0);
                else
                    draw_champname(scx, cn, (a_cn-1), 138, PPis_here[cn]);
            }
        }
    }
    */
    
    draw_party_conditions(scx);  
    
    FRAME_DROPPER:

    release_bitmap(scx);
    RETURN(rv);
}

struct animap *get_monster_view(struct inst *p_inst,
    struct obj_arch *p_arch, int sm, int p_face, int tflags, int *info_flags)
{
    struct animap *amap = NULL;
    int bl = (p_arch->rhack & RHACK_ONLY_DEFINED);
    int pif = p_inst->facedir;
    
    onstack("get_monster_view");

    if (p_arch->arch_flags & ARFLAG_INSTTURN) {
        if (p_inst->gfxflags & G_ATTACKING) {
            int i_caa = p_inst->ai->attack_anim;
            amap = do_dynamic_scale(sm, p_arch, V_ATTVIEW + i_caa, bl);
            RETURN(amap);
        }
    }
    
    if (pif == p_face) {
        amap = do_dynamic_scale(sm, p_arch, V_OUTVIEW, bl);
        *info_flags |= BACK_VIEW;
    
    } else if ((pif+2)%4 == p_face) {
        if (p_inst->gfxflags & G_ATTACKING) {
            int i_caa = p_inst->ai->attack_anim;
            amap = do_dynamic_scale(sm, p_arch, V_ATTVIEW + i_caa, bl);
        } else
            amap = do_dynamic_scale(sm, p_arch, V_DVIEW, bl);
    
    } else {
        *info_flags |= SIDE_VIEW;
        
        if ((pif+1)%4 == p_face) {
            amap = do_dynamic_scale(sm, p_arch, V_EXTVIEW, bl);
            if (!amap)
                *info_flags |= FLIPH;
        }
        
        if (!amap)
            amap = do_dynamic_scale(sm, p_arch, V_SIDEVIEW, bl);

    }
    
    RETURN(amap);    
}

struct animap *get_fly_view(int o_type, struct obj_arch *p_obj_arch, 
    struct inst *p_inst, int sm, int vsm, int p_face, int flags, int *flip) 
{
    struct animap *amap = NULL;
    int tp = p_inst->tile;
    int bl = (p_obj_arch->rhack & RHACK_ONLY_DEFINED);
    
    // flying away
    if (p_inst->facedir == p_face) {
        amap = do_dynamic_scale(vsm, p_obj_arch, V_OUTVIEW, bl);
                    
        if (flags & LEFT_SIDE)
            *flip = 1;
        else if (flags & RIGHT_SIDE)
            *flip = 0;
        else if (tp == p_face || tp == (p_face+3)%4) {
            *flip = 1;
        }

    // flying toward
    } else if ((p_inst->facedir+2)%4 == p_face) {
        amap = do_dynamic_scale(vsm, p_obj_arch, V_INVIEW, bl);
        
        if (flags & LEFT_SIDE)
            *flip = 1;
        else if (flags & RIGHT_SIDE)
            *flip = 0;
        else if (tp == p_face || tp == (p_face+3)%4) {
            *flip = 1;
        }
         
        // if we're cheating and using the same front/back view
        // then flip the front view so it looks right   
        if (p_obj_arch->inview[0] &&
            p_obj_arch->inview[0] == p_obj_arch->outview[0])
        {
            *flip = !(*flip);
        }
    
    // flying side    
    } else {
        amap = do_dynamic_scale(vsm, p_obj_arch, V_SIDEVIEW, bl);
        
        if (p_inst->facedir == (p_face+1)%4)
            *flip = 1;   
    } 
    
    if (!amap) {
        amap = do_dynamic_scale(vsm, p_obj_arch, V_DVIEW, bl);
        *flip = 0;
    }
    
    return amap;
}

struct animap *get_rend_view(int o_type, struct obj_arch *p_obj_arch, 
    int flag, int sm, int vsm, int *p_xhack, int *info_flags)
{
    struct animap *amap = NULL;
    int bl;
    
    onstack("get_rend_view");
    stackbackc('(');
    v_onstack(p_obj_arch->luaname);
    addstackc(')');
    
    bl = (p_obj_arch->rhack & RHACK_ONLY_DEFINED);
    
    if (o_type == OBJTYPE_UPRIGHT || o_type == OBJTYPE_FLOORITEM) {
        
        if (flag & ZERO_RANGE) {
            amap = p_obj_arch->inview[0];
            
            if (flag & SIDE_VIEW) {
                struct animap *tmap;
                tmap = p_obj_arch->inview[1];
                
                if (tmap)
                    amap = tmap;
                    
                if (bl && !tmap)
                    amap = NULL;
            }

            if (p_obj_arch->rhack & RHACK_DOORFRAME) {
                if (frontsquarehack && !sideobshack) {
                    lod_use(amap);
                    RETURN(amap);
                } else {
                    RETURN(NULL);
                }
            } else { 
                lod_use(amap);
                RETURN(amap);
            }
        }
    }
    
    if (flag & SIDE_VIEW) {
             
        // reversed side
        if (o_type == OBJTYPE_WALLITEM) {
            if (flag & ZERO_RANGE) {

                amap = p_obj_arch->inview[0];

                if (flag & SUBWIDTH) {
                    struct animap *tmap = p_obj_arch->inview[1];
                    if (tmap) {
                        amap = tmap;
                        *info_flags |= TWO_DIRECTIONS;
                    }
                }

                lod_use(amap);
                RETURN(amap);
            }
            
            if (flag & SUBWIDTH) {   
                amap = do_dynamic_scale(sm, p_obj_arch, V_OUTVIEW, bl);
                
                if (amap)
                    *info_flags |= TWO_DIRECTIONS;
            }
        }
        
        if ((p_obj_arch->rhack & RHACK_STAIRS) && 
            !frontsquarehack)
        {
            amap = do_dynamic_scale(sm, p_obj_arch, V_OUTVIEW, bl);
        }        
        
        if (amap == NULL)
            amap = do_dynamic_scale(vsm, p_obj_arch, V_SIDEVIEW, bl);
    }
    
    if (amap == NULL) {
        v_onstack("do_dynamic_scale(V_DVIEW)");
        amap = do_dynamic_scale(vsm, p_obj_arch, V_DVIEW, bl);
        v_upstack();
        
        if (amap && (flag & SIDE_VIEW) && p_xhack != NULL) {
            *info_flags |= COERCED_VIEW;

            *p_xhack = 0;
            if (sm == 0)
                *p_xhack = -3*(amap->w/4);
                
            // shift dynamic objects in a bit to match perspective
            else if (sm == 1)
                *p_xhack = 40 - amap->w/2;
            else if (sm == 2)
                *p_xhack = 90 - amap->w/2;
        }
    }
    
    RETURN(amap);
}

int wall_animap_blit(int softmode, struct wallset *ws_cur,
    BITMAP *scx, int wallidx,
    int tx, int ty, int gfr_flag, int flags,
    int xcrop_l, int xcrop_r, int cache_entry)
{
    BITMAP *sbmp;
    int used_scratch = 0;
    int xv, yv;
    struct animap *amap;
    int retval = 0;
    int alpha = 0;
    int no_flip_bmp;

    if (wallidx == WALLIDX_OVERRIDE)
        amap = Override_Bmp;
    else if (wallidx >= 0xFF)
        amap = ws_cur->xwall[wallidx - 0xFF];
    else
        amap = ws_cur->wall[wallidx];
    
    no_flip_bmp = (wallidx >= WALLBMP_FRONT1 && wallidx <= WALLBMP_FRONT3);
        
    if (flags & FLIPH) {
        if (wallidx == WALLBMP_FLOOR) {
            if (ws_cur->alt_floor) {
                flags &= ~(FLIPH);
                amap = ws_cur->alt_floor;
            } else if (!(ws_cur->flipflags & OV_FLOOR))
                flags &= ~(FLIPH);
        } else if (wallidx == WALLBMP_ROOF) {
            if (ws_cur->alt_roof) {
                flags &= ~(FLIPH);
                amap = ws_cur->alt_roof;
            } else if (!(ws_cur->flipflags & OV_ROOF))
                flags &= ~(FLIPH);
        }
    }
    
    if (amap == NULL)
        return retval;
    
    lod_use(amap);
    
    if (gfr_flag == -1) {
        tx = (tx+448) - amap->w;
    }
        
    if (amap->flags & AM_HASALPHA) {
        retval = 1;
        alpha = 1;
        if (!softmode)
            return 1;
    }
        
    // grab the right frame of the animap
    if (amap->numframes > 1) {
        // ignore the "frame" parameter, it's nonsense most of the time
        int fcr = (gd.framecounter / amap->framedelay) % amap->numframes;
        
        sbmp = amap->c_b[fcr];
    } else
        sbmp = amap->b;
        
    if (sbmp == NULL)
        return 0;
    
    if (xcrop_l || xcrop_r) {
        sbmp = create_sub_bitmap(sbmp, xcrop_l, 0,
                sbmp->w - (xcrop_l + xcrop_r), sbmp->h);
        used_scratch = 1;
    }
    
    if ((flags & FLIPH) && !no_flip_bmp)
        xv = tx - amap->xoff;
    else
        xv = tx + amap->xoff;
        
    yv = ty + amap->yoff;
        
    if (flags & FLIPH) {
        if (alpha || gd.glowmask) {
            BITMAP *ybmp;
            
            ybmp = create_bitmap(sbmp->w, sbmp->h);
            clear_to_color(ybmp, makecol(255, 0, 255));
            draw_sprite_h_flip(ybmp, sbmp, 0, 0);
            if (alpha) {
                set_alpha_blender();
                draw_trans_sprite(scx, ybmp, xv, yv);
            } else
                draw_sprite(scx, ybmp, xv, yv);
            block_glowmask(0, ybmp, xv, yv, alpha);
            destroy_bitmap(ybmp);
        } else
            draw_sprite_h_flip(scx, sbmp, xv, yv);
    } else {
        if (alpha) {
            set_alpha_blender();
            draw_trans_sprite(scx, sbmp, xv, yv);
        } else
            draw_sprite(scx, sbmp, xv, yv);
        block_glowmask(0, sbmp, xv, yv, alpha);
    }
        
    if (used_scratch)
        destroy_bitmap(sbmp);
    
    return retval;
}

int cloud_handler(BITMAP *scx, BITMAP *sub_b, struct inst *p_c_inst,
    int sm, int ymodf, int rx, int ry, int xtweak, int ytweak, int tflags, int alpha)
{
    struct obj_arch *p_c_arch;
    int mx, my, sx, sy;
    int cpow;
    BITMAP *c_mod_b;
    int st = 0;
    int cf, flip;
    
    onstack("cloud_handler");
        
    p_c_arch = Arch(p_c_inst->arch);

    cpow = p_c_inst->charge;
    
    if (gd.farcloudhack)
        cpow = (cpow*3) / 5;
    
    st = p_c_arch->sizetweak;
    if (!st) st = 1;
    sx = (sub_b->w*(cpow*st))/64;
    sy = (sub_b->h*(cpow*st))/64;
    c_mod_b = create_bitmap_ex(32, sx, sy);
    
    cf = 48*sm;
        
    flip = !!(p_c_inst->gfxflags & G_FLIP);

    if (cpow == 64) {
        blit(sub_b, c_mod_b, 0, 0, 0, 0, sx, sy);
    } else {   
        DSB_aa_scale_blit(0, sub_b, c_mod_b, 0, 0, sub_b->w, sub_b->h, 0, 0, sx, sy);
    }

    mx = rx + xtweak - (sx / 2);
    
    if (ymodf)
        my = ry + xtweak - ((sy/2) + ymodf);
    else
        my = ry + ytweak - sub_b->h + (sub_b->h - sy)/2;
    
    defer_draw(OBJTYPE_CLOUD, c_mod_b, 1, sm, mx, my, cf, flip, alpha,
        p_c_inst, !!(p_c_arch->rhack & RHACK_NOSHADE));
    
    RETURN(0);    
}

BITMAP *door_decoration_compo(struct inst *p_inst, struct animap *base, BITMAP *base_b,
    struct animap *deco, int sm, BITMAP *compo_base, int cb_has_alpha) 
{
    BITMAP *compo = NULL;
    BITMAP *d_sub_b;
    BITMAP *modified_deco = NULL;
    int cx, cy;
    int xt, yt;
    
    onstack("door_decoration_compo");
    
    xt = deco->xoff;
    yt = deco->yoff;        
            
    if (deco->flags & AM_DYNGEN)
        tweak_tweaks(sm, &xt, &yt);
            
    d_sub_b = fz_animap_subframe(deco, p_inst);
    
    if (compo_base)
        compo = compo_base;
    else {
        compo = create_bitmap_ex(32, base_b->w, base_b->h);
        blit(base_b, compo, 0, 0, 0, 0, base_b->w, base_b->h);
        cb_has_alpha = !!(base->flags & AM_HASALPHA);
    }
    
    cx = ((compo->w/2) - (d_sub_b->w/2)) + xt;
    cy = ((compo->h/2) - (d_sub_b->h/2)) + yt; 
    
    // need to alphaize the base if the deco has alpha and base does not
    // (This is not efficient, but this will probably not happen much)
    if (!cb_has_alpha && (deco->flags & AM_HASALPHA)) {
        int x, y;
        for (y=0;y<compo->h;++y) {
            for (x=0;x<compo->w;++x) {
                int cpx = _getpixel32(compo, x, y);
                cpx = (cpx & 0x00FFFFFF) | 0xFF000000;
                if (cpx == 0xFFFF00FF) {
                    cpx = 0;
                }
                _putpixel32(compo, x, y, cpx);    
            }
        }
        cb_has_alpha = 1;    
    }
    
    // if the deco does not have alpha but the comp base does,
    // then turn the deco's power pink into holes in the comp base  
    // (This is not efficient, but this will probably not happen much)
    if (!(deco->flags & AM_HASALPHA)) {
        int paletted = 0;
        if (deco->flags & AM_256COLOR) {
            paletted = 1;
            select_palette((RGB*)deco->pal);
        }
        if (cb_has_alpha) {
            int x, y;
            modified_deco = create_bitmap_ex(32, d_sub_b->w, d_sub_b->h);
            for(y=0;y<modified_deco->h;++y) {
                for(x=0;x<modified_deco->w;++x) {
                    int palidx;
                    int i_color;
                    if (paletted) {
                        palidx = _getpixel(d_sub_b, x, y);
                        if (palidx == 0) {
                            i_color = 0x00FF00FF;
                        } else {
                            RGB *rgb_color = &(deco->pal[palidx]);
                            i_color = makecol32(rgb_color->r * 4, rgb_color->g * 4, rgb_color->b * 4);
                            i_color = 0xFF000000 | i_color;
                            if (i_color == 0xFFFC00FC)
                                i_color = 0;
                        }
                    } else {
                        i_color = _getpixel32(d_sub_b, x, y);
                        i_color = 0xFF000000 | (i_color & 0x00FFFFFF);
                        if (i_color == 0xFFFF00FF)
                            i_color = 0;
                    }
                    _putpixel32(modified_deco, x, y, i_color);
                }
            }
            d_sub_b = modified_deco;
        }   
    }
            
    draw_sprite(compo, d_sub_b, cx, cy);
    
    if (modified_deco)
        destroy_bitmap(modified_deco);
    
    RETURN(compo);    
}

BITMAP *side_slide_slice(int dtype, struct inst *p_inst, BITMAP *base,
    BITMAP *compobase, int cut, int xm, int alpha)
{
    BITMAP *newcompo;
    BITMAP *drawbase;
    
    onstack("side_slide_slice");
    
    SETUP_COMPO();
    
    if (dtype == DTYPE_LEFT ||
        ((G_Fdir < 2) && (dtype == DTYPE_NORTHWEST)) ||
        ((G_Fdir >= 2) && (dtype == DTYPE_SOUTHEAST)))
    {
        blit(drawbase, newcompo, cut, 0, 0, 0,  drawbase->w - cut, drawbase->h - xm); 
    
    } else {
        
        blit(drawbase, newcompo, 0, 0, cut, 0, 
            drawbase->w - cut, drawbase->h - xm);      
    }
    
    RETURN(newcompo);
}

BITMAP *left_right_slice(BITMAP *base, BITMAP *compobase, int cut, int xm, int alpha) {
    BITMAP *newcompo;
    BITMAP *drawbase;
    
    onstack("left_right_slice");
    
    SETUP_COMPO();
    
    blit(drawbase, newcompo, cut, 0, 0, 0, 
        drawbase->w/2 - cut, drawbase->h - xm);
    
    blit(drawbase, newcompo, drawbase->w/2, 0, drawbase->w/2+cut, 0, 
        drawbase->w/2 - cut, drawbase->h - xm);
        
    if (compobase)
        destroy_bitmap(compobase);
        
    RETURN(newcompo);      
}

BITMAP *magic_dissolve_slice(BITMAP *base, BITMAP *compobase, int crop, int cropmax, int alpha) {
    BITMAP *newcompo;
    BITMAP *drawbase;
    int dis, i_dis;
                        
    onstack("magic_dissolve_slice");
    
    SETUP_COMPO();

    if (cropmax == 32)
        dis = crop * 8;
    else
        dis = (int)(256 * ((double)crop/(double)cropmax));
    
    i_dis = 256 - dis;
    
    set_dissolve_blender(0, 0, 0, i_dis);
    draw_trans_sprite(newcompo, drawbase, 0, 0);

    if (compobase)
        destroy_bitmap(compobase);
        
    RETURN(newcompo);      
}

int draw_door(int softmode, struct inst *p_inst, struct obj_arch *p_arch,
    BITMAP *scx, int rx, int ry, int sm, int tflags)
{
    struct animap *amap = NULL;
    struct animap *d_amap;
    struct animap *s_amap;
    int xhack = 0;
    int info_flags = 0;
    int sm_rv = 0;
    int cf;
    
    if (tflags & ZERO_RANGE)
        return 0;
        
    if ((tflags & SIDE_VIEW) && !frontsquarehack)
        return 0;
    
    onstack("draw_door");
    
    if (tflags & SIDE_VIEW)
        amap = do_dynamic_scale(sm, p_arch, V_SIDEVIEW, 0);
    if (!amap)
        amap = do_dynamic_scale(sm, p_arch, V_DVIEW, 0);
        
    d_amap = do_dynamic_scale(sm, p_arch, V_INVIEW, 0);
    s_amap = do_dynamic_scale(sm, p_arch, V_OUTVIEW, 0);
    
    if (amap) {
        BITMAP *sub_b;
        BITMAP *drawtarg;
        BITMAP *compo = NULL;
        BITMAP *cut_compo = NULL;
        int xtweak = 0;
        int ytweak = 0;
        int mx, my;
        int alpha = 0;
        int crop;
        int cropmax;
        
        crop = p_inst->crop;
        cropmax = p_arch->cropmax;
        
        if (amap->flags & AM_HASALPHA) {
            sm_rv = alpha = 1;
            if (!softmode)
                RETURN(1);
        }
        
        sub_b = fz_animap_subframe(amap, p_inst);
        
        if (d_amap) { 
            compo = door_decoration_compo(p_inst, amap, sub_b, d_amap, sm, NULL, alpha);  
            if (d_amap->flags & AM_HASALPHA) {
                sm_rv = alpha = 1;   
                if (!softmode) {
                    RETURN(1);
                }
            }
        }
        
        xtweak += amap->xoff;       
        ytweak += amap->yoff;
        
        if (amap->flags & AM_DYNGEN)
            tweak_tweaks(sm, &xtweak, &ytweak);
        
        // hand tweak
        if (sm == 0) {
            ytweak += -6;
                
        } else if (sm == 1) {
            ytweak += -4;
            
            if (tflags & SIDE_VIEW) {
                //xtweak = (sub_b->w/2);
                xtweak += 64;
            }
            
        } else {
            ytweak += 1;
            
            if (tflags & SIDE_VIEW)
                //xtweak = (sub_b->w * 31)/30;
                xtweak += 99;
        }
        
        if ((tflags & SIDE_VIEW) && (tflags & SUBWIDTH))
            xtweak *= -1;

        mx = (rx + xtweak) - (sub_b->w/2);
        my = (ry + ytweak) - sub_b->h;
        
        DO_SM_POSITION_HACKS();

        if ((p_inst->gfxflags & G_BASHED) && s_amap) {                        
            compo = door_decoration_compo(p_inst, amap, sub_b, s_amap, sm, compo, alpha);
            if (s_amap->flags & AM_HASALPHA) {
                sm_rv = alpha = 1; 
                if (!softmode) {
                    RETURN(1);
                }
            }
        } else {
            int xcut = (sub_b->w*crop)/cropmax;
            int ycut = (sub_b->h*crop)/cropmax;
            int cut;
            int height_shift = 0;
            int frameoffset = 0;
                        
            // draw the window
            if (!sm && !(tflags & SIDE_VIEW) && 
                (gd.gameplay_flags & GP_WINDOW_ON))
            {       
                struct animap window_animap;    
                int doorcut = ycut;
                             
                if (p_arch->dtype > DTYPE_UPDOWN)
                    doorcut = 0;
                    
                memset(&window_animap, 0, sizeof(struct animap));
                window_animap.pal = &(gd.simg.window_palette[0]);
                window_animap.numframes = 1;
                window_animap.b = gd.simg.door_window;
                window_animap.w = gd.simg.door_window->w;
                window_animap.yoff = doorcut;
                window_animap.flags = AM_VIRTUAL | AM_256COLOR;                    
                compo = door_decoration_compo(p_inst, amap, sub_b, &window_animap, sm, compo, alpha);
                if (window_animap.flags & AM_HASALPHA) {
                    sm_rv = alpha = 1; 
                    if (!softmode) {
                        RETURN(1);
                    }
                }
            }
            
            if (!sm)
                height_shift = 4;
            else if (sm == 1)
                height_shift = 2;
                     
            if (crop && (p_arch->dtype == DTYPE_LEFTRIGHT))  {  
                int xm = height_shift;
                              
                cut = (sub_b->w*(crop/2))/cropmax;
                
                compo = left_right_slice(sub_b, compo, cut, xm, alpha);
                                           
            } else {
                int xm = 0;
                
                cut = ycut;
                
                if ((crop == 0) || (p_arch->dtype == DTYPE_UPDOWN)) {
                    BITMAP *doorbmp = compo;                        
                    if (!doorbmp)
                        doorbmp = sub_b;
                    
                    if (cut) {
                        cut += frameoffset;
                    } else {
                        xm = height_shift;
                    }

                    cut_compo = create_sub_bitmap(doorbmp, 0, cut,
                        doorbmp->w, doorbmp->h - (cut+xm));
                        
                } else if (p_arch->dtype == DTYPE_MAGICDOOR) {
                    compo = magic_dissolve_slice(sub_b, compo, crop, cropmax, alpha);
                
                } else {
                    compo = side_slide_slice(p_arch->dtype, p_inst, sub_b, compo, xcut, height_shift, alpha);
                }
            }
        }
        
        if (cut_compo)
            drawtarg = cut_compo;
        else if (compo)
            drawtarg = compo;
        else
            drawtarg = sub_b;
            
        cf = act_tint_intensity;
        if (p_arch->shade)
            cf = (cf * p_arch->shade->d[!!(tflags & SIDE_VIEW)][sm]) / 64;
        else    
            cf = (cf * gfxctl.shade[SHO_DOOR][!!(tflags & SIDE_VIEW)][sm]) / 64;
                
        render_shaded_object(sm, scx, drawtarg, mx, my, cf, 
            !!(p_inst->gfxflags & G_FLIP), 0, alpha,
            p_inst, !!(p_arch->rhack & RHACK_NOSHADE));
            
        if (cut_compo) {
            destroy_bitmap(cut_compo);
        }
        
        if (compo) {
            destroy_bitmap(compo);
        }
    }
    
    RETURN(sm_rv);    
}

int draw_single_floorthing(struct inst_loc *dt_il,
    int softmode, BITMAP *scx, 
    int face, int rx, int ry, int vsm, int sm, 
    int tflags, int cznum)
{
    struct inst *p_obj_inst;
    struct obj_arch *p_obj_arch;
    struct animap *amap;
    int sub_b_height = 0;
    int mx, my;
    int frm = 0;
    int ablit = 0;
    int xhack = 0;
    int info_flags = 0;
    int cf = act_tint_intensity;
    int flip_it = 0;
    int retval = 0;
    int glow = 0;
    int b_inside_draw = 0;
    int shotype;
    int shadestrength=0;
    
    onstack("draw_single_floorthing");
    
    p_obj_inst = oinst[dt_il->i];
    p_obj_arch = Arch(p_obj_inst->arch);
    glow = !!(p_obj_arch->rhack & RHACK_NOSHADE);
    
    if (p_obj_inst->gfxflags & OF_INACTIVE)
        RETURN(0);
        
    if (p_obj_arch->type == OBJTYPE_WALLITEM)
        RETURN(0);
        
    if ((tflags & BOTTOM_ZONE) && p_obj_arch->type == OBJTYPE_UPRIGHT) {
        RETURN(0);
    }
    if ((tflags & TOP_ZONE) && p_obj_arch->type != OBJTYPE_UPRIGHT) {
        RETURN(0);
    }
    
    // new section as of 0.55
    // (per-arch shade added as of 0.58)
    shotype = p_obj_arch->type - 1;
    if (shotype == -1 || shotype > SHO_CLOUD)
        shotype = SHO_CLOUD;
    
    if (p_obj_arch->shade) {
        shadestrength = p_obj_arch->shade->d[!!(tflags & SIDE_VIEW)][sm];
    } else 
        shadestrength = gfxctl.shade[shotype][!!(tflags & SIDE_VIEW)][sm];
    
    cf = (cf * shadestrength) / 64;
    
    if (p_obj_arch->type == OBJTYPE_DOOR) {
        int d = 0;
        int b_doorblock;
        
        if (tflags & SIDE_VIEW)
            b_doorblock = 0;
        else
            b_doorblock = Psquarehack;
            
        if (p_obj_inst->facedir) {
            if (p_obj_inst->facedir % 2 == G_Fdir % 2)
                b_doorblock = 0;
        }

        if (!b_doorblock) {
            draw_door(softmode, p_obj_inst, p_obj_arch,
                scx, rx, ry, sm, tflags);
        }
            
        RETURN(d);
    }
    
    if (p_obj_arch->type == OBJTYPE_CLOUD) {
        if (tflags & ZERO_RANGE) {
            RETURN(0);
        } 
    }
    
    if (tflags & SIDE_VIEW) {

        if (p_obj_arch->rhack & RHACK_DOORFRAME) {
            if (!frontsquarehack) {
                RETURN(0);
            } else if (tflags & ZERO_RANGE) {
                RETURN(0);
            }
        }
            
        if (tflags & SUBWIDTH)
            flip_it = 1;
            
    } else {

        if ((p_obj_arch->rhack & RHACK_DOORFRAME) ||
            (p_obj_arch->rhack & RHACK_DOORBUTTON))
        {
            int b_doorblock = Psquarehack;
            
            if (p_obj_inst->facedir) {
                if (p_obj_inst->facedir % 2 == G_Fdir % 2)
                    b_doorblock = 0;
            }
            
            if (gd.gameplay_flags & GP_WINDOW_ON)
                b_doorblock = Psquarehack;
            else if (tflags & ZERO_RANGE)
                b_doorblock = 0;

            if (b_doorblock) {
                RETURN(0);
            }
        }
    }
    
    if (p_obj_inst->flytimer)
        amap = get_fly_view(p_obj_arch->type, p_obj_arch, p_obj_inst, 
            sm, vsm, G_Fdir, tflags, &flip_it);
    else
        amap = get_rend_view(p_obj_arch->type, p_obj_arch, 
            tflags, sm, vsm, &xhack, &info_flags);
    
    if (amap) {
        BITMAP *sub_b;
        int used_sub = 0;
        int xtweak = p_obj_inst->x_tweak + amap->xoff;
        int ytweak = p_obj_inst->y_tweak + amap->yoff;
        int flying = 0;
        int deferred = 0;
        int mfactor = 64;
        int alpha = 0;

        if (amap->flags & AM_HASALPHA) {
            alpha = 1;
            if (!softmode)
                RETURN(1);
        }

        if (tflags & SUBWIDTH)
            xtweak *= -1;

        sub_b = fz_animap_subframe(amap, p_obj_inst);
            
        if (p_obj_arch->rhack & RHACK_POWERSCALE) {
            mfactor = (p_obj_inst->flytimer+20)/4;
            if (mfactor > 64)
                mfactor = 64;
            else if (mfactor < 16)
                mfactor = 16;
        }
        
        // don't dynamically shade pregenerated objects
        if (!(amap->flags & AM_DYNGEN))
            cf = 0;
            
        if (mfactor < 64) {  
            int mfactor960 = 15*mfactor;          
            BITMAP *x_sub = create_bitmap((sub_b->w*mfactor960)/960, (sub_b->h*mfactor960)/960);
        
            stretch_blit(sub_b, x_sub, 0, 0, sub_b->w,
                sub_b->h, 0, 0, x_sub->w, x_sub->h);
            
            if (used_sub)
                destroy_bitmap(sub_b);
            
            sub_b = x_sub;
            used_sub = 1;              
        }    
        
        if (amap->flags & AM_DYNGEN) {
            if (p_obj_arch->rhack & RHACK_DOORBUTTON)
                long_tweak_tweaks(sm, &xtweak, &ytweak);
            else
                tweak_tweaks(sm, &xtweak, &ytweak);
                
            // dynamically generated flooritems don't work the same way
            // ... but flying objects still do
            if (p_obj_arch->type != OBJTYPE_THING)
                flip_it = 0;
        }
            
        if (p_obj_arch->type == OBJTYPE_CLOUD) {
            int cr;
            int ymodf;
            int c_rx = rx;
            int c_xhack = 0;

            if (tflags & SIDE_VIEW) {
                if (sm == 1)
                    c_xhack = 48;
                else if (sm == 2)
                    c_xhack = 96;

                if (tflags & SUBWIDTH)
                    c_rx -= c_xhack;
                else
                    c_rx += c_xhack;
            }
            
            if (face == DIR_CENTER)
                ymodf = 0;
            else {
                ymodf = calcymodf(158 - p_obj_arch->ytweak,
                    sm, vsm);
            }
            
            cr = cloud_handler(scx, sub_b, p_obj_inst, 
                sm, ymodf, c_rx, ry, xtweak, ytweak, tflags, alpha);
                
            goto REND_DONE;    
        }        
        
        // coord specified is bottom center
        // unless using side view, then don't convert x
        if (tflags & SIDE_VIEW) {
            mx = rx + xtweak;
            
            if (tflags & SUBWIDTH) {
                mx -= sub_b->w;
                mx -= xhack;
            } else
                mx += xhack;
                
        } else
            mx = rx + xtweak - (sub_b->w/2);
                    
        my = ry + ytweak - sub_b->h;
        
        DO_SM_POSITION_HACKS();
        
        if (p_obj_inst->flytimer) {
            int ymodf = 166;
            int ft = p_obj_inst->flytimer;
            int m_ft = p_obj_inst->max_flytimer;
            
            /*
            // cheesy parabolic thingy
            if (ft <= (m_ft*3)/4) {
                ymodf *= ft;
                ymodf /= (m_ft*3)/4;
            } 
            */        

            flying = 1;
            
            ymodf = calcymodf(ymodf, sm, vsm);
            my -= ymodf;
            
            deferred = defer_draw(OBJTYPE_THING, sub_b, used_sub,
                sm, mx, my, cf, flip_it, alpha, p_obj_inst, glow);
        }
        
        if (!deferred) {

            render_shaded_object(sm, scx, sub_b, mx, my, cf,
                flip_it, 0, alpha, p_obj_inst, glow);
        }
        
        if (cznum && !flying) {
            int vy = my;
            int vh = sub_b->h;
            
            if (UNLIKELY(deferred)) {
                coord_inst_r_error("Tried to defer clickzone", dt_il->i, 255);
                drop_flying_inst(dt_il->i, p_obj_inst);
                goto REND_DONE;
            }

            if (cznum != 255 && p_obj_arch->type == OBJTYPE_THING) {
                int bx = 0;
                int vcznum = cznum;
                int xr, yr, sx, sy;
                
                if (cznum > 100) {
                    vcznum -= 100;
                    bx = gd.vxo+64;
                    vy += (gd.vyo + gd.vyo_off + 18);
                }
                
                if (vh < 32) {
                    vy -= (32-vh);
                    vh = 32;
                }
                
                xr = mx+bx;
                yr = vy;
                sx = sub_b->w;
                sy = vh;
                
                if (cz[vcznum].w) {
                    int cz_xe = cz[vcznum].x + cz[vcznum].w;
                    int cz_ye = cz[vcznum].y + cz[vcznum].h;

                    if (cz[vcznum].x < xr) {
                        sx += (xr - cz[vcznum].x);
                        xr = cz[vcznum].x;
                    }
                        
                    if (cz[vcznum].y < yr) {
                        sy += (yr - cz[vcznum].y);
                        yr = cz[vcznum].y;
                    }
                        
                    if (cz_xe > xr + sx)
                        sx = cz_xe - xr;
                        
                    if (cz_ye > yr + sy)
                        sy = cz_ye - yr;
                }

                make_cz(vcznum, xr, yr, sx, sy, dt_il->i);
            
            } else if (p_obj_arch->rhack & RHACK_CLICKABLE) {
                int czx = g_i_cztrk;
                int advance = 1;
                
                if ((p_obj_arch->rhack & RHACK_DOORBUTTON) ||
                    (p_obj_arch->rhack & RHACK_DOORFRAME) ||
                    (p_obj_arch->rhack & RHACK_MIRROR))
                {
                    czx = 11;
                    advance = 0;
                }
                
                make_cz(czx, mx, vy, sub_b->w, vh, dt_il->i);
                
                if (advance) {
                    g_i_cztrk++; 
                    if (g_i_cztrk == 38)
                        g_i_cztrk = 19;
                }
            }      
        }
        
        REND_DONE:         
        if (sub_b)
            sub_b_height = sub_b->h;
        if (used_sub && !deferred) 
            destroy_bitmap(sub_b);
            
        b_inside_draw = !!(p_obj_arch->arch_flags & ARFLAG_DRAW_CONTENTS);
        if (b_inside_draw && p_obj_inst->inside_n) {
            int i_i;
            for (i_i=0;i_i<p_obj_inst->inside_n;++i_i) {
                unsigned int in = p_obj_inst->inside[i_i];
                int cur_x_base = 0;
                int cur_y_base = 0;
                struct inst_loc vdtil;
                
                if (in) {
                    int xt = 0;
                    int yt = 24;
                    int stripped_tflags = tflags;
                    
                    cur_x_base = rx + xtweak;
                    cur_y_base = my + sub_b_height;
                    
                    memset(&vdtil, 0, sizeof(struct inst_loc));
                    vdtil.i = in;

                    stripped_tflags &= ~(BOTTOM_ZONE);
                    stripped_tflags &= ~(TOP_ZONE);
                    
                    tweak_tweaks(sm, &xt, &yt);
                    
                    // hack for table renderings to look right
                    if (vsm == sm)
                        cur_y_base -= yt;
                    else
                        cur_y_base -= yt/6;
    
                    if (draw_single_floorthing(&vdtil, softmode,
                        scx, face, cur_x_base, cur_y_base,
                        vsm, sm, stripped_tflags, 0))
                    {
                        retval = 1;
                        if (!softmode)
                            RETURN(1);
                    }
                }
            }
        }
    }
    
    if (ablit && !softmode)
        RETURN(1);
        
    RETURN(retval);
} 

int draw_floorstuff(int softmode, BITMAP *scx, struct dtile *dt, 
    int face, int rx, int ry, int vsm, int tflags, int cznum)
{
    int sm;
    struct inst_loc *dt_il;
    struct inst_loc *door_l = NULL;
       
    sm = (vsm%3);
    
    dt_il = dt->il[face];
    while (dt_il != NULL) {
        struct inst *p_obj_inst;
        struct obj_arch *p_obj_arch;
   
        p_obj_inst = oinst[dt_il->i];
        p_obj_arch = Arch(p_obj_inst->arch);
        
        // this loop doesn't cover decals, hazes or monsters
        if (p_obj_arch->type == OBJTYPE_FLOORITEM ||
            p_obj_arch->type == OBJTYPE_HAZE ||
            p_obj_arch->type == OBJTYPE_MONSTER) 
        {
            goto NEXT_THING;
        }
        
        // viewangle must go here to block doorbuttons
        if (p_obj_arch->viewangle) {
            if (!valid_viewangle(G_Fdir, p_obj_arch->viewangle))
                goto NEXT_THING;
        }
        
        // slightly defer doors
        /* (no longer done because ESB does this for you)
        if (p_obj_arch->type == OBJTYPE_DOOR) {
            goto ADD_TO_DOOR_LIST;
        }
        */
        
        // doorbutton hack and defer
        if (p_obj_arch->rhack & RHACK_DOORBUTTON) {
            if (tflags & SIDE_VIEW) {
                if (sm != 2 || !(tflags & SUBWIDTH)) {
                    goto NEXT_THING;
                }
            }
            
            goto ADD_TO_DOOR_LIST;
        }
        
        // ... or clouds when you're standing in them
        if ((tflags & ZERO_RANGE) && p_obj_arch->type == OBJTYPE_CLOUD)
            goto NEXT_THING;
        
        if (draw_single_floorthing(dt_il, softmode, scx,
            face, rx, ry, vsm, sm, tflags, cznum))
        {
            struct inst_loc *wp_door;
            return(1);

            // this code actually has nothing to do with the above
            // condition. it's just stuck here so it'll usually get skipped.
            ADD_TO_DOOR_LIST:
            wp_door = dsbfastalloc(sizeof(struct inst_loc));
            wp_door->i = dt_il->i;
            wp_door->n = door_l;
            door_l = wp_door;
        }
        
        NEXT_THING:
        dt_il = dt_il->n;
    }
    
    // defer doors to here
    while (door_l != NULL) {
        if (draw_single_floorthing(door_l, softmode, scx,
            face, rx, ry, vsm, sm, tflags, cznum))
        {
            return(1);
        }

        door_l = door_l->n;
    }

    return(0);   
}

int draw_floorstuff_cloudsonly(int softmode, BITMAP *scx, struct dtile *dt,
    int face, int rx, int ry, int vsm, int tflags, int cznum)
{
    int sm;
    struct inst_loc *dt_il;

    sm = (vsm%3);

    dt_il = dt->il[face];
    while (dt_il != NULL) {
        struct inst *p_obj_inst;
        struct obj_arch *p_obj_arch;

        p_obj_inst = oinst[dt_il->i];
        p_obj_arch = Arch(p_obj_inst->arch);

        if (p_obj_arch->type != OBJTYPE_CLOUD) {
            goto NEXT_THING;
        }

        if (draw_single_floorthing(dt_il, softmode, scx,
            face, rx, ry, vsm, sm, tflags, cznum))
        {
            return(1);
        }

        NEXT_THING:
        dt_il = dt_il->n;
    }

    return(0);
}

int draw_wallstuff(int softmode, BITMAP *scx, struct dtile *dt, 
    int face, int rx, int ry, int sm, int tflags, int cznum)
{
    struct inst_loc *dt_il;
    int x_obj_base = 0;
    int y_obj_base = 0;
    int sm_rv = 0;
    int glow = 0;
    int one_wallitem = !!(gd.gameplay_flags & GP_ONE_WALLITEM);
    int drawn = 0;
    
    dt_il = dt->il[face];
    while (dt_il != NULL) {
        struct inst *p_obj_inst;
        struct obj_arch *p_obj_arch;
        struct animap *amap;
        int mx, my;
        int xhack = 0;
        int frm = 0;
        int ablit = 0;
        int info_flags = 0;
        int block_rend = 0;
        unsigned int inst = dt_il->i;
        
        p_obj_inst = oinst[inst];
        p_obj_arch = Arch(p_obj_inst->arch);
        
        // don't draw inactive objects
        if (p_obj_inst->gfxflags & OF_INACTIVE) {
            dt_il = dt_il->n;
            continue;       
        }
        
        glow = !!(p_obj_arch->rhack & RHACK_NOSHADE);
                  
        // put pickableuppable things in alcoves
        if (p_obj_arch->type == OBJTYPE_THING) {       
            if (y_obj_base) {
                if (draw_single_floorthing(dt_il, softmode, scx,
                    face, x_obj_base, y_obj_base, sm, sm, 0, cznum))
                {
                    sm_rv = 1;
                    if (!softmode)
                        return(1);
                }
            }
            
            dt_il = dt_il->n;
            continue;
        }
        
        // don't draw if we're limited and there are more down the line
        if (one_wallitem) {
            int another_later = 0;
            struct inst_loc *c_dt_il = dt_il->n;
            while (c_dt_il != NULL) {
                struct inst *c_p_inst = oinst[c_dt_il->i];
                struct obj_arch *c_p_arch = Arch(c_p_inst->arch);
                
                if (c_p_arch->type == OBJTYPE_WALLITEM) {
                    if (c_p_arch->dview[0]) {
                        if (!(c_p_inst->gfxflags & G_INACTIVE)) {
                            another_later = 1;
                            break;
                        }
                    }
                }
                c_dt_il = c_dt_il->n;
            }
            
            if (another_later) {
                dt_il = dt_il->n;
                continue;
            }
        }
        
        // don't draw irrelevant crap
        if (p_obj_arch->type != OBJTYPE_WALLITEM) {
            dt_il = dt_il->n;
            continue;
        }
        
        amap = get_rend_view(OBJTYPE_WALLITEM, p_obj_arch, 
            tflags, sm, sm, &xhack, &info_flags);
    
        if (amap != NULL) {  
            BITMAP *sub_b;
            int used_sub = 0;
            int xtweak = amap->xoff;
            int ytweak = amap->yoff;
            int tf;
            int m_flags = (tflags & SUBWIDTH);
            int b_inside_draw = 0;
            int alpha = 0;
            
            if (amap->flags & AM_HASALPHA) {
                sm_rv = 1;
                alpha = 1;
                if (!softmode)
                    return(1);
            }
            
            if (amap->flags & AM_DYNGEN) {
                if (tflags & SIDE_VIEW)
                    long_tweak_tweaks(sm, &xtweak, &ytweak);
                else
                    tweak_tweaks(sm, &xtweak, &ytweak);
            }
                                
            sub_b = fz_animap_subframe(amap, p_obj_inst);
            
            if (tflags & SIDE_VIEW) {
                // coord specified is inside edge thingy...
                // except if zero range, then it is outside edge
                if (tflags & ZERO_RANGE) {
                    if (tflags & SUBWIDTH)
                        mx = (rx - xtweak) - sub_b->w;
                    else
                        mx = (rx + xtweak);
                } else {
                    if (tflags & SUBWIDTH)
                        mx = (rx - xtweak);
                    else
                        mx = (rx + xtweak) - sub_b->w;
                }
                
                my = ry + ytweak;
            } else {
                // coord specified is top center
                mx = rx + xtweak - (sub_b->w/2);
                my = ry + ytweak;
                
                if (p_obj_arch->type == OBJTYPE_WALLITEM) {
                    unsigned short arch_flags = p_obj_arch->arch_flags;
                    if (arch_flags & ARFLAG_DROP_ZONE) {
                        x_obj_base = rx + xtweak;
                        y_obj_base = my + sub_b->h - (sub_b->h/16);

                    } else if (arch_flags & ARFLAG_DRAW_CONTENTS)
                        b_inside_draw = 1;
                }

            }
                                                
            if (!(amap->flags & AM_DYNGEN))
                tf = 0;
            else {
                tf = act_tint_intensity;
                
                if (p_obj_arch->shade)
                    tf = (tf * p_obj_arch->shade->d[!!(tflags & SIDE_VIEW)][sm]) / 64;
                else    
                    tf = (tf * gfxctl.shade[SHO_WALLITEM][!!(tflags & SIDE_VIEW)][sm]) / 64;
            }
            
            // don't flip the object if we're rendering an otherside view    
            if (info_flags & TWO_DIRECTIONS)
                m_flags = 0;
            
            if (p_obj_arch->rhack & RHACK_WALLPATCH) {
                if (!(tflags & SIDE_VIEW)) {
                    if (cwsa->wpatch[sm]) {
                        struct animap *wp = cwsa->wpatch[sm];
                        draw_sprite(scx, wp->b, rx-(wp->b->w/2), ry+wp->yoff);
                    }
                } else {
                    if (cwsa->sidepatch && !sm) {
                        struct animap *sp = cwsa->sidepatch;
                        int vx = rx;
                        
                        if (tflags & SUBWIDTH)
                            draw_sprite_h_flip(scx, sp->b, rx, ry+sp->yoff);
                        else
                            draw_sprite(scx, sp->b, rx-(sp->w), ry+sp->yoff);
                    }
                }
            }
            
            if (sm == 0 && !(tflags & SIDE_VIEW)) {
                if (p_obj_arch->rhack & RHACK_WRITING) {
                    block_rend = draw_wall_writing(softmode, scx,
                        dt_il->i, rx, ry);
                }
            }
            
            DO_SM_POSITION_HACKS();
                
            if (sm == 2) {
                if (amap->yoff < 8)
                    my += 2;
            }
            
            if (!block_rend) {
                BITMAP *g_bmp = sub_b;
                BITMAP *cut_bmp = NULL;
                
                if (p_obj_arch->rhack & RHACK_DYNCUT) {
                    int xskew, yskew;
                    cut_bmp = create_lua_sub_hack(inst, sub_b,
                        &xskew, &yskew, !!(tflags & SIDE_VIEW));
                    if (cut_bmp) {
                        g_bmp = cut_bmp;
                        mx += xskew;
                        my += yskew;
                    }
                }
                
                // respect this flag (for some reason...)
                if (!(tflags & SIDE_VIEW) &&
                    (p_obj_inst->gfxflags & G_FLIP))
                {
                    m_flags = SUBWIDTH;
                }
                
                render_shaded_object(sm, scx, g_bmp,
                    mx, my, tf, m_flags, 0, alpha,
                    p_obj_inst, glow);
                    
                drawn = 1;
                
                if (cut_bmp)
                    destroy_bitmap(cut_bmp);
            }
                
            if (b_inside_draw && p_obj_inst->inside_n) {
                int i_i;
                for (i_i=0;i_i<p_obj_inst->inside_n;++i_i) {
                    unsigned int in = p_obj_inst->inside[i_i];
                    int cur_x_base, cur_y_base;
                    struct inst_loc vdtil;
                    
                    if (in) {
                        cur_x_base = rx + xtweak;
                        cur_y_base = my + sub_b->h - (sub_b->h/12);
                        vdtil.i = in;
                        vdtil.slave = 0;
                        vdtil.n = NULL;

                        if (draw_single_floorthing(&vdtil, softmode,
                            scx, face, cur_x_base, cur_y_base,
                            sm, sm, 0, 0))
                        {
                            sm_rv = 1;
                            if (!softmode)
                                return(1);
                        }
                    }
                }
            }
            
            if (cznum) {
                int bx = 0;
                int by = 0;
                int vcznum = cznum;
                
                // hack for windowed walls
                if (cznum > 100) {
                    vcznum -= 100;
                    bx = gd.vxo+64;
                    by = gd.vyo + gd.vyo_off + 18;
                }
                
                if (p_obj_arch->arch_flags & ARFLAG_DROP_ZONE)
                    vcznum = 10+vcznum;
                else {
                    vcznum = g_i_cztrk++; 
                    if (g_i_cztrk == 38)
                        g_i_cztrk = 19;
                }
                make_cz(vcznum, mx+bx, my+by, sub_b->w, sub_b->h, dt_il->i);
            }
            
            if (sm == 0 && !(tflags & SIDE_VIEW)) {
                if (p_obj_arch->rhack & RHACK_MIRROR) {
                    int cnum;
                    int success = ex_exvar(dt_il->i, "champion", &cnum);
                    if (success) {
                        if (cnum < 0 || cnum > gd.num_champs)
                            success = 0;
                    }
                    if (success && cnum) {
                        int prv;
                        
                        prv = draw_a_portrait(softmode, scx, cnum, p_obj_arch,
                            p_obj_inst, mx ,my);

                        if (prv) {
                            sm_rv = 1;
                            if (!softmode)
                                return(1);
                        }
                    }
                }
            }
                            
            if (used_sub)
                destroy_bitmap(sub_b);            
        }
           
        dt_il = dt_il->n;      
    }
    
    return(sm_rv);   
}

int draw_haze_in_dir(int dir, int softmode, struct wallset *ws_cur,
    BITMAP *scx, struct dtile *dt, int rx, int ry, int w, int h,
    struct animap *dmask, int flags, int sm)
{
    int usm = 0;
    struct inst_loc *dt_il;
    int glow = 0;
    
    dt_il = dt->il[dir];
    
    while (dt_il != NULL) {
        struct inst *p_obj_inst;
        struct obj_arch *p_obj_arch;
        int vdraw = 0;
        struct animap *amap;
        
        p_obj_inst = oinst[dt_il->i];
        p_obj_arch = Arch(p_obj_inst->arch);
        
        if (p_obj_inst->gfxflags & OF_INACTIVE) {
            dt_il = dt_il->n;
            continue;
        }
        
        glow = !!(p_obj_arch->rhack & RHACK_NOSHADE);
        
        if (p_obj_arch->viewangle) {
            if (!valid_viewangle(G_Fdir, p_obj_arch->viewangle)) {
                dt_il = dt_il->n;
                continue;
            }
        }
        
        if (p_obj_arch->type == OBJTYPE_HAZE) {
            vdraw = 1;
            amap = p_obj_arch->dview[0];
        } else {
            if ((flags & ZERO_RANGE) && p_obj_arch->type == OBJTYPE_CLOUD) {
                int spow = (p_obj_inst->charge-12)/16;
                vdraw = 2;
                
                if (spow < 0) spow = 0;
                else if (spow > 2) spow = 2;
                
                amap = p_obj_arch->inview[spow];
            }
        }
        
        if (vdraw && amap) {
            int sx, sy, bsx, bsy;
            BITMAP *sbmp;
            BITMAP *tbmp;
            int rev = 0;
            int revdraw = 0;
            int alpha;
            
            lod_use(amap);
            alpha = !!(amap->flags & AM_HASALPHA);
            
            if (alpha) {
                usm = 1;
                if (!softmode) {
                    return(1);
                }
            }
            
            if (vdraw == 2 && (p_obj_inst->gfxflags & G_FLIP))
                rev = 1;
           
            tbmp = animap_subframe(amap, 0);

            if (p_obj_arch->rhack & RHACK_BLUEHAZE) {
                bsx = -1 * (tbmp->w/2) * ((gd.framecounter % 70) > 35);
                bsy = -1 * ((gd.framecounter/(5*(1+sm))) % tbmp->h);
                
                if ((gd.framecounter % 80) > 40)
                    rev = 1;
                    
            } else {
                bsx = bsy = 0;
            }
             
            sx = bsx;
            sy = bsy;
        
            sbmp = create_bitmap_ex(32, w, h);
            while (sy < h) {
                sx = bsx;
                
                while (sx < w) {
                    if (dmask) {
                        masked_copy(tbmp, sbmp, 0, 0, sx, sy, 
                            tbmp->w, tbmp->h, dmask->b, sx, sy, makecol32(255, 0, 255),
                            makecol32(255, 0, 255), rev, (flags & SUBWIDTH));
                        
                        revdraw = 1;
                        
                    } else
                        blit(tbmp, sbmp, 0, 0, sx, sy, tbmp->w, tbmp->h);
                    
                    sx += tbmp->w;
                }
                
                sy += tbmp->h;        
            }
            
            if (alpha || glow || gd.glowmask) {
                BITMAP *ybmp;

                if (rev && !revdraw) {
                    ybmp = create_bitmap_ex(32, sbmp->w, sbmp->h);
                    clear_to_color(ybmp, makecol(255, 0, 255));
                    draw_sprite_h_flip(ybmp, sbmp, 0, 0);
                } else
                    ybmp = sbmp;
                    
                if (alpha) {
                    set_alpha_blender();
                    draw_trans_sprite(scx, ybmp, rx, ry);
                } else
                    draw_sprite(scx, ybmp, rx, ry);
                    
                block_glowmask(glow, ybmp, rx, ry, alpha);
                
                if (ybmp != sbmp)
                    destroy_bitmap(ybmp);
                    
            } else {
                if (rev && !revdraw)
                    draw_sprite_h_flip(scx, sbmp, rx, ry);
                else
                    draw_sprite(scx, sbmp, rx, ry);
            }
            
            destroy_bitmap(sbmp);
        }
        
        dt_il = dt_il->n;
    }
    
    return(usm);
}

int draw_haze(int softmode, struct wallset *ws_cur, BITMAP *scx, struct dtile *dt,
     int rx, int ry, int w, int h, int dmsel, int flags, int sm)
{
    int r;
    struct animap *dmask;
      
    if (dmsel)
        dmask = haze_base->wall[dmsel];
    else
        dmask = NULL;
    
    lod_use(dmask);
    if (flags & ZERO_RANGE) {
        int y;
        int r = 0;
        for (y=0;y<=4;++y) {
            int u;
            
            u = draw_haze_in_dir(y, softmode, ws_cur,
                scx, dt, rx, ry, w, h, dmask, flags, sm);
                
            if (u) {
                r = 1;
                if (!softmode)
                    return(1);
            }
        }
        
        return(r);
    }
    
    r = draw_haze_in_dir(DIR_CENTER, softmode, ws_cur,
        scx, dt, rx, ry, w, h, dmask, flags, sm);
        
    return(r);
}

int draw_floordecals(int softmode, BITMAP *scx, struct dtile *dt, 
    int rx, int ry, int sm, int tflags)
{
    struct inst_loc *dt_il;
       
    dt_il = dt->il[DIR_CENTER];
    while (dt_il != NULL) {
        struct inst *p_obj_inst;
        struct obj_arch *p_obj_arch;
        struct animap *amap;
        int mx, my;
        int xhack = 0;
        int frm = 0;
        int ablit = 0;
        int info_flags = 0;
                
        p_obj_inst = oinst[dt_il->i];
        p_obj_arch = Arch(p_obj_inst->arch);
        
        if (p_obj_arch->type != OBJTYPE_FLOORITEM ||
            (p_obj_inst->gfxflags & OF_INACTIVE)) 
        {
            goto NEXT_DECAL;
        }
        
        if (p_obj_arch->viewangle) {
            if (!valid_viewangle(G_Fdir, p_obj_arch->viewangle))
                goto NEXT_DECAL;
        } 
        
        if (p_obj_arch->rhack & RHACK_FAKEWALL)
            dt->SCRATCH |= 4;
            
        if (p_obj_arch->rhack & RHACK_INVISWALL)
            dt->SCRATCH |= 2;
        
        if ((tflags & ZERO_RANGE)) {
            if (!p_obj_arch->inview[0]) {           
                goto NEXT_DECAL;
            }
        }

        amap = get_rend_view(OBJTYPE_FLOORITEM, p_obj_arch, 
            tflags, sm, sm, &xhack, &info_flags);

        if (amap != NULL) {  
            BITMAP *sub_b;
            int xtweak = amap->xoff;
            int ytweak = amap->yoff;
            int alpha = 0;
            int flip = 0;
            int tf = act_tint_intensity;
            
            if (p_obj_arch->shade)
                tf = (tf * p_obj_arch->shade->d[!!(tflags & SIDE_VIEW)][sm]) / 64;
            else    
                tf = (tf * gfxctl.shade[SHO_FLOORITEM][!!(tflags & SIDE_VIEW)][sm]) / 64;
                     
            lod_use(amap);
            if (amap->flags & AM_DYNGEN)
                tweak_tweaks(sm, &xtweak, &ytweak);
            else
                tf = 0;
                                 
            sub_b = animap_subframe(amap, 0);         
            
            if (tflags & SIDE_VIEW) {
                // coord specified is outer edge center
                mx = rx;
                
                if (tflags & SUBWIDTH)
                    mx -= (sub_b->w + xtweak + xhack);
                else
                    mx += (xtweak + xhack);
                
            } else {
                // coord specified is center
                mx = rx + xtweak - (sub_b->w/2);  
            }
            
            if (tflags & ZERO_RANGE)
                my = ry + ytweak - sub_b->h;
            else
                my = ry + ytweak - (sub_b->h/2);
                
            DO_SM_POSITION_HACKS();
            
            if (amap->flags & AM_HASALPHA)
                alpha = 1;
                
            flip = !!(p_obj_inst->gfxflags & G_FLIP);
            if (tflags & SUBWIDTH)
                flip = !flip;
            
            render_shaded_object(sm, scx, sub_b, mx, my, tf, 
                flip, 0, alpha, p_obj_inst, !!(p_obj_arch->rhack & RHACK_NOSHADE));        
        }
        
        NEXT_DECAL:
        dt_il = dt_il->n;      
    }
    
    return(0);   
}

int draw_monster(int softmode, BITMAP *scx, struct dtile *dt, 
    int face, int brx, int bry, int sm, int tflags, 
    int clobbercz, int mycz, int prop_tws, int ft)
{
    struct inst_loc *dt_il;
    int retval = 0;
    int char_here = 0;
    int glow = 0;
    int shadestrength = 0;
    int nextshadestrength = 0;
    int tws;
      
    if (dt->exparrep && face < 4) {
        int ap = (dt->exparrep - 1);
        int abstilepos = face;
        int relpos = ((abstilepos+4) - gd.p_face[ap]) % 4;
        int whothere = gd.guypos[relpos/2][(relpos%3 > 0)][ap];
        if (whothere)
            char_here = gd.party[whothere - 1];
    }
    
    dt_il = dt->il[face];
    while (char_here || dt_il != NULL) {
        unsigned int inst = 0;
        struct inst *p_m_inst;
        struct obj_arch *p_m_arch;
        struct animap *amap;
        int info_flags = 0;
        int rx, ry;
        int adj_m_face = (G_Fdir+ft)%4;
        
        rx = brx;
        ry = bry;

        if (char_here) {
            int i_rcn = gd.champs[char_here - 1].exinst;
            if (!i_rcn)
                goto CONTINUE_MLOOP;
            p_m_inst = oinst[i_rcn];
            if (p_m_inst == NULL)
                goto CONTINUE_MLOOP;
                
            inst = i_rcn;
            // face the monster the same way as this party
            p_m_inst->facedir = gd.p_face[dt->exparrep - 1];

        } else {
            inst = dt_il->i;
            p_m_inst = oinst[dt_il->i];
            
        }
        
        p_m_arch = Arch(p_m_inst->arch);
        
        if (p_m_arch->type != OBJTYPE_MONSTER)
            goto CONTINUE_MLOOP;
        
        amap = get_monster_view(p_m_inst, p_m_arch, sm, 
            adj_m_face, tflags, &info_flags);
        
        if (amap) {
            BITMAP *sub_b;
            BITMAP *draw_b = NULL;
            BITMAP *cut_bmp = NULL;
            int used_sub = 0;
            int xtweak = p_m_inst->x_tweak + amap->xoff;
            int ytweak = p_m_inst->y_tweak + amap->yoff;
            int mx, my;
            int flip_it;
            int cf;
            int alpha = 0;
            int b_sidv = !!(tflags & SIDE_VIEW);
            
            if (p_m_arch->shade)
                shadestrength = p_m_arch->shade->d[b_sidv][sm];
            else
                shadestrength = gfxctl.shade[SHO_MONSTER][b_sidv][sm];
            
            nextshadestrength = shadestrength;
            if (sm < 2) {
                if (p_m_arch->shade)
                    nextshadestrength = p_m_arch->shade->d[b_sidv][sm+1];
                else
                    nextshadestrength = gfxctl.shade[SHO_MONSTER][b_sidv][sm+1];
            }
            
            cf = act_tint_intensity;
            cf = (cf * shadestrength) / 64;
            
            if (amap->flags & AM_HASALPHA) {
                if (!softmode)
                    return(1);
                    
                retval = 1;
                alpha = 1;
            }
            
            glow = !!(p_m_arch->rhack & RHACK_NOSHADE);
            
            if (p_m_arch->rhack & RHACK_2WIDE) {
                
                if (info_flags & SIDE_VIEW) {
                    rx = MONSTERXBASE;
                    
                    // suppress all zones for a sideview monster
                    if (clobbercz) clobbercz = 94;
                    
                    if (tflags & SIDE_VIEW)
                        goto SLAVE_OK;   
                }
                    
                if (!char_here && (info_flags & BACK_VIEW)) {
                    if (!dt_il->slave && face != DIR_CENTER) 
                        goto CONTINUE_MLOOP;
                } else {
                    if (dt_il->slave)
                        goto CONTINUE_MLOOP;
                }
            }
            SLAVE_OK:
            
            flip_it = (info_flags & FLIPH);
            
            // flipping side views doesn't do what we want
            // unless it's overtly specified as ok
            if (p_m_inst->gfxflags & G_FLIP) {
                int sideflip = 1;
                if (!p_m_arch->side_flip_ok && (info_flags & SIDE_VIEW))
                    sideflip = 0;
                    
                if (sideflip)
                    flip_it = 1;
            }
            
            // reverse the offset and tweak on the flip side
            if (tflags & SUBWIDTH)
                xtweak *= -1;
            
            if (amap->numframes > 1) {
                sub_b = monster_animap_subframe(amap, p_m_inst->frame);
            } else                   
                sub_b = amap->b;   
            
            if (amap->flags & AM_DYNGEN)
                tweak_tweaks(sm, &xtweak, &ytweak);
            else
                cf = 0;        
            
            // coord specified is bottom center
            mx = (rx + xtweak) - (sub_b->w/2);
            my = (ry + ytweak) - sub_b->h;
            
            draw_b = sub_b;
            if (p_m_arch->rhack & RHACK_DYNCUT) {
                int xskew, yskew;
                cut_bmp = create_lua_sub_hack(inst, sub_b,
                    &xskew, &yskew, !!(tflags & SIDE_VIEW));
                if (cut_bmp) {
                    draw_b = cut_bmp;
                    mx += xskew;
                    my += yskew;
                }
            }
            
            // translate the old tws values into something that
            // works with having a dynamic shadestrength
            tws = (prop_tws * nextshadestrength) / 60;

            render_shaded_object(sm, scx, draw_b, mx, my, cf,
                flip_it, tws, alpha, p_m_inst, glow);
                    
            // maybe i don't destroy clickzones
            if (clobbercz && (p_m_arch->arch_flags & ARFLAG_NOCZCLOBBER))
                clobbercz = 0;
            
            // destroy the item clickzone i'm standing on
            // and destroy the dropzones too, while i'm at it
            // (unless the gameflag is keeping it oldschool)
            if (!char_here && clobbercz) {
                // hack for centered monsters
                if (clobbercz > 90) {
                    cz[clobbercz-90].w = 0;
                    cz[clobbercz-89].w = 0;
                    if (!(gd.gameplay_flags & GP_NOCLOBBER_DROP)) {
                        cz[clobbercz-80].w = 0;
                        cz[clobbercz-79].w = 0;
                    }
                } else {
                    cz[clobbercz].w = 0;
                    if (!(gd.gameplay_flags & GP_NOCLOBBER_DROP))
                        cz[clobbercz+10].w = 0;
                }
            }
            
            if (!char_here && mycz) {
                make_cz(mycz, mx, my, draw_b->w, draw_b->h, dt_il->i);
            }
            
            if (cut_bmp)
                destroy_bitmap(cut_bmp);
                     
            if (used_sub)
                destroy_bitmap(sub_b);
        }
        
        CONTINUE_MLOOP:
        if (char_here)
            char_here = 0;
        else
            dt_il = dt_il->n;
    }
    
    return(retval);
}

BITMAP *find_rendtarg(int softmode) {
    
    if (FORCE_RENDER_TARGET) 
        return(FORCE_RENDER_TARGET);

    if (softmode)
        return(soft_buffer);
    else
        return(scr_pages[gd.scr_blit]);
}

void dungeonclip(BITMAP *scx, int bx, int by) {
    set_clip_rect(scx, bx, by, bx+447, by+271);
}

void openclip(int softmode) {
    set_clip_rect(find_rendtarg(softmode), 0, 0, 639, 479);    
}

void setup_background(BITMAP *scx, int forceblack) {   
    if (gii.main_background && !forceblack) {
        BITMAP *draw_bmp;
        lod_use(gii.main_background);
        draw_bmp = animap_subframe(gii.main_background, 0);
        blit(draw_bmp, scx, 0, 0, 0, 0, draw_bmp->w, draw_bmp->h); 
    } else {   
        clear_to_color(scx, 0);
    }
}

int render_dungeon_view(int softmode, int need_cz) {
    static struct dtile *peri[2];
    static struct dtile *vis[4][3];
    static struct dtile copy_tile[4][3];
    static struct dtile full;
    struct dtile *it_ct;
    struct animap *floor_bitmap = NULL;
    struct animap *roof_bitmap = NULL;
    int majority_floor_set = 0;
    int floor_votes = 0;
    struct dungeon_level *dd;
    int used_softmode = 0;
    int awall;
    BITMAP *scx;
    BITMAP *light_buffer;
    int yc, xc;
    int lev, px, py;
    int bx, by;
    int gfr;
    int oddeven,aoddeven;
    int bsx, bsy, o_dx = 0, o_dy = 0, i_dx = 0, i_dy = 0;
    int tlight = -1;
    int ccnt = 0;
    int wall_show;
    int fdir, fdf, fdl, fdr;
    int floor_size;
    int pz;
    int b_iter;
    struct wallset *stored_cwsa;
    unsigned int stored_cwsa_num;

    onstack("render_dungeon_view");
    
    if (need_cz) {
        memset(cz, 0, sizeof(cz));
        need_cz = 1;
    }
    
    if (gfxctl.l_alt_targ) {
        struct alt_targ *at = gfxctl.l_alt_targ;
        light_buffer = at->targ;
        scx = at->targ;
        G_mask_xsub = bx = 0;
        G_mask_ysub = by = 0;
        lev = at->lev;
        px = at->px;
        py = at->py;
        fdir = at->dir;
        tlight = at->tlight;
    } else {
        scx = find_rendtarg(softmode);
        light_buffer = soft_buffer;
        G_mask_xsub = bx = gd.vxo;
        G_mask_ysub = by = gd.vyo + gd.vyo_off;
        lev = gd.p_lev[gd.a_party];
        px = gd.p_x[gd.a_party];
        py = gd.p_y[gd.a_party];
        fdir = gd.p_face[gd.a_party];
    }
    
    // global variable hacks
    G_Fdir = fdir;
    cwsa_num = dun[lev].wall_num;
    cwsa = ws_tbl[cwsa_num];
    
    // push the wallset so the weird "voting" code
    // down there can alter the wallset temporarily... if needed
    stored_cwsa_num = cwsa_num;
    stored_cwsa = cwsa;
    base_active_set = cwsa;
    
    dd = &(dun[lev]);
    
    act_tintr = getr(dd->tint);
    act_tintg = getg(dd->tint);
    act_tintb = getb(dd->tint);
    act_tint_intensity = dd->tint_intensity;
    b_iter = !!(dd->level_flags & DLF_ITERATIVE_CEIL);
    
    if (gfxctl.l_alt_targ) {
        clear_to_color(scx, 0);
    } else {
        setup_background(scx, 0); 
    } 

    // set up base clickzones
    g_i_cztrk = 19;

    oddeven = (lev + px + py + fdir) % 2;
    aoddeven = (oddeven + 1) % 2;

    if (tlight < 0) {
        tlight = calc_total_light(dd);
        if (tlight < 0) tlight = 0;

        if (tlight > gd.max_total_light)
            tlight = gd.max_total_light;
    }
    
    //  animated walls?
    gfr = 0;

    fdl = (fdir + 1) % 4;
    fdf = (fdir + 2) % 4;
    fdr = (fdir + 3) % 4;
    
    switch(fdir) {
        case 0:
            bsx = px - 1;
            bsy = py - 3;
            i_dx = 1;
            o_dy = 1;
            break;
            
        case 1:
            bsx = px + 3;
            bsy = py - 1;
            i_dy = 1;
            o_dx = -1;
            break;
            
        case 2:
            bsx = px + 1;
            bsy = py + 3;
            i_dx = -1;
            o_dy = -1;
            break;
        
        case 3:
            bsx = px - 3;
            bsy = py + 1;
            i_dy = -1;
            o_dx = 1;
            break;
    }
    
    // set up outer wall
    memset(&full, 0, sizeof(struct dtile));
    full.w = 1;
    
    peri[0] = peri[1] = &full;
    
    if ((bsy - i_dy > 0) && (bsy - i_dy < dd->ysiz))
        if ((bsx - i_dx > 0) && (bsx - i_dx < dd->xsiz)) 
            peri[0] = &(dd->t[bsy-i_dy][bsx-i_dx]);    
    if ((bsy + 3*i_dy > 0) && (bsy + 3*i_dy < dd->ysiz))
        if ((bsx + 3*i_dx > 0) && (bsx + 3*i_dx < dd->xsiz))
            peri[1] = &(dd->t[bsy+(3*i_dy)][bsx+(3*i_dx)]);
            
    // mask off fake walls--they get set later  
    peri[0]->SCRATCH = 0;
    peri[1]->SCRATCH = 0;
    
    // no expar here
    peri[0]->exparrep = 0;
    peri[1]->exparrep = 0;
        
    while (ccnt<12) {
        struct dtile *tbl = &full;
        int u;
        int sx = (ccnt/3)*o_dx + (ccnt%3)*i_dx;
        int sy = (ccnt/3)*o_dy + (ccnt%3)*i_dy;
        int vy = ccnt/3;
        int vx = ccnt%3;
        
        if (bsx+sx >= 0 && bsx+sx < dd->xsiz) {
            if (bsy+sy >= 0 && bsy+sy < dd->ysiz) {
                int u;
                
                tbl = &(dd->t[bsy+sy][bsx+sx]);
                tbl->exparrep = 0;
                
                // do i need to draw an external party representation?
                for (u=0;u<4;++u) {
                    if (gd.p_lev[u] == lev) {
                        if (gd.p_x[u] == bsx+sx &&
                            gd.p_y[u] == bsy+sy)
                        {
                            tbl->exparrep = u + 1;
                        }
                    }
                }
            }
        }
            
        vis[vy][vx] = tbl;
        vis[vy][vx]->SCRATCH = 0;
        memcpy(&(copy_tile[vy][vx]), tbl, sizeof(struct dtile));  
        
        ++ccnt;
    }
    
    // set up screen
    dungeonclip(scx, bx, by);
    acquire_bitmap(scx);
    
    // take a poll of what's in the distance 
    // and alter the copied version of the wallset table accordingly 
    // (except if we're drawing iteratively, which confuses everything) 
    if (!b_iter) {
        for (xc=2;xc>=0;--xc) {
            struct dtile *ct = vis[0][xc];
            if (ct->altws[DIR_CENTER] && (!DrawWall(ct) || AlwaysDraw(ct))) {
                int ws_tbl_cset = ct->altws[DIR_CENTER];          
                if (!majority_floor_set)
                    majority_floor_set = ws_tbl_cset;
                if (ws_tbl_cset && majority_floor_set == ws_tbl_cset)    
                    floor_votes++;                   
            }
        }
    }
    if (floor_votes == 3) {
        unsigned int old_ws_num = cwsa_num + 1;
        unsigned int new_ws_num = majority_floor_set;
        int i;        
        for(i=0;i<12;++i) {
            int vy = i/3;
            int vx = i%3;
            
            if (!copy_tile[vy][vx].altws[DIR_CENTER]) {
                copy_tile[vy][vx].altws[DIR_CENTER] = old_ws_num;
            } else if (copy_tile[vy][vx].altws[DIR_CENTER] == new_ws_num) {
                copy_tile[vy][vx].altws[DIR_CENTER] = 0;
            }
        }
        
        cwsa_num = new_ws_num - 1;
        cwsa = ws_tbl[cwsa_num];
    }
    
     // grab floor and ceiling
    if (gfxctl.floor_override)
        floor_bitmap = gfxctl.floor_override;
    else
        floor_bitmap = cwsa->wall[WALLBMP_FLOOR];    
    roof_bitmap = cwsa->wall[WALLBMP_ROOF];
    
    lod_use(floor_bitmap);
    lod_use(roof_bitmap);
       
    // get coords
    if (floor_bitmap->numframes > 1) {
        gfr = (gd.framecounter / floor_bitmap->framedelay) %
            (floor_bitmap->numframes);
        floor_size = floor_bitmap->c_b[gfr]->h;
    } else
        floor_size = floor_bitmap->b->h;
    
    // blit roof
    if (gfxctl.roof_override) {
        int flipnum = (gfxctl.override_flip & OV_ROOF) && oddeven;
        if (oddeven && gfxctl.roof_alt_override)
            Override_Bmp = gfxctl.roof_alt_override;
        else
            Override_Bmp = gfxctl.roof_override;
        roof_bitmap = Override_Bmp;
        ablit(scx, WALLIDX_OVERRIDE, bx, by, gfr, flipnum);
    } else
        ablit(scx, WALLBMP_ROOF, bx, by, gfr, oddeven);
    
  
    // draw modified sections of ceiling  
    for (pz=0;pz<=1;++pz) {
        struct dtile *pct = peri[pz];
        if (pct->altws[DIR_CENTER] && (!DrawWall(pct) || AlwaysDraw(pct))) {
            trapblit(1, scx, bx, by, peri[pz], -1 + 4*pz, 0, oddeven, 0, floor_bitmap, roof_bitmap);
        }
    }
    for (yc=0;yc<=3;++yc) {
        for (xc=2;xc>=0;--xc) {
            struct dtile *ct = &(copy_tile[yc][xc]);
            if (ct->altws[DIR_CENTER] && (!DrawWall(ct) || AlwaysDraw(ct))) {
                trapblit(1, scx, bx, by, ct, xc, yc, oddeven, 0, floor_bitmap, roof_bitmap);
            }
        }
        // do only the distance if iterative mode
        if (b_iter)
            break;
    }
    
    // blit floor
    if (gfxctl.floor_override) {
        int flipnum = (gfxctl.override_flip & OV_FLOOR) && oddeven;
        if (oddeven && gfxctl.floor_alt_override)
            Override_Bmp = gfxctl.floor_alt_override;
        else
            Override_Bmp = gfxctl.floor_override;
        floor_bitmap = Override_Bmp;
        ablit(scx, WALLIDX_OVERRIDE, bx,
            by + (272 - floor_size), gfr, flipnum);
    } else {
        ablit(scx, WALLBMP_FLOOR, bx,
            by + (272 - floor_size), gfr, oddeven);
    }
    
    // draw modified sections of floor  
    for (pz=0;pz<=1;++pz) {
        struct dtile *pct = peri[pz];
        if (pct->altws[DIR_CENTER] && (!DrawWall(pct) || AlwaysDraw(pct))) {
            trapblit(0, scx, bx, by, peri[pz], -1 + 4*pz, 0, oddeven, 0, floor_bitmap, roof_bitmap);
        }
    }
    for (yc=0;yc<=3;++yc) {
        for (xc=2;xc>=0;--xc) {
            struct dtile *ct = &(copy_tile[yc][xc]);
            if (ct->altws[DIR_CENTER] && (!DrawWall(ct) || AlwaysDraw(ct))) {
                trapblit(0, scx, bx, by, ct, xc, yc, oddeven, 0, floor_bitmap, roof_bitmap);
            }
        }
    }
    
    // pop the stored parameters (nothing may have changed)
    cwsa_num = stored_cwsa_num;
    cwsa = stored_cwsa;

    // draw periphery walls
    if (tlight >= 0) {
        awall = WALLBMP_FARWALL3 + aoddeven;
        
        if (DrawWall(peri[0])) {
            ChooseSet(peri[0]->altws, fdl);
            ablit(scx, awall, bx, by+50, gfr, 0);
        }
        
        if (DrawWall(peri[1])) {
            ChooseSet(peri[1]->altws, fdr);
            ablit(scx, awall, OSIDE(bx), by+50, gfr, 1);
        }
    }
    
    Psquarehack = DrawWall(vis[2][1]);
       
    // draw back walls
    frontsquarehack = DrawWall(vis[0][1]);
    if (tlight >= 0) {
        renddecals(scx, vis[0][0], bx+64, by+137, 2, SIDE_VIEW);

        awall = WALLBMP_FRONT3;
        ChooseSet(vis[0][0]->altws, fdf);
        
        if (DrawWall(vis[0][0])) {

            if (cwsa->type == WST_LR_SEP) {
                ablit(scx, XWALLBMP_LEFT3 + aoddeven + 0xFF, bx, by+50, 0, 0);
            } else {
                cropblit(scx, awall, bx, by+50, gfr, aoddeven, 42*oddeven,
                    42*aoddeven, 1+aoddeven);
            }

            ChooseSet(vis[0][0]->altws, fdl);
            ablit(scx, WALLBMP_PERS3 + aoddeven, bx+148, by+50, gfr, 0);
            
            rendwall(scx, vis[0][0], (fdir+2)%4, bx+72, by+WD2Y, 2, 0);           
            rendwall(scx, vis[0][0], (fdir+1)%4, bx+174, by+78, 2, SIDE_VIEW);
            
        } else {
            MONSTERXBASE = bx+102;
            
            rendssmonster(scx, vis[0][0], fdir, bx+LD2X0, by+D2YF, 2, 0, 48);
            rendssmonster(scx, vis[0][0], (fdir+1)%4, bx+LD2X1, by+D2YF, 2, 0, 48);
            
            // uprights
            rendfloor(scx, vis[0][0], DIR_CENTER, bx, by+142, 2, SIDE_VIEW);
            
            // farthest visible object -- left side
            rendfloor(scx, vis[0][0], (fdir+3)%4, bx+52, by+142, 5, LEFT_SIDE);
            rendfloor(scx, vis[0][0], (fdir+2)%4, bx+122, by+142, 5, LEFT_SIDE);
            
            rendmonster(scx, vis[0][0], DIR_CENTER, MONSTERXBASE, by+142, 2, 0);
            
            rendmonster(scx, vis[0][0], (fdir+3)%4, bx+LD2X3, by+D2YC, 2, 0);
            rendmonster(scx, vis[0][0], (fdir+2)%4, bx+LD2X2, by+D2YC, 2, 0);
            
            render_deferred(scx);
            rendhaze(scx, vis[0][0], bx, by+50, 148, 102, 0, 0, 2);
            if (!(DrawWall(vis[0][1]))) {
                awall = WALLBMP_PERS3;
                rendhaze(scx, vis[0][0], bx+148, by+50, 22, 102, awall, 0, 2);
            }
        }

        renddecals(scx, vis[0][2], bx+384, by+137, 2, SUBWIDTH|SIDE_VIEW);
        awall = WALLBMP_FRONT3;
        ChooseSet(vis[0][2]->altws, fdf);
        
        if (DrawWall(vis[0][2])) {
            
            if (cwsa->type == WST_LR_SEP) {
                ablit(scx, XWALLBMP_LEFT3 + oddeven + 0xFF, bx, by+50, -1, 1);
            } else {
                cropblit(scx, awall, OSIDE(bx+42), by+50, gfr, oddeven,
                    42*oddeven, 42*aoddeven, 3+aoddeven);
            }
            
            awall = WALLBMP_PERS3 + oddeven;
            ChooseSet(vis[0][2]->altws, fdr);
            
            ablit(scx, awall, OSIDE(bx-148), by+50, gfr, 1);
            
            rendwall(scx, vis[0][2], (fdir+2)%4, bx+376, by+WD2Y, 2, 0);            
            rendwall(scx, vis[0][2], (fdir+3)%4, bx+274, by+78, 2, SUBWIDTH|SIDE_VIEW);
        } else { 
            MONSTERXBASE = bx+346;
               
            rendssmonster(scx, vis[0][2], fdir, bx+RD2X0, by+D2YF, 2, SUBWIDTH, 48);
            rendssmonster(scx, vis[0][2], (fdir+1)%4, bx+RD2X1, by+D2YF, 2, SUBWIDTH, 48);
            
            rendfloor(scx, vis[0][2], DIR_CENTER, bx+448, by+142, 2, 
                SUBWIDTH|SIDE_VIEW);
                          
            // farthest visible object -- right side
            rendfloor(scx, vis[0][2], (fdir+3)%4, bx+326, by+142, 5, RIGHT_SIDE|SUBWIDTH);
            rendfloor(scx, vis[0][2], (fdir+2)%4, bx+396, by+142, 5, RIGHT_SIDE|SUBWIDTH);
            
            rendmonster(scx, vis[0][2], DIR_CENTER, MONSTERXBASE, by+142, 2, SUBWIDTH);
            
            rendmonster(scx, vis[0][2], (fdir+3)%4, bx+RD2X3, by+D2YC, 2, SUBWIDTH);
            rendmonster(scx, vis[0][2], (fdir+2)%4, bx+RD2X2, by+D2YC, 2, SUBWIDTH);

            render_deferred(scx);

            rendhaze(scx, vis[0][2], O_HSIDE(bx+42), by+50, 148, 102, 0, 0, 2);
            if (!(DrawWall(vis[0][1]))) {
                awall = WALLBMP_PERS3;
                rendhaze(scx, vis[0][2], O_HSIDE(bx-148), by+50, 22, 102, awall, SUBWIDTH, 2);
            }
        }
                
        awall = WALLBMP_FRONT3;
        ChooseSet(vis[0][1]->altws, fdf);
        
        renddecals(scx, vis[0][1], bx+224, by+137, 2, 0);
        if (DrawWall(vis[0][1])) {
            
            if (cwsa->type == WST_LR_SEP) {
                ablit(scx, awall, bx+148, by+50, gfr, aoddeven);
            } else {
                // does this look right?
                cropblit(scx, awall, bx+148, by+50, gfr, aoddeven,
                    14+(10*aoddeven), 14+(10*oddeven), 5+aoddeven);
            }
                
            // now i can actually center things right?!
            rendwall(scx, vis[0][1], (fdir+2)%4, bx+224, by+WD2Y, 2, 0);
        
        } else {
            MONSTERXBASE = bx+224;
            
            rendssmonster(scx, vis[0][1], fdir, bx+D2X0, by+D2YF, 2, 0, 48);
            rendssmonster(scx, vis[0][1], (fdir+1)%4, bx+D2X1, by+D2YF, 2, SUBWIDTH, 48);
            
            gd.farcloudhack = 1;
            rendfloor_cloudsonly(scx, vis[0][1], fdir, bx+198, by+138, 5, 0);
            rendfloor_cloudsonly(scx, vis[0][1], (fdir+1)%4, bx+250, by+138, 5, SUBWIDTH);
            gd.farcloudhack = 0;
            
            // floor uprights (doorframes etc.) in middle of tile
            rendfloor(scx, vis[0][1], DIR_CENTER, bx+224, by+142, 2, 0);
                        
            // farthest visible object
            rendfloor(scx, vis[0][1], (fdir+3)%4, bx+190, by+142, 5, 0);
            rendfloor(scx, vis[0][1], (fdir+2)%4, bx+258, by+142, 5, SUBWIDTH);
            
            rendmonster(scx, vis[0][1], DIR_CENTER, MONSTERXBASE, by+142, 2, 0);
            
            rendmonster(scx, vis[0][1], (fdir+3)%4, bx+D2X3, by+D2YC, 2, 0);
            rendmonster(scx, vis[0][1], (fdir+2)%4, bx+D2X2, by+D2YC, 2, SUBWIDTH);
            
            render_deferred(scx);
            rendhaze(scx, vis[0][1], bx+148, by+50, 152, 102, 0, 0, 2);
        }    
    }
    
    // draw middle walls
    frontsquarehack = DrawWall(vis[1][1]);
    if (tlight >= 0) {
        renddecals(scx, vis[1][0], bx, by+165, 1, SIDE_VIEW);

        awall = WALLBMP_FRONT2;
        ChooseSet(vis[1][0]->altws, fdf);
        
        DRAW_ITERCEIL(0, 1);
        if (DrawWall(vis[1][0])) {

            if (cwsa->type == WST_LR_SEP) {
                ablit(scx, XWALLBMP_LEFT2 + oddeven + 0xFF, bx, by+40, 0, 0);
            } else {
                cropblit(scx, awall, bx, by+40, gfr, oddeven,
                    92*aoddeven, 92*oddeven, 7+oddeven);
            }
            
            ChooseSet(vis[1][0]->altws, fdl);
            ablit(scx, WALLBMP_PERS2 + oddeven, bx+120, by+40, gfr, 0);
            
            rendwall(scx, vis[1][0], (fdir+2)%4, bx+15, by+74, 1, 0);            
            rendwall(scx, vis[1][0], (fdir+1)%4, bx+156, by+78, 1, SIDE_VIEW);
            
            rendfloor_cloudsonly(scx, vis[1][0], DIR_CENTER, bx, by+174, 1, SIDE_VIEW);
            render_deferred(scx);
            
        } else {
            MONSTERXBASE = bx+48;
            
            // far left side scale 2
            rendfloor(scx, vis[1][0], fdir, bx+24, by+158, 2, LEFT_SIDE | BOTTOM_ZONE);
            rendfloor(scx, vis[1][0], (fdir+1)%4, bx+104, by+158, 2, LEFT_SIDE | BOTTOM_ZONE);
            
            rendssmonster(scx, vis[1][0], fdir, bx+LD1X0, by+D1YF, 1, 0, 32);
            rendssmonster(scx, vis[1][0], (fdir+1)%4, bx+LD1X1, by+D1YF, 1, 0, 32);
            
            render_deferred(scx);
            
            rendfloor(scx, vis[1][0], DIR_CENTER, bx, by+174, 1, SIDE_VIEW);
            
            rendfloor(scx, vis[1][0], fdir, bx+24, by+158, 2, LEFT_SIDE | TOP_ZONE);
            rendfloor(scx, vis[1][0], (fdir+1)%4, bx+104, by+158, 2, LEFT_SIDE | TOP_ZONE);
            
            // left side scale 1
            rendfloor(scx, vis[1][0], (fdir+2)%4, bx+84, by+174, 4, LEFT_SIDE);
            
            rendmonster(scx, vis[1][0], DIR_CENTER, MONSTERXBASE, by+176, 1, 0);
            
            rendmonster(scx, vis[1][0], (fdir+3)%4, bx+LD1X3, by+D1YC, 1, 0);
            rendmonster(scx, vis[1][0], (fdir+2)%4, bx+LD1X2, by+D1YC, 1, 0);
            
            render_deferred(scx);
            
            rendhaze(scx, vis[1][0], bx, by+40, 120, 142, 0, 0, 1);
            if (!(DrawWall(vis[1][1]))) {
                awall = WALLBMP_PERS2;
                rendhaze(scx, vis[1][0], bx+120, by+40, 28, 142, awall, 0, 1);
            }
        }
        
        renddecals(scx, vis[1][2], bx+448, by+165, 1, SUBWIDTH|SIDE_VIEW);
        awall = WALLBMP_FRONT2;
        ChooseSet(vis[1][2]->altws, fdf);
        
        DRAW_ITERCEIL(2, 1);
        if (DrawWall(vis[1][2])) {

            if (cwsa->type == WST_LR_SEP) {
                ablit(scx, XWALLBMP_LEFT2 + aoddeven + 0xFF, bx, by+40, -1, 1);
            } else {
                cropblit(scx, awall, OSIDE(bx+92), by+40, gfr, aoddeven,
                    92*aoddeven, 92*oddeven, 9+oddeven);
            }
            
            awall = WALLBMP_PERS2 + aoddeven;
            ChooseSet(vis[1][2]->altws, fdr);
            ablit(scx, awall, OSIDE(bx-120), by+40, gfr, 1);

            rendwall(scx, vis[1][2], (fdir+2)%4, bx+433, by+74, 1, 0);            
            rendwall(scx, vis[1][2], (fdir+3)%4, bx+292, by+78, 1, SUBWIDTH|SIDE_VIEW);
            
            rendfloor_cloudsonly(scx, vis[1][2], DIR_CENTER, bx+448, by+174, 1,
                SUBWIDTH|SIDE_VIEW);
                
            render_deferred(scx);
            
        } else {
            MONSTERXBASE = bx+400;
               
            // far right side scale 2
            rendfloor(scx, vis[1][2], fdir, bx+344, by+158, 2, RIGHT_SIDE|SUBWIDTH | BOTTOM_ZONE);
            rendfloor(scx, vis[1][2], (fdir+1)%4, bx+424, by+158, 2, RIGHT_SIDE|SUBWIDTH | BOTTOM_ZONE);
            
            rendssmonster(scx, vis[1][2], fdir, bx+RD1X0, by+D1YF, 1, SUBWIDTH, 32);
            rendssmonster(scx, vis[1][2], (fdir+1)%4, bx+RD1X1, by+D1YF, 1, SUBWIDTH, 32);
            
            render_deferred(scx);
            
            rendfloor(scx, vis[1][2], DIR_CENTER, bx+448, by+174, 1, 
                SUBWIDTH|SIDE_VIEW);
                
            rendfloor(scx, vis[1][2], fdir, bx+344, by+158, 2, RIGHT_SIDE|SUBWIDTH | TOP_ZONE);
            rendfloor(scx, vis[1][2], (fdir+1)%4, bx+424, by+158, 2, RIGHT_SIDE|SUBWIDTH | TOP_ZONE);
            
            // right side scale 1
            rendfloor(scx, vis[1][2], (fdir+3)%4, bx+364, by+174, 4, RIGHT_SIDE|SUBWIDTH);
            
            rendmonster(scx, vis[1][2], DIR_CENTER, MONSTERXBASE, by+176, 1, SUBWIDTH);
            
            rendmonster(scx, vis[1][2], (fdir+3)%4, bx+RD1X3, by+D1YC, 1, SUBWIDTH);
            rendmonster(scx, vis[1][2], (fdir+2)%4, bx+RD1X2, by+D1YC, 1, SUBWIDTH);
            
            render_deferred(scx);

            rendhaze(scx, vis[1][2], O_HSIDE(bx+92), by+40, 144, 142, 0, 0, 1);
            if (!(DrawWall(vis[1][1]))) {
                awall = WALLBMP_PERS2;
                rendhaze(scx, vis[1][2], O_HSIDE(bx-120), by+40, 28, 142, awall, SUBWIDTH, 1);
            }
        }
                            
        awall = WALLBMP_FRONT2;
        ChooseSet(vis[1][1]->altws, fdf);
        renddecals(scx, vis[1][1], bx+224, by+165, 1, 0);
        MONSTERXBASE = bx+224;
        DRAW_ITERCEIL(1, 1);
        if (DrawWall(vis[1][1])) {

            if (cwsa->type == WST_LR_SEP) {
                ablit(scx, awall, bx+120, by+40, gfr, oddeven);
            } else {
                cropblit(scx, awall, bx+120, by+40, gfr, oddeven, 2, 2,
                    11+oddeven);
            }
                
            rendwall(scx, vis[1][1], (fdir+2)%4, bx+224, by+74, 1, 0);
            
            rendfloor_cloudsonly(scx, vis[1][1], DIR_CENTER, bx+224, by+174, 1, 0);
            render_deferred(scx);
        
        } else {
            // closest scale 2 -- back of tile
            rendfloor(scx, vis[1][1], fdir, bx+182, by+158, 2, BOTTOM_ZONE);
            rendfloor(scx, vis[1][1], (fdir+1)%4, bx+266, by+158, 2, SUBWIDTH | BOTTOM_ZONE);
            
            rendssmonster(scx, vis[1][1], fdir, bx+D1X0, by+D1YF, 1, 0, 32);
            rendssmonster(scx, vis[1][1], (fdir+1)%4, bx+D1X1, by+D1YF, 1, 0, 32);
            
            render_deferred(scx);
            
            // floor uprights (doorframes etc.) in middle of tile
            rendfloor(scx, vis[1][1], DIR_CENTER, bx+224, by+174, 1, 0);
            
            rendfloor(scx, vis[1][1], fdir, bx+182, by+158, 2, TOP_ZONE);
            rendfloor(scx, vis[1][1], (fdir+1)%4, bx+266, by+158, 2, SUBWIDTH | TOP_ZONE);
            
            // farthest scale 1 objects [4%3 = 1, for 6view hack]
            rendfloor(scx, vis[1][1], (fdir+3)%4, bx+172, by+174, 4, 0);
            rendfloor(scx, vis[1][1], (fdir+2)%4, bx+276, by+174, 4, SUBWIDTH);
            
            rendmonster(scx, vis[1][1], DIR_CENTER, MONSTERXBASE, by+176, 1, 0);
            
            rendmonster(scx, vis[1][1], (fdir+3)%4, bx+D1X3, by+D1YC, 1, 0);
            rendmonster(scx, vis[1][1], (fdir+2)%4, bx+D1X2, by+D1YC, 1, SUBWIDTH);
            
            render_deferred(scx);
            rendhaze(scx, vis[1][1], bx+120, by+40, 208, 142, 0, 0, 1);
        }        
    }
           
    // draw front walls
    frontsquarehack = DrawWall(vis[2][1]);
    sideobshack = (DrawWall(vis[3][0]) && DrawWall(vis[3][2]));

    awall = WALLBMP_FRONT1;
    ChooseSet(vis[2][0]->altws, fdf);
    
    renddecals(scx, vis[2][0], bx, by+207, 0, SIDE_VIEW);
    DRAW_ITERCEIL(0, 2);
    if (DrawWall(vis[2][0])) {

        if (cwsa->type == WST_LR_SEP) {
            ablit(scx, XWALLBMP_LEFT1 + aoddeven + 0xFF, bx, by+18, 0, 0);
        } else {
            cropblit(scx, awall, bx, by+18, gfr, aoddeven,
                320*oddeven, 320*aoddeven, 13+aoddeven);
        }
        
        ChooseSet(vis[2][0]->altws, fdl);
        ablit(scx, WALLBMP_PERS1 + aoddeven, bx+64, by+18, gfr, 0);
        
        rendwall(scx, vis[2][0], (fdir+2)%4, bx-94, by+72, 0, 0);             
        rendwall(scx, vis[2][0], (fdir+1)%4, bx+128, by+76, 0, SIDE_VIEW);
        
    } else {
        MONSTERXBASE = bx-44;
        
        // far left side
        rendfloor(scx, vis[2][0], (fdir+1)%4, bx+48, by+190, 1, LEFT_SIDE | BOTTOM_ZONE);
        
        rendssmonster(scx, vis[2][0], (fdir+1)%4, bx+36, by+208, 0, SIDE_VIEW, 30);
        
        render_deferred(scx);
        
        // floor uprights (doorframes etc.) 
        rendfloor(scx, vis[2][0], DIR_CENTER, bx, by+216, 0, SIDE_VIEW);
        
        rendfloor(scx, vis[2][0], (fdir+1)%4, bx+48, by+190, 1, LEFT_SIDE | TOP_ZONE);
        
        // close left side
        rendfloor(scx, vis[2][0], (fdir+2)%4, bx+16, by+216, 3, LEFT_SIDE);
        
        rendmonster(scx, vis[2][0], DIR_CENTER, bx-44, by+224, 0, 0);
        rendmonster(scx, vis[2][0], (fdir+2)%4, bx+4, by+240, 0, SIDE_VIEW);
        
        rendhaze(scx, vis[2][0], bx, by+18, 64, 222, 0, 0, 0);
        
        render_deferred(scx);
        if (!(DrawWall(vis[2][1]))) {
            awall = WALLBMP_PERS1;
            rendhaze(scx, vis[2][0], bx+64, by+18, 56, 222, awall, 0, 0);
        }    
    }
    
    awall = WALLBMP_FRONT1;
    ChooseSet(vis[2][2]->altws, fdf);
    
    renddecals(scx, vis[2][2], bx+448, by+207, 0, SUBWIDTH|SIDE_VIEW);
    DRAW_ITERCEIL(2, 2);
    if (DrawWall(vis[2][2])) {

        if (cwsa->type == WST_LR_SEP) {
            ablit(scx, XWALLBMP_LEFT1 + oddeven + 0xFF, bx, by+18, -1, 1);
        } else {
            cropblit(scx, awall, OSIDE(bx+320), by+18, gfr, oddeven,
                320*oddeven, 320*aoddeven, 15+aoddeven);
        }
        
        awall = WALLBMP_PERS1 + oddeven;
        ChooseSet(vis[2][2]->altws, fdr);
        ablit(scx, awall, OSIDE(bx-64), by+18, gfr, 1);
        
        rendwall(scx, vis[2][2], (fdir+2)%4, bx+(448+94), by+72, 0, 0);        
        rendwall(scx, vis[2][2], (fdir+3)%4, bx+320, by+76, 0, SUBWIDTH|SIDE_VIEW);
    
    } else {  
        MONSTERXBASE=bx+492;
        
        // far right side
        rendfloor(scx, vis[2][2], fdir, bx+400, by+190, 1, RIGHT_SIDE|SUBWIDTH | BOTTOM_ZONE);
        
        rendssmonster(scx, vis[2][2], fdir, bx+412, by+208, 0, SUBWIDTH|SIDE_VIEW, 30);
        
        render_deferred(scx);
        
        // floor uprights (doorframes etc.)
        rendfloor(scx, vis[2][2], DIR_CENTER, bx+448, by+216, 0, SUBWIDTH|SIDE_VIEW);
        
        rendfloor(scx, vis[2][2], fdir, bx+400, by+190, 1, RIGHT_SIDE|SUBWIDTH | TOP_ZONE);
        
        // close right side
        rendfloor(scx, vis[2][2], (fdir+3)%4, bx+432, by+216, 3, RIGHT_SIDE|SUBWIDTH);
        
        rendmonster(scx, vis[2][2], DIR_CENTER, MONSTERXBASE, by+224, 0, SUBWIDTH);
        rendmonster(scx, vis[2][2], (fdir+3)%4, bx+444, by+240, 0, SUBWIDTH|SIDE_VIEW);
        
        rendhaze(scx, vis[2][2], O_HSIDE(bx+320), by+18, 64, 222, 0, 0, 0);
        
        render_deferred(scx);
        if (!(DrawWall(vis[2][1]))) {
            awall = WALLBMP_PERS1;
            rendhaze(scx, vis[2][2], O_HSIDE(bx-64), by+18, 56, 222, awall, SUBWIDTH, 0);
        }       
    }
    
    // draw very front wall -- might do various tricks
    awall = WALLBMP_FRONT1;
    ChooseSet(vis[2][1]->altws, fdf);
    renddecals(scx, vis[2][1], bx+224, by+207, 0, 0);
    wall_show = DrawWall(vis[2][1]);
    if ((vis[2][1]->SCRATCH & 4) && (gd.gameplay_flags & GP_WINDOW_ON))
        wall_show = 0;
        
    MONSTERXBASE = bx+224;
    DRAW_ITERCEIL(1, 2);
    if (wall_show) {
        int xcrop_l = 30+(2*aoddeven);
        int xcrop_r = 32+(2*oddeven);
        
        if (gd.gameplay_flags & GP_WINDOW_ON) {
            struct animap *cwall = cwsa->wall[awall];
            int nwidth, nheight;
            int wsiz = cwsa->window->w;
            int hsiz;
            BITMAP *wt;

            nwidth = cwall->w - (xcrop_l + xcrop_r);
            if (cwall->numframes > 1)
                nheight = cwall->c_b[0]->h;
            else
                nheight = cwall->b->h;
            if (cwsa->type == WST_LR_SEP)
                nwidth = cwall->w;

            if (cwsa->window->numframes > 1)
                 hsiz = cwsa->window->c_b[0]->h;
            else
                 hsiz = cwsa->window->b->h;

            wt = create_bitmap(nwidth, nheight);


            if (cwsa->type == WST_LR_SEP) {
                ablit(wt, awall, 0, 0, gfr, aoddeven);
            } else {
                cropblit(wt, awall, 0, 0, gfr, aoddeven,
                    xcrop_l, xcrop_r, 17+aoddeven);
            }
            
            rendczwall(wt, vis[2][1], (fdir+2)%4,
                160, 54, 0, 0, 101*need_cz);
            
            draw_gfxicon(wt, cwsa->window, (nwidth/2)-(wsiz/2),
                (nheight/2)-(hsiz/2), 0, NULL);
            
            masked_blit(wt, scx, 0, 0, bx+64, by+18, wt->w, wt->h);
            
            destroy_bitmap(wt);          
        
        } else {
            if (cwsa->type == WST_LR_SEP) {
                ablit(scx, awall, bx+64, by+18, gfr, aoddeven);
            } else {
                cropblit(scx, awall, bx+64, by+18, gfr, aoddeven,
                    xcrop_l, xcrop_r, 17+aoddeven);
            }
            
            rendczwall(scx, vis[2][1], (fdir+2)%4,
                bx+224, by+72, 0, 0, 1*need_cz);
        }
        
        rendfloor_cloudsonly(scx, vis[2][1], DIR_CENTER,
            MONSTERXBASE, by+216, 0, 0);
        
        make_cz(39, bx+64, by+30, bx+320, by+120, (1 | vis[2][1]->SCRATCH));
        
    } else {   
        int actually_open = 1;
        
        if (vis[2][1]->w & 1)
            actually_open = 0;
        
        // far objects
        rendfloor(scx, vis[2][1], fdir, bx+162, by+190, 1, BOTTOM_ZONE);
        rendfloor(scx, vis[2][1], (fdir+1)%4, bx+286, by+190, 1, SUBWIDTH | BOTTOM_ZONE);
        
        rendssmonster(scx, vis[2][1], fdir,
            bx+176, by+208, 0, 0, 30);
        
        rendssmonster(scx, vis[2][1], (fdir+1)%4,
            bx+272, by+208, 0, SUBWIDTH, 30);
        
        render_deferred(scx);
            
        // floor uprights (doorframes etc.)
        rendczfloor(scx, vis[2][1], DIR_CENTER, MONSTERXBASE, by+216,
            0, 0, 255*need_cz);
            
        rendfloor(scx, vis[2][1], fdir, bx+162, by+190, 1, TOP_ZONE);
        rendfloor(scx, vis[2][1], (fdir+1)%4, bx+286, by+190, 1, SUBWIDTH | TOP_ZONE);
        
        // near (reachable) objects
        rendczfloor(scx, vis[2][1], (fdir+3)%4, bx+148, by+216, 3, 0, 5*need_cz*actually_open);
        if (actually_open)
            emptyzone(5*need_cz, 10, bx+140, by+216, 160, 56);
        
        rendczfloor(scx, vis[2][1], (fdir+2)%4, bx+300, by+216, 3, SUBWIDTH, 4*need_cz*actually_open);
        if (actually_open)
            emptyzone(4*need_cz, 10, bx+308, by+216, 160, 56);
        
        rendczmonster(scx, vis[2][1], DIR_CENTER, 
            bx+224, by+224, 0, 0, /*hack -->*/ 94, 18*need_cz);
        
        rendczmonster(scx, vis[2][1], (fdir+3)%4,
            bx+156, by+240, 0, 0, 5, 17*need_cz);
        
        rendczmonster(scx, vis[2][1], (fdir+2)%4,
            bx+292, by+240, 0, SUBWIDTH, 4, 16*need_cz);
        
        // throwzones
        if (need_cz) {
            make_cz(38, bx+64, by+20, 160, 140, 0);
            make_cz(39, bx+224, by+20, 160, 140, 0);
        }   
    }
    
    // draw side walls    
    awall = WALLBMP_PERS0 + oddeven;
    ChooseSet(vis[3][0]->altws, fdl);
    renddecals(scx, vis[3][0], bx, by+272, 0, ZERO_RANGE|SIDE_VIEW);
    DRAW_ITERCEIL(0, 3);
    if (DrawWall(vis[3][0])) {
        ablit(scx, awall, bx, by, gfr, 0);

        rendwall(scx, vis[3][0], (fdir+1)%4, bx, by, 0, ZERO_RANGE|SIDE_VIEW);
    }
    
    awall = WALLBMP_PERS0 + aoddeven;
    ChooseSet(vis[3][2]->altws, fdr);
    renddecals(scx, vis[3][2], bx+448, by+272, 0, ZERO_RANGE|SIDE_VIEW|SUBWIDTH);
    DRAW_ITERCEIL(2, 3);
    if (DrawWall(vis[3][2])) {
        ablit(scx, awall, OSIDE(bx), by, gfr, 1);

        rendwall(scx, vis[3][2], (fdir+3)%4,
            bx+448, by, 0, ZERO_RANGE|SIDE_VIEW|SUBWIDTH);
    }
     
    render_deferred(scx);
    ChooseSet(vis[2][1]->altws, fdf);
    rendhaze(scx, vis[2][1], bx+64, by+18, 320, 222, 0, 0, 0);
    
    awall = WALLBMP_PERS0 + oddeven;
    ChooseSet(vis[3][0]->altws, fdl);
    if (!(DrawWall(vis[3][0]))) {
        MONSTERXBASE=bx-80;
        
        rendfloor(scx, vis[3][0], DIR_CENTER, bx, by+272, 0, ZERO_RANGE|SIDE_VIEW);
        rendmonster(scx, vis[3][0], (fdir+1)%4, MONSTERXBASE, by+244, 0, 0);
        rendmonster(scx, vis[3][0], DIR_CENTER, bx-90, by+262, 0, 0);
        rendhaze(scx, vis[3][0], bx, by, 64, 272, awall, 0, 0);         
    }
    
    awall = WALLBMP_PERS0 + aoddeven;
    ChooseSet(vis[3][2]->altws, fdr);
    if (!(DrawWall(vis[3][2]))) {
        MONSTERXBASE=bx+528;
        
        rendfloor(scx, vis[3][2], DIR_CENTER, bx+448, by+272, 0, 
            ZERO_RANGE|SIDE_VIEW|SUBWIDTH);
        rendmonster(scx, vis[3][2], fdir, MONSTERXBASE, by+244, 0, SUBWIDTH);
        rendmonster(scx, vis[3][2], DIR_CENTER, bx+538, by+262, 0, SUBWIDTH);
        rendhaze(scx, vis[3][2], O_HSIDE(bx), by, 64, 272, awall, SUBWIDTH, 0);
    }
  
    DRAW_ITERCEIL(1, 3);
    // zero-range flooritems
    renddecals(scx, vis[3][1], bx+224, by+272, 0, ZERO_RANGE);
    rendfloor(scx, vis[3][1], DIR_CENTER, bx+224, by+272, 0, ZERO_RANGE);
    
    // same-tile objects
    rendczfloor(scx, vis[3][1], fdir, bx+128, by+256, 0, ZERO_RANGE, 2*need_cz);
    emptyzone(2*need_cz, 10, bx+124, by+272, 200, 56);
    
    rendczfloor(scx, vis[3][1], (fdir+1)%4, bx+320, by+256, 0,
         ZERO_RANGE|SUBWIDTH, 3*need_cz);
    emptyzone(3*need_cz, 10, bx+324, by+272, 200, 56);
    
    render_deferred(scx);
        
    ChooseSet(vis[3][1]->altws, fdf);
    rendhaze(scx, vis[3][1], bx, by, VIEWPORTW, VIEWPORTH, 0, ZERO_RANGE, 0);

    release_bitmap(scx);
    
    if (tlight < 90) {
        int stlight = ((tlight + 1) / 2) * 2;
        int blv = (tlight < 90)? 180-(2*stlight) : 0;
        void *darkdun = fancy_darkdun;
        void *darkdun24 = nocyan_darkdun24;
        
        if (gd.glowmask == NULL) {      
            set_blender_mode(darkdun, darkdun, darkdun24, 0, 0, 0, blv);  
            drawing_mode(DRAW_MODE_TRANS, light_buffer, 0, 0);
            rectfill(light_buffer, bx, by, bx+447, by+271, 0);
            drawing_mode(DRAW_MODE_SOLID, light_buffer, 0, 0);
        } else {
            int ix, iy;
            for (iy=0;iy<VIEWPORTH;++iy) {
                for (ix=0;ix<VIEWPORTW;++ix) {
                    int tl = ix + iy*VIEWPORTW;
                    if (gd.glowmask[tl] != 255) {
                        int p = dsbgetpixel(light_buffer, bx+ix, by+iy);
                        int p_r = getr(p);
                        int p_g = getg(p);
                        int p_b = getb(p);
                        int am_blv = blv - gd.glowmask[tl];
                            
                        if (am_blv > 0) {              
                            p_r = p_r - am_blv;
                            if (p_r < 0) p_r = 0;
                            p_g = p_g - am_blv;
                            if (p_g < 0) p_g = 0;
                            p_b = p_b - am_blv;
                            if (p_b < 0) p_b = 0;
                        
                            dsbputpixel(light_buffer, bx+ix, by+iy, makecol(p_r, p_g, p_b));
                        }
                    } 
                }
            }
        }
        used_softmode = 1;
    }
    
    if (gd.distort_func) {
        distort_viewport(gd.distort_func, scx, bx, by, gd.framecounter);
    }
    
    if (gd.glowmask) {
        dsbfree(gd.glowmask);
        gd.glowmask = NULL;  
    }
    
    RETURN(used_softmode);
}

void draw_luaclickzones(int softmode) {
    BITMAP *scx;
    int i=0;
    int srn;
    
    onstack("draw_luaclickzones");
    
    scx = find_rendtarg(softmode);
    
    while(i<lua_cz_n) {
        if (lua_cz[i].w) {
            rect(scx, lua_cz[i].x, lua_cz[i].y, 
                lua_cz[i].x+lua_cz[i].w, lua_cz[i].y+lua_cz[i].h, makecol(0, 255, 255));
        }
        i++;   
    }
    
    for(srn=0;srn<NUM_SR;++srn) {
        if (gfxctl.SR[srn].flags & SRF_ACTIVE) {
            i = 0;        
            while(i<gfxctl.SR[srn].cz_n) {
                if (gfxctl.SR[srn].cz[i].w) {
                    rect(scx, gfxctl.SR[srn].cz[i].x, gfxctl.SR[srn].cz[i].y,
                        gfxctl.SR[srn].cz[i].x+gfxctl.SR[srn].cz[i].w,
                            gfxctl.SR[srn].cz[i].y+gfxctl.SR[srn].cz[i].h, makecol(255, 255, 0));
                }
                i++;
            }
        }
    }
    
    for(srn=0;srn<4;++srn) {
        if (gfxctl.ppos_r[srn].flags & SRF_ACTIVE) {
            i = 0;        
            while(i<gfxctl.ppos_r[srn].cz_n) {
                if (gfxctl.ppos_r[srn].cz[i].w) {
                    rect(scx, gfxctl.ppos_r[srn].cz[i].x, gfxctl.ppos_r[srn].cz[i].y,
                        gfxctl.ppos_r[srn].cz[i].x+gfxctl.ppos_r[srn].cz[i].w,
                            gfxctl.ppos_r[srn].cz[i].y+gfxctl.ppos_r[srn].cz[i].h, makecol(192, 255, 64));
                }
                i++;
            }
        }
    }
    
    VOIDRETURN();
}

void draw_clickzones(int softmode) {
    BITMAP *scx;
    int i=0;
    
    scx = find_rendtarg(softmode);
    
    while(i<NUM_ZONES) {
        int cid = makecol(0, 255, 0);
        if (cid >= 19 && cid <= 38)
            cid = makecol(255, 255, 255);
        if (cz[i].w) {
            rect(scx, cz[i].x, cz[i].y, 
                cz[i].x+cz[i].w, cz[i].y+cz[i].h, cid);
        }
        i++;   
    }
    
    draw_luaclickzones(softmode);
}

void destroy_subrend(void) {
    
    clear_lua_cz();
    
    if (gfxctl.subrend != NULL) {
        destroy_animap(gfxctl.subrend);
        gfxctl.subrend =  NULL;
    }
    gfxctl.do_subrend = 0;
}

int render_inventory_view(int softmode, int pt) {
    char o_name[30];
    char o_name2[30];
    char o_name3[30];
    int bx, by;
    BITMAP *scx;
    struct animap *inv_a;
    BITMAP *inv_s = NULL;
    int pcidx;
    struct champion *me;
    int i, cl;
    int norend = 0;
    int bgc = makecol(182, 182, 182);
    int lbgc;
    
    onstack("render_inventory_view");
    
    scx = find_rendtarg(softmode);
    memset(cz, 0, sizeof(cz));
    gd.need_cz = 0;    

    bx = gd.vxo;
    by = gd.vyo + gd.vyo_off;
    pcidx = gd.party[gd.who_look] - 1;
    me = &(gd.champs[pcidx]);
    
    setup_background(scx, 0);
    
    if (me->invscr)
        inv_a = me->invscr;
    else
        inv_a = gii.inventory_background;
     
    lod_use(inv_a);
    if (inv_a) {
        inv_s = animap_subframe(inv_a, 0);
    }
        
    cl = leader_color_type(gd.who_look, 1);
    
    acquire_bitmap(scx);
    if (inv_s)
        blit(inv_s, scx, 0, 0, bx, by, 448, 272);
        
    call_lua_inventory_renderer(scx, pcidx+1, bx, by, cl);    
    //textout_ex(scx, font, o_name, bx+6, by+6, cl, -1);
    release_bitmap(scx);
    /*
    for (i=0;i<3;i++) {
        int bv = me->bar[i];
        int dbv = bv/10;
        int mbv = me->maxbar[i];
        int dmbv = mbv/10;
        
        // health + stamina have values of 1-9 displayed as 1
        // mana has the value displayed as 0, so spellcasting doesn't seem odd
        if (i <= 1) {
            if (bv > 0 && dbv == 0)
                dbv = 1;
    
            if (mbv > 0 && dmbv == 0)
                dmbv = 1;
        }
        
        sprintf(o_name, "%d", dbv);
        sprintf(o_name2, "%d", dmbv);
        
        acquire_bitmap(scx);
        textout_right_ex(scx, font, o_name, bx+146, by+16*i+224, bgc, -1);
        textout_right_ex(scx, font, o_name2, bx+194, by+16*i+224, bgc, -1);
        textout_ex(scx, font, "/", bx+146, by+16*i+224, bgc, -1);
        textout_ex(scx, font, gd.barname[i], bx+10, by+16*i+224, bgc, -1);
        release_bitmap(scx);
    }
    
    if (me->custom_v & CUSV_LOAD_COLOR)
        lbgc = me->custom_load_color_value;
    else
        lbgc = bgc;
    
    sprintf(o_name, "%d", (me->load/10));
    sprintf(o_name2, "%d", (me->load%10));
    sprintf(o_name3, "%d", (me->maxload/10));
    acquire_bitmap(scx);
    textout_ex(scx, font, "LOAD", bx+208, by+256, lbgc, -1);
    textout_ex(scx, font, ".", bx+332, by+256, lbgc, -1);
    textout_ex(scx, font, "/", bx+356, by+256,lbgc, -1);
    textout_ex(scx, font, "KG", bx+416, by+256, lbgc, -1);
    textout_right_ex(scx, font, o_name, bx+332, by+256, lbgc, -1);
    textout_ex(scx, font, o_name2, bx+344, by+256, lbgc, -1);
    textout_right_ex(scx, font, o_name3, bx+404, by+256, lbgc, -1);
    */
    
    for (i=0;i<gii.max_invslots;++i) {
        draw_i_pl(scx, pcidx, i);
        // clean this out every frame to avoid lingering junk
        me->inv_queue_data[i] = 0;
    }
    
    if (gd.who_method == (gd.who_look+1)) {
        int mslot = gd.method_loc;
        int vx = gii.inv[mslot].x;
        int vy = gii.inv[mslot].y;
        
        if (vx || vy) {
            draw_gfxicon(scx, gii.sel_box, bx + vx, by + vy, 0, NULL);
        }
    }
        
    if (me->inv[0] && (pt || gfxctl.do_subrend)) {
        destroy_subrend();
        gd.lua_bool_hack = 1;
        call_member_func(me->inv[0], "subrenderer", LHFALSE);
    } else if (!me->inv[INV_R_HAND])
        destroy_subrend();
     
    gd.infobox = 0;
       
    if (gd.mouse_mode >= MM_EYELOOK) {
        unsigned int zoneobj = (gd.mouse_mode - MM_EYELOOK);
        
        if (gii.eye.img_ex)
            draw_gfxicon(scx, gii.eye.img_ex, bx+gii.eye.x, by+gii.eye.y, 0, 0);
        else if (gii.eye.img)
            draw_gfxicon(scx, gii.eye.img, bx+gii.eye.x, by+gii.eye.y, 0, 0);
        
        if (zoneobj) {
            
            destroy_subrend();
            gd.lua_bool_hack = 1;
            call_member_func(zoneobj, "subrenderer", LHTRUE);
            
            if (!gfxctl.subrend)
                draw_obj_infobox(scx, bx+gii.srx, by+gii.sry, pt);
                
            gfxctl.do_subrend = 1;
                
        } else
            draw_inv_statsbox(scx, bx+gii.srx, by+gii.sry, pt);
        
        norend = 1;
    } else {
        if (gii.eye.img)
            draw_gfxicon(scx, gii.eye.img, bx+gii.eye.x, by+gii.eye.y, 0, 0);
    }
    
    draw_eye_border(scx, me, bx+gii.eye.x, by+gii.eye.y);

    if (gii.mouth.img) {
        draw_gfxicon(scx, gii.mouth.img, bx+gii.mouth.x, by+gii.mouth.y, 0, 0);
        draw_mouth_border(scx, me, bx+gii.mouth.x, by+gii.mouth.y);
    }
                    
    if (gfxctl.subrend)
        draw_sprite(scx, gfxctl.subrend->b, bx+gii.srx, by+gii.sry);
    else {
        if (gfxctl.i_subrend && !norend) {
            draw_sprite(scx, gfxctl.i_subrend, bx+gii.srx+6, by+gii.sry);

            if (gd.gui_mode == GUI_NAME_ENTRY)
                draw_name_entry(scx);
        }
    }
    release_bitmap(scx);
    
    make_cz(ZONE_EYE, bx+gii.eye.x, by+gii.eye.y, 35, 35, 0);
    make_cz(ZONE_EXITBOX, bx+gii.exit.x, by+gii.exit.y, 17, 17, 0);
      
    if (*gd.gl_viewstate == VIEWSTATE_CHAMPION) {
        if (gd.gui_mode == GUI_NAME_ENTRY) {
            memset(cz, 0, sizeof(cz));
            setup_rei_name_entry_cz();
        } else {
            if (!gfxctl.subrend)
                setup_res_rei_cz();
        }
    } else
        setup_misc_inv(scx, bx, by, pt);

    draw_champions(softmode, scx, gd.party[gd.who_look]);
    
    RETURN(0);   
}    
