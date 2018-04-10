#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "objects.h"
#include "gamelock.h"
#include "global_data.h"
#include "champions.h"
#include "lua_objects.h"
#include "timer_events.h"
#include "gproto.h"
#include "uproto.h"
#include "arch.h"
#include "render.h"

extern int (*dsbgetpixel)(BITMAP *, int, int);
extern void (*dsbputpixel)(BITMAP *, int, int, int);

static int psxf_yval = 16;

extern FONT *ROOT_SYSTEM_FONT;
extern int ws_tbl_len;
extern struct wallset **ws_tbl;

extern char *Iname[4];

extern struct condition_desc *i_conds;
extern struct condition_desc *p_conds;

extern struct dungeon_level *dun;

extern int debug;
extern int viewstate;

struct inst *oinst[NUM_INST];

extern struct global_data gd;
extern struct graphics_control gfxctl;
extern struct inventory_info gii;
extern BITMAP *scr_pages[3];
extern BITMAP *soft_buffer;
extern volatile unsigned int updateok;

extern lua_State *LUA;
extern FILE *errorlog;

struct clickzone cz[NUM_ZONES];
int lua_cz_n;
struct clickzone *lua_cz;

void set_mouse_override(struct animap *override_image) {
    onstack("set_mouse_override");
    
    if (override_image) {
        lod_use(override_image);
        gd.simg.mouse_override = override_image;
        override_image->ext_references++;
    }
    
    VOIDRETURN();    
}

void clear_mouse_override(void) {
    onstack("clear_mouse_override");
    
    if (gd.simg.mouse_override) {
        gd.simg.mouse_override->ext_references -= 1;
        gd.simg.mouse_override = NULL;
    }
    
    VOIDRETURN();
}

void draw_mouseptr(BITMAP *scx, struct animap *special_ptr, int allow_override) {
    struct animap *mouseptr;
    
    if (allow_override && gd.simg.mouse_override) {
        mouseptr = gd.simg.mouse_override;  
    } else {
        if (special_ptr != NULL)
            mouseptr = special_ptr;
        else
            mouseptr = gd.simg.mouse;
    }
    
    draw_gfxicon(scx, mouseptr, mouse_x, mouse_y, 0, NULL);
}

int draw_wall_writing(int softmode, BITMAP *scx, unsigned int inst,
    int mx, int my)
{
    int i_lines = 0;
    struct obj_arch *arch = Arch(oinst[inst]->arch);
    char *s_writing = exs_exvar(inst, "text");
    char *s_awptr = s_writing;
    char *s_lp[5];
    int ic;
    int i_fh = 24;
    int i_x = 0;
    
    onstack("draw_wall_writing");
    
    if (s_writing == NULL)
        RETURN(0);
    
    while(i_lines < 5) {
        s_lp[i_lines] = s_awptr;
        
        while (*s_awptr != '/' && *s_awptr != '\0')
            ++s_awptr;

        ++i_lines;
        if (*s_awptr == '\0')
            break;

        *s_awptr = '\0';
        ++s_awptr;
    }
    
    // global vertical offset
    my += arch->s_parm1;
    
    for (ic=0;ic<i_lines;++ic) {
        textout_centre_ex(scx, arch->w_font, s_lp[ic], mx,
            my+(ic*i_fh+i_x), 0, -1);
        
        // brick vertical offset (between 2nd and 3rd lines)
        if (ic == 1)
            i_x = arch->s_parm2;
    }
    
    dsbfree(s_writing);
    
    RETURN(1);
}

char *determine_name(unsigned int inst, int flags, int *del) {
    struct inst *p_inst = oinst[inst];
    struct obj_arch *p_arch = Arch(p_inst->arch);
    const char *dname = p_arch->dname;
    char *sdname = (char *)dname;
    
    onstack("determine_name");
    
    if (dname != NULL) {
        if (p_arch->rhack & RHACK_GOT_NAMECHANGER) {
            int who_look = -1;

            if (flags)
                who_look = gd.party[gd.who_look];

            sdname = call_member_func_s(inst, "namechanger", who_look);

            if (sdname == NULL)
                sdname = (char *)dname;
            else
                *del = 1;
        }
    }
    
    RETURN(sdname);
}

void show_name(BITMAP *scx, unsigned int inst, int x, int y, int col, int flags) {
    char *sdname;
    const char *rfname = "sys_render_current_item_text";
    int del_sdname = 0;
    
    onstack("show_name");
    
    sdname = determine_name(inst, flags, &del_sdname);
        
    if (sdname != NULL) {
        if (gfxctl.itemname_drawzone) {
            int idz = gfxctl.itemname_drawzone;
            
            lua_getglobal(LUA, rfname);
            setup_lua_bitmap(LUA, gfxctl.SR[idz].rtarg);
            
            if (sdname && *sdname != '\0')
                lua_pushstring(LUA, sdname);
            else
                lua_pushnil(LUA);   
            lua_pushboolean(LUA, (*gd.gl_viewstate >= VIEWSTATE_INVENTORY));
            lc_call_topstack(3, rfname);
        
            make_cz(74+idz, x, y, gfxctl.SR[idz].sizex, gfxctl.SR[idz].sizey, 0);
            draw_sprite(scx, gfxctl.SR[idz].rtarg->b, x, y);              
            
        } else {
            textout_ex(scx, font, sdname, x, y, col, -1);
        }
        
        if (del_sdname)
            dsbfree(sdname);
    }
        
    VOIDRETURN();
}

void setup_res_rei_cz(void) {
    if (gd.viewmode < 3) {
        int singlezone = ZONE_RES;
        
        if (gd.viewmode == REINCARNATE)
            singlezone = ZONE_REI;
            
        make_cz(singlezone, gd.vxo+gii.srx+6, gd.vyo + gd.vyo_off + gii.sry, 231, 117, 0); 
    } else {
        make_cz(ZONE_RES, gd.vxo+gii.srx+6, gd.vyo + gd.vyo_off + gii.sry, 113, 117, 0);
        make_cz(ZONE_REI, gd.vxo+gii.srx+124, gd.vyo + gd.vyo_off + gii.sry, 113, 117, 0);    
    }
    
    make_cz(ZONE_CANCEL, gd.vxo+gii.srx+6, gd.vyo + gd.vyo_off + gii.sry + 120, 231, 25, 0);  
}

void setup_rei_name_entry_cz(void) {

    make_cz(ZONE_REI, gd.vxo+gii.srx+192, gd.vyo + gd.vyo_off + gii.sry + 122, 41, 21, 0);
}

void getgiidimensions(struct iidesc *ii, int *w, int *h) {
    struct animap *ii_img;

    if (ii->img == NULL) {
        *w = 32;
        *h = 32;
        return;
    }
    
    ii_img = ii->img;
    *w = ii_img->w;
    if (ii_img->numframes > 1) {
        *h = ii_img->c_b[0]->h;
    } else
        *h = ii_img->b->h;
        
    return;
}

