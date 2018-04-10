#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "global_data.h"
#include "defs.h"
#include "uproto.h"
#include "compile.h"
#include "loadpng.h"
#include "sfmt.h"

#define MERSENNE_SIZE 2048

sfmt_t mersenne_state;
unsigned int mersenne_array[MERSENNE_SIZE];

extern FONT *ROOT_SYSTEM_FONT;
extern FILE *errorlog;
extern struct global_data gd;

const char *FONT_NAME = "AAASYSFONT";

void poop_out(char *errmsg) {
    onstack("QUIT_ERROR_MESSAGE");
    stackbackc('(');
    v_onstack(errmsg);
    addstackc(')');
    
    set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
    fprintf(errorlog, "Error: %s\n", errmsg);
	MessageBox(win_get_window(), errmsg, "Error", MB_ICONSTOP);
	DSBallegshutdown();
	fclose(errorlog);
	exit(1);
}

void init_fonts(void) {
    onstack("init_fonts");
    
    gd.rdat = load_datafile_object(fixname("graphics.dsb"), FONT_NAME);
    
    if (gd.rdat == NULL)
        gd.rdat = load_datafile_object(bfixname("graphics.dat"), FONT_NAME);    
    
    if (gd.rdat == NULL) {
        poop_out("Couldn't load a font");         
    }
    
    font = gd.rdat->dat;
    ROOT_SYSTEM_FONT = gd.rdat->dat;
    
    VOIDRETURN();
}   

const char *make_noslash(const char *l_str) {
    const char *noslash_l_str;
    const char *ptr;
    
    if (!l_str)
        return NULL;
    
    ptr = l_str;
    noslash_l_str = l_str;
    
    while (*ptr != '\0') {
        if (*ptr == '/') {
            noslash_l_str = (ptr+1);
        }
        
        ptr++;
    }
    
    return(noslash_l_str);
}

BITMAP *image_loader(const char *l_str, const char *longname, RGB *use256) {
    static char p_str[384];
    const char *noslash_l_str;
    BITMAP *r_bmp;
    
    onstack("image_loader");
    stackbackc('(');
    v_onstack(l_str);
    addstackc(')');
    
    noslash_l_str = make_noslash(l_str);
    
    if (longname)
        snprintf(p_str, sizeof(p_str), "%s/%s", gd.dprefix, longname);
    
    if (!longname)
        snprintf(p_str, sizeof(p_str), "%s/%s%s", gd.dprefix, l_str, ".bmp");
    r_bmp = load_bmp(p_str, use256);
    if (r_bmp != NULL) {
        if (gd.compile)
            add_iname(l_str, noslash_l_str, "bmp", longname);
        RETURN(r_bmp);
    }
    
    if (!longname)
        snprintf(p_str, sizeof(p_str), "%s/%s%s", gd.dprefix, l_str, ".png");
    r_bmp = load_png(p_str, NULL);
    if (r_bmp != NULL) {
        gd.loaded_tga = 1;
        if (gd.compile)
            add_iname(l_str, noslash_l_str, "png", longname);
        RETURN(r_bmp);
    }

    if (!longname)
        snprintf(p_str, sizeof(p_str), "%s/%s%s", gd.dprefix, l_str, ".pcx");
    r_bmp = load_pcx(p_str, use256);
    if (r_bmp != NULL) {
        if (gd.compile)
            add_iname(l_str, noslash_l_str, "pcx", longname);
        RETURN(r_bmp);
    }
    
    if (!longname)
        snprintf(p_str, sizeof(p_str), "%s/%s%s", gd.dprefix, l_str, ".tga");
    r_bmp = load_tga(p_str, NULL);
    if (r_bmp != NULL) {
        gd.loaded_tga = 1;
        if (gd.compile)
            add_iname(l_str, noslash_l_str, "tga", longname);
        RETURN(r_bmp);
    }
    
    snprintf(p_str, sizeof(p_str), "%s/graphics.dsb#%s", gd.dprefix, noslash_l_str);
    r_bmp = load_pcx(p_str, use256);
    if (r_bmp == NULL) {
        r_bmp = load_bmp(p_str, use256);
        if (r_bmp == NULL) {
            r_bmp = load_png(p_str, NULL);
            if (r_bmp == NULL) {
                r_bmp = load_tga(p_str, NULL);
                if (r_bmp != NULL) {
                    gd.loaded_tga = 1;
                    RETURN(r_bmp);
                }
            } else
                gd.loaded_tga = 1;
        }
    }
    if (r_bmp != NULL) RETURN(r_bmp);

    
    snprintf(p_str, sizeof(p_str), "%s/graphics2.dat#%s", gd.bprefix, noslash_l_str);
    r_bmp = load_pcx(p_str, use256); 
    if (r_bmp != NULL) RETURN(r_bmp);
    
    snprintf(p_str, sizeof(p_str), "%s/graphics.dat#%s", gd.bprefix, noslash_l_str);
    r_bmp = load_pcx(p_str, use256); 
    if (r_bmp != NULL) RETURN(r_bmp);  
       
    if (longname)
        snprintf(p_str, sizeof(p_str), "Bitmap not found:\n%s (%s)", noslash_l_str, longname);
    else     
        snprintf(p_str, sizeof(p_str), "Bitmap not found:\n%s", noslash_l_str);
    poop_out(p_str);
    RETURN(NULL);   
}

