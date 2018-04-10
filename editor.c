#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <commctrl.h>
#include <signal.h>
#include "E_res.h"
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "compile.h"
#include "arch.h"
#include "editor.h"
#include "editor_hwndptr.h"
#include "editor_gui.h"
#include "editor_menu.h"

int CLOCKFREQ = 7;

char ESBClassName[ ] = "DSBED";
char ESBSubClassName[ ] = "EditorSub";
char ESBLocClassName[ ] = "LocatorWin";
char ESBFloatClassName[ ] = "ESBToolWindow";

extern struct ext_hwnd_info *ESBxh;

struct global_data gd;
FILE *errorlog;
HWND sys_hwnd;
struct inventory_info gii;
extern struct inst *oinst[];
extern HINSTANCE global_instance;

// common variables that may not be needed
unsigned int i_s_error_c;
struct clickzone *lua_cz;
int lua_cz_n;
FONT *ROOT_SYSTEM_FONT;

extern char dbuffer[1024];
extern struct dungeon_level *dun;
extern int cwsa_num;
extern int ws_tbl_len;

struct editor_global edg;
extern int debug;

void DSBallegshutdown(void) {
    onstack("DSBallegshutdown");
    allegro_exit();
    VOIDRETURN();
}

LRESULT CALLBACK ESBwproc (HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    unsigned short cmd_id;
    unsigned short cmd_type;
    NMHDR *ncmd;
    HDC hdc;
    
    onstack("ESBwproc");
    ESB_MESSAGE_DEBUG();
    
    switch (message) {
        case WM_CREATE:
            ed_menu_setup(hwnd);
            break;
            
        case WM_CLOSE:
            ed_quit_out();
            break;
            
        case WM_SIZE:
            ed_resizesubs(hwnd);
            break;
            
        case WM_ENABLE: {
            if (edg.toolbars_floated && !edg.in_purge) {
                EnableWindow(edg.f_tool_win, wParam);
                EnableWindow(edg.f_item_win, wParam);
            }
            RETURN(DefWindowProc (hwnd, message, wParam, lParam));    
        } break;
            
        case WM_NCACTIVATE: {
            RETURN(ed_sync_popups(hwnd, message, wParam, lParam)); 
        } break;
            
        case WM_COMMAND:
            if (!edg.in_purge) {
                cmd_type = HIWORD(wParam);
                cmd_id = LOWORD(wParam);
                ed_menucommand(hwnd, cmd_id);
            }
            break;

        case WM_NOTIFY:
            ncmd = (NMHDR*)lParam;
            
            switch (ncmd->code) {
                case TVN_SELCHANGED:
                    clbr_sel(lParam);
                    break;
                    
                case TVN_DELETEITEM:
                    clbr_del(lParam);
                    break;
            }
            
            break;
            
        case EDM_MAIN_TREE_WS_ADD:
            if (!edg.in_purge)
                ed_m_main_tree_ws_add((const char*)lParam);
            break;

        default:
            RETURN(DefWindowProc (hwnd, message, wParam, lParam));
    }

    RETURN(0);
}

