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

void ed_modify_pickwin_size(HWND hwnd, int id_sb,
    int id_b_gadget, int id_h_gadget, int id_pickwin)
{
    struct ext_hwnd_info *xinfo;
    HWND w_draw;
    RECT r_window;
    int i_n;
    int dw, dh;
    int w, h;
    int boxw = 0;
    int boxh = 0;
    int force_size = 0;
    
    onstack("ed_modify_pickwin_size");

    xinfo = get_hwnd(hwnd);
    
    GetWindowRect(hwnd, &r_window); 
    w = r_window.right - r_window.left;
    h = r_window.bottom - r_window.top;
    
    if (w < xinfo->b_w) {
        force_size = 1;
        w = xinfo->b_w;
    }
    
    if (h < xinfo->b_h) {
        force_size = 1;
        h = xinfo->b_h;
    }
    
    if (force_size) {
        SetWindowPos(hwnd, NULL, r_window.left, r_window.top,
            w, h, SWP_NOZORDER);    
    }
    
    dw = w - xinfo->c_w;
    dh = h - xinfo->c_h;

    SendDlgItemMessage(hwnd, id_sb, WM_SIZE, 0, 0);
    
    for(i_n = id_b_gadget; i_n <= id_h_gadget; ++i_n) {
        POINT p;
        HWND w = GetDlgItem(hwnd, i_n);
        RECT r_s;
        
        if (!w || i_n == id_pickwin)
            continue;
        
        GetWindowRect(w, &r_s);
        
        p.x = r_s.left;
        p.y = r_s.top;
        
        ScreenToClient(hwnd, &p);
        p.x += dw;
        
        boxw = r_s.right - r_s.left;
        boxh = r_s.bottom - r_s.top;
        
        SetWindowPos(w, NULL, p.x, p.y, boxw, boxh, SWP_NOZORDER); 
    }
    
    w_draw = GetDlgItem(hwnd, id_pickwin);
    if (w_draw) {
        POINT p;
        RECT r_s;
        GetWindowRect(w_draw, &r_s);
        
        p.x = r_s.left;
        p.y = r_s.top;
        
        ScreenToClient(hwnd, &p);
        
        boxw = r_s.right - r_s.left + dw;
        boxh = r_s.bottom - r_s.top + dh;
        
        SetWindowPos(w_draw, NULL, p.x, p.y, boxw, boxh, SWP_NOZORDER);                 
    }
    
    xinfo->c_w = w;
    xinfo->c_h = h;
    
    ed_scrollset(0, xinfo->plevel, xinfo->b_buf_win, boxw, boxh);
    InvalidateRect(hwnd, NULL, TRUE);  

    VOIDRETURN();    
}

