#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <commctrl.h>
#include <shlobj.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "objects.h"
#include "global_data.h"
#include "uproto.h"
#include "compile.h"
#include "arch.h"
#include "trans.h"
#include "E_res.h"
#include "champions.h"
#include "editor.h"
#include "editor_hwndptr.h"
#include "editor_gui.h"
#include "editor_shared.h"
#include "editor_menu.h"
#include "editor_clipboard.h"

HTREEITEM global_ws_treenode;

extern FILE *errorlog;
extern HWND sys_hwnd;
extern lua_State *LUA;
extern struct editor_global edg;
extern struct global_data gd;
extern struct inventory_info gii;

extern struct inst *oinst[NUM_INST];
extern struct dungeon_level *dun;
extern struct translate_table Translate;

void ed_purge_treeview(HWND tv) {
    onstack("ed_purge_treeview");
    edg.in_purge++;
    TreeView_DeleteAllItems(tv);
    edg.in_purge--;
    VOIDRETURN();
}

void destroy_eextch() {
    int i;
    for (i=0;i<gd.num_champs;++i) {
        dsbfree(edg.c_ext[i].desg);
    }
    dsbfree(edg.c_ext);
    edg.c_ext = NULL;   
}

// start in the current directory rather than in the root
int CALLBACK esb_selectcallback(HWND hwnd, UINT u_msg, LPARAM lp, LPARAM data) {
   TCHAR ts_dir[MAX_PATH];

   switch(u_msg) {
       case BFFM_INITIALIZED:
          if (GetCurrentDirectory(sizeof(ts_dir)/sizeof(TCHAR), ts_dir)) {
             // wparam is TRUE since you are passing a path
             SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)ts_dir);
          }
          break;

       case BFFM_SELCHANGED:
          if (SHGetPathFromIDList((LPITEMIDLIST) lp ,ts_dir)) {
             SendMessage(hwnd, BFFM_SETSTATUSTEXT, 0, (LPARAM)ts_dir);
          }
          break;
       }

    return 0;
}

