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

#define CHECKCOORDVAL(C, N) \
    if (lua_isnumber(LUA, N))\
        inst_coords[C] = lua_tointeger(LUA, N)

extern struct global_data gd;
extern struct editor_global edg;
extern struct inst *oinst[NUM_INST];
extern struct dungeon_level *dun;
extern lua_State *LUA;

extern HWND sys_hwnd;
extern FILE *errorlog;

void item_action_full_disable(HWND hwnd) {
    int i;
    for(i=1941;i<1960;++i) {
        HWND d = GetDlgItem(hwnd, i);
        EnableWindow(d, FALSE);
    }    
}
void item_action_msg_toggle(HWND hwnd, int v) {
    int i;
    for(i=1962;i<1970;++i) {
        HWND d = GetDlgItem(hwnd, i);
        EnableWindow(d, v);
    }    
}

INT_PTR CALLBACK ESB_item_action_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo;
    
    onstack("ESB_item_action_proc");

    switch(message) {
        case WM_INITDIALOG: {
            int ignore_cap = 0;
            struct inst *p_inst;
            int nsel = 0;
            int movesel = 0;
            int cs = 0;
            int inst_coords[4];
            int cc;
            int line = 2;
            
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            item_action_full_disable(hwnd);
            item_action_msg_toggle(hwnd, FALSE);
            
            p_inst = oinst[xinfo->inst];
            inst_coords[0] = p_inst->level;
            inst_coords[1] = p_inst->x;
            inst_coords[2] = p_inst->y;
            inst_coords[3] = 4;
            if (inst_coords[0] < 0) {
                inst_coords[0] = 0;
                inst_coords[1] = 0;
                inst_coords[2] = 0;
            }
            
            SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEMSG, CB_ADDSTRING,
                0, (int)"M_ACTIVATE");
            SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEMSG, CB_ADDSTRING,
                0, (int)"M_DEACTIVATE");
            SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEMSG, CB_ADDSTRING,
                0, (int)"M_TOGGLE");
            SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEMSG, CB_ADDSTRING,
                0, (int)"M_NEXTTICK");
            SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEMSG, CB_ADDSTRING,
                0, (int)"M_RESET");
            SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEMSG, CB_ADDSTRING,
                0, (int)"M_CLEANUP");
            SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEMSG, CB_ADDSTRING,
                0, (int)"M_EXPIRE");
            SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEMSG, CB_ADDSTRING,
                0, (int)"M_DESTROY");
            edg.op_hwnd = hwnd;
            edg.op_b_id = ESB_IAP_CHOOSEMSG;
            edg.op_winmsg = CB_ADDSTRING;                  
            lc_parm_int("__ed_fill_userdefined_msgs", 0);
            
            ignore_cap = ex_exvar(xinfo->inst, "ignore_capacity", NULL);
            if (ignore_cap)
                SendDlgItemMessage(hwnd, ESB_IAPM_NOCAP, BM_SETCHECK, BST_CHECKED, 0);
            
            luastacksize(16);
            lua_getglobal(LUA, "__ed_item_action_setup");
            lua_pushinteger(LUA, xinfo->inst);
            if (lua_pcall(LUA, 1, 14, 0))
                bomb_lua_error(NULL);
                
            nsel = lua_tointeger(LUA, -14);
            if (nsel < 0 || nsel > 5) nsel = 0;
            SendMessage(hwnd, WM_COMMAND, nsel + 1903, 0);
            if (nsel == 3 || nsel == 4) {
                const char *dstr = lua_tostring(LUA, -13);
                if (dstr && dstr != "") {
                    int targ = ESB_IA_NEWARCHF;
                    if (nsel == 4) targ = ESB_IA_ALLARCHF;
                    SetDlgItemText(hwnd, targ, dstr);
                }
            }
                
            if (lua_toboolean(LUA, -12)) {
                const char *strid = lua_tostring(LUA, -11);
                if (strid && strid != "") {
                    int cfind = SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEMSG,
                        CB_FINDSTRINGEXACT, -1, (int)strid);
                    if (cfind != CB_ERR)
                        cs = cfind;
                }
                SendDlgItemMessage(hwnd, ESB_IAM_SENDMSG, BM_SETCHECK, BST_CHECKED, 0); 
                SendMessage(hwnd, WM_COMMAND, ESB_IAM_SENDMSG, 0);   
            }
            SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEMSG, CB_SETCURSEL, cs, 0);
            if (lua_toboolean(LUA, -10))
                SendDlgItemMessage(hwnd, ESB_IAP_TOTLOP, BM_SETCHECK, BST_CHECKED, 0); 
            else
                SendDlgItemMessage(hwnd, ESB_IAP_TOACTION, BM_SETCHECK, BST_CHECKED, 0); 
                
            movesel = lua_tointeger(LUA, -9);
            if (movesel < 0 || movesel > 5) movesel = 0;
            SendMessage(hwnd, WM_COMMAND, movesel + 1921, 0);
            
            SendDlgItemMessage(hwnd, ESB_IAP_TILEPICK, CB_ADDSTRING,
                0, (int)"North/Northwest");
            SendDlgItemMessage(hwnd, ESB_IAP_TILEPICK, CB_ADDSTRING,
                0, (int)"East/Northeast");
            SendDlgItemMessage(hwnd, ESB_IAP_TILEPICK, CB_ADDSTRING,
                0, (int)"South/Southeast");
            SendDlgItemMessage(hwnd, ESB_IAP_TILEPICK, CB_ADDSTRING,
                0, (int)"West/Southwest");
            SendDlgItemMessage(hwnd, ESB_IAP_TILEPICK, CB_ADDSTRING,
                0, (int)"Center");
            
            CHECKCOORDVAL(0, -8);
            CHECKCOORDVAL(1, -7);
            CHECKCOORDVAL(2, -6);
            CHECKCOORDVAL(3, -5); 
            SetDlgItemInt(hwnd, ESB_IAP_LEVV, inst_coords[0], 0);
            SetDlgItemInt(hwnd, ESB_IAP_XV, inst_coords[1], 0);
            SetDlgItemInt(hwnd, ESB_IAP_YV, inst_coords[2], 0);
            SendDlgItemMessage(hwnd, ESB_IAP_TILEPICK, CB_SETCURSEL, 4, 0);
            SendDlgItemMessage(hwnd, ESB_IAP_TILEPICK, CB_SETCURSEL, inst_coords[3], 0);
            
            if (lua_toboolean(LUA, -4))
                SendMessage(hwnd, WM_COMMAND, ESB_IAPM_TARG2OP, 0);    
            else
                SendMessage(hwnd, WM_COMMAND, ESB_IAPM_OP2TARG, 0);
                
            if (lua_toboolean(LUA, -3))
                SendMessage(hwnd, WM_COMMAND, ESB_IAPM_NOCAP, 0); 
                            
            SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEPARTY, CB_ADDSTRING,
                0, (int)"--(Nobody)--");
            SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEPARTY, CB_SETITEMDATA, 0, 0);
            SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEPARTY, CB_ADDSTRING,
                0, (int)"--(Leader)--");
            SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEPARTY, CB_SETITEMDATA, 1, -1);           
            for (cc=0;cc<gd.num_champs;cc++) {
                if (edg.c_ext[cc].desg) {
                    SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEPARTY, CB_ADDSTRING,
                        0, (int)edg.c_ext[cc].desg);
                    SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEPARTY, CB_SETITEMDATA,
                        line, cc+1);
                    line++;
                }
            } 
            SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEPARTY, CB_SETCURSEL, 0, 0);
            cc = lua_tointeger(LUA, -2);
            if (cc != 0) {
                int i;
                for(i=0;i<line;++i) {
                    int idata = SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEPARTY,
                        CB_GETITEMDATA, i, 0);
                    if (idata == cc) {
                        SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEPARTY, CB_SETCURSEL, i, 0);
                        break;
                    }
                }
            }            
                
            if (lua_toboolean(LUA, -1))
                SendMessage(hwnd, WM_COMMAND, ESB_IAI_PUTAWAY, 0);    
            else
                SendMessage(hwnd, WM_COMMAND, ESB_IAI_HANDS, 0);
                
            lua_pop(LUA, 14);

        } break;
            
        case WM_COMMAND: {
            int wp = LOWORD(wParam);
            switch (wp) {
                case IDOK: {
                    int targmode = 0;
                    int movemode = 0;
                    char tstr[100];
                    int i;
                    int csel;
                    
                    xinfo = get_hwnd(hwnd);
                    
                    memset(tstr, 0, 100);
                    
                    for (i=ESB_IA_TARGETLIST;i<=ESB_IA_ALLARCH;++i) {
                        if (SendDlgItemMessage(hwnd, i, BM_GETCHECK, 0, 0)) {
                            targmode = i - ESB_IA_TARGETLIST;
                            break;
                        }
                    }
                    for (i=ESB_IAP_NONE;i<=ESB_IAP_MOVEMOUSE;++i) {
                        if (SendDlgItemMessage(hwnd, i, BM_GETCHECK, 0, 0)) {
                            movemode = i - ESB_IAP_NONE;
                            break;
                        }
                    }
                    
                    if (targmode == 3)
                        GetDlgItemText(hwnd, ESB_IA_NEWARCHF, tstr, 100);
                    else if (targmode == 4)
                        GetDlgItemText(hwnd, ESB_IA_ALLARCHF, tstr, 100);
                    
                    luastacksize(16);
                    lua_getglobal(LUA, "__ed_item_action_store");
                    lua_pushinteger(LUA, xinfo->inst);
                    lua_pushinteger(LUA, targmode);
                    if (*tstr != '\0')
                        lua_pushstring(LUA, tstr);
                    else
                        lua_pushnil(LUA);
                    lua_pushboolean(LUA, SendDlgItemMessage(hwnd, ESB_IAM_SENDMSG, BM_GETCHECK, 0, 0)); 
                    csel = SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEMSG, CB_GETCURSEL, 0, 0);
                    SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEMSG, CB_GETLBTEXT, csel, (int)tstr);
                    lua_pushstring(LUA, tstr);
                    lua_pushboolean(LUA, SendDlgItemMessage(hwnd, ESB_IAP_TOTLOP, BM_GETCHECK, 0, 0)); 
                    lua_pushinteger(LUA, movemode);
                    lua_pushinteger(LUA, GetDlgItemInt(hwnd, ESB_IAP_LEVV, NULL, FALSE));
                    lua_pushinteger(LUA, GetDlgItemInt(hwnd, ESB_IAP_XV, NULL, FALSE));
                    lua_pushinteger(LUA, GetDlgItemInt(hwnd, ESB_IAP_YV, NULL, FALSE));
                    lua_pushinteger(LUA, SendDlgItemMessage(hwnd, ESB_IAP_TILEPICK, CB_GETCURSEL, 0, 0));
                    lua_pushboolean(LUA, SendDlgItemMessage(hwnd, ESB_IAPM_TARG2OP, BM_GETCHECK, 0, 0));
                    lua_pushboolean(LUA, SendDlgItemMessage(hwnd, ESB_IAPM_NOCAP, BM_GETCHECK, 0, 0)); 
                    csel = SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEPARTY, CB_GETCURSEL, 0, 0);
                    lua_pushinteger(LUA, SendDlgItemMessage(hwnd, ESB_IAP_CHOOSEPARTY, CB_GETITEMDATA, csel, 0)); 
                    lua_pushboolean(LUA, SendDlgItemMessage(hwnd, ESB_IAI_PUTAWAY, BM_GETCHECK, 0, 0));  
                    
                    if (lua_pcall(LUA, 15, 0, 0))
                        bomb_lua_error(NULL);
                                       
                    SendMessage(hwnd, WM_CLOSE, 0, 0);
                } break;
                
                case IDCANCEL: {
                    SendMessage(hwnd, WM_CLOSE, 0, 0);
                } break;
                
                case ESB_IA_TARGETLIST:
                case ESB_IA_OPBY:
                case ESB_IA_ORIG:
                case ESB_IA_NEW:
                case ESB_IA_ALLARCH: {
                    int i;
                    HWND f1 = GetDlgItem(hwnd, ESB_IA_NEWARCHF);
                    HWND f2 = GetDlgItem(hwnd, ESB_IA_ALLARCHF);
                    HWND c1 = GetDlgItem(hwnd, ESB_IA_NEWARCHC);
                    HWND c2 = GetDlgItem(hwnd, ESB_IA_ALLARCHC);
                    int fc1v = FALSE;
                    int fc2v = FALSE;
                    
                    for (i=ESB_IA_TARGETLIST;i<=ESB_IA_ALLARCH;++i) {
                        if (i == wp)
                            SendDlgItemMessage(hwnd, i, BM_SETCHECK, BST_CHECKED, 0);
                        else
                            SendDlgItemMessage(hwnd, i, BM_SETCHECK, BST_UNCHECKED, 0);
                    } 
                    
                    if (wp == ESB_IA_NEW)
                        fc1v = TRUE;
                    else if (wp == ESB_IA_ALLARCH)
                        fc2v = TRUE;
                        
                    EnableWindow(f1, fc1v);
                    EnableWindow(c1, fc1v);
                    EnableWindow(f2, fc2v);
                    EnableWindow(c2, fc2v);
                              
                } break;
                
                case ESB_IA_NEWARCHC:
                case ESB_IA_ALLARCHC: {
                    edg.op_arch = 0;
                    ESB_modaldb(ESB_ARCHPICKER, hwnd, ESB_archpick_proc, 0); 
                    if (edg.op_arch != 0xFFFFFFFF) {                             
                        struct obj_arch *p_arch = Arch(edg.op_arch);
                        SetDlgItemText(hwnd, wp - 1, p_arch->luaname);
                    } 
                } break;
                
                case ESB_IAM_SENDMSG: {
                    int state = SendDlgItemMessage(hwnd, ESB_IAM_SENDMSG, BM_GETCHECK, 0, 0);
                    item_action_msg_toggle(hwnd, state);
                } break;
                
                case ESB_IAP_TOACTION: {
                    SendDlgItemMessage(hwnd, ESB_IAP_TOACTION, BM_SETCHECK, BST_CHECKED, 0);
                    SendDlgItemMessage(hwnd, ESB_IAP_TOTLOP, BM_SETCHECK, BST_UNCHECKED, 0);
                } break;
                
                case ESB_IAP_TOTLOP: {
                    SendDlgItemMessage(hwnd, ESB_IAP_TOACTION, BM_SETCHECK, BST_UNCHECKED, 0);
                    SendDlgItemMessage(hwnd, ESB_IAP_TOTLOP, BM_SETCHECK, BST_CHECKED, 0);
                } break;
                
                case ESB_IAP_NONE:
                case ESB_IAP_MOVELOC:
                case ESB_IAP_MOVEINTO:
                case ESB_IAP_MOVETOLOC:
                case ESB_IAP_MOVECHAR:
                case ESB_IAP_MOVEMOUSE: {
                    int first = 0;
                    int last = 0;
                    int i;                 
                    for (i=ESB_IAP_NONE;i<=ESB_IAP_MOVEMOUSE;++i) {
                        if (i == wp)
                            SendDlgItemMessage(hwnd, i, BM_SETCHECK, BST_CHECKED, 0);
                        else
                            SendDlgItemMessage(hwnd, i, BM_SETCHECK, BST_UNCHECKED, 0);
                    }
                    item_action_full_disable(hwnd);
                    
                    if (wp == ESB_IAP_MOVELOC) {
                        first = 1941;
                        last = 1948;
                    } else if (wp == ESB_IAP_MOVEINTO) {
                        first = 1950;
                        last = 1953;
                    } else if (wp == ESB_IAP_MOVETOLOC) {
                        first = 1950;
                        last = 1952;
                    } else if (wp == ESB_IAP_MOVECHAR) {
                        first = 1955;
                        last = 1958;
                    }         
                    if (first) {
                        for(i=first;i<=last;++i) {
                            HWND d = GetDlgItem(hwnd, i);
                            EnableWindow(d, TRUE);
                        }
                    }     
                } break;
        
                case ESB_IAP_LOCSELECT: {
                    xinfo = get_hwnd(hwnd);
                    edg.op_inst = xinfo->inst;
                    edg.op_picker_val = 1;
                    edg.op_p_lev = GetDlgItemInt(hwnd, ESB_IAP_LEVV, NULL, FALSE);
                    if (edg.op_p_lev < 0 || edg.op_p_lev > gd.dungeon_levels)
                        edg.op_p_lev = 0;
                    edg.op_x = GetDlgItemInt(hwnd, ESB_IAP_XV, NULL, FALSE);
                    edg.op_y = GetDlgItemInt(hwnd, ESB_IAP_YV, NULL, FALSE);
                    edg.q_sp_options = 1;
                    ESB_modaldb(ESB_PICKER, hwnd, ESB_picker_proc, 1);
                    edg.q_sp_options = 0;
                    edg.op_picker_val = 0;
                    if (edg.op_p_lev >= 0) {
                        SetDlgItemInt(hwnd, ESB_IAP_LEVV, edg.op_p_lev, 0);
                        SetDlgItemInt(hwnd, ESB_IAP_XV, edg.op_x, 0);
                        SetDlgItemInt(hwnd, ESB_IAP_YV, edg.op_y, 0);
                    }
                } break;
                
                case ESB_IAPM_OP2TARG: {
                    SendDlgItemMessage(hwnd, ESB_IAPM_OP2TARG, BM_SETCHECK, BST_CHECKED, 0);
                    SendDlgItemMessage(hwnd, ESB_IAPM_TARG2OP, BM_SETCHECK, BST_UNCHECKED, 0);
                } break;
                
                case ESB_IAPM_TARG2OP: {
                    SendDlgItemMessage(hwnd, ESB_IAPM_OP2TARG, BM_SETCHECK, BST_UNCHECKED, 0);
                    SendDlgItemMessage(hwnd, ESB_IAPM_TARG2OP, BM_SETCHECK, BST_CHECKED, 0);
                } break;
                
                case ESB_IAI_HANDS: {
                    SendDlgItemMessage(hwnd, ESB_IAI_HANDS, BM_SETCHECK, BST_CHECKED, 0);
                    SendDlgItemMessage(hwnd, ESB_IAI_PUTAWAY, BM_SETCHECK, BST_UNCHECKED, 0);
                } break;
                
                case ESB_IAI_PUTAWAY: {
                    SendDlgItemMessage(hwnd, ESB_IAI_HANDS, BM_SETCHECK, BST_UNCHECKED, 0);
                    SendDlgItemMessage(hwnd, ESB_IAI_PUTAWAY, BM_SETCHECK, BST_CHECKED, 0);
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

INT_PTR CALLBACK ESB_door_edit_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo;
    
    onstack("ESB_door_edit_proc");

    switch(message) {
        case WM_INITDIALOG: {
            struct obj_arch *p_arch;
            int got_zoable;
            int zoable;
            int got_power;
            int bash_power;
            int fire_power; 
                  
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            p_arch = Arch(oinst[xinfo->inst]->arch);
            
            got_zoable = ex_exvar(xinfo->inst, "zoable", &zoable);
            if (!got_zoable || zoable >= 9999)
                SendDlgItemMessage(hwnd, ESB_D_NOZO, BM_SETCHECK, BST_CHECKED, 0);
            else
                SetDlgItemInt(hwnd, ESB_D_ZO, zoable, 0);            
                
            got_power = ex_exvar(xinfo->inst, "bash_power", &bash_power);
            if (!got_power) {
                bash_power = queryluaint(p_arch, "bash_power");
                if (!bash_power)
                    bash_power = 9999;
            }
            got_power = ex_exvar(xinfo->inst, "fire_power", &fire_power);
            if (!got_power) {
                fire_power = queryluaint(p_arch, "fire_power");
                if (!fire_power)
                    fire_power = 9999;
            }
            
            if (!bash_power || bash_power >= 9999)
                SendDlgItemMessage(hwnd, ESB_D_NOBASH, BM_SETCHECK, BST_CHECKED, 0);
            else
                SetDlgItemInt(hwnd, ESB_D_BASH, bash_power, 0);
                
            if (!fire_power || fire_power >= 9999)
                SendDlgItemMessage(hwnd, ESB_D_NOFIRE, BM_SETCHECK, BST_CHECKED, 0);
            else
                SetDlgItemInt(hwnd, ESB_D_FIRE, fire_power, 0);
                
            SendMessage(hwnd, WM_COMMAND, ESB_D_NOZO, 0);
            SendMessage(hwnd, WM_COMMAND, ESB_D_NOBASH, 0);
            SendMessage(hwnd, WM_COMMAND, ESB_D_NOFIRE, 0);
            
        } break;
        
        case WM_COMMAND: {
            int wp = LOWORD(wParam);
            switch (wp) {
                case IDOK: {
                    const char *spec_error = NULL;
                    int bash_power = GetDlgItemInt(hwnd, ESB_D_BASH, NULL, FALSE);
                    int fire_power = GetDlgItemInt(hwnd, ESB_D_FIRE, NULL, FALSE);
                    int zo_power = GetDlgItemInt(hwnd, ESB_D_ZO, NULL, FALSE);
                    if (SendDlgItemMessage(hwnd, ESB_D_NOZO, BM_GETCHECK, 0, 0))
                        zo_power = 9999;
                    if (SendDlgItemMessage(hwnd, ESB_D_NOBASH, BM_GETCHECK, 0, 0))
                        bash_power = 9999;
                    if (SendDlgItemMessage(hwnd, ESB_D_NOFIRE, BM_GETCHECK, 0, 0))
                        fire_power = 9999;
                    
                    if (bash_power <= 0)
                        spec_error = "Bash power";
                    else if (fire_power <= 0)
                        spec_error = "Fire power";
                    else if (zo_power <= 0)
                        spec_error = "Zo power";
                        
                    if (spec_error) {
                        char errstr[120];
                        sprintf(errstr, "You must specify a valid %s\n(Or check \"Immune\")", spec_error);
                        MessageBox(hwnd, errstr, "Error", MB_ICONEXCLAMATION);
                        RETURN(TRUE);
                    } 
                    
                    xinfo = get_hwnd(hwnd);
                    
                    luastacksize(6);
                    lua_getglobal(LUA, "__ed_doored_store");
                    lua_pushinteger(LUA, xinfo->inst);
                    lua_pushinteger(LUA, bash_power);
                    lua_pushinteger(LUA, fire_power);
                    lua_pushinteger(LUA, zo_power);
                    if (lua_pcall(LUA, 4, 0, 0))
                        bomb_lua_error(NULL);
                    
                    SendMessage(hwnd, WM_CLOSE, 0, 0);  
                    
                } break;
                
                case IDCANCEL: {
                    SendMessage(hwnd, WM_CLOSE, 0, 0);    
                } break;
                
                case ESB_D_NOZO: {
                    int state = SendDlgItemMessage(hwnd, ESB_D_NOZO, BM_GETCHECK, 0, 0);
                    HWND txw = GetDlgItem(hwnd, ESB_D_ZO);
                    int s = TRUE;
                    if (state == BST_CHECKED)
                        s = FALSE;
                    EnableWindow(txw, s);
                } break;
                
                case ESB_D_NOBASH: {
                    int state = SendDlgItemMessage(hwnd, ESB_D_NOBASH, BM_GETCHECK, 0, 0);
                    HWND txw = GetDlgItem(hwnd, ESB_D_BASH);
                    int s = TRUE;
                    if (state == BST_CHECKED)
                        s = FALSE;
                    EnableWindow(txw, s);
                } break;
                
                case ESB_D_NOFIRE: {
                    int state = SendDlgItemMessage(hwnd, ESB_D_NOFIRE, BM_GETCHECK, 0, 0);
                    HWND txw = GetDlgItem(hwnd, ESB_D_FIRE);
                    int s = TRUE;
                    if (state == BST_CHECKED)
                        s = FALSE;
                    EnableWindow(txw, s);
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

INT_PTR CALLBACK ESB_counter_edit_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo;
    
    onstack("ESB_counter_edit_proc");

    switch(message) {
        case WM_INITDIALOG: {
            int got_count;
            int base_count;
            int reverse_state = BST_CHECKED;
            int disable_state = BST_UNCHECKED;
                        
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
       
            if (ex_exvar(xinfo->inst, "no_reverse", NULL))
                reverse_state = BST_UNCHECKED;
            
            if (ex_exvar(xinfo->inst, "disable_self", NULL))
                disable_state = BST_CHECKED;
            
            SendDlgItemMessage(hwnd, ESB_CO_REVERSE, BM_SETCHECK, reverse_state, 0);
            SendDlgItemMessage(hwnd, ESB_CO_DISABLE, BM_SETCHECK, disable_state, 0);           
                
            got_count = ex_exvar(xinfo->inst, "count", &base_count);
            if (!got_count) {
                base_count = 2;
            }
            SetDlgItemInt(hwnd, ESB_EDITCOUNT, base_count, 0);
            
        } break;
        
        case WM_COMMAND: {
            int wp = LOWORD(wParam);
            switch (wp) {
                case IDOK: {
                    const char *spec_error = NULL;
                    int count = GetDlgItemInt(hwnd, ESB_EDITCOUNT, NULL, FALSE);
                    int reverse = SendDlgItemMessage(hwnd, ESB_CO_REVERSE, BM_GETCHECK, 0, 0);
                    int disable_self = SendDlgItemMessage(hwnd, ESB_CO_DISABLE, BM_GETCHECK, 0, 0);
                    
                    xinfo = get_hwnd(hwnd);
                    
                    luastacksize(6);
                    lua_getglobal(LUA, "__ed_countered_store");
                    lua_pushinteger(LUA, xinfo->inst);
                    lua_pushinteger(LUA, count);
                    lua_pushboolean(LUA, reverse);
                    lua_pushboolean(LUA, disable_self);
                    if (lua_pcall(LUA, 4, 0, 0))
                        bomb_lua_error(NULL);
                    
                    SendMessage(hwnd, WM_CLOSE, 0, 0);  
                    
                } break;
                
                case IDCANCEL: {
                    SendMessage(hwnd, WM_CLOSE, 0, 0);    
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

static void do_bit_enable(HWND hwnd) {
    int ic;
    int chkval = FALSE;
    
    if (SendDlgItemMessage(hwnd, ESB_BIT_ENABLE, BM_GETCHECK, 0, 0)) {
        chkval = TRUE;
    }
    
    for (ic=0;ic<10;++ic) {
        HWND ic_w = GetDlgItem(hwnd, ESB_IBITBOX + ic);
        EnableWindow(ic_w, chkval);
    }
}

INT_PTR CALLBACK ESB_trigger_controller_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo;
    
    onstack("ESB_trigger_controller_proc");

    switch(message) {
        case WM_INITDIALOG: {
            int fdir;
            int pr_get, pr_val;
            int check_get, check_val;
            int send_reverse, orig_preserve;
            int disable_self;
            int nline = 1;
            int use_bits = 0;
            int cc;
                              
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            
            luastacksize(15);
            lua_getglobal(LUA, "__ed_triggercontroller_getblockdirs_and_bits");
            lua_pushinteger(LUA, xinfo->inst);
            if (lua_pcall(LUA, 1, 13, 0))
                bomb_lua_error(NULL);
            for(fdir=0;fdir<4;++fdir) {
                int blocked_dir = lua_toboolean(LUA, -9 + (-1 * (4 - fdir)));
                int state = BST_CHECKED;
                if (blocked_dir)
                    state = BST_UNCHECKED;
            
                SendDlgItemMessage(hwnd, ESB_TRNORTH + fdir, BM_SETCHECK, state, 0);
            }
            use_bits = lua_toboolean(LUA, -9);
            if (use_bits) {
                int bd;
                SendDlgItemMessage(hwnd, ESB_BIT_ENABLE, BM_SETCHECK, BST_CHECKED, 0);
                for(bd=0;bd<8;++bd) {
                    int v = lua_toboolean(LUA, -1 * (8 - bd));
                    int state = BST_UNCHECKED;
                    if (v)
                        state = BST_CHECKED;
                        
                    SendDlgItemMessage(hwnd, ESB_IB_01 + bd, BM_SETCHECK, state, 0);
                }
            } else {
                do_bit_enable(hwnd);
            }
            lua_pop(LUA, 13);
            
            send_reverse = ex_exvar(xinfo->inst, "send_reverse", NULL);
            if (send_reverse) {
                HWND only_window = GetDlgItem(hwnd, ESB_TC_REVERSEONLY);
                int only_reverse = send_reverse = ex_exvar(xinfo->inst, "send_reverse_only", NULL);
                
                SendDlgItemMessage(hwnd, ESB_TC_REVERSEMSG, BM_SETCHECK, BST_CHECKED, 0);
                EnableWindow(only_window, TRUE);
                if (only_reverse) {
                    SendDlgItemMessage(hwnd, ESB_TC_REVERSEONLY, BM_SETCHECK, BST_CHECKED, 0);
                }
            }
                
            orig_preserve = ex_exvar(xinfo->inst, "originator_preserve", NULL);
            if (orig_preserve)
                SendDlgItemMessage(hwnd, ESB_TC_PRESERVE, BM_SETCHECK, BST_CHECKED, 0);
                
            disable_self = ex_exvar(xinfo->inst, "disable_self", NULL);
            if (disable_self)
                SendDlgItemMessage(hwnd, ESB_TC_DISABLESELF, BM_SETCHECK, BST_CHECKED, 0);
            
            pr_get = ex_exvar(xinfo->inst, "probability", &pr_val);
            if (!pr_get)
                pr_val = 100;
            SetDlgItemInt(hwnd, ESB_TC_PERCENT, pr_val, FALSE);
            
            SendDlgItemMessage(hwnd, ESB_TP_SPECCHAMP, CB_ADDSTRING,
                0, (int)"--(Anybody)--");
            SendDlgItemMessage(hwnd, ESB_TP_SPECCHAMP, CB_SETITEMDATA, 0, 0);          
            for (cc=0;cc<gd.num_champs;cc++) {
                if (edg.c_ext[cc].desg) {
                    SendDlgItemMessage(hwnd, ESB_TP_SPECCHAMP, CB_ADDSTRING,
                        0, (int)edg.c_ext[cc].desg);
                    SendDlgItemMessage(hwnd, ESB_TP_SPECCHAMP, CB_SETITEMDATA,
                        nline, cc+1);
                    nline++;
                }
            } 
            SendDlgItemMessage(hwnd, ESB_TP_SPECCHAMP, CB_SETCURSEL, 0, 0);
            
            check_get = ex_exvar(xinfo->inst, "party_contains", &check_val);
            if (!check_get || check_val < -1) {
                SendDlgItemMessage(hwnd, ESB_TP_EITHER, BM_SETCHECK, BST_CHECKED, 0);
                SendMessage(hwnd, WM_COMMAND, ESB_TP_EITHER, 0);
            } else if (check_val == -1) {
                SendDlgItemMessage(hwnd, ESB_TP_EMPTY, BM_SETCHECK, BST_CHECKED, 0);
                SendMessage(hwnd, WM_COMMAND, ESB_TP_EMPTY, 0);
            } else if (check_val == 0) {
                int memb_check, membs;
                memb_check = ex_exvar(xinfo->inst, "party_members", &membs);
                if (!memb_check || membs < 1)
                    membs = 1;
                else if (membs > 4)
                    membs = 4;      
                SetDlgItemInt(hwnd, ESB_TP_PARTYNUMBER, membs, 0);
                              
                SendDlgItemMessage(hwnd, ESB_TP_ATLEASTONE, BM_SETCHECK, BST_CHECKED, 0);
                SendMessage(hwnd, WM_COMMAND, ESB_TP_ATLEASTONE, 0);
            } else {
                int i;
                SendDlgItemMessage(hwnd, ESB_TP_SPECIFIC, BM_SETCHECK, BST_CHECKED, 0);
                SendMessage(hwnd, WM_COMMAND, ESB_TP_SPECIFIC, 0);
                for(i=0;i<nline;++i) {
                    int idata = SendDlgItemMessage(hwnd, ESB_TP_SPECCHAMP, CB_GETITEMDATA, i, 0);
                    if (idata == check_val) {
                        SendDlgItemMessage(hwnd, ESB_TP_SPECCHAMP, CB_SETCURSEL, i, 0);
                        break;
                    }
                }
            }  
        } break;
        
        case WM_COMMAND: {
            int wp = LOWORD(wParam);
            switch (wp) {
                case IDOK: {  
                    int fdir;
                    int blocked_dirs = 0;
                    int blockdirs[4];
                    int tp_nil = 0;
                    int tp_options;
                    int tp_members = 0; 
                    int chance;          
                    int send_reverse = 0;
                    int send_reverse_only = 0;
                    int orig_preserve = 0;
                    int disable_self = 0;
                    int use_bits = 0;
                    
                    xinfo = get_hwnd(hwnd);
                    
                    memset(blockdirs, 0, sizeof(int) * 4);
                    
                    if (SendDlgItemMessage(hwnd, ESB_TP_EMPTY, BM_GETCHECK, 0, 0)) {
                        tp_options = -1;
                    } else if (SendDlgItemMessage(hwnd, ESB_TP_ATLEASTONE, BM_GETCHECK, 0, 0)) {
                        tp_options = 0;
                        tp_members = GetDlgItemInt(hwnd, ESB_TP_PARTYNUMBER, 0, 0);
                        if (tp_members < 1 || tp_members > 4) {
                            MessageBox(hwnd, "Required members must be between 1 and 4.", "Parameter Error", MB_ICONEXCLAMATION);
                            RETURN(TRUE);
                        }
                    } else if (SendDlgItemMessage(hwnd, ESB_TP_SPECIFIC, BM_GETCHECK, 0, 0)) {
                        int csel = SendDlgItemMessage(hwnd, ESB_TP_SPECCHAMP, CB_GETCURSEL, 0, 0);
                        tp_options = SendDlgItemMessage(hwnd, ESB_TP_SPECCHAMP, CB_GETITEMDATA, csel, 0);
                    } else {
                        tp_nil = 1;
                    }
                    
                    for(fdir=0;fdir<4;++fdir) {
                        int open_val = SendDlgItemMessage(hwnd, ESB_TRNORTH + fdir, BM_GETCHECK, 0, 0);
                        blockdirs[fdir] = !open_val;
                        blocked_dirs += !open_val;
                    }
                    if (blocked_dirs >= 4) {
                        MessageBox(hwnd, "At least one party direction must be allowable.", "Facing Error", MB_ICONEXCLAMATION);
                        RETURN(TRUE);
                    }
                                  
                    chance = GetDlgItemInt(hwnd, ESB_TC_PERCENT, NULL, FALSE);
                    if (chance < 1 || chance > 100) {
                        MessageBox(hwnd, "Probability must be between 1 and 100.", "Parameter Error", MB_ICONEXCLAMATION);
                        RETURN(TRUE);
                    }  
                    
                    if (SendDlgItemMessage(hwnd, ESB_TC_REVERSEMSG, BM_GETCHECK, 0, 0)) {
                        send_reverse = 1;
                        if (SendDlgItemMessage(hwnd, ESB_TC_REVERSEONLY, BM_GETCHECK, 0, 0)) {
                            send_reverse_only = 1;
                        }
                    }
                                            
                    if (SendDlgItemMessage(hwnd, ESB_TC_PRESERVE, BM_GETCHECK, 0, 0))
                        orig_preserve = 1;
                        
                    if (SendDlgItemMessage(hwnd, ESB_BIT_ENABLE, BM_GETCHECK, 0, 0))
                        use_bits = 1;
                        
                    if (SendDlgItemMessage(hwnd, ESB_TC_DISABLESELF, BM_GETCHECK, 0, 0))
                        disable_self = 1;
                     
                    luastacksize(22);
                    lua_getglobal(LUA, "__ed_triggercontroller_store");
                    lua_pushinteger(LUA, xinfo->inst);
                    if (tp_nil)
                        lua_pushnil(LUA);
                    else
                        lua_pushinteger(LUA, tp_options);
                    lua_pushinteger(LUA, tp_members);
                    lua_pushinteger(LUA, chance);
                    lua_pushboolean(LUA, send_reverse);
                    lua_pushboolean(LUA, send_reverse_only);
                    lua_pushboolean(LUA, orig_preserve);
                    lua_pushboolean(LUA, disable_self);
                                        
                    for (fdir=0;fdir<4;++fdir)
                        lua_pushboolean(LUA, blockdirs[fdir]);
                        
                    // store bit information
                    lua_pushboolean(LUA, use_bits);
                    if (use_bits) {
                        int bt;
                        for (bt=0;bt<8;bt++) {
                            int cbt_v = SendDlgItemMessage(hwnd, ESB_IB_01 + bt, BM_GETCHECK, 0, 0);
                            lua_pushboolean(LUA, cbt_v);
                        }
                    } else {
                        int bt;
                        for (bt=0;bt<8;bt++) {
                            lua_pushnil(LUA);
                        }
                    }
                        
                    if (lua_pcall(LUA, 21, 0, 0))
                        bomb_lua_error(NULL);
                    
                    SendMessage(hwnd, WM_CLOSE, 0, 0);  
                    
                } break;
                
                case IDCANCEL: {
                    SendMessage(hwnd, WM_CLOSE, 0, 0);    
                } break;
                
                case ESB_BIT_ENABLE: {
                    do_bit_enable(hwnd);
                } break;
                
                case ESB_TP_EITHER:
                case ESB_TP_EMPTY:
                case ESB_TP_ATLEASTONE:
                {
                    HWND sc = GetDlgItem(hwnd, ESB_TP_SPECCHAMP);
                    HWND pn = GetDlgItem(hwnd, ESB_TP_PARTYNUMBER);
                    int pn_val = FALSE;
                    
                    if (wp == ESB_TP_ATLEASTONE)
                        pn_val = TRUE;
                        
                    EnableWindow(sc, FALSE);
                    EnableWindow(pn, pn_val);
                } break;
                
                case ESB_TP_SPECIFIC: {
                    HWND sc = GetDlgItem(hwnd, ESB_TP_SPECCHAMP);
                    HWND pn = GetDlgItem(hwnd, ESB_TP_PARTYNUMBER);
                    EnableWindow(sc, TRUE);
                    EnableWindow(pn, FALSE);
                } break;
                
                case ESB_TC_REVERSEMSG: {
                    int rval = SendDlgItemMessage(hwnd, ESB_TC_REVERSEMSG, BM_GETCHECK, 0, 0);
                    HWND ro = GetDlgItem(hwnd, ESB_TC_REVERSEONLY);
                    if (rval)
                        EnableWindow(ro, TRUE);
                    else
                        EnableWindow(ro, FALSE);
                }
                
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
        
        


