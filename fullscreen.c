#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "objects.h"
#include "global_data.h"
#include "champions.h"
#include "lua_objects.h"
#include "gproto.h"
#include "uproto.h"
#include "fullscreen.h"

#define FADE_INCREMENT  8

BITMAP *FORCE_RENDER_TARGET = NULL;

extern struct global_data gd;
extern BITMAP *scr_pages[3];
extern BITMAP *soft_buffer;
extern int viewstate;
extern lua_State *LUA;
extern int CLOCKFREQ;
extern unsigned int CurrentKey;

extern int updateok;
extern int instant_click;
extern HANDLE semaphore;

int fullscreen_mode(struct fullscreen *fsd) {
    BITMAP *drawbuf;
    int i_exit = 0;
    int local_updateok = 0;
    int mouse_lock = 1;
    unsigned int tick = 0;
    int fade_running = 0;
    int fadein = 255;
    int fadeout = 0;
    
    onstack("fullscreen_mode");
    
    if (gd.ini_options & INIOPT_SEMA) {
        updateok = 0;
        ReleaseSemaphore(semaphore, 1, NULL);
        local_updateok = 1;
    }

    if (fsd->fs_scrbuf)
        drawbuf = fsd->fs_scrbuf;
    else
        drawbuf = soft_buffer;
        
    luastacksize(6);
    dsb_clear_keybuf();
    
    instant_click = 0;
    while (!i_exit || fade_running) {
        
        if (!(gd.ini_options & INIOPT_SEMA)) {
            local_updateok = systemtimer();
        } else {
            local_updateok = updateok;
        }
        
        if (local_updateok) {      
            if (viewstate < VIEWSTATE_FROZEN) {
                gd.framecounter++;
                if (gd.framecounter >= FRAMECOUNTER_MAX)
                    gd.framecounter = 0;
                    
                tick++;
            }
            
            if (tick >= CLOCKFREQ) {
                if (fsd->lr_update != LUA_NOREF) {
                    int do_exit = 0;
                    lua_rawgeti(LUA, LUA_REGISTRYINDEX, fsd->lr_update);
                    do_exit = lc_call_topstack(0, "fullscreen_update");
                    if (!i_exit && do_exit) {
                        i_exit = do_exit;
                        if (fsd->fs_fademode)
                            fade_running = 1;
                        else 
                            break;
                    }
                    if (gd.zero_int_result) {
                        gd.zero_int_result = 0;
                        break;
                    }
                }
                tick = 0;
                
                check_sound_channels();
            }
                        
            if (fsd->fs_bflags & FS_B_DRAW_GAME) {
                FORCE_RENDER_TARGET = drawbuf;
                render_dungeon_view(1, 0);
                openclip(1);
                draw_interface(1, VIEWSTATE_DUNGEON, 0);
                draw_champions(1, FORCE_RENDER_TARGET, -1);
                FORCE_RENDER_TARGET = NULL;
            } else {
                clear_to_color(drawbuf, 0);   
            }

            if (fsd->lr_bkg_draw == LUA_NOREF) {
                int drawval = 0;
                
                if (fsd->bkg)
                    draw_gfxicon(drawbuf, fsd->bkg, 0, 0, 0, NULL);
                                
            } else {
                int do_exit = 0;
                struct animap *bkg_animap;
                lua_rawgeti(LUA, LUA_REGISTRYINDEX, fsd->lr_bkg_draw);

                bkg_animap = dsbcalloc(1, sizeof(struct animap));
                bkg_animap->b = drawbuf;
                bkg_animap->numframes = 1;
                bkg_animap->flags = AM_VIRTUAL;
                
                setup_lua_bitmap(LUA, bkg_animap);
                
                lua_pushinteger(LUA, mouse_x);
                lua_pushinteger(LUA, mouse_y);
                do_exit = lc_call_topstack(3, "fullscreen_draw");
                if (!i_exit) i_exit = do_exit;
                if (gd.zero_int_result) {
                    gd.zero_int_result = 0;
                    break;
                }
                
                // don't destroy_animap here, or we'll trash our screenbuffer!
                dsbfree(bkg_animap);
            }
            
            if (fsd->fs_mousemode) {
                struct animap *fsd_mptr = NULL;
                if (fsd->fs_mousemode != FS_MOUSEPTR)
                    fsd_mptr = fsd->mouse;
                    
                draw_mouseptr(drawbuf, fsd_mptr, 1);
            }
            
            if (fsd->fs_fademode) {
                int drawval = 0;
                if (fadein) {
                    drawval = fadein;
                    fadein -= FADE_INCREMENT;
                    if (fadein < 0)
                        fadein = 0;
                } else if (fade_running) {
                    drawval = fadeout;
                    fadeout += FADE_INCREMENT;
                    if (fadeout > 255) {
                        fadeout = 255;
                        fade_running = 0;
                    }
                }
                if (drawval > 0) {
                    set_trans_blender(0, 0, 0, drawval);
                    drawing_mode(DRAW_MODE_TRANS, drawbuf, 0, 0);
                    rectfill(drawbuf, 0, 0, 639, 479, 0);
                    drawing_mode(DRAW_MODE_SOLID, NULL, 0, 0);
                }
            }

            local_updateok = 0;                
            if (gd.lua_error_found)
                draw_lua_errors(drawbuf);
                
            if (gd.stored_rcv == 255) {
                lstacktop();
                RETURN(0);
            }
                
            scfix(1);
                        
        } else
            Sleep(5);
            
        if (gd.ini_options & INIOPT_SEMA) {
            WaitForSingleObject(semaphore, 50);
        }
        
        instant_click = 0;
        dsb_shift_immed_key_queue();
        
        if (CurrentKey) {
            int c_mx = 0;
            int c_my = 0;
            int iclick = 0;
            
            if (CurrentKey & 0x80000000) {
                c_mx = (CurrentKey >> 4) & 8191;
                c_my = (CurrentKey >> 18) & 8191;
                iclick = CurrentKey & 7;
            } 
                
            if (!i_exit && iclick) {
                if (!mouse_lock) {
                    mouse_lock = 1;
                    if (fsd->lr_on_click != LUA_NOREF) {
                        lua_rawgeti(LUA, LUA_REGISTRYINDEX, fsd->lr_on_click);
                        lua_pushinteger(LUA, c_mx);
                        lua_pushinteger(LUA, c_my);
                        lua_pushinteger(LUA, iclick);
                        i_exit = lc_call_topstack(3, "fullscreen_click");
                        if (i_exit) {
                            if (fsd->fs_fademode)
                                fade_running = 1;
                            else
                                break;
                        }
                        if (gd.zero_int_result) {
                            gd.zero_int_result = 0;
                            break;
                        }
                    }
                }
            } else
                mouse_lock = 0;
        } else
            mouse_lock = 0;
    }
    
    dsb_clear_keybuf();
    lstacktop();
    RETURN(i_exit);
}