int editor_create_new_gamedir() {
    BROWSEINFO brinfo;
    LPITEMIDLIST p_dpl;
    char *new_dprefix = NULL;
    char dname[MAX_PATH];
    char rdname[MAX_PATH];
    char bdname[MAX_PATH];
    char edname[MAX_PATH];
    char *errcond = NULL;
    int got_path = 0;
    int y;
    FILE *fp;
    
    onstack("editor_create_new_gamedir");
    
    GetCurrentDirectory(MAX_PATH, dname);
    GetCurrentDirectory(MAX_PATH, rdname);
    sprintf(bdname, "%s\\base", rdname);
    sprintf(edname, "%s\\editor", rdname);
        
    memset(&brinfo, 0, sizeof(BROWSEINFO));
    brinfo.hwndOwner = sys_hwnd;
    brinfo.lpszTitle = "Please choose a location to place your new dungeon.\n"
        "It should be an empty folder, which you can also create here.";
    brinfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_STATUSTEXT;
    brinfo.lpfn = esb_selectcallback;
    brinfo.pszDisplayName = dname;
    
    p_dpl = SHBrowseForFolder(&brinfo);
    
    if (p_dpl) {
        TCHAR ts_path[MAX_PATH];
        LPMALLOC p_malloc;
        
        if (SHGetPathFromIDList(p_dpl, ts_path)) {
            new_dprefix = dsbstrdup(ts_path);
        }
        
        if (SUCCEEDED(SHGetMalloc(&p_malloc))) {
            p_malloc->lpVtbl->Free(p_malloc, p_dpl);
            p_malloc->lpVtbl->Release(p_malloc);
        }
    } 
    
    if (!new_dprefix) {
        RETURN(0);
    }
    
    if (!strcmp(new_dprefix, rdname)) {
        errcond = "Cannot create dungeon in the DSB root.";    
    } else if (!strcmp(new_dprefix, bdname)) {
        errcond = "Cannot create dungeon in the base code folder.";
    } else if (!strcmp(new_dprefix, edname)) {
        errcond = "Cannot create dungeon in the editor folder.";
    }
    
    if (errcond) {
        char errbuf[1000];
        sprintf(errbuf, "%s\n\nNew dungeons should be placed in a new, empty subfolder.", errcond);
        MessageBox(NULL, errbuf, "Location Error", MB_ICONEXCLAMATION);
        dsbfree(new_dprefix);
        RETURN(0);
    }

    sprintf(dname, "%s\\%s", new_dprefix, "dungeon.lua");
    fp = fopen(dname, "r");
    if (fp) {
        char errbuf[1000];
        fclose(fp);
        sprintf(errbuf, "A dungeon.lua already exists in %s.\nYou must specify an empty folder.", new_dprefix);
        MessageBox(NULL, errbuf, "Error", MB_ICONEXCLAMATION);
        dsbfree(new_dprefix);
        RETURN(0);
    }
    
    fp = fopen(dname, "w");
    if (!fp) {
        char errbuf[1000];
        fclose(fp);
        sprintf(errbuf, "Could not write into %s.\nYou must specify a folder with write permissions.", new_dprefix);
        MessageBox(NULL, errbuf, "Error", MB_ICONEXCLAMATION);
        dsbfree(new_dprefix);
        RETURN(0);
    }
    
    fprintf(fp, "---DSB ESB---\n");
    fprintf(fp, "dsb_text2map(0, 32, 32, 100, 1, {\n");
    fprintf(fp, "\"10000000000000000000000000000000\",");
    for(y=0;y<31;y++) {
        fprintf(fp, "\"00000000000000000000000000000000\",");
    }
    fprintf(fp, "} )\n");
    fprintf(fp, "dsb_level_wallset(0, wallset.default)\n");
    fprintf(fp, "dsb_party_place(0, 0, 0, 0)\n");
    fflush(fp);
    fclose(fp);
    
    editor_load_dungeon(dname);
    dsbfree(new_dprefix);
    
    dsbfree(edg.valid_savename);
    edg.valid_savename = dsbstrdup(dname);
    
    RETURN(1);
}

