#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "gproto.h"
#include "movebuffer.h"
#include "timer_events.h"
#include "arch.h"
#include "gamelock.h"
#include "keyboard.h"
#include "istack.h"
#include "mparty.h"
#include "render.h"

int viewstate = 0;

extern struct dsbkbinfo kbi;

extern volatile unsigned int updateok;
extern int debug;
extern struct global_data gd;
extern struct graphics_control gfxctl;
extern BITMAP *scr_pages[3];
extern BITMAP *soft_buffer;
extern BITMAP *FORCE_RENDER_TARGET;
extern struct dungeon_level *dun;
extern struct movequeue *mq;

extern struct timer_event *te;
extern struct timer_event *new_te;
extern struct timer_event *eot_te;
extern struct timer_event *a_te;

extern int T_BPS;
extern int CLOCKFREQ;
extern HANDLE semaphore;
extern int instant_click;

extern int show_fps;

extern unsigned int CurrentKey;

extern struct clickzone cz[NUM_ZONES];
extern struct inst *oinst[NUM_INST];
extern int Gmparty_flags;
extern istack istaparty;
    
void renderer_fixes(int viewstate) {
    int i;
    
    for (i=0;i<4;++i) {
        if (gd.showdam_ttl[i]) {
            --gd.showdam_ttl[i];
            
            if (!gd.showdam_ttl[i]) {
                gd.showdam[i] = 0;
                gd.showdam_t[i] = 0;
            }
        }
    }
    
    if (viewstate == VIEWSTATE_DUNGEON)
        gd.need_cz = 1;
        
    if (gd.attack_msg_ttl) {
        --gd.attack_msg_ttl;   
        if (gd.attack_msg_ttl == 0) {
            if (gd.attack_msg) {
                dsbfree(gd.attack_msg);
                gd.attack_msg = NULL;
            }
        }
    }
               
    if (gd.arrowon)
        gd.arrowon = 0;
    else              
        if (gd.litarrow)
            gd.litarrow = 0;
            
}

int go_oof(int ap, int tx, int ty, int oface) {

    onstack("go_oof");

    if (gd.leader != -1) {
        ist_push(&istaparty, ap);
        lc_parm_int("sys_wall_hit", 4, gd.p_lev[ap], tx, ty, oface);
        ist_pop(&istaparty);
        
        RETURN(1);
    }

    RETURN(0);
}

void go_oof_on_object(int ap, int tx, int ty, int oface, int hv_inst) {

    onstack("go_oof_on_object");

    if (gd.leader != -1) {
        ist_push(&istaparty, ap);
        lc_parm_int("sys_inst_hit", 4, gd.p_lev[ap], tx, ty, oface);
        ist_pop(&istaparty);
    }

    VOIDRETURN();
}

int dungeon_move(int ap, int mtype) {
    int dx = 0, dy = 0;
    int tx, ty;
    int oface;
    int encum = 1;
    
    onstack("dungeon_move");
        
    if (mtype <= MOVE_LEFT) {
        oface = (gd.p_face[ap] + mtype) % 4;
    
        face2delta(oface, &dx, &dy);
        tx = gd.p_x[ap] + dx;
        ty = gd.p_y[ap] + dy;
        
        ist_push(&istaparty, ap);
        if (lc_parm_int("sys_forbid_move", 3, gd.p_lev[ap], tx, ty))
            RETURN(0);
        ist_pop(&istaparty);
            
        // calls on_try_move
        if (p_to_tile(ap, T_TRY_MOVE, oface))
            RETURN(0);
        
        if (!check_for_wall(gd.p_lev[ap], tx, ty)) {
            int hv = party_objblock(ap, gd.p_lev[ap], tx, ty);
            if (!hv) {
                struct inst_loc *pushcol;
                
                switch (mtype) {
                    case MOVE_UP:
                        pushcol = party_extcollision_forward(ap, oface, tx, ty);
                    break;
                    
                    case MOVE_RIGHT:
                        pushcol = party_extcollision_right(ap, oface, tx, ty);
                    break;
    
                    case MOVE_BACK:
                        pushcol = party_extcollision_back(ap, oface, tx, ty);
                    break;
                    
                    case MOVE_LEFT:
                        pushcol = party_extcollision_left(ap, oface, tx, ty);
                    break;
                }
                
                if (GMP_ENABLED() && (Gmparty_flags & MPF_SPLITMOVE))
                    ap = mparty_split(gd.leader, ap);
                    
                party_moveto(ap, gd.p_lev[ap], tx, ty, 0, PCC_ACTUALMOVE);
                
                ist_push(&istaparty, ap);
                encum += lc_parm_int("sys_party_move", 0);
                ist_pop(&istaparty);
        
            } else if (hv > 0)
                encum += go_oof(ap, tx, ty, oface);
                
            if (hv != 0) {
                go_oof_on_object(ap, tx, ty, oface, hv);
            }
            
        } else {
            encum += go_oof(ap, tx, ty, oface);
        }
        
        if (mtype == MOVE_LEFT)
            gd.litarrow = 4;
        else if (mtype == MOVE_BACK)
            gd.litarrow = 5;
        else if (mtype == MOVE_RIGHT)
            gd.litarrow = 6;
        else
            gd.litarrow = 2;

    } else {
        
        if (mtype == MOVE_TURNRIGHT) {
            int oldface = gd.p_face[ap];

            gd.p_face[ap] = (gd.p_face[ap] + 1) % 4;
            gd.litarrow = 3; 
            change_facing(ap, oldface, gd.p_face[ap], 1);
        } else if (mtype == MOVE_TURNLEFT) {
            int oldface = gd.p_face[ap];
            
            gd.p_face[ap] = (gd.p_face[ap] + 3) % 4;
            gd.litarrow = 1;
            change_facing(ap, oldface, gd.p_face[ap], 1);
        }
    }
    
    gd.arrowon = 1;
    RETURN(encum);
}

