#include <allegro.h>
#include <winalleg.h>
#include <commctrl.h>
#include "E_res.h"
#include "objects.h"
#include "uproto.h"
#include "global_data.h"
#include "editor_hwndptr.h"
#include "editor_gui.h"

struct ext_hwnd_info *ESBxh;

struct ext_hwnd_info *get_hwnd(HWND v) {
    struct ext_hwnd_info *itv = ESBxh;
    while (itv) {
        if (itv->h == v)
            return itv;        
        itv = itv->n;
    }    
    return NULL;
}

struct ext_hwnd_info *add_hwnd(HWND v, int i_inst, int uz) {
    struct ext_hwnd_info *nv;
    nv = dsbcalloc(1, sizeof(struct ext_hwnd_info));
    nv->h = v;
    nv->inst = i_inst;
    nv->uzone = uz;
    nv->n = ESBxh;    
    ESBxh = nv;
    return nv;    
}

struct ext_hwnd_info *add_hwnd_b(HWND v, BITMAP *b_buf, HWND b_buf_win) {
    struct ext_hwnd_info *nv;
    nv = add_hwnd(v, 0, 0);
    nv->b_buf = b_buf;
    nv->b_buf_win = b_buf_win;
    return nv;
}

void add_xinfo_coordinates(HWND hwnd, struct ext_hwnd_info *x, int base) {
    RECT r_window;
    onstack("add_xinfo_coordinates");
    
    GetWindowRect(hwnd, &r_window);
    x->c_w = r_window.right - r_window.left;
    x->c_h = r_window.bottom - r_window.top;
    
    if (base) {
        x->b_w = x->c_w;
        x->b_h = x->c_h;
    }

    VOIDRETURN();
}

void del_hwnd(HWND v) {
    struct ext_hwnd_info *itv = ESBxh;
    if (ESBxh && ESBxh->h == v) {
        ESBxh = ESBxh->n;
        dsbfree(itv);
        return;
    }    
    while (itv->n) {
        if (itv->n->h == v) {
            struct ext_hwnd_info *itv_n = itv->n;
            itv->n = itv->n->n;
            dsbfree(itv_n);
            return;
        }        
        itv = itv->n;
    }  
}