void setup_misc_inv(BITMAP *scx, int bx, int by, int pt) {
    int pcidx;
    struct champion *me;
    int viewmode = 0;
    int w, h;
    
    pcidx = gd.party[gd.who_look] - 1;
    me = &(gd.champs[pcidx]);
    
    make_cz(ZONE_MOUTH, bx+gii.mouth.x, by+gii.mouth.y, 35, 35, 0);
    
    if (gii.sleep.img)
        draw_gfxicon(scx, gii.sleep.img, bx+gii.sleep.x, by+gii.sleep.y, 0, 0);
    getgiidimensions(&(gii.sleep), &w, &h);
    make_cz(ZONE_ZZZ, bx+gii.sleep.x, by+gii.sleep.y, w, h, 0);
    
    if (gii.save.img)
        draw_gfxicon(scx, gii.save.img, bx+gii.save.x, by+gii.save.y, 0, 0);
    getgiidimensions(&(gii.save), &w, &h);
    make_cz(ZONE_DISK, bx+gii.save.x, by+gii.save.y, w, h, 0);
    
    if (!gd.infobox && (!gfxctl.subrend || gd.mouse_mode == MM_MOUTHLOOK)) {
        int barlen;
        int fc, cc;
        
        if (pt)
            destroy_system_subrenderers();
        
        if (gfxctl.food_water == NULL) {
            gfxctl.subrend_targ = &(gfxctl.food_water);
            lc_parm_int("sys_render_mainsub", 1, pcidx + 1);
            gfxctl.subrend_targ = NULL;
        }
        
        if (gfxctl.food_water != NULL)
            draw_sprite(scx, gfxctl.food_water->b, bx+gii.srx, by+gii.sry);
        
        // draw conditions like poisoned
        for(cc=0;cc<gd.num_conds;++cc) {
            if (me->condition[cc] && i_conds[cc].bmp) {
                struct inst fake_obj;
                struct inst *p_fake_obj = NULL;
                
                int rx = gii.srx+6;
                int ry = gii.sry;
                
                if (me->cond_animtimer[cc] != FRAMECOUNTER_NORUN) {
                    memset(&fake_obj, 0, sizeof(struct inst));
                    fake_obj.frame = me->cond_animtimer[cc];
                    
                    p_fake_obj = &fake_obj;
                }
                
                draw_gfxicon(scx, i_conds[cc].bmp, bx+rx, by+ry, 0, p_fake_obj);
            }    
        }
    }   
}

void draw_inv_statsbox(BITMAP *scx, int px, int py, int pt) {
    int rchamp = gd.party[gd.who_look];
    
    onstack("draw_inv_statsbox");
    
    destroy_system_subrenderers();
    
    if (gfxctl.statspanel == NULL) {
        gfxctl.subrend_targ = &(gfxctl.statspanel);
        lc_parm_int("sys_render_stats", 1, rchamp);
        gfxctl.subrend_targ = NULL;
    }

    if (gfxctl.statspanel != NULL) {
        masked_blit(gfxctl.statspanel->b, scx, 0, 0, px, py,
            gfxctl.statspanel->b->w, gfxctl.statspanel->b->h);
    }

    gd.infobox = 1;
   
    VOIDRETURN();  
}

void draw_obj_infobox(BITMAP *scx, int px, int py, int pt) {
    struct inst *p_inst;
    struct obj_arch *p_arch;
    const char *RENDERER = "sys_render_object";
    struct animap *an;
    
    p_inst = oinst[gd.mouseobj];
    p_arch = Arch(p_inst->arch);
    
    if ((p_inst->gfxflags & G_ALT_ICON) && p_arch->alt_icon)
        an = p_arch->alt_icon;
    else
        an = p_arch->icon;
    
    if (pt || an->numframes > 1)
        destroy_system_subrenderers();
        
    if (gfxctl.objlook == NULL) {
        gfxctl.subrend_targ = &(gfxctl.objlook);
        lua_getglobal(LUA, RENDERER);
        if (!lua_isnil(LUA, -1)) {
            char *s_str = NULL;
            int del = 0;
            
            s_str = determine_name(gd.mouseobj, 1, &del);
            
            lua_pushinteger(LUA, gd.party[gd.who_look]);
            lua_pushinteger(LUA, gd.mouseobj);
            lua_pushstring(LUA, s_str);
            lc_call_topstack(3, RENDERER);
            lstacktop();
            
            if (del)
                dsbfree(s_str);
        }
        gfxctl.subrend_targ = NULL;
    }

    if (gfxctl.objlook != NULL)
        draw_sprite(scx, gfxctl.objlook->b, px, py);
    
    gd.infobox = 1;
    
    return;
}

BITMAP *make_guy_icon(int id) {
    int bgc;
    BITMAP *pl_bitmap;
    BITMAP *s_icon;
    BITMAP *uguy;
    int alpha = 0;
    
    onstack("make_guy_icon");
    
    if (gd.playerbitmap[id]) {
        lod_use(gd.playerbitmap[id]);
        pl_bitmap = animap_subframe(gd.playerbitmap[id], 0);
    } else {
        bgc = gd.playercol[id];
        pl_bitmap = NULL;
    }
    
    if (gd.simg.guy_icons) {
        uguy = animap_subframe(gd.simg.guy_icons, 0);
        alpha = 1;
    } else {
        uguy = gd.simg.guy_icons_8;
    }
    s_icon = create_bitmap_ex(32, uguy->w, uguy->h);
    
    if (pl_bitmap) {
        int w38 = pl_bitmap->w - 10;
        int x;
        for (x=0;x<4;++x) {
            blit(pl_bitmap, s_icon, 10, 22, x*w38, 0, w38, s_icon->h);
        }
    } else {
        clear_to_color(s_icon, bgc);
    }
    
    if (alpha) {
        copy_alpha_channel(uguy, s_icon);
    } else {
        select_palette(gd.simg.guy_palette);
        draw_sprite(s_icon, uguy, 0, 0);
    }
        
    RETURN(s_icon);
}

