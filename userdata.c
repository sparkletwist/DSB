#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "keyboard.h"

struct dsbkbinfo kbi;

extern int CurrentKey;
extern CRITICAL_SECTION Queue_CS;

extern BITMAP *soft_buffer;
extern struct global_data gd;
extern int windowed;
extern int dynamicswitch;
extern int show_fps;

static int Disp_Zone;

// all these functions override the normal "font"
// with the DSB ROOT_SYSTEM_FONT so they stay consistent
// even if you're using a different font
extern FONT *ROOT_SYSTEM_FONT;

int DSBcheckkey(int i_kbc) {
    if (CurrentKey) {
        // top bit means something special, not keyboard info
        if (!(CurrentKey & 0x80000000)) {
            if (CurrentKey == kbi.k[i_kbc][0])
                return 1;
           
            if (CurrentKey == kbi.k[i_kbc][1])
                return 1; 
        }     
    }
    return 0;
}

int DSBcheckkey_state(int i_kbc) {

    if (kbi.k[i_kbc][0] > 0) {
        if (key[kbi.k[i_kbc][0]])
            return 1;
    }
    
    if (kbi.k[i_kbc][1] > 0) {
        if (key[kbi.k[i_kbc][1]])
            return 1;
    }
    
    return 0;
}

void get_key_config(void) {
    int u;
    const char *KBS = "Keyboard";
    
    onstack("get_key_config");
    
    kbi.k[KD_LEFT_TURN][0] = get_config_int(KBS, "TurnLeft", KEY_7_PAD);
    kbi.k[KD_LEFT_TURN][1] = get_config_int(KBS, "TurnLeftAlt", -1);

    kbi.k[KD_FORWARD][0] = get_config_int(KBS, "MoveForward", KEY_8_PAD);
    kbi.k[KD_FORWARD][1] = get_config_int(KBS, "MoveForwardAlt", KEY_UP);
    
    kbi.k[KD_RIGHT_TURN][0] = get_config_int(KBS, "TurnRight", KEY_9_PAD);
    kbi.k[KD_RIGHT_TURN][1] = get_config_int(KBS, "TurnRightAlt", -1);
    
    kbi.k[KD_LEFT][0] = get_config_int(KBS, "MoveLeft", KEY_4_PAD);
    kbi.k[KD_LEFT][1] = get_config_int(KBS, "MoveLeftAlt", KEY_LEFT);

    kbi.k[KD_BACK][0] = get_config_int(KBS, "MoveBack", KEY_5_PAD);
    kbi.k[KD_BACK][1] = get_config_int(KBS, "MoveBackAlt", KEY_DOWN);
    
    kbi.k[KD_RIGHT][0] = get_config_int(KBS, "MoveRight", KEY_6_PAD);
    kbi.k[KD_RIGHT][1] = get_config_int(KBS, "MoveRightAlt", KEY_RIGHT);
    
    kbi.k[KD_INVENTORY1][0] = get_config_int(KBS, "Inv1", KEY_F1);
    kbi.k[KD_INVENTORY1][1] = get_config_int(KBS, "Inv1Alt", KEY_1);
    
    kbi.k[KD_INVENTORY2][0] = get_config_int(KBS, "Inv2", KEY_F2);
    kbi.k[KD_INVENTORY2][1] = get_config_int(KBS, "Inv2Alt", KEY_2);
    
    kbi.k[KD_INVENTORY3][0] = get_config_int(KBS, "Inv3", KEY_F3);
    kbi.k[KD_INVENTORY3][1] = get_config_int(KBS, "Inv3Alt", KEY_3);
    
    kbi.k[KD_INVENTORY4][0] = get_config_int(KBS, "Inv4", KEY_F4);
    kbi.k[KD_INVENTORY4][1] = get_config_int(KBS, "Inv4Alt", KEY_4);
    
    kbi.k[KD_INVENTORY_CYCLE][0] = get_config_int(KBS, "InvCycle", KEY_F5);
    kbi.k[KD_INVENTORY_CYCLE][1] = get_config_int(KBS, "InvCycleAlt", KEY_5);
    
    kbi.k[KD_INVENTORY_CYCLE_BACK][0] = get_config_int(KBS, "InvCycleBack", KEY_F6);
    kbi.k[KD_INVENTORY_CYCLE_BACK][1] = get_config_int(KBS, "InvCycleBackAlt", KEY_6);
    
    kbi.k[KD_LEADER_CYCLE][0] = get_config_int(KBS, "LeaderCycle", KEY_F9);
    kbi.k[KD_LEADER_CYCLE][1] = get_config_int(KBS, "LeaderCycleAlt", KEY_0_PAD);
    
    kbi.k[KD_LEADER_CYCLE_BACK][0] = get_config_int(KBS, "LeaderCycleBack", KEY_F10);
    kbi.k[KD_LEADER_CYCLE_BACK][1] = get_config_int(KBS, "LeaderCycleBackAlt", KEY_DEL_PAD);
    
    kbi.k[KD_ROWSWAP][0] = get_config_int(KBS, "RowSwap", KEY_PLUS_PAD);
    kbi.k[KD_ROWSWAP][1] = get_config_int(KBS, "RowSwapAlt", -1);
    
    kbi.k[KD_SIDESWAP][0] = get_config_int(KBS, "SideSwap", KEY_ENTER_PAD);
    kbi.k[KD_SIDESWAP][1] = get_config_int(KBS, "SideSwapAlt", -1);
    
    kbi.k[KD_FREEZEGAME][0] = get_config_int(KBS, "Freeze", KEY_ESC);
    kbi.k[KD_FREEZEGAME][1] = get_config_int(KBS, "FreezeAlt", -1);
    
    kbi.k[KD_GOSLEEP][0] = get_config_int(KBS, "Sleep", -1);
    kbi.k[KD_GOSLEEP][1] = get_config_int(KBS, "SleepAlt", -1);
    
    kbi.k[KD_QUICKSAVE][0] = get_config_int(KBS, "QuickSave", -1);
    kbi.k[KD_QUICKSAVE][1] = get_config_int(KBS, "QuickSaveAlt", -1);
    
    kbi.k[KD_QUICKQUIT][0] = get_config_int(KBS, "QuickQuit", -1);
    kbi.k[KD_QUICKQUIT][1] = get_config_int(KBS, "QuickQuitAlt", -1);

    for (u=0;u<4;++u) {
        char p[24];
        int in = KD_WZ00 + u;
        sprintf(p, "AttackZone%d", u);
        kbi.k[in][0] = get_config_int(KBS, p, -1);
        strcat(p, "Alt");
        kbi.k[in][1] = get_config_int(KBS, p, -1);       
    }
    
    for (u=0;u<12;++u) {
        char p[24];
        int in = KD_MZ01 + u;
        sprintf(p, "MagicZone%d", u+1);
        kbi.k[in][0] = get_config_int(KBS, p, -1);
        strcat(p, "Alt");
        kbi.k[in][1] = get_config_int(KBS, p, -1);       
    }
    
    kbi.want_config = get_config_int(KBS, "Config", 0);

    VOIDRETURN();
}