int editor_load_dungeon(char *fname) {
    unsigned static int i_chkv[3] = { KEY_VAL_1, KEY_VAL_2, KEY_VAL_3 };
    PACKFILE *ef;
    int rv;
    int n = 0;
    int fail = 0;
    char tdbuffer[1024];

    onstack("editor_load_dungeon");

    ef = pack_fopen(fname, F_READ);
    if (ef == NULL) {
        RETURN(1);
    }

    while(!fail & n < 3) {
        unsigned int i_r;
        i_r = pack_mgetl(ef);
        if (i_r != i_chkv[n])
            fail = 1;
        ++n;
    }

    pack_fclose(ef);

    if (fail) {
        int z = MessageBox(sys_hwnd,
        "This dungeon was not created by ESB.\nSome data may not import correctly and might be lost if you save.\n\nLoad it anyway?", "Nonstandard File",
            MB_ICONINFORMATION|MB_YESNO);

        /* didn't click YES */
        if (z != 6)
            RETURN(2);
    }
    
    edg.runmode = EDITOR_PARSING;
    
    edg.draw_mode = DRM_DONOTHING;
    dsbfree(edg.draw_arch);
    edg.draw_arch = NULL;
    dsbfree(edg.draw_ws);
    edg.draw_ws = NULL;
    
    deselect_edit_clipboard();
    destroy_edit_clipboard();
    destroy_eextch();
    destroy_dungeon();
    lua_close(LUA);
    purge_all_arch();
    purge_translation_table(&Translate);
    ed_purge_treeview(edg.treeview);
    purge_dirty_list();
    
    dsbfree(gd.dprefix);
    memset(tdbuffer, 0, sizeof(tdbuffer));
    GetCurrentDirectory(sizeof(tdbuffer), tdbuffer);
    gd.dprefix = dsbstrdup(tdbuffer);
    
    SetCurrentDirectory(edg.curdir);
    
    gd.exestate = STATE_LOADING;
    initlua(bfixname("lua.dat"));
    gd.lua_init = 1;
    //initlua("internal/internal.lua");
    lc_parm_int("__editor_metatables", 0);
    edg.editor_flags = 255;
    src_lua_file(bfixname("global.lua"), 0);
    src_lua_file("editor/editor.lua", 0);
    init_exvars();
    src_lua_file(bfixname("inventory_info.lua"), 0);
    src_lua_file(bfixname("util.lua"), 0);
    src_lua_file("editor/custom.lua", 1);
    
    destroy_dynalloc_constants();
    import_lua_constants();
    
    setup_editor_paths(fname);
    src_lua_file(fixname("startup.lua"), 1);
    
    gd.exestate = STATE_OBJPARSE;
    initialize_objects();
    load_system_objects(LOAD_OBJ_ALL);

    gd.exestate = STATE_DUNGEONLOAD;
    esb_clear_used_wallset_table();
    src_lua_file(fname, 0);
    lc_parm_int("__editor_fix_wallsets", 0);
    
    edg.ed_lev = gd.p_lev[0];
    ed_init_level_bmp(edg.ed_lev, sys_hwnd);
    ed_resizesubs(sys_hwnd);
    ed_build_tree(edg.treeview, 0, NULL);
    edg.last_inst = 0;
    edg.global_mark_level = -1;
    
    luastacksize(2);
    lua_getglobal(LUA, "EDITOR_FLAGS"); 
    if (lua_isnumber(LUA, -1)) {
        edg.editor_flags = lua_tointeger(LUA, -1);
    }
    lua_pop(LUA, 1);
    
    gd.exestate = STATE_EDITOR;
    edg.runmode = EDITOR_RUNNING;
    
    sprintf(tdbuffer, "%s loaded.", fname);
    SetWindowText(edg.infobar, tdbuffer);
    purge_dirty_list();
    
    fflush(errorlog);

    RETURN(0);
}

void ed_get_drawnumber(struct obj_arch *p_arch, int dir, int inact, int cell,
    int *num, int *ord)
{
    const char *cs_edr = "editor_drawnumber";
    
    luaonstack(cs_edr);

    lua_getglobal(LUA, cs_edr);
    lua_pushstring(LUA, p_arch->luaname);
    lua_pushinteger(LUA, dir);
    lua_pushboolean(LUA, inact);
    lua_pushboolean(LUA, cell);

    if (lua_pcall(LUA, 4, 2, 0) != 0)
        lua_function_error(cs_edr, lua_tostring(LUA, -1));
    
    if (lua_isnil(LUA, -2)) {
        *num = -1;
        *ord = 0;
        lua_pop(LUA, 2);
        VOIDRETURN();
    }
        
    *num = lua_tointeger(LUA, -2);
    *ord = lua_tointeger(LUA, -1);
    
    lua_pop(LUA, 2);
    lstacktop();
    
    VOIDRETURN();
}

int ed_obj_add(const char *arch_name, int level, int x, int y, int dir) {
    const char *cs_edr = "editor_spawn";
    int rv = 0;

    luaonstack(cs_edr);
    
    luastacksize(6);
    purge_dirty_list();
    
    lua_getglobal(LUA, cs_edr);
    lua_pushstring(LUA, arch_name);
    lua_pushinteger(LUA, level);
    lua_pushinteger(LUA, x);
    lua_pushinteger(LUA, y);
    lua_pushinteger(LUA, dir);
    
    if (lua_pcall(LUA, 5, 1, 0) != 0)
        lua_function_error(cs_edr, lua_tostring(LUA, -1));
        
    if (!lua_isnumber(LUA, -1) || (rv = lua_tointeger(LUA, -1)) <= 0) {
        poop_out("Lua function editor_spawn did not return a valid instance.\n\n"
            "If you are using an unmodified editor.lua, this is a bug in ESB.");
        RETURN(0);
    }    
    lua_pop(LUA, 1);
    
    RETURN(rv);
}