int check_moves() {
    int rv = 0;
    
    if (gd.move_lock == 0) {
        int m_t, m_x, m_y;
        int lockvar = 0;
                    
        // do a move
        while (!lockvar && popmovequeue(&m_t, &m_x, &m_y)) {
            
            // NOTICE: we CANNOT process any mouseclicks immediately after
            // a move because the renderer sets up the clickzones and so
            // a frame must be drawn before it has a clue what's going on!
            if (m_t == PRESS_MOVEKEY) {
                int ap = gd.a_party;
                int movedir = m_x;
                lockvar = 1;
                
                if (gd.forbid_move_timer && movedir < MOVE_TURNLEFT) {
                    int dest_dir = (gd.p_face[ap] + movedir) % 4;
                    if (gd.forbid_move_dir == dest_dir) {
                        // requeue and give up
                        addmovequeuefront(m_t, m_x, m_y);
                        return 0;    
                    }                    
                }
                
                gd.move_lock = dungeon_move(ap, movedir);
                if (gd.move_lock)
                    gd.forbid_move_timer = 0;
                rv = 1;
            }
              
        }
    }
    
    return rv;
}

/*
#define IFACE_KEY(I, K) \
    if (DSBcheckkey(K)) {\
        if (!iface_lock[I]) {\
            addmovequeue(PRESS_INTERFACEKEY, K, 0);\
            iface_lock[I] = 1;\
        }\
    } else\
        iface_lock[I] = 0
*/

//no more locking
#define IFACE_KEY(I, K) \
    if (DSBcheckkey(K)) {\
        addmovequeue(PRESS_INTERFACEKEY, K, 0);\
    }
        