LRESULT CALLBACK ESBswproc (HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    HDC hdc;
    SCROLLINFO sbar;
    int i_base;
    int i_sd;
    int xx, yy;
    HWND phwnd = GetParent(hwnd);
    struct ext_hwnd_info *xinfo;

    onstack("ESBswproc");
    
    sbar.cbSize = sizeof(SCROLLINFO);
    xinfo = get_hwnd(phwnd);

    switch (message) {
        case EDM_VOID_PSEL: {
            xinfo->flags &= ~(XF_PSEL);
        } break;
        
        case WM_LBUTTONDOWN: {
            if (edg.draw_mode != DRM_DONOTHING) {
                xx = LOWORD(lParam);
                yy = HIWORD(lParam);
                int pc = 0;
                if (xinfo && xinfo->wtype) {
                    if (!(xinfo->wtype & XW_NO_CLICKS)) {
                        pc = ed_picker_click(xinfo->wtype, hwnd,
                            xinfo->inst, xinfo->plevel, xx, yy);
                    }
                } else {
                    ed_window_mouseclick(hwnd, xx, yy, 0);
                    xinfo->flags |= XF_PSEL;
                }
                // the picker window must be an ESB modal, not a windows one!
                if (pc) {
                    ESB_go_up_one(phwnd);
                }
            }
                           
        } break;
            
        case WM_LBUTTONUP: {
            if (edg.draw_mode != DRM_DONOTHING) {
                if (xinfo && !xinfo->wtype &&  
                    xinfo->flags & XF_PSEL) 
                {
                    xx = LOWORD(lParam);
                    yy = HIWORD(lParam);
                    
                    edg.sbmp = xinfo->b_buf;
                    ed_get_uzonetype(hwnd, xx, yy);
                    xinfo->flags &= ~(XF_PSEL);
                    edg.sbmp = NULL;
                }
            }
        } break;    
            
        case WM_RBUTTONDOWN: {
            if (xinfo && !xinfo->wtype) {
                xx = LOWORD(lParam);
                yy = HIWORD(lParam);
                ed_window_rightclick(hwnd, xx, yy, wParam);
            }
        } break;
            
        case WM_MOUSEMOVE: {
            if (edg.draw_mode != DRM_DONOTHING) {
                if (xinfo && !xinfo->wtype) {
                    xx = LOWORD(lParam);
                    yy = HIWORD(lParam);
                    
                    edg.lmm_lparam = lParam;
        
                    edg.sbmp = xinfo->b_buf;
                    if ((wParam & MK_LBUTTON) && 
                        (xinfo->flags & XF_PSEL))
                    {
                        if (edg.draw_mode == DRM_DRAWDUN) {
                            ed_window_mouseclick(hwnd, xx, yy, 1);
                        } else if (edg.draw_mode == DRM_WALLSET && edg.draw_ws) {
                            ed_window_mouseclick(hwnd, xx, yy, 1);
                        }
                    } 
                    ed_get_uzonetype(hwnd, xx, yy);
                    edg.sbmp = NULL;
                }    
            }
        } break;

        case WM_PAINT: {
            if (xinfo)
                ESB_dungeon_windowpaint(hwnd, xinfo->b_buf);
        } break;
            
        case WM_HSCROLL:
            sbar.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_HORZ, &sbar);
            i_base = sbar.nPos;
            
            switch (LOWORD(wParam)) {
                case SB_LINELEFT:
                    sbar.nPos -= 1;
                    break;
                    
                case SB_PAGELEFT:
                    sbar.nPos -= sbar.nPage;
                    break;
                    
                case SB_LINERIGHT:
                    sbar.nPos += 1;
                    break;
                    
                case SB_PAGERIGHT:
                    sbar.nPos += sbar.nPage;
                    break;
                    
                case SB_THUMBTRACK:
                    sbar.nPos = sbar.nTrackPos;
                    break;
            }
            
            sbar.fMask = SIF_POS;
            SetScrollInfo(hwnd, SB_HORZ, &sbar, TRUE);
            GetScrollInfo(hwnd, SB_HORZ, &sbar);
            if (i_base != sbar.nPos) {
                ScrollWindow(hwnd, i_base - sbar.nPos, 0, NULL, NULL);
                UpdateWindow(hwnd);
            }
            
            break;
            
        case WM_VSCROLL:
            sbar.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &sbar);
            i_base = sbar.nPos;

            switch (LOWORD(wParam)) {
                case SB_LINEUP:
                    sbar.nPos -= 1;
                    break;
                    
                case SB_PAGEUP:
                    sbar.nPos -= sbar.nPage;
                    break;

                case SB_LINEDOWN:
                    sbar.nPos += 1;
                    break;
                    
                case SB_PAGEDOWN:
                    sbar.nPos += sbar.nPage;
                    break;

                case SB_THUMBTRACK:
                    sbar.nPos = sbar.nTrackPos;
                    break;
            }
            
            sbar.fMask = SIF_POS;
            SetScrollInfo(hwnd, SB_VERT, &sbar, TRUE);
            GetScrollInfo(hwnd, SB_VERT, &sbar);
            if (i_base != sbar.nPos) {
                ScrollWindow(hwnd, 0, i_base - sbar.nPos, NULL, NULL);
                UpdateWindow(hwnd);
            }
            break;
            
        case WM_COMMAND: {
            int cmd_type = HIWORD(wParam);
            int cmd_id = LOWORD(wParam);
            ed_menucommand(hwnd, cmd_id);
        } break;

        default:
            RETURN(DefWindowProc (hwnd, message, wParam, lParam));
    }

    RETURN(0);
}