INT_PTR CALLBACK ESB_archpick_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo = NULL;
    
    onstack("ESB_archpick_proc");
    ESB_MESSAGE_DEBUG();

    switch(message) { 
        case WM_COMMAND: {
            int lw = LOWORD(wParam);
            if (lw == IDCANCEL)
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            else if (lw == IDOK) {
                xinfo = get_hwnd(hwnd);
                if (xinfo->sel_str) {
                    int archid;

                    luastacksize(4);
                    lua_getglobal(LUA, "obj");
                    lua_pushstring(LUA, xinfo->sel_str);
                    lua_gettable(LUA, -2);
                    lua_pushstring(LUA, "regnum");
                    lua_gettable(LUA, -2);
                    archid = (int)lua_touserdata(LUA, -1);
                    lua_pop(LUA, 3);
                    
                    edg.op_arch = archid; 
                    SendMessage(hwnd, WM_CLOSE, 0, 0);
                    RETURN(TRUE);   
                }    
            }
        } break; 
        
        case WM_NOTIFY: {
            NMHDR *ncmd = (NMHDR*)lParam;
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
                        SendMessage(hwnd, WM_COMMAND, IDOK, 0);
                    }
                } break;
                    
                case TVN_DELETEITEM: {
                    clbr_del(lParam);
                } break;
            }  
        } break;
              
        case WM_INITDIALOG: {
            int selected = 0;
            HWND treehwnd = GetDlgItem(hwnd, ESB_ARCHTABLE);
            
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            xinfo->sel_str = NULL;              
            
            ed_build_tree(treehwnd, edg.op_arch, edg.op_base_sel_str);
            if (edg.op_base_sel_str && edg.op_data_ptr) {
                HTREEITEM nitem = (HTREEITEM)edg.op_data_ptr;
                TreeView_SelectItem(treehwnd, nitem);
                edg.op_data_ptr = NULL;
                selected = 1;
            }
            if (edg.op_arch != 0 && !selected)
                TreeView_Expand(treehwnd, TreeView_GetRoot(treehwnd), TVE_EXPAND);
                
            edg.op_arch = 0xFFFFFFFF;
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

INT_PTR CALLBACK ESB_archpick_swap_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo;
    
    onstack("ESB_archpick_swap_proc");

    switch(message) { 
        case WM_COMMAND: {
            int lw = LOWORD(wParam);
            if (lw == IDCANCEL)
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            else if (lw == IDOK) {
                int swap_state = 0;
                int target_opby = 0;
                
                xinfo = get_hwnd(hwnd);
                
                luastacksize(5);
                lua_getglobal(LUA, "__ed_swap_store");
                lua_pushinteger(LUA, xinfo->inst);
                if (xinfo->sel_str)
                    lua_pushstring(LUA, xinfo->sel_str);
                else
                    lua_pushnil(LUA);
                swap_state = SendDlgItemMessage(hwnd, ESB_SWAPISFULLSWAP, BM_GETCHECK, 0, 0);
                lua_pushboolean(LUA, swap_state);
                target_opby = SendDlgItemMessage(hwnd, ESB_SWAP_OPBY, BM_GETCHECK, 0, 0);
                lua_pushboolean(LUA, target_opby);
                lc_call_topstack(4, "ed_swap_store");
                
                SendMessage(hwnd, WM_CLOSE, 0, 0);
                RETURN(TRUE);     
            }
        } break; 
        
        case WM_NOTIFY: {
            NMHDR *ncmd = (NMHDR*)lParam;
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
                        
                    if (xinfo->sel_str)
                        SetDlgItemText(hwnd, ESB_SWAPARCHID, xinfo->sel_str);
                    else
                        SetDlgItemText(hwnd, ESB_SWAPARCHID, "(None Selected)");                 
                } break;
                
                case NM_DBLCLK: {
                    if (xinfo->sel_str) {
                        SendMessage(hwnd, WM_COMMAND, IDOK, 0);
                    }
                } break;
                    
                case TVN_DELETEITEM:
                    clbr_del(lParam);
                    break;
            }  
        } break;
              
        case WM_INITDIALOG: {
            const char *base_sel = exs_exvar(edg.op_inst, "arch");
            int box, swaptarg, swaptarg_v;
            HWND treehwnd = GetDlgItem(hwnd, ESB_ARCHTABLE_SWAP);
            
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            xinfo->sel_str = NULL;              
            edg.op_arch = 0xFFFFFFFF;
            
            ed_build_tree(treehwnd, -1, base_sel);
            if (edg.op_data_ptr) {
                HTREEITEM nitem = (HTREEITEM)edg.op_data_ptr;
                TreeView_SelectItem(treehwnd, nitem);
                edg.op_data_ptr = NULL;
            }
            dsbfree(base_sel);
            
            box = lc_parm_int("__ed_swap_checkbox", 1, xinfo->inst);
            if (box)
                SendDlgItemMessage(hwnd, ESB_SWAPISFULLSWAP, BM_SETCHECK, BST_CHECKED, 0);
                
            swaptarg = ex_exvar(xinfo->inst, "target_opby", &swaptarg_v);
            if (swaptarg)
                SendDlgItemMessage(hwnd, ESB_SWAP_OPBY, BM_SETCHECK, BST_CHECKED, 0);
            else
                SendDlgItemMessage(hwnd, ESB_SWAP_SLIST, BM_SETCHECK, BST_CHECKED, 0);           
            
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


INT_PTR CALLBACK ESB_targetidchooseproc (HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    unsigned int inst;
    int sidx = 0;
    int mv = 0;
    
    onstack("ESB_targetidchooseproc");

    switch(message) {
        case WM_INITDIALOG:
            ed_fillboxfromtile(hwnd, ESB_IDBOX_T, edg.op_p_lev, edg.op_x, edg.op_y, -1);
            break;
            
        case WM_COMMAND:
            inst = ed_optinst(hwnd, ESB_IDBOX_T, &sidx);
            if (inst || LOWORD(wParam) == IDCANCEL) {
                int hw = HIWORD(wParam);
                switch(LOWORD(wParam)) {
                    case IDCANCEL:
                        SendMessage(hwnd, WM_CLOSE, 0, 0);
                    break;        
                    
                    case ESB_IDBOX_T: {
                        if (hw == LBN_DBLCLK) {
                            goto PICK_FALLTHROUGH;
                        }
                    } break;

                    case ESB_IDPICK_T:
                        PICK_FALLTHROUGH:
                        edg.op_inst = inst;
                        EndDialog(hwnd, 1);
                    break;
                }
            }
            break;

        case WM_CLOSE:
            edg.op_inst = 0;
            EndDialog(hwnd, 0);
            break;

        default:
            RETURN(FALSE);
    }

    RETURN(TRUE);
}

int ed_picker_click(int type, HWND ehwnd, int inst, int level, int x, int y) {
    int tx, ty;
    int nx, ny;
    int ch;
    int need_update = 0;
    struct dungeon_level *dd;

    onstack("ed_picker_click");

    dd = &(dun[level]);
    ch = edg.cells->w / 8;
    tninfo(ehwnd, dd, ch, x, y, &nx, &ny, &tx, &ty);
    
    if (tx >= 0 && ty >= 0) {
        if (tx < dd->xsiz && ty < dd->ysiz) {
            edg.op_p_lev = level;
            edg.op_x = tx;
            edg.op_y = ty;
                 
            if (type == 2) {
                int oi = ed_location_pick(level, tx, ty);
                if (oi == -1) {
                    force_redraw(ehwnd, 1);
                    DialogBox(GetModuleHandle(NULL),
                        MAKEINTRESOURCE(ESB_IDCHOOSE_T),
                        ehwnd, ESB_targetidchooseproc);
                    oi = edg.op_inst;                    
                }
                if (oi > 0) {
                    HWND phwnd = GetParent(ehwnd);
                    SendMessage(phwnd, EDM_TARGETPICK, oi, 0);
                } else
                    edg.op_x = -1;
            }
            force_redraw(ehwnd, 1);
        }
    }
    
    RETURN(0);
}

int ed_location_pick(int lev, int tx, int ty) {
    struct dungeon_level *dd;
    int i;
    int op_inst = 0;
    
    onstack("ed_location_pick");
    
    dd = &(dun[lev]);
    for (i=0;i<=4;++i) {
        struct inst_loc *dt = dd->t[ty][tx].il[i];
        while (dt != NULL) {
            if (!dt->slave) {
                if (!op_inst) op_inst = dt->i;
                else {
                    op_inst = -1;
                    break;
                }
            }
            dt = dt->n;
        }
        if (op_inst == -1)
            break;
    }
    
    RETURN(op_inst);
}

void do_level_settings(HWND hwnd, struct ext_hwnd_info *xinfo, HWND tx) {
    RECT r_client;
    int i_w, i_h;
    char dumper[16];
    
    onstack("do_level_settings");
    
    ed_init_level_bmp(xinfo->plevel, hwnd);
    GetClientRect(xinfo->b_buf_win, &r_client);
    i_w = r_client.right - r_client.left;
    i_h = r_client.bottom - r_client.top;
    ed_scrollset(xinfo->wtype, xinfo->plevel, xinfo->b_buf_win, i_w, i_h);
    
    snprintf(dumper, sizeof(dumper), "Level %d", xinfo->plevel);
    SetWindowText(tx, dumper);
    
    InvalidateRect(xinfo->b_buf_win, NULL, TRUE);
    UpdateWindow(xinfo->b_buf_win);
    
    VOIDRETURN();
}

void reset_scroll_coordinates(HWND bb) {
    SCROLLINFO sbar;
    int hsval, vsval;
    onstack("reset_scroll_coordinates");
    
    memset(&sbar, 0, sizeof(SCROLLINFO));
    sbar.cbSize = sizeof(SCROLLINFO);
    sbar.fMask = SIF_POS;
    GetScrollInfo(bb, SB_HORZ, &sbar);
    hsval = sbar.nPos;
    sbar.nPos = 0;
    SetScrollInfo(bb, SB_HORZ, &sbar, TRUE);
    
    memset(&sbar, 0, sizeof(SCROLLINFO));
    sbar.cbSize = sizeof(SCROLLINFO);
    sbar.fMask = SIF_POS;
    GetScrollInfo(bb, SB_VERT, &sbar);
    vsval = sbar.nPos;
    sbar.nPos = 0;
    SetScrollInfo(bb, SB_VERT, &sbar, TRUE);
     
    ScrollWindow(bb, -1 * hsval, -1 * vsval, NULL, NULL);
    UpdateWindow(bb);
    
    VOIDRETURN();
}

void scroll_picker_to_coordinate(HWND bb, int x, int y) {
    SCROLLINFO sbar;
    int ch = edg.cells->w / 8;
    int nhpos, nvpos;
    int wxsize, wysize;
    RECT r_client;
    
    onstack("scroll_picker_to_coordinate");
    
    GetClientRect(bb, &r_client);
    wxsize = r_client.right - r_client.left;
    wysize = r_client.bottom - r_client.top;
    
    nhpos = DRAW_OFFSET + (x*ch - wxsize/2);
    nvpos = DRAW_OFFSET + (y*ch - wysize/2);
    
    memset(&sbar, 0, sizeof(SCROLLINFO));
    sbar.cbSize = sizeof(SCROLLINFO);
    sbar.fMask = SIF_POS;
    sbar.nPos = nhpos;
    SetScrollInfo(bb, SB_HORZ, &sbar, TRUE);
    
    memset(&sbar, 0, sizeof(SCROLLINFO));
    sbar.cbSize = sizeof(SCROLLINFO);
    sbar.fMask = SIF_POS;
    sbar.nPos = nvpos;
    SetScrollInfo(bb, SB_VERT, &sbar, TRUE);       
            
    ScrollWindow(bb, nhpos, nvpos, NULL, NULL);
    UpdateWindow(bb);  
    
    VOIDRETURN(); 
}

// coordinate picker
INT_PTR CALLBACK ESB_picker_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    int i_res;

    onstack("ESB_picker_proc");

    switch(message) { 
        case EDM_DVKEY: {
            if (wParam == 'Q')
                PostMessage(hwnd, WM_COMMAND, ESB_LGOUP, 0);
            else if (wParam == 'E') 
                PostMessage(hwnd, WM_COMMAND, ESB_LGODN, 0);   
        } break;
        
        case WM_COMMAND: {
            struct ext_hwnd_info *xinfo;
            int lw = LOWORD(wParam);
            xinfo = get_hwnd(hwnd);
            if (lw == ESB_LGOUP) {
                int pl = (xinfo->plevel - 1);
                if (pl >= 0) {
                    HWND dl = GetDlgItem(hwnd, ESB_ILEVEL);
                    xinfo->plevel = pl;
                    do_level_settings(hwnd, xinfo, dl);
                }    
            } else if (lw == ESB_LGODN) {
                int pl = (xinfo->plevel + 1);
                if (pl < gd.dungeon_levels) {
                    HWND dl = GetDlgItem(hwnd, ESB_ILEVEL);
                    xinfo->plevel = pl;
                    do_level_settings(hwnd, xinfo, dl);
                } 
            } else if (lw == IDOK || lw == ESB_LP_OK) {
                if (!edg.q_sp_options) {
                    int silent = SendDlgItemMessage(hwnd, ESB_PSSILENT, BM_GETCHECK, 0, 0);
                    int defbuzz = SendDlgItemMessage(hwnd, ESB_PSSBUZZ, BM_GETCHECK, 0, 0);
                    int ssound = SendDlgItemMessage(hwnd, ESB_PSSOUND, BM_GETCHECK, 0, 0);
                    char dtext[100] = { 0 };
                    if (ssound) {
                        GetDlgItemText(hwnd, ESB_PSTYPESOUND, dtext, sizeof(dtext));
                        if (*dtext == '\0') {
                            MessageBox(hwnd, "You must specify a sound name.", "Sound Error", MB_ICONEXCLAMATION);
                            RETURN(TRUE);
                        }
                    }
    
                    lc_parm_int("editor_exvar_coordinate_picker", 4, xinfo->inst,
                        edg.op_p_lev, edg.op_x, edg.op_y);
                    
                    lua_getglobal(LUA, "editor_store_sound_exvars");
                    lua_pushinteger(LUA, xinfo->inst);
                    lua_pushboolean(LUA, 1);
                    lua_pushboolean(LUA, silent);
                    lua_pushboolean(LUA, defbuzz);
                    lua_pushboolean(LUA, ssound);
                    lua_pushstring(LUA, dtext); 
                    lc_call_topstack(6, "editor_store_sound_exvars");
                }
                
                ESB_go_up_one(hwnd);    
            } else if (lw == IDCANCEL || lw == ESB_LP_CAN) {
                edg.op_p_lev = -1;
                ESB_go_up_one(hwnd);
            }    
        } break; 
        
        case WM_SIZE: {
            ed_modify_pickwin_size(hwnd, ESB_PICKER_SB,
                ESB_ILEVEL, ESB_PSOUNDBOX, ESB_PICKWINDOW);
        } break;            
              
        case WM_INITDIALOG: {
            struct ext_hwnd_info *xinfo;
            struct obj_arch *p_arch;
            int s;
            HWND dl = GetDlgItem(hwnd, ESB_ILEVEL);
            HWND bb = GetDlgItem(hwnd, ESB_PICKWINDOW);
            
            edg.allow_control = GetDlgItem(hwnd, ESB_PSTYPESOUND);
                        
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            add_xinfo_coordinates(hwnd, xinfo, 1);
            xinfo->wtype = edg.op_picker_val;
            xinfo->b_buf = NULL;
            xinfo->b_buf_win = bb;
            xinfo->plevel = edg.op_p_lev;
            SendMessage(dl, WM_SETFONT, 0, 0);       
            do_level_settings(hwnd, xinfo, dl);
            scroll_picker_to_coordinate(bb, edg.op_x, edg.op_y);
            SetFocus(hwnd);
            
            if (edg.q_sp_options) {
                int i;
                for(i=1057;i<=1061;++i) {
                    HWND w = GetDlgItem(hwnd, i);
                    EnableWindow(w, FALSE);
                }
            } else {
                luastacksize(5);
                lua_getglobal(LUA, "editor_get_sound_options");
                lua_pushinteger(LUA, xinfo->inst);
                lua_pushboolean(LUA, 1);
                lua_pcall(LUA, 2, 4, 0);
                s = lua_tointeger(LUA, -1);            
                if (lua_toboolean(LUA, -2)) {
                    HWND cst = GetDlgItem(hwnd, ESB_PSSOUND);
                    HWND cst_field = GetDlgItem(hwnd, ESB_PSTYPESOUND);
                    EnableWindow(cst, TRUE);
                    EnableWindow(cst_field, TRUE);
                    if (s == 3) {
                        char *soundname = exs_exvar(xinfo->inst, "sound");
                        SetDlgItemText(hwnd, ESB_PSTYPESOUND, soundname);
                        SendMessage(cst, BM_SETCHECK, BST_CHECKED, 0);
                    }
                }            
                if (lua_toboolean(LUA, -3)) {
                    HWND bzz = GetDlgItem(hwnd, ESB_PSSBUZZ);
                    EnableWindow(bzz, TRUE);
                    if (s == 2)
                        SendMessage(bzz, BM_SETCHECK, BST_CHECKED, 0);
                }            
                if (lua_toboolean(LUA, -4)) {
                    HWND sil = GetDlgItem(hwnd, ESB_PSSILENT);
                    EnableWindow(sil, TRUE);
                    if (s == 1)
                        SendMessage(sil, BM_SETCHECK, BST_CHECKED, 0);
                }            
                lua_pop(LUA, 4);
            }             
        } break;
        
        case WM_CLOSE: {
            ESB_go_up_one(hwnd);
        } break;
        
        case WM_DESTROY: {
            struct ext_hwnd_info *xinfo = get_hwnd(hwnd);
            if (xinfo->b_buf)
                destroy_bitmap(xinfo->b_buf);
            del_hwnd(hwnd);
        } break;
                
        default:
            RETURN(FALSE);
    }

    RETURN(TRUE);
}

