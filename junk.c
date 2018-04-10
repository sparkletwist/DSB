#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include <fmod.h>
#include "objects.h"
#include "global_data.h"
#include "defs.h"
#include "uproto.h"
#include "gproto.h"

extern const char *CSSAVEFILE[];
char *Iname[4];
char *Backup_Iname = NULL;

extern BITMAP *pcxload(const char *p_str, const char *longname);
extern void (*dsbputpixel)(BITMAP *, int, int, int);
extern FMOD_SYSTEM *f_system;

extern BITMAP *scr_pages[3];
extern BITMAP *soft_buffer;
extern int updateok;
extern int instant_click;
extern struct global_data gd;

extern struct clickzone cz[NUM_ZONES];

extern FILE *errorlog;
extern FONT *ROOT_SYSTEM_FONT;

extern int T_BPS;
extern HANDLE semaphore;

extern int cracked;

#define NSTARS 60

int check_for_any_savegame(void) {
    int ns = 0;
    
    onstack("check_for_any_savegame");
    
    ns += check_for_savefile(CSSAVEFILE[0], &(Iname[0]), NULL);
    ns += check_for_savefile(CSSAVEFILE[1], &(Iname[1]), NULL);
    ns += check_for_savefile(CSSAVEFILE[2], &(Iname[2]), NULL);
    ns += check_for_savefile(CSSAVEFILE[3], &(Iname[3]), NULL);
    
    RETURN(ns);
}

FMOD_SOUND *internal_load_sound_data(char *s_dataname) {
    DATAFILE *emusic;
    FMOD_SOUND *music = NULL;
    FMOD_CHANNEL *chan;
    FMOD_CREATESOUNDEXINFO ex_i;

    onstack("internal_load_sound_data");
    
    emusic = load_datafile_object(bfixname("graphics.dat"), s_dataname);
    memset(&ex_i, 0, sizeof(FMOD_CREATESOUNDEXINFO));
    ex_i.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
    ex_i.length = emusic->size;
    FMOD_System_CreateSound(f_system, emusic->dat, FMOD_OPENMEMORY|FMOD_2D,
        &ex_i, &music);
    unload_datafile_object(emusic);

    RETURN(music);
}

int show_the_end_screen(int b_s_reload) {
    BITMAP *the_end;
    BITMAP *restart;
    int r_v = 1;
    int ct = 0;
    int uclock;
    
    onstack("show_the_end_screen");
    
    while (ct<10) {
        int r = systemtimer();
        ct += r;
        clear_to_color(soft_buffer, makecol(0, 0, 72));
        scfix(1);
        Sleep(5);
    }
    
    the_end = pcxload("THE_END", NULL);
    restart = pcxload("RESTART_BUTTON", NULL);
    
    uclock = 31;
    while (uclock > 0) {
        updateok = systemtimer();
        if (updateok) {
            clear_to_color(soft_buffer, makecol(0,0,72));
            
            set_trans_blender(0, 0, 0, 255-(uclock*8));

            draw_trans_sprite(soft_buffer, the_end, XRES/2 - the_end->w/2,
                YRES/2 - (the_end->h/2+20));

            updateok = 0;
            --uclock;
            scfix(1);
        } else
            Sleep(1);
    }
    
    if (!b_s_reload && gd.reload_option) {
        int r_opt = 0;
        while (!r_opt) {
            updateok = systemtimer();
            if (updateok) {
                clear_to_color(soft_buffer, makecol(0,0,72));
                draw_sprite(soft_buffer, the_end, XRES/2 - the_end->w/2,
                    YRES/2 - (the_end->h/2+20));
                    
                draw_sprite(soft_buffer, restart, XRES/2 - restart->w/2,
                    YRES - (YRES/6));
                  
                draw_mouseptr(soft_buffer, NULL, 0);  

                scfix(1);
            } else {
                Sleep(10);
            }

            if (gd.stored_rcv == 255) {
                r_v = 101;
                break;
            }
            
            if (mouse_b & 1) {
                if (mouse_x > XRES/2 - restart->w/2 &&
                    mouse_x < XRES/2 + restart->w/2)
                {
                    if (mouse_y > YRES - (YRES/6) &&
                        mouse_y < YRES - (YRES/6) + restart->h)
                    {
                        r_opt = 1;
                        r_v = 2;
                    }

                }

                while(mouse_b) rest(1);
            }
        }
    } else {
        while(!(mouse_b & 1)) {
            if (gd.stored_rcv == 255)
                break;
            rest(10);
        }
        while(mouse_b) rest(5);
        r_v = 101;
    }
    
    destroy_all_sound_handles();
    
    if (r_v == 2) {
        updateok = systemtimer();
        load_savefile(gd.reload_option);
        unfreeze_sound_channels();
    }
    
    
    destroy_bitmap(the_end);
    destroy_bitmap(restart);
    
    RETURN(r_v);
}

