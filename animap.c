#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "gproto.h"

#define TRAPXS      30
#define TRAPLXS     36
#define TRAPXC      14

#define M_OUTPUT(Z) textout_ex(scx, font, Z, bxc, byc, color, bg);

extern struct global_data gd;
extern struct wallset **ws_tbl;

int (*dsbgetpixel)(BITMAP *, int, int);
void (*dsbputpixel)(BITMAP *, int, int, int);

extern FILE *errorlog;

struct animap *sysaniload(const char *bmpname, const char *bmplongname) {
    BITMAP *base_b;
    struct animap *amap;
    
    onstack("sysaniload");
    
    base_b = pcxload(bmpname, bmplongname);
    amap = prepare_animap_from_bmp(base_b, gd.loaded_tga);
    gd.loaded_tga = 0;    
    
    RETURN(amap);
} 

void init_colordepth(int cd) {
    if (cd == 15) {
        dsbgetpixel = _getpixel15;
        dsbputpixel = _putpixel15;
    } else if (cd == 16) {
        dsbgetpixel = _getpixel16;
        dsbputpixel = _putpixel16;  
    } else if (cd == 24) {
        dsbgetpixel = _getpixel24;
        dsbputpixel = _putpixel24;
    } else if (cd == 32) {
        dsbgetpixel = _getpixel32;
        dsbputpixel = _putpixel32;
    } else {
        char blah[24];
        snprintf(blah, sizeof(blah), "Color depth %d unsupported");
        poop_out(blah);
    }
}

int mline_breaktext(BITMAP *scx, FONT *font, int bxc, int byc,
    int color, int bg, char *str, int cpl, int maxlines, int ypl, int flags)
{
    char *istr;
    char *bptr;
    char *lptr;
    char *last_safe;
    int linesdone = 0;
    
    onstack("mline_breaktext");
    
    istr = dsbstrdup(str);
    bptr = istr;
    
    while (linesdone < maxlines) {
        int charsdone = 0;
        
        last_safe = lptr = bptr;
        
        if (*lptr == '/') {
            *last_safe = '\0';
            bptr = (last_safe + 1);
            goto BREAK_FORCED;
        }
        
        while (charsdone < cpl) {
            ++lptr;
            ++charsdone;
            
            if (*lptr == '\0')
                goto BREAKTEXT_DONE;
            
            if (*lptr == ' ')
                last_safe = lptr;
                
            if (*lptr == '/') {
                last_safe = lptr;
                *last_safe = '\0';
                M_OUTPUT(bptr);
                bptr = (last_safe + 1);
                goto BREAK_FORCED;
            }
            
        }
        
        //didn't find anything
        if (last_safe == bptr) {
            char fixer;
            
            last_safe += (cpl-1);
            fixer = *last_safe;
            *last_safe = '\0';
            M_OUTPUT(bptr);
            bptr = last_safe;
            *last_safe = fixer;
        } else {
            *last_safe = '\0';
            M_OUTPUT(bptr);
            bptr = (last_safe+1);
        }
        
        BREAK_FORCED:
        byc += ypl;
        ++linesdone;
    }
    goto LOOP_DONE;
    
    BREAKTEXT_DONE:
    M_OUTPUT(bptr);
    ++linesdone;
        
    LOOP_DONE:
    dsbfree(istr);
    RETURN(linesdone);
}

void st_colorize(struct animap *a) {
    int x, y;
    BITMAP *b;
    int ppink;
    int black;
    
    if (!a || a->numframes > 1)
        return;        
    if (a->flags & AM_HASALPHA)
        return;
    
    onstack("st_colorize");
    
    ppink = makecol(255, 0, 255);
    black = makecol(0, 0, 0);
    
    b = a->b;
    for (y=0;y<b->h;++y) {
        for (x=0;x<b->w;++x) {
            unsigned int px = dsbgetpixel(b, x, y);
            if (px != ppink && px != black) {
                dsbputpixel(b, x, y, makecol(0, 222, 222));
            }
        }
    }
    
    VOIDRETURN();    
}

