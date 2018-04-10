#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <commctrl.h>
#include "E_res.h"
#include "objects.h"
#include "global_data.h"
#include "editor.h"
#include "editor_hwndptr.h"
#include "editor_gui.h"
#include "editor_menu.h"


#define NUM_POPUPS  4

extern HWND sys_hwnd;
extern char ESBFloatClassName[];
extern char ESBLocClassName[];
extern struct editor_global edg;
HINSTANCE global_instance;

void ed_resizesubs(HWND ehwnd) {
    int i_mainw;
    int i_calch;
    int infobarheight;
    int sw[2] = { 100, -1 };
    RECT r_client;
    int viewheight = 0;
    int treeheight = 0;
    
    onstack("ed_resizesubs");

    SendMessage(edg.infobar, WM_SIZE, 0, 0);
    
    GetWindowRect(edg.infobar, &r_client);
    infobarheight = r_client.bottom - r_client.top;
    
    sw[0] = ((r_client.right - r_client.left)*5) / 8;
    SendMessage(edg.infobar, SB_SETPARTS, 2, (LPARAM)sw);

    GetClientRect(ehwnd, &r_client);
    i_mainw = r_client.right - (r_client.left + TREEWIDTH);
    i_calch = r_client.bottom - (r_client.top + infobarheight);
    viewheight = (i_calch / 4) - 2;
    treeheight = i_calch - (viewheight + CTRLHEIGHT + 2);
    
    if (edg.toolbars_floated) {
        i_mainw = r_client.right - r_client.left;
    } else {
        SetWindowPos(edg.treeview, NULL, i_mainw, CTRLHEIGHT,
            TREEWIDTH, treeheight, SWP_NOZORDER);
            
        SetWindowPos(edg.tile_list, NULL, i_mainw, i_calch - viewheight,
            TREEWIDTH, viewheight - 2, SWP_NOZORDER);
            
        SetWindowPos(edg.loc, NULL, i_mainw + 4, 2,
            CTRLREALSIZE, CTRLREALSIZE, SWP_NOZORDER);
            
        SetWindowPos(edg.coordview, NULL, i_mainw + 4 + CTRLREALSIZE + 2, 2,
            TREEWIDTH - (CTRLREALSIZE + 2), CTRLREALSIZE, SWP_NOZORDER);
    }
    
    SetWindowPos(edg.subwin, NULL, 0, 0, i_mainw, i_calch, SWP_NOZORDER);
    
    ed_scrollset(0, edg.ed_lev, edg.subwin, i_mainw, i_calch);
      
    InvalidateRect(edg.coordview, NULL, FALSE);
    UpdateWindow(edg.coordview);
    
    InvalidateRect(edg.subwin, NULL, TRUE);
    UpdateWindow(edg.subwin);
    
    VOIDRETURN();
}

void ed_resize_float(HWND *hwnd) {
    int h, w;
    RECT r_client;
    
    onstack("ed_resize_float");
    
    GetClientRect(hwnd, &r_client); 
    w = r_client.right - r_client.left;
    h = r_client.bottom - r_client.top;
    
    if (hwnd == edg.f_item_win) {
        SetWindowPos(edg.tile_list, NULL, 0, 0, w, h, SWP_NOZORDER);                
    } else {
        SetWindowPos(edg.treeview, NULL, 0, CTRLREALSIZE + 2, w, h - (CTRLREALSIZE + 2), SWP_NOZORDER); 
        
        SetWindowPos(edg.coordview, NULL, 6 + CTRLREALSIZE, 2,
            TREEWIDTH - (CTRLREALSIZE + 2), CTRLREALSIZE, SWP_NOZORDER);        
    }
    
    VOIDRETURN();    
}

void ed_create_float(void) {
    RECT r_client;
    POINT tool;
    POINT item;
    int maxheight;
    int toolw, toolh, itemw, itemh;
    int viewheight;
    
    onstack("ed_create_float");
    
    GetClientRect(sys_hwnd, &r_client);
    maxheight = (r_client.bottom - r_client.top) - 16;
    toolw = itemw = TREEWIDTH + 16;
    tool.x = item.x = r_client.right - (r_client.left + toolw);
    tool.y = -12;
    itemh = (maxheight / 4) + 16;
    toolh = maxheight - itemh;
    item.y = tool.y + toolh;
    
    ClientToScreen(sys_hwnd, &tool);
    ClientToScreen(sys_hwnd, &item);
    
    edg.f_tool_win = CreateWindowEx(WS_EX_TOOLWINDOW, ESBFloatClassName,
        "Archetypes",
        WS_VISIBLE | WS_POPUP | WS_SYSMENU | WS_THICKFRAME | WS_CAPTION,
        tool.x, tool.y, toolw, toolh, sys_hwnd, NULL, global_instance, NULL);
        
    edg.f_item_win = CreateWindowEx(WS_EX_TOOLWINDOW, ESBFloatClassName,
        "Tile Items",
        WS_VISIBLE | WS_POPUP | WS_SYSMENU | WS_THICKFRAME | WS_CAPTION,
        item.x, item.y, itemw, itemh, sys_hwnd, NULL, global_instance, NULL);
        
    edg.loc = CreateWindowEx(0, ESBLocClassName, NULL,
        WS_CHILD | WS_VISIBLE, 0, 0, CTRLREALSIZE, CTRLREALSIZE,
        edg.f_tool_win, NULL, global_instance, NULL);
                
    edg.treeview = CreateWindowEx(0, WC_TREEVIEW, NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER |
        TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        0, 0, 0, 0, edg.f_tool_win, NULL, global_instance, NULL);
        
    edg.coordview = CreateWindowEx(0, "STATIC", NULL,
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, edg.f_tool_win, NULL, global_instance, NULL);
    
    edg.tile_list = CreateWindowEx(WS_EX_CLIENTEDGE,
        "STATIC", NULL, WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, edg.f_item_win, NULL, global_instance, NULL);
        
    ed_resize_float(edg.f_tool_win);
    ed_resize_float(edg.f_item_win);
    
    SendMessage(sys_hwnd, WM_NCACTIVATE, 1, 0); 
        
    VOIDRETURN();    
}

