#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include <commctrl.h>
#include "E_res.h"
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "editor.h"
#include "editor_hwndptr.h"
#include "editor_gui.h"
#include "editor_menu.h"
#include "editor_clipboard.h"

int SYSTEM_firstload = 1;

extern INT_PTR CALLBACK ESBdproc (HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);

extern HWND sys_hwnd;

static const char Filter[] = "Lua Files\0*.lua\0";
static char lua_buffer[4] = "lua";

char dbuffer[1024];

extern struct editor_global edg;
extern struct global_data gd;

extern struct inst *oinst[NUM_INST];
extern struct dungeon_level *dun;

extern HWND sys_hwnd;
extern FILE *errorlog;

HMENU h_menu;

static void init_dlg(HWND hwnd, OPENFILENAME *fstructure) {
    memset(fstructure, 0, sizeof(OPENFILENAME));

    fstructure->lStructSize = sizeof(OPENFILENAME);
    fstructure->hwndOwner = hwnd;
    fstructure->lpstrFilter = Filter;
    fstructure->lpstrFile = dbuffer;
    fstructure->nMaxFile = sizeof(dbuffer);
    fstructure->lpstrDefExt = lua_buffer;
    fstructure->Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
}

void ed_menu_setup(HWND ehwnd) {
    HMENU h_filemenu;
    
    onstack("ed_menu_setup");

    h_menu = CreateMenu();
    
    h_filemenu = CreatePopupMenu();
    AppendMenu(h_filemenu, MF_STRING, ESBM_NEW, "&New\tCtrl+N");
    AppendMenu(h_filemenu, MF_STRING, ESBM_OPEN, "&Open...\tCtrl+O");
    AppendMenu(h_filemenu, MF_STRING, ESBM_SAVE, "&Save\tCtrl+S");
    AppendMenu(h_filemenu, MF_STRING, ESBM_SAVEAS, "Save &As...");
    AppendMenu(h_filemenu, MF_MENUBREAK, 0, NULL);
    AppendMenu(h_filemenu, MF_STRING, ESBM_EXIT, "E&xit\tAlt+F4");
    AppendMenu(h_menu, MF_STRING|MF_POPUP, (UINT)h_filemenu, "&File");
    
    SetMenu(ehwnd, h_menu);
        
    VOIDRETURN();
}

void ed_firstload_menu_setup(HWND ehwnd) {
    HMENU h_editmenu;
    HMENU h_viewmenu;
    
    if (!SYSTEM_firstload)
        return;
        
    onstack("ed_firstload_menu_setup");
        
    h_editmenu = CreatePopupMenu();
    AppendMenu(h_editmenu, MF_STRING, ESBM_MODE_DD, "&Dungeon Draw\tD");
    AppendMenu(h_editmenu, MF_STRING, ESBM_MODE_AI, "&Add Object\tA");
    AppendMenu(h_editmenu, MF_STRING, ESBM_MODE_WSG, "Wall &Graphics\tG");
    AppendMenu(h_editmenu, MF_STRING, ESBM_MODE_PRP, "Set &Party Start\tY");
    AppendMenu(h_editmenu, MF_STRING, ESBM_MODE_CUT, "&Cut and Paste\tX");
    AppendMenu(h_editmenu, MF_MENUBREAK, 0, NULL);
    AppendMenu(h_editmenu, MF_STRING, ESBM_LEVELINFO, "&Level Info\tW");
    AppendMenu(h_editmenu, MF_STRING, ESBM_GLOBALINFO, "Global &Info");
    AppendMenu(h_editmenu, MF_STRING, ESBM_CHAMPS, "&Champion Roster");
    AppendMenu(h_editmenu, MF_STRING, ESBM_PREFS, "ESB &Options");
    //AppendMenu(h_editmenu, MF_STRING, ESBM_GRAPHICS, "&Graphics Helper");
    AppendMenu(h_editmenu, MF_MENUBREAK, 0, NULL);
    AppendMenu(h_editmenu, MF_STRING, ESBM_RELOADARCH, "&Reload Archs\tCtrl+R");
    
    h_viewmenu = CreatePopupMenu();
    AppendMenu(h_viewmenu, MF_STRING, ESBM_PREVL, "&Previous Level\tQ");
    AppendMenu(h_viewmenu, MF_STRING, ESBM_NEXTL, "&Next Level\tE");
    AppendMenu(h_viewmenu, MF_MENUBREAK, 0, NULL);
    AppendMenu(h_viewmenu, MF_STRING, ESBM_DOUBLEZOOM, "&Double Zoom");
    AppendMenu(h_viewmenu, MF_STRING, ESBM_TARGREVERSE, "&Reverse Target Lines");
    AppendMenu(h_viewmenu, MF_STRING, ESBM_FLOATWIN, "F&loat Tool Windows");
    AppendMenu(h_viewmenu, MF_MENUBREAK, 0, NULL);
    AppendMenu(h_viewmenu, MF_STRING, ESBM_SEARCHFIND, "&Find Insts\tCtrl+F");
    

    AppendMenu(h_menu, MF_STRING|MF_POPUP, (UINT)h_editmenu, "&Edit");
    AppendMenu(h_menu, MF_STRING|MF_POPUP, (UINT)h_viewmenu, "&View");

    SetMenu(ehwnd, h_menu);       
    SYSTEM_firstload = 0;
    
    VOIDRETURN();        
} 