void output_key_config(void) {
    int u;
    const char *KBS = "Keyboard";

    onstack("output_key_config");

    set_config_int(KBS, "Config", 0);

    set_config_int(KBS, "TurnLeft", kbi.k[KD_LEFT_TURN][0]);
    set_config_int(KBS, "TurnLeftAlt", kbi.k[KD_LEFT_TURN][1]);

    set_config_int(KBS, "TurnRight", kbi.k[KD_RIGHT_TURN][0]);
    set_config_int(KBS, "TurnRightAlt", kbi.k[KD_RIGHT_TURN][1]);

    set_config_int(KBS, "MoveForward", kbi.k[KD_FORWARD][0]);
    set_config_int(KBS, "MoveForwardAlt", kbi.k[KD_FORWARD][1]);

    set_config_int(KBS, "MoveLeft", kbi.k[KD_LEFT][0]);
    set_config_int(KBS, "MoveLeftAlt", kbi.k[KD_LEFT][1]);

    set_config_int(KBS, "MoveBack", kbi.k[KD_BACK][0]);
    set_config_int(KBS, "MoveBackAlt", kbi.k[KD_BACK][1]);

    set_config_int(KBS, "MoveRight", kbi.k[KD_RIGHT][0]);
    set_config_int(KBS, "MoveRightAlt", kbi.k[KD_RIGHT][1]);
    
    for (u=0;u<4;++u) {
        char p[12];
        int in = KD_INVENTORY1 + u;
        sprintf(p, "Inv%d", u+1);
        set_config_int(KBS, p, kbi.k[in][0]);
        strcat(p, "Alt");
        set_config_int(KBS, p, kbi.k[in][1]);       
    }
    set_config_int(KBS, "InvCycle", kbi.k[KD_INVENTORY_CYCLE][0]);
    set_config_int(KBS, "InvCycleAlt", kbi.k[KD_INVENTORY_CYCLE][1]);
    
    set_config_int(KBS, "InvCycleBack", kbi.k[KD_INVENTORY_CYCLE_BACK][0]);
    set_config_int(KBS, "InvCycleBackAlt", kbi.k[KD_INVENTORY_CYCLE_BACK][1]);
    
    set_config_int(KBS, "RowSwap", kbi.k[KD_ROWSWAP][0]);
    set_config_int(KBS, "RowSwapAlt", kbi.k[KD_ROWSWAP][1]);
    
    set_config_int(KBS, "SideSwap", kbi.k[KD_SIDESWAP][0]);
    set_config_int(KBS, "SideAlt", kbi.k[KD_SIDESWAP][1]);
    
    set_config_int(KBS, "LeaderCycle", kbi.k[KD_LEADER_CYCLE][0]);
    set_config_int(KBS, "LeaderCycleAlt", kbi.k[KD_LEADER_CYCLE][1]);
    
    set_config_int(KBS, "LeaderCycleBack", kbi.k[KD_LEADER_CYCLE_BACK][0]);
    set_config_int(KBS, "LeaderCycleBackAlt", kbi.k[KD_LEADER_CYCLE_BACK][1]);
    
    for (u=0;u<4;++u) {
        char p[24];
        int in = KD_WZ00 + u;
        sprintf(p, "AttackZone%d", u);
        set_config_int(KBS, p, kbi.k[in][0]);
        strcat(p, "Alt");
        set_config_int(KBS, p, kbi.k[in][1]);       
    }
    
    set_config_int(KBS, "Freeze", kbi.k[KD_FREEZEGAME][0]);
    set_config_int(KBS, "FreezeAlt", kbi.k[KD_FREEZEGAME][1]);
    
    set_config_int(KBS, "Sleep", kbi.k[KD_GOSLEEP][0]);
    set_config_int(KBS, "SleepAlt", kbi.k[KD_GOSLEEP][1]);
    
    set_config_int(KBS, "QuickSave", kbi.k[KD_QUICKSAVE][0]);
    set_config_int(KBS, "QuickSaveAlt", kbi.k[KD_QUICKSAVE][1]);
    
    set_config_int(KBS, "QuickQuit", kbi.k[KD_QUICKQUIT][0]);
    set_config_int(KBS, "QuickQuitAlt", kbi.k[KD_QUICKQUIT][1]);
    
    for (u=0;u<12;++u) {
        char p[24];
        int in = KD_MZ01 + u;
        sprintf(p, "MagicZone%d", u+1);
        set_config_int(KBS, p, kbi.k[in][0]);
        strcat(p, "Alt");
        set_config_int(KBS, p, kbi.k[in][1]);       
    }

    flush_config_file();

    VOIDRETURN();
}