void draw_guy_icons(BITMAP *scx, int px, int py, int force_draw) {
    int xp, yp;
    int ap = gd.a_party;
    struct animap *a_overlay = gd.simg.guy_icons_overlay;
    
    for (yp=0;yp<2;yp++) {
        for(xp=0;xp<2;xp++) {
            int ip = gd.guypos[yp][xp][ap];
            
            if (force_draw || (ip && gd.mouse_guy != ip)) {
                BITMAP *my_icon;
                int idir;
                int xl, yl;
                
                if (force_draw) {
                    ip = force_draw;
                    xl = px;
                    yl = py;
                } else {
                    xl = px+40*xp;
                    yl = py+30*yp;
                } 
                
                --ip;    
                my_icon = gd.simg.guy[ip];
                if (my_icon == NULL)
                    my_icon = gd.simg.guy[ip] = make_guy_icon(ip);
                    
                idir = gd.g_facing[ip];
                if (a_overlay) {
                    BITMAP *b_overlay = animap_subframe(a_overlay, 0);
                    BITMAP *ybmp = create_sub_bitmap(my_icon, idir*38, 0, 38, 28);
                    set_alpha_blender();
                    draw_trans_sprite(scx, ybmp, xl, yl);
                    destroy_bitmap(ybmp);
                    ybmp = create_sub_bitmap(b_overlay, idir*38, 0, 38, 28);
                    set_alpha_blender();
                    draw_trans_sprite(scx, ybmp, xl, yl);
                    destroy_bitmap(ybmp);                 
                } else {
                    if (!force_draw && (gd.gameplay_flags & GP_PARTYINVIS)) {
                        BITMAP *sbmp;
                        
                        sbmp = create_sub_bitmap(my_icon, idir*38, 0, 38, 28);
                        set_trans_blender(0, 0, 0, 255);
                        draw_lit_sprite(scx, sbmp, xl, yl, 127);
                        destroy_bitmap(sbmp);
                    } else {
                        masked_blit(my_icon, scx, idir*38, 0, xl, yl, 38, 28);
                    }
                }
                
                if (force_draw)
                    return;
            }
            
        }
    }
    return;    
}

void draw_spell_interface(BITMAP *scx, int i_index, int vs) {
    int px, py;
    int frozenstate = (vs >= VIEWSTATE_SLEEPING);
    int forbiddenstate = 0;
    const char *rfname = "sys_render_magic";
    
    onstack("draw_spell_interface");
    
    if (!(gfxctl.SR[i_index].flags & SRF_ACTIVE)) {
        VOIDRETURN();
    }
    
    px = gfxctl.SR[i_index].rtx;
    py = gfxctl.SR[i_index].rty;
    
    cclear_m_cz(gfxctl.SR[i_index].cz, gfxctl.SR[i_index].cz_n);
    
    if (gd.leader == -1) {
        VOIDRETURN();
    }
        
    if (forbid_magic(gd.whose_spell, gd.party[gd.whose_spell]))
        forbiddenstate = 1;
        
    lua_getglobal(LUA, rfname);
    setup_lua_bitmap(LUA, gfxctl.SR[i_index].rtarg);
    lua_pushinteger(LUA, gd.whose_spell);
    lua_pushboolean(LUA, frozenstate);
    lua_pushboolean(LUA, forbiddenstate);
    lc_call_topstack(4, rfname);
    
    draw_sprite(scx, gfxctl.SR[i_index].rtarg->b, px, py);
    
    if (!gd.gl->lockc[LOCK_MAGIC]) {
        make_cz(74+i_index, px, py,
            gfxctl.SR[i_index].sizex, gfxctl.SR[i_index].sizey, 0);
    }
    
    VOIDRETURN();
}

void call_lua_inventory_renderer(BITMAP *scx, int lwho, int bx, int by, int namedraw) {
    BITMAP *scx_sub;
    const char *rfname = "sys_render_inventory_text";
    struct animap screen_animap;
    
    onstack("call_lua_inventory_renderer");

    luastacksize(5);
    lua_getglobal(LUA, rfname);
    
    scx_sub = create_sub_bitmap(scx, bx, by, VIEWPORTW, VIEWPORTH);
    memset(&screen_animap, 0, sizeof(struct animap));
    screen_animap.b = scx_sub;
    screen_animap.numframes = 1;
    screen_animap.w = scx_sub->w;
    screen_animap.flags = AM_VIRTUAL;
    setup_lua_bitmap(LUA, &screen_animap);
    
    lua_pushinteger(LUA, lwho);
    lua_pushinteger(LUA, namedraw+1);
    lc_call_topstack(3, rfname);    
    destroy_bitmap(scx_sub);
    
    VOIDRETURN();
}

void call_lua_top_renderer(struct animap *rtarg, int cstate, int ppos, int lwho, int here,
    const char *port_name, int *p_x, int *p_y) 
{
    const char *rfname = "sys_render_portraits_and_info";  
    
    onstack("call_lua_top_renderer");
    
    luastacksize(9);
    lua_getglobal(LUA, rfname);
    setup_lua_bitmap(LUA, rtarg);
    lua_pushinteger(LUA, ppos);
    lua_pushinteger(LUA, lwho);
    lua_pushboolean(LUA, (cstate == TOP_DEAD));
    lua_pushboolean(LUA, (cstate == TOP_PORT));
    lua_pushinteger(LUA, leader_color_type(ppos, here) + 1);
    if (port_name)  
        lua_pushstring(LUA, port_name);
    else
        lua_pushnil(LUA);
        
    lc_call_topstack_two_results(7, rfname, p_x, p_y);    
    
    VOIDRETURN();  
}

void call_lua_damage_renderer(struct animap *rtarg, int cstate, int ppos, int lwho) {
    const char *rfname = "sys_render_character_damage";  
    
    onstack("call_lua_damage_renderer");
    
    luastacksize(9);
    lua_getglobal(LUA, rfname);
    setup_lua_bitmap(LUA, rtarg);
    lua_pushinteger(LUA, ppos);
    lua_pushinteger(LUA, lwho);
    lua_pushboolean(LUA, (cstate == TOP_DEAD));
    lua_pushboolean(LUA, (cstate == TOP_PORT));
    lua_pushinteger(LUA, gd.showdam_t[ppos]);
    lua_pushinteger(LUA, gd.showdam[ppos]);
        
    lc_call_topstack(7, rfname);    
    
    VOIDRETURN();       
}

int find_targeted_csr(struct animap *sbmp, int xx, int yy, struct clickzone ***p_lcz, int **p_lczn, int *p_cx, int *p_cy) {
    int srn;

    for(srn=0;srn<NUM_SR+4;++srn) {
        sys_rend *csr;
        
        if (srn >= NUM_SR)
            csr = &(gfxctl.ppos_r[srn-NUM_SR]);
        else
            csr = &(gfxctl.SR[srn]);
        
        if ((csr->flags & SRF_ACTIVE) && sbmp == csr->rtarg) {
            *p_lcz = &(csr->cz);
            *p_lczn = &(csr->cz_n);
            *p_cx = xx + csr->rtx;
            *p_cy = yy + csr->rty;
            return (srn+1);
        }   
    }
    return 0;
}

int find_valid_czn(struct clickzone *p_lcz, int lczn ) {
    int zn = 0;
    while (zn < lczn && p_lcz[zn].w)
        zn++;
    
    return zn;
}

int find_associated_csr_ppos(struct animap *sbmp, int csr, int csr_found) {
    int ppos = 0;
    
    if (!csr_found)
        ppos = gd.who_look;
    else if (csr == SR_MAGIC)
        ppos = gd.whose_spell;
    else if (csr == SR_METHODS)
        ppos = gd.who_method - 1;
    else {
        for(ppos=0;ppos<4;++ppos) {
            if (gfxctl.ppos_r[ppos].rtarg == sbmp)
                break;
        }             
        if (ppos > 3)
            ppos = gd.leader;
    }
    
    return ppos;
}