void ed_fill_from_ttargtable_hz(int usehz, HWND shwnd, int dlgitem, int inst) {
    onstack("ed_fill_from_ttargtable");
    if (usehz) {
        stackbackc('_');
        v_onstack("hz");
    }
    SendDlgItemMessage(shwnd, dlgitem, LB_RESETCONTENT, 0, 0);
    edg.op_hwnd = shwnd;
    edg.op_b_id = dlgitem;
    edg.op_winmsg = LB_ADDSTRING;
    if (usehz)
        edg.op_hwnd_extent = 1;       
             
    lc_parm_int("__ed_fill_from_ttargtable", 1, inst); 
    
    if (usehz) {
        SendDlgItemMessage(shwnd, dlgitem, LB_SETHORIZONTALEXTENT,
            edg.op_hwnd_extent, 0); 
        edg.op_hwnd_extent = 0;
    }
    VOIDRETURN(); 
}

void ed_fill_from_ttargtable(HWND shwnd, int dlgitem, int inst) {
    ed_fill_from_ttargtable_hz(0, shwnd, dlgitem, inst);
}

int ed_get_selected_msg(HWND shwnd) {
    int ch_it;
    int checkeditem = 0;
    int op_msg = -1;
    
    onstack("ed_get_selected_msg");
    
    for(ch_it=ESB_MACTIVATE;ch_it<=ESB_MUSER;ch_it++) {
        int ischk = SendDlgItemMessage(shwnd, ch_it, BM_GETCHECK, 0, 0);
        if (ischk) {
            switch (ch_it) {
                case ESB_MACTIVATE: op_msg = 100000; break;
                case ESB_MDEACTIVATE: op_msg = 100001; break;
                case ESB_MTOGGLE: op_msg = 100002; break;
                case ESB_MNEXTTICK: op_msg = 100003; break;
                case ESB_MRESET: op_msg = 100005; break;
                case ESB_MCLEANUP: op_msg = 100007; break;
                case ESB_MDESTROY: op_msg = 100255; break;
                case ESB_MEXPIRE: op_msg = 100010; break;
                case ESB_MSPECIAL: op_msg = 0; break;
                case ESB_MUSER: {
                    char udmsg[100];
                    HWND mu = GetDlgItem(shwnd, ESB_MUSERPICK);
                    int cs = SendMessage(mu, CB_GETCURSEL, 0, 0);
                    SendMessage(mu, CB_GETLBTEXT, cs, (int)udmsg);
                    luastacksize(3);
                    lua_getglobal(LUA, "__ed_user_def_string_msg");
                    lua_pushstring(LUA, udmsg);
                    op_msg = lc_call_topstack(1, "ed_user_def_string");
                }                
            }
            // found one, exit the loop
            break;
        }
    }
    RETURN(op_msg);    
}

