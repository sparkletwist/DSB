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

#define CONDPUSHSTRING(L, S) \
    if (S && *S != '\0') lua_pushstring(L, S); else lua_pushnil(L)

extern struct global_data gd;
extern struct editor_global edg;
extern struct inst *oinst[NUM_INST];
extern struct dungeon_level *dun;
extern lua_State *LUA;

extern HWND sys_hwnd;
extern FILE *errorlog;

INT_PTR CALLBACK ESB_opby_ed_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    struct ext_hwnd_info *xinfo;
    
    onstack("ESB_opby_ed_proc");

    switch(message) {
        case WM_INITDIALOG: {
            struct inst *p_inst;
            struct obj_arch *p_arch;
            int opby_party, opby_thing, opby_monster;
            int opby_empty_only = 0;
            int opby_party_face, opby_party_face_found;
            int wrong_dir_untrigger = 0;
            int const_weight = 0;
            int disable_self = 0;
            int no_tc = 0;
            int off_trigger = 0;
            int destroy = 0;
            int spec_opby = 0;
            char *opby;
            int s;
            HWND pbox = GetDlgItem(hwnd, ESB_PARTYFACING);
            int wallitem  = 0;
            int teleporter = 0;
            int airstate = 0;
            
            xinfo = add_hwnd(hwnd, edg.op_inst, 0);
            p_inst = oinst[xinfo->inst];
            p_arch = Arch(p_inst->arch);
            if (p_arch->type == OBJTYPE_WALLITEM)
                wallitem = 1;
            if (queryluabool(p_arch, "esb_use_coordinates"))
                teleporter = 1;
            
            if (wallitem) {
                HWND mon = GetDlgItem(hwnd, ESB_OPBY_MONSTER);
                HWND pf = GetDlgItem(hwnd, ESB_PARTYFACING);
                HWND opparty = GetDlgItem(hwnd, ESB_OPBY_PARTY);
                HWND anyclick = GetDlgItem(hwnd, ESB_ANYCLICK);
                HWND emptyonly = GetDlgItem(hwnd, ESB_EMPTYONLY);
                HWND opspec = GetDlgItem(hwnd, ESB_OPBYSPECIFIC);
                HWND wrongdir = GetDlgItem(hwnd, ESB_WRONGDIRUNTRIG);
                
                ShowWindow(wrongdir, FALSE);
                EnableWindow(mon, FALSE);
                ShowWindow(mon, FALSE);
                EnableWindow(pf, FALSE);
                ShowWindow(pf, FALSE);
                EnableWindow(opparty, FALSE);
                ShowWindow(opparty, FALSE);
                
                EnableWindow(anyclick, TRUE);
                EnableWindow(emptyonly, TRUE);
                EnableWindow(opspec, TRUE);
                
                opby_empty_only = ex_exvar(xinfo->inst, "opby_empty_hand_only", &s);
                opby_party = 0;
                opby_monster = 0;
            } else {
                HWND anyclick = GetDlgItem(hwnd, ESB_ANYCLICK);
                HWND emptyonly = GetDlgItem(hwnd, ESB_EMPTYONLY);
                HWND opspec = GetDlgItem(hwnd, ESB_OPBYSPECIFIC);
                
                ShowWindow(anyclick, FALSE);
                ShowWindow(emptyonly, FALSE);
                ShowWindow(opspec, FALSE);
                
                opby_party = ex_exvar(xinfo->inst, "opby_party", &s);
                opby_monster = ex_exvar(xinfo->inst, "opby_monster", &s);
            }
            opby_thing = ex_exvar(xinfo->inst, "opby_thing", &s);
            opby_party_face_found = ex_exvar(xinfo->inst, "opby_party_face", &opby_party_face);
            
            wrong_dir_untrigger = ex_exvar(xinfo->inst, "wrong_direction_untrigger", &s); 
            
            if (!wallitem && !teleporter) {
                const_weight = ex_exvar(xinfo->inst, "const_weight", &s);
                no_tc = ex_exvar(xinfo->inst, "no_tc", &s);
                off_trigger = ex_exvar(xinfo->inst, "off_trigger", &s);
            }
            
            if (!teleporter) {
                disable_self = ex_exvar(xinfo->inst, "disable_self", &s);
                destroy = ex_exvar(xinfo->inst, "destroy", &s);
            }
            
            if (teleporter) {
                int z;
                HWND wrongdir = GetDlgItem(hwnd, ESB_WRONGDIRUNTRIG);
                
                for(z=ESB_OPOPTIONS;z<=ESB_DESTROYOPBY;++z) {
                    HWND item = GetDlgItem(hwnd, z);
                    EnableWindow(item, FALSE);
                }
                
                ShowWindow(wrongdir, FALSE);
                
            } else if (wallitem) {
                int z;
                for(z=ESB_AIRBOX;z<=ESB_AIRONLY;++z) {
                    HWND item = GetDlgItem(hwnd, z);
                    EnableWindow(item, FALSE);
                }
            } else {
                // neither teleporter nor wallitem
                // enable all the targeting modes
                int z;
                for(z=ESB_TRM_ON;z<=ESB_TRMODEBOX;++z) {
                    HWND item = GetDlgItem(hwnd, z);
                    EnableWindow(item, TRUE);
                } 
            }   
            
            if (!wallitem) {
                HWND wrongdir = GetDlgItem(hwnd, ESB_WRONGDIRUNTRIG);
                
                SendMessage(pbox, CB_ADDSTRING, 0, (int)"Any Facing");
                SendMessage(pbox, CB_ADDSTRING, 0, (int)"Facing N");
                SendMessage(pbox, CB_ADDSTRING, 0, (int)"Facing E");
                SendMessage(pbox, CB_ADDSTRING, 0, (int)"Facing S");
                SendMessage(pbox, CB_ADDSTRING, 0, (int)"Facing W");
            
                if (opby_party_face_found) { 
                    SendMessage(pbox, CB_SETCURSEL, (opby_party_face % 4) + 1, 0);
                    
                    if (!teleporter) {
                        EnableWindow(wrongdir, TRUE);
                        if (wrong_dir_untrigger) {
                            SendMessage(wrongdir, BM_SETCHECK, BST_CHECKED, 0);
                        }
                    }
                    
                } else {
                    SendMessage(pbox, CB_SETCURSEL, 0, 0);
                    EnableWindow(wrongdir, FALSE);
                }
            }
            
            if (opby_party) {
                SendDlgItemMessage(hwnd, ESB_OPBY_PARTY, BM_SETCHECK, BST_CHECKED, 0);
                SendMessage(hwnd, WM_COMMAND, ESB_OPBY_PARTY, 0);
            }
            
            if (opby_thing)
                SendDlgItemMessage(hwnd, ESB_OPBY_THING, BM_SETCHECK, BST_CHECKED, 0);       
            if (opby_monster)
                SendDlgItemMessage(hwnd, ESB_OPBY_MONSTER, BM_SETCHECK, BST_CHECKED, 0);
            
            if (!wallitem && !teleporter) {
                if (const_weight) {
                    SendMessage(hwnd, WM_COMMAND, ESB_TRM_CONST, 0); 
                } else if (no_tc) {
                    SendMessage(hwnd, WM_COMMAND, ESB_TRM_NOTC, 0); 
                } else if (off_trigger) {
                    SendMessage(hwnd, WM_COMMAND, ESB_TRM_OFF, 0); 
                } else {
                    SendMessage(hwnd, WM_COMMAND, ESB_TRM_ON, 0); 
                }
            }
                 
            if (disable_self)
                SendDlgItemMessage(hwnd, ESB_DISABLESELF, BM_SETCHECK, BST_CHECKED, 0);       
            if (destroy)
                SendDlgItemMessage(hwnd, ESB_DESTROYOPBY, BM_SETCHECK, BST_CHECKED, 0); 
                
            if (teleporter) airstate = 1;
            if (ex_exvar(xinfo->inst, "air", &s)) airstate = 1;
            if (ex_exvar(xinfo->inst, "not_in_air", &s)) airstate = 0;
            if (ex_exvar(xinfo->inst, "air_only", &s)) airstate = 2;
            
            if (!wallitem) {
                if (airstate == 2)
                    SendDlgItemMessage(hwnd, ESB_AIRONLY, BM_SETCHECK, BST_CHECKED, 0); 
                else if (airstate == 1)
                    SendDlgItemMessage(hwnd, ESB_GROUNDAIR, BM_SETCHECK, BST_CHECKED, 0); 
                else
                    SendDlgItemMessage(hwnd, ESB_GROUNDONLY, BM_SETCHECK, BST_CHECKED, 0);
            }                      
              
            opby = exs_exvar(xinfo->inst, "opby");  
            if (opby) {
                SendDlgItemMessage(hwnd, ESB_OPBY_ARCH, BM_SETCHECK, BST_CHECKED, 0);
                SendMessage(hwnd, WM_COMMAND, ESB_OPBY_ARCH, 0);
                SetDlgItemText(hwnd, ESB_OP_WHAT, opby);
                dsbfree(opby);
                spec_opby = 1;
            }
            if (wallitem) {
                HWND opbypc = GetDlgItem(hwnd, ESB_OPBY_PARTYCARRY);
                EnableWindow(opbypc, FALSE);
                opbypc = GetDlgItem(hwnd, ESB_OPBY_NOTCARRY);
                EnableWindow(opbypc, FALSE);
            } else {
                if (ex_exvar(xinfo->inst, "except_when_carried", &s)) {
                    SendDlgItemMessage(hwnd, ESB_OPBY_NOTCARRY, BM_SETCHECK, BST_CHECKED, 0);  
                }
            }  
                      
            opby = exs_alt_exvar(xinfo->inst, "opby_party_carry", "opby");
            if (opby) {
                SendDlgItemMessage(hwnd, ESB_OPBY_PARTYCARRY, BM_SETCHECK, BST_CHECKED, 0);
                SendMessage(hwnd, WM_COMMAND, ESB_OPBY_PARTYCARRY, 0);
                SetDlgItemText(hwnd, ESB_OP_CARRY, opby);
                dsbfree(opby);
            }

            opby = exs_exvar(xinfo->inst, "opby_class");
            if (opby) {
                SendDlgItemMessage(hwnd, ESB_OPBY_ARCHCLASS, BM_SETCHECK, BST_CHECKED, 0);
                SendMessage(hwnd, WM_COMMAND, ESB_OPBY_ARCHCLASS, 0);
                SetDlgItemText(hwnd, ESB_OPARCH, opby);
                dsbfree(opby);
                spec_opby = 1;
            }
            
            if (wallitem) {
                if (spec_opby || opby_thing) {
                    SendDlgItemMessage(hwnd, ESB_OPBYSPECIFIC, BM_SETCHECK, BST_CHECKED, 0);
                    SendMessage(hwnd, WM_COMMAND, ESB_OPBYSPECIFIC, 0);
                } else if (opby_empty_only) {
                    SendDlgItemMessage(hwnd, ESB_EMPTYONLY, BM_SETCHECK, BST_CHECKED, 0); 
                    SendMessage(hwnd, WM_COMMAND, ESB_EMPTYONLY, 0);
                } else {
                    SendDlgItemMessage(hwnd, ESB_ANYCLICK, BM_SETCHECK, BST_CHECKED, 0);
                    SendMessage(hwnd, WM_COMMAND, ESB_ANYCLICK, 0);
                }     
            }
             
        } break;
            
        case WM_COMMAND: {
            int lww = LOWORD(wParam);
            switch (lww) {
                case IDOK: {
                    char buf[250];
                    int wallitem = 0;
                    int teleporter = 0;
                    int location = 1;
                    struct inst *p_inst;
                    struct obj_arch *p_arch;
                    int wallselmode = 0;
                    int const_weight = 0;
                    int destroy = 0;
                    int disable = 0;
                    int opby_party = 0;
                    int opby_party_face = 0;
                    int opby_thing = 0;
                    int opby_monster = 0;
                    int opby_except_when_carried = 0;
                    int wrong_dir_untrigger = 0;
                    int no_tc = 0;
                    int off_trigger = 0;
                    char *opby_arch = NULL;
                    char *opby_arch_class = NULL;
                    char *opby_party_carry_arch = NULL;
                    
                    xinfo = get_hwnd(hwnd);
                    p_inst = oinst[xinfo->inst];
                    p_arch = Arch(p_inst->arch);
                    if (p_arch->type == OBJTYPE_WALLITEM)
                        wallitem = 1;
                    if (queryluabool(p_arch, "esb_use_coordinates"))
                        teleporter = 1; 
                        
                    if (SendDlgItemMessage(hwnd, ESB_GROUNDAIR, BM_GETCHECK, 0, 0))
                        location = 2;
                    else if (SendDlgItemMessage(hwnd, ESB_AIRONLY, BM_GETCHECK, 0, 0))
                        location = 3;
                        
                    if (wallitem) {
                        if (SendDlgItemMessage(hwnd, ESB_ANYCLICK, BM_GETCHECK, 0, 0))
                            wallselmode = 1;
                        else if (SendDlgItemMessage(hwnd, ESB_EMPTYONLY, BM_GETCHECK, 0, 0))
                            wallselmode = 2;
                        else
                            wallselmode = 3;
                    }
                        
                    if (!wallitem) {
                        const_weight = SendDlgItemMessage(hwnd, ESB_TRM_CONST, BM_GETCHECK, 0, 0);
                        no_tc = SendDlgItemMessage(hwnd, ESB_TRM_NOTC, BM_GETCHECK, 0, 0);
                        off_trigger = SendDlgItemMessage(hwnd, ESB_TRM_OFF, BM_GETCHECK, 0, 0);
                    }
                    
                    if (wallselmode != 2)
                        destroy = SendDlgItemMessage(hwnd, ESB_DESTROYOPBY, BM_GETCHECK, 0, 0);
                    disable = SendDlgItemMessage(hwnd, ESB_DISABLESELF, BM_GETCHECK, 0, 0);
                    
                    opby_party = SendDlgItemMessage(hwnd, ESB_OPBY_PARTY, BM_GETCHECK, 0, 0); 
                    opby_party_face = SendDlgItemMessage(hwnd, ESB_PARTYFACING, CB_GETCURSEL, 0, 0);
                    if (opby_party_face > 0) {
                        wrong_dir_untrigger = SendDlgItemMessage(hwnd, ESB_WRONGDIRUNTRIG, BM_GETCHECK, 0, 0);
                    }
                    
                    opby_thing = SendDlgItemMessage(hwnd, ESB_OPBY_THING, BM_GETCHECK, 0, 0); 
                    // don't set opby_thing if we aren't in specific opby mode
                    if (wallitem && wallselmode != 3)
                        opby_thing = 0;
                        
                    opby_monster = SendDlgItemMessage(hwnd, ESB_OPBY_MONSTER, BM_GETCHECK, 0, 0); 
                    
                    if (SendDlgItemMessage(hwnd, ESB_OPBY_ARCH, BM_GETCHECK, 0, 0)) {
                        GetDlgItemText(hwnd, ESB_OP_WHAT, buf, sizeof(buf));
                        opby_arch = dsbstrdup(buf);
                    }
                    if (SendDlgItemMessage(hwnd, ESB_OPBY_ARCHCLASS, BM_GETCHECK, 0, 0)) {
                        GetDlgItemText(hwnd, ESB_OPARCH, buf, sizeof(buf));
                        opby_arch_class = dsbstrdup(buf);
                    }                    
                    if (SendDlgItemMessage(hwnd, ESB_OPBY_PARTYCARRY, BM_GETCHECK, 0, 0)) {
                        GetDlgItemText(hwnd, ESB_OP_CARRY, buf, sizeof(buf));
                        opby_party_carry_arch = dsbstrdup(buf);
                        opby_except_when_carried = SendDlgItemMessage(hwnd, ESB_OPBY_NOTCARRY, BM_GETCHECK, 0, 0); 
                    }
                    
                    luastacksize(21);
                    lua_getglobal(LUA, "__ed_opbyed_store");
                    lua_pushinteger(LUA, xinfo->inst);
                    lua_pushboolean(LUA, wallitem);
                    lua_pushboolean(LUA, teleporter);
                    lua_pushinteger(LUA, location);
                    lua_pushinteger(LUA, wallselmode);
                    lua_pushboolean(LUA, const_weight);
                    lua_pushboolean(LUA, destroy);
                    lua_pushboolean(LUA, disable);
                    lua_pushboolean(LUA, no_tc);
                    lua_pushboolean(LUA, off_trigger);
                    lua_pushboolean(LUA, opby_party);
                    lua_pushinteger(LUA, opby_party_face);
                    lua_pushboolean(LUA, wrong_dir_untrigger);
                    lua_pushboolean(LUA, opby_thing);
                    lua_pushboolean(LUA, opby_monster);
                    CONDPUSHSTRING(LUA, opby_arch);
                    CONDPUSHSTRING(LUA, opby_arch_class);
                    CONDPUSHSTRING(LUA, opby_party_carry_arch);
                    lua_pushboolean(LUA, opby_except_when_carried);
                    if (lua_pcall(LUA, 19, 0, 0))
                        bomb_lua_error(NULL);

                    SendMessage(hwnd, WM_CLOSE, 0, 0);
                    dsbfree(opby_arch);
                    dsbfree(opby_arch_class);
                    dsbfree(opby_party_carry_arch);
                } break;
                
                case IDCANCEL: {
                    SendMessage(hwnd, WM_CLOSE, 0, 0);
                } break;
                
                case ESB_ANYCLICK:
                case ESB_EMPTYONLY: {
                    int wi;
                    for(wi=ESB_OPBY_THING;wi<=ESB_OPARCH;wi++) {
                        HWND wi_hwnd = GetDlgItem(hwnd, wi);
                        EnableWindow(wi_hwnd, FALSE);
                    } 
                    if (lww == ESB_EMPTYONLY) {
                        HWND oppc = GetDlgItem(hwnd, ESB_OPBY_PARTYCARRY);
                        HWND opdestroy = GetDlgItem(hwnd, ESB_DESTROYOPBY);
                        EnableWindow(oppc, TRUE);
                        SendMessage(hwnd, WM_COMMAND, ESB_OPBY_PARTYCARRY, 0);  
                        EnableWindow(opdestroy, FALSE);
                    } else {
                        HWND opdestroy = GetDlgItem(hwnd, ESB_DESTROYOPBY);
                        EnableWindow(opdestroy, TRUE);
                    }   
                        
                } break;
                
                case ESB_OPBYSPECIFIC: {
                    HWND opthing = GetDlgItem(hwnd, ESB_OPBY_THING);
                    HWND oparch = GetDlgItem(hwnd, ESB_OPBY_ARCH);
                    HWND oparchclass = GetDlgItem(hwnd, ESB_OPBY_ARCHCLASS);
                    HWND oppc = GetDlgItem(hwnd, ESB_OPBY_PARTYCARRY);
                    HWND op_c = GetDlgItem(hwnd, ESB_OP_CARRY);
                    HWND op_choose = GetDlgItem(hwnd, ESB_OPCHOOSECARRY);
                    HWND opdestroy = GetDlgItem(hwnd, ESB_DESTROYOPBY);
                    EnableWindow(opdestroy, TRUE);
                    EnableWindow(opthing, TRUE);
                    SendMessage(hwnd, WM_COMMAND, ESB_OPBY_THING, 0);
                    EnableWindow(oparch, TRUE);
                    SendMessage(hwnd, WM_COMMAND, ESB_OPBY_ARCH, 0); 
                    EnableWindow(oparchclass, TRUE);
                    SendMessage(hwnd, WM_COMMAND, ESB_OPBY_ARCHCLASS, 0); 
                    EnableWindow(oppc, FALSE);
                    EnableWindow(op_c, FALSE);
                    EnableWindow(op_choose, FALSE);    
                } break;
                                
                case ESB_OPBY_PARTY: {
                    struct obj_arch *p_xinfo_arch;
                    xinfo = get_hwnd(hwnd);
                    p_xinfo_arch = Arch(oinst[xinfo->inst]->arch);
                    if (p_xinfo_arch->type != OBJTYPE_WALLITEM) {
                        HWND ckb = GetDlgItem(hwnd, ESB_OPBY_PARTY);
                        HWND wi_1 = GetDlgItem(hwnd, ESB_PARTYFACING);
                        HWND wrongdir = GetDlgItem(hwnd, ESB_WRONGDIRUNTRIG);
                        int chk = SendMessage(ckb, BM_GETCHECK, 0, 0);
                                              
                        EnableWindow(wi_1, !!chk);
                        if (!chk)
                            EnableWindow(wrongdir, FALSE);
                        else
                            SendMessage(hwnd, WM_COMMAND, ESB_PARTYFACING, 0);
                    }
                } break;
                
                case ESB_OPBY_ARCH: {
                    HWND ckb = GetDlgItem(hwnd, ESB_OPBY_ARCH);
                    HWND wi_1 = GetDlgItem(hwnd, ESB_OPCHOOSEWHAT);
                    HWND wi_2 = GetDlgItem(hwnd, ESB_OP_WHAT);
                    int chk = SendMessage(ckb, BM_GETCHECK, 0, 0);
                    EnableWindow(wi_1, !!chk);
                    EnableWindow(wi_2, !!chk);
                } break;
                
                case ESB_OPBY_PARTYCARRY: {
                    HWND ckb = GetDlgItem(hwnd, ESB_OPBY_PARTYCARRY);
                    HWND wi_1 = GetDlgItem(hwnd, ESB_OPCHOOSECARRY);
                    HWND wi_2 = GetDlgItem(hwnd, ESB_OP_CARRY);
                    HWND wi_3 = GetDlgItem(hwnd, ESB_OPBY_NOTCARRY);
                    int chk = SendMessage(ckb, BM_GETCHECK, 0, 0);
                    EnableWindow(wi_1, !!chk);
                    EnableWindow(wi_2, !!chk);
                    EnableWindow(wi_3, !!chk);
                } break;
                
                case ESB_OPBY_ARCHCLASS: {
                    HWND ckb = GetDlgItem(hwnd, ESB_OPBY_ARCHCLASS);
                    HWND wi_1 = GetDlgItem(hwnd, ESB_OPARCH);
                    int chk = SendMessage(ckb, BM_GETCHECK, 0, 0);
                    EnableWindow(wi_1, !!chk);
                } break;
                
                case ESB_OPCHOOSEWHAT: {
                    struct obj_arch *p_xinfo_arch;
                    xinfo = get_hwnd(hwnd);
                    p_xinfo_arch = Arch(oinst[xinfo->inst]->arch);
                    edg.op_inst = xinfo->inst;
                    edg.op_arch = 4;
                    if (p_xinfo_arch->type != OBJTYPE_WALLITEM)
                        edg.op_arch = (5 << 8) | 4;
                    ESB_modaldb(ESB_ARCHPICKER, hwnd, ESB_archpick_proc, 0); 
                    if (edg.op_arch != 0xFFFFFFFF) {
                        struct obj_arch *p_arch = Arch(edg.op_arch);
                        SetDlgItemText(hwnd, ESB_OP_WHAT, p_arch->luaname);
                    }
                } break;
                
                case ESB_OPCHOOSECARRY: {
                    xinfo = get_hwnd(hwnd);
                    edg.op_inst = xinfo->inst;
                    edg.op_arch = 4;
                    ESB_modaldb(ESB_ARCHPICKER, hwnd, ESB_archpick_proc, 0); 
                    if (edg.op_arch != 0xFFFFFFFF) {
                        struct obj_arch *p_arch = Arch(edg.op_arch);
                        SetDlgItemText(hwnd, ESB_OP_CARRY, p_arch->luaname);
                    }
                } break;
                
                case ESB_TRM_ON:
                case ESB_TRM_OFF:
                case ESB_TRM_CONST:
                case ESB_TRM_NOTC: {
                    int z;
                    for(z=ESB_TRM_ON;z<=ESB_TRM_NOTC;++z) {
                        if (z != lww)
                            SendDlgItemMessage(hwnd, z, BM_SETCHECK, BST_UNCHECKED, 0);
                    }
                    SendDlgItemMessage(hwnd, lww, BM_SETCHECK, BST_CHECKED, 0); 
                } break;
                
                case ESB_GROUNDONLY:
                case ESB_GROUNDAIR:
                case ESB_AIRONLY: {
                    int z;
                    for(z=ESB_GROUNDONLY;z<=ESB_AIRONLY;++z) {
                        if (z != lww)
                            SendDlgItemMessage(hwnd, z, BM_SETCHECK, BST_UNCHECKED, 0);
                    }
                    SendDlgItemMessage(hwnd, lww, BM_SETCHECK, BST_CHECKED, 0); 
                } break;
                
                
                case ESB_PARTYFACING: {
                    int csel = SendDlgItemMessage(hwnd, ESB_PARTYFACING, CB_GETCURSEL, 0, 0);
                    HWND wrongdir = GetDlgItem(hwnd, ESB_WRONGDIRUNTRIG);
                            
                    if (csel > 0) { 
                        EnableWindow(wrongdir, TRUE);
                    } else {
                        EnableWindow(wrongdir, FALSE);
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