void zonetext(int zoneid, int *zcheck, const char *str, int x, int y) {
    FONT *font = ROOT_SYSTEM_FONT;
    int w, h;
    int i_txtc;
    
    w = text_length(font, str) / 2;
    if (w < 48) w = 48;
    
    h = text_height(font) + 2;
    
    i_txtc = makecol(255, 255, 255);
    if (!Disp_Zone && *zcheck == 0) {
        if (mouse_x > x - w && mouse_x < x + w) {
            if (mouse_y > y - 2 && mouse_y < y + h) {
                *zcheck = zoneid;
                i_txtc = makecol(0, 255, 0);
            }
        }
    }
    
    if (Disp_Zone == zoneid) {
        i_txtc = makecol(255, 255, 0);
    }

    textout_centre_ex(soft_buffer, font, str, x, y, i_txtc, -1);
}

void gui_key_config(void) {
    const int pagelen = 12;
    
    const char *mdirnames[] = {
        "MOVE FORWARD",
        "MOVE LEFT",
        "MOVE BACK",
        "MOVE RIGHT",
        "TURN LEFT",
        "TURN RIGHT",
        "INVENTORY 1",
        "INVENTORY 2",
        "INVENTORY 3",
        "INVENTORY 4",
        "NEXT INVENTORY",
        "PREV INVENTORY",
        
        "SWAP LEADER ROW",
        "SWAP LEADER SIDE",
        "NEXT LEADER",
        "PREV LEADER",
        "CHAR 1/PASS",
        "CHAR 2/METHOD 1",
        "CHAR 3/METHOD 2",
        "CHAR 4/METHOD 3",
        "FREEZE GAME",
        "GO TO SLEEP",
        "QUICK SAVE",
        "EXIT GAME",
        
        "MAGIC 1",
        "MAGIC 2",
        "MAGIC 3",
        "MAGIC 4",
        "MAGIC BACK",
        "MAGIC CAST",
        "RUNE 1",
        "RUNE 2",
        "RUNE 3",
        "RUNE 4",
        "RUNE 5",
        "RUNE 6"
    };
    
    int updateok = 0;
    int i_keymode = 0;
    int in_config = 1;
    int nzone = 999;
    int quitzone = 9999;
    int fupdate = 0;
    char s_kpn_buf[80];
    int page = 0;
    
    // override things that
    // might otherwise be different
    FONT *font = ROOT_SYSTEM_FONT;
    int sw = 640;

    onstack("gui_key_config");
    
    kbi.want_config = 0;
    Disp_Zone = 0;

    while (in_config) {
        int z = 0;
        
        if (fupdate) {
            updateok = 1;
            fupdate = 0;
        } else
            updateok = systemtimer();

        if (updateok) {
            int ic;
            
            clear_to_color(soft_buffer, makecol(0,0,72));
            
            textout_centre_ex(soft_buffer, font, "DSB KEYBOARD CONFIGURATION",
                sw / 2, 16, makecol(255, 255, 255), -1);

            if (Disp_Zone) {
                int zn = Disp_Zone - 1;
                if (zn >= 100) zn -= 100;
                textout_ex(soft_buffer, font,
                    "PRESS THE KEY THAT YOU WANT TO USE FOR:",
                    8, 48, makecol(255, 255, 255), -1);
                    
                textout_centre_ex(soft_buffer, font, mdirnames[zn],
                    sw / 2, 62, makecol(255, 255, 255), -1);
                    
            } else {
                textout_ex(soft_buffer, font,
                    "LEFT CLICK ON THE KEY YOU WISH TO REDEFINE,",
                    24, 48, makecol(255, 255, 255), -1);

                textout_ex(soft_buffer, font,
                    "OR RIGHT CLICK TO UNDEFINE IT.",
                    24, 62, makecol(255, 255, 255), -1);
            }
                
            for (ic=0;ic<pagelen;++ic) {
                int poic = pagelen*page + ic;
                int yv = 96+(26*ic);
                const char *s_kpn;
                
                textout_ex(soft_buffer, font, mdirnames[poic],
                    32, yv, makecol(255, 255, 255), -1);
                    
                if (kbi.k[poic][0] != -1) {
                    int i = 0;
                    const char *raw_scn = scancode_to_name(kbi.k[poic][0]);
                    while (raw_scn[i] != '\0') {
                        s_kpn_buf[i] = toupper(raw_scn[i]);
                        ++i;
                    }
                    s_kpn_buf[i] = '\0';
                    s_kpn = s_kpn_buf;
                } else
                    s_kpn = "UNDEFINED";
                zonetext(poic+1, &z, s_kpn, 320, yv);
                
                if (kbi.k[poic][1] != -1) {
                    int i = 0;
                    const char *raw_scn = scancode_to_name(kbi.k[poic][1]);
                    while (raw_scn[i] != '\0') {
                        s_kpn_buf[i] = toupper(raw_scn[i]);
                        ++i;
                    }
                    s_kpn_buf[i] = '\0';
                    s_kpn = s_kpn_buf;
                } else
                    s_kpn = "UNDEFINED";
                zonetext(poic+101, &z, s_kpn, 480, yv);
            }
            

            zonetext(nzone, &z, "NEXT PAGE", sw / 2, 420);
            zonetext(quitzone, &z, "CLICK HERE WHEN FINISHED", sw / 2, 448);
            
            if (!Disp_Zone)
                draw_mouseptr(soft_buffer, NULL, 0);

            scfix(1);
            updateok = 0;
        } else {
            Sleep(1);
        }

        if (gd.stored_rcv == 255) {
            goto QUIT_OUT_RIGHT_NOW;
            break;
        }
        
        if (Disp_Zone) {
            int i_key, code;
            int cn;
            int alt = 0;
            int code_get = 0;
            
            dsb_clear_keybuf();
            while (!code_get) {
                if (keypressed()) {
                    i_key = readkey();
                    code = (i_key & 0xFF00) >> 8;
                    code_get = 1;
                } else {
                    int ividx;
                    int vkeys[] = { KEY_ALT, KEY_ALTGR, KEY_LCONTROL,
                        KEY_RCONTROL, KEY_LSHIFT, KEY_RSHIFT };
                    for(ividx=0;ividx<6;++ividx) {
                        if (key[vkeys[ividx]]) {
                            code = vkeys[ividx];
                            code_get = 1;
                            break;
                        }
                    }
                }
                Sleep(2);
            }
            
            cn = Disp_Zone - 1;
            if (cn >= 100) {
                cn -= 100;
                alt = 1;
            }
            
            kbi.k[cn][alt] = code;
            Disp_Zone = 0;
            while(mouse_b) rest(10);
        }
        
        if (!Disp_Zone && (mouse_b & 1)) {
            if (z == quitzone) {
                in_config = 0;
                output_key_config();
                dsb_clear_keybuf();
                break;
            } else if (z == nzone) {
                page++;
                if (page == 3) page = 0;
                while(mouse_b) rest(10);
            } else if (z) {
                Disp_Zone = z;
                fupdate = 1;
            }
        }
        
        if (!Disp_Zone && (mouse_b & 2)) {
            if (z < nzone) {
                int cn;
                int alt = 0;
                
                cn = z - 1;
                if (cn >= 100) {
                    cn -= 100;
                    alt = 1;
                }
                
                kbi.k[cn][alt] = -1;
            }
        }
    }
    
    while(mouse_b) rest(10);
    
    QUIT_OUT_RIGHT_NOW:
    VOIDRETURN();
}