void put_into_pos(int ap, int i) {
    int xp, yp;
    
    onstack("put_into_pos");
    
    for (yp=0;yp<2;yp++) {
        for (xp=0;xp<2;xp++) {
            if (!gd.guypos[yp][xp][ap]) {
                gd.guypos[yp][xp][ap] = i;
                VOIDRETURN();
            }
        }
    }
    
    recover_error("Added member to full party");
    VOIDRETURN();    
}

void remove_from_pos(int ap, int i) {
    int xp, yp;
    
    onstack("remove_from_pos");
    
    for (yp=0;yp<2;yp++) {
        for (xp=0;xp<2;xp++) {
            
            if (gd.guypos[yp][xp][ap] == i) {
                gd.guypos[yp][xp][ap] = 0;
                VOIDRETURN();
            }
        }
    } 
    
    recover_error("Removed nonexistent");
    VOIDRETURN();   
}

int remove_from_all_pos(int i) {
    int xp, yp, ap;

    onstack("remove_from_all_pos");

    for (ap=0;ap<4;ap++) {
        for (yp=0;yp<2;yp++) {
            for (xp=0;xp<2;xp++) {
                if (gd.guypos[yp][xp][ap] == i) {
                    gd.guypos[yp][xp][ap] = 0;
                    RETURN(ap);
                }
            }
        }
    }

    recover_error("Removed nonexistent (from all)");
    RETURN(0);
}

int find_remove_from_pos_ec(int ap, int i, int *rx, int *ry) {
    int xp, yp;
    
    onstack("find_remove_from_pos_ec");
    
    for (yp=0;yp<2;yp++) {
        for (xp=0;xp<2;xp++) {
            
            if (gd.guypos[yp][xp][ap] == i) {
                gd.guypos[yp][xp][ap] = 0;
                *rx = xp;
                *ry = yp;
                RETURN(1);
            }
        }
    } 
    
    RETURN(0);
}   

void find_remove_from_pos(int ap, int i, int *rx, int *ry) {
    onstack("find_remove_from_pos");
    if (find_remove_from_pos_ec(ap, i, rx, ry)) {
        VOIDRETURN();
    }
    recover_error("Find_removed nonexistent");
    VOIDRETURN();   
}

int find_remove_from_pos_tolerant(int ap, int i, int *rx, int *ry) {
    int ecv;
    
    onstack("find_remove_from_pos_tolerant");
    
    ecv = find_remove_from_pos_ec(ap, i, rx, ry);
    RETURN(ecv);
}

void draw_movement_arrows(BITMAP *scx, int i_index, int vs) {
    int px, py;
    const char *rfname = "sys_render_arrows";
    
    onstack("draw_movement_arrows");
    
    if (!(gfxctl.SR[i_index].flags & SRF_ACTIVE)) {
        VOIDRETURN();
    }
    
    px = gfxctl.SR[i_index].rtx;
    py = gfxctl.SR[i_index].rty;
    
    cclear_m_cz(gfxctl.SR[i_index].cz, gfxctl.SR[i_index].cz_n);
    
    lua_getglobal(LUA, rfname);
    setup_lua_bitmap(LUA, gfxctl.SR[i_index].rtarg);
    if (gd.litarrow)
        lua_pushinteger(LUA, gd.litarrow);
    else
        lua_pushnil(LUA);
    lua_pushboolean(LUA, (vs >= VIEWSTATE_INVENTORY));
    lc_call_topstack(3, rfname);
    
    if (!gd.gl->lockc[LOCK_MOVEMENT]) {
        make_cz(74+i_index, px, py,
            gfxctl.SR[i_index].sizex, gfxctl.SR[i_index].sizey, 0);
    }
    
    draw_sprite(scx, gfxctl.SR[i_index].rtarg->b, px, py);
    
    VOIDRETURN();
}

void draw_user_bitmaps(BITMAP *scx, int i_start, int vs) {
    int i_index;
    const char *rfname = "sys_render_other";
    
    onstack("draw_user_bitmaps");
    
    for(i_index=i_start;i_index<NUM_SR;++i_index) {
        int px, py;
        
        if (!(gfxctl.SR[i_index].flags & SRF_ACTIVE)) {
            continue;
        } 
        
        // HANDLED means its draw function is called elsewhere
        if (gfxctl.SR[i_index].flags & SRF_HANDLED) {
            continue;
        } 
        
        px = gfxctl.SR[i_index].rtx;
        py = gfxctl.SR[i_index].rty;
        
        lua_getglobal(LUA, rfname);
        setup_lua_bitmap(LUA, gfxctl.SR[i_index].rtarg);
        lua_pushstring(LUA, gfxctl.SR[i_index].renderer_name);
        lua_pushboolean(LUA, (vs >= VIEWSTATE_INVENTORY));
        lc_call_topstack(3, rfname);
        
        make_cz(74+i_index, px, py,
            gfxctl.SR[i_index].sizex, gfxctl.SR[i_index].sizey, 0);
        
        draw_sprite(scx, gfxctl.SR[i_index].rtarg->b, px, py);        
    }

    VOIDRETURN();    
}