HTREEITEM treeadd(HWND tree, HTREEITEM h_par,
    HTREEITEM h_ins_after, const char *str, char internalcode)
{
    TV_ITEM *tvi;
    TV_INSERTSTRUCT tv_ins;
    HTREEITEM h_item;
    
    onstack("treeadd");
    
    tv_ins.hParent = h_par;
    tv_ins.hInsertAfter = h_ins_after;
    tvi = &(tv_ins.item);
    
    tvi->mask = TVIF_TEXT | TVIF_PARAM;
    tvi->pszText = (char*)str;
    tvi->cchTextMax = strlen(str);

    if (internalcode) {
        char vstr[250];
        char *dupstr;
        snprintf(vstr, sizeof(vstr), "%c%s", internalcode, str);
        dupstr = dsbstrdup(vstr);
        tvi->lParam = (LPARAM)dupstr;
    } else {
        tvi->lParam = 0;
    }
        
    h_item = (HTREEITEM)SendMessage(tree, TVM_INSERTITEM, 0, (LPARAM)&tv_ins);
    
    RETURN(h_item);
}

void ed_m_main_tree_ws_add(const char *lstr) {
    treeadd(edg.treeview, global_ws_treenode, TVI_LAST, lstr, 'w');  
}

void ed_build_tree_wallsets(HWND tree) {
    if (tree != edg.treeview)
        return;
        
    global_ws_treenode = treeadd(tree, NULL, TVI_LAST, "Wallsets", 0);
       
    edg.op_hwnd = sys_hwnd;
    edg.op_b_id = -1;
    edg.op_winmsg = EDM_MAIN_TREE_WS_ADD;           
    lc_parm_int("__ed_add_wallsets", 0);     
}

void ed_main_treeview_refresh_wallsets(void) {
    SendMessage(edg.treeview, TVM_DELETEITEM, 0, (int)global_ws_treenode);
    ed_build_tree_wallsets(edg.treeview);    
}

void ed_build_tree(HWND tree, int category, const char *base_sel) {
    static const char *cs_otbl = "__object_table";
    
    char vxname[7][2][20] = {
        { "FloorFlats", "FLOORFLAT" },
        { "FloorUprights", "FLOORUPRIGHT" },
        { "Doors", "DOOR" },
        { "WallItems", "WALLITEM" },
        { "Things", "THING" },
        { "Monsters", "MONSTER" },
        { "Hazes", "HAZE" }
    };
    
    int vtype;
    
    v_onstack("ed_build_tree");
    
    luastacksize(8);

    for (vtype=0;vtype<7;++vtype) {
        HTREEITEM htbase;
        int vclass = 1;
        
        if (category > 0) {
            if (category & 0xFF00) {
                int cat1 = category & 0xFF;
                int cat2 = (category & 0xFF00) >> 8;
                if (vtype != cat1 && vtype != cat2)
                    continue;
            } else if (vtype != category)
                continue;
        }

        htbase = treeadd(tree, NULL, TVI_LAST, vxname[vtype][0], 0);
        
        lua_getglobal(LUA, cs_otbl);
        lua_pushstring(LUA, vxname[vtype][1]);
        
        if (lua_pcall(LUA, 1, 2, 0) != 0)
            lua_function_error(cs_otbl, lua_tostring(LUA, -1));

        while (vclass < 100) {
            const char *cl_name;
            int vitem = 1;
            HTREEITEM htpar;
            
            lua_pushinteger(LUA, vclass);
            lua_gettable(LUA, -2);
            
            if (lua_isnil(LUA, -1)) {
                lua_pop(LUA, 1);
                break;
            }
            
            cl_name = lua_tostring(LUA, -1);
            htpar = treeadd(tree, htbase, TVI_SORT, cl_name, 0);
            
            lua_pushstring(LUA, cl_name);
            lua_gettable(LUA, -4);

            while (vitem < 1000) {
                HTREEITEM newitem;
                const char *s_iname;
                
                lua_pushinteger(LUA, vitem);
                lua_gettable(LUA, -2);

                if (lua_isnil(LUA, -1)) {
                    lua_pop(LUA, 1);
                    break;
                }
                s_iname = lua_tostring(LUA, -1);
                
                newitem = treeadd(tree, htpar, TVI_SORT, s_iname, 'i');
                if (base_sel && !strcmp(s_iname, base_sel)) {
                    edg.op_data_ptr = (void*)newitem;
                }
                
                lua_pop(LUA, 1);
                ++vitem;
            }
            
            lua_pop(LUA, 2);
            ++vclass;
        }
        lua_pop(LUA, 2);
    }
    
    if (category == 0) {
        ed_build_tree_wallsets(tree);
    }
    
    lstacktop();
    
    v_upstack();
}

