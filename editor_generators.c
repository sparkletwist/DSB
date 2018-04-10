#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <commctrl.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "editor.h"
#include "E_res.h"
#include "objects.h"
#include "champions.h"
#include "gproto.h"
#include "monster.h"
#include "global_data.h"
#include "uproto.h"
#include "compile.h"
#include "lua_objects.h"
#include "arch.h"
#include "editor_hwndptr.h"
#include "editor_gui.h"
#include "editor_menu.h"
#include "editor_shared.h"

extern struct global_data gd;
extern struct editor_global edg;
extern struct inst *oinst[NUM_INST];
extern struct dungeon_level *dun;
extern lua_State *LUA;

extern HWND sys_hwnd;
extern FILE *errorlog;

INT_PTR CALLBACK ESB_monstergen_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo;
    
    onstack("ESB_monstergen_proc");

    switch(message) {
        case WM_INITDIALOG: {
            int take_n;
            int rgd;
            int hp;
            int moncount;
            
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            luastacksize(11);
            lua_getglobal(LUA, "__ed_monstergen_setup");
            lua_pushinteger(LUA, xinfo->inst);
            if (lua_pcall(LUA, 1, 9, 0))
                bomb_lua_error(NULL);
            SetDlgItemText(hwnd, ESB_GENMONTXT, lua_tostring(LUA, -9));
            if (lua_isnumber(LUA, -8)) {
                int n = lua_tointeger(LUA, -8);
                if (n == 2)
                    SendDlgItemMessage(hwnd, ESB_MDEFAULT, BM_SETCHECK, BST_CHECKED, 0);
                else
                    SendDlgItemMessage(hwnd, ESB_MSILENT, BM_SETCHECK, BST_CHECKED, 0);
            } else {
                const char *sel = lua_tostring(LUA, -8);
                SetDlgItemText(hwnd, ESB_MONSOUNDEDIT, sel);
                SendDlgItemMessage(hwnd, ESB_MSPECIFIC, BM_SETCHECK, BST_CHECKED, 0);
            }
            
            for(take_n=0;take_n<4;take_n++) {
                int iv = lua_isnumber(LUA, -7 + take_n);
                if (iv) {
                    HWND wv = GetDlgItem(hwnd, ESB_GEN1 + take_n);
                    EnableWindow(wv, FALSE);
                } else {
                    int b = lua_toboolean(LUA, -7 + take_n);
                    if (b) {
                        SendDlgItemMessage(hwnd, ESB_GEN1 + take_n,
                            BM_SETCHECK, BST_CHECKED, 0);
                    }
                }
            }
            
            rgd = lua_tointeger(LUA, -3);
            SetDlgItemInt(hwnd, ESB_MONRECHARGEIN, rgd, FALSE);
            hp = lua_tointeger(LUA, -2);
            if (hp == 0) {
                HWND hpcap = GetDlgItem(hwnd, ESB_GENHPLBL);
                HWND hpfield = GetDlgItem(hwnd, ESB_EDITMONHP);
                EnableWindow(hpcap, FALSE);
                EnableWindow(hpfield, FALSE);
                SendDlgItemMessage(hwnd, ESB_MGENDEFAULT, BM_SETCHECK, BST_CHECKED, 0);
            } else
                SetDlgItemInt(hwnd, ESB_EDITMONHP, hp, FALSE);
                
            moncount = lua_tointeger(LUA, -1);
            SetDlgItemInt(hwnd, ESB_EDITMONLIMIT, moncount, FALSE);
            if (moncount == 0) {
                HWND dbox = GetDlgItem(hwnd, ESB_EDITMONLIMIT);
                EnableWindow(dbox, FALSE);
            } else {
                HWND chk_w = GetDlgItem(hwnd, ESB_MONLIMITCHECK);
                SendMessage(chk_w, BM_SETCHECK, BST_CHECKED, 0);
            }
  
            lua_pop(LUA, 9);
        } break;
            
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDOK: {
                    int checks = 0;
                    int moncount = 0;
                    int chkb;
                    char *mon_string;
                    int archsound = SendDlgItemMessage(hwnd, ESB_MDEFAULT, BM_GETCHECK, 0, 0);
                    int ssound = SendDlgItemMessage(hwnd, ESB_MSPECIFIC, BM_GETCHECK, 0, 0);
                    char dtext[100] = { 0 };
                    int regenv;

                    xinfo = get_hwnd(hwnd);
                                        
                    mon_string = dsbfastalloc(400);
                    GetDlgItemText(hwnd, ESB_GENMONTXT, mon_string, 400);
                    if (*mon_string == '\0') {
                        MessageBox(hwnd, "You must specify a monster to generate.",
                            "Generator Error", MB_ICONEXCLAMATION);
                        RETURN(TRUE);
                    }
                    
                    for (chkb=0;chkb<4;chkb++) {
                        int cid = ESB_GEN1 + chkb;
                        if (SendDlgItemMessage(hwnd, cid, BM_GETCHECK, 0, 0))
                            checks++;  
                    }
                    if (!checks) {
                        MessageBox(hwnd, "You must specify a number of monsters to generate.",
                            "Generator Error", MB_ICONEXCLAMATION);
                        RETURN(TRUE);
                    }
                    if (ssound) {
                        GetDlgItemText(hwnd, ESB_MONSOUNDEDIT, dtext, sizeof(dtext));
                        if (*dtext == '\0') {
                            MessageBox(hwnd, "You must specify a sound name.", "Sound Error", MB_ICONEXCLAMATION);
                            RETURN(TRUE);
                        }
                    }
    
                    luastacksize(12);
                    lua_getglobal(LUA, "__ed_monstergen_store");
                    lua_pushinteger(LUA, xinfo->inst);
                    lua_pushstring(LUA, mon_string);
                    if (ssound)
                        lua_pushstring(LUA, dtext);
                    else if (archsound)
                        lua_pushinteger(LUA, 2);
                    else
                        lua_pushinteger(LUA, 1);
                    for (chkb=0;chkb<4;chkb++) {
                        int cid = ESB_GEN1 + chkb;
                        if (SendDlgItemMessage(hwnd, cid, BM_GETCHECK, 0, 0))
                            lua_pushboolean(LUA, 1);
                        else
                            lua_pushboolean(LUA, 0);  
                    }
                    regenv = GetDlgItemInt(hwnd, ESB_MONRECHARGEIN, NULL, FALSE);
                    lua_pushinteger(LUA, regenv);
                    if (SendDlgItemMessage(hwnd, ESB_MGENDEFAULT, BM_GETCHECK, 0, 0)) 
                        lua_pushinteger(LUA, 0);
                    else {
                        int hp = GetDlgItemInt(hwnd, ESB_EDITMONHP, NULL, FALSE);
                        lua_pushinteger(LUA, hp);
                    }
                    
                    if (SendDlgItemMessage(hwnd, ESB_MONLIMITCHECK, BM_GETCHECK, 0, 0)) {
                        moncount = GetDlgItemInt(hwnd, ESB_EDITMONLIMIT, NULL, FALSE);
                    }
                    lua_pushinteger(LUA, moncount);
                    
                    if (lua_pcall(LUA, 10, 0, 0))
                        bomb_lua_error(NULL);                  
                    
                    SendMessage(hwnd, WM_CLOSE, 0, 0);
                } break;
                
                case IDCANCEL: {
                    SendMessage(hwnd, WM_CLOSE, 0, 0);
                } break;
                
                case ESB_CHOOSEMON: {
                    xinfo = get_hwnd(hwnd);
                    edg.op_inst = xinfo->inst;
                    edg.op_arch = 5;
                    ESB_modaldb(ESB_ARCHPICKER, hwnd, ESB_archpick_proc, 0); 
                    if (edg.op_arch != 0xFFFFFFFF) {
                        int chkb;
                        int checks = 0;
                        struct obj_arch *p_arch = Arch(edg.op_arch);
                        SetDlgItemText(hwnd, ESB_GENMONTXT, p_arch->luaname); 
                        for (chkb=0;chkb<4;chkb++) {
                            int cid = ESB_GEN1 + chkb;
                            HWND chk_w = GetDlgItem(hwnd, cid);
                            if (chkb > 0 && p_arch->msize >= 4) {
                                SendMessage(chk_w, BM_SETCHECK, BST_UNCHECKED, 0);
                                EnableWindow(chk_w, FALSE);
                            } else if (chkb > 1 && p_arch->msize >= 2) {
                                SendMessage(chk_w, BM_SETCHECK, BST_UNCHECKED, 0);
                                EnableWindow(chk_w, FALSE);
                            } else {
                                EnableWindow(chk_w, TRUE);
                                if (SendMessage(chk_w, BM_GETCHECK, 0, 0)) 
                                    checks++;
                            }
                        }
                        if (!checks)
                            SendDlgItemMessage(hwnd, ESB_GEN1, BM_SETCHECK, BST_CHECKED, 0);              
                    }                   
                } break;
                
                case ESB_MONLIMITCHECK: {
                    int lchecked = SendDlgItemMessage(hwnd, ESB_MONLIMITCHECK, BM_GETCHECK, 0, 0);
                    HWND dbox = GetDlgItem(hwnd, ESB_EDITMONLIMIT);
                    if (lchecked) {
                        EnableWindow(dbox, TRUE);
                    } else {
                        EnableWindow(dbox, FALSE);
                    }
                } break;
                
                case ESB_MGENDEFAULT: {
                    HWND ckb = GetDlgItem(hwnd, ESB_MGENDEFAULT);
                    HWND wi_1 = GetDlgItem(hwnd, ESB_GENHPLBL);
                    HWND wi_2 = GetDlgItem(hwnd, ESB_EDITMONHP);
                    int chk = SendMessage(ckb, BM_GETCHECK, 0, 0); 
                    if (chk) {
                        EnableWindow(wi_1, FALSE);
                        EnableWindow(wi_2, FALSE);
                    } else {
                        EnableWindow(wi_1, TRUE);
                        EnableWindow(wi_2, TRUE);
                    }                  
                } break;
            }
        } break;

        case WM_CLOSE: {
            edg.op_inst = 0;
            ESB_go_up_one(hwnd);
        } break;
        
        case WM_DESTROY: {
            del_hwnd(hwnd);            
        }

        default:
            RETURN(FALSE);
    }

    RETURN(TRUE);
}