int zone_do_autosel(int fv, int cancenter, int ortho, int x, int y) {
    int hv = fv / 2;
    int zcv = 0;
    int zch = 0;
    
    if (cancenter) {
        int tv = fv / 3;
        int t2v = (fv * 2) / 3;    
        if (x > tv && x < t2v && y > tv && y < t2v)
            return 4;
    }
    
    if (ortho) {
        if (y > hv) zcv = 1;      
        if (x > hv) zch = 1;
    } else {
        if (x < y) zcv = 1;
        if (x > (fv - y)) zch = 1;
    }
    
    if (zcv && !zch) return(3);
    return (zcv + zch);   
}

int determine_zoneclick(int za, int x, int y) {
    int zcv = 0;
    int zch = 0;
    
    onstack("determine_zoneclick");
    
    if (za < 2 || za > 6)
        RETURN(-1);    

    // that's the only zone
    if (za == 6)
        RETURN(4);
        
    RETURN(zone_do_autosel(65, (za >= 4),
        (za == 3 || za == 4), x, y));
}

LRESULT CALLBACK ESBlocproc (HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT y_paint;
    HDC hdc;
    
    onstack("ESBlocproc");

    switch (message) {
        case WM_LBUTTONDOWN: {
            int xx = LOWORD(lParam);
            int yy = HIWORD(lParam);
            if (hwnd == edg.loc) {
                int zt, zc;
                zt = locw_update(edg.loc, NULL);           
                zc = determine_zoneclick(zt, xx, yy);
                if (zc != -1) {
                    edg.d_dir = zc;
                    locw_update(edg.loc, NULL);
                }
            } else {
                int zmt;
                HWND phwnd = GetParent(hwnd);
                struct ext_hwnd_info *xinfo = get_hwnd(phwnd);
                zmt = determine_zoneclick(xinfo->zm, xx, yy);
                if (zmt != -1) {
                    ed_move_tile_position(xinfo->inst, zmt); 
                    xinfo->zm = locw_update(hwnd, xinfo);      
                }
            } 
                               
        }
        break;
            
        case WM_PAINT: {
            hdc = BeginPaint(hwnd, &y_paint);

            if (hwnd == edg.loc)
                locw_draw(hwnd, hdc, NULL);
            else {
                HWND phwnd = GetParent(hwnd);
                struct ext_hwnd_info *xinfo = get_hwnd(phwnd);
                if (xinfo && xinfo->inst)
                    locw_draw(hwnd, hdc, xinfo);
            }
                
            EndPaint(hwnd, &y_paint);
        }
        break;
            
        default:
            RETURN(DefWindowProc (hwnd, message, wParam, lParam));
    }

    RETURN(0);
}

int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int CRAP)