int DSBkeyboard_input(int i_kl) {
    
    static int lockable_keys[] = {
        KEY_ENTER,
        KEY_ESC,
        KEY_ESC,
        -1
    };
    
    // none of this locking stuff is needed anymore
    // since 0.58 when i (mostly) switched to a
    // keyboard queue -- i won't delete it yet
    // in case it's needed for something
    static int dir_lock[6] = { 0 };
    static int access_lock = 0;
    static int rune_lock = 0;
    static int magic_lock[12] = { 0 };
    static int method_lock[4] = { 0 };
    static int iface_lock[6] = { 0 };
    
    onstack("DSBkeyboard_input");
    
    if (kbi.k[KD_FREEZEGAME][0] > 0)
        lockable_keys[1] = kbi.k[KD_FREEZEGAME][0];
    if (kbi.k[KD_FREEZEGAME][1] > 0)
        lockable_keys[2] = kbi.k[KD_FREEZEGAME][1];
    
    if (gd.gui_mode == GUI_NAME_ENTRY) {
        DSBkeyboard_name_input();
        RETURN(0);
    } else if (gd.gui_mode == GUI_ENTER_SAVENAME) {
        DSBkeyboard_gamesave_input();
        RETURN(0);
    }
    
    if (*gd.gl_viewstate <= VIEWSTATE_INVENTORY) {
        int i_inv = 0;
        int i_magic = 0;
        int i_method = 0;
        int unlocks = 6;
        
        for (i_inv = 0;i_inv < 6;i_inv++) {
            if (DSBcheckkey(KD_INVENTORY1 + i_inv)) {
                if (!access_lock) {
                    addmovequeue(PRESS_INVKEY, i_inv, 0);
                    access_lock = 0;
                }
            } else
                --unlocks;
        }
        if (!unlocks)
            access_lock = 0;
        
        for (i_magic = 0;i_magic < 12;i_magic++) {
            if (DSBcheckkey(KD_MZ01 + i_magic)) {
                if (!magic_lock[i_magic]) {
                    if (i_magic < 6 || !rune_lock) {
                        addmovequeue(PRESS_MAGICKEY, i_magic, 0);
                        magic_lock[i_magic] = 0;
                        if (i_magic >= 6)
                            rune_lock = 4;
                    }
                }
            } else {
                magic_lock[i_magic] = 0;
            }
        }
        if (rune_lock > 0)
            --rune_lock;
        
        for (i_method = 0;i_method < 4;i_method++) {
            if (DSBcheckkey(KD_WZ00 + i_method)) {
                if (!method_lock[i_method]) {
                    addmovequeue(PRESS_METHODKEY, i_method, 0);
                    method_lock[i_method] = 0;
                }
            } else
                method_lock[i_method] = 0;
        }
        
        IFACE_KEY(0, KD_ROWSWAP);
        IFACE_KEY(1, KD_SIDESWAP);
        IFACE_KEY(2, KD_LEADER_CYCLE);
        IFACE_KEY(3, KD_LEADER_CYCLE_BACK);
        IFACE_KEY(4, KD_GOSLEEP);
        IFACE_KEY(5, KD_QUICKSAVE);        
    }
    
    if (*gd.gl_viewstate != VIEWSTATE_DUNGEON)
        RETURN(0);
    
    /*
    if (i_kl) {
        int i = 0;
        while (lockable_keys[i] != -1) {
            if (key[lockable_keys[i]])
                RETURN(1);
                 
            ++i;
        }
        
        RETURN(0);
    }
    */
    
    if (DSBcheckkey(KD_FORWARD)) {
        if (!dir_lock[0]) {
            addmovequeue(PRESS_MOVEKEY, MOVE_UP, 0);
            dir_lock[0] = 0;
            RETURN(0);
        }
    } else
        dir_lock[0] = 0;
    
    if (DSBcheckkey(KD_LEFT_TURN)) {
        if (!dir_lock[5]) {
            addmovequeue(PRESS_MOVEKEY, MOVE_TURNLEFT, 0);
            dir_lock[5] = 0;
            RETURN(0);
        }
    } else
        dir_lock[5] = 0;
    
    if (DSBcheckkey(KD_RIGHT_TURN)) {
        if (!dir_lock[4]) {
            addmovequeue(PRESS_MOVEKEY, MOVE_TURNRIGHT, 0);
            dir_lock[4] = 0;
            RETURN(0);
        }
    } else
        dir_lock[4] = 0;
    
    if (DSBcheckkey(KD_LEFT)) {
        if (!dir_lock[1]) {
            addmovequeue(PRESS_MOVEKEY, MOVE_LEFT, 0);
            dir_lock[1] = 0;
            RETURN(0);
        }
    } else
        dir_lock[1] = 0;
    
    if (DSBcheckkey(KD_BACK)) {
        if (!dir_lock[2]) {
            addmovequeue(PRESS_MOVEKEY, MOVE_BACK, 0);
            dir_lock[2] = 0;
            RETURN(0);
        }
    } else
        dir_lock[2] = 0;
    
    if (DSBcheckkey(KD_RIGHT)) {
        if (!dir_lock[3]) {
            addmovequeue(PRESS_MOVEKEY, MOVE_RIGHT, 0);
            dir_lock[3] = 0;
            RETURN(0);
        }
    } else
        dir_lock[3] = 0;
              
    RETURN(0);
}

void misc_frame_updates(void) {
    if (gd.mouse_hidden == HIDEMOUSEMODE_HIDE_OVERRIDE) {
        if (mouse_x != gd.mouse_hcx || mouse_y != gd.mouse_hcy) {
            gd.mouse_hidden = HIDEMOUSEMODE_SHOW;
        }
    }
}

