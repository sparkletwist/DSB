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
#include "arch.h"
#include "editor_hwndptr.h"
#include "editor_gui.h"
#include "editor_menu.h"
#include "editor_shared.h"
#include "editor_clipboard.h"

#define CHECK_BOX(V) SendDlgItemMessage(hwnd, V, BM_SETCHECK, BST_CHECKED, 0)
#define QUERY_BOX(V) SendDlgItemMessage(hwnd, V, BM_GETCHECK, 0, 0)
#define SET_FLAG_ON_BOX(F, V) \
    do {if (QUERY_BOX(V)) p_inst->gfxflags |= F;\
    else p_inst->gfxflags &= ~(F); } while (0) 
    
#define EDITFLAG_TO_BOX(F, V) if (edg.editor_flags & F) \
    SendDlgItemMessage(hwnd, V, BM_SETCHECK, BST_CHECKED, 0)
#define GAMEFLAG_TO_BOX(F, V) if (gd.gameplay_flags & F) \
    SendDlgItemMessage(hwnd, V, BM_SETCHECK, BST_CHECKED, 0)
    
#define EDITFLAG_FROM_BOX(F, V) if (SendDlgItemMessage(hwnd, V, BM_GETCHECK, 0, 0)) \
    edg.editor_flags |= F; else edg.editor_flags &= ~(F)
#define GAMEFLAG_FROM_BOX(F, V) if (SendDlgItemMessage(hwnd, V, BM_GETCHECK, 0, 0)) \
    gd.gameplay_flags |= F; else gd.gameplay_flags &= ~(F)
 
#define ITEMFROMARCHBOOL(S, V) \
    do {\
    lua_pushstring(LUA, S);\
    lua_gettable(LUA, -2);\
    if (lua_isboolean(LUA, -1) && lua_toboolean(LUA, -1)) {\
        HWND hw = GetDlgItem(hwnd, V);\
        ShowWindow(hw, SW_SHOW);\
        EnableWindow(hw, TRUE);\
    }\
    lua_pop(LUA, 1);\
    } while(0)

extern struct global_data gd;
extern struct editor_global edg;
extern struct inst *oinst[NUM_INST];
extern struct dungeon_level *dun;
extern lua_State *LUA;

extern HWND sys_hwnd;
extern FILE *errorlog;

unsigned int ed_optinst(HWND hwnd, int box, int *v) {
    LRESULT lr_val;
    unsigned int i_val;
    
    onstack("ed_optinst");
    
    lr_val = SendDlgItemMessage(hwnd, box, LB_GETCURSEL, 0, 0);
    if (lr_val != LB_ERR) {
        i_val = SendDlgItemMessage(hwnd, box, LB_GETITEMDATA,
            (WPARAM)lr_val, 0);
        *v = (int)lr_val;
    } else
        i_val = 0;
    
    RETURN(i_val);
}

INT_PTR CALLBACK ESBidchooseproc (HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    unsigned int inst;
    int sidx = 0;
    int mv = 0;
    
    onstack("ESBidchooseproc");

    switch(message) {
        case WM_INITDIALOG:
            ed_fillboxfromtile(hwnd, ESB_IDBOX, edg.ed_lev, edg.lsx, edg.lsy, -1);
            break;
            
        case WM_COMMAND:
            inst = ed_optinst(hwnd, ESB_IDBOX, &sidx);
            if (inst || LOWORD(wParam) == IDCANCEL) {
                int hw = HIWORD(wParam);
                switch(LOWORD(wParam)) {
                    case IDCANCEL:
                        EndDialog(hwnd, 1);
                    break;        
                    
                    case ESB_IDBOX: {
                        if (hw == LBN_DBLCLK) {
                            goto EDIT_FALLTHROUGH;
                        }
                    } break;

                    case ESB_IDEDIT:
                        EDIT_FALLTHROUGH:
                        edg.op_inst = inst;
                        DialogBox(GetModuleHandle(NULL),
                            MAKEINTRESOURCE(ESB_EXVEDIT),
                            hwnd, ESBdproc);
                        edg.op_inst = 0;
                        ed_fillboxfromtile(hwnd, ESB_IDBOX,
                            edg.ed_lev, edg.lsx, edg.lsy, -1); 
                    break;        
                                                
                    case ESB_IDDEL:
                        recursive_inst_destroy(inst, 0);
                        mv = ed_fillboxfromtile(hwnd, ESB_IDBOX,
                            edg.ed_lev, edg.lsx, edg.lsy, -1);
                        if (!mv)
                            EndDialog(hwnd, 1);
                        else {
                            if (sidx > 0) sidx--;
                            SendDlgItemMessage(hwnd, ESB_IDBOX,
                                LB_SETCURSEL, sidx, 0);
                        }
                        break;

                    case ESB_MOVEUP:
                        mv = inst_moveup(inst);
                        ed_fillboxfromtile(hwnd, ESB_IDBOX, edg.ed_lev, edg.lsx, edg.lsy, sidx - mv);
                        break;

                    case ESB_MOVEDOWN:
                        mv = inst_movedown(inst);
                        ed_fillboxfromtile(hwnd, ESB_IDBOX, edg.ed_lev, edg.lsx, edg.lsy, sidx + mv);
                        break;
                }
            }
            break;

        case WM_CLOSE:
            EndDialog(hwnd, 1);
            break;

        default:
            RETURN(FALSE);
    }

    RETURN(TRUE);
}

