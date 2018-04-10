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
extern struct inventory_info gii;
extern struct inst *oinst[NUM_INST];
extern struct dungeon_level *dun;
extern lua_State *LUA;

extern HWND sys_hwnd;
extern FILE *errorlog;

int MAIN_SKILLS[4];
int SUB_SKILLS[4][4];

int ed_inventory_move(struct champion *charid, int moving, int from, int to) 
{
    int swapped = charid->inv[to];
    
    onstack("ed_inventory_move");
    
    charid->inv[to] = charid->inv[from];
    charid->inv[from] = swapped;
    oinst[moving]->y = to;    
    if (swapped) {
        oinst[swapped]->y = from;
    }
    
    RETURN(swapped);
}

INT_PTR CALLBACK ESB_edit_chexvar_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo;
    char dt_fast = '\0';
    
    onstack("ESB_edit_chexvar_proc"); 
    
    switch(message) { 
        case WM_INITDIALOG: {
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);    
            ed_grab_ch_exvars_for(xinfo->inst + 1, hwnd, ESB_CHEXLIST);
            lc_parm_int("__init_ch_exvar_editing", 1, xinfo->inst + 1); 
        } break;
        
        case WM_COMMAND: {
            int csel = 0;
            char *exvn;
            int memb;
            int lua_memb = 0;
            int lw = LOWORD(wParam);
            struct champion *me = NULL;
            HWND hlist = GetDlgItem(hwnd, ESB_CHEXLIST);
                        
            xinfo = get_hwnd(hwnd);
            if (xinfo == NULL)
                break;
                
            memb = xinfo->inst;
            me = &(gd.champs[memb]);
            lua_memb = memb + 1;
            
            if (lw == IDCANCEL) {
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            }
            
            switch(lw) {
                case ESB_CHEXADD: {
                    edg.op_inst = lua_memb; // set for goto, NOT add_exvar_proc
                    edg.w_t_string = "Add ch_exvar";
                    edg.exvn_string = "What would you like to call the new ch_exvar?";
                    DialogBox(GetModuleHandle(NULL),
                        MAKEINTRESOURCE(ESB_EXVARADD),
                        hwnd, ESB_add_exvar_proc);
                    if (edg.exvn_string) {
                        exvn = (char*)edg.exvn_string;
                        csel = -1;
                        goto CHEXVAR_EDIT;
                    }

                } break;
                
                case ESB_CHEXDEL: {
                    exvn = ed_makeselexvar(hlist, &csel, NULL);
                    if (exvn != NULL) {
                        char *buffer = dsbfastalloc(250);
                        snprintf(buffer, 250, "ch_exvar[%d].%s = nil",
                            lua_memb, exvn);
                        // meh. if this fails we have worse problems.
                        luaL_dostring(LUA, buffer);
                        ed_grab_ch_exvars_for(lua_memb,
                            hwnd, ESB_CHEXLIST);
                        if (csel > 0) csel--;
                        SendDlgItemMessage(hwnd, ESB_CHEXLIST,
                            LB_SETCURSEL, csel, 0);
                        dsbfree(exvn);
                    }
                    
                } break;
                
                case ESB_CHEXEDIT: {
                    exvn = ed_makeselexvar(hlist, &csel, NULL);
                    
                    CHEXVAR_EDIT:
                    if (exvn != NULL) {
                        int r;
                        
                        edg.ch_ex_mode = 1;
                        r = ed_edit_exvar(hwnd, lua_memb,
                            "ch_exvar", exvn, dt_fast);
                        edg.ch_ex_mode = 0;
                        
                        if (r) {
                            ed_grab_ch_exvars_for(lua_memb,
                                hwnd, ESB_CHEXLIST);   
                            if (csel == -1) {
                                char dbuf[100];
                                snprintf(dbuf, 99, "%s =", exvn);
                                csel = SendDlgItemMessage(hwnd, ESB_CHEXLIST,
                                    LB_FINDSTRING, 0, (int)dbuf);
                            }   
                            SendDlgItemMessage(hwnd, ESB_CHEXLIST,
                                LB_SETCURSEL, csel, 0);
                        }
                        free(exvn);
                    }
                } break;
            }
        } break;
        
        case WM_CLOSE: {
            xinfo = get_hwnd(hwnd);
            lc_parm_int("__finish_ch_exvar_editing", 1, xinfo->inst + 1); 
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

void fill_table_mastery(int memb) {
    int m, s;
    
    onstack("fill_table_mastery");
    
    for(m=0;m<4;++m) {
        MAIN_SKILLS[m] = determine_mastery(1, memb, m, 0, 0);
        for(s=0;s<4;++s) {
            SUB_SKILLS[m][s] = determine_mastery(1, memb, m, s+1, 0);
        }
    }
        
    VOIDRETURN();    
}

void set_mastery_from_table(int memb) {
    int c_sk;
    struct champion *me;
    
    onstack("set_mastery_from_table");
    
    me = &(gd.champs[memb]);

    for (c_sk=0;c_sk<4;++c_sk) {
        int c_sub;
        
        int main_level = MAIN_SKILLS[c_sk];
        int xp = levelxp(main_level);
        
        me->xp[c_sk] = xp;
        for(c_sub=0;c_sub<4;++c_sub) {
            int sub_level = SUB_SKILLS[c_sk][c_sub];
            int sub_xp = levelxp(sub_level);
            me->sub_xp[c_sk][c_sub] = sub_xp / gd.max_subsk[c_sk];
        }
    }

    VOIDRETURN();
}

void unpack_to_window(HWND hwnd, int memb, int cl) {
    static const char *SUBSKILLNAME[] = {
        "Swing",
        "Thrust",
        "Bash",
        "Defend",
        "Climb",
        "Combat",
        "Throw",
        "Shoot",
        "Luck",
        "Potions",
        "War Cry",
        "Shields",
        "Fire",
        "Air",
        "Void",
        "Poison"
    };
        
    int sub;
    
    onstack("unpack_to_window");
    
    SetDlgItemInt(hwnd, ESB_EDIT_SKMAIN, MAIN_SKILLS[cl], 0);
    
    for(sub=0;sub<4;sub++) {
        SetDlgItemInt(hwnd, ESB_SK_EDIT1+sub, SUB_SKILLS[cl][sub], 0);
        SetDlgItemText(hwnd, ESB_CAP_S1+sub, SUBSKILLNAME[cl*4+sub]);
    }
    
    VOIDRETURN();   
}

int pack_from_window(HWND hwnd, int memb, int cl, int silent) {
    int sub;
    int current_level;

    onstack("pack_from_window");
    
    current_level = GetDlgItemInt(hwnd, ESB_EDIT_SKMAIN, NULL, FALSE);
    if (current_level < 0 || current_level > gd.max_lvl) {
        if (!silent) {
            char errmsg[80];
            sprintf(errmsg, "Level must be between 0 and %ld", gd.max_lvl);
            MessageBox(hwnd, errmsg, "Error", MB_ICONEXCLAMATION);
        }
        RETURN(1);
    }
    MAIN_SKILLS[cl] = current_level;
    
    for(sub=0;sub<4;sub++) {
        current_level = GetDlgItemInt(hwnd, ESB_SK_EDIT1+sub, NULL, FALSE);
        if (current_level < 0 || current_level > gd.max_lvl) {
            if (!silent) {
                char errmsg[80];
                sprintf(errmsg, "Subskill Level must be between 0 and %ld", gd.max_lvl);
                MessageBox(hwnd, errmsg, "Error", MB_ICONEXCLAMATION);
            }
            RETURN(1);   
            break;
        }
        
        SUB_SKILLS[cl][sub] = current_level;
    }

    RETURN(0);
}

INT_PTR CALLBACK ESB_edit_champion_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo;
    
    onstack("ESB_edit_champion_proc");
    switch(message) { 
        case WM_INITDIALOG: {
            int n;
            int memb;
            struct champion *me;
            
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            memb = xinfo->inst;
            me = &(gd.champs[memb]);
            gd.max_lvl = luacharglobal("xp_levels");
            
            SetDlgItemInt(hwnd, ESB_EDITHEALTH, me->maxbar[0], 0);
            SetDlgItemInt(hwnd, ESB_EDITSTAM, me->maxbar[1], 0);
            SetDlgItemInt(hwnd, ESB_EDITMANA, me->maxbar[2], 0);
                       
            SetDlgItemInt(hwnd, ESB_EDITSTR, me->maxstat[0], 0);
            SetDlgItemInt(hwnd, ESB_EDITDEX, me->maxstat[1], 0);
            SetDlgItemInt(hwnd, ESB_EDITWIS, me->maxstat[2], 0);
            SetDlgItemInt(hwnd, ESB_EDITVIT, me->maxstat[3], 0);
            SetDlgItemInt(hwnd, ESB_EDITAMA, me->maxstat[4], 0);
            SetDlgItemInt(hwnd, ESB_EDITAFI, me->maxstat[5], 0);
            SetDlgItemInt(hwnd, ESB_EDITLUCK, me->maxstat[6], 0);
            
            fill_table_mastery(memb);
            unpack_to_window(hwnd, memb, 0);
            SendDlgItemMessage(hwnd, ESB_RB_FIGHTER, BM_SETCHECK, BST_CHECKED, 0);
                        
            SetDlgItemText(hwnd, ESB_INTERNALNAME, edg.c_ext[memb].desg);
            SetDlgItemText(hwnd, ESB_FIRSTNAME, me->name);
            SetDlgItemText(hwnd, ESB_LASTNAME, me->lastname);
            if (me->port_name)
                SetDlgItemText(hwnd, ESB_PORTRAITNAME, me->port_name);
              
            if (me->method_name)  
                SetDlgItemText(hwnd, ESB_UNARMEDMETHOD, me->method_name);
                
            ed_fillboxfromtile(hwnd, ESB_CHARINVLIST, LOC_CHARACTER, memb, 0, -1); 
            
            luastacksize(8);
            lua_getglobal(LUA, "inventory_info");
            for (n=0;n<gii.max_invslots;++n) {
                const char *slotname = NULL;
                char invstring[120];
                lua_pushinteger(LUA, n);
                lua_gettable(LUA, -2);
                if (lua_istable(LUA, -1)) {
                    lua_pushstring(LUA, "name");
                    lua_gettable(LUA, -2);
                    if (lua_isstring(LUA, -1)) {
                        slotname = lua_tostring(LUA, -1);
                    } else
                        lua_pop(LUA, 2);
                } else
                    lua_pop(LUA, 1);
                if (slotname) {
                    snprintf(invstring, 120, "%ld - %s", n, slotname);
                    lua_pop(LUA, 2);
                } else
                    snprintf(invstring, 120, "%ld [Unknown]", n);
                SendDlgItemMessage(hwnd, ESB_INVMOVETOBOX, CB_ADDSTRING, 0, (int)invstring);
            }
            lua_pop(LUA, 1);
            SendDlgItemMessage(hwnd, ESB_INVMOVETOBOX, CB_SETCURSEL, 0, 0);
            
        } break;
        
        case WM_COMMAND: {
            int memb;
            struct champion *me = NULL;
            int lw = LOWORD(wParam);
            int hw = HIWORD(wParam);
            
            xinfo = get_hwnd(hwnd);
            if (xinfo == NULL)
                break;
                
            memb = xinfo->inst;
            me = &(gd.champs[memb]);
            
            if (lw == IDCANCEL) {
                SendMessage(hwnd, WM_CLOSE, 0, 0);
                break;
            } else if (lw == IDOK) {
                char firstname[8];
                char portrait[60];
                char method[60];
                int badxp = 0;
                int badval = 0;
                int c_st;
                int c_sk;
                int rbc;
                int current = 0;
                
                GetDlgItemText(hwnd, ESB_FIRSTNAME, firstname, 8);
                if (firstname[0] == '\0') {
                    MessageBox(hwnd, "Character must have a name", "Error", MB_ICONEXCLAMATION);
                    break;
                }
                
                GetDlgItemText(hwnd, ESB_PORTRAITNAME, portrait, 60); 
                if (portrait[0] == '\0') {
                    MessageBox(hwnd, "Character must have a portrait", "Error", MB_ICONEXCLAMATION);
                    break;
                }
                
                GetDlgItemText(hwnd, ESB_UNARMEDMETHOD, method, 60); 
                if (method[0] == '\0') {
                    MessageBox(hwnd, "Character must have unarmed attack methods", "Error", MB_ICONEXCLAMATION);
                    break;
                }
                
                for (c_st=0;c_st<3;++c_st) {
                    int val = GetDlgItemInt(hwnd, ESB_EDITHEALTH + c_st, NULL, FALSE);
                    int min = 1;
                    if (c_st >= 2)
                        min = 0;
                    if (val < min || val > 9990) {
                        const char *errtitle = "Invalid value";
                        char errmsg[80];
                        if (val > 9000)
                            errtitle = "OVER NINE THOUSAND!!!";
                        sprintf(errmsg, "Value must be between %ld and %ld", min, 9990);
                        MessageBox(hwnd, errmsg, errtitle, MB_ICONEXCLAMATION);
                        badval = 1;
                        break;
                    }   
                }                
                if (badval)
                    break; 
                    
                for (c_st=0;c_st<=6;++c_st) {
                    int val = GetDlgItemInt(hwnd, ESB_EDITSTR + c_st, NULL, FALSE);
                    if (val < 1 || val > 2559) {
                        MessageBox(hwnd, "Value must be between 1 and 2559", "Invalid value", MB_ICONEXCLAMATION);
                        badval = 1;
                        break;
                    }   
                }                
                if (badval)
                    break; 
                
                for(rbc=0;rbc<4;rbc++) {
                    if (SendDlgItemMessage(hwnd, ESB_RB_FIGHTER+rbc, BM_GETCHECK, 0, 0)) {
                        current = rbc;
                        break;
                    }
                }   
                badxp = pack_from_window(hwnd, memb, current, 0);       
                if (badxp)
                    break;
                    
                set_mastery_from_table(memb);
                    
                GetDlgItemText(hwnd, ESB_FIRSTNAME, me->name, 8);
                GetDlgItemText(hwnd, ESB_LASTNAME, me->lastname, 20);
                
                dsbfree(me->port_name);
                me->port_name = dsbstrdup(portrait);
                
                dsbfree(me->method_name);
                me->method_name = dsbstrdup(method);
                
                for (c_st=0;c_st<3;++c_st) {
                    int val = GetDlgItemInt(hwnd, ESB_EDITHEALTH + c_st, NULL, FALSE);
                    me->maxbar[c_st] = val;  
                }                
                    
                for (c_st=0;c_st<=6;++c_st) {
                    int val = GetDlgItemInt(hwnd, ESB_EDITSTR + c_st, NULL, FALSE);
                    me->maxstat[c_st] = val;
                }     
                
                SendMessage(hwnd, WM_CLOSE, 0, 0);
                break;
            }
            
            switch (lw) {   
                case ESB_RB_FIGHTER:
                case ESB_RB_NINJA:
                case ESB_RB_PRIEST:
                case ESB_RB_WIZARD: {
                    int targ = lw - ESB_RB_FIGHTER;
                    int rbc;
                    int current = 0;
                    
                    for(rbc=0;rbc<4;rbc++) {
                        if (SendDlgItemMessage(hwnd, ESB_RB_FIGHTER+rbc, BM_GETCHECK, 0, 0)) {
                            current = rbc;
                            break;
                        }
                    }
                    if (current != targ) {
                        int packbad = pack_from_window(hwnd, memb, current, 1);
                        if (packbad) {
                            ; // ugh
                        } else {
                            SendDlgItemMessage(hwnd, ESB_RB_FIGHTER+current, BM_SETCHECK, BST_UNCHECKED, 0);
                            SendDlgItemMessage(hwnd, ESB_RB_FIGHTER+targ, BM_SETCHECK, BST_CHECKED, 0);
                            unpack_to_window(hwnd, memb, targ);
                        }
                    }                    
                } break;
                
                case (ESB_CHEXVAREDIT): {
                    edg.op_inst = memb;
                    ESB_modaldb(ESB_CHEXVEDIT, hwnd, ESB_edit_chexvar_proc, 0);
                } break;
                             
                case (ESB_CHARINVLIST): {
                    if (hw == LBN_DBLCLK) {
                        SendMessage(hwnd, WM_COMMAND, ESB_INVEDIT, 0);
                        break;
                    }
                } break;
                
                case (ESB_INVADDPACK): {
                    int pack_full = 0;
                    int freespot = INV_PACK;
                    while (me->inv[freespot]) {
                        freespot++;
                        if (freespot == gii.max_invslots) {
                            MessageBox(hwnd, "Pack is full.", "Error", MB_ICONEXCLAMATION);
                            pack_full = 1;
                            break;
                        }
                    }
                    if (pack_full)
                        break;
                    edg.op_arch = 4;
                    ESB_modaldb(ESB_ARCHPICKER, hwnd, ESB_archpick_proc, 0); 
                    if (edg.op_arch != 0xFFFFFFFF) {                             
                        struct obj_arch *p_arch = Arch(edg.op_arch);
                        int nobj = ed_obj_add(p_arch->luaname, LOC_CHARACTER, memb+1, freespot, 0);
                        ed_fillboxfromtile(hwnd, ESB_CHARINVLIST, LOC_CHARACTER, memb, 0, -1);
                        ed_reselect_inst(hwnd, ESB_CHARINVLIST, nobj);
                    }             
                } break;
                
                case (ESB_INVADDHAND): {
                    int freespot = 0;
                    if (me->inv[0])
                        freespot = 1;
                    if (me->inv[0] && me->inv[1]) {
                        MessageBox(hwnd, "Both hands are full.", "Error", MB_ICONEXCLAMATION);
                        break;
                    }  
                    edg.op_arch = 4;
                    ESB_modaldb(ESB_ARCHPICKER, hwnd, ESB_archpick_proc, 0); 
                    if (edg.op_arch != 0xFFFFFFFF) {                             
                        struct obj_arch *p_arch = Arch(edg.op_arch);
                        int nobj = ed_obj_add(p_arch->luaname, LOC_CHARACTER, memb+1, freespot, 0);
                        ed_fillboxfromtile(hwnd, ESB_CHARINVLIST, LOC_CHARACTER, memb, 0, -1);
                        ed_reselect_inst(hwnd, ESB_CHARINVLIST, nobj);
                    }             
                } break;
                                
                case (ESB_INVEDIT): {
                    int sidx, cinst;
                    cinst = ed_optinst(hwnd, ESB_CHARINVLIST, &sidx);
                    if (cinst) {
                        edg.op_inst = cinst;
                        DialogBox(GetModuleHandle(NULL),
                            MAKEINTRESOURCE(ESB_EXVEDIT),
                            hwnd, ESBdproc);
                        ed_fillboxfromtile(hwnd, ESB_CHARINVLIST, LOC_CHARACTER, memb, 0, sidx);
                    }
                } break;
                
                case (ESB_INVDELETE): {
                    int sidx;
                    int cinst = ed_optinst(hwnd, ESB_CHARINVLIST, &sidx);
                    if (cinst) {
                        recursive_inst_destroy(cinst, 0);
                        if (sidx > 0) --sidx;
                        ed_fillboxfromtile(hwnd, ESB_CHARINVLIST, LOC_CHARACTER, memb, 0, sidx);
                    }
                } break;
                
                case (ESB_INVMOVEUP): {
                    int sidx = LB_ERR;
                    int cinst = ed_optinst(hwnd, ESB_CHARINVLIST, &sidx);
                    if (cinst) {
                        int slot = oinst[cinst]->y;
                        if (slot > 0) {
                            int swapped = ed_inventory_move(me, cinst, slot, slot-1);
                            if (swapped) sidx--;
                            ed_fillboxfromtile(hwnd, ESB_CHARINVLIST, LOC_CHARACTER, memb, 0, sidx); 
                        }
                    }                                           
                } break;
                
                case (ESB_INVMOVEDOWN): {
                    int sidx = LB_ERR;
                    int cinst = ed_optinst(hwnd, ESB_CHARINVLIST, &sidx);
                    if (cinst) {
                        int slot = oinst[cinst]->y;
                        if (slot < gii.max_invslots - 1) {
                            int swapped = ed_inventory_move(me, cinst, slot, slot+1);
                            if (swapped) sidx++;
                            ed_fillboxfromtile(hwnd, ESB_CHARINVLIST, LOC_CHARACTER, memb, 0, sidx);  
                        }
                    }                   
                } break;
                
                case (ESB_INVMOVETO): {
                    int sidx = LB_ERR;
                    int cinst = ed_optinst(hwnd, ESB_CHARINVLIST, &sidx);
                    if (cinst) {
                        int slot = oinst[cinst]->y;
                        int newslot = SendDlgItemMessage(hwnd, ESB_INVMOVETOBOX, CB_GETCURSEL, 0, 0);
                        if (newslot != CB_ERR && (slot != newslot)) {
                            int swapped = ed_inventory_move(me, cinst, slot, newslot);
                            ed_fillboxfromtile(hwnd, ESB_CHARINVLIST, LOC_CHARACTER, memb, 0, -1); 
                            ed_reselect_inst(hwnd, ESB_CHARINVLIST, cinst);
                        }
                    }                                           
                } break;
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

INT_PTR CALLBACK ESBpartyproc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo;
    
    onstack("ESBpartyproc");

    switch(message) { 
        case EDM_PARTY_REFRESH: {
            int cs_t = SendDlgItemMessage(hwnd, ESB_TOTALCHARS, LB_GETCURSEL, 0, 0); 
            int cs_c = SendDlgItemMessage(hwnd, ESB_CURCHARS, LB_GETCURSEL, 0, 0); 
            int p;
            SendDlgItemMessage(hwnd, ESB_TOTALCHARS, LB_RESETCONTENT, 0, 0);
            SendDlgItemMessage(hwnd, ESB_CURCHARS, LB_RESETCONTENT, 0, 0);
            for (p=0;p<gd.num_champs;++p) {
                char sdesg[40];
                const char *desg = edg.c_ext[p].desg; 
                if (desg != NULL) {
                    sprintf(sdesg, "%s", desg);
                } else
                    sprintf(sdesg, "[%d] (unused)", p+1);
                    
                SendDlgItemMessage(hwnd, ESB_TOTALCHARS, LB_ADDSTRING, 0, (int)sdesg);
            }   
            for (p=0;p<4;++p) {
                char desg[50];
                if (gd.party[p]) {
                    int id = gd.party[p] - 1;
                    sprintf(desg, "(%d) %s", p, edg.c_ext[id].desg);    
                } else
                    sprintf(desg, "(%d) --Empty--", p);   
                
                SendDlgItemMessage(hwnd, ESB_CURCHARS, LB_ADDSTRING, 0, (int)desg);    
            }    
            
            SendDlgItemMessage(hwnd, ESB_CURCHARS, LB_SETCURSEL, cs_c, 0); 
            SendDlgItemMessage(hwnd, ESB_TOTALCHARS, LB_SETCURSEL, cs_t, 0); 
        } break;
        
        case WM_COMMAND: {
            int lw = LOWORD(wParam);
            int hw = HIWORD(wParam);
            if (lw == IDCANCEL) {
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            } else if (lw == IDOK) {
                SendMessage(hwnd, WM_CLOSE, 0, 0);   
            }
            switch (lw) {
                case ESB_TOTALCHARS: {
                    if (hw == LBN_DBLCLK) {
                        SendMessage(hwnd, WM_COMMAND, ESB_EDITCHAMP, 0);
                    }
                } break;
                
                case ESB_ADDMEMB: {
                    int selm = SendDlgItemMessage(hwnd, ESB_TOTALCHARS, LB_GETCURSEL, 0, 0);
                    int selslot = SendDlgItemMessage(hwnd, ESB_CURCHARS, LB_GETCURSEL, 0, 0);
                    if (selm != LB_ERR) {
                        int freeslot = -1;
                        int failed = 0;
                        int p;
                        
                        if (!edg.c_ext[selm].desg)
                            break;
                            
                        for(p=0;p<4;++p) {
                            if (gd.party[p] == selm + 1) {
                                MessageBox(hwnd, "Member is already in party.", "Error", MB_ICONEXCLAMATION);
                                failed = 1;
                                break;
                            }
                        }
                        if (failed)
                            break;
                              
                        for(p=0;p<4;++p) {
                            if (gd.party[p] == 0) {
                                freeslot = p;
                                break;
                            }
                        }
                        if (freeslot == -1) {
                            MessageBox(hwnd, "Party is full.", "Error", MB_ICONEXCLAMATION);
                            break;
                        }
                        
                        if (selslot != LB_ERR) {
                            freeslot = selslot;
                        }           
                        gd.party[freeslot] = selm + 1;
                        SendMessage(hwnd, EDM_PARTY_REFRESH, 0, 0);

                    }
                } break;
                
                case ESB_REMCHAR: {
                    int selslot = SendDlgItemMessage(hwnd, ESB_CURCHARS, LB_GETCURSEL, 0, 0);
                    if (selslot != LB_ERR) {
                        if (gd.party[selslot] == 0) {
                            MessageBox(hwnd, "Empty slot selected.", "Error", MB_ICONEXCLAMATION);  
                            break;
                        }
                        gd.party[selslot] = 0;
                        SendMessage(hwnd, EDM_PARTY_REFRESH, 0, 0);
                    } else
                        MessageBox(hwnd, "Nothing selected.", "Error", MB_ICONEXCLAMATION);    
                } break;
                
                case ESB_EDITCHAMP: {
                    int selslot = SendDlgItemMessage(hwnd, ESB_TOTALCHARS, LB_GETCURSEL, 0, 0);
                    if (selslot != LB_ERR) {
                        int validslot = 1;
                        
                        if (edg.c_ext[selslot].desg == NULL)
                            validslot = 0;
                        
                        if (validslot) {
                            edg.op_inst = selslot;
                            ESB_modaldb(ESB_CHAREDIT, hwnd, ESB_edit_champion_proc, 0);
                        }
                    }
                } break;
                
                case ESB_NEWCHAMP: {
                    int i;
                    const char *r_string = NULL;
                    int freeslot = -1;
                    int selslot = SendDlgItemMessage(hwnd, ESB_TOTALCHARS, LB_GETCURSEL, 0, 0);
                
                    edg.w_t_string = "Add new champion";
                    edg.exvn_string = "Enter in a unique id for this champion";
                    DialogBox(GetModuleHandle(NULL),
                        MAKEINTRESOURCE(ESB_EXVARADD),
                        hwnd, ESB_add_exvar_proc);
                    if (edg.exvn_string) {
                        r_string = edg.exvn_string;
                    }
                    if (r_string == NULL)
                        break;
                                                
                    for (i=0;i<gd.num_champs;++i) {
                        if (edg.c_ext[i].desg &&
                            !strcmp(r_string, edg.c_ext[i].desg))
                        {
                            char errstr[60];
                            snprintf(errstr, sizeof(errstr),
                                "Champion id %s is already in use", r_string);
                            dsbfree(r_string);
                            r_string = NULL; 
                            MessageBox(hwnd, errstr, "Error", MB_ICONEXCLAMATION);  
                            break;
                        }
                    } 
                    if (r_string == NULL)
                        break;
                        
                    // avoid a namespace collision
                    luastacksize(2);
                    lua_getglobal(LUA, r_string);
                    if (!lua_isnil(LUA, -1)) {
                        lua_pop(LUA, 1);
                        MessageBox(hwnd, "Invalid id: Namespace Collision", "Error", MB_ICONEXCLAMATION); 
                        break;
                    }
                    lua_pop(LUA, 1); 
                        
                    if (selslot != LB_ERR) {
                        if (edg.c_ext[selslot].desg == NULL)
                            freeslot = selslot;
                    }
                    if (freeslot == -1) {
                        for (i=0;i<gd.num_champs;++i) {
                            if (edg.c_ext[i].desg == NULL) {
                                freeslot = i;
                                break;
                            }
                        }
                    }
                    if (freeslot == -1) {           
                        freeslot = gd.num_champs;                     
                    }
                    
                    luastacksize(5);
                    lua_getglobal(LUA, "__ed_champ_create");
                    lua_pushinteger(LUA, freeslot + 1);
                    lua_pushstring(LUA, r_string);
                    lc_call_topstack(2, "ed_champ_create");
                    dsbfree(r_string);
                    
                    SendMessage(hwnd, EDM_PARTY_REFRESH, 0, 0);
                } break;
                
                case ESB_DELCHAMP: {
                    int selslot = SendDlgItemMessage(hwnd, ESB_TOTALCHARS, LB_GETCURSEL, 0, 0);
                    if (selslot != LB_ERR) {
                        int i;
                        struct champion *me = &(gd.champs[selslot]);
                        if (edg.c_ext[selslot].desg == NULL)
                            break;
                            
                        for (i=0;i<gii.max_invslots;++i) {
                            int inst_id = me->inv[i];
                            if (inst_id)
                                limbo_instance(inst_id);
                        }
                        destroy_champion(selslot);
                        
                        for (i=0;i<4;++i) {
                            if (gd.party[i] == selslot + 1)
                                gd.party[i] = 0;
                        }   
                        
                        luastacksize(5);
                        lua_pushnil(LUA);
                        lua_setglobal(LUA, edg.c_ext[selslot].desg);
                        lua_getglobal(LUA, "reverse_champion_table");
                        lua_pushinteger(LUA, selslot + 1);
                        lua_pushnil(LUA);
                        lua_settable(LUA, -3);
                        lua_pop(LUA, 1);
                        
                        dsbfree(edg.c_ext[selslot].desg);
                        memset(&(gd.champs[selslot]), 0, sizeof(struct champion));
                        memset(&(edg.c_ext[selslot]), 0, sizeof(struct eextchar));
                        
                        if (selslot + 1 == gd.num_champs) {
                            gd.num_champs--;
                            while (gd.num_champs > 0 && edg.c_ext[gd.num_champs-1].desg == NULL)
                                gd.num_champs--;
                            if (gd.num_champs == 0) {
                                dsbfree(gd.champs);
                                gd.champs = NULL;
                                dsbfree(edg.c_ext);
                                edg.c_ext = NULL;
                            } else {
                                gd.champs = dsbrealloc(gd.champs, gd.num_champs * sizeof(struct champion));
                                edg.c_ext = dsbrealloc(edg.c_ext, gd.num_champs * sizeof(struct eextchar));
                            }  
                        }
                        
                        SendMessage(hwnd, EDM_PARTY_REFRESH, 0, 0);
                    }
                } break; 
            }
        } break; 
                      
        case WM_INITDIALOG: {
            xinfo = add_hwnd(hwnd, 0, 0);
            SendMessage(hwnd, EDM_PARTY_REFRESH, 0, 0);
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

INT_PTR CALLBACK ESB_mirror_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo;
    
    onstack("ESB_mirror_proc");

    switch(message) { 
        case WM_COMMAND: {
            int lw = LOWORD(wParam);
            if (lw == IDCANCEL)
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            else if (lw == IDOK) {
                int selfield;
                int selchamp;
                int offer_mode = 3;
                
                xinfo = get_hwnd(hwnd);
                
                selfield = SendDlgItemMessage(hwnd, ESB_MCONTAINS, CB_GETCURSEL, 0, 0);
                selchamp = SendDlgItemMessage(hwnd, ESB_MCONTAINS,
                    CB_GETITEMDATA, selfield, 0);
   
                if (SendDlgItemMessage(hwnd, ESB_RESONLY, BM_GETCHECK, 0, 0))
                    offer_mode = 1;
                else if (SendDlgItemMessage(hwnd, ESB_REINONLY, BM_GETCHECK, 0, 0))
                    offer_mode = 2;
                
                lc_parm_int("__ed_mirrorchampion_store", 3,
                    xinfo->inst, selchamp, offer_mode);
                    
                SendMessage(hwnd, WM_CLOSE, 0, 0);      
            }
        } break; 
              
        case WM_INITDIALOG: {
            int have_con, contains;
            int have_resrei, resrei_mode;
            int cc;
            int line = 1;
            
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            
            have_con = ex_exvar(xinfo->inst, "champion", &contains);
            if (have_con) {
                if (contains < 1 || contains > gd.num_champs)
                    have_con = 0;
                else {
                    if (edg.c_ext[contains-1].desg == NULL)
                        have_con = 0;
                }
            }
            SendDlgItemMessage(hwnd, ESB_MCONTAINS, CB_ADDSTRING,
                0, (int)"--(Nobody)--");
            SendDlgItemMessage(hwnd, ESB_MCONTAINS, CB_SETITEMDATA, 0, 0);
            for (cc=0;cc<gd.num_champs;cc++) {
                if (edg.c_ext[cc].desg) {
                    SendDlgItemMessage(hwnd, ESB_MCONTAINS, CB_ADDSTRING,
                        0, (int)edg.c_ext[cc].desg);
                    SendDlgItemMessage(hwnd, ESB_MCONTAINS, CB_SETITEMDATA,
                        line, cc+1);
                    if (have_con && (contains-1) == cc) {
                        SendDlgItemMessage(hwnd, ESB_MCONTAINS, CB_SETCURSEL, line, 0);
                    } 
                    line++;
                }
            }
            if (!have_con)
                SendDlgItemMessage(hwnd, ESB_MCONTAINS, CB_SETCURSEL, 0, 0);
                
            have_resrei = ex_exvar(xinfo->inst, "offer_mode", &resrei_mode);
            
            if (have_resrei && resrei_mode == 1) {
                SendDlgItemMessage(hwnd, ESB_RESONLY, BM_SETCHECK, BST_CHECKED, 0);
            } else if (have_resrei && resrei_mode == 2) {
                SendDlgItemMessage(hwnd, ESB_REINONLY, BM_SETCHECK, BST_CHECKED, 0);    
            } else {
                SendDlgItemMessage(hwnd, ESB_RESREI, BM_SETCHECK, BST_CHECKED, 0);
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
   