void get_sysgfx(void) {
    BITMAP *testbmp;
        
    onstack("get_sysgfx");
    
    gd.simg.control_bkg = sysaniload("CONTROL_BACKGROUND", NULL);
    gd.simg.control_long = sysaniload("CONTROL_LONG", NULL);
    gd.simg.control_short = sysaniload("CONTROL_SHORT", NULL);
    
    gd.simg.mouse = sysaniload("PTRARROW", NULL);
    gd.simg.mousehand = sysaniload("PTRHAND", NULL);
    if (gd.ini_options & INIOPT_STMOUSE) {
        st_colorize(gd.simg.mouse);
        st_colorize(gd.simg.mousehand);
    }
        
    /*
    gd.simg.bar_dmg = sysaniload("BAR_DAMAGE", NULL);
    gd.simg.full_dmg = sysaniload("FULL_DAMAGE", NULL);
    */
    gd.max_dtypes = 3;
    
    // special
    gd.simg.no_use = pcxload("NO_USE", NULL);
    
    // 8 bit
    gd.simg.door_window = pcxload8("DOOR_WINDOW", NULL, gd.simg.window_palette);
    
    // guy icons
    gd.simg.guy_icons = sysaniload("HERO_ICONS", NULL);
    if (gd.simg.guy_icons->flags & AM_HASALPHA) {
        gd.simg.guy_icons_overlay = sysaniload("HERO_ICONS_OVERLAY", NULL);
    } else {
        gd.simg.guy_icons_8 = pcxload8("HERO_ICONS", NULL, gd.simg.guy_palette);
        destroy_animap(gd.simg.guy_icons);
        gd.simg.guy_icons = NULL;
    }
    
    testbmp = pcxload("OPTION_RES", NULL);
    destroy_bitmap(testbmp);
    testbmp = pcxload("OPTION_REI", NULL);
    destroy_bitmap(testbmp);
    testbmp = pcxload("OPTION_RES_REI", NULL);
    destroy_bitmap(testbmp);
    testbmp = pcxload("REI_NAMEENTRY", NULL);
    destroy_bitmap(testbmp);
  
    VOIDRETURN();
}

void masked_copy(BITMAP *src, BITMAP *dest, int sx, int sy, 
    int dx, int dy, int w, int h, BITMAP *masker, 
    int UNUSED_X_x, int UNUSED_X_y, int mcol, int mcol2, int rev, int mrev)
{
    int xi, yi;

    if (dx < 0) {
        sx = (-1*dx) % src->w;
        dx = 0;
        w -= sx;
    }
    
    if (dy < 0) {
        sy = (-1*dy) % src->h;
        dy = 0;
        h -= sy;
    }
    
    if (dx+w > dest->w) w = (dest->w - dx);
    if (dy+h > dest->h) h = (dest->h - dy);

    for (yi=0;yi<h;yi++) {
        for (xi=0;xi<w;xi++) {
            int tx;
            int c, m;
            
            if (rev) tx = ((src->w - 1) - (sx+xi));
            else tx = (sx+xi);
            c = _getpixel32(src, tx, sy+yi);
            
            if (mrev) tx = ((masker->w-1) - (dx+xi));
            else tx = dx+xi;
            m = _getpixel32(masker, tx, dy+yi);
        
            if (m == mcol)
                _putpixel32(dest, dx+xi, dy+yi, mcol2);
            else
                _putpixel32(dest, dx+xi, dy+yi, c);
            
        }
    }      
}

//viewport is 448x272
// ceiling goes 3-3-2 (or 3-3-3-3-2-2)
int trapblit(int dmode, BITMAP *scx, int bx, int by, struct dtile *ct,
    int xc, int yc, int oo, int flags,
    struct animap *normal_floor, struct animap *normal_roof)
{
    static BITMAP *flipsrc_f = NULL;
    static BITMAP *flipsrc_r = NULL;
    static BITMAP *flip_img_f = NULL;
    static BITMAP *flip_img_r = NULL;
    int mode = 0;
    int ix, iy;
    int xpbase, ypbase;
    int x_len, y_len;
    int x_in;
    struct wallset *ws;
    struct animap *fla;
    BITMAP *flb;
    int oo_f, oo_r;
    int y_floor_offset = 132;
    int y_floor_grab_offset = 0;
    