INT_PTR CALLBACK ESBdproc (HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    int i_res;
    int jump_out = 0;
    char dt_fast = '\0';

    onstack("ESBdproc");

    switch(message) {
        case WM_COMMAND: {
            struct ext_hwnd_info *xinfo = get_hwnd(hwnd);
            if (xinfo && xinfo->inst && oinst[xinfo->inst]) {
                char *exvn;
                int selexv;
                int csel;
                int hw = HIWORD(wParam);
                HWND hlist = GetDlgItem(hwnd, ESB_EXVARLIST);
                                
                switch(LOWORD(wParam)) {
                    case IDCANCEL:
                        jump_out = 1;
                    break;
                    
                    case ESB_MONDEF: {
                        HWND ckb = GetDlgItem(hwnd, ESB_MONDEF);
                        HWND wi_1 = GetDlgItem(hwnd, ESB_HPS);
                        HWND wi_2 = GetDlgItem(hwnd, ESB_MHP);
                        int chk = SendMessage(ckb, BM_GETCHECK, 0, 0); 
                        if (chk) {
                            EnableWindow(wi_1, FALSE);
                            EnableWindow(wi_2, FALSE);
                        } else {
                            EnableWindow(wi_1, TRUE);
                            EnableWindow(wi_2, TRUE);
                        }                  
                    } break;
                    
                    case ESB_OBJARCHCHG: {
                        struct obj_arch *p_arch;
                        int objtype = 0;
                        edg.op_inst = xinfo->inst;
                        p_arch = Arch(oinst[xinfo->inst]->arch);
                        objtype = p_arch->type;
                        if (objtype < OBJTYPE_WALLITEM) {
                            MessageBox(hwnd, "Cannot change arch.\nInvalid object type.",
                                "Invalid Type", MB_ICONEXCLAMATION);
                        } else {   
                            edg.op_arch = 0;
                            edg.op_base_sel_str = p_arch->luaname;
                            ESB_modaldb(ESB_ARCHPICKER, hwnd, ESB_archpick_proc, 0); 
                            if (edg.op_arch != 0xFFFFFFFF) {  
                                HWND locator_win;
                                char s_txt[128]; 
                                
                                oinst[xinfo->inst]->arch = edg.op_arch;
                                p_arch = Arch(edg.op_arch);
                                snprintf(s_txt, sizeof(s_txt), "Edit %s [%lu]",
                                    p_arch->luaname, xinfo->inst);
                                SetWindowText(hwnd, s_txt); 
                                
                                locator_win = GetDlgItem(hwnd, ESB_DLOCATOR);
                                InvalidateRect(locator_win, NULL, TRUE);
                                UpdateWindow(locator_win);   
                            }
                            edg.op_base_sel_str = NULL;
                        }  
                    } break;
                                    
                    case ESB_EXVARLIST: {
                        if (hw == LBN_DBLCLK) {
                            exvn = ed_makeselexvar(hlist, &csel, &dt_fast);
                            goto EXVAR_EDIT;
                        }
                    } break;
                    
                    case ESB_CONLIST: {
                        if (hw == LBN_DBLCLK) {
                            int sidx;
                            int inst = ed_optinst(hwnd, ESB_CONLIST, &sidx);
                            if (inst) {
                                edg.op_inst = inst;
                                DialogBox(GetModuleHandle(NULL),
                                    MAKEINTRESOURCE(ESB_EXVEDIT),
                                    hwnd, ESBdproc);
                            }                            
                        }
                    } break;
                    
                    case ESB_ADDEXV: {
                        edg.op_inst = xinfo->inst; // set for goto, NOT add_exvar_proc
                        edg.w_t_string = "Add exvar";
                        edg.exvn_string = "What would you like to call the new exvar?";
                        DialogBox(GetModuleHandle(NULL),
                            MAKEINTRESOURCE(ESB_EXVARADD),
                            hwnd, ESB_add_exvar_proc);
                        if (edg.exvn_string) {
                            exvn = (char*)edg.exvn_string;
                            csel = -1;
                            goto EXVAR_EDIT;
                        }
 
                    } break;
                    
                    case ESB_DELEXV: {
                        exvn = ed_makeselexvar(hlist, &csel, NULL);
                        if (exvn != NULL) {
                            char *buffer = dsbfastalloc(250);
                            snprintf(buffer, 250, "exvar[%d].%s = nil",
                                xinfo->inst, exvn);
                            // meh. if this fails we have worse problems.
                            luaL_dostring(LUA, buffer);
                            ed_grab_exvars_for(xinfo->inst,
                                hwnd, ESB_EXVARLIST);
                            if (csel > 0) csel--;
                            SendDlgItemMessage(hwnd, ESB_EXVARLIST,
                                LB_SETCURSEL, csel, 0);
                            dsbfree(exvn);
                        }
                        
                    } break;
                    
                    case ESB_EDEXV: {
                        exvn = ed_makeselexvar(hlist, &csel, NULL);
                        
                        EXVAR_EDIT:
                        if (exvn != NULL) {
                            int r;
                            
                            edg.ch_ex_mode = 0;
                            r = ed_edit_exvar(hwnd, xinfo->inst,
                                "exvar", exvn, dt_fast);
                                
                            if (r) {
                                ed_grab_exvars_for(xinfo->inst,
                                    hwnd, ESB_EXVARLIST);   
                                if (csel == -1) {
                                    char dbuf[100];
                                    snprintf(dbuf, 99, "%s =", exvn);
                                    csel = SendDlgItemMessage(hwnd, ESB_EXVARLIST,
                                        LB_FINDSTRING, 0, (int)dbuf);
                                }   
                                SendDlgItemMessage(hwnd, ESB_EXVARLIST,
                                    LB_SETCURSEL, csel, 0);
                            }
                            free(exvn);
                        }
                    } break;
                    
                    case ESB_ADDCON: {
                        edg.op_inst = xinfo->inst;
                        DialogBox(GetModuleHandle(NULL),
                            MAKEINTRESOURCE(ESB_CONBUILDER),
                            hwnd, ESB_build_container);
                        ed_fillboxfromtile(hwnd, ESB_CONLIST,
                            LOC_IN_OBJ, xinfo->inst, 0, -1); 
                    } break;
                    
                    case ESB_EDITCON: {
                        int sidx;
                        int inst = ed_optinst(hwnd, ESB_CONLIST, &sidx);
                        if (inst) {
                            edg.op_inst = inst;
                            DialogBox(GetModuleHandle(NULL),
                                MAKEINTRESOURCE(ESB_EXVEDIT),
                                hwnd, ESBdproc);
                            ed_fillboxfromtile(hwnd, ESB_CONLIST,
                                LOC_IN_OBJ, xinfo->inst, 0, sidx); 
                        }
                    } break;
                    
                    case ESB_DELCON: {
                        int sidx;
                        int inst = ed_optinst(hwnd, ESB_CONLIST, &sidx);
                        if (inst) {
                            recursive_inst_destroy(inst, 0);
                            if (sidx > 0) --sidx;
                            ed_fillboxfromtile(hwnd, ESB_CONLIST,
                                LOC_IN_OBJ, xinfo->inst, 0, sidx); 
                        }
                    } break;
                    
                    case ESB_EDCOOR: {
                        int storev;
                        int success;
                        struct inst *p_inst = oinst[xinfo->inst];
                        
                        edg.op_inst = xinfo->inst;
                        edg.op_picker_val = 1;
                        
                        success = ex_exvar(xinfo->inst, "lev", &storev);
                        if (success) edg.op_p_lev = storev;
                        else if (p_inst->level >= 0) edg.op_p_lev = p_inst->level;
                        else edg.op_p_lev = edg.ed_lev;
                        
                        edg.op_x = edg.op_y = 0;
                        
                        if (p_inst->level >= 0) {
                            int xs, ys;
                            int xsuc = ex_exvar(xinfo->inst, "x", &xs);
                            int ysuc = ex_exvar(xinfo->inst, "y", &ys);
                            
                            if (xsuc) edg.op_x = xs;
                            else edg.op_x = p_inst->x;
                            
                            if (ysuc) edg.op_y = ys;
                            else edg.op_y = p_inst->y;
                        }
                        
                        ESB_modaldb(ESB_PICKER, hwnd, ESB_picker_proc, 1);
                        ed_grab_exvars_for(xinfo->inst,
                            hwnd, ESB_EXVARLIST);
                            
                        edg.op_picker_val = 0;
                            
                    } break;
                    
                    case ESB_EDGEN: {
                        edg.op_inst = xinfo->inst;
                        ESB_modaldb(ESB_MONSTERGEN, hwnd, ESB_monstergen_proc, 0);
                        ed_grab_exvars_for(xinfo->inst,
                            hwnd, ESB_EXVARLIST);                        
                    } break;
                    
                    case ESB_EDOPBY: {
                        edg.op_inst = xinfo->inst;
                        ESB_modaldb(ESB_OPBYED, hwnd, ESB_opby_ed_proc, 0);
                        ed_grab_exvars_for(xinfo->inst,
                            hwnd, ESB_EXVARLIST);                        
                    } break;
                    
                    case ESB_EDSPINB: {
                        edg.op_inst = xinfo->inst;
                        ESB_modaldb(ESB_SPINEDIT, hwnd, ESB_spinedit_proc, 0);
                        ed_grab_exvars_for(xinfo->inst,
                            hwnd, ESB_EXVARLIST);                        
                    } break;
                    
                    case ESB_EDSHOOT: {
                        edg.op_inst = xinfo->inst;
                        ESB_modaldb(ESB_SHOOTEROPT, hwnd, ESB_shooter_proc, 0);
                        ed_grab_exvars_for(xinfo->inst,
                            hwnd, ESB_EXVARLIST);                        
                    } break;
                    
                    case ESB_EDMIRROR: {
                        edg.op_inst = xinfo->inst;
                        ESB_modaldb(ESB_MIRROROPT, hwnd, ESB_mirror_proc, 0);
                        ed_grab_exvars_for(xinfo->inst,
                            hwnd, ESB_EXVARLIST);                        
                    } break;
                    
                    case ESB_EDARCH: {
                        edg.op_inst = xinfo->inst;
                        ESB_modaldb(ESB_ARCHPICKER_SWAP, hwnd, ESB_archpick_swap_proc, 0);
                        ed_grab_exvars_for(xinfo->inst,
                            hwnd, ESB_EXVARLIST);                        
                    } break;
                    
                    case ESB_EDFUNCS: {
                        edg.op_inst = xinfo->inst;
                        ESB_modaldb(ESB_FUNCCALLEROPT, hwnd, ESB_function_caller_proc, 0);
                        ed_grab_exvars_for(xinfo->inst,
                            hwnd, ESB_EXVARLIST);                        
                    } break;
                    
                    case ESB_EDITEMACTION: {
                        edg.op_inst = xinfo->inst;
                        ESB_modaldb(ESB_ITEMACTIONEDITOR, hwnd, ESB_item_action_proc, 0);
                        ed_grab_exvars_for(xinfo->inst,
                            hwnd, ESB_EXVARLIST);                        
                    } break;
                    
                    case ESB_EDTRIGCON: {
                        edg.op_inst = xinfo->inst;
                        ESB_modaldb(ESB_REQEDITOR, hwnd, ESB_trigger_controller_proc, 0);
                        ed_grab_exvars_for(xinfo->inst,
                            hwnd, ESB_EXVARLIST);                        
                    } break;
                    
                    case ESB_EDCOUNTER: {
                        edg.op_inst = xinfo->inst;
                        ESB_modaldb(ESB_COUNTEREDITOR, hwnd, ESB_counter_edit_proc, 0);
                        ed_grab_exvars_for(xinfo->inst,
                            hwnd, ESB_EXVARLIST);                        
                    } break;
                    
                    case ESB_EDDOORSTR: {
                        edg.op_inst = xinfo->inst;
                        ESB_modaldb(ESB_DOOREDITOR, hwnd, ESB_door_edit_proc, 0);
                        ed_grab_exvars_for(xinfo->inst,
                            hwnd, ESB_EXVARLIST);                        
                    } break;
                    
                    case ESB_EDTAR: {
                        int storev;
                        int success;
                        struct inst *p_inst = oinst[xinfo->inst];
                        struct obj_arch *p_arch = Arch(p_inst->arch);
                        
                        edg.op_inst = xinfo->inst;
                        edg.op_picker_val = 2;
                        edg.op_p_lev = edg.ed_lev;
                        edg.op_x = -1;
                        if (queryluabool(p_arch, "esb_repeating"))
                            edg.q_r_options = 1;
                        else if (queryluabool(p_arch, "esb_probability"))
                            edg.q_r_options = 2;
                        else
                            edg.q_r_options = 0;
                                                
                        ESB_modaldb(ESB_TPICKER, hwnd, ESB_target_picker_proc, 1);
                        ed_grab_exvars_for(xinfo->inst,
                            hwnd, ESB_EXVARLIST);
                            
                        edg.op_picker_val = 0;
                            
                    } break;
                    
                    case ESB_DELETETHIS: {
                        int boxresult;
                        int lev = oinst[xinfo->inst]->level;
                        if (lev == LOC_PARCONSPC) {
                            MessageBox(hwnd, "Cannot delete this inst.\n"
                                "It is used as a champion's external view.",
                                "Error", MB_ICONEXCLAMATION);
                            break;
                        }
                        boxresult = MessageBox(hwnd,
                            "Delete this object instance?", "Deletion Warning",
                            MB_ICONEXCLAMATION|MB_YESNO);
                        if (boxresult == 6) {        
                            recursive_inst_destroy(xinfo->inst, 0);
                            xinfo->inst = 0;
                            SendMessage(hwnd, WM_CLOSE, 0, 0); 
                        }
                    } break;
                    
                }
            }
        }
        break;
        
        case WM_INITDIALOG: // 272
        if (edg.op_inst && oinst[edg.op_inst]) {
            struct inst *p_inst = oinst[edg.op_inst];
            struct obj_arch *p_arch = Arch(p_inst->arch);
            char s_txt[128];
            HWND nwhwnd;
            
            stackbackc(':');
            v_onstack("initdialog");
            
            add_hwnd(hwnd, edg.op_inst, edg.uzone);
            purge_dirty_list();
            
            snprintf(s_txt, sizeof(s_txt), "Edit %s [%lu]",
                p_arch->luaname, edg.op_inst);
            SetWindowText(hwnd, s_txt);
                            
            ed_grab_exvars_for(edg.op_inst, hwnd, ESB_EXVARLIST);
            
            if (p_inst->gfxflags & OF_INACTIVE)
                CHECK_BOX(ESB_STINA);

            if (p_inst->gfxflags & G_ALT_ICON)
                CHECK_BOX(ESB_STALT);
                
            if (p_inst->gfxflags & G_BASHED)
                CHECK_BOX(ESB_STBASH);
                
            if (p_inst->gfxflags & G_FLIP)
                CHECK_BOX(ESB_STFLIP);  
                
            ed_fillboxfromtile(hwnd, ESB_CONLIST,
                LOC_IN_OBJ, edg.op_inst, 0, -1);
                
            if (p_arch->type == OBJTYPE_THING) {
                HWND chg = GetDlgItem(hwnd, ESB_GRPC);
                HWND chgbox = GetDlgItem(hwnd, ESB_CHARGE);
                EnableWindow(chg, TRUE);
                EnableWindow(chgbox, TRUE);
                snprintf(s_txt, sizeof(s_txt), "%lu", p_inst->charge);
                SendDlgItemMessage(hwnd, ESB_CHARGE,
                    WM_SETTEXT, 0, (LPARAM)s_txt);
                EnableWindow(GetDlgItem(hwnd, ESB_STALT), TRUE);
            }
            
            snprintf(s_txt, sizeof(s_txt), "%lu", p_inst->frame);
            SendDlgItemMessage(hwnd, ESB_FRAMECNT, WM_SETTEXT, 0, (LPARAM)s_txt);
            
            luastacksize(6);
            lua_getglobal(LUA, "obj");
            lua_pushstring(LUA, p_arch->luaname);
            lua_gettable(LUA, -2);
            ITEMFROMARCHBOOL("esb_use_coordinates", ESB_EDCOOR);
            ITEMFROMARCHBOOL("esb_use_coordinates", ESB_EDOPBY);
            ITEMFROMARCHBOOL("esb_can_spin", ESB_EDSPINB);
            ITEMFROMARCHBOOL("esb_monster_generator", ESB_EDGEN);
            ITEMFROMARCHBOOL("esb_shooter", ESB_EDSHOOT);
            ITEMFROMARCHBOOL("esb_mirror", ESB_EDMIRROR);
            ITEMFROMARCHBOOL("esb_swapper", ESB_EDARCH);
            ITEMFROMARCHBOOL("esb_function_caller", ESB_EDFUNCS);
            ITEMFROMARCHBOOL("esb_item_action", ESB_EDITEMACTION);
            ITEMFROMARCHBOOL("esb_trigger_controller", ESB_EDTRIGCON);
            ITEMFROMARCHBOOL("esb_counter", ESB_EDCOUNTER);
            lua_pop(LUA, 2);
            lstacktop();
            
            if (p_arch->type == OBJTYPE_MONSTER ||
                p_arch->type == OBJTYPE_DOOR)
            {
                int cursel = p_inst->facedir;
                HWND fdirbox = GetDlgItem(hwnd, ESB_FDIRBOX);
                EnableWindow(GetDlgItem(hwnd, ESB_FDID), TRUE);
                EnableWindow(fdirbox, TRUE);
                if (p_arch->type == OBJTYPE_DOOR) {
                    SendMessage(fdirbox, CB_ADDSTRING, 0, (int)"Auto");
                    SendMessage(fdirbox, CB_ADDSTRING, 0, (int)"Force E/W");
                    SendMessage(fdirbox, CB_ADDSTRING, 0, (int)"Force N/S");
                    if (cursel > 2) cursel = 1;
                } else {
                    SendMessage(fdirbox, CB_ADDSTRING, 0, (int)"North");
                    SendMessage(fdirbox, CB_ADDSTRING, 0, (int)"East");
                    SendMessage(fdirbox, CB_ADDSTRING, 0, (int)"South");
                    SendMessage(fdirbox, CB_ADDSTRING, 0, (int)"West");
                }
                SendMessage(fdirbox, CB_SETCURSEL, cursel, 0); 
                
                if (p_arch->type == OBJTYPE_DOOR) {
                    HWND hw = GetDlgItem(hwnd, ESB_EDDOORSTR);
                    ShowWindow(hw, SW_SHOW);
                    EnableWindow(hw, TRUE);
                }   
            }
            
            if (p_arch->type > OBJTYPE_INVALID) {
                int tt;
                luastacksize(4);
                lua_getglobal(LUA, "editor_check_taketargets");
                lua_pushstring(LUA, p_arch->luaname);
                tt = lc_call_topstack(1, "editor_check_taketargets");
                if (tt) {
                    EnableWindow(GetDlgItem(hwnd, ESB_EDTAR), TRUE);
                    if (tt > 1)
                        EnableWindow(GetDlgItem(hwnd, ESB_EDOPBY), TRUE);
                }
            }  
            
            if (p_arch->type == OBJTYPE_WALLITEM) {
                EnableWindow(GetDlgItem(hwnd, ESB_STFLIP), TRUE);
            }
                        
            if (p_arch->type == OBJTYPE_DOOR) {
                HWND crp = GetDlgItem(hwnd, ESB_GRPI);
                HWND crpbox = GetDlgItem(hwnd, ESB_CROP);
                EnableWindow(crp, TRUE);
                EnableWindow(crpbox, TRUE);
                
                EnableWindow(GetDlgItem(hwnd, ESB_STBASH), TRUE);
                EnableWindow(GetDlgItem(hwnd, ESB_STFLIP), TRUE);
                
                snprintf(s_txt, sizeof(s_txt), "%lu", p_inst->crop);
                SendDlgItemMessage(hwnd, ESB_CROP,
                    WM_SETTEXT, 0, (LPARAM)s_txt);
            }
            
            if (p_arch->type == OBJTYPE_MONSTER) {
                HWND dhwnd[4] = { 0 };
                int i_c;
                
                EnableWindow(GetDlgItem(hwnd, ESB_STFLIP), TRUE);
                
                dhwnd[0] = GetDlgItem(hwnd, ESB_GRPM);
                dhwnd[1] = GetDlgItem(hwnd, ESB_MONDEF);
                dhwnd[2] = GetDlgItem(hwnd, ESB_MHP);
                dhwnd[3] = GetDlgItem(hwnd, ESB_HPS);
                i_c = 4;
           
                if (!p_inst->ai->d_hp) {
                    SendMessage(dhwnd[1], BM_SETCHECK, BST_CHECKED, 0);
                    p_inst->ai->hp = ed_calculate_mon_hp(p_inst);
                    i_c = 1;
                }
                
                snprintf(s_txt, sizeof(s_txt), "%d", p_inst->ai->hp);
                SetWindowText(dhwnd[2], s_txt);
                
                for (;i_c>=0;--i_c) {
                    if (dhwnd[i_c])
                        EnableWindow(dhwnd[i_c], TRUE);
                }
                    
            }
        }
        break;

        case WM_CLOSE: {
            jump_out = 1;    
        }
        break;
        
        case WM_DESTROY:
            del_hwnd(hwnd);
        break;
        
        default:
            RETURN(FALSE);
    }
    
    if (jump_out) {
        struct ext_hwnd_info *xinfo = get_hwnd(hwnd);
        if (xinfo && xinfo->inst && oinst[xinfo->inst]) {
            struct inst *p_inst = oinst[xinfo->inst];
            struct obj_arch *p_arch = Arch(p_inst->arch);
            int old_actstate = !!(p_inst->gfxflags & OF_INACTIVE);
            int new_actstate;
            i_res = GetDlgItemInt(hwnd, ESB_CHARGE, NULL, FALSE);
            if (i_res > 255) i_res = 255;
            p_inst->charge = i_res;

            if (p_arch->type == OBJTYPE_DOOR) {
                i_res = GetDlgItemInt(hwnd, ESB_CROP, NULL, FALSE);
                if (i_res > p_arch->cropmax) i_res = p_arch->cropmax;
                p_inst->crop = i_res;
            }
            
            i_res = GetDlgItemInt(hwnd, ESB_FRAMECNT, NULL, FALSE);
            if (i_res < 0 || i_res >= FRAMECOUNTER_MAX) i_res = 0;
            p_inst->frame = i_res;
               
            SET_FLAG_ON_BOX(OF_INACTIVE, ESB_STINA);
            SET_FLAG_ON_BOX(G_ALT_ICON, ESB_STALT);
            SET_FLAG_ON_BOX(G_BASHED, ESB_STBASH);
            SET_FLAG_ON_BOX(G_FLIP, ESB_STFLIP);
            
            new_actstate = !!(p_inst->gfxflags & OF_INACTIVE);
            if (old_actstate != new_actstate) {
                luastacksize(5);
                if (new_actstate)
                    lua_getglobal(LUA, "editor_inst_deactivate");
                else
                    lua_getglobal(LUA, "editor_inst_activate");
                lua_pushinteger(LUA, xinfo->inst);
                lua_pushstring(LUA, p_arch->luaname);
                if (lua_pcall(LUA, 2, 0, 0) != 0) {
                    lua_function_error("instchanger", lua_tostring(LUA, -1));
                }
                lstacktop();
            } 

            if (p_arch->type == OBJTYPE_MONSTER) {
                i_res = QUERY_BOX(ESB_MONDEF);
                if (i_res) {
                    p_inst->ai->d_hp = 0;
                } else {
                    p_inst->ai->d_hp = 1;
                    p_inst->ai->hp = GetDlgItemInt(hwnd, ESB_MHP,
                        NULL, FALSE);
                    if (p_inst->ai->hp <= 0)
                        p_inst->ai->hp = 1;
                }
            }
            
            if (p_arch->type == OBJTYPE_MONSTER ||
                p_arch->type == OBJTYPE_DOOR)
            {
                HWND fdirbox = GetDlgItem(hwnd, ESB_FDIRBOX);
                int cursel = SendMessage(fdirbox, CB_GETCURSEL, 0, 0); 
                if (p_arch->type == OBJTYPE_MONSTER &&
                    p_arch->msize == 2 && p_inst->tile != DIR_CENTER)
                {
                    if (cursel == p_inst->tile ||
                        cursel == (p_inst->tile + 3) % 4)
                    {
                        depointerize(xinfo->inst);
                        p_inst->facedir = cursel;
                        pointerize(xinfo->inst);
                    }
                    
                } else {
                    p_inst->facedir = cursel;
                }               
            }
        }        
        EndDialog(hwnd, 0);
    }

    RETURN(TRUE);
}