void float_toolbars(void) {
    onstack("float_toolbars");

    ed_purge_treeview(edg.treeview);
    DestroyWindow(edg.loc);
    DestroyWindow(edg.treeview);
    DestroyWindow(edg.coordview);
    DestroyWindow(edg.tile_list);
    edg.loc = edg.treeview = edg.coordview = edg.tile_list = NULL;
    
    ed_create_float();
    setup_right_side_window_fonts();
    
    ed_build_tree(edg.treeview, 0, NULL);
    
    ed_resizesubs(sys_hwnd);
    VOIDRETURN();    
}

void unfloat_toolbars(void) {
    onstack("unfloat_toolbars");
    
    DestroyWindow(edg.f_tool_win);
    DestroyWindow(edg.f_item_win);
    edg.f_tool_win = edg.f_item_win = NULL;
    
    init_right_side_windows(global_instance);
    setup_right_side_window_fonts();
    
    ed_build_tree(edg.treeview, 0, NULL);
    
    ed_resizesubs(sys_hwnd);
    VOIDRETURN();
}

void init_right_side_windows(HINSTANCE hThisInstance) {
    onstack("init_right_side_windows");
    
    edg.loc = CreateWindowEx(0, ESBLocClassName, NULL,
        WS_CHILD | WS_VISIBLE, 0, 0, CTRLREALSIZE, CTRLREALSIZE,
        sys_hwnd, NULL, hThisInstance, NULL);
                
    edg.treeview = CreateWindowEx(0, WC_TREEVIEW, NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER |
        TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        0, 0, 0, 0, sys_hwnd, NULL, hThisInstance, NULL);
        
    edg.coordview = CreateWindowEx(0, "STATIC", NULL,
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, sys_hwnd, NULL, hThisInstance, NULL);
    
    edg.tile_list = CreateWindowEx(WS_EX_CLIENTEDGE,
        "STATIC", NULL, WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, sys_hwnd, NULL, hThisInstance, NULL);
        
    VOIDRETURN();
}

void setup_right_side_window_fonts(void) {
    onstack("setup_right_side_window_fonts");
    
    SendMessage(edg.coordview, WM_SETFONT, 0, 0);
    SendMessage(edg.tile_list, WM_SETFONT,
        (int)GetStockObject(DEFAULT_GUI_FONT), 0);

    VOIDRETURN();
}

LRESULT CALLBACK ed_sync_popups (HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam) 
{
    int n;
    HWND hw_popups[NUM_POPUPS]; 
    HWND l_hwnd = (HWND)lParam;
    int keep_active = !!(wParam);
    int sync_others = 1;
    
    onstack("ed_sync_popups");
    
    if (!edg.toolbars_floated) {
        RETURN(DefWindowProc(hwnd, message, wParam, lParam));
    }
    
    hw_popups[0] = sys_hwnd;
    hw_popups[1] = edg.f_tool_win;
    hw_popups[2] = edg.f_item_win;
    hw_popups[3] = NULL;
    
    for(n=0;n<NUM_POPUPS;++n) {
        if (hw_popups[n] && hw_popups[n] == l_hwnd) {
            keep_active = 1;
            sync_others = 0;
            break;
        }
    }
    
    if (lParam == -1) {
        RETURN(DefWindowProc(hwnd, message, keep_active, 0));    
    }
    
    if (sync_others) {
        for(n=0;n<NUM_POPUPS;++n) {
            if (hw_popups[n] && hw_popups[n] != hwnd) {
                if (hw_popups[n] != l_hwnd) {
                    SendMessage(hw_popups[n], WM_NCACTIVATE,
                        keep_active, -1);    
                }            
            }
        }        
    }
    
    
    RETURN(DefWindowProc(hwnd, message, keep_active, lParam));
}

LRESULT CALLBACK ESBtoolproc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    onstack("ESBtoolproc");

    switch (message) {
        
        case WM_SIZE:
            ed_resize_float(hwnd);
            break;
        
        case WM_NCACTIVATE: {
            RETURN(ed_sync_popups(hwnd, message, wParam, lParam)); 
        } break;
        
        case WM_CLOSE: {
            ed_menucommand(sys_hwnd, ESBM_FLOATWIN);   
        } break;
        
        case WM_NOTIFY: {
            NMHDR *ncmd = (NMHDR*)lParam;
            
            switch (ncmd->code) {
                case TVN_SELCHANGED:
                    clbr_sel(lParam);
                    break;
                    
                case TVN_DELETEITEM:
                    clbr_del(lParam);
                    break;
            }
            
        } break;
            
        case EDM_MAIN_TREE_WS_ADD:
            ed_m_main_tree_ws_add((const char*)lParam);
            break;
                                   
        default:
            RETURN(DefWindowProc (hwnd, message, wParam, lParam));
    }

    RETURN(0);
}

/*        

*/
    