{
    MSG messages;            /* Here messages to the application are saved */
    WNDCLASSEX wincl;        /* Data structure for the windowclass */
    WNDCLASSEX wsmallc;
    WNDCLASSEX wtoolc;
    WNDCLASSEX wlocc;
    HBRUSH blackbrush;
    HRESULT hr_c;
    int doing;
    
    ACCEL myAccel[] = {
        { 0, '1', ESBA_MODE1 },
        { 0, '2', ESBA_MODE2 },
        { 0, '3', ESBA_MODE3 },
        { 0, '4', ESBA_MODE4 },
        { 0, '5', ESBA_MODE5 },
        { FVIRTKEY, VK_SPACE, ESBA_EDITLAST },
        { FVIRTKEY, 'Q', ESBM_PREVL },
        { FVIRTKEY, 'E', ESBM_NEXTL },
        { FVIRTKEY, 'W', ESBM_LEVELINFO },
        { FVIRTKEY, 'A', ESBM_MODE_AI },
        { FVIRTKEY, 'G', ESBM_MODE_WSG },
        { FVIRTKEY, 'D', ESBM_MODE_DD },
        { FVIRTKEY, 'Y', ESBM_MODE_PRP },
        { FVIRTKEY, 'X', ESBM_MODE_CUT },
        { FCONTROL | FVIRTKEY, 'X', ESBM_MODESWAP_DO_CUT },
        { FCONTROL | FVIRTKEY, 'C', ESBM_MODESWAP_DO_COPY },
        { FCONTROL | FVIRTKEY, 'V', ESBM_MODESWAP_DO_PASTE },
        { FCONTROL | FVIRTKEY, 'N', ESBM_NEW },
        { FCONTROL | FVIRTKEY, 'O', ESBM_OPEN },
        { FCONTROL | FVIRTKEY, 'S', ESBM_SAVE },
        { FCONTROL | FVIRTKEY, 'R', ESBM_RELOADARCH },
        { FCONTROL | FVIRTKEY, 'F', ESBM_SEARCHFIND }
    };
    
    COLORREF cl_Black = 0;

    signal(SIGSEGV, sigcrash);
    signal(SIGILL, sigcrash);
    signal(SIGFPE, sigcrash);

    init_stack();
    errorlog = fopen("log.txt", "w");

    onstack("ESBmain");

    install_allegro(SYSTEM_NONE, &errno, atexit);

    three_finger_flag = 1;
    memset(&gd, 0, sizeof(struct global_data));
    memset(&gii, 0, sizeof(struct inventory_info));
    dun = NULL;
    cwsa_num = 0;
    
    gd.edit_mode = 1;
    
    gd.session_seed = clock() + time(NULL);
	DSBsrand(gd.session_seed);
    gd.exestate = STATE_LOADING;
    gd.p_lev[0] = gd.p_lev[1] = gd.p_lev[2] = gd.p_lev[3] = LOC_LIMBO;
    gd.who_look = -1;
    gd.leader = -1;
    gd.gl = NULL;
    ESBxh = NULL;

    ws_tbl_len = 0;
    
    /*
    MessageBox(NULL, "Unimplemented", "Unimplemented", MB_ICONEXCLAMATION);
    RETURN(0);
    //////////
    */
    
    memset(&edg, 0, sizeof(struct editor_global));
    
    edg.runmode = EDITOR_LOADING;
    edg.ed_lev = -5;
    edg.editor_flags = 255;
    
    InitCommonControls();
    hr_c = CoInitialize(NULL);
    if (hr_c != S_OK) {
        if (hr_c == S_FALSE)
            CoUninitialize();
        else
            poop_out("Unable to CoInitialize.");
    }
    
    memset(&wincl, 0, sizeof(WNDCLASSEX));
    memset(&wsmallc, 0, sizeof(WNDCLASSEX));
    memset(&wlocc, 0, sizeof(WNDCLASSEX));
    memset(&wtoolc, 0, sizeof(WNDCLASSEX));
    blank_translation_table();
    
    wincl.hInstance = hThisInstance;
    wincl.lpszClassName = ESBClassName;
    wincl.lpfnWndProc = ESBwproc;      /* This function is called by windows */
    wincl.style = CS_DBLCLKS;                 /* Catch double-clicks */
    wincl.cbSize = sizeof(WNDCLASSEX);
    wincl.hIcon = LoadIcon(hThisInstance, MAKEINTRESOURCE(ESB_ICON));
    wincl.hIconSm = LoadIcon(hThisInstance, MAKEINTRESOURCE(ESB_ICON));
    wincl.hCursor = LoadCursor(NULL, IDC_ARROW);
    wincl.lpszMenuName = NULL;
    wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
    wincl.cbWndExtra = 0;                      /* structure or the window instance */
    wincl.hbrBackground = (HBRUSH)COLOR_BACKGROUND;
    
    blackbrush = CreateSolidBrush(cl_Black);

    wsmallc.hInstance = hThisInstance;
    wsmallc.lpszClassName = ESBSubClassName;
    wsmallc.lpfnWndProc = ESBswproc;      
    wsmallc.cbSize = sizeof(WNDCLASSEX);
    wsmallc.hCursor = LoadCursor(NULL, IDC_ARROW);              
    wsmallc.hbrBackground = blackbrush;
    
    wlocc.hInstance = hThisInstance;
    wlocc.lpszClassName = ESBLocClassName;
    wlocc.lpfnWndProc = ESBlocproc;
    wlocc.cbSize = sizeof(WNDCLASSEX);
    wlocc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wlocc.hbrBackground = (HBRUSH)COLOR_BACKGROUND;
    
    wtoolc.hInstance = hThisInstance;
    wtoolc.lpszClassName = ESBFloatClassName;
    wtoolc.lpfnWndProc = ESBtoolproc;
	wtoolc.cbSize = sizeof(WNDCLASSEX);
	wtoolc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wtoolc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);

    if (!RegisterClassEx(&wincl)) {
        char r[40];
        snprintf(r, sizeof(r), "Window failed\nError %lu", GetLastError());
        MessageBox(NULL, r, "uhoh", MB_ICONSTOP);
        RETURN(0);
    }

    if (!RegisterClassEx(&wsmallc) || !RegisterClassEx(&wtoolc)) {
        char r[40];
        snprintf(r, sizeof(r), "SubWindow failed\nError %lu", GetLastError());
        MessageBox(NULL, r, "uhoh", MB_ICONSTOP);
        RETURN(0);
    }
    
   if (!RegisterClassEx(&wlocc)) {
        char r[40];
        snprintf(r, sizeof(r), "LocatorWindow Failed\nError %lu", GetLastError());
        MessageBox(NULL, r, "uhoh", MB_ICONSTOP);
        RETURN(0);
    }
  
    sys_hwnd = CreateWindowEx (
        0,
        ESBClassName,         /* Classname */
        "Editor Strikes Back",       /* Title Text */
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,       /* Windows decides the position */
        CW_USEDEFAULT,       /* where the window ends up on the screen */
        BXS,                 /* The programs width */
        BYS,                 /* and height in pixels */
        HWND_DESKTOP,        /* The window is a child-window to desktop */
        NULL,                /* No menu */
        hThisInstance,       /* Program Instance handler */
        NULL                 /* No Window Creation data */
    );
    
    edg.subwin = CreateWindowEx(0, ESBSubClassName, NULL,
        WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL, 0, 0, BXS, BYS,
        sys_hwnd, NULL, hThisInstance, NULL);
    add_hwnd_b(sys_hwnd, NULL, edg.subwin);
    
    edg.infobar = CreateWindowEx(0, STATUSCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
        sys_hwnd, NULL, hThisInstance, NULL);

    edg.toolbars_floated = 0;
    init_right_side_windows(hThisInstance);          
    if (!edg.infobar || !edg.treeview || !edg.tile_list) {
        MessageBox(NULL, "Initialization Failed", "uhoh", MB_ICONSTOP);
        RETURN(0);
    }
    setup_right_side_window_fonts();
    
    ShowWindow(sys_hwnd, CRAP);
    UpdateWindow(sys_hwnd);
    
    win_set_window(sys_hwnd);
    allegro_init();
    set_color_depth(desktop_color_depth());
    
    set_config_file("dsb.ini");
    gd.bprefix = dsbstrdup(get_config_string("Settings", "Base", "base"));
    
    load_editor_bitmaps();
    
    edg.runmode = EDITOR_RUNNING;
    ed_resizesubs(sys_hwnd);

    initlua(bfixname("lua.dat"));
    src_lua_file(bfixname("global.lua"), 0);
    src_lua_file("editor/editor.lua", 0);
    init_all_objects();
    gd.max_lvl = 0;
    
    GetCurrentDirectory(sizeof(dbuffer), dbuffer);
    edg.curdir = dsbstrdup(dbuffer);
    memset(dbuffer, 0, sizeof(dbuffer));
    
    edg.ed_lev = gd.p_lev[0];
    edg.uzone = INVALID_UZONE_VALUE;
    edg.f_d_dir = -1;
    
    edg.haccel = CreateAcceleratorTable(myAccel,
        sizeof(myAccel) / sizeof(ACCEL));
    
    SetWindowText(edg.infobar, "ESB 0.9n (DSB 0.68) Ready");
    ed_resizesubs(sys_hwnd);
    force_redraw(edg.subwin, 1);
    purge_dirty_list();
    
    global_instance = hThisInstance;
    while (GetMessage(&messages, NULL, 0, 0) > 0) {
        if (!TranslateAccelerator(sys_hwnd, edg.haccel, &messages)) {
            TranslateMessage(&messages);
            DispatchMessage(&messages);
        }
    }

    DeleteObject(blackbrush);
    free(edg.curdir);
    CoUninitialize();

    return messages.wParam;
}