int ed_initial_validate(const char *buffer) {
    int got_equal = 0;
    int got_table = 0;
    int got_quote = 0;
    
    onstack("ed_initial_validate");
    while (*buffer != '\0') {
        if (got_quote) {
            if (*buffer == '"')
                got_quote = 0;
            buffer++;
            continue;
        }
        if (*buffer == '"')
            got_quote = 1;
            
        if (*buffer == '=') {
            got_equal++;
            if (!got_table)
                RETURN(0);
        }
        if (*buffer == '{') {
            got_table++;
        }
        
        if (*buffer == '(' || *buffer == ')')
            RETURN(0);
            
        if (*buffer == ',' && !got_table)
            RETURN(0);
            
        if (*buffer == '}') {
            --got_table;
            if (got_table < 0)
                RETURN(0);
        }
        ++buffer;
    }
    
    if (got_table != 0)
        RETURN(0);
    
    RETURN(1);    
}

int ed_lua_alert_niltest(HWND hwnd, const char *bptr) {
    char *lbuffer = dsbfastalloc(1600);
    
    onstack("ed_lua_alert_niltest");
   
    snprintf(lbuffer, 1600, "EXVAR_EXPRESSION = %s", bptr);
    if (luaL_dostring(LUA, lbuffer)) {
        MessageBox(hwnd, lua_tostring(LUA, -1),
            "Lua Error", MB_ICONEXCLAMATION);
        RETURN(0);
    } 
    
    lua_getglobal(LUA, "EXVAR_EXPRESSION");
    if (lua_isnil(LUA, -1)) {
        MessageBox(hwnd, "The specified expression evaluates to nil.\n"
            "This is probably not what you want.",
            "Oops", MB_ICONEXCLAMATION);
        lua_pop(LUA, 1);
        RETURN(0);
    }
    if (lua_isfunction(LUA, -1)) {
        MessageBox(hwnd, "The specified expression evaluates to a function.\n\n"
            "You can't save Lua functions directly in exvars.\n"
            "Any DSB code that stores information about a function\n"
            "in an exvar stores its name as a string instead.\n",
            "Oops", MB_ICONEXCLAMATION);
        lua_pop(LUA, 1);
        RETURN(0);
    }
    lua_pop(LUA, 1);   
    RETURN(1);
}

int ed_inside_move(struct inst *in_what, int moving, int from, int to) 
{
    int swapped = in_what->inside[to];
    
    onstack("ed_inside_move");
    
    in_what->inside[to] = in_what->inside[from];
    in_what->inside[from] = swapped;
    oinst[moving]->y = to;    
    if (swapped) {
        oinst[swapped]->y = from;
    }
    
    RETURN(swapped);
}

INT_PTR CALLBACK ESB_build_container(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    int do_item_fill = 0;
    int do_conedit = 0;
    int terminating = 0;
    struct ext_hwnd_info *xinfo;
    
    onstack("ESB_build_container"); // container builder window

    switch(message) { 
        case WM_COMMAND: {
            int hw = HIWORD(wParam);
            xinfo = get_hwnd(hwnd);
            
            switch(LOWORD(wParam)) {
                case IDCANCEL: {
                    terminating = 1;
                    EndDialog(hwnd, 0);    
                } break;
                        
                case ESB_CONTITEMS: {
                    if (hw == LBN_DBLCLK) {
                        do_conedit = 1;
                    }
                } break;
                
                case ESB_CT_ADD: {
                    do_item_fill = 1;                      
                } break;
                
                case ESB_CT_EDIT: {
                    do_conedit = 1;
                } break;
                
                case ESB_CT_DEL: {
                    int sidx;
                    int cinst = ed_optinst(hwnd, ESB_CONTITEMS, &sidx);
                    if (cinst) {
                        recursive_inst_destroy(cinst, 0);
                        if (sidx > 0) --sidx;
                        ed_fillboxfromtile(hwnd, ESB_CONTITEMS,
                            LOC_IN_OBJ, xinfo->inst, 0, sidx); 
                    }
                } break;
                
                case ESB_CT_COMP: { // compress button
                    struct inst *p_inst = oinst[xinfo->inst];
                    int sidx = 0; 
                    int pushes = 1;
                    int cinst = ed_optinst(hwnd, ESB_CONTITEMS, &sidx);
                    while (pushes && p_inst->inside_n > 1) {
                        int i;
                        pushes = 0;
                        for(i=1;i<p_inst->inside_n;++i) {
                            if (p_inst->inside[i] && !p_inst->inside[i-1]) {
                                int moving = p_inst->inside[i];
                                ed_inside_move(p_inst, moving, i, i-1);
                                pushes++;
                            }
                        }       
                    }
                    ed_fillboxfromtile(hwnd, ESB_CONTITEMS,
                        LOC_IN_OBJ, xinfo->inst, 0, -1);
                    ed_reselect_inst(hwnd, ESB_CONTITEMS, cinst); 
                } break;
                
                case ESB_CT_MOVEUP: {
                    struct inst *p_inst = oinst[xinfo->inst];
                    int sidx;
                    int cinst = ed_optinst(hwnd, ESB_CONTITEMS, &sidx);
                    if (cinst) {
                        int slot = oinst[cinst]->y;
                        if (slot > 0) {
                            int swapped = ed_inside_move(p_inst, cinst, slot, slot-1);
                            if (swapped) sidx--;
                            ed_fillboxfromtile(hwnd, ESB_CONTITEMS,
                                LOC_IN_OBJ, xinfo->inst, 0, sidx); 
                        }
                    }
                } break;
                
                case ESB_CT_MOVEDOWN: {
                    struct inst *p_inst = oinst[xinfo->inst];
                    int sidx;
                    int cinst = ed_optinst(hwnd, ESB_CONTITEMS, &sidx);
                    if (cinst) {
                        int slot = oinst[cinst]->y;
                        if (slot < (p_inst->inside_n - 1)) {
                            int swapped = ed_inside_move(p_inst, cinst, slot, slot+1);
                            if (swapped) sidx++;
                            ed_fillboxfromtile(hwnd, ESB_CONTITEMS,
                                LOC_IN_OBJ, xinfo->inst, 0, sidx); 
                        }
                    }                   
                } break;
            }
            
            if (do_conedit) {
                int sidx, cinst;
                cinst = ed_optinst(hwnd, ESB_CONTITEMS, &sidx);
                if (cinst) {
                    edg.op_inst = cinst;
                    DialogBox(GetModuleHandle(NULL),
                        MAKEINTRESOURCE(ESB_EXVEDIT),
                        hwnd, ESBdproc);
                    ed_fillboxfromtile(hwnd, ESB_CONTITEMS,
                        LOC_IN_OBJ, xinfo->inst, 0, sidx); 
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
                    do_item_fill = 1;
                } break;
                    
                case TVN_DELETEITEM:
                    clbr_del(lParam);
                    break;
            }  
        } break;
              
        case WM_INITDIALOG: {
            HWND treehwnd = GetDlgItem(hwnd, ESB_CONTREE);
            ed_build_tree(treehwnd, 4, NULL);
            TreeView_Expand(treehwnd, TreeView_GetRoot(treehwnd), TVE_EXPAND);
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            xinfo->sel_str = NULL;            
            ed_fillboxfromtile(hwnd, ESB_CONTITEMS,
                LOC_IN_OBJ, edg.op_inst, 0, -1);  
        } break;

        case WM_CLOSE: {
            terminating = 1;
            EndDialog(hwnd, 0);
        } break;
        
        case WM_DESTROY: {
            terminating = 1;
            del_hwnd(hwnd);
        } break;
        
        default:
            RETURN(FALSE);
    }
    
    if (!terminating) {
        if (do_item_fill && xinfo->sel_str) {
            struct inst *p_inst = oinst[xinfo->inst];
            int freespot = 0;
            if (p_inst->inside_n) {
                while (freespot < p_inst->inside_n &&
                    p_inst->inside[freespot])
                {
                    freespot++;
                }
            }
            ed_obj_add(xinfo->sel_str, LOC_IN_OBJ, xinfo->inst, freespot, 0);
            ed_fillboxfromtile(hwnd, ESB_CONTITEMS, LOC_IN_OBJ,
                xinfo->inst, 0, freespot);  
        } 
    }

    RETURN(TRUE);
}