// target picker
INT_PTR CALLBACK ESB_target_picker_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    int i_res;

    onstack("ESB_target_picker_proc");

    switch(message) { 
        case EDM_DVKEY: {
            if (wParam == 'Q')
                PostMessage(hwnd, WM_COMMAND, ESB_TLGOUP, 0);
            else if (wParam == 'E') 
                PostMessage(hwnd, WM_COMMAND, ESB_TLGODN, 0);   
        } break;
        
        case EDM_TARGETPICK: {
            int ntinst = wParam;
            int delay = GetDlgItemInt(hwnd, ESB_DELAYVAL, NULL, FALSE);
            int msg = ed_get_selected_msg(hwnd);
            int data = -1;
            
            if (SendDlgItemMessage(hwnd, ESB_D_CUSTOM, BM_GETCHECK, 0, 0)) {
                data = GetDlgItemInt(hwnd, ESB_CUSTOMVAL, NULL, FALSE);
            }
            
            if (msg > -1) {
                struct ext_hwnd_info *xinfo;
                xinfo = get_hwnd(hwnd);
                lc_parm_int("__ed_new_ttargeted_inst", 4, ntinst, msg, delay, data);
                ed_fill_from_ttargtable_hz(1, hwnd, ESB_TTARGETS, xinfo->inst);   
            }            
        } break;
        
        case WM_COMMAND: {
            struct ext_hwnd_info *xinfo;
            int lw = LOWORD(wParam);
            xinfo = get_hwnd(hwnd);
            if (lw == ESB_TLGOUP) {
                int pl = (xinfo->plevel - 1);
                if (pl >= 0) {
                    HWND dl = GetDlgItem(hwnd, ESB_TILEVEL);
                    xinfo->plevel = pl;
                    do_level_settings(hwnd, xinfo, dl);
                }    
            } else if (lw == ESB_TLGODN) {
                int pl = (xinfo->plevel + 1);
                if (pl < gd.dungeon_levels) {
                    HWND dl = GetDlgItem(hwnd, ESB_TILEVEL);
                    xinfo->plevel = pl;
                    do_level_settings(hwnd, xinfo, dl);
                } 
            } else if (lw == ESB_TTARGETS) {
                int hw = HIWORD(wParam);
                if (hw == LBN_SELCHANGE) {
                    int tinst;
                    int sel = SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0);
                    if (sel >= 0) {
                        tinst = lc_parm_int("__ed_get_ttargeted_inst", 1, sel+1);
                        edg.op_x = -1;
                        if (oinst[tinst]) {
                            struct inst *p_t_inst = oinst[tinst];
                            if (p_t_inst->level >= 0) {
                                edg.op_p_lev = p_t_inst->level;
                                edg.op_x = p_t_inst->x;
                                edg.op_y = p_t_inst->y;  
                            }
                        }   
                    }
                    force_redraw(xinfo->b_buf_win, 1);
                }
            } else if (lw == ESB_TDELETE) {
                HWND box = GetDlgItem(hwnd, ESB_TTARGETS);
                int sel = SendMessage(box, LB_GETCURSEL, 0, 0);
                int cmsg = ESB_TTARGETS | (LBN_SELCHANGE << 16);
                if (sel >= 0) {
                    lc_parm_int("__ed_remove_ttargeted_inst", 1, sel+1);
                    ed_fill_from_ttargtable_hz(1, hwnd, ESB_TTARGETS, xinfo->inst);
                    edg.op_x = -1;
                    if (sel > 0) --sel;
                    SendMessage(box, LB_SETCURSEL, sel, 0);
                    SendMessage(hwnd, WM_COMMAND, cmsg, (int)box);
                }
            } else if (lw == ESB_TFFIND) {
                int tinst;
                HWND box = GetDlgItem(hwnd, ESB_TTARGETS);
                int sel = SendMessage(box, LB_GETCURSEL, 0, 0);
                if (sel >= 0) {
                    tinst = lc_parm_int("__ed_get_ttargeted_inst", 1, sel+1);
                    if (oinst[tinst]) {
                        struct inst *p_t_inst = oinst[tinst];
                        if (p_t_inst->level >= 0) {
                            HWND bb = GetDlgItem(hwnd, ESB_TPICKWINDOW);
                            HWND dl = GetDlgItem(hwnd, ESB_TILEVEL);
                            xinfo->plevel = p_t_inst->level;
                            
                            reset_scroll_coordinates(bb);
                            do_level_settings(hwnd, xinfo, dl);
                            scroll_picker_to_coordinate(bb,
                                p_t_inst->x, p_t_inst->y);
                            RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
                        }
                    }
                }
            } else if (lw == ESB_TSET) {
                HWND box = GetDlgItem(hwnd, ESB_TTARGETS);
                int sel = SendMessage(box, LB_GETCURSEL, 0, 0);
                if (sel >= 0) {
                    int delay = GetDlgItemInt(hwnd, ESB_DELAYVAL, NULL, FALSE);
                    int msg = ed_get_selected_msg(hwnd);
                    int data = -1;
                    
                    if (SendDlgItemMessage(hwnd, ESB_D_CUSTOM, BM_GETCHECK, 0, 0)) {
                        data = GetDlgItemInt(hwnd, ESB_CUSTOMVAL, NULL, FALSE);
                    }
            
                    lc_parm_int("__ed_retarget_ttargeted_inst", 4, sel+1, msg, delay, data);
                    ed_fill_from_ttargtable_hz(1, hwnd, ESB_TTARGETS, xinfo->inst);
                    SendMessage(box, LB_SETCURSEL, sel, 0);
                    
                }
            } else if (lw >= ESB_SSILENT && lw <= ESB_SCUSTOM) {
                int p;
                for (p=ESB_SSILENT;p<=ESB_SCUSTOM;++p) {
                    if (p == lw)
                        SendDlgItemMessage(hwnd, p, BM_SETCHECK, BST_CHECKED, 0);
                    else
                        SendDlgItemMessage(hwnd, p, BM_SETCHECK, BST_UNCHECKED, 0);
                } 
                
            } else if (lw >= ESB_MACTIVATE && lw <= ESB_MUSER) {
                int p;
                for (p=ESB_MACTIVATE;p<=ESB_MUSER;++p) {
                    if (p == lw)
                        SendDlgItemMessage(hwnd, p, BM_SETCHECK, BST_CHECKED, 0);
                    else
                        SendDlgItemMessage(hwnd, p, BM_SETCHECK, BST_UNCHECKED, 0);
                }      
                
            } else if (lw >= ESB_D_DEFAULT && lw <= ESB_D_CUSTOM) {
                HWND wn = GetDlgItem(hwnd, ESB_CUSTOMVAL);
                int p;
                int env = FALSE;
                
                if (lw == ESB_D_CUSTOM)
                    env = TRUE;
                EnableWindow(wn, env);
                
                for (p=ESB_D_DEFAULT;p<=ESB_D_CUSTOM;++p) {
                    if (p == lw)
                        SendDlgItemMessage(hwnd, p, BM_SETCHECK, BST_CHECKED, 0);
                    else
                        SendDlgItemMessage(hwnd, p, BM_SETCHECK, BST_UNCHECKED, 0);
                }               
                        
            } else if (lw == IDOK || lw == ESB_TLP_OK) {
                int silent = SendDlgItemMessage(hwnd, ESB_SSILENT, BM_GETCHECK, 0, 0);
                int defclick = SendDlgItemMessage(hwnd, ESB_SDCLICK, BM_GETCHECK, 0, 0);
                int ssound = SendDlgItemMessage(hwnd, ESB_SCUSTOM, BM_GETCHECK, 0, 0);
                char dtext[100] = { 0 };
                
                if (xinfo->q_r_options == 2) {
                    int pr = GetDlgItemInt(hwnd, ESB_REPEATVAL, NULL, FALSE);
                    if (pr < 1 || pr > 100) {
                        MessageBox(hwnd, "Probability must be between 1 and 100.", "Parameter Error", MB_ICONEXCLAMATION);
                        RETURN(TRUE);
                    }                        
                }
                
                if (ssound) {
                    GetDlgItemText(hwnd, ESB_TYPESOUND, dtext, sizeof(dtext));
                    if (*dtext == '\0') {
                        MessageBox(hwnd, "You must specify a sound name.", "Sound Error", MB_ICONEXCLAMATION);
                        RETURN(TRUE);
                    }
                }        
                luastacksize(8);      
                lua_getglobal(LUA, "editor_store_sound_exvars");
                lua_pushinteger(LUA, xinfo->inst);
                lua_pushboolean(LUA, 0);
                lua_pushboolean(LUA, silent);
                lua_pushboolean(LUA, defclick);
                lua_pushboolean(LUA, ssound);
                lua_pushstring(LUA, dtext); 
                lc_call_topstack(6, "editor_store_sound_exvars");
                
                lua_getglobal(LUA, "editor_exvar_target_picker");
                lua_pushinteger(LUA, xinfo->inst);
                lua_getglobal(LUA, "__TTARGTABLE");
                lc_call_topstack(2, "editor_exvar_target_picker");
                
                lua_pushnil(LUA);
                lua_setglobal(LUA, "__TTARGTABLE");
                
                if (xinfo->q_r_options == 1) {
                    int rep = GetDlgItemInt(hwnd, ESB_REPEATVAL, NULL, FALSE);
                    lc_parm_int("__ed_repeat_store", 2, xinfo->inst, rep);
                } else if (xinfo->q_r_options == 2) {
                    int pr = GetDlgItemInt(hwnd, ESB_REPEATVAL, NULL, FALSE);
                    lc_parm_int("__ed_probability_store", 2, xinfo->inst, pr);
                }
                  
                ESB_go_up_one(hwnd);    
                
            } else if (lw == IDCANCEL || lw == ESB_TLP_CAN) {
                ESB_go_up_one(hwnd);
            }    
        } break; 
              
        case WM_INITDIALOG: {
            int special_only = 0;
            struct ext_hwnd_info *xinfo;
            struct inst *p_inst;
            struct obj_arch *p_arch;
            HWND dl = GetDlgItem(hwnd, ESB_TILEVEL);
            HWND bb = GetDlgItem(hwnd, ESB_TPICKWINDOW);
            int s;
            int special_mode = 0;
            
            edg.allow_control = GetDlgItem(hwnd, ESB_TYPESOUND);
                        
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            add_xinfo_coordinates(hwnd, xinfo, 1);
            xinfo->wtype = edg.op_picker_val;
            xinfo->b_buf = NULL;
            xinfo->b_buf_win = bb;
            p_inst = oinst[xinfo->inst];
            p_arch = Arch(p_inst->arch);
            if (p_inst->level >= 0)
                edg.op_p_lev = p_inst->level;
            xinfo->plevel = edg.op_p_lev;
            SendMessage(dl, WM_SETFONT, 0, 0);       
            do_level_settings(hwnd, xinfo, dl);
            if (p_inst->level >= 0)
                scroll_picker_to_coordinate(bb, p_inst->x, p_inst->y);
            SetFocus(hwnd);            
            SetDlgItemInt(hwnd, ESB_DELAYVAL, 0, FALSE);    
        
            lc_parm_int("__ed_target_exvars_to_ttargtable", 1, xinfo->inst);
            ed_fill_from_ttargtable_hz(1, hwnd, ESB_TTARGETS, xinfo->inst);
            
            if (edg.q_r_options) {
                int rv = 0;
                if (edg.q_r_options == 1) {
                    ex_exvar(xinfo->inst, "repeat_rate", &rv);
                    SetDlgItemText(hwnd, ESB_REPEATBOX, "Repeat Rate"); 
                } else if (edg.q_r_options == 2) {
                    int pr_get = ex_exvar(xinfo->inst, "probability", &rv);
                    if (!pr_get)
                        rv = 100;
                    SetDlgItemText(hwnd, ESB_REPEATBOX, "% Chance");     
                }
                HWND box = GetDlgItem(hwnd, ESB_REPEATBOX);
                HWND item = GetDlgItem(hwnd, ESB_REPEATVAL);
                EnableWindow(box, TRUE);
                EnableWindow(item, TRUE);
                SetDlgItemInt(hwnd, ESB_REPEATVAL, rv, FALSE);
                xinfo->q_r_options = edg.q_r_options;
            }
            
            if (queryluapresence(p_arch, "esb_targ_draw_color")) {
                int n;
                HWND sw = GetDlgItem(hwnd, ESB_MSPECIAL);
                HWND dl = GetDlgItem(hwnd, ESB_DELAYBOX);
                HWND dli = GetDlgItem(hwnd, ESB_DELAYVAL);
                HWND sb;
                EnableWindow(sw, TRUE);
                SendMessage(sw, BM_SETCHECK, BST_CHECKED, 0);
                for(n=ESB_MACTIVATE;n<ESB_MSPECIAL;++n) {
                    HWND selb = GetDlgItem(hwnd, n);
                    EnableWindow(selb, FALSE);
                }
                special_only = 1;
                EnableWindow(dl, FALSE);
                EnableWindow(dli, FALSE);
                
                sb = GetDlgItem(hwnd, ESB_D_DEFAULT);
                EnableWindow(sb, FALSE);
                sb = GetDlgItem(hwnd, ESB_D_CUSTOM);
                EnableWindow(sb, FALSE);
                sb = GetDlgItem(hwnd, ESB_DATAGRPBOX);
                EnableWindow(sb, FALSE);
                special_mode = 1;
                
                sb = GetDlgItem(hwnd, ESB_TSET);
                EnableWindow(sb, FALSE);
                
            } else {
                int dmsg = queryluaint(p_arch, "default_msg");
                int dslot = ESB_MACTIVATE;
                if (dmsg == 100001) dslot = ESB_MDEACTIVATE;
                if (dmsg == 100002) dslot = ESB_MTOGGLE;
                if (dmsg == 100003) dslot = ESB_MNEXTTICK;
                SendDlgItemMessage(hwnd, dslot, BM_SETCHECK, BST_CHECKED, 0);
            }
            
            if (!special_only) {
                int udef;
                edg.op_hwnd = hwnd;
                edg.op_b_id = ESB_MUSERPICK;
                edg.op_winmsg = CB_ADDSTRING;                  
                udef = lc_parm_int("__ed_fill_userdefined_msgs", 0);
                if (udef > 0) {
                    HWND udefb = GetDlgItem(hwnd, ESB_MUSER);
                    HWND udef_pick = GetDlgItem(hwnd, ESB_MUSERPICK);
                    EnableWindow(udefb, TRUE); 
                    EnableWindow(udef_pick, TRUE); 
                    SendMessage(udef_pick, CB_SETCURSEL, 0, 0);                  
                }
            }
            
            luastacksize(5);
            lua_getglobal(LUA, "editor_get_sound_options");
            lua_pushinteger(LUA, xinfo->inst);
            lua_pushboolean(LUA, 0);
            lua_pcall(LUA, 2, 4, 0);
            s = lua_tointeger(LUA, -1);            
            if (lua_toboolean(LUA, -2)) {
                HWND cst = GetDlgItem(hwnd, ESB_SCUSTOM);
                HWND cst_field = GetDlgItem(hwnd, ESB_TYPESOUND);
                EnableWindow(cst, TRUE);
                EnableWindow(cst_field, TRUE);
                if (s == 3) {
                    char *soundname = exs_exvar(xinfo->inst, "sound");
                    SetDlgItemText(hwnd, ESB_TYPESOUND, soundname);
                    SendMessage(cst, BM_SETCHECK, BST_CHECKED, 0);
                }
            }            
            if (lua_toboolean(LUA, -3)) {
                HWND clk = GetDlgItem(hwnd, ESB_SDCLICK);
                EnableWindow(clk, TRUE);
                if (s == 2)
                    SendMessage(clk, BM_SETCHECK, BST_CHECKED, 0);
            }            
            if (lua_toboolean(LUA, -4)) {
                HWND sil = GetDlgItem(hwnd, ESB_SSILENT);
                EnableWindow(sil, TRUE);
                if (s == 1)
                    SendMessage(sil, BM_SETCHECK, BST_CHECKED, 0);
            }            
            lua_pop(LUA, 4);  
            
            if (!special_mode) {
                SendDlgItemMessage(hwnd, ESB_D_DEFAULT, BM_SETCHECK, BST_CHECKED, 0);
            }
            
        } break;
        
        case WM_SIZE: {
            ed_modify_pickwin_size(hwnd, ESB_TPICKER_SB,
                ESB_TLGOUP, ESB_TYPESOUND, ESB_TPICKWINDOW);
        } break;  
        
        case WM_CLOSE: {
            ESB_go_up_one(hwnd);
        } break;
        
        case WM_DESTROY: {
            struct ext_hwnd_info *xinfo = get_hwnd(hwnd);
            if (xinfo->b_buf)
                destroy_bitmap(xinfo->b_buf);
            del_hwnd(hwnd);
        } break;
                
        default:
            RETURN(FALSE);
    }

    RETURN(TRUE);
}