int draw_interface(int softmode, int interface_vs, int really) {
    BITMAP *scx;
    int i;
    int x_o, y_o;
    int la = 0;
    int siz = 58;
    int arrow_draw = 0;
    int rv = 0;
    int rtx = 0;
    int rty = 0;
    
    onstack("draw_interface");

    scx = find_rendtarg(softmode);

    for (i=56;i<74+NUM_SR;++i) {
        cz[i].h = 0;
        cz[i].w = 0;
    }

    cclear_m_cz(gfxctl.SR[SR_METHODS].cz, gfxctl.SR[SR_METHODS].cz_n);
    
    acquire_bitmap(scx);
     
    if (gd.attack_msg || gd.attack_msg_ttl) {
        const char *rfname = "sys_render_attack_result";
        
        luastacksize(6);

        lua_getglobal(LUA, rfname);
        setup_lua_bitmap(LUA, gfxctl.SR[SR_METHODS].rtarg);
        lua_pushboolean(LUA, (interface_vs >= VIEWSTATE_SLEEPING));
        if (gd.attack_msg) {
            lua_pushstring(LUA, gd.attack_msg);
        } else {
            lua_pushinteger(LUA, gd.attack_dmg_msg);
        }        
        lua_pushboolean(LUA, !(gd.attack_msg));
        
        if (gfxctl.SR[SR_METHODS].flags & SRF_ACTIVE) {
            lc_call_topstack(4, rfname);
        } else {
            lua_pop(LUA, 5);
        }
        
        goto SKIP_MAIN_RENDER_AMETHOD;    
    
    } else if (gd.attack_msg_ttl) {
        /*
        int dval = gd.attack_dmg_msg;
        int dscale = dval + 18;
        if (dval >= 20) dscale += 4;
        if (dval < 90 && dscale > 90) dscale = 90;
        if (dscale < 40) dscale = 40;
        
        if (dscale >= 100)
            draw_gfxicon(scx, gd.simg.damage, 456, 172, 0, NULL);
        else {
            BITMAP *damage_bmp;
            int hscale = (dscale*3)/2;
            int xo, yo;
            int nw, nh;
            BITMAP *scal_damage;
            struct animap scal_damage_animap;
            
            release_bitmap(scx);
            
            damage_bmp = animap_subframe(gd.simg.damage, 0);
            
            if (hscale > 100) hscale = 100;
            nw = (damage_bmp->w*dscale)/100;
            nh = (damage_bmp->h*hscale)/100;
            
            scal_damage = create_bitmap(nw, nh);
            stretch_blit(damage_bmp, scal_damage, 0, 0, damage_bmp->w,
                damage_bmp->h, 0, 0, nw, nh);
            
            xo = (damage_bmp->w - nw)/2;
            yo = (damage_bmp->h - nh)/2;
            
            memset(&scal_damage_animap, 0, sizeof(struct animap));
            scal_damage_animap.numframes = 1;
            scal_damage_animap.b = scal_damage;
            scal_damage_animap.w = scal_damage->w;
            scal_damage_animap.flags = gd.simg.damage->flags;

            acquire_bitmap(scx);
            draw_gfxicon(scx, &scal_damage_animap, 456+xo, 172+yo, 0, NULL);
            destroy_bitmap(scal_damage);    
        }
        
        textprintf_centre_ex(scx, font, 548, 212, gd.sys_col,
            -1, "%d", gd.attack_dmg_msg);
        */
            
        //goto SKIP_DRAW_AMETHOD;    
    }
    
    if (gd.who_method) {
        int wm = gd.who_method;
        const char *rfname = "sys_render_attack_method";
        int nm = gd.num_methods;
        
        luastacksize(8);

        lua_getglobal(LUA, rfname);
        setup_lua_bitmap(LUA, gfxctl.SR[SR_METHODS].rtarg);
        lua_pushboolean(LUA, (interface_vs >= VIEWSTATE_SLEEPING));
        lua_pushinteger(LUA, wm - 1);
        
        if (gd.method_obj) {
            lua_pushinteger(LUA, gd.method_loc);
            lua_pushinteger(LUA, gd.method_obj);
        } else {
            lua_pushnil(LUA);
            lua_pushnil(LUA);
        }
        
        lua_pushinteger(LUA, nm);
        
        lua_newtable(LUA);
        for (i=0;i<nm;++i) {
            lua_pushinteger(LUA, i+1);
            lua_pushstring(LUA, gd.c_method[i]->name);
            lua_settable(LUA, -3);
        }
        
        if (gfxctl.SR[SR_METHODS].flags & SRF_ACTIVE) {
            lc_call_topstack(7, rfname);
        } else {
            lua_pop(LUA, 8);
        }
        
    } else {
        const char *rfname = "sys_render_attack_options";
        
        luastacksize(4);
        
        lua_getglobal(LUA, rfname);
        setup_lua_bitmap(LUA, gfxctl.SR[SR_METHODS].rtarg);
        lua_pushboolean(LUA, (interface_vs >= VIEWSTATE_SLEEPING));
        
        if (gfxctl.SR[SR_METHODS].flags & SRF_ACTIVE) {
            lc_call_topstack(2, rfname);
        } else {
            lua_pop(LUA, 3);
        }
    }

    SKIP_MAIN_RENDER_AMETHOD:     
    if (gfxctl.SR[SR_METHODS].flags & SRF_ACTIVE) {    
        rtx = gfxctl.SR[SR_METHODS].rtx;
        rty = gfxctl.SR[SR_METHODS].rty;    
        draw_sprite(scx, gfxctl.SR[SR_METHODS].rtarg->b, rtx, rty);
    
        if (!gd.gl->lockc[LOCK_ATTACK]) {
            make_cz(74, rtx, rty,
                gfxctl.SR[SR_METHODS].sizex, gfxctl.SR[SR_METHODS].sizey, 0);
        }
    }

    if (gd.mouseobj) {
        show_name(scx, gd.mouseobj, gfxctl.curobj_x, gfxctl.curobj_y, gd.sys_col, 0);
    }
    
    draw_movement_arrows(scx, SR_MOVE, interface_vs);
    draw_spell_interface(scx, SR_MAGIC, interface_vs);
    draw_user_bitmaps(scx, SR_USER1, interface_vs);
    
    render_console(scx);
    
    draw_guy_icons(scx, gfxctl.guy_x + 2, gfxctl.guy_y, 0);

    if (debug) {
        release_bitmap(scx);
        draw_clickzones(softmode);
        acquire_bitmap(scx);
    }
    
    if (!really) {
        RETURN(rv);
    }
    
    if (gd.mouse_guy) {
        draw_guy_icons(scx, mouse_x-18, mouse_y-14, gd.mouse_guy);         
        release_bitmap(scx);
        RETURN(rv);     
    }
    
    if (gd.mouse_mode || gd.mouse_hidden) {
        release_bitmap(scx);
        RETURN(rv);
    }
    
    if (gd.leader == -1 && !gd.mouseobj)
        arrow_draw = -1;
    else if (interface_vs >= VIEWSTATE_SLEEPING &&
        interface_vs != VIEWSTATE_CHAMPION)
    {
        arrow_draw = 1;
    } else {
        int PORTW = 138*4;
        int PORTH = 64;
        int arrow_zone = 1;
        
        // viewport is not an arrow zone
        if ((mouse_x >= (gd.vxo - 2)) && (mouse_y >= (gd.vyo - 2))) {
            if ((mouse_x < (gd.vxo + VIEWPORTW + 2)) && (mouse_y < (gd.vyo + VIEWPORTH + 2))) {
                arrow_zone = 0;
            }
        }
        
        // titlebars are not an arrow zone
        if ((mouse_x >= gfxctl.port_x) && (mouse_y >= gfxctl.port_y)) {
            if ((mouse_x < gfxctl.port_x + PORTW) && (mouse_y < gfxctl.port_y + PORTH)) {
                arrow_zone = 0;
            }
        }
        
        if (arrow_zone)
            arrow_draw = 5;
    }
    
    if (arrow_draw) {
        draw_mouseptr(scx, NULL, 1);
    } else {    
        if (gd.mouseobj)
            rv = draw_objicon(scx, gd.mouseobj, mouse_x-16, mouse_y-16, 1);
        else {
            struct animap *b_h;
            struct champion *ld = &(gd.champs[gd.party[gd.leader]-1]);

            if (ld->hand) {
                b_h = ld->hand;
            } else
                b_h = gd.simg.mousehand;

            draw_mouseptr(scx, b_h, 1);
        }
    }
    
    release_bitmap(scx);
    RETURN(rv);
}

