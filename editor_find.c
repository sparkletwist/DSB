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

int sel2inst(HWND hwnd, int ctrl) {
    char txf[120];
    char *tx_scan = txf;
    int sel_id = LB_ERR;
    int selinst = 0;
    
    onstack("sel2inst");
    
    sel_id = SendDlgItemMessage(hwnd, ctrl, LB_GETCURSEL, 0, 0);
    if (sel_id == LB_ERR) {
        RETURN(0);
    }
    SendDlgItemMessage(hwnd, ctrl, LB_GETTEXT, sel_id, (LPARAM)txf);
    while (*tx_scan != '@' && *tx_scan != '(')
        ++tx_scan;
    --tx_scan;
    *tx_scan = '\0';
    selinst = atoi(txf);
    
    if (!oinst[selinst]) {
        MessageBox(hwnd, "This instance no longer exists.", "Error", MB_ICONEXCLAMATION); 
        RETURN(0);
    }
    
    RETURN(selinst);    
}

INT_PTR CALLBACK ESB_goto_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo;
    
    onstack("ESB_goto_proc");
    
    switch(message) { 
        case WM_COMMAND: {
            int lw = LOWORD(wParam);
            int hw = HIWORD(wParam);
            if (lw == IDCANCEL) {
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            } else if (lw == IDOK || lw == ESB_EDITMAINVIEW) {
                edg.op_picker_val = 0;
                if (edg.op_p_lev != edg.ed_lev) {
                    edg.ed_lev = edg.op_p_lev;
                    ed_init_level_bmp(edg.ed_lev, sys_hwnd);
                    ed_resizesubs(sys_hwnd);
                    QO_MOUSEOVER();
                }
                edg.global_mark_level = edg.op_p_lev;
                edg.global_mark_x = edg.op_x;
                edg.global_mark_y = edg.op_y;
                RedrawWindow(edg.subwin, NULL, NULL, RDW_INVALIDATE);
                scroll_picker_to_coordinate(edg.subwin, edg.op_x, edg.op_y);
                force_redraw(edg.subwin, 1);
                edg.op_x = -1;
                ESB_go_up_one(hwnd);
                SendMessage(hwnd, WM_CLOSE, 0, 0);
                RedrawWindow(edg.subwin, NULL, NULL, RDW_INVALIDATE);
                RETURN(TRUE);   
            }
        } break; 
                      
        case WM_INITDIALOG: {
            HWND dl = GetDlgItem(hwnd, ESB_GOTOLEVINFO);
            HWND bb = GetDlgItem(hwnd, ESB_GOTOINSTLOCW);
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            xinfo->wtype = XW_NO_CLICKS;
            xinfo->b_buf = NULL;
            xinfo->b_buf_win = bb;
            xinfo->plevel = oinst[xinfo->inst]->level;
            edg.op_picker_val = 2;
            edg.op_p_lev = xinfo->plevel;
            edg.op_x = oinst[xinfo->inst]->x;
            edg.op_y = oinst[xinfo->inst]->y;   
            SendMessage(dl, WM_SETFONT, 0, 0);    
            do_level_settings(hwnd, xinfo, dl);
            scroll_picker_to_coordinate(bb, edg.op_x, edg.op_y);
            SetFocus(hwnd);            
        } break;

        case WM_CLOSE: {
            edg.op_picker_val = 0;
            edg.op_x = -1;
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

INT_PTR CALLBACK ESBsearchproc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo;
    
    onstack("ESBsearchproc");

    switch(message) { 
        case WM_COMMAND: {
            int lw = LOWORD(wParam);
            int hw = HIWORD(wParam);
            if (lw == IDCANCEL) {
                SendMessage(hwnd, WM_CLOSE, 0, 0);
                break;
            } else if (lw == IDOK) {
                SendMessage(hwnd, WM_CLOSE, 0, 0); 
                break;
            }
            
            xinfo = get_hwnd(hwnd);
            switch (lw) {
                case ESB_FINDRESULTS: {
                    if (hw == LBN_DBLCLK) {
                        SendMessage(hwnd, WM_COMMAND, ESB_FINDGOTO, 0);
                    }
                } break;
                
                case ESB_FINDEDIT: {
                    int inst = sel2inst(hwnd, ESB_FINDRESULTS);
                    if (inst) {
                        edg.op_inst = inst;
                        DialogBox(GetModuleHandle(NULL),
                            MAKEINTRESOURCE(ESB_EXVEDIT),
                            hwnd, ESBdproc);       
                    }                    
                } break;
                
                case ESB_FINDGOTO: {
                    int inst = sel2inst(hwnd, ESB_FINDRESULTS);
                    if (inst) {
                        edg.op_inst = inst;
                        while (oinst[edg.op_inst]->level == LOC_IN_OBJ) {
                            edg.op_inst = oinst[edg.op_inst]->x;
                        }
                        if (oinst[edg.op_inst]->level < 0) {
                            MessageBox(hwnd, "This instance is not in the dungeon.", "Cannot Go To", MB_ICONEXCLAMATION); 
                            edg.op_inst = 0;
                            break;  
                        }      
                        ESB_modaldb(ESB_GOTORESULTS, hwnd, ESB_goto_proc, 0);
                    }
                } break;
                
                case ESB_PICKSEARCH: {
                    if (xinfo->sel_str) {
                        char stringbuffer[120];
                        char *opby_string = NULL;
                        int results;
                        int opby = SendDlgItemMessage(hwnd, ESB_DOOPBYSEARCH, BM_GETCHECK, 0, 0);
                        if (opby) {
                            GetDlgItemText(hwnd, ESB_OPBYEXVAR, stringbuffer, 120);
                            if (*stringbuffer != '\0')
                                opby_string = stringbuffer;
                        }
                        SendDlgItemMessage(hwnd, ESB_FINDRESULTS, LB_RESETCONTENT, 0, 0);
                        edg.op_hwnd = hwnd;
                        edg.op_b_id = ESB_FINDRESULTS;
                        edg.op_winmsg = LB_ADDSTRING;           
                        luastacksize(4);
                        lua_getglobal(LUA, "__ed_find_insts");
                        lua_pushstring(LUA, xinfo->sel_str);
                        if (opby && opby_string)
                            lua_pushstring(LUA, opby_string);
                        else
                            lua_pushnil(LUA);
                        results = lc_call_topstack(2, "ed_find_insts");
                        if (!results) {
                            MessageBox(hwnd, "No results found.", "Results", MB_ICONINFORMATION);
                        }
                    } else
                        MessageBox(hwnd, "No search arch selected.", "Cannot Search", MB_ICONEXCLAMATION);
                } break;
            }
        } break; 
        
        case WM_NOTIFY: {
            NMHDR *ncmd = (NMHDR*)lParam;
            if (ncmd->idFrom != ESB_FINDARCHSEARCH)
                break;
            xinfo = get_hwnd(hwnd);
            switch (ncmd->code) {
                case TVN_SELCHANGED: {
                    NM_TREEVIEW *nmtv;
                    TV_ITEM *seltv;
                    nmtv = (NM_TREEVIEW*)lParam;
                    seltv = &(nmtv->itemNew);
                    if (seltv->lParam)
                        xinfo->sel_str = (char*)(seltv->lParam + 1); 
                    else
                        xinfo->sel_str = NULL;                 
                } break;
                
                case NM_DBLCLK: {
                    if (xinfo->sel_str) {
                        SendMessage(hwnd, WM_COMMAND, ESB_PICKSEARCH, 0);
                    }
                } break;
                    
                case TVN_DELETEITEM:
                    clbr_del(lParam);
                    break;
            }  
        } break;
              
        case WM_INITDIALOG: {
            HWND treehwnd = GetDlgItem(hwnd, ESB_FINDARCHSEARCH);
            
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            xinfo->sel_str = NULL;     
            
            ed_build_tree(treehwnd, -1, NULL);         
            edg.op_arch = 0xFFFFFFFF;
        } break;

        case WM_CLOSE: {
            force_redraw(edg.subwin, 1); 
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