INT_PTR CALLBACK ESB_add_exvar_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    int i_res;

    onstack("ESB_add_exvar_proc");

    switch(message) { 
        case WM_COMMAND: {
            int lw = LOWORD(wParam);
            if (lw == ESB_ADDOK) {
                char buffer[60];
                char *bptr = buffer;
                GetDlgItemText(hwnd, ESB_NEWNAME, buffer, sizeof(buffer));
                if (*bptr == 0)
                    RETURN(TRUE);
                while (*bptr) {
                    if (*bptr != '_' && 
                        !isalpha(*bptr) && 
                        (bptr == buffer || !isdigit(*bptr))) 
                    {
                        MessageBox(hwnd, "Illegal name.\n"
                            "Value must be alphanumeric.", "Error", MB_ICONEXCLAMATION);
                        RETURN(TRUE);
                    }
                    bptr++;
                }
                edg.exvn_string = dsbstrdup(buffer);
                EndDialog(hwnd, 0); 
            } else if (lw == IDCANCEL) {
                EndDialog(hwnd, 0);
            }   
        } break;
        
        case WM_INITDIALOG: {
            if (edg.w_t_string)
                SetWindowText(hwnd, edg.w_t_string);
            SetDlgItemText(hwnd, ESB_AEXTITLE, edg.exvn_string);
            edg.exvn_string = NULL;
        } break; 

        case WM_CLOSE: {
            edg.exvn_string = NULL;
            EndDialog(hwnd, 0);
        } break;
        
        default:
            RETURN(FALSE);
    }

    RETURN(TRUE);
}

INT_PTR CALLBACK ESB_edit_exvar_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    int i_res;

    onstack("ESB_edit_exvar_proc");

    switch(message) { 
        case WM_COMMAND: {
            struct ext_hwnd_info *xinfo = get_hwnd(hwnd);
            if (xinfo && xinfo->inst && oinst[xinfo->inst]) {
                int lw = LOWORD(wParam);
                if (lw == IDOK || lw == ESB_EXVAREDITUSE) {
                    int isok = 1;
                    char *buffer = dsbfastalloc(1600);
                    memset(buffer, 0, 1600);
                    edg.completion_flag = 0;
                    
                    GetDlgItemText(hwnd, ESB_EXVBOX, buffer, 1600);
                    if (ed_initial_validate(buffer)) {
                        char *bptr = buffer;
                        while (*bptr == ' ') bptr++;
                        if (ed_lua_alert_niltest(hwnd, bptr)) {
                            char *lbuffer = dsbfastalloc(1600);
                            snprintf(lbuffer, 1600, "%s[%ld].%s = %s", xinfo->data_prefix,
                                xinfo->inst, edg.exvn_string, bptr);
                            if (luaL_dostring(LUA, lbuffer)) {
                                MessageBox(hwnd, lua_tostring(LUA, -1),
                                    "Mysterious Lua Error", MB_ICONSTOP);
                                isok = 0;
                            }
                        } else
                            isok = 0;
                    } else {
                        isok = 0;
                        MessageBox(hwnd, "Syntax error.\n\nMake sure "
                            "tables are closed properly, and\n"
                            "there are no function calls.",
                            "Error", MB_ICONEXCLAMATION);
                    } 
                    
                    if (isok) {
                        edg.completion_flag = 1;
                        EndDialog(hwnd, 0);
                    }  
                } else if (lw == IDCANCEL) {
                    EndDialog(hwnd, 0);
                }
            }   
        } break; 
              
        case WM_INITDIALOG: // 272
        if (edg.ch_ex_mode || (edg.op_inst && oinst[edg.op_inst])) {
            struct ext_hwnd_info *xinfo;
            char s_txt[100];
            SetDlgItemText(hwnd, ESB_EXVBOX, edg.op_string);                       
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            sprintf(xinfo->data_prefix, "%s", edg.data_prefix);
            snprintf(s_txt, sizeof(s_txt), "Edit %s[%d].%s",
                edg.data_prefix, edg.op_inst, edg.exvn_string);
            SetWindowText(hwnd, s_txt);
            
            SendDlgItemMessage(hwnd, ESB_EXVAREDITUSE, WM_SETFONT,
                (int)GetStockObject(DEFAULT_GUI_FONT), 0);
        }
        break;

        case WM_CLOSE: {
            edg.completion_flag = 0;
            EndDialog(hwnd, 0);
        }
        break;
        
        case WM_DESTROY: {
            del_hwnd(hwnd);
        } break;
        
        default:
            RETURN(FALSE);
    }

    RETURN(TRUE);
}

INT_PTR CALLBACK ESB_level_info_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    onstack("ESB_level_info_proc");
    
    switch(message) { 
        case WM_COMMAND: {
            int lw = LOWORD(wParam);
            switch (lw) {
                case ESB_LI_OK: {
                    int l = edg.ed_lev;
                    char n_wallset[120];
                    int newx, newy;
                    int n_light;
                    int n_xpm;
                    int b_iter;
                    int b_inac;
                    unsigned int dlf_flags = 0;
                    struct dungeon_level *dd = &(dun[l]);
                    newx = GetDlgItemInt(hwnd, ESB_XSIZ, NULL, FALSE);
                    newy = GetDlgItemInt(hwnd, ESB_YSIZ, NULL, FALSE);
                    n_light = GetDlgItemInt(hwnd, ESB_LIGHTLEVEL, NULL, FALSE);
                    n_xpm = GetDlgItemInt(hwnd, ESB_XPM, NULL, FALSE);
                    GetDlgItemText(hwnd, ESB_WALLSET, n_wallset, sizeof(n_wallset));
                    if (newx < 1 || newx > 255 || newy < 1 || newy > 255) {
                        MessageBox(hwnd, "Level dimensions must be between 1 and 255.", "Invalid Level Size", MB_ICONEXCLAMATION);
                        RETURN(TRUE);
                    }
                    if (n_light < 0 || n_light > 100) {
                        MessageBox(hwnd, "Light level must be between 0 and 100.", "Invalid Light Level", MB_ICONEXCLAMATION);
                        RETURN(TRUE);
                    }
                    if (n_xpm < 0 || n_xpm > 100) {
                        MessageBox(hwnd, "XP Multiplier must be between 0 and 100.", "Invalid XP Multiplier", MB_ICONEXCLAMATION);
                        RETURN(TRUE);
                    }
                    
                    if (newx != dd->xsiz || newy != dd->ysiz) {
                        struct dungeon_level ndd;
                        int v, xx, yy;
                        int lx, ly;
                        memset(&ndd, 0, sizeof(struct dungeon_level));
                        if (newx < dd->xsiz || newy < dd->ysiz) {
                            int z = MessageBox(hwnd,
                                "You are reducing the size of this level.\n"
                                "Insts in the reduced portion will be completely deleted!\n"
                                "Are you sure you want to proceed?", "Deletion Warning",
                                    MB_ICONEXCLAMATION|MB_YESNO);
                            if (z != 6) {
                                RETURN(TRUE);
                            } else {
                                int i;
                                char *marked = dsbfastalloc(NUM_INST);
                                memset(marked, 0, NUM_INST);
                                for (i=0;i<NUM_INST;++i) {
                                    if (oinst[i]) {
                                        struct inst *p_inst = oinst[i];
                                        if (p_inst->level == l &&
                                            (p_inst->x >= newx || p_inst->y >= newy)) 
                                        {
                                            inst_destroy(i, 0);
                                            marked[i] = 1;
                                        }
                                    }
                                } 
                                for (i=0;i<NUM_INST;++i) {
                                    if (marked[i]) {
                                        lc_parm_int("clean_up_target_exvars", 1, i);
                                    }
                                }
                                
                                luastacksize(4);
                                lua_getglobal(LUA, "_ESBLEVELUSE_WALLSETS");
                                for(yy=0;yy<newy;++yy) {
                                    for(xx=0;xx<newx;++xx) {
                                        if (xx >= newx || yy >= newy) {
                                            int d;
                                            for(d=0;d<5;++d) {
                                                int f = wscomp(l, xx, yy, d);
                                                lua_pushinteger(LUA, f);
                                                lua_pushnil(LUA);
                                                lua_settable(LUA, -3);
                                            }
                                        }
                                    }
                                }
                                lua_pop(LUA, 1);
                                      
                            }    
                        }
                        
                        // it's easier to alloc a new block and copy
                        // than messing around with all the other crap...
                        ndd.t = dsbcalloc(newy, sizeof(struct dtile*));
                        for(v=0;v<newy;++v)
                            ndd.t[v] = dsbcalloc(newx, sizeof(struct dtile));
                        for(yy=0;yy<newy;++yy) {
                            for(xx=0;xx<newx;++xx) {
                                ndd.t[yy][xx].w = 1;
                            }
                        }
                        lx = dd->xsiz;
                        if (newx < lx) lx = newx;
                        ly = dd->ysiz;
                        if (newy < ly) ly = newy;
                        for (yy=0;yy<ly;++yy) {
                            memcpy(ndd.t[yy], dd->t[yy], sizeof(struct dtile) * lx);
                        }
                        
                        for(v=0;v<dd->ysiz;++v)
                            dsbfree(dd->t[v]);
                        dsbfree(dd->t);
                        
                        dd->t = ndd.t;
                        dd->xsiz = newx;
                        dd->ysiz = newy;
                        ed_init_level_bmp(edg.ed_lev, sys_hwnd);
                        ed_resizesubs(sys_hwnd);   
                    }
                    
                    luastacksize(6);
                    lua_getglobal(LUA, "_ESBWALLSET_NAMES");
                    lua_pushinteger(LUA, l);
                    lua_pushstring(LUA, n_wallset);
                    lua_settable(LUA, -3);
                    lua_pop(LUA, 1);
                    
                    dd->lightlevel = n_light;
                    dd->xp_multiplier = n_xpm;
                    
                    b_iter = SendDlgItemMessage(hwnd, ESB_LI_ITERAT, BM_GETCHECK, 0, 0); 
                    if (b_iter) dlf_flags |= DLF_ITERATIVE_CEIL;
                    
                    b_inac = SendDlgItemMessage(hwnd, ESB_LI_INACTIVE, BM_GETCHECK, 0, 0); 
                    if (b_inac) dlf_flags |= DLF_START_INACTIVE;
                    
                    dd->level_flags = dlf_flags;
                    
                    ESB_go_up_one(hwnd);
                } break;
                
                case IDCANCEL:
                case ESB_LI_CANCEL:
                    SendMessage(hwnd, WM_CLOSE, 0, 0);
                break;
            }
        } break;
        
        case WM_INITDIALOG: {
            const char *lstring;
            char dt[200];
            int l = edg.ed_lev;
            struct dungeon_level *dd = &(dun[l]);
            
            sprintf(dt, "%ld", dd->xsiz);
            SetDlgItemText(hwnd, ESB_XSIZ, dt);
        
            sprintf(dt, "%ld", dd->ysiz);
            SetDlgItemText(hwnd, ESB_YSIZ, dt);
            
            sprintf(dt, "%ld", dd->lightlevel);
            SetDlgItemText(hwnd, ESB_LIGHTLEVEL, dt);
            
            sprintf(dt, "%ld", dd->xp_multiplier);
            SetDlgItemText(hwnd, ESB_XPM, dt);
            
            if (dd->level_flags & DLF_ITERATIVE_CEIL)
                SendDlgItemMessage(hwnd, ESB_LI_ITERAT, BM_SETCHECK, BST_CHECKED, 0); 
                                
            if (dd->level_flags & DLF_START_INACTIVE)
                SendDlgItemMessage(hwnd, ESB_LI_INACTIVE, BM_SETCHECK, BST_CHECKED, 0); 
                        
            luastacksize(5);
            edg.op_hwnd = hwnd;
            edg.op_b_id = ESB_WALLSET;
            edg.op_winmsg = CB_ADDSTRING;           
            lc_parm_int("__ed_add_wallsets", 0);
            
            lua_getglobal(LUA, "_ESBWALLSET_NAMES");
            lua_pushinteger(LUA, l);
            lua_gettable(LUA, -2);
            lstring = lua_tostring(LUA, -1); 
            SendDlgItemMessage(hwnd, ESB_WALLSET,
                CB_SELECTSTRING, -1, (int)lstring);
            lua_pop(LUA, 2);
            lstacktop();
            
        } break;
        
        case WM_CLOSE: {
            ESB_go_up_one(hwnd);
        } break;
                
        default:
            RETURN(FALSE);
    }
    RETURN(TRUE);
}