int show_front_door(void) {
    int i_option = 0;
    int b_can_resume;
 
    onstack("show_front_door");
    
    if (gd.p_lev[gd.a_party] < 0) {
        poop_out("Party is not placed properly.\nYour dungeon.lua needs a dsb_party_place() call.\n\nThis error could also be the result of Lua parsing errors\nin your dungeon or your custom Lua.\nCheck log.txt for error details.");
        RETURN(-2);
    }
    
    b_can_resume = check_for_any_savegame();
    
    i_option = lc_parm_int("sys_game_start", 1, b_can_resume);
    
    if (i_option == 0) {
        RETURN(0);
    } else if (i_option == 1) {
        if (b_can_resume) {
            RETURN(1);
        } else {
            RETURN(0);
        }
    } else {
        RETURN(-2);
    }
}

void show_load_ready(int rtp_only) {
    FONT *font = ROOT_SYSTEM_FONT;
    int i_choice = 0;
    char *svf;
    int cx, cy;
    int i_clock = 0;
    int clicktimer = 0;
    
    onstack("show_load_ready");
    
    cx = XRES/2 - (gd.simg.control_bkg->b->w/2);
    cy = YRES/2 - (gd.simg.control_bkg->b->h/2);
    
    if (rtp_only)
        goto READY_TO_PLAY_ONLY;
    
    instant_click = 0;
    while (!i_choice || clicktimer) {
        updateok = systemtimer();
        if (updateok) {
            clear_to_color(soft_buffer, 0);
            memset(cz, 0, sizeof(cz));

            draw_gui_controls(soft_buffer, cx, cy, 0);
            draw_file_names(soft_buffer, cx, cy);
            draw_mouseptr(soft_buffer, NULL, 0);

            updateok = 0;
            scfix(1);
        } else
            Sleep(4);
            
        if (gd.stored_rcv == 255) {
            Sleep(5);
            VOIDRETURN();
        }
            
        if (!clicktimer && ((instant_click & 1) || (mouse_b & 1))) {
            int z = scan_clickzones(cz, mouse_x, mouse_y, 10);
            if (z != -1) {
                gd.gui_down_button = z;
                clicktimer = 25;
                interface_click();
                svf = pick_savefile_zone(z);
                i_choice = check_for_savefile(svf, NULL, NULL);
                if (!i_choice) {
                    dsbfree(svf);
                    svf = NULL;
                }
            }
        }
        instant_click = 0;

        if (clicktimer) {
            --clicktimer;
            if ((mouse_b & 1) && (clicktimer == 0))
                clicktimer = 1;
                
            if (!clicktimer)
                gd.gui_down_button = 0;
        }
    }
    
    gd.reload_option = svf;
    load_savefile(svf);
    
    READY_TO_PLAY_ONLY:
    while (mouse_b) Sleep(4);
    
    clear_to_color(soft_buffer, 0);
    scfix(1);
    while (i_clock < 10) {
        updateok = systemtimer();
        if (updateok)
            ++i_clock;
        Sleep(10);
    }
    
    i_choice = 0;
    instant_click = 0;
    while (!i_choice || clicktimer) {
        updateok = systemtimer();
        if (updateok) {
            clear_to_color(soft_buffer, 0);
            memset(cz, 0, sizeof(cz));

            draw_gui_controls(soft_buffer, cx, cy, 12);
            
            textout_ex(soft_buffer, font, "GAME LOADED, READY TO PLAY.",
                cx+64, cy+50, makecol(255, 255, 255), -1);
                
            textout_ex(soft_buffer, font, "PLAY GAME", cx+170, cy+220,
                makecol(255, 255, 255), -1);
            
            draw_mouseptr(soft_buffer, NULL, 0);

            updateok = 0;
            scfix(1);
        } else
            Sleep(4);
            
        if (gd.stored_rcv == 255) {
            Sleep(5);
            VOIDRETURN();
        }

        if (!i_choice && ((instant_click & 1) || (mouse_b & 1))) {
            int z = scan_clickzones(cz, mouse_x, mouse_y, 10);
            if (z == 4) {
                i_choice = 1;
                gd.gui_down_button = z;
                clicktimer = 25;
                interface_click();
            }
        }
        
        if (clicktimer)
            --clicktimer;
            
        instant_click = 0;
    }

    while (mouse_b) Sleep(4);

    unfreeze_sound_channels();
    gd.gui_down_button = 0;

    VOIDRETURN();
}