// FIXED!
// f_d is no longer leaked, and it can load from independent files
FONT *fontload(const char *l_str, const char *longname) {
    BITMAP *r_bmp;
    FONT *r_font;
    static char p_str[384];
    const char *noslash_l_str;
    DATAFILE *f_d;
    
    onstack("fontload");
    
    set_color_conversion(COLORCONV_NONE);
    
    noslash_l_str = make_noslash(l_str);
    
    if (longname)
        snprintf(p_str, sizeof(p_str), "%s/%s", gd.dprefix, longname);
    
    if (!longname)
        snprintf(p_str, sizeof(p_str), "%s/%s%s", gd.dprefix, l_str, ".bmp");
    r_font = load_bitmap_font(p_str, NULL, NULL);
    if (r_font != NULL) {
        if (gd.compile)
            add_iname(l_str, noslash_l_str, "bmp", longname);
        set_color_conversion(COLORCONV_KEEP_ALPHA);
        RETURN(r_font);
    }

    if (!longname)
        snprintf(p_str, sizeof(p_str), "%s/%s%s", gd.dprefix, l_str, ".pcx");
    r_font = load_bitmap_font(p_str, NULL, NULL);
    if (r_font != NULL) {
        if (gd.compile)
            add_iname(l_str, noslash_l_str, "pcx", longname);
        set_color_conversion(COLORCONV_KEEP_ALPHA);
        RETURN(r_font);
    }
    
    snprintf(p_str, sizeof(p_str), "%s/graphics.dsb#%s", gd.dprefix, noslash_l_str);
    r_bmp = load_pcx(p_str, NULL);
    if (r_bmp == NULL)
        r_bmp = load_bmp(p_str, NULL);
    if (r_bmp != NULL) {
        r_font = grab_font_from_bitmap(r_bmp);
        destroy_bitmap(r_bmp);
        set_color_conversion(COLORCONV_KEEP_ALPHA);
        RETURN(r_font);
    }
    
    set_color_conversion(COLORCONV_KEEP_ALPHA);
    
    f_d = load_datafile_object(bfixname("graphics.dat"), noslash_l_str);  
    if (f_d != NULL) {
        gd.queued_pointer = f_d;
        RETURN((FONT*)f_d->dat);
    }
 
    if (longname)
        snprintf(p_str, sizeof(p_str), "Font not found:\n%s (%s)", noslash_l_str, longname);
    else    
        snprintf(p_str, sizeof(p_str), "Font not found:\n%s", noslash_l_str);
    poop_out(p_str);
    RETURN(NULL);   
}

// now loads more than pcx
BITMAP *pcxload(const char *l_str, const char *longname) {
    BITMAP *l_img = NULL;
    onstack("pcxload");
    l_img = image_loader(l_str, longname, NULL);
    RETURN(l_img);
}

BITMAP *pcxload8(const char *l_str, const char *longname, RGB *P_use) {
    BITMAP *l_img = NULL;
    onstack("pcxload8");
    set_color_conversion(COLORCONV_NONE);
    l_img = image_loader(l_str, longname, P_use);
    set_color_conversion(COLORCONV_KEEP_ALPHA);
    RETURN(l_img);
}

void face2delta(int face, int *dx, int *dy) {
    
    *dx = (face%2);
    *dy = ((face+1)%2);
    
    if (face == 0)
        *dy *= -1;
    
    if (face == 3)
        *dx *= -1;
}

// invalid tiles can cause strange things to happen
int tileshift(int in_tile, int shift_by, int *chg) {
    static const char *x = "There's nothing interesting in here.";
    int ret = 0;
        
    if (shift_by % 2) {
        // 1 or 3
        if (in_tile == 1) ret = 0;
        else if (in_tile == 3) ret = 2;
        else ret = in_tile+1;
    } else {
        // 0 or 2
        if (in_tile == 0) ret = 3;
        else if (in_tile == 3) ret = 0;
        else if (in_tile == 1) ret = 2;
        else if (in_tile == 2) ret = 1;
    }
    
    return ret;
}

int shiftdirto(int src, int dest) {
    if (dest == (src+3) % 4)
        return (dest + 3) % 4; 
        
    return dest;  
}

void rebuild_rng_queue() {
    onstack("rebuild_rng_queue");
    gd.rng_index = 0;
    sfmt_fill_array32(&mersenne_state, mersenne_array, MERSENNE_SIZE);
    VOIDRETURN();    
}

int DSBsrand(unsigned int seed) {
    onstack("DSBsrand");
    
    v_onstack("DSBsrand.stdlib");
    srand(seed);
    v_upstack();
    
    v_onstack("DSBsrand.sfmt");
    sfmt_init_gen_rand(&mersenne_state, seed);
    v_upstack();
    
    rebuild_rng_queue();
    RETURN(0);
}

int srand_to_session_seed() {
    onstack("srand_to_session_seed");
	RETURN(DSBsrand(gd.session_seed));
}

int DSBprand(int min, int max) {
    unsigned int randval = DSBrand();
    int range = (max - min) + 1;
    return min + (randval % range);  
}

int DSBtrivialrand() {
    return rand();   
}

unsigned int DSBrand() {
    unsigned int generated;
    if (gd.rng_index == MERSENNE_SIZE) {
        rebuild_rng_queue();
    }
    generated = mersenne_array[gd.rng_index];
    gd.rng_index++;
    return generated;
}