INT_PTR CALLBACK ESB_global_info_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    char ws_store_string[40];
    onstack("ESB_global_info_proc");
    
    switch(message) { 
        case WM_COMMAND: {
            int lw = LOWORD(wParam);
            switch (lw) {
                case ESB_ADDWS: {
                    edg.w_t_string = "Add wallset";
                    edg.exvn_string = "What would you like to call the new wallset?";
                    DialogBox(GetModuleHandle(NULL),
                        MAKEINTRESOURCE(ESB_EXVARADD),
                        hwnd, ESB_add_exvar_proc);
                    if (edg.exvn_string) {
                        luastacksize(4);
                        lua_getglobal(LUA, "_ESB_WALLSETS_USED");
                        lua_pushstring(LUA, edg.exvn_string);
                        lua_pushboolean(LUA, 1);
                        lua_settable(LUA, -3);
                        lua_pop(LUA, 1);
                        SendDlgItemMessage(hwnd, ESB_GWSLIST, LB_RESETCONTENT, 0, 0);
                        edg.op_hwnd = hwnd;
                        edg.op_b_id = ESB_GWSLIST;
                        edg.op_winmsg = LB_ADDSTRING;           
                        lc_parm_int("__ed_add_wallsets", 0); 
                        
                        ed_main_treeview_refresh_wallsets();
                        snprintf(ws_store_string, sizeof(ws_store_string),
                            "%s", edg.exvn_string);
                        dsbfree(edg.draw_ws);
                        edg.draw_ws = dsbstrdup(ws_store_string);            
                    }
                } break;
                
                case ESB_DELWS: {
                    int csel;
                    csel = SendDlgItemMessage(hwnd, ESB_GWSLIST, LB_GETCURSEL, 0, 0);
                    if (csel != LB_ERR) {
                        int cv;
                        char wstxt[150];
                        SendDlgItemMessage(hwnd, ESB_GWSLIST, LB_GETTEXT, csel, (int)wstxt);
                        luastacksize(5);
                        lua_getglobal(LUA, "__ed_wallset_usage_check");
                        lua_pushstring(LUA, wstxt);
                        cv = lc_call_topstack(1, "delete_wallset");
                        if (cv) {
                            MessageBox(hwnd, "Cannot delete.\nThat wallset is in use in the dungeon.\n",
                                "Cannot delete wallset", MB_ICONEXCLAMATION);
                            RETURN(TRUE); 
                        }
                        lua_getglobal(LUA, "_ESB_WALLSETS_USED");
                        lua_pushstring(LUA, wstxt);
                        lua_pushnil(LUA);
                        lua_settable(LUA, -3);
                        lua_pop(LUA, 1); 
                        SendDlgItemMessage(hwnd, ESB_GWSLIST, LB_RESETCONTENT, 0, 0);
                        edg.op_hwnd = hwnd;
                        edg.op_b_id = ESB_GWSLIST;
                        edg.op_winmsg = LB_ADDSTRING;           
                        lc_parm_int("__ed_add_wallsets", 0);
                        
                        ed_main_treeview_refresh_wallsets();
                        dsbfree(edg.draw_ws);
                        edg.draw_ws = NULL;     
                    }   
                } break;
                
                case ESB_LEVCOMMIT: {
                    int nl = GetDlgItemInt(hwnd, ESB_SETLEVELS, NULL, FALSE);
                    int sizdif = (nl - gd.dungeon_levels);
                    if (nl < 1 || nl > 512) {
                        MessageBox(hwnd, "The number of levels must be between 1 and 512.", "Invalid Number of Levels", MB_ICONEXCLAMATION);
                        RETURN(TRUE);   
                    } 
                    if (nl == gd.dungeon_levels) {
                        SendMessage(hwnd, WM_CLOSE, 0, 0);
                        RETURN(TRUE);
                    }
                    if (nl < gd.dungeon_levels) {
                        int z = MessageBox(hwnd,
                            "You are reducing the number of levels.\n"
                            "Everything in the removed levels will be completely deleted!\n"
                            "Are you sure you want to proceed?", "Deletion Warning",
                                MB_ICONEXCLAMATION|MB_YESNO);
                        if (z != 6) {
                            RETURN(TRUE);
                        } else {
                            int i;
                            char *marked = dsbfastalloc(NUM_INST);
                            memset(marked, 0, NUM_INST);
                            for (i=0;i<NUM_INST;++i) {
                                if (oinst[i]) {
                                    struct inst *p_inst = oinst[i];
                                    if (p_inst->level >= nl) {
                                        inst_destroy(i, 0);
                                        marked[i] = 1;
                                    }
                                }
                            } 
                            for (i=0;i<NUM_INST;++i) {
                                if (marked[i]) {
                                    lc_parm_int("clean_up_target_exvars", 1, i);
                                }
                            }
                        }                        
                    }
                    
                    luastacksize(5);
                    lua_getglobal(LUA, "_ESBWALLSET_NAMES");
                    if (sizdif < 0) {
                        while (gd.dungeon_levels > nl) {
                            int gdi = gd.dungeon_levels - 1;
                            struct dungeon_level *dd = &(dun[gdi]);
                            destroy_dungeon_level(dd);
                            --gd.dungeon_levels;
                            lua_pushinteger(LUA, gdi);
                            lua_pushnil(LUA);
                            lua_settable(LUA, -3);
                        }
                    }
                    dun = dsbrealloc(dun, sizeof(struct dungeon_level) * nl);
                    if (sizdif > 0) {
                        int i;
                        memset(&(dun[gd.dungeon_levels]), 0,
                            sizeof(struct dungeon_level) * sizdif);
                            
                        // hack to ensure default is available
                        lua_getglobal(LUA, "_ESB_WALLSETS_USED");
                        lua_pushstring(LUA, "default");
                        lua_pushboolean(LUA, 1);
                        lua_settable(LUA, -3);
                        lua_pop(LUA, 1);
                    
                        for(i=gd.dungeon_levels;i<nl;++i) {
                            int xx, yy;
                            int v;
                            
                            dun[i].xsiz = 32;
                            dun[i].ysiz = 32;
                            dun[i].lightlevel = 0;
                            dun[i].xp_multiplier = i/2 + 1;
                            dun[i].tint_intensity = DSB_DEFAULT_TINT_INTENSITY;
                            
                            luastacksize(4);
                            
                            lua_pushinteger(LUA, i);
                            lua_pushstring(LUA, "default");
                            lua_settable(LUA, -3);
                            
                            dun[i].t = dsbcalloc(32, sizeof(struct dtile*));
                            for(v=0;v<32;++v)
                                dun[i].t[v] = dsbcalloc(32, sizeof(struct dtile));
                            
                            for(yy=0;yy<32;++yy) {
                                for(xx=0;xx<32;++xx) {
                                    dun[i].t[yy][xx].w = 1;
                                }
                            }    
                        }
                    }                        
                    gd.dungeon_levels = nl;
                    lua_pop(LUA, 1);
                    lua_pushinteger(LUA, nl - 1);
                    lua_setglobal(LUA, "ESB_LAST_LEVEL");
                    
                    if (edg.ed_lev >= gd.dungeon_levels) {
                        edg.ed_lev = gd.dungeon_levels - 1;
                        ed_init_level_bmp(edg.ed_lev, sys_hwnd);
                        ed_resizesubs(sys_hwnd);                        
                    } 
                    
                    MessageBox(hwnd, "Number of levels changed.\n", "Success", MB_ICONINFORMATION);                    
                    SendMessage(hwnd, WM_CLOSE, 0, 0);                   
                                         
                } break;
                
                case ESB_PSET: {
                    int npx, npy, npl, npf;
                    HWND fdirbox = GetDlgItem(hwnd, ESB_STARTFACE);
                    
                    npl = GetDlgItemInt(hwnd, ESB_STARTLEV, 0, 0);
                    npx = GetDlgItemInt(hwnd, ESB_STARTX, 0, 0); 
                    npy = GetDlgItemInt(hwnd, ESB_STARTY, 0, 0);
                    if (npl < 0 || npl >= gd.dungeon_levels) {
                        char errmsg[200];
                        snprintf(errmsg, sizeof(errmsg), "Start level out of range.\nValue must be between 0 and %ld",
                            gd.dungeon_levels - 1);
                        MessageBox(hwnd, errmsg, "Bad Party Start Position", MB_ICONEXCLAMATION);  
                        RETURN(TRUE);
                    }
                    if (npx < 0 || npx >= dun[npl].xsiz) {
                        char errmsg[200];
                        snprintf(errmsg, sizeof(errmsg), "Start X out of range.\nValue must be between 0 and %ld",
                            dun[npl].xsiz - 1);
                        MessageBox(hwnd, errmsg, "Bad Party Start Position", MB_ICONEXCLAMATION);  
                        RETURN(TRUE); 
                    }
                    if (npy < 0 || npy >= dun[npl].ysiz) {
                        char errmsg[200];
                        snprintf(errmsg, sizeof(errmsg), "Start Y out of range.\nValue must be between 0 and %ld",
                            dun[npl].ysiz - 1);
                        MessageBox(hwnd, errmsg, "Bad Party Start Position", MB_ICONEXCLAMATION);  
                        RETURN(TRUE); 
                    }  
                    
                    gd.p_x[0] = npx;
                    gd.p_y[0] = npy;
                    gd.p_lev[0] = npl;
                    gd.p_face[0] = SendMessage(fdirbox, CB_GETCURSEL, 0, 0); 
                    
                    force_redraw(edg.subwin, 1);                  
                } break;
                
                case IDCANCEL:
                    SendMessage(hwnd, WM_CLOSE, 0, 0);
                break;
            }
        } break;
        
        case WM_INITDIALOG: {
            HWND fdirbox = GetDlgItem(hwnd, ESB_STARTFACE);
            char dlgtext[120];
            sprintf(dlgtext, "The dungeon currently has %ld level%s", gd.dungeon_levels,
                (gd.dungeon_levels == 1) ? "." : "s.");
            SetDlgItemText(hwnd, ESB_CURLEVELS, dlgtext);
            SetDlgItemInt(hwnd, ESB_SETLEVELS, gd.dungeon_levels, 0);
            
            edg.op_hwnd = hwnd;
            edg.op_b_id = ESB_GWSLIST;
            edg.op_winmsg = LB_ADDSTRING;           
            lc_parm_int("__ed_add_wallsets", 0);
            
            SendMessage(fdirbox, CB_ADDSTRING, 0, (int)"North");
            SendMessage(fdirbox, CB_ADDSTRING, 0, (int)"East");
            SendMessage(fdirbox, CB_ADDSTRING, 0, (int)"South");
            SendMessage(fdirbox, CB_ADDSTRING, 0, (int)"West");
            SendMessage(fdirbox, CB_SETCURSEL, gd.p_face[0], 0);
            
            SetDlgItemInt(hwnd, ESB_STARTLEV, gd.p_lev[0], 0);
            SetDlgItemInt(hwnd, ESB_STARTX, gd.p_x[0], 0);
            SetDlgItemInt(hwnd, ESB_STARTY, gd.p_y[0], 0);
                        
        } break;
        
        case WM_CLOSE: {
            ESB_go_up_one(hwnd);
        } break;
                
        default:
            RETURN(FALSE);
    }
    RETURN(TRUE);
}