int DSBgameloop(int new_game) {
    int altdown = 0;
    int dirty = 1;
    int softmode = 1;
    int next_softmode = 1;
    int ingameloop = 1;
    int viewstate_PUSH = VIEWSTATE_DUNGEON;
    int keylock = 1;
    int game_fps = 0;
    int framesdrawn = 0;
    int gclock = 0;
    int mouse_lock = 0;
    int rcv = 0;
    int d_rest = 1;
    int dec_this_cyc = 0;
    
    onstack("DSBgameloop");
    
    updateok = 0;
    gd.need_cz = 1;
    
    // hack
    gd.gl_viewstate = &viewstate;
    viewstate = VIEWSTATE_DUNGEON;
    
    FORCE_RENDER_TARGET = NULL;
    
    gd.tickclock = 0; 
    gd.a_tickclock = 0;
    
    if (new_game)
        party_enter_level(0, gd.p_lev[0]);
    else {
        lc_parm_int("sys_game_load", 0); 
        import_shading_info();          
    }
        
    gd.mouse_hidden = HIDEMOUSEMODE_SHOW;
    clear_mouse_override();
    if (gd.stored_rcv == 255)
        ingameloop = 0;
        
    if (gd.ini_options & INIOPT_SEMA) {
        ReleaseSemaphore(semaphore, 1, NULL);
    }
    
    dsb_clear_immed_key_queue();
    instant_click = 0;
    
    lc_parm_int("sys_game_beginplay", 0);
    
    while(ingameloop > 0) {
        int process_tick = TICK_NONE;
        int ticks_missing = 0;
        
        if (!(gd.ini_options & INIOPT_SEMA)) {
            updateok = systemtimer();
        }
        
        if (updateok > 0) {

            process_tick = TICK_NONE;               
            dirty = 1;
            
            ++gclock;
            gd.magic_this_frame = 1;

            // don't advance a frozen game
            if (viewstate < VIEWSTATE_FROZEN) {
                gd.framecounter++;
                if (gd.framecounter >= FRAMECOUNTER_MAX)
                    gd.framecounter = 0;
                    
                if ((gd.framecounter % T_BPS) == 0) {
                    lc_parm_int("sys_tick_second", 0);
                }
                    
                adv_inst_and_cond_framecounters();
                
                gd.tickclock++;
                if (viewstate == VIEWSTATE_SLEEPING)
                    gd.tickclock += CLOCKFREQ;
                    
                lc_parm_int("sys_tick_frame", 0);
                    
                gd.a_tickclock++;
                
            }
            gd.gui_framecounter++;
            gd.lod_timer++;
            frame_lod_use_check();
            misc_frame_updates();
            
            if (viewstate == VIEWSTATE_GUI) {
                do_gui_update();
            }
                                            
            updateok = 0;
                                
            // update the frame rate
            if (gclock == T_BPS) {
                game_fps = framesdrawn;
                framesdrawn = gclock = 0;
                
                check_sound_channels();
            }
                       
            if (gd.everyone_dead) {
                int n;
                
                for (n=0;n<4;++n) {
                    gd.showdam[n] = 0;
                    gd.showdam_ttl[n] = 0;
                }
                
                --gd.everyone_dead;
                
                if (gd.everyone_dead == 1) {
                    rcv = -2;
                    ingameloop = 0;
                }
                
                continue;   
            }
                       
            // process queued up instant action type stuff
            if (!gd.need_cz && !(gd.gl->lockc[LOCK_MOVEMENT] || gd.gl->lockc[LOCK_MOUSE])) {
                while (mq && mq->type >= PRESS_INVKEY) {
                    int m_x, m_y, m_t;
                    if (popmovequeue(&m_t, &m_x, &m_y)) {                   
                        if (m_t >= PRESS_MOUSELEFT)
                            got_mouseclick((m_t - PRESS_MOUSELEFT), m_x, m_y, viewstate);
                        else
                            got_mkeypress(m_t, m_x);
                    }       
                }
            }
            
            if (gd.tickclock >= CLOCKFREQ) {
                process_tick = TICK_TIMED;
            }
            
            // check_moves does a lot of stuff. it's not just a check.
            // it needs to be executed no matter what.
            if (check_moves()) {
                if (process_tick == TICK_NONE) {
                    process_tick = TICK_FORCED;
                    ticks_missing = CLOCKFREQ - gd.tickclock;
                }
            }
        
            if (gd.a_tickclock >= CLOCKFREQ) {
                gd.a_tickclock -= CLOCKFREQ;
                run_timers(a_te, &a_te, TE_DECMODE);
            }
            
            // a dungeon update frame (~5-6hz)  
            if (process_tick) {
                v_onstack("gameloop.process_tick");
                gd.tickclock = 0;
                gd.process_tick_mode = process_tick;
                
                purge_dirty_list();
                
                set_3d_soundcoords();
                renderer_fixes(viewstate);
                update_console();
                
                if (ticks_missing > 0) {
                    adv_crmotion_with_missing(ticks_missing);
                }
                 
                if (!gd.gl->lockc[LOCK_MOVEMENT]) {
                    party_update();
                    if (gd.move_lock > 0)
                        --gd.move_lock;
                    
                    if (gd.forbid_move_timer > 0)
                        --gd.forbid_move_timer;
                }
                   
                run_timers(te, &te, TE_FULLMODE);
                run_timers(a_te, &a_te, TE_RUNMODE);
                append_new_queue_te(te, new_te); 
                
                if (gd.run_everywhere_count > 0)
                    gd.run_everywhere_count--;
                else
                    gd.run_everywhere_count = 3;
                
                v_upstack();
            } else {
                collect_lua_incremental_garbage(1);
            }
        }
        
        // don't drag a guy too far out from the corner
        if (gd.mouse_guy && 
            ((mouse_x < (gfxctl.guy_x)) || (mouse_x > gfxctl.guy_x + 80)) || 
            ((mouse_y < gfxctl.guy_y) || (mouse_y > gfxctl.guy_y + 60)))
        {
            gd.mouse_guy = 0;
        }
        
        dsb_shift_immed_key_queue();
        
        // don't lock out a new mouse event
        if (CurrentKey & 0x80000000)
            mouse_lock = 0;
        
        if (!mouse_lock && !gd.gl->lockc[LOCK_MOUSE]) {
            if (viewstate == VIEWSTATE_SLEEPING) {
                if (mouse_b) {
                    wake_up();
                    viewstate = VIEWSTATE_DUNGEON;
                    mouse_lock = 1;
                }

            } else if (viewstate == VIEWSTATE_FROZEN) {
                if (mouse_b) {
                    viewstate = viewstate_PUSH;
                    unfreeze_sound_channels();
                    mouse_lock = 1;
                }

            } else {
                int meta = (key[KEY_LSHIFT] || key[KEY_RSHIFT] ||
                    key[KEY_LCONTROL] || key[KEY_RCONTROL]);
                
                int c_mx = mouse_x;
                int c_my = mouse_y;   
                int iclick = 0;
                
                if (CurrentKey & 0x80000000) {
                    c_mx = (CurrentKey >> 4) & 8191;
                    c_my = (CurrentKey >> 18) & 8191;
                    iclick = CurrentKey & 7;
                } 
                    
                if ((iclick & 1) || ((mouse_b & 1) && !meta)) {
                    if (mouse_y < gd.vyo || viewstate != VIEWSTATE_DUNGEON) {
                        got_mouseclick(0, c_mx, c_my, viewstate);
                    } else
                        addmovequeue(PRESS_MOUSELEFT, c_mx, c_my);
                    mouse_lock = 1;

                } else if (iclick & 2) {
                    if (viewstate == VIEWSTATE_GUI) {
                        got_mouseclick(1, c_mx, c_my, viewstate); 
                    } else   
                        addmovequeue(PRESS_MOUSERIGHT, c_mx, c_my);
                    mouse_lock = 2;
                
                } else if ((iclick & 4) || ((mouse_b & 1) && meta)) {
                    if (viewstate != VIEWSTATE_DUNGEON) {
                        got_mouseclick(2, c_mx, c_my, viewstate);
                    } else
                        addmovequeue(PRESS_MOUSEMIDDLE, c_mx, c_my);
                    mouse_lock = 4;                    
                }
                instant_click = 0;
            }
        
        } else {
            if (!mouse_b) {
                mouse_lock = 0;

                if (gd.mouse_mode >= MM_EYELOOK)
                    gfxctl.do_subrend = 1;
                    
                gd.mouse_mode = 0;
            }
        }
        
        if (gd.stored_rcv) {
            ingameloop = 0;
            updateok = 0;
            rcv = gd.stored_rcv;
            gd.stored_rcv = 0;
        }
        
        while (eot_te) {
            struct timer_event *c_eot_te = eot_te;
            eot_te = NULL;
            run_timers(c_eot_te, &c_eot_te, TE_FULLMODE);
            if (c_eot_te) {
                program_puke("Leaky eot_te queue");
            }
        }
        
        if (dirty && !updateok) {
            int idr = 0;
            int ifd = 0;
            BITMAP *sct;
            int vxo, vyo, tvyo;
            
            
            softmode = next_softmode;
            if (!softmode)
                softmode = check_subgfx_state(viewstate);
            
            sct = find_rendtarg(softmode);
            
            vxo = gd.vxo;
            vyo = gd.vyo + gd.vyo_off;
            tvyo = vyo - 86;
            
            if (viewstate == VIEWSTATE_DUNGEON) {
                
                next_softmode = render_dungeon_view(softmode, gd.need_cz);
                
                if (next_softmode == -1) {
                    purge_defer_list();
                    openclip(softmode);
                    softmode = next_softmode = 1;
                    render_dungeon_view(1, gd.need_cz);
                    openclip(1);
                    sct = find_rendtarg(softmode);
                } else
                    openclip(softmode);
                    
                idr = draw_champions(softmode, sct, -1);
                    
                gd.need_cz = 0;
            }
            
            if (viewstate == VIEWSTATE_INVENTORY ||
                viewstate == VIEWSTATE_CHAMPION) 
            {
                render_inventory_view(softmode, process_tick);               

            } else if (viewstate == VIEWSTATE_SLEEPING) {
                
                setup_background(sct, 0);
                idr = draw_champions(softmode, sct, -1);
                textout_centre_ex(sct, font, "WAKE UP", vxo+224, tvyo+208, gd.sys_col, -1);

            } else if (viewstate == VIEWSTATE_FROZEN) {

                setup_background(sct, 0);
                idr = draw_champions(softmode, sct, -1);
                textout_centre_ex(sct, font, "GAME FROZEN", vxo+224, tvyo+208, gd.sys_col, -1);
                textout_centre_ex(sct, font, "PRESS ENTER FOR DSB MENU", vxo+224, tvyo+236, gd.sys_col, -1);

            } else if (viewstate == VIEWSTATE_GUI) {
                do_gui_options(softmode);
            }

            if (show_fps) {
                textprintf_ex(sct, font, 8, 470,
                    makecol(255, 255, 255), -1, "MODE %d FPS %d",
                    softmode, game_fps);
            }
            
            ifd = draw_interface(softmode, viewstate, 1);

            if (gd.lua_error_found)
                draw_lua_errors(sct);
                
            scfix(softmode);
            
            if (idr || ifd || !gd.trip_buffer)
                next_softmode = 1;
            
            dirty = 0;
            ++framesdrawn;
            d_rest = 0;
        }
        
        if (!(gd.ini_options & INIOPT_SEMA)) {
            if (!dirty && !updateok) {
                if (!d_rest) {
                    int sti = getsleeptime();
                    if (sti > 0) {
                        d_rest = 1;
                        rest(sti);
                    }
    
                } else
                    rest(1);
            }
        }
        
        if (DSBcheckkey(KD_FREEZEGAME)) {
            if (viewstate == VIEWSTATE_FROZEN) {
                viewstate = viewstate_PUSH;
                unfreeze_sound_channels();
            } else {
                viewstate_PUSH = viewstate;
                viewstate = VIEWSTATE_FROZEN;
                freeze_sound_channels();
                dsb_mem_maintenance();
                collect_lua_garbage();
                dsb_clear_keybuf();  
                gd.litarrow = 0;
            }
            while(DSBcheckkey_state(KD_FREEZEGAME)) {
                Sleep(5);
                updateok = 0;
            }
        }
        
        if (DSBcheckkey(KD_QUICKQUIT)) {
            gd.stored_rcv = 255;
            break;
        }
                
        if (key[KEY_ENTER]) {
            if (viewstate == VIEWSTATE_FROZEN) {
                while(key[KEY_ENTER]) {
                    //poll_keyboard();
                    Sleep(5);
                }

                gui_frozen_screen_menu();
                
                while(key[KEY_ENTER]) {
                    //poll_keyboard();
                    Sleep(5);
                    updateok = 0;
                }
            }
            if (gd.stored_rcv) {
                ingameloop = 0;
                rcv = gd.stored_rcv;
                gd.stored_rcv = 0;
            }
        }
            
        
        if (!gd.gl->lockc[LOCK_MOVEMENT])
            keylock = DSBkeyboard_input(keylock);
            
        if (gd.ini_options & INIOPT_SEMA) {
            WaitForSingleObject(semaphore, 50);
        }
    }
    destroymovequeue();
    
    RETURN(rcv);
}