INT_PTR CALLBACK ESB_shooter_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    int ni;
    struct ext_hwnd_info *xinfo;
    
    onstack("ESB_shooter_proc");

    switch(message) { 
        case WM_COMMAND: {
            int lw = LOWORD(wParam);
            if (lw == IDCANCEL)
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            else if (lw == IDOK) {
                char *dtext = dsbfastalloc(250);
                char *shoottext = dsbfastalloc(150);
                int ssound = SendDlgItemMessage(hwnd, ESB_SHSPSND, BM_GETCHECK, 0, 0);
                int sshoot = SendDlgItemMessage(hwnd, ESB_SHOOTNEW, BM_GETCHECK, 0, 0);
                int shoot_square = SendDlgItemMessage(hwnd, ESB_SHOOTTILE, BM_GETCHECK, 0, 0);
                int proj_power, proj_drag, proj_regen;
                int proj_sideinfo = 0;
                
                if (sshoot) {
                    GetDlgItemText(hwnd, ESB_TYPESHOOTTHING, shoottext, 150);
                    if (*shoottext == '\0') {
                        MessageBox(hwnd, "You must specify an arch to spawn and shoot.", "Shooter Error", MB_ICONEXCLAMATION);
                        RETURN(TRUE);
                    }
                }
                
                proj_power = GetDlgItemInt(hwnd, ESB_SHPOWER, NULL, FALSE);
                proj_drag = GetDlgItemInt(hwnd, ESB_SHDRAG, NULL, FALSE);
                proj_regen = GetDlgItemInt(hwnd, ESB_SRECHARGEIN, NULL, FALSE);
                if (proj_power < 1) {
                    MessageBox(hwnd, "The shooter's power must be 1 or greater.", "Projectile Error", MB_ICONEXCLAMATION);
                    RETURN(TRUE);
                }
                
                if (proj_drag > proj_power) {
                    MessageBox(hwnd, "The shooter's drag must be less than the shooter's power.", "Projectile Error", MB_ICONEXCLAMATION);
                    RETURN(TRUE);
                }
                
                if (ssound) {
                    GetDlgItemText(hwnd, ESB_TYPESHSPSND, dtext, 250);
                    if (*dtext == '\0') {
                        MessageBox(hwnd, "You must specify a sound name.", "Sound Error", MB_ICONEXCLAMATION);
                        RETURN(TRUE);
                    }
                }
                
                for (proj_sideinfo=0;proj_sideinfo<4;proj_sideinfo++) {
                    if (SendDlgItemMessage(hwnd, ESB_SIDERAND + proj_sideinfo, BM_GETCHECK, 0, 0))
                        break;
                }
                
                xinfo = get_hwnd(hwnd);
                                
                luastacksize(8);
                lua_getglobal(LUA, "__ed_shooter_store");
                lua_pushinteger(LUA, xinfo->inst);
                lua_pushinteger(LUA, proj_power);
                lua_pushinteger(LUA, proj_drag);
                lua_pushinteger(LUA, proj_regen); 
                lua_pushinteger(LUA, proj_sideinfo);
                if (ssound)
                    lua_pushstring(LUA, dtext);
                else
                    lua_pushnil(LUA);
                    
                if (shoot_square)
                    lua_pushinteger(LUA, 1);
                else if (sshoot)
                    lua_pushstring(LUA, shoottext);
                else
                    lua_pushnil(LUA);
                
                if (lua_pcall(LUA, 7, 0, 0))
                    bomb_lua_error(NULL);                  
                
                SendMessage(hwnd, WM_CLOSE, 0, 0);                
                                  
            }
            
            switch (lw) {
                case ESB_SHOOTSTOREDOBJ: {
                    HWND box = GetDlgItem(hwnd, ESB_TYPESHOOTTHING);
                    HWND btn = GetDlgItem(hwnd, ESB_CHOOSESHOOTTHING);
                    EnableWindow(box, FALSE);
                    EnableWindow(btn, FALSE);
                    SendDlgItemMessage(hwnd, ESB_SHOOTSTOREDOBJ, BM_SETCHECK, BST_CHECKED, 0);
                    SendDlgItemMessage(hwnd, ESB_SHOOTTILE, BM_SETCHECK, BST_UNCHECKED, 0); 
                    SendDlgItemMessage(hwnd, ESB_SHOOTNEW, BM_SETCHECK, BST_UNCHECKED, 0); 
                } break;
                
                case ESB_SHOOTNEW: {
                    HWND box = GetDlgItem(hwnd, ESB_TYPESHOOTTHING);
                    HWND btn = GetDlgItem(hwnd, ESB_CHOOSESHOOTTHING);
                    EnableWindow(box, TRUE);
                    EnableWindow(btn, TRUE);
                    SendDlgItemMessage(hwnd, ESB_SHOOTSTOREDOBJ, BM_SETCHECK, BST_UNCHECKED, 0);
                    SendDlgItemMessage(hwnd, ESB_SHOOTTILE, BM_SETCHECK, BST_UNCHECKED, 0); 
                    SendDlgItemMessage(hwnd, ESB_SHOOTNEW, BM_SETCHECK, BST_CHECKED, 0); 
                } break;
                
                case ESB_SHOOTTILE: {
                    HWND box = GetDlgItem(hwnd, ESB_TYPESHOOTTHING);
                    HWND btn = GetDlgItem(hwnd, ESB_CHOOSESHOOTTHING);
                    EnableWindow(box, FALSE);
                    EnableWindow(btn, FALSE);
                    SendDlgItemMessage(hwnd, ESB_SHOOTSTOREDOBJ, BM_SETCHECK, BST_UNCHECKED, 0);
                    SendDlgItemMessage(hwnd, ESB_SHOOTTILE, BM_SETCHECK, BST_CHECKED, 0); 
                    SendDlgItemMessage(hwnd, ESB_SHOOTNEW, BM_SETCHECK, BST_UNCHECKED, 0); 
                } break;
                
                case ESB_CHOOSESHOOTTHING: {
                    xinfo = get_hwnd(hwnd);
                    edg.op_inst = xinfo->inst;
                    edg.op_arch = 4;
                    ESB_modaldb(ESB_ARCHPICKER, hwnd, ESB_archpick_proc, 0); 
                    if (edg.op_arch != 0xFFFFFFFF) {       
                        struct obj_arch *p_arch = Arch(edg.op_arch);
                        SetDlgItemText(hwnd, ESB_TYPESHOOTTHING, p_arch->luaname);
                    }             
                } break;
                
                case ESB_SHSILENT: {
                    SendDlgItemMessage(hwnd, ESB_SHSILENT, BM_SETCHECK, BST_CHECKED, 0); 
                    SendDlgItemMessage(hwnd, ESB_SHSPSND, BM_SETCHECK, BST_UNCHECKED, 0); 
                } break;
                
                case ESB_SHSPSND: {
                    SendDlgItemMessage(hwnd, ESB_SHSILENT, BM_SETCHECK, BST_UNCHECKED, 0); 
                    SendDlgItemMessage(hwnd, ESB_SHSPSND, BM_SETCHECK, BST_CHECKED, 0); 
                } break;
                
                case ESB_SIDERAND:
                case ESB_LEFTFORCED:
                case ESB_RIGHTFORCED:
                case ESB_DOUBLESHOOT: {
                    for (ni=ESB_SIDERAND;ni<=ESB_DOUBLESHOOT;++ni) {
                        if (lw == ni)
                            SendDlgItemMessage(hwnd, ni, BM_SETCHECK, BST_CHECKED, 0); 
                        else
                            SendDlgItemMessage(hwnd, ni, BM_SETCHECK, BST_UNCHECKED, 0); 
                    }
                } break;
            }
        } break; 
              
        case WM_INITDIALOG: {
            const char *shoots;
            const char *sound;
            int dbl;
            int side;
            int ppower = 0;
            int proj_power = 1;
            int proj_drag = 0;
            int proj_recharge = 0;
            int shoot_loc;
            
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            
            shoot_loc = ex_exvar(xinfo->inst, "shoot_square", &side); 
            
            if (shoot_loc) {
                PostMessage(hwnd, WM_COMMAND, ESB_SHOOTTILE, 0);
            } else {
                shoots = exs_exvar(xinfo->inst, "shoots");
                if (shoots) {
                    SetDlgItemText(hwnd, ESB_TYPESHOOTTHING, shoots);
                    dsbfree(shoots);
                    PostMessage(hwnd, WM_COMMAND, ESB_SHOOTNEW, 0);
                } else {
                    PostMessage(hwnd, WM_COMMAND, ESB_SHOOTSTOREDOBJ, 0);
                }
            }
            
            sound = exs_exvar(xinfo->inst, "sound");
            if (sound && *sound != '\0') {
                SendDlgItemMessage(hwnd, ESB_SHSPSND, BM_SETCHECK, BST_CHECKED, 0);
                SetDlgItemText(hwnd, ESB_TYPESHSPSND, sound);
                dsbfree(sound);
            } else {
                SendDlgItemMessage(hwnd, ESB_SHSILENT, BM_SETCHECK, BST_CHECKED, 0); 
                dsbfree(sound);
            } 
            
            dbl = ex_exvar(xinfo->inst, "double", &side);
            if (dbl) {
                SendDlgItemMessage(hwnd, ESB_DOUBLESHOOT, BM_SETCHECK, BST_CHECKED, 0);     
            } else {
                int fside = ex_exvar(xinfo->inst, "force_side", &side);
                if (fside) {
                    if (side == 1)
                        SendDlgItemMessage(hwnd, ESB_LEFTFORCED, BM_SETCHECK, BST_CHECKED, 0); 
                    else
                        SendDlgItemMessage(hwnd, ESB_RIGHTFORCED, BM_SETCHECK, BST_CHECKED, 0); 
                } else
                    SendDlgItemMessage(hwnd, ESB_SIDERAND, BM_SETCHECK, BST_CHECKED, 0);       
            }
            
            ex_exvar(xinfo->inst, "power", &ppower);
            if (ppower > 0) proj_power = ppower;
            
            ex_exvar(xinfo->inst, "delta", &proj_drag);
            ex_exvar(xinfo->inst, "regen", &proj_recharge);
            SetDlgItemInt(hwnd, ESB_SHPOWER, proj_power, FALSE);
            SetDlgItemInt(hwnd, ESB_SHDRAG, proj_drag, FALSE);
            SetDlgItemInt(hwnd, ESB_SRECHARGEIN, proj_recharge, FALSE);
              
        } break;

        case WM_CLOSE: {
            ESB_go_up_one(hwnd);
        } break;
        
        case WM_DESTROY: {
            del_hwnd(hwnd);
        } break;
        
        default:
            RETURN(FALSE);
    }
    RETURN(TRUE);
}