    onstack("trapblit");
        
    oo_f = oo;
    oo_r = oo;
    
    ws = ws_tbl[ct->altws[DIR_CENTER] - 1];
    fla = ws->wall[WALLBMP_FLOOR];
    if (oo_f) {
        if (ws->alt_floor) {
            fla = ws->alt_floor;
            oo_f = 0;
        } else if (!(ws->flipflags & OV_FLOOR))
            oo_f = 0;
    }
    
    lod_use(fla);
    if (fla->numframes > 1) {
        int fcr = (gd.framecounter / fla->framedelay) % fla->numframes;
        flb = fla->c_b[fcr];
    } else
        flb = fla->b;
        
    if (!flb || flb->h < 140 || flb->w < 272)
        goto TRY_CEILING;
        
    if (fla == normal_floor)
        goto TRY_CEILING;
    
    if (dmode == 1)
        goto TRY_CEILING;

    ypbase = 0; 
    //y_floor_offset = 272 - flb->h;
    y_floor_grab_offset = (flb->h - 140);
    
    // setup trapezoid by distance
    if (yc == 3) {
        y_len = 33;
        x_in = 33;
        ypbase += 107;
    } else if (yc == 2) {
        y_len = 58;
        ypbase += 49;
        x_in = 91;
    } else if (yc == 1) {
        y_len = 30;
        ypbase += 19;
        x_in = 121;
    } else if (yc == 0) {
        y_len = 19;
        x_in = 140;
    } else {
        RETURN(0);
    }
    
    // left or right sides
    if (xc == 0 || xc == 2) {
        if (yc == 0) {
            x_len = x_in - 6;
        } else {
            x_len = TRAPXS + x_in;
        }
    } else if (xc == 1)
        x_len = 448 - (2 * (TRAPXS + x_in));
    else
        x_len = TRAPLXS;
    
    if (yc == 0 && xc != 1) {
        if (xc == 0)
            xpbase = TRAPLXS;
        else if (xc == 2)
            xpbase = 448 - (x_len + TRAPLXS);
        else if (xc == -1)
            xpbase = 0;
        else if (xc == 3)
            xpbase = 448 - TRAPLXS;
    } else {
        if (xc == 2) {
            xpbase = 448 - (TRAPXS + x_in);
        } else
            xpbase = xc * (TRAPXS + x_in);
    }
    
    if (yc == 0) {
        BITMAP *fsrc = flb;
        int h = flb->h - 140;
        
        if (h > 0) {
            if (oo_f) {
                if (flipsrc_f != flb) {
                    if (flip_img_f)
                        destroy_bitmap(flip_img_f);
                    flip_img_f = create_bitmap(flb->w, flb->h);
                    clear_to_color(flip_img_f, 0);
                    draw_sprite_h_flip(flip_img_f, flb, 0, 0);
                    flipsrc_f = flb;    
                }
                fsrc = flip_img_f;
            } 
        
            blit(fsrc, scx, xpbase, 0, xpbase + bx, ypbase + by + y_floor_offset - h, x_len, h);
        }
    }

    for (iy=0;iy < y_len;++iy) {
        for (ix = 0;ix < x_len;++ix) {
            
            // flipping
            int bgx = xpbase + ix;
            if (oo_f) bgx = (flb->w - 1) - bgx;
            
            // no bounds checking, but we should be fine...
            int i_px = dsbgetpixel(flb, bgx, ypbase + iy + y_floor_grab_offset);
            dsbputpixel(scx, xpbase + ix + bx, ypbase + iy + by + y_floor_offset, i_px);
        }
        
        if (yc == 0 && (xc != 1)) {
            if (xc == 0) {
                xpbase -= 2;
                x_len++;
            } else if (xc == 2) {
                xpbase++;
                x_len++;
            } else if (xc == -1) {
                x_len -= 2;
            } else if (xc == 3) {
                xpbase += 2;
                x_len -= 2;
            }
        } else {    
            if (xc == 0)
                --x_len;
            else if (xc == 1) {
                --xpbase;
                x_len += 2;
            } else if (xc == 2) {
                ++xpbase;
                --x_len;
            }
        }
    }
    
