#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "global_data.h"
#include "gproto.h"
#include "console.h"
#include "uproto.h"

extern struct global_data gd;
extern struct graphics_control gfxctl;
extern int (*dsbgetpixel)(BITMAP *, int, int);
extern void (*dsbputpixel)(BITMAP *, int, int, int);

struct c_line cons[16];
extern FONT *ROOT_SYSTEM_FONT;

void draw_lua_errors(BITMAP *scx) {
    textprintf_ex(scx, ROOT_SYSTEM_FONT, 16, 4, makecol(255, 196, 0),
        -1, "%2d LUA ERROR%s SEE LOG.TXT", gd.lua_error_found,
        (gd.lua_error_found == 1)? ":" : "S:");
}

void init_console(void) {
    memset(cons, 0, sizeof(struct c_line) * 4);
}

void destroy_console(void) {
    int i;
    
    for(i=0;i<4;i++) {
        if (cons[i].txt != NULL) {
            dsbfree(cons[i].txt);
        }
    }
    init_console();
}
    
void render_console(BITMAP *scx) {
    const int powerpink = makecol(255, 0, 255);
    int i;
    
    for(i=0;i<16;i++) {
        if (cons[i].txt != NULL) {
            if (cons[i].linebmp) {
                int x, y;
                int h;
                BITMAP *tdraw_bkg_bmp;
                BITMAP *tdraw_fg_bmp;
                BITMAP *curframe_bmp;
                
                lod_use(cons[i].linebmp);
                curframe_bmp = animap_subframe(cons[i].linebmp, 0);
                h = text_height(font);
                tdraw_bkg_bmp = create_bitmap(648, h);
                tdraw_fg_bmp = create_bitmap(648, h);
                for(x=0;x<54;++x) {
                    blit(curframe_bmp, tdraw_bkg_bmp, 10, 0, 12*x, 0, 12, h);
                }
                clear_to_color(tdraw_fg_bmp, powerpink); 
                textout_ex(tdraw_fg_bmp, font, cons[i].txt, 1, 0, 0, -1);
                
                for(y=0;y<tdraw_bkg_bmp->h;++y) {
                    for(x=0;x<tdraw_bkg_bmp->w;++x) {
                        int txtc = dsbgetpixel(tdraw_fg_bmp, x, y);
                        if (txtc != powerpink) {
                            int dc = dsbgetpixel(tdraw_bkg_bmp, x, y);
                            dsbputpixel(tdraw_fg_bmp, x, y, dc);
                        }
                    }
                } 
            
                masked_blit(tdraw_fg_bmp, scx, 0, 0, gfxctl.con_x, gfxctl.con_y+gd.t_ypl*i, 640, h);
                
                destroy_bitmap(tdraw_bkg_bmp);
                destroy_bitmap(tdraw_fg_bmp);
            } else {
                textout_ex(scx, font, cons[i].txt, gfxctl.con_x, gfxctl.con_y+gd.t_ypl*i,
                    cons[i].linecol, -1);
            }
            
        }
    }
}

void update_console(void) {
    int i;
    
    for (i=0;i<16;i++) {
        if (cons[i].txt) {
            --cons[i].ttl;
            
            if (!cons[i].ttl) {
                dsbfree(cons[i].txt);
                cons[i].txt = NULL;
            }
        }
    }    
}

void console_system_message(const char *textmsg, int in_color) {
    pushtxt_cons_bmp(textmsg, in_color, NULL, 50);
}

void pushtxt_cons_bmp(const char *textmsg, int in_color, struct animap *bg_col, int i_ttl) {
    int i;
    int last = gfxctl.console_lines - 1;
    
    if (cons[0].txt)
        dsbfree(cons[0].txt);
    
    for (i=1;i<16;i++)
        memcpy(&(cons[i-1]), &(cons[i]), sizeof(struct c_line));

    cons[last].txt = dsbstrdup(textmsg);
    cons[last].linecol = in_color;
    cons[last].linebmp = bg_col;
    cons[last].ttl = i_ttl;   
}