void gui_frozen_screen_menu(void) {
    FONT *font = ROOT_SYSTEM_FONT;
    int in_config = 1;
    int fupdate = 1;
    int updateok = 1;
    int quitzone = 9999;
    int base = 128;
    int sz = 32;
    int mousedown = 0;
    // override
    int sw = 640;
    int exportable;
    
    onstack("gui_frozen_screen_menu");
    
    exportable = is_game_exportable();

    while (in_config) {
        int z = 0;
        
        if (fupdate) {
            updateok = 1;
            fupdate = 0;
        } else
            updateok = systemtimer();

        if (updateok) { 
            clear_to_color(soft_buffer, makecol(0,0,72));
            
            textout_centre_ex(soft_buffer, font, "DSB EXTRA OPTIONS MENU",
                sw / 2, 16, makecol(255, 255, 255), -1);
                
            zonetext(1, &z, "CONFIGURE KEYBOARD", sw / 2, base);

            if (dynamicswitch) {
                if (windowed)
                    zonetext(3, &z, "GO FULLSCREEN", sw/ 2, base+sz*2);
                else
                    zonetext(3, &z, "GO WINDOWED", sw / 2, base+sz*2);
            }
            
            if (exportable) {
                zonetext(4, &z, "EXPORT TO DUNGEON", sw / 2, base+sz*4);
            }
                
            zonetext(quitzone, &z, "CLICK HERE TO RETURN", sw / 2, 432);
            
            draw_mouseptr(soft_buffer, NULL, 0);

            scfix(1);
            updateok = 0;
            
            if (gd.stored_rcv == 255) {
                in_config = 0;
            }
            if (mouse_b & 1) {
                if (!mousedown) {
                    mousedown = 1;
                    if (z == quitzone)
                        in_config = 0;
                    else {
                        switch (z) {
                            case 1:
                                while(mouse_b) rest(5);
                                gui_key_config();
                            break;
                            
                            case 3:
                                if (dynamicswitch) {
                                    while(mouse_b) rest(5);
                                    if (windowed) {
                                        set_gfx_mode(GFX_DIRECTX, XRES, YRES, 0, 0);
                                        set_display_switch_mode(SWITCH_AMNESIA); 
                                        windowed = 0; 
                                    } else {
                                        set_gfx_mode(GFX_AUTODETECT_WINDOWED, XRES, YRES, 0, 0);
                                        set_display_switch_mode(SWITCH_BACKGROUND);
                                        windowed = 1;
                                    } 
                                    set_config_int("Settings", "Windowed", windowed);
                                    flush_config_file();  
                                } 
                            break;
                            
                            case 4:
                                if (exportable) {
                                    store_exported_character_data();
                                    gd.stored_rcv = -4;
                                    in_config = 0;
                                    rest(1);
                                }
                            break;
                        }
                    }
                }
            } else
                mousedown = 0;
        } else
            Sleep(1);
    } 
    
    while (mouse_b) rest(5); 
    
    VOIDRETURN();
    
}