    // Draw ceiling
    TRY_CEILING:            
    if (dmode == 0) {
        RETURN(0);
    }

    fla = ws->wall[WALLBMP_ROOF];
    if (oo_r) {
        if (ws->alt_roof) {
            fla = ws->alt_roof;
            oo_r = 0;
        } else if (!(ws->flipflags & OV_ROOF))
            oo_r = 0;
    }
    
    lod_use(fla);
    if (fla->numframes > 1) {
        int fcr = (gd.framecounter / fla->framedelay) % fla->numframes;
        flb = fla->c_b[fcr];
    } else
        flb = fla->b;

    if (!flb || flb->h < 58 || flb->w < 272)
        RETURN(0);
        
    if (fla == normal_roof)
        RETURN(0);

    ypbase = 0;
    
    if (yc == 3) {
        y_len = 18;
        x_in = 0;
    } else if (yc == 2) {
        y_len = 22;
        ypbase += 18;
        x_in = 47;
    } else if (yc == 1) {
        y_len = 12;
        ypbase += 40;
        x_in = 106;
    } else if (yc == 0) {
        y_len = 6;
        ypbase += 52;
        x_in = 138;
    } else {
        RETURN(0);
    }

    if (xc == 2)
        xpbase = 448 - (TRAPXC + x_in);
    else if (xc == -1)
        xpbase = 0;
    else if (xc == 3)
        xpbase = 448;
    else
        xpbase = xc * (TRAPXC + x_in);

    if (xc == 0 || xc == 2)
        x_len = TRAPXC + x_in;
    else if (xc == 1)
        x_len = 448 - (2 * (TRAPXC + x_in));
    else if (xc == 3)
        x_len = 0;
    else
        x_len = 0;
        
    for (iy=0;iy < y_len;++iy) {
        int var = 3;
        
        for (ix = 0;ix < x_len;++ix) {
            int bgx = xpbase + ix;
            if (oo_r) bgx = (flb->w - 1) - bgx;
            
            int i_px = dsbgetpixel(flb, bgx, ypbase + iy);
            dsbputpixel(scx, xpbase + ix + bx, ypbase + iy + by, i_px);
        }

        if (mode == 2) {
            mode = -1;
            var = 2;
        }
        
        if (yc == 0 && (xc != 1)) {
            if (xc == 0) {
                xpbase += 6; 
                x_len -= 6;
            } else if (xc == -1) {
                x_len += 6;
            } else if (xc == 3) {
                x_len += 6;
                xpbase -= 6;
            } else if (xc == 2) {
                x_len -= 6;
            } 
        }
        
        if (xc == 0)
            x_len += var;
        else if (xc == 1) {
            xpbase += var;
            x_len -= 2*var;
        } else if (xc == 2) {
            xpbase -= var;
            x_len += var;
        }
        
        ++mode;
    }
    
    if (yc == 0) {
        BITMAP *fsrc = flb;
        int h = flb->h - 58;
        
        if (h > 0) {      
            if (oo_r) {
                if (flipsrc_r != flb) {
                    if (flip_img_r)
                        destroy_bitmap(flip_img_r);
                    flip_img_r = create_bitmap(flb->w, flb->h);
                    clear_to_color(flip_img_r, 0);
                    draw_sprite_h_flip(flip_img_r, flb, 0, 0);
                    flipsrc_r = flb;    
                }
                fsrc = flip_img_r;
            }         
        
            blit(fsrc, scx, xpbase, ypbase + y_len, xpbase + bx, ypbase + by + y_len, x_len, h); 
        }
    }
    
    RETURN(0);
}