INT_PTR CALLBACK ESB_spinedit_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo;
    
    onstack("ESB_spinedit_proc");

    switch(message) { 
        case WM_COMMAND: {
            int lw = LOWORD(wParam);
            if (lw == IDCANCEL)
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            else if (lw == IDOK) {
                int spsel;
                for (spsel=0;spsel<=7;++spsel) {
                    if (SendDlgItemMessage(hwnd, ESB_SP_FUNCHANGED + spsel, BM_GETCHECK, 0, 0))
                        break;
                }
                xinfo = get_hwnd(hwnd);
                luastacksize(3);
                lua_getglobal(LUA, "__ed_spined_store");
                lua_pushinteger(LUA, xinfo->inst);
                lua_pushinteger(LUA, spsel);
                if (lua_pcall(LUA, 2, 0, 0))
                    bomb_lua_error(NULL);                  
                
                lstacktop();
                SendMessage(hwnd, WM_CLOSE, 0, 0);      
            }
        } break; 
              
        case WM_INITDIALOG: {
            int found;
            int face = 0;
            int spin = 2;
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            found = ex_exvar(xinfo->inst, "face", &face);
            if (found && face >= 0) {
                SendDlgItemMessage(hwnd, ESB_SP_NORTH + (face % 4), BM_SETCHECK, BST_CHECKED, 0);
            } else {
                found = ex_exvar(xinfo->inst, "spin", &spin);
                if (found && spin > 0) {
                    if (spin > 3) spin = 0;
                    SendDlgItemMessage(hwnd, ESB_SP_1 + spin - 1, BM_SETCHECK, BST_CHECKED, 0);
                } else
                    SendDlgItemMessage(hwnd, ESB_SP_FUNCHANGED, BM_SETCHECK, BST_CHECKED, 0);
                    
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