void update_draw_mode(HWND ehwnd) {
    char buffer[256];
    int i_drmode = edg.draw_mode;
    char *cs_txstr;
    
    onstack("update_draw_mode");
    
    if (i_drmode == DRM_DRAWDUN)
        cs_txstr = "Dungeon Drawing Mode";
        
    else if (i_drmode == DRM_PARTYSTART)
        cs_txstr = "Choose Party Start Position";
        
    else if (i_drmode == DRM_CUTPASTE)
        cs_txstr = "Cut and Paste Mode";
        
    else if (i_drmode == DRM_WALLSET) {
        snprintf(buffer, sizeof(buffer), "Painting WS: %s", edg.draw_ws);
        cs_txstr = buffer;
    }
        
    else if (i_drmode == DRM_DRAWOBJ) {
        snprintf(buffer, sizeof(buffer), "Adding: %s", edg.draw_arch);
        cs_txstr = buffer;
    }
    
    SendMessage(ehwnd, SB_SETTEXT, 1, (LPARAM)cs_txstr);
    
    VOIDRETURN();
}

void SetD(unsigned char dmode, char *s_etext, char *s_wtext) {
    int zt, ps;
    HMENU winmenu;
    HMENU edmenu;
    
    onstack("SetD");
    
    if (s_etext != edg.draw_arch) {
        dsbfree(edg.draw_arch);
        edg.draw_arch = dsbstrdup(s_etext);
    }
    if (dmode == DRM_DRAWOBJ && !edg.draw_arch)
        VOIDRETURN();
        
    if (s_wtext != edg.draw_ws) {
        dsbfree(edg.draw_ws);
        edg.draw_ws = dsbstrdup(s_wtext);
    }
    if (dmode == DRM_WALLSET && !edg.draw_ws)
        VOIDRETURN();
        
    edg.wsd_x = -10;
    edg.wsd_y = -10;
        
    edg.draw_mode = dmode;
    if (dmode != DRM_CUTPASTE)
        deselect_edit_clipboard();
    if (dmode == DRM_PARTYSTART) {
        if (edg.d_dir > 3) {
            edg.d_dir = 0;
            edg.f_d_dir = -1;
            edg.autosel = 0;
        }
    }
    
    force_redraw(edg.subwin, 1);
    
    edg.force_zone++;
    zt = locw_update(edg.loc, NULL);
    edg.force_zone--;
    
    if (dmode == DRM_DRAWOBJ)
        ed_set_autosel_modes(zt);
    QO_MOUSEOVER();
        
    winmenu = GetMenu(sys_hwnd);
    edmenu = GetSubMenu(winmenu, 1);
    
    if (!edmenu)
        VOIDRETURN();
    
    for(ps=1;ps<MAX_DRM_VAL;++ps) {
        CheckMenuItem(edmenu, ps-1, MF_BYPOSITION | MF_UNCHECKED);
    }
    if (dmode > 0)
        CheckMenuItem(edmenu, dmode-1, MF_BYPOSITION | MF_CHECKED);
        
    VOIDRETURN();
}