/*
void remask_blit(BITMAP *src, BITMAP *dest, int sx, int sy, 
    int dx, int dy, int w, int h, int delcol, int convcol, int conv_to)
{
    int xi, yi;
    
    for (yi=0;yi<h;yi++) {
        for (xi=0;xi<w;xi++) {
            int c;
            
            c = getpixel(src, sx+xi, sy+yi);
        
            if (c != delcol) {
                if (c == convcol)
                    putpixel(dest, dx+xi, dy+yi, conv_to);
                else
                    putpixel(dest, dx+xi, dy+yi, c);
            }
            
        }
    }      
}
*/

BITMAP *animap_subframe(struct animap *amap, int d) {
    int cframe;
    
    if (amap->numframes <= 1)
        return amap->b;
    
    cframe = ((gd.framecounter / amap->framedelay) + d) % amap->numframes;
    return (amap->c_b[cframe]);
}

// use the frame within the instance
// (fz = freeze, which used to be handled here)
BITMAP *fz_animap_subframe(struct animap *amap, struct inst *p_inst) {
    if (amap) {
        int cframe;
        
        if (amap->numframes <= 1) {
            return amap->b;
        }
        
        cframe = ((p_inst->frame / amap->framedelay) % amap->numframes);
        return (amap->c_b[cframe]);  
    } else
        return NULL;
}

BITMAP *monster_animap_subframe(struct animap *amap, int i_framecount) {
    if (amap) {
        int cframe = n_animap_subframe(amap, 0, i_framecount);
        return (amap->c_b[cframe]);
    } else
        return NULL;
}
    
int n_animap_subframe(struct animap *amap, int delta, int fc) {
    int cframe;
    if (!amap) return 0;    
    cframe = ((fc / amap->framedelay) + delta) % amap->numframes;
    return cframe;
}

struct animap *create_vanimap(int w, int h) {
    struct animap *my_a_map;
    BITMAP *bitmap;

    onstack("create_vanimap");

    bitmap = create_bitmap(w, h);

    my_a_map = dsbmalloc(sizeof(struct animap));
    memset(my_a_map, 0, sizeof(struct animap));
    
    my_a_map->b = bitmap;
    my_a_map->flags = AM_VIRTUAL;
    
    my_a_map->w = w;
    my_a_map->numframes = 1;
    
    RETURN(my_a_map);
}

void conddestroy_dynscaler(struct animap_dynscale *ad) {
    int i;
    
    onstack("conddestroy_dynscaler");
    
    if (ad == NULL) {
        VOIDRETURN();
    }
    
    for (i=0;i<6;++i) {
        if (ad->v[i]) {
            destroy_animap(ad->v[i]);
        }
    }
    dsbfree(ad);    

    VOIDRETURN();
}

void conddestroy_lod(struct lod_data *lod) {
    onstack("conddestroy_lod");
    
    if (lod == NULL) {
        VOIDRETURN();
    }
    dsbfree(lod->shortname);
    dsbfree(lod->filename);
    dsbfree(lod);

    VOIDRETURN();
}

void destroy_animap(struct animap *lbmp) {
    
    onstack("destroy_animap");

    if (lbmp->cloneof) {
        if (lbmp->cloneof->clone_references >= 1 && 
            lbmp->cloneof->ext_references >= 1)
        {
            lbmp->cloneof->ext_references -= 1;
            lbmp->cloneof->clone_references -= 1;
        } else {
            program_puke("Destroyed cloned bitmap with no references");
        }
    } else {
        if (lbmp->numframes > 1) {
            int n;
            for (n=0;n<lbmp->numframes;++n) {
                destroy_bitmap(lbmp->c_b[n]);
            }
            dsbfree(lbmp->c_b);
        } else {
            if (lbmp->b) {
                destroy_bitmap(lbmp->b);
            }
        }

        if (lbmp->flags & AM_256COLOR)
            condfree(lbmp->pal);
    }
    
    conddestroy_dynscaler(lbmp->ad);
    conddestroy_lod(lbmp->lod);

    dsbfree(lbmp);
    
    VOIDRETURN();
}