INT_PTR CALLBACK ESBoptionsproc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    onstack("ESBoptionsproc");
    switch(message) { 
        case WM_COMMAND: {
            int lw = LOWORD(wParam);
            if (lw == ESB_PROK || lw == IDOK) {
                EDITFLAG_FROM_BOX(EEF_NOLIMBOSAVE, ESB_PR_LIMBO);
                EDITFLAG_FROM_BOX(EEF_FLOORLOCK, ESB_PR_FLLOCK);
                EDITFLAG_FROM_BOX(EEF_ALCOVE, ESB_PR_ALCOVE);
                EDITFLAG_FROM_BOX(EEF_CEILPIT, ESB_PR_CEILPIT);
                EDITFLAG_FROM_BOX(EEF_DOORFRAME, ESB_PR_DOORF);
                GAMEFLAG_FROM_BOX(GP_CSB_REINCAR, ESB_PR_CSBREIN);
                GAMEFLAG_FROM_BOX(GP_END_EMPTY, ESB_PR_ENDNOPARTY);
                GAMEFLAG_FROM_BOX(GP_NOCLOBBER_DROP, ESB_PR_NOCLOBBERCZ); 
                GAMEFLAG_FROM_BOX(GP_FAST_ADVANCE, ESB_PR_ADVANCE);
                GAMEFLAG_FROM_BOX(GP_ONE_WALLITEM, ESB_PR_ONEWALLITEM);
                GAMEFLAG_FROM_BOX(GP_NO_LAUNCH_TELEPORT, ESB_PR_NOLAUNCHTELEPORT);
                GAMEFLAG_FROM_BOX(GP_ZERO_EXP_MOD, ESB_PR_ZEROEXP);
                GAMEFLAG_FROM_BOX(GP_MAX_STAT_BONUS, ESB_PR_FULLSTATS);
                luastacksize(2);
                lua_pushinteger(LUA, edg.editor_flags);
                lua_setglobal(LUA, "EDITOR_FLAGS"); 
                ESB_go_up_one(hwnd);                  
            } else if (lw == ESB_PRCAN || lw == IDCANCEL) {
                ESB_go_up_one(hwnd);
            } 
        }   
        break; 
              
        case WM_INITDIALOG: // 272
            EDITFLAG_TO_BOX(EEF_NOLIMBOSAVE, ESB_PR_LIMBO);
            EDITFLAG_TO_BOX(EEF_FLOORLOCK, ESB_PR_FLLOCK);
            EDITFLAG_TO_BOX(EEF_ALCOVE, ESB_PR_ALCOVE);
            EDITFLAG_TO_BOX(EEF_CEILPIT, ESB_PR_CEILPIT);
            EDITFLAG_TO_BOX(EEF_DOORFRAME, ESB_PR_DOORF);
            GAMEFLAG_TO_BOX(GP_CSB_REINCAR, ESB_PR_CSBREIN);
            GAMEFLAG_TO_BOX(GP_END_EMPTY, ESB_PR_ENDNOPARTY);
            GAMEFLAG_TO_BOX(GP_NOCLOBBER_DROP, ESB_PR_NOCLOBBERCZ);
            GAMEFLAG_TO_BOX(GP_FAST_ADVANCE, ESB_PR_ADVANCE);
            GAMEFLAG_TO_BOX(GP_ONE_WALLITEM, ESB_PR_ONEWALLITEM);
            GAMEFLAG_TO_BOX(GP_NO_LAUNCH_TELEPORT, ESB_PR_NOLAUNCHTELEPORT);
            GAMEFLAG_TO_BOX(GP_ZERO_EXP_MOD, ESB_PR_ZEROEXP);
            GAMEFLAG_TO_BOX(GP_MAX_STAT_BONUS, ESB_PR_FULLSTATS);
        break;

        case WM_CLOSE: {
            ESB_go_up_one(hwnd);
        }
        break;
                
        default:
            RETURN(FALSE);
    }

    RETURN(TRUE);
}

BITMAP *ESBpcxload(char *afilename) {
    BITMAP *bmp;
    
    onstack("ESBpcxload");
    
    bmp = load_bitmap(afilename, NULL);
    if (bmp == NULL) {
        char err[40];
        snprintf(err, sizeof(err), "Could not load: %s", afilename);
        poop_out(err);
    }
    RETURN(bmp);
}

BITMAP *ed_create_comp(struct dungeon_level *dd, int xx, int yy, int ch) {
    BITMAP *comp_bmp;
    int ilc;
    struct edraw_list *ddraw_list = NULL;
    int cs;
    
    onstack("ed_create_comp");

    cs = (dd->t[yy][xx].w & 1);

    for(ilc=0;ilc<=4;++ilc) {
        struct inst_loc *dt = dd->t[yy][xx].il[ilc];
        while (dt != NULL) {
            unsigned int inst = dt->i;
            struct inst *p_inst = oinst[inst];
            struct obj_arch *p_arch = Arch(p_inst->arch);
            int num, ord, act;

            if (p_inst->gfxflags & OF_INACTIVE)
                act = 1;
            else
                act = 0;

            ed_get_drawnumber(p_arch, ilc, act, cs, &num, &ord);

            if (num >= 0) {
                struct edraw_list *ne;

                ne = dsbfastalloc(sizeof(struct edraw_list));
                ne->num = num;
                ne->pri = ord;
                ne->dest_x = 0;
                ne->dest_y = 0;
                ne->n = NULL;

                if (ddraw_list) {
                    if (ord <= ddraw_list->pri) {
                        ne->n = ddraw_list;
                        ddraw_list = ne;
                    } else {
                        struct edraw_list *cr = ddraw_list;

                        while (cr->n && ord > cr->n->pri)
                            cr = cr->n;

                        if (cr->n)
                            ne->n = cr->n;

                        cr->n = ne;
                    }

                } else
                    ddraw_list = ne;
            }

            dt = dt->n;
        }
    }

    comp_bmp = create_bitmap(ch, ch);
    blit(edg.cells, comp_bmp, ch*cs, 0, 0, 0, ch, ch);

    if (ddraw_list) {
        struct edraw_list *dr = ddraw_list;

        while (dr != NULL) {
            int xc = (dr->num % 8);
            int yc = (dr->num / 8) + 1;

            masked_blit(edg.cells, comp_bmp, xc*ch, yc*ch,
                dr->dest_x, dr->dest_y, ch, ch);

            dr = dr->n;
        }

        ddraw_list = NULL;
    }
    
    RETURN(comp_bmp);
}

void ed_paint_wallset(BITMAP *scx, int l,
    int xx, int yy, int drawx, int drawy) 
{
    struct dungeon_level *dd = &(dun[l]);
    int cell = (dd->t[yy][xx].w & 1);
    int d;
    for(d=0;d<5;++d) {
        int bn;
        int cmp = wscomp(l, xx, yy, d);
        
        bn = 4 + d;
        if (d == 4) {
            if (cell) bn = 3;
            else bn = 2;
        }
        
        lua_pushinteger(LUA, cmp);
        lua_gettable(LUA, -2);
        if (lua_isstring(LUA, -1)) {
            int ch = edg.cells->w / 8;
            masked_blit(edg.cells, scx, bn * ch, 0, 
                (drawx*ch), (drawy*ch), ch, ch);
        }
        lua_pop(LUA, 1);   
    }
}

void ed_draw_backbuffer(BITMAP *scx, int i_alevel, int xdf_flags) {

    onstack("ed_draw_backbuffer");

    if (i_alevel >= 0) {
        struct dungeon_level *dd;
        int ch = edg.cells->w / 8;
        int xx, yy;
        BITMAP *sbmp;
        int xmax, ymax;

        dd = &(dun[i_alevel]);

        xmax = dd->xsiz;
        ymax = dd->ysiz;
        
        for(yy=0;yy<ymax;++yy) {
            for(xx=0;xx<xmax;++xx) {
                sbmp = ed_create_comp(dd, xx, yy, ch);
                blit(sbmp, scx, 0, 0, (xx*ch), (yy*ch), ch, ch);
                destroy_bitmap(sbmp);
            }
        }
        
        if (xdf_flags & XDF_WSPAINT) {
            luastacksize(6);
            lua_getglobal(LUA, "_ESBLEVELUSE_WALLSETS");
            for(yy=0;yy<ymax;++yy) {
                for(xx=0;xx<xmax;++xx) {
                    ed_paint_wallset(scx, i_alevel, xx, yy, xx, yy);
                }
            }
            lua_pop(LUA, 1);
        }
        
        if (xdf_flags & XDF_PARTYPOS) {
            if (i_alevel == gd.p_lev[0]) {
                int s_mark_w = edg.marked_cell->w / 4;
                masked_blit(edg.marked_cell, scx, s_mark_w * gd.p_face[0], 0,
                    gd.p_x[0]*ch - (ch/2), gd.p_y[0]*ch - (ch/2),
                    s_mark_w, s_mark_w);
            }
        }
        
        if (edg.op_picker_val && edg.op_x >= 0 &&
            i_alevel == edg.op_p_lev) 
        {
            int ch32 = (ch * 3) / 2;
            int pvi = edg.op_picker_val - 1;
            masked_blit(edg.marked_cell, scx, pvi * ch32, ch*2,
                edg.op_x*ch - (ch/4), edg.op_y*ch - (ch/4), ch32, ch32);            
        }
        
        if (i_alevel == edg.global_mark_level) {
            int ch32 = (ch * 3) / 2;
            int pvi = 1;
            masked_blit(edg.marked_cell, scx, pvi * ch32, ch*2,
                edg.global_mark_x*ch - (ch/4), edg.global_mark_y*ch - (ch/4), ch32, ch32);  
        }            
    }
    
    VOIDRETURN();
}

void ed_main_draw(HWND ehwnd, BITMAP *backbuffer, HDC hdc,
    int x1, int y1, int x2, int y2) 
{
    SCROLLINFO sbar;
    int xp, yp;
    int sx, sy;
    
    onstack("ed_main_draw");
    
    sbar.cbSize = sizeof(SCROLLINFO);
    sbar.fMask = SIF_POS;
    GetScrollInfo(ehwnd, SB_HORZ, &sbar);
    xp = sbar.nPos;
    GetScrollInfo(ehwnd, SB_VERT, &sbar);
    yp = sbar.nPos;
    
    if (backbuffer == NULL)
        VOIDRETURN();
    
    if (x2 == 0) {
        int w, h;
        
        w = backbuffer->w;
        h = backbuffer->h;
        x2 = w + DRAW_OFFSET;
        y2 = h + DRAW_OFFSET;
    }
    
    sx = x1 - DRAW_OFFSET;
    sy = y1 - DRAW_OFFSET;
    
    blit_to_hdc(backbuffer, hdc, sx, sy, x1 - xp, y1 - yp, x2 - x1, y2 - y1);
    
    VOIDRETURN();
}

void ESB_dungeon_windowpaint(HWND ehwnd, BITMAP *backbuffer) {
    PAINTSTRUCT y_paint;
    HDC hdc;
    int x1, y1, x2, y2;
    SCROLLINFO sbar;
    int xp, yp;
    
    onstack("ESB_dungeon_windowpaint");
    
    if (edg.runmode < EDITOR_RUNNING)
        VOIDRETURN();
        
    sbar.cbSize = sizeof(SCROLLINFO);
    sbar.fMask = SIF_POS;
    GetScrollInfo(ehwnd, SB_HORZ, &sbar);
    xp = sbar.nPos;
    GetScrollInfo(ehwnd, SB_VERT, &sbar);
    yp = sbar.nPos;
    
    hdc = BeginPaint(ehwnd, &y_paint);
    x1 = y_paint.rcPaint.left;
    x2 = y_paint.rcPaint.right;
    y1 = y_paint.rcPaint.top;
    y2 = y_paint.rcPaint.bottom;
    ed_main_draw(ehwnd, backbuffer, hdc, x1+xp, y1+yp, x2+xp, y2+yp);
    EndPaint(ehwnd, &y_paint);
    
    VOIDRETURN();
}

void force_redraw(HWND ehwnd, int db) {
    struct ext_hwnd_info *xinfo;
    HDC hdc;
    HWND phwnd;
    int level;
    int xdraw = 0;
    
    onstack("force_redraw");
    
    phwnd = GetParent(ehwnd);
    xinfo = get_hwnd(phwnd);
    if (!xinfo || ehwnd != xinfo->b_buf_win)
        VOIDRETURN();
        
    if (xinfo->wtype) {
        level = xinfo->plevel;
        xdraw = 0;
    } else {
        level = edg.ed_lev;
        if (edg.draw_mode == DRM_PARTYSTART)
            xdraw = XDF_PARTYPOS;
        else if (edg.draw_mode == DRM_WALLSET)
            xdraw = XDF_WSPAINT;
    }
    
    hdc = GetDC(ehwnd);
    if (db)
        ed_draw_backbuffer(xinfo->b_buf, level, xdraw);
    ed_main_draw(ehwnd, xinfo->b_buf, hdc, 0, 0, 0, 0);
    ReleaseDC(ehwnd, hdc);
    
    VOIDRETURN();
}

void ESBsysmsg(char *s_str, int bk) {
    if (!bk || edg.runmode >= EDITOR_RUNNING) {
        SetWindowText(edg.infobar, s_str);
    }
}

void load_editor_bitmaps(void) {
    edg.cells = ESBpcxload("editor/cells.pcx");
    edg.marked_cell = ESBpcxload("editor/marked_cell.pcx");
    edg.zones = ESBpcxload("editor/zones.pcx");
    
    edg.true_cells = edg.cells;
    edg.true_marked_cell = edg.marked_cell;
}

void ed_lua_retrieve_targlist(int *c, int **tl, int **ml,
    const char *fetcher, int lev, int x, int y) 
{
    int i, n;
    
    onstack("ed_lua_retrieve_targlist");
    
    luastacksize(8);
    lua_getglobal(LUA, fetcher);
    lua_pushinteger(LUA, lev);
    lua_pushinteger(LUA, x);
    lua_pushinteger(LUA, y);
    
    v_luaonstack(fetcher);
    if(lua_pcall(LUA, 3, 3, 0)) {
        char pxstr[200];
        sprintf(pxstr, "Internal Lua Error\n\n%s", lua_tostring(LUA, -1));
        poop_out(pxstr);
        VOIDRETURN();
    }
    v_upstack();
    
    if (lua_isnil(LUA, -1)) {
        lua_pop(LUA, 3); 
        *c = 0;
        *tl = NULL;
        *ml = NULL;
        VOIDRETURN();
    } 
    n = lua_tointeger(LUA, -3);
    if (n == 0) {
        program_puke("Bad fetchtable size");
        VOIDRETURN();
    }
    *tl = dsbcalloc(n, sizeof(int)); 
    *ml = dsbcalloc(n, sizeof(int));     
    for (i=0;i<n;++i) {
        int store;             
        lua_pushinteger(LUA, i+1);
        lua_gettable(LUA, -2);
        store = lua_tointeger(LUA, -1);
        (*tl)[i] = store;
        lua_pop(LUA, 1);
        
        lua_pushinteger(LUA, i+1);
        lua_gettable(LUA, -3);
        store = lua_tointeger(LUA, -1);
        (*ml)[i] = store;
        lua_pop(LUA, 1);
    }
    *c = n;
    lua_pop(LUA, 3);
    lstacktop();
    VOIDRETURN();
}