int lgetarch(const char *cs_arch) {
    int nv;
    
    luastacksize(4);

    lua_getglobal(LUA, "obj");
    lua_pushstring(LUA, cs_arch);
    lua_gettable(LUA, -2);
    lua_pushstring(LUA, "regnum");
    lua_gettable(LUA, -2);
    
    nv = (int)lua_touserdata(LUA, -1);
    
    lua_pop(LUA, 3);
    
    return nv;
}

void ed_tag_l(const char *s_add_me) {
    onstack("ed_tag_l");
    
    if (edg.op_b_id == -1)
        SendMessage(edg.op_hwnd, edg.op_winmsg, 0, (LPARAM)s_add_me);
    else {
        SendDlgItemMessage(edg.op_hwnd, edg.op_b_id,
            edg.op_winmsg, 0, (LPARAM)s_add_me);
            
        if (edg.op_hwnd_extent > 0) {
            int n_extent = text_length(font, s_add_me) * 0.7;
            if (n_extent > edg.op_hwnd_extent)
                edg.op_hwnd_extent = n_extent;
        } 

    }
    
    VOIDRETURN();
}

char *calctpos(int i, int solid) {
    static char d[3];
    memset(d, 0, sizeof(d));
    if (i == DIR_CENTER) {
        d[0] = 'C';
    } else {
        if (solid) {
            if (i == DIR_NORTH) d[0] = 'N';
            else if (i == DIR_EAST) d[0] = 'E';
            else if (i == DIR_SOUTH) d[0] = 'S';
            else d[0] = 'W';
        } else {
            if (i <= DIR_NORTHEAST) d[0] = 'N';
            else d[0] = 'S';
            
            if (i == DIR_NORTHWEST || 
                i == DIR_SOUTHWEST)
            {
                d[1] = 'W';
            } else d[1] = 'E';
        }
    }      
    return d;
}