void conddestroy_animap(struct animap *ani) {
    if (ani != NULL)
        destroy_animap(ani);
}

BITMAP *scalebmp(BITMAP *b, int m, int d, int ym, int yd,
    int cd, int x_comp, int y_comp) 
{
    BITMAP *rb;
    int new_w, new_h;
    int w, h;
    
    onstack("scalebmp");
    
    w = b->w;
    h = b->h;
    
    new_w = ((w - x_comp) * m)/d;
    new_h = ((h - y_comp) * ym)/yd;
    
    if (ym > 60 && (new_h % 2))
        ++new_h;

    if (cd == 8) {
        rb = create_bitmap_ex(8, new_w, new_h);
    } else
        rb = create_bitmap(new_w, new_h);

    DSB_aa_scale_blit(0, b, rb, 0, 0, b->w, b->h, 0, 0, new_w, new_h);
    
    RETURN(rb);
}

struct animap *scaleimage(struct animap *base_map, int sm) {
    struct animap *r_map;
    int m, d;
    int xcmp, ycmp;
    
    onstack("scaleimage");
        
    r_map = dsbmalloc(sizeof(struct animap));
    memcpy(r_map, base_map, sizeof(struct animap));
    r_map->ad = NULL;
    r_map->lod = NULL;
    
    if (sm == 1) {
        m = 2;
        d = 3;
    } else if (sm == 3) {
        // vsm of sm 0
        m = 9;
        d = 10;
    } else if (sm == 4) {
        // vsm of sm 1
        m = 3;
        d = 5;
    } else if (sm == 5) {
        // vsm of sm 2
        m = 101;
        d = 250;
    } else if (sm == 2) {
        m = 9;
        d = 20;
    }
    
    if (sm == 2 || sm == 5) {
        xcmp = base_map->far_x_compress;
        ycmp = base_map->far_y_compress;
    } else {        
        xcmp = base_map->mid_x_compress;
        ycmp = base_map->mid_y_compress;
    }
    
    if (base_map->numframes > 1) {
        int n;
        r_map->c_b = dsbmalloc(base_map->numframes * sizeof(BITMAP*));
        for (n=0;n<base_map->numframes;++n) {
            BITMAP *b_base = base_map->c_b[n];
            int cd = bitmap_color_depth(b_base);
            BITMAP *b_res = scalebmp(b_base, m, d, m, d, cd, xcmp, ycmp);
            r_map->c_b[n] = b_res;
        }
        r_map->w = r_map->c_b[0]->w;
    } else {
        int cd = bitmap_color_depth(base_map->b);
        r_map->b = scalebmp(base_map->b, m, d, m, d, cd, xcmp, ycmp);
        r_map->w = r_map->b->w;
    }
    
    r_map->flags |= AM_DYNGEN;
    
    RETURN(r_map);      
}

struct animap *scaledoor(struct animap *base_map, int sm) { 
    struct animap *r_map;
    int xm, xd, ym, yd;
    int cd;
    int xcmp, ycmp;
    
    onstack("scaledoor");
        
    r_map = dsbmalloc(sizeof(struct animap));
    memcpy(r_map, base_map, sizeof(struct animap));
    r_map->ad = NULL;
    r_map->lod = NULL;
    
    if (sm == 1) {
        xm = 2;
        xd = 3;
        ym = 83;
        yd = 120;
    } else {
        xm = ym = 1;
        xd = yd = 2;
    }
    
    if (sm == 2 || sm == 5) {
        xcmp = base_map->far_x_compress;
        ycmp = base_map->far_y_compress;
    } else {        
        xcmp = base_map->mid_x_compress;
        ycmp = base_map->mid_y_compress;
    }
    