void draw_bordered_box(BITMAP *scx, int xt, int yt, int border) {
    if (border == 1) {
        draw_gfxicon(scx, gii.hurt_box, xt, yt, 0, NULL);
    } else if (border == 2) {
        draw_gfxicon(scx, gii.boost_box, xt, yt, 0, NULL);
    }
}

void draw_eye_border(BITMAP *scx, struct champion *w, int xt, int yt) {
    int border = 0;
    int sc;
    
    for(sc=0;sc<6;++sc) {
        if (w->stat[sc]/10 < w->maxstat[sc]/10) {
            border = 1;
            break;
        } else if ((w->stat[sc]/10 > w->maxstat[sc]/10) && !border) {
            border = 2; 
        }  
    }
    
    if (!border) {
        int cc;
        
        for(cc=0;cc<gd.num_conds;++cc) {
            if (w->condition[cc]) {
                if (i_conds[cc].flags & COND_EYE_RED) {
                    border = 1;
                    break;
                } else if (i_conds[cc].flags & COND_EYE_GREEN) {
                    border = 2;
                }
            }
        }
    }
    
    draw_bordered_box(scx, xt, yt, border);
}

void draw_mouth_border(BITMAP *scx, struct champion *w, int xt, int yt) {
    int draw = 0;
    int cc;
    
    if (w->food < 512 || w->water < 512)
        draw = 1;
    else {
        for(cc=0;cc<gd.num_conds;++cc) {
            if (w->condition[cc]) {
                if (i_conds[cc].flags & COND_MOUTH_RED) {
                    draw = 1;
                    break;   
                } else if (i_conds[cc].flags & COND_MOUTH_GREEN) {
                    draw = 2;
                }
            }    
        }
    }
    
    draw_bordered_box(scx, xt, yt, draw);
}

void draw_save_name(BITMAP *scx, int x, int y, int id, int zone) {
    FONT *font = ROOT_SYSTEM_FONT;
    int tcl;
    char *tx;
    
    if (Iname[id]) {
        tcl = makecol(255, 255, 255);
        tx = Iname[id];
    } else {
        tcl = makecol(192, 192, 192);
        if (gd.exestate == STATE_RESUME)
            tx = "INVALID GAME";
        else
            tx = "NEW SAVEGAME";
    }
    
    textout_ex(scx, font, tx, x, y, tcl, -1);
    
    if (zone != -1 && (id == zone)) {
        if ((gd.gui_framecounter / 8) % 2 == 0) {
            int cc;
            cc = makecol(255,255,0);
            textout_ex(scx, font, "_", x + text_length(font, tx), y, cc, -1);
        }
    }
}

void draw_file_names_r(BITMAP *scx, int bx, int by, int zone) {
    draw_save_name(scx, bx+46, by+146, 0, zone);
    draw_save_name(scx, bx+260, by+146, 1, zone);
    draw_save_name(scx, bx+46, by+220, 2, zone);
    draw_save_name(scx, bx+260, by+220, 3, zone);
}

void draw_file_names_csave(BITMAP *scx, int bx, int by) {
    draw_file_names_r(scx, bx, by, gd.active_szone);
}

void draw_file_names(BITMAP *scx, int bx, int by) {
    FONT *font = ROOT_SYSTEM_FONT;
    int vmod = 0;

    draw_file_names_r(scx, bx, by, -1);
    
    if (gd.exestate == STATE_RESUME) {
        vmod = 10;
    }
    
    textout_ex(scx, font, "SELECT GAME FILE", bx+130, by+40+vmod,
        makecol(255,255,255), -1);
        
    if (!vmod) {
        textout_ex(scx, font, "RIGHT CLICK TO CANCEL", 104, by+70,
            makecol(255,255,255), -1);
    }
}

void draw_gui_controls(BITMAP *scx, int bx, int by, int modesel) {
    BITMAP *control_bkg;
    BITMAP *control_short;
    BITMAP *control_long;
    int lv[6] = { 0, 0, 0, 0, 0, 0 };

    onstack("draw_gui_controls");
    
    control_bkg = animap_subframe(gd.simg.control_bkg, 0);
    control_long = animap_subframe(gd.simg.control_long, 0);
    control_short = animap_subframe(gd.simg.control_short, 0); 

    blit(control_bkg, scx, 0, 0, bx, by, control_bkg->w, control_bkg->h);
        
    set_trans_blender(0, 0, 0, 255);
    lv[gd.gui_down_button] = 32;

    if (modesel & 1) {
        draw_lit_sprite(scx, control_long, bx+20, by+124, lv[1]);
        make_cz(1, bx+20, by+124, control_long->w, control_long->h, 0);
    } else if (modesel & 4) {
        blit(control_bkg, scx, 0, 24, bx+0, by+100, control_bkg->w, 80);
    } else {
        draw_lit_sprite(scx, control_short, bx+20, by+124, lv[2]);
        make_cz(2, bx+20, by+124, control_short->w, control_short->h, 0);
        draw_lit_sprite(scx, control_short, bx+232, by+124, lv[3]);
        make_cz(3, bx+232, by+124, control_short->w, control_short->h, 0);
    }

    if (modesel & 8) {
        draw_lit_sprite(scx, control_long, bx+20, by+198, lv[4]);
        make_cz(4, bx+20, by+198, control_long->w, control_long->h, 0);
    } else {
        draw_lit_sprite(scx, control_short, bx+20, by+198, lv[4]);
        make_cz(4, bx+20, by+198, control_short->w, control_short->h, 0);
        draw_lit_sprite(scx,    control_short, bx+232, by+198, lv[5]);
        make_cz(5, bx+232, by+198, control_short->w, control_short->h, 0);
    }

#ifdef SUB_VERSION
    textprintf_ex(scx, ROOT_SYSTEM_FONT, bx+384-24, by+6, makecol(145,145,145),
        -1, "V%d%s%d%c", MAJOR_VERSION, ".", MINOR_VERSION, SUB_VERSION);
#else
    textprintf_ex(scx, ROOT_SYSTEM_FONT, bx+384-12, by+6, makecol(145,145,145),
        -1, "V%d%s%d", MAJOR_VERSION, ".", MINOR_VERSION);
#endif

    VOIDRETURN();
}