void ed_menucommand(HWND hwnd, unsigned short cmd_id) {
    BOOL b_res;
    HDC hdc;
    HMENU mymenu = GetMenu(hwnd);
    HMENU thismenu;
    const int DOUBLEZOOMPOS = 3;
    const int TARGREVERSEPOS = 4;
    const int FLOATPOS = 5;

    onstack("ed_menucommand");
            
    switch (cmd_id) {
        case ESBM_NEW: {
            if (editor_create_new_gamedir()) { 
                ed_firstload_menu_setup(hwnd);
                SetD(DRM_DRAWDUN, NULL, NULL);  
                update_draw_mode(edg.infobar);
            } 
        } break;
        
        case ESBM_OPEN: {
            int cmode;
            MSG messages;
            OPENFILENAME fstructure;
            
            init_dlg(hwnd, &fstructure);
            
            cmode = edg.draw_mode;
            SetD(DRM_DONOTHING, edg.draw_arch, edg.draw_ws);
            b_res = GetOpenFileName(&fstructure);
            if (b_res) {
                SendMessage(sys_hwnd, EDM_VOID_PSEL, 0, 0);                  
                dsbfree(edg.valid_savename);
                edg.valid_savename = dsbstrdup(fstructure.lpstrFile);
                editor_load_dungeon(fstructure.lpstrFile);                
                ed_firstload_menu_setup(hwnd);
                SetD(DRM_DRAWDUN, NULL, NULL);
                update_draw_mode(edg.infobar);
            } else
                SetD(cmode, edg.draw_arch, edg.draw_ws);
            
        } break;
        
        case ESBM_SAVE: {
            if (edg.valid_savename) {
                editor_export_dungeon(edg.valid_savename);
            } else {
                ed_menucommand(hwnd, ESBM_SAVEAS);
                VOIDRETURN();
            }
        } break;
            
        case ESBM_SAVEAS: {
            OPENFILENAME fstructure;
            
            if (!dun) {
                MessageBox(sys_hwnd, "Cannot save empty dungeon.", "Oh right", MB_ICONEXCLAMATION);
                VOIDRETURN();
            }
            
            init_dlg(hwnd, &fstructure);
            fstructure.Flags |= OFN_OVERWRITEPROMPT;
            
            b_res = GetSaveFileName(&fstructure);

            if (b_res) {
                dsbfree(edg.valid_savename);
                edg.valid_savename = dsbstrdup(fstructure.lpstrFile);
                editor_export_dungeon(fstructure.lpstrFile);
            }
            
        } break;
        
        case ESBM_MODE_DD:
        case ESBM_MODE_AI:
        case ESBM_MODE_WSG:
        case ESBM_MODE_PRP: 
        case ESBM_MODE_CUT: {
            int targetmode = cmd_id - DRM_ESBM_MODEBASE;
            SetD(targetmode, edg.draw_arch, edg.draw_ws);
            update_draw_mode(edg.infobar);
        } break;
        
        case ESBM_DOUBLEZOOM: {
            thismenu = GetSubMenu(mymenu, 2);    
            if (edg.modeflags & EMF_DOUBLEZOOM) {
                CheckMenuItem(thismenu, DOUBLEZOOMPOS, MF_BYPOSITION | MF_UNCHECKED);
                destroy_bitmap(edg.cells);
                destroy_bitmap(edg.marked_cell);
                edg.cells = edg.true_cells;
                edg.marked_cell = edg.true_marked_cell;
                edg.modeflags &= ~(EMF_DOUBLEZOOM);
            } else {
                BITMAP *nce;
                CheckMenuItem(thismenu, DOUBLEZOOMPOS, MF_BYPOSITION | MF_CHECKED);
                edg.modeflags |= EMF_DOUBLEZOOM;
                nce = create_bitmap(edg.cells->w*2, edg.cells->h*2);
                stretch_blit(edg.cells, nce, 0, 0, edg.cells->w, edg.cells->h,
                    0, 0, nce->w, nce->h);
                edg.cells = nce;
                nce = create_bitmap(edg.marked_cell->w*2, edg.marked_cell->h*2);
                stretch_blit(edg.marked_cell, nce, 0, 0,
                    edg.marked_cell->w, edg.marked_cell->h,
                    0, 0, nce->w, nce->h);
                edg.marked_cell = nce;
            }
            if (edg.ed_lev >= 0) {
                ed_init_level_bmp(edg.ed_lev, sys_hwnd);
                ed_resizesubs(sys_hwnd);
            }
        } break;
        
        case ESBM_TARGREVERSE: {
            thismenu = GetSubMenu(mymenu, 2);    
            if (edg.modeflags & EMF_TARGREVERSE) {
                CheckMenuItem(thismenu, TARGREVERSEPOS, MF_BYPOSITION | MF_UNCHECKED);
                edg.modeflags &= ~(EMF_TARGREVERSE);
            } else {
                CheckMenuItem(thismenu, TARGREVERSEPOS, MF_BYPOSITION | MF_CHECKED);
                edg.modeflags |= EMF_TARGREVERSE;
            }
        } break;
        
        case ESBM_FLOATWIN: {
            thismenu = GetSubMenu(mymenu, 2);    
            if (edg.toolbars_floated) {
                CheckMenuItem(thismenu, FLOATPOS, MF_BYPOSITION | MF_UNCHECKED);
                edg.toolbars_floated = 0;
                unfloat_toolbars();
            } else {
                CheckMenuItem(thismenu, FLOATPOS, MF_BYPOSITION | MF_CHECKED);
                edg.toolbars_floated = 1;
                float_toolbars();
            }
        } break;
        
        case ESBM_LEVELINFO: {
            HWND *f = GetFocus();
            ESB_modaldb(ESB_LEVELINFO, sys_hwnd, ESB_level_info_proc, 0);
            SetFocus(f);
        } break;
        
        case ESBM_GLOBALINFO: {
            HWND *f = GetFocus();
            ESB_modaldb(ESB_PGLOBALINFO, sys_hwnd, ESB_global_info_proc, 0);
            SetFocus(f);
        } break;
        
        case ESBM_PREFS: {
            HWND *f = GetFocus();
            ESB_modaldb(ESB_PREFS, sys_hwnd, ESBoptionsproc, 0);
            SetFocus(f);
        } break;
        
        case ESBM_CHAMPS: {
            HWND *f = GetFocus();
            ESB_modaldb(ESB_PARTYBUILD, sys_hwnd, ESBpartyproc, 0);
            SetFocus(f);
        } break;
        
        case ESBM_SEARCHFIND: {
            HWND *f = GetFocus();
            ESB_modaldb(ESB_FINDER, sys_hwnd, ESBsearchproc, 0);
            SetFocus(f);
        } break;
            
        case ESBM_EXIT:
            ed_quit_out();
            break;
            
        case ESBM_PREVL: {
            if (edg.ed_lev > 0) {
                edg.ed_lev--;
                ed_init_level_bmp(edg.ed_lev, sys_hwnd);
                ed_resizesubs(sys_hwnd);
                QO_MOUSEOVER();
            }  
        } break;
        
        case ESBM_NEXTL: {
            if (edg.ed_lev >= 0 && edg.ed_lev < (gd.dungeon_levels - 1)) {
                edg.ed_lev++;
                ed_init_level_bmp(edg.ed_lev, sys_hwnd);
                ed_resizesubs(sys_hwnd);
                QO_MOUSEOVER();
            } 
        } break;

        case ESBM_RELOADARCH: {
            ed_reload_all_arch();
        } break;
            
        case ESBA_MODE1:
        case ESBA_MODE2:
        case ESBA_MODE3:
        case ESBA_MODE4:
        case ESBA_MODE5:
            edg.d_dir = cmd_id - ESBA_MODE1;
            locw_update(edg.loc, NULL);
            break;
            
        case ESBA_EDITLAST: {
            if (edg.last_inst && oinst[edg.last_inst]) {
                edg.op_inst = edg.last_inst;

                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(ESB_EXVEDIT),
                    sys_hwnd, ESBdproc);
            }
        } break;
        
        case ESBM_DO_CUT:
        case ESBM_DO_COPY:
        case ESBM_DO_PASTE:
            ed_clipboard_command(cmd_id - ESBM_DO_CUT);
            break;
        
        case ESBM_MODESWAP_DO_CUT:
        case ESBM_MODESWAP_DO_COPY:
        case ESBM_MODESWAP_DO_PASTE: {
            if (!SYSTEM_firstload) {
                POINT pt;
                struct dungeon_level *dd;
                int ch;
                int nx, ny, tx, ty;
                
                if (edg.draw_mode != DRM_CUTPASTE) {
                    SetD(DRM_CUTPASTE, edg.draw_arch, edg.draw_ws);
                    update_draw_mode(edg.infobar);
                }
                
                GetCursorPos(&pt);
                MapWindowPoints(NULL, edg.subwin, &pt, 1); 
                
                dd = &(dun[edg.ed_lev]);
                ch = edg.cells->w / 8;
                tninfo(edg.subwin, dd, ch, pt.x, pt.y, &nx, &ny, &tx, &ty);
                
                clipboard_mark(edg.ed_lev, tx, ty);
                ed_clipboard_command(cmd_id - ESBM_MODESWAP_DO_CUT);
                ed_main_window_mouseover(tx, ty);
            }
        } break;

    }

    VOIDRETURN();
}