    if (r_map->numframes > 1) {
        int n;
        r_map->c_b = dsbmalloc(base_map->numframes * sizeof(BITMAP*));
        for (n=0;n<r_map->numframes;++n) {
            BITMAP *b_base = base_map->c_b[n];
            int cd = bitmap_color_depth(b_base);
            BITMAP *b_res = scalebmp(b_base, xm, xd, ym, yd, cd, xcmp, ycmp);
            r_map->c_b[n] = b_res;
        }
        r_map->w = r_map->c_b[0]->w;
    } else {
        int cd = bitmap_color_depth(base_map->b);
        r_map->b = scalebmp(base_map->b, xm, xd, ym, yd, cd, xcmp, ycmp);
        r_map->w = r_map->b->w;
    }
        
    r_map->flags |= AM_DYNGEN;

    RETURN(r_map);      
}

struct animap *setup_animation(int table_animate, struct animap *lbmp,
    struct animap *bmp_table[256], int i_num_frames, int framedelay) {
    int num_frames;
    BITMAP *pb_bmp;
    int n;
    int w;
    int h;
    
    onstack("setup_animation");
    
    num_frames = i_num_frames;

    if (table_animate) {
        struct animap *ani_bmp;
        int iframe;
        
        if (table_animate <= 1) {
            RETURN(NULL);
        }
        
        ani_bmp = dsbmalloc(sizeof(struct animap));
        memcpy(ani_bmp, bmp_table[0], sizeof(struct animap));
        ani_bmp->numframes = table_animate;
        ani_bmp->framedelay = framedelay;
        ani_bmp->c_b = dsbmalloc(ani_bmp->numframes * sizeof(BITMAP*));
        
        for (iframe=0;iframe<table_animate;++iframe) {
            v_onstack("setup_animation.table_copy_iframe");
            if (bmp_table[iframe]->numframes <= 1) {
                int w = bmp_table[iframe]->b->w;
                int h = bmp_table[iframe]->b->h;
                ani_bmp->c_b[iframe] = create_bitmap(w, h);
                blit(bmp_table[iframe]->b, ani_bmp->c_b[iframe], 0, 0, 0, 0, w, h);
                ani_bmp->w = w;
            } else {
                ani_bmp->c_b[iframe] = create_bitmap(1, 1);
            }  
            v_upstack(); 
        }
        
        RETURN(ani_bmp); 
        
    } else {
        if (lbmp->numframes > 1 || num_frames <= 0) {
            RETURN(NULL);
        }
        pb_bmp = lbmp->b;
        if (pb_bmp == NULL) {
            RETURN(NULL);
        }
   
        lbmp->numframes = num_frames;
        lbmp->framedelay = framedelay;
        lbmp->c_b = dsbmalloc(lbmp->numframes * sizeof(BITMAP*));
        w = pb_bmp->w / num_frames;
        h = pb_bmp->h;
        
        for (n=0;n<lbmp->numframes;++n) {
            lbmp->c_b[n] = create_bitmap(w, h);
            blit(pb_bmp, lbmp->c_b[n], n*w, 0, 0, 0, w, h);
        }
        
        destroy_bitmap(pb_bmp);
        
        lbmp->w = lbmp->c_b[0]->w;
        
        RETURN(NULL);
    }
}

int bmp_alpha_scan(BITMAP *ibmp) {
    int everyzero = 1;
    int x, y;
    
    onstack("bmp_alpha_scan");
    
    for (y=0;y<ibmp->h;++y) {
        for (x=0;x<ibmp->w;++x) {
            unsigned int px = _getpixel32(ibmp, x, y);
            unsigned int a = geta(px);
            if (a != 0) {
                if (a != 255) {
                    RETURN(1);
                }
                
                everyzero = 0;
            }
        }
    }
    RETURN(!everyzero);
}

struct animap *prepare_animap_from_bmp(BITMAP *in_bmp, int targa) {
    struct animap *my_a_map;
    
    onstack("prepare_animap_from_bmp");
    
    my_a_map = dsbmalloc(sizeof(struct animap));
    memset(my_a_map, 0, sizeof(struct animap));

    if (targa) {
        if (bmp_alpha_scan(in_bmp))
            my_a_map->flags = AM_HASALPHA;
    }

    my_a_map->b = in_bmp;   
    my_a_map->w = in_bmp->w; 
    
    RETURN(my_a_map);
}