void do_gui_options(int softmode) {
    FONT *font = ROOT_SYSTEM_FONT;
    int bx, by;
    int tby;
    BITMAP *scx;
    
    onstack("do_gui_options");
    
    scx = find_rendtarg(softmode);
    
    setup_background(scx, 1);
    
    bx = gd.vxo;
    by = gd.vyo + gd.vyo_off;
    // hack to avoid having to change lots of coords
    tby = by - 86;

    draw_champions(softmode, scx, gd.party[gd.who_look]);
    memset(cz, 0, sizeof(cz));
    
    draw_gui_controls(scx, bx, by, (gd.gui_mode & 1));

    if (gd.gui_mode == GUI_WANT_SAVE) {
        textout_ex(scx, font, "CANCEL", bx+188, tby+232,
            makecol(255,255,255), -1);
        textout_ex(scx, font, "SAVE GAME", bx+64, tby+306,
            makecol(255,255,255), -1);
        textout_ex(scx, font, "QUIT GAME", bx+278, tby+306,
            makecol(255,255,255), -1);
            
        textout_ex(scx, font, "READY TO SAVE GAME", bx+118, tby+136,
            makecol(255,255,255), -1);
            
    } else if (gd.gui_mode == GUI_PICK_FILE) {
        draw_file_names(scx, bx, by);
        
    } else if (gd.gui_mode == GUI_ENTER_SAVENAME) {
        draw_file_names_csave(scx, bx, by);
        
        textout_ex(scx, font, "TYPE THE NAME TO USE", bx+104, tby+126,
            makecol(255,255,255), -1);

        textout_ex(scx, font, "AND THEN PRESS ENTER", bx+104, tby+126+15,
            makecol(255,255,255), -1);
            
        textout_ex(scx, font, "RIGHT CLICK TO CANCEL", bx+104, tby+156,
            makecol(255,255,255), -1);
        
    }
        
    VOIDRETURN();
}

void do_gui_update(void) {
    if (gd.gui_button_wait) {
        --gd.gui_button_wait;
        if (gd.gui_button_wait == 0) {
            int nm = gd.gui_next_mode;
            if (nm == 0) {
                *gd.gl_viewstate = VIEWSTATE_INVENTORY;
                gd.gui_mode = 0;
                unfreeze_sound_channels();
            } else if (nm == 255) {
                gd.stored_rcv = 100;
                unfreeze_sound_channels();
            } else {
                gd.gui_mode = nm;
            }
            
            gd.gui_next_mode = 0;
            gd.gui_down_button = 0;
        }
    }
}

void draw_name_entry(BITMAP *scx) {
    FONT *font = ROOT_SYSTEM_FONT;
    int who_look = gd.who_look;
    struct champion *me = &(gd.champs[gd.party[who_look] - 1]);
    int bx = gd.vxo;
    int by = gd.vyo + gd.vyo_off;
    int cl = makecol(182, 182, 182);
    int i;
    
    for(i=0;i<7;++i) {
        if (me->name[i] == '\0') {
            int icl = cl;
            if (gd.curs_pos < 7 && gd.curs_pos == i)
                icl = makecol(255, 255, 0);
                
            textout_ex(scx, font, "_", bx + gii.srx + 154 + i*12, by + gii.sry + 4, icl, -1);
        }
    }
    
    for (i=0;i<19;++i) {
        if (me->lastname[i] == '\0') {
            int icl = cl;
            
            if (gd.curs_pos >= 7 && (gd.curs_pos - 7) == i)
                icl = makecol(255, 255, 0);
            
            textout_ex(scx, font, "_", bx + gii.srx + 10 + i*12, by + gii.sry + 40, icl, -1);
        }
    }
    
    textout_ex(scx, font, me->name, bx + gii.srx + 154, by + gii.sry + 4, cl, -1);
    textout_ex(scx, font, me->lastname, bx + gii.srx + 10, by + gii.sry + 40, cl, -1);
}

int check_subgfx_state(int vs) {
    int cc;
    int tlight;
    struct dungeon_level *dd;
    
    if (vs != VIEWSTATE_DUNGEON)
        return 1;
    
    if (!gd.trip_buffer)
        return 1;
        
    if (gd.distort_func > 0)
        return 1;
        
    // alpha dudes
    if (gd.simg.guy_icons)
        return 1;

    // give post-checks a chance to work
    if (gd.softframecnt > 0) {
        --gd.softframecnt;
        return 1;
    }
        
    dd = &(dun[gd.p_lev[gd.a_party]]);
    
    // check lightlevel
    tlight = calc_total_light(dd);
    if (tlight < 90)
        return 1;
    
    // check overlay graphics
    for(cc=0;cc<gd.num_pconds;++cc) {
        if (gd.partycond[cc] && p_conds[cc].pbmp) {
            if (p_conds[cc].pbmp->flags & AM_HASALPHA)
                return 1;
        }
    }
    
    return 0;   
}

int initialize_wallset(struct wallset *newset) {

    onstack("initialize_wallset");
    
    newset->flipflags = 0xFF;
    
    ws_tbl_len++;
    ws_tbl = dsbrealloc(ws_tbl, sizeof(struct wallset*) * ws_tbl_len);
    ws_tbl[(ws_tbl_len-1)] = newset;

    lua_newtable(LUA);
    lua_pushstring(LUA, "type");
    lua_pushinteger(LUA, DSB_LUA_WALLSET);
    lua_settable(LUA, -3);
    lua_pushstring(LUA, "ref_int");
    lua_pushinteger(LUA, (ws_tbl_len-1));
    lua_settable(LUA, -3);
    lua_pushstring(LUA, "ref");
    lua_pushlightuserdata(LUA, (void*)(ws_tbl_len-1));
    lua_settable(LUA, -3);
    
    RETURN((ws_tbl_len-1));
}

void psxf(const char *upd) {
    
    onstack("startup_psxf");
    
    textout_ex(screen, ROOT_SYSTEM_FONT, upd, 8, psxf_yval, makecol(255, 255, 255), -1);
    if (debug)
        fprintf(errorlog, "@@@ STARTUP: %s\n", upd);
    psxf_yval += 14;

    VOIDRETURN();
}

void define_alt_targ(int lev, int x, int y, int dir, int tlight) {
    onstack("define_alt_targ");

    if (gfxctl.l_alt_targ)
        destroy_alt_targ();
        
    gfxctl.l_alt_targ = dsbmalloc(sizeof(struct alt_targ));

    gfxctl.l_alt_targ->targ = gfxctl.l_viewport_rtarg->b;
    gfxctl.l_alt_targ->lev = lev;
    gfxctl.l_alt_targ->px = x;
    gfxctl.l_alt_targ->py = y;
    gfxctl.l_alt_targ->dir = dir;
    gfxctl.l_alt_targ->tlight = tlight;
        
    VOIDRETURN();
}

void destroy_alt_targ(void) {
    onstack("destroy_alt_targ");
    
    dsbfree(gfxctl.l_alt_targ);
    gfxctl.l_alt_targ = NULL;
    
    VOIDRETURN();
}

// on strange hardware, this may cause explosions at a
// distance to look strange, due to byte order issues.
// if this happens, rewrite it with getr etc. macros...
void copy_alpha_channel(BITMAP *src, BITMAP *dest) {
    int x, y;
    
    for(y=0;y<src->h;++y) {
        for (x=0;x<src->w;++x) {
            int a_pix = _getpixel32(src, x, y);
            int d_pix = _getpixel32(dest, x, y);
            a_pix = a_pix & 0xFF000000;
            d_pix = (d_pix & 0x00FFFFFF) | a_pix;
            _putpixel32(dest, x, y, d_pix);
        }
    }

}