int show_ending(int boring) {
    int local_updateok;
    FMOD_SOUND *music;
    FMOD_CHANNEL *chan;
    int i_clock = 0;
    int v;
    BITMAP *dng;
    BITMAP *rt;
    BITMAP *the_end;
    int wht = makecol(255, 255, 255);
    int starsx[NSTARS];
    int starsy[NSTARS];
    FMOD_CREATESOUNDEXINFO ex_i;
    int c_T_BPS = T_BPS;
    int xtimer=0;
    int oxtimer[64] = { 0 };
    int xtd = 2;
    char sm;
    
    onstack("show_ending");
    
    clear_mouse_override();
    if (boring) {
        show_the_end_screen(1);
        RETURN(255);
    }
    
    music = internal_load_sound_data("CHEESE");
    
    FMOD_System_PlaySound(f_system, FMOD_CHANNEL_FREE, music, TRUE, &chan);
    FMOD_Channel_SetLoopCount(chan, -1);
    while (key[KEY_ESC]) Sleep(1);
    
    FMOD_Channel_SetPaused(chan, FALSE);

    clear_to_color(soft_buffer, 0);
    scfix(1);
    
    local_updateok = 0;
    while (i_clock < 10) {
        local_updateok = systemtimer();
        if (local_updateok)
            ++i_clock;
        Sleep(10);
    }
    
    dng = pcxload("DSBSMALL", NULL);
    the_end = pcxload("THE_END", NULL);

    for(v=0;v<NSTARS;++v) {
        starsx[v] = 2 + DSBtrivialrand()%636;
        starsy[v] = 64 + DSBtrivialrand()%320;
    }

    if (gd.trip_buffer) {
        sm = 0;
        rt = scr_pages[gd.scr_blit];
    } else {
        sm = 1;
        rt = soft_buffer;
    }

    T_BPS = 60;
    init_systemtimer();

    while ((gd.stored_rcv != 255) && !(mouse_b & 1) && !key[KEY_ESC]) {

        local_updateok = systemtimer();
        //updateok = 1;
        if (local_updateok) {
            int n;
            int z;
            char tscr[23];
            int bv = 1;

            if (xtimer < 16 && xtimer > -16)
                bv = 2;

            xtimer += xtd*bv;
            if (xtimer < -36 || xtimer > 36)
                xtd *= -1;

            for (z=30;z>=0;--z)
                oxtimer[z+1] = oxtimer[z];
            oxtimer[0] = xtimer;

            if (!sm)
                rt = scr_pages[gd.scr_blit];

            clear_to_color(rt, 0);

            for(z=0;z<NSTARS;++z) {
                starsx[z] += (z%16+1);

                if (starsx[z] > 637) {
                    starsx[z] = 2;
                    starsy[z] = 64 + DSBtrivialrand()%320;
                }

                dsbputpixel(rt, starsx[z], starsy[z], wht);
            }

            for (z=0;z<32;z++) {
                n = oxtimer[z];
                if (n > 32) n = 32;
                if (n < -32) n = -32;

                masked_blit(dng, rt, z*10, 0, 164+z*10, 30+n, 10, dng->h);
            }
            
            draw_sprite(rt, the_end, (320 - the_end->w/2), 260);

            scfix(sm);
        }
    }

    rest(2);
    
    destroy_bitmap(dng);
    
    FMOD_Channel_Stop(chan);
    FMOD_Sound_Release(music);
    
    RETURN(255);
}