int ed_paint_targets(int n, int *tl, int *ml,
    int l, int sx, int sy) 
{
    int i;
    int pt = 0;
    int line_background = makecol(0, 0, 0);
    
    onstack("ed_paint_targets");
    
    if (edg.modeflags & EMF_TARGREVERSE) {
        line_background = makecol(255, 255, 255);
    }
     
    for (i=0;i<n;++i) {
        int cti = tl[i];
        if (oinst[cti]) {
            struct inst *p_t_inst = oinst[cti];
            int ctm = ml[i];
            int tilesize = edg.cells->w / 8;
            int d_sx, d_sy;
            int d_dx, d_dy;
            float lf = 1.0f;
            float o_lf = 2.0f;
            int incol = 0;
            
            if (edg.modeflags & EMF_DOUBLEZOOM) {
                lf *= 2.0f;
                o_lf *= 2.0f;
            }
           
            if (p_t_inst->level != l)
                continue;
            if (p_t_inst->x == sx && p_t_inst->y == sy)
                continue;
            
            d_sx = (sx * tilesize) + tilesize/2;
            d_sy = (sy * tilesize) + tilesize/2;
            d_dx = (p_t_inst->x * tilesize) + tilesize/2;
            d_dy = (p_t_inst->y * tilesize) + tilesize/2;
                  
            if (edg.sbmp) {      
                thick_line(edg.sbmp, d_sx, d_sy, d_dx, d_dy,
                    o_lf, line_background);
                thick_line(edg.sbmp, d_sx, d_sy, d_dx, d_dy,
                    lf, ctm);
            }
                
            pt++;
        }      
    }
        
    RETURN(pt);
}

int ed_paint_teleports(int n, int *tl, int *ml,
    int l, int sx, int sy) 
{
    int pt = 0;
    int i;
    
    onstack("ed_paint_teleports");
     
    for (i=0;i<n;++i) {
        int dx = ml[i];
        int dy = tl[i];
        int tilesize = edg.cells->w / 8;
        int d_sx, d_sy;
        int d_dx, d_dy;
        float lf = 1.0f;
        float o_lf = 2.0f;
        int incol = makecol(0, 144, 220);
  
        if (edg.modeflags & EMF_DOUBLEZOOM) {
            lf *= 2.0f;
            o_lf *= 2.0f;
        }
        
        if (dx < 0 || dy < 0)
            continue;
        if (dx >= dun[l].xsiz || dy >= dun[l].ysiz)
            continue;
        
        d_sx = (sx * tilesize) + tilesize/2;
        d_sy = (sy * tilesize) + tilesize/2;
        d_dx = (dx * tilesize) + tilesize/2;
        d_dy = (dy * tilesize) + tilesize/2;
        
        if (edg.sbmp) {
            thick_line(edg.sbmp, d_sx, d_sy, d_dx, d_dy,
                o_lf, makecol(0, 0, 0));
            thick_line(edg.sbmp, d_sx, d_sy, d_dx, d_dy,
                lf, incol);
        }
            
        pt++;     
    }
    
    RETURN(pt);
}

// the tile we're over has changed
void ed_main_window_mouseover(int tx, int ty) {
    char overtxt[90];
    int olev = edg.ed_lev;
    edg.o_tx = tx;
    edg.o_ty = ty;
    
    onstack("ed_main_window_mouseover");
    
    // over a tile
    if (tx >=0 && ty >= 0) {
        int dr = 0;
        int t_c;
        int *tln;
        int *mln;
        char dstr[300];
        sprintf(dstr, "%d, %d, %d", edg.ed_lev, tx, ty);
        SetWindowText(edg.coordview, dstr);
        
        if (edg.draw_mode == DRM_WALLSET) {
            int cpos;
            
            *dstr = '\0';
            luastacksize(4);
            lua_getglobal(LUA, "_ESBLEVELUSE_WALLSETS");
            for (cpos=4;cpos>=0;--cpos) {
                int compval = wscomp(edg.ed_lev, tx, ty, cpos);
                lua_pushinteger(LUA, compval);
                lua_gettable(LUA, -2);
                if (lua_isstring(LUA, -1)) {
                    char cstr[50];
                    const char *compstr = lua_tostring(LUA, -1);
                    snprintf(cstr, sizeof(cstr), "[%s]: %s\n",
                        calctpos(cpos, 1), compstr);
                    strcat(dstr, cstr);
                }
                lua_pop(LUA, 1);
            }
            lua_pop(LUA, 1);
            
            SetWindowText(edg.tile_list, dstr);    
        } else {
            ed_fillboxfromtile(edg.tile_list, ESBF_STATICFILL,
                edg.ed_lev, tx, ty, -1);
        }
            
        if (edg.dirty) {
            edg.dirty = 0;
            force_redraw(edg.subwin, 1);
        }
        
        if (edg.draw_mode != DRM_WALLSET) {
            if (edg.modeflags & EMF_TARGREVERSE) {
                ed_lua_retrieve_targlist(&t_c, &tln, &mln,
                    "__ed_getalltargs_reversed", olev, tx, ty);
            } else {
                ed_lua_retrieve_targlist(&t_c, &tln, &mln,
                    "__ed_getalltargs", olev, tx, ty);
            }
            
            if (t_c > 0) {
                dr += ed_paint_targets(t_c, tln, mln, olev, tx, ty);
                dsbfree(tln);
                dsbfree(mln);
            }       
            ed_lua_retrieve_targlist(&t_c, &tln, &mln,
                "__ed_getalldests", olev, tx, ty);
            if (t_c > 0) {
                dr += ed_paint_teleports(t_c, tln, mln, olev, tx, ty);
                dsbfree(tln);
                dsbfree(mln);
            }
        }
    
        if (dr) {
            edg.dirty = dr;
            force_redraw(edg.subwin, 0);
        }
                
    // just clearing the mouseovers and redrawing
    } else {
        SetWindowText(edg.coordview, "");
        SetWindowText(edg.tile_list, "");
        
        if (edg.dirty) {
            edg.dirty = 0;
            force_redraw(edg.subwin, 1);
        }
    }
    
    VOIDRETURN();
}

void ed_get_uzonetype(HWND ehwnd, int x, int y) {
    int ch;
    int rx, ry;
    int xp, yp;
    int tx, ty;
    int cxv, cyv;
    SCROLLINFO sbar;
    unsigned char ouzone;
    struct dungeon_level *dd;
    
    if (edg.ed_lev < 0)
        return;

    dd = &(dun[edg.ed_lev]);
    ch = edg.cells->w / 8;
    
    sbar.cbSize = sizeof(SCROLLINFO);
    sbar.fMask = SIF_POS;
    GetScrollInfo(ehwnd, SB_HORZ, &sbar);
    xp = sbar.nPos;
    GetScrollInfo(ehwnd, SB_VERT, &sbar);
    yp = sbar.nPos;
    
    rx = (x - DRAW_OFFSET + xp);
    ry = (y - DRAW_OFFSET + yp);
    if (rx >= 0 && ry >= 0) {
        tx = rx / ch;
        ty = ry / ch;
    } else {
        tx = -1;
        ty = -1;
    }
    
    if (tx >= 0 && ty >= 0) {
        if (tx < dd->xsiz && ty < dd->ysiz) {
            int oddir = edg.d_dir;
            ouzone = edg.uzone;
            edg.uzone = (dd->t[ty][tx].w & 1);
        
            if (tx != edg.o_tx || ty != edg.o_ty)
                ed_main_window_mouseover(tx, ty);
                
            if (edg.autosel) {
                int w = edg.cells->w / 8;
                int cent = (edg.autosel & 2);
                int ortho = !edg.uzone;
                int stx = rx - (tx * ch);
                int sty = ry - (ty * ch);
                
                if (edg.draw_mode != DRM_DRAWOBJ)
                    ortho = 0;
             
                if ((edg.autosel & 4) && !edg.uzone)
                    edg.d_dir = 4;
                else 
                    edg.d_dir = zone_do_autosel(w, cent, ortho, stx, sty);
            }
            
            if (ouzone != edg.uzone || oddir != edg.d_dir) {
                HDC hdc = GetDC(edg.loc);
                locw_draw(edg.loc, hdc, NULL);
                ReleaseDC(edg.loc, hdc);
            }
                
            return;
        }
    }
    
    if (edg.o_tx != -1)
        ed_main_window_mouseover(-1, -1);
}

void tninfo(HWND ehwnd, struct dungeon_level *dd, int ch, int x, int y,
    int *nx, int *ny, int *tx, int *ty)
{
    SCROLLINFO sbar;
    int xp, yp;
    int rx, ry;
    
    sbar.cbSize = sizeof(SCROLLINFO);
    sbar.fMask = SIF_POS;
    GetScrollInfo(ehwnd, SB_HORZ, &sbar);
    xp = sbar.nPos;
    GetScrollInfo(ehwnd, SB_VERT, &sbar);
    yp = sbar.nPos;
    
    rx = (x - DRAW_OFFSET + xp);
    ry = (y - DRAW_OFFSET + yp);
    
    *tx = rx / ch;
    *ty = ry / ch;

    *nx = ((x + (xp % ch))/ch)*ch - (xp%ch);
    *ny = ((y + (yp % ch))/ch)*ch - (yp%ch);
    
    if (rx < 0 || ry < 0)
        *tx = *ty = -1;
}

void ed_updatescr(HWND ehwnd, int lev, struct dungeon_level *dd,
    int ch, int tx, int ty, int nx, int ny)
{
    struct ext_hwnd_info *xinfo;
    HWND phwnd;
    BITMAP *tcomp;
    HDC hdc;
    
    onstack("ed_updatescr");

    phwnd = GetParent(ehwnd);
    xinfo = get_hwnd(phwnd);
    if (xinfo->b_buf_win != ehwnd)
        VOIDRETURN();
    
    tcomp = ed_create_comp(dd, tx, ty, ch);
    blit(tcomp, xinfo->b_buf, 0, 0, tx*ch, ty*ch, ch, ch);
    
    if (edg.draw_mode == DRM_WALLSET) {
        int d;
        luastacksize(4);
        lua_getglobal(LUA, "_ESBLEVELUSE_WALLSETS");
        ed_paint_wallset(xinfo->b_buf, lev, tx, ty, tx, ty);
        lua_pop(LUA, 1);
    }
    
    hdc = GetDC(ehwnd);
    blit_to_hdc(xinfo->b_buf, hdc, tx*ch, ty*ch, nx, ny, ch, ch);
    ReleaseDC(ehwnd, hdc);
    destroy_bitmap(tcomp);
    
    VOIDRETURN();
}