void reduce_alpha_channel(BITMAP *src, int amount) {
    int x, y;
    
    for(y=0;y<src->h;++y) {
        for (x=0;x<src->w;++x) {
            int s_pix = _getpixel32(src, x, y);
            int alpha_bits = s_pix & 0xFF000000;
            int alpha = alpha_bits >> 24;
            int d_pix;
            
            alpha = alpha - amount;
            if (alpha < 0)
                alpha = 0;
            alpha_bits = alpha << 24;
            
            d_pix = (s_pix & 0x00FFFFFF) | alpha_bits;
            _putpixel32(src, x, y, d_pix);
        }
    }    
}

void xdoubleblit(BITMAP *buf, BITMAP *scx, int xr, int yr) {
    int x, y;
    
    for(y=0;y<yr;++y) {
        for(x=0;x<xr;++x) {
            int p = dsbgetpixel(buf, x, y);
            dsbputpixel(scx, x*2, y*2, p);
            dsbputpixel(scx, x*2+1, y*2, p);
            dsbputpixel(scx, x*2, y*2+1, p);
            dsbputpixel(scx, x*2+1, y*2+1, p);
        }
    }    
}

void uglyscaleblit(BITMAP *buf, BITMAP *scx, int xr, int yr) {
    int xt = 0;
    int yt = 0;
    int x, y;
    
    for(y=0;y<yr;++y) {
        int repeats;
        if ((y % 2) == 0)
            repeats = 2;
        else
            repeats = 1;
            
        while (repeats) {
            for(x=0,xt=0;x<xr;++x) {
                int p = dsbgetpixel(buf, x, y);
                if ((x % 2) == 0) {
                    dsbputpixel(scx, xt++, yt, p);
                    dsbputpixel(scx, xt++, yt, p);
                } else {
                    dsbputpixel(scx, xt++, yt, p);
                }
            }
            --repeats;
            yt++;
        }
    }    
}

void scfix(char u_sb) {

    if (!gd.trip_buffer) {
        //vsync();
        acquire_screen();
        if (gd.double_size == 1) {
            xdoubleblit(soft_buffer, screen, XRES, YRES);
        /*} else if (gd.double_size == 3) {
            uglyscaleblit(soft_buffer, screen, XRES, YRES);
        */
        } else {
            blit(soft_buffer, screen, 0, 0, 0, 0, XRES, YRES);
        }
        release_screen();
        return;
    }

	if (u_sb) {
        acquire_bitmap(scr_pages[gd.scr_blit]);
        blit(soft_buffer, scr_pages[gd.scr_blit], 0, 0, 0, 0, XRES, YRES);
        release_bitmap(scr_pages[gd.scr_blit]);
    }

    gd.scr_show = gd.scr_blit;
	gd.scr_blit = (gd.scr_blit+1)%3;

	while(poll_scroll()) {
        Sleep(1);
    }

	request_video_bitmap(scr_pages[gd.scr_show]);
}

BITMAP *create_lua_sub_hack(unsigned int inst,
    BITMAP *b_base, int *xsk, int *ysk, int sideview) 
{
    BITMAP *tmp_a_bmp = NULL;
    
    onstack("create_lua_sub_hack");
    
    *xsk = 0;
    *ysk = 0;
    
    tmp_a_bmp = create_bitmap(b_base->w, b_base->h);
    blit(b_base, tmp_a_bmp, 0, 0, 0, 0, b_base->w, b_base->h);
    
    if (member_begin_call(inst, "bitmap_tweaker")) {
        struct animap vanimap; 
        int coords[2];
        
        memset(&vanimap, 0, sizeof(struct animap));
        vanimap.b = tmp_a_bmp;
        vanimap.flags = AM_VIRTUAL;
        
        setup_lua_bitmap(LUA, &vanimap);                    
        lua_pushinteger(LUA, tmp_a_bmp->w);
        lua_pushinteger(LUA, tmp_a_bmp->h);
        lua_pushboolean(LUA, !!sideview);

        member_finish_call(4, 2);
        member_retrieve_stackparms(2, coords);
        
        *xsk = coords[0];
        *ysk = coords[1];
    }

    RETURN(tmp_a_bmp);
}

void activate_gui_zone(char *dstr, int zid, int x, int y, int w, int h, int flags) {

    onstack("activate_gui_zone");
    
    if (zid >= NUM_SR) {
        poop_out("Too many GUI zones created!");
        VOIDRETURN();
    }
    
    // special mouse hand id (current_item)
    if (zid == -1) {
        gfxctl.curobj_x = x;
        gfxctl.curobj_y = y;  
    // special portrait id (portraits)
    } else if (zid == -2) {
        gfxctl.port_x = x;
        gfxctl.port_y = y;
    // special console id (console)
    } else if (zid == -3) {
        gfxctl.con_x = x;
        gfxctl.con_y = y;
    // special guyicon id (guy_icons)
    } else if (zid == -4) {
        gfxctl.guy_x = x;
        gfxctl.guy_y = y;
    } else {
        gfxctl.SR[zid].renderer_name = dstr;
        gfxctl.SR[zid].rtarg = create_vanimap(w, h); 
        gfxctl.SR[zid].rtx = x;
        gfxctl.SR[zid].rty = y;  
        gfxctl.SR[zid].sizex = w; 
        gfxctl.SR[zid].sizey = h;        
        gfxctl.SR[zid].flags = SRF_ACTIVE | flags;
        gfxctl.SR[zid].cz_n = 0;
        gfxctl.SR[zid].cz = NULL;      
    }
    
    VOIDRETURN();
}

void internal_gui_command(const char cmd, int val) {
    onstack("internal_gui_command");
    
    if (cmd == 'c') {
        gfxctl.console_lines = val;
    } else if (cmd == 'i') {
        gfxctl.itemname_drawzone = val;
    }
    
    VOIDRETURN();
}

// called via Lua to import the shading_info table
void internal_shadeinfo(unsigned int arch, int otype, int odir, int d1, int d2, int d3) {
    onstack("internal_shadeinfo");
    
    if (otype == 0 || odir == 0) {
        VOIDRETURN();
    }
    
    if (arch == 0xFFFFFFFF) {
        gfxctl.shade[otype-1][odir-1][0] = d1;
        gfxctl.shade[otype-1][odir-1][1] = d2;
        gfxctl.shade[otype-1][odir-1][2] = d3;
    } else {
        struct obj_arch *p_arch = Arch(arch);
        if (!p_arch->shade)
            p_arch->shade = dsbcalloc(1, sizeof(struct arch_shadectl));
        
        p_arch->shade->d[odir-1][0] = d1;
        p_arch->shade->d[odir-1][1] = d2;
        p_arch->shade->d[odir-1][2] = d3;
    }
    
    VOIDRETURN();    
}