int ed_fillboxfromtile(HWND ehwnd, int boxid, int lev, int tx, int ty, int dcurs) {
    int i;
    struct dungeon_level *dd;
    int i_ele = 0;
    int filltype = boxid;
    int fillflags = 0;
    int ctrlfill = 1;
    char fbuf[1000];
    int bl = sizeof(fbuf);
    int left = bl;
    
    onstack("ed_fillboxfromtile");
    
    if (filltype < 0) {
        boxid = 0;
        fillflags = -1 * filltype;
        ctrlfill = 0;
        fbuf[0] = '\0';
    }
    
    SendDlgItemMessage(ehwnd, boxid, LB_RESETCONTENT, 0, 0);
    
    if (lev == LOC_IN_OBJ || lev == LOC_CHARACTER) {
        int found_num = 0;
        int *array = NULL;
        int array_n = 0;
        int try_name_slot = 0;
        
        if (lev == LOC_IN_OBJ) {
            int in_inst = tx;
            struct inst *p_con_inst = NULL;
            p_con_inst = oinst[in_inst];
            if (p_con_inst) {
                array = p_con_inst->inside;
                array_n = p_con_inst->inside_n;
            }
        } else {
            int in_char = tx;
            struct champion *me = &(gd.champs[in_char]);
            array = me->inv;
            array_n = gii.max_invslots;
            try_name_slot = 1;
            luastacksize(8);
            lua_getglobal(LUA, "inventory_info");
        }
        
        if (array && array_n) {
            int i;
            for (i=0;i<array_n;++i) {
                int iinst = array[i];
                if (iinst && oinst[iinst]) {
                    struct inst *p_inst = oinst[iinst];
                    struct obj_arch *p_arch = Arch(p_inst->arch);
                    char buf[60];
                    char slotdesg[15];
                    int idx;
                    int failname = 1;
                    
                    if (try_name_slot) {
                        const char *sname = NULL;
                        
                        lua_pushinteger(LUA, i);
                        lua_gettable(LUA, -2);
                        if (lua_istable(LUA, -1)) {
                            lua_pushstring(LUA, "name");
                            lua_gettable(LUA, -2);
                            if (lua_isstring(LUA, -1)) {
                                sname = lua_tostring(LUA, -1);
                            } else {
                                lua_pop(LUA, 2);
                            }
                        } else {
                            lua_pop(LUA, 1);
                        }
                            
                        if (sname) {
                            snprintf(slotdesg, sizeof(slotdesg), "%ld/%s", i, sname); 
                            lua_pop(LUA, 2);
                            failname = 0;
                        } else
                            failname = 1;   
                    }
                    
                    if (failname)
                        snprintf(slotdesg, sizeof(slotdesg), "%ld", i);
                    
                    snprintf(buf, sizeof(buf),
                        "<%s>%s%s [%lu]", slotdesg,
                            (p_inst->gfxflags & OF_INACTIVE)? "(I) " : " ",
                            p_arch->luaname, iinst);

                    if (ctrlfill) {
                        idx = SendDlgItemMessage(ehwnd, boxid, LB_ADDSTRING,
                            0, (LPARAM)buf);
                        SendDlgItemMessage(ehwnd, boxid, LB_SETITEMDATA,
                            idx, iinst);
                    } else {
                        left -= 2;
                        strncat(fbuf, buf, left);
                        strncat(fbuf, "\r\n", left);
                        left -= strlen(buf);
                    }
                        
                    found_num++;
                }    
            }    
        }
        i_ele = 0;
        
        if (try_name_slot)
            lua_pop(LUA, 1);
        
        goto FINISH_IT;
    }

    dd = &(dun[lev]);

    for (i=0;i<=4;++i) {
        struct inst_loc *dt = dd->t[ty][tx].il[i];
        int flw = (dd->t[ty][tx].w & 1);
        char buf[50];
        while (dt != NULL) {
            int idx;
            char *slvstr;
            struct inst *p_inst = oinst[dt->i];
            struct obj_arch *p_arch = Arch(p_inst->arch);
            
            if (dt->slave) slvstr = "](s)";
            else slvstr = "]";
            
            snprintf(buf, sizeof(buf),
                "<%s>%s%s [%lu%s", calctpos(i, flw),
                    (p_inst->gfxflags & OF_INACTIVE)? "(I) " : " ",
                    p_arch->luaname, dt->i, slvstr);

            if (ctrlfill) {
                idx = SendDlgItemMessage(ehwnd, boxid, LB_ADDSTRING,
                    0, (LPARAM)buf);
                SendDlgItemMessage(ehwnd, boxid, LB_SETITEMDATA, idx, dt->i);
            } else {
                left -= 2;
                strncat(fbuf, buf, left);
                strncat(fbuf, "\r\n", left);
                left -= strlen(buf);
            }

            ++i_ele;

            dt = dt->n;
        }
    }
    
    FINISH_IT:
    if (ctrlfill) {
        if (dcurs >= 0)
            SendDlgItemMessage(ehwnd, boxid, LB_SETCURSEL, dcurs, 0);
    } else {
        SetWindowText(ehwnd, fbuf);    
    }
    
    RETURN(i_ele);

}

void ed_reselect_inst(HWND hwnd, int item, int cinst) {
    int zcn = 1;
    int sl = 0;
    
    onstack("ed_reselect_inst");
    
    if (!cinst)
        VOIDRETURN();
    
    while (zcn != 0) {
        zcn = SendDlgItemMessage(hwnd, item, LB_GETITEMDATA, sl, 0);
        if (zcn == cinst) {
            SendDlgItemMessage(hwnd, item, LB_SETCURSEL, sl, 0);
            break;
        }
        ++sl;
    }
    VOIDRETURN();
}