void ed_window_rightclick(HWND ehwnd, int x, int y, WPARAM wParam) {
    int ch;
    int tx, ty;
    int nx, ny;
    int need_update = 0;
    struct dungeon_level *dd;

    onstack("ed_window_rightclick");

    if (edg.ed_lev < 0)
        VOIDRETURN();
        
    if (edg.global_mark_level >= 0) {
        edg.global_mark_level = -1;
        edg.dirty = 1;
        need_update = 1;
    }

    dd = &(dun[edg.ed_lev]);
    ch = edg.cells->w / 8;
    tninfo(ehwnd, dd, ch, x, y, &nx, &ny, &tx, &ty);

    if (tx >= 0 && ty >= 0) {
        if (tx < dd->xsiz && ty < dd->ysiz) {
            if (wParam & MK_SHIFT) {
                int i;

                for (i=0;i<=4;++i) {
                    struct inst_loc *dt = dd->t[ty][tx].il[i];
                    while (dt != NULL) {
                        struct inst_loc *dtn = dt->n;
                        if (!dt->slave) {
                            // queue this one or the thing will crash!
                            recursive_inst_destroy(dt->i, 1);
                            free(dt);
                        }
                        dt = dtn;
                    }
                    dd->t[ty][tx].il[i] = NULL;
                }
                need_update = 1;
            } else if (edg.draw_mode == DRM_CUTPASTE) {
                POINT pt;
                
                HMENU cut_menu = CreatePopupMenu();
                AppendMenu(cut_menu, MF_STRING, ESBM_DO_CUT, "Cut");
                AppendMenu(cut_menu, MF_STRING, ESBM_DO_COPY, "Copy");
                AppendMenu(cut_menu, MF_STRING, ESBM_DO_PASTE, "Paste");
                SetForegroundWindow(ehwnd);
                
                pt.x = x;
                pt.y = y;
                MapWindowPoints(ehwnd, NULL, &pt, 1); 
                TrackPopupMenu(cut_menu, 0, pt.x, pt.y, 0, ehwnd, NULL);
                
                clipboard_mark(edg.ed_lev, tx, ty);
                         
            } else {
                int op_inst = 0;
                
                op_inst = ed_location_pick(edg.ed_lev, tx, ty);
                
                edg.lsx = tx;
                edg.lsy = ty;
                edg.op_inst = op_inst;
                
                if (op_inst == -1) {
                    need_update = DialogBox(GetModuleHandle(NULL),
                        MAKEINTRESOURCE(ESB_IDCHOOSE),
                        sys_hwnd, ESBidchooseproc);

                    op_inst = edg.op_inst;
                }
                
                if (op_inst > 0) {
                    DialogBox(GetModuleHandle(NULL),
                        MAKEINTRESOURCE(ESB_EXVEDIT),
                        sys_hwnd, ESBdproc);
                    need_update = 1;
                }
                
                edg.op_inst = 0;
            }
        }
    }
    
    if (need_update) {
        if (edg.dirty) {
            edg.dirty = 0;
            force_redraw(edg.subwin, 1);
        } else
            ed_updatescr(ehwnd, edg.ed_lev, dd, ch, tx, ty, nx, ny);
    }

    VOIDRETURN();
}

void ed_window_mouseclick(HWND ehwnd, int x, int y, int inh) {
    int ch;
    int tx, ty;
    int nx, ny;
    int need_update = 0;
    struct dungeon_level *dd;
    
    onstack("ed_window_mouseclick");
    
    if (edg.ed_lev < 0)
        VOIDRETURN();
        
    dd = &(dun[edg.ed_lev]);
    ch = edg.cells->w / 8;
    tninfo(ehwnd, dd, ch, x, y, &nx, &ny, &tx, &ty);
    
    if (tx >= 0 && ty >= 0) {
        if (tx < dd->xsiz && ty < dd->ysiz) {

            if (edg.draw_mode == DRM_DRAWDUN) {
                if (inh) {
                    int d = dd->t[ty][tx].w;

                    if (!d && edg.dmode) {
                        dd->t[ty][tx].w |= 1;
                        need_update = 1;
                    } else if (d && !edg.dmode) {
                        dd->t[ty][tx].w &= ~1;
                        need_update = 1;
                    }

                } else {
                    dd->t[ty][tx].w ^= 1;
                    edg.dmode = (dd->t[ty][tx].w & 1);
                    need_update = 1;
                }
            } else if (edg.draw_mode == DRM_DRAWOBJ) {
                if (!inh) {
                    int d_dir;
                    
                    if (edg.f_d_dir == -1)
                        d_dir = edg.d_dir;
                    else
                        d_dir = edg.f_d_dir;

                    edg.last_inst = ed_obj_add(edg.draw_arch, edg.ed_lev,
                        tx, ty, d_dir);
                    need_update = 1;
                    ed_fillboxfromtile(edg.tile_list, ESBF_STATICFILL,
                        edg.ed_lev, tx, ty, -1);  
                }
            } else if (edg.draw_mode == DRM_WALLSET) {
                int dodraw = 1;
                int idir = edg.d_dir;
                if (inh) {
                    idir = DIR_CENTER;
                    if (tx == edg.wsd_x && ty == edg.wsd_y)
                        dodraw = 0;
                }
                    
                if (dodraw) {
                    int packc = wscomp(edg.ed_lev, tx, ty, idir);
                    luastacksize(6);
                    lua_getglobal(LUA, "__ed_ws_drawpack");
                    lua_pushinteger(LUA, edg.ed_lev);
                    lua_pushinteger(LUA, packc);
                    lua_pushstring(LUA, edg.draw_ws);
                    if (lua_pcall(LUA, 3, 0, 0)) {
                        MessageBox(sys_hwnd,
                            "Wallset draw failure!\n\nThis is probably a bug in ESB.",
                            "Error", MB_ICONSTOP); 
                        VOIDRETURN();
                    }
                    need_update = 1;
                    edg.wsd_x = tx;
                    edg.wsd_y = ty;
                }
            } else if (edg.draw_mode == DRM_PARTYSTART) {
                gd.p_lev[0] = edg.ed_lev;
                gd.p_x[0] = tx;
                gd.p_y[0] = ty;
                gd.p_face[0] = edg.d_dir;
                force_redraw(edg.subwin, 1);
            }
        }
    }
    
    if (edg.global_mark_level >= 0) {
        edg.global_mark_level = -1;
        force_redraw(edg.subwin, 1);
        need_update = 0;
    }
        
    if (need_update)
        ed_updatescr(ehwnd, edg.ed_lev, dd, ch, tx, ty, nx, ny);
    
    VOIDRETURN();
}

void ed_scrollset(int picker, int olev, HWND sub, int i_w, int i_h) {
    struct dungeon_level *dd = NULL;
    SCROLLINFO sbar;
    int sbhorz_h, sbvert_w;
    
    onstack("ed_scrollset");
    
    if (olev >= 0)
        dd = &(dun[olev]);
      
    if (picker) {
        sbhorz_h = sbvert_w = 0;
    } else {
        sbhorz_h = GetSystemMetrics(SM_CXVSCROLL);
        sbvert_w = GetSystemMetrics(SM_CYHSCROLL);
    }
    
    memset(&sbar, 0, sizeof(SCROLLINFO));
    sbar.cbSize = sizeof(SCROLLINFO);
    sbar.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_RANGE;
    if (dd) {
        int i_reqsize;
        int ch = edg.cells->w / 8;

        i_reqsize = (2*DRAW_OFFSET + sbhorz_h) + dd->xsiz * ch;

        sbar.nMin = 0;
        sbar.nMax = i_reqsize;
        sbar.nPage = i_w;
    }
    SetScrollInfo(sub, SB_HORZ, &sbar, TRUE);
    
    if (dd) {
        int i_reqsize;
        int ch = edg.cells->w / 8;
        
        i_reqsize = (2*DRAW_OFFSET + sbvert_w) + dd->ysiz * ch;

        sbar.nMin = 0;
        sbar.nMax = i_reqsize;
        sbar.nPage = i_h;
    }
    SetScrollInfo(sub, SB_VERT, &sbar, TRUE);  
      
    VOIDRETURN();
}

void ed_init_level_bmp(int lev, HWND hwnd) {
    struct ext_hwnd_info *xinfo = get_hwnd(hwnd);
    int ch;

    onstack("ed_init_level_bmp");
    
    ch = edg.cells->w / 8;
    
    if (xinfo->b_buf)
        destroy_bitmap(xinfo->b_buf);
        
    xinfo->b_buf = create_bitmap(ch * dun[lev].xsiz, ch * dun[lev].ysiz);
    
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
    force_redraw(xinfo->b_buf_win, 1);
    
    VOIDRETURN();
}

void compilate_blit(BITMAP *czb, int hl, int zn, int fcol) {
    BITMAP *comp_b;
    int fx, fy;
            
    if (hl == 0) {
        fx = 24; fy = 10;
    } else if (hl == 1) {
        fx = 50; fy = 20;
    } else if (hl == 2) {
        fx = 40; fy = 50;
    } else if (hl == 3) {
        fx = 10; fy = 40;
    }  else
        fx = fy = 32;
        
    comp_b = create_bitmap(65, 65);
    blit(edg.zones, comp_b, 65*zn, 0, 0, 0, 65, 65);
    
    floodfill(comp_b, fx, fy, fcol);
    
    set_trans_blender(0, 0, 0, 176);
    draw_trans_sprite(czb, comp_b, 0, 0);
    
    destroy_bitmap(comp_b);    
}

int locw_draw(HWND ehwnd, HDC hdc, struct ext_hwnd_info *exi) {
    int zn = 0;
    unsigned char iz;
    BITMAP *czb;

    if (edg.uzone == INVALID_UZONE_VALUE)
        return;
    
    if (edg.zones == NULL)
        return;

    onstack("locw_draw");
        
    czb = create_bitmap(65, 65);
        
    if (exi)
        iz = exi->uzone;
    /* redundant 
    else if (edg.draw_mode == DRM_DRAWDUN)
        iz = edg.uzone;
    */
    else
        iz = edg.uzone;
    blit(edg.zones, czb, 65*iz, 0, 0, 0, 65, 65);
    
    if (exi || edg.draw_mode == DRM_DRAWOBJ) {
        int archnum;
        struct obj_arch *p_arch;
        int hl;
        int fcol;
        int uzone;
        
        if (exi) {
            struct inst *p_inst = oinst[exi->inst];
            archnum = p_inst->arch;
            uzone = exi->uzone;
            
            if (p_inst->level < 0) {
                zn = 7;
                if (exi) exi->zm = zn;
                blit_to_hdc(edg.zones, hdc, 65*zn, 0, 0, 0, 65, 65);
                RETURN(zn);
            }
        } else {
            archnum = lgetarch(edg.draw_arch);
            uzone = edg.uzone;
        }

        p_arch = Arch(archnum);
        
        if (p_arch->type == OBJTYPE_MONSTER) {
            if (p_arch->msize == 4) zn = 6;
            else zn = 4;
            fcol = makecol(255, 0, 96);
        } else if (p_arch->type == OBJTYPE_THING) {
            if (uzone == 1) zn = 2;
            else zn = 3;
            fcol = makecol(0, 192, 192);
        } else if (p_arch->type == OBJTYPE_WALLITEM) {
            zn = 2;
            fcol = makecol(0, 192, 0);
        } else if (p_arch->type == OBJTYPE_DOOR) {
            zn = 6;
            fcol = makecol(96, 192, 0);
        } else {
            if (uzone == 1) zn = 5;
            else zn = 4;
            fcol = makecol(192, 127, 0);
            if (!exi && edg.force_zone) {
                edg.d_dir = 4;
                if (edg.editor_flags & EEF_FLOORLOCK)
                    edg.autosel = 255;
            }
                
        }
        if (exi) exi->zm = zn;

        if (exi) {
            struct inst *p_inst = oinst[exi->inst];
            hl = p_inst->tile;
        } else {
            if (zn == 6) {
                edg.f_d_dir = hl = 4;
            } else if (edg.d_dir == 4 && zn < 4)
                edg.f_d_dir = hl = 0;
            else {
                edg.f_d_dir = -1;
                hl = edg.d_dir;
            }
        }
        
        compilate_blit(czb, hl, zn, fcol);
        
    } else if (edg.draw_mode == DRM_PARTYSTART) {
        zn = 8;
        compilate_blit(czb, edg.d_dir, zn, makecol(32, 255, 96)); 
        edg.autosel = 1;   
    } else if (edg.draw_mode == DRM_WALLSET) {
        zn = 5;
        compilate_blit(czb, edg.d_dir, zn, makecol(255, 48, 200)); 
        edg.autosel = 6; 
    }
        
    blit_to_hdc(czb, hdc, 0, 0, 0, 0, 65, 65);
    
    destroy_bitmap(czb);
    
    RETURN(zn);
}

void recursive_inst_destroy(int inst, int dpsquare) {
    struct inst_loc *il;
    struct inst *p_inst;
    
    onstack("recursive_inst_destroy");
    
    p_inst = oinst[inst];
    il = p_inst->chaintarg;
    while (il) {
        int dinst = il->i;
        struct inst *p_dinst = oinst[dinst];
        struct inst_loc *il_n = il->n;
        p_dinst->uplink = 0;
        if (dpsquare && p_inst->level >= 0 &&
            p_inst->level == p_dinst->level &&
            p_inst->x == p_dinst->x &&
            p_inst->y == p_dinst->y)
        {
            // leave this one alone, something else will get it
        } else
            recursive_inst_destroy(dinst, 0);
        dsbfree(il);
        il = il_n;
    }
    p_inst->chaintarg = NULL;
    inst_destroy(inst, !!(dpsquare)); 
    lc_parm_int("clean_up_target_exvars", 1, inst);  
    
    VOIDRETURN();    
}

int locw_update(HWND hw, struct ext_hwnd_info *exi) {
    int lw;
    HDC hdc;
    
    onstack("locw_update");
    
    hdc = GetDC(hw);
    lw = locw_draw(hw, hdc, exi);
    ReleaseDC(hw, hdc);
    
    RETURN(lw);
}