void show_introduction(void) {
    int local_updateok;
    FONT *font = ROOT_SYSTEM_FONT;
    int uclock = 80;
    BITMAP *dng;
    BITMAP *sb;
    int ct = 0;
    DATAFILE *s_dat;
    FMOD_CREATESOUNDEXINFO ex_i;
    FMOD_SOUND *gsnd;
    FMOD_CHANNEL *chan;
    int gig = DSBtrivialrand()%32;

    onstack("show_introduction");
    
    systemtimer();
    while (ct<10) {
        int r = systemtimer();
        ct += r;
        clear_to_color(soft_buffer, makecol(0, 0, 72));
        scfix(1);
        Sleep(1);
    }
    
    if (lc_parm_int("sys_game_intro", 0))
        VOIDRETURN();
    
    if (!gig) {
        s_dat = load_datafile_object(bfixname("graphics.dat"), "MONSTER_OOWOOAH");
        if (s_dat) {
            memset(&ex_i, 0, sizeof(FMOD_CREATESOUNDEXINFO));
            ex_i.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
            ex_i.length = s_dat->size;
            FMOD_System_CreateSound(f_system, s_dat->dat, FMOD_OPENMEMORY|FMOD_2D,
                &ex_i, &gsnd);
            unload_datafile_object(s_dat);
        } else gig = 1;
    }
    
    dng = pcxload("DUNGEON", NULL);
    sb = pcxload("SBPCX", NULL);
    
    local_updateok = 0;    
    updateok = 0;
    ReleaseSemaphore(semaphore, 1, NULL);
    
    while (uclock > 0) {
        if (gd.ini_options & INIOPT_SEMA)
            local_updateok = updateok;
        else
            local_updateok = systemtimer();
        
        if (local_updateok) {
            clear_to_color(soft_buffer, makecol(0,0,72));
            stretch_blit(dng, soft_buffer, 0, 0, dng->w, dng->h, 2+(uclock*4), 96+(uclock),
                (dng->w*(80-uclock))/80, (dng->h*(80-uclock))/80);
            scfix(1);
            uclock -= (3*local_updateok);
            local_updateok = 0;
            updateok = 0;
        } else {
            if (gd.ini_options & INIOPT_SEMA)
                WaitForSingleObject(semaphore, 50);
            else
                rest(0);
        }
        
        if (gd.stored_rcv == 255)
            break;
    }
        
    if (!gig)
        FMOD_System_PlaySound(f_system, FMOD_CHANNEL_FREE, gsnd, FALSE, &chan);

    if (gd.stored_rcv == 255)
        goto ALL_FINISHED;

    uclock = 31;
    while (uclock > 0) {
        
        if (gd.ini_options & INIOPT_SEMA)
            local_updateok = updateok;
        else
            local_updateok = systemtimer();
        
        if (local_updateok) {
            clear_to_color(soft_buffer, makecol(0,0,72));
            blit(dng, soft_buffer, 0, 0, 2, 96, dng->w, dng->h);
            set_trans_blender(0, 0, 0, 255-(uclock*8));
            draw_trans_sprite(soft_buffer, sb, 17, 245);
            local_updateok = 0;
            updateok = 0;
            --uclock;
            scfix(1);
        } else {
            if (gd.ini_options & INIOPT_SEMA)
                WaitForSingleObject(semaphore, 50);
            else
                rest(0);
        }
        
        if (gd.stored_rcv == 255) {
            goto ALL_FINISHED;
        }
    }
    
    ct = 0;
    while(ct<30) {
        if (gd.ini_options & INIOPT_SEMA) {
            ct += updateok;
            updateok = 0;
            WaitForSingleObject(semaphore, 50);
        } else {
            ct += systemtimer();
            scfix(1);
            Sleep(1);
        }
    }
    
    updateok=0;

    ALL_FINISHED:
    destroy_bitmap(dng);
    destroy_bitmap(sb);
    
    if (!gig) {
        FMOD_Channel_Stop(chan);
        FMOD_Sound_Release(gsnd);
    }
    
    VOIDRETURN();
}