char *ed_makeselexvar(HWND hlist, int *retselex, char *dt) {
    int selexv;
    
    onstack("ed_makeselexvar");

    selexv = SendMessage(hlist, LB_GETCURSEL, 0, 0);
    if (selexv != LB_ERR) {
        char *tnpr;
        char *txf;
        int mlen;
        
        mlen = SendMessage(hlist, LB_GETTEXTLEN, selexv, 0);
        txf = dsbcalloc(mlen + 1, 1);
        SendMessage(hlist, LB_GETTEXT, selexv, (LPARAM)txf);
        tnpr = txf;
        while (*tnpr != '=')
            ++tnpr;
        --tnpr;
        *tnpr = '\0';
        
        if (dt != NULL)
            *dt = *(tnpr + 3);
            
        if (retselex != NULL)
            *retselex = selexv;
        
        RETURN(txf);
    }
    
    RETURN(NULL);
}

void ed_grab_exvars_for(int inst, HWND hwnd, int p) {
    onstack("ed_grab_exvars_for");
    SendDlgItemMessage(hwnd, p, LB_RESETCONTENT, 0, 0);
    edg.op_hwnd = hwnd;
    edg.op_b_id = p;
    edg.op_winmsg = LB_ADDSTRING;
    lc_parm_int("__ed_fillexvar", 1, inst);
    VOIDRETURN();
}

void ed_grab_ch_exvars_for(int ch, HWND hwnd, int p) {
    onstack("ed_grab_ch_exvars_for");
    SendDlgItemMessage(hwnd, p, LB_RESETCONTENT, 0, 0);
    edg.op_hwnd = hwnd;
    edg.op_b_id = p;
    edg.op_winmsg = LB_ADDSTRING;
    lc_parm_int("__ed_fillchexvar", 1, ch);
    VOIDRETURN();
}

int ed_edit_exvar(HWND owner_hwnd, int inst,
    const char *data_table, const char *exvn, char dt_fast) 
{
    int boolv = 0;
    int cf;
    
    onstack("ed_edit_exvar");
    
    luastacksize(8);
    
    if (dt_fast == 't')
        boolv = 1;
    else if (dt_fast == 'f')
        boolv = 2;
            
    lua_getglobal(LUA, "__serialize");
    lua_pushstring(LUA, " ");
    lua_pushstring(LUA, exvn);
    
    lua_getglobal(LUA, data_table);
    lua_pushinteger(LUA, inst);
    lua_gettable(LUA, -2);
    if (lua_isnil(LUA, -1)) {
        lua_pop(LUA, 1);
        lua_pushinteger(LUA, inst);
        lua_newtable(LUA);
        lua_settable(LUA, -3);
        lua_pushinteger(LUA, inst);
        lua_gettable(LUA, -2);
    }
    lua_pushstring(LUA, exvn);
    
    if (boolv) {
        lua_pushboolean(LUA, boolv - 1);
        lua_settable(LUA, -3);
        lua_pop(LUA, 5);
        RETURN(1);        
    }
    lua_gettable(LUA, -2);
    
    lua_remove(LUA, -2);
    lua_remove(LUA, -2);
    
    if (lua_pcall(LUA, 3, 1, 0) != 0) {
        lua_function_error("serializer", lua_tostring(LUA, -1));
        RETURN(0);
    }
  
    if (lua_isstring(LUA, -1)) {
        const char *serialized = lua_tostring(LUA, -1);
        
        edg.op_inst = inst;
        edg.op_string = serialized;
        edg.exvn_string = exvn;
        edg.data_prefix = data_table;
        DialogBox(GetModuleHandle(NULL),
            MAKEINTRESOURCE(ESB_EXVAREDIT),
            owner_hwnd, ESB_edit_exvar_proc);
        edg.op_string = NULL;
        edg.exvn_string = NULL;
        edg.data_prefix = NULL;
    }
    
    lua_pop(LUA, 1);
    
    cf = edg.completion_flag;
    edg.completion_flag = 0;
    RETURN(cf);
}