void ed_set_autosel_modes(int zt) {
    onstack("ed_set_autosel_modes");
    if (edg.autosel == 255) edg.autosel = 0;
    else {
        if (zt > 5) edg.autosel = 0;
        else if (zt >= 4) edg.autosel = 2;
        else edg.autosel = 1;
    }     
    VOIDRETURN();
}  


void clbr_del(LPARAM lParam) {
    NM_TREEVIEW *nmtv;
    TV_ITEM *seltv;
    
    onstack("clbr_del");
    
    nmtv = (NM_TREEVIEW*)lParam;
    if (nmtv) {
        seltv = &(nmtv->itemOld); 
        if (seltv) {
            dsbfree((void*)seltv->lParam);
        }
    }
    
    VOIDRETURN();
}

void clbr_sel(LPARAM lParam) {
    static int queuemode = DRM_DRAWDUN;
    int zt;
    NM_TREEVIEW *nmtv;
    TV_ITEM *seltv;
    
    onstack("clbr_sel");
    
    if (!edg.in_purge) {
        nmtv = (NM_TREEVIEW*)lParam;
        seltv = &(nmtv->itemNew);
        if (seltv->lParam != 0) {
            char *pointer = (char *)seltv->lParam;
            if (edg.draw_mode != DRM_DRAWOBJ &&
                edg.draw_mode != DRM_WALLSET)
            {
                queuemode = edg.draw_mode;
            }
            
            if (*pointer == 'i')
                SetD(DRM_DRAWOBJ, pointer + 1, edg.draw_ws);
            else if (*pointer == 'w')
                SetD(DRM_WALLSET, edg.draw_arch, pointer + 1);
                
        } else {
            if (edg.draw_mode == DRM_DRAWOBJ ||
                edg.draw_mode == DRM_WALLSET) 
            {
                SetD(queuemode, edg.draw_arch, edg.draw_ws);
                edg.autosel = 255;
            }   
        }
            
        update_draw_mode(edg.infobar);
        
        edg.force_zone++;
        zt = locw_update(edg.loc, NULL);
        edg.force_zone--;
        ed_set_autosel_modes(zt);
    }
    
    VOIDRETURN();
}

void ed_quit_out(void) {
    int z = MessageBox(sys_hwnd, "Are you sure you wish to quit?",
        "Confirm", MB_ICONEXCLAMATION|MB_YESNO);

    if (z != 6)
        return;
        
    DestroyAcceleratorTable(edg.haccel);
    PostQuitMessage(0);
}