INT_PTR CALLBACK ESB_function_caller_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    int ni;
    struct ext_hwnd_info *xinfo;
    
    onstack("ESB_function_caller_proc");

    switch(message) { 
        case WM_COMMAND: {
            int lw = LOWORD(wParam);
            if (lw == IDCANCEL)
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            else if (lw == IDOK) {
                char retstr[120];
                
                xinfo = get_hwnd(hwnd);
                
                luastacksize(6);
                lua_getglobal(LUA, "__ed_func_caller_store");   
                lua_pushinteger(LUA, xinfo->inst);           
                GetDlgItemText(hwnd, ESB_EDFUNC_A, retstr, 120);
                if (retstr[0] == '\0') lua_pushnil(LUA);
                else lua_pushstring(LUA, retstr);
                GetDlgItemText(hwnd, ESB_EDFUNC_D, retstr, 120);
                if (retstr[0] == '\0') lua_pushnil(LUA);
                else lua_pushstring(LUA, retstr); 
                GetDlgItemText(hwnd, ESB_EDFUNC_T, retstr, 120);
                if (retstr[0] == '\0') lua_pushnil(LUA);
                else lua_pushstring(LUA, retstr); 
                GetDlgItemText(hwnd, ESB_EDFUNC_N, retstr, 120);
                if (retstr[0] == '\0') lua_pushnil(LUA);
                else lua_pushstring(LUA, retstr);  
                lc_call_topstack(5, "ed_func_caller_store");

                SendMessage(hwnd, WM_CLOSE, 0, 0);                                
            }
            
        } break; 
              
        case WM_INITDIALOG: {
            char *dstr = NULL;
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            
            dstr = exs_exvar(xinfo->inst, "m_a");
            if (dstr) {
                SetDlgItemText(hwnd, ESB_EDFUNC_A, dstr);
                dsbfree(dstr);
            }
            dstr = exs_exvar(xinfo->inst, "m_d");
            if (dstr) {
                SetDlgItemText(hwnd, ESB_EDFUNC_D, dstr);
                dsbfree(dstr);
            }
            dstr = exs_exvar(xinfo->inst, "m_t");
            if (dstr) {
                SetDlgItemText(hwnd, ESB_EDFUNC_T, dstr);
                dsbfree(dstr);
            }
            dstr = exs_exvar(xinfo->inst, "m_n");
            if (dstr) {
                SetDlgItemText(hwnd, ESB_EDFUNC_N, dstr);
                dsbfree(dstr);
            }

        } break;

        case WM_CLOSE: {
            ESB_go_up_one(hwnd);
        } break;
        
        case WM_DESTROY: {
            del_hwnd(hwnd);
        } break;
        
        default:
            RETURN(FALSE);
    }
    RETURN(TRUE);
}

