#include <allegro.h>
#include <winalleg.h>
#include <commctrl.h>
#include "E_res.h"
#include "objects.h"
#include "uproto.h"
#include "global_data.h"
#include "editor.h"
#include "editor_hwndptr.h"
#include "editor_gui.h"

extern struct editor_global edg;
static int global_modal_count = 0;

int ESB_modaldb(int ir, HWND owner, DLGPROC lpDialogFunc, int dvhack) {
    HWND dg;
    MSG messages;
    HWND grabbed;
    int modals;
    int imout = 0;
    
    onstack("ESB_modaldb");

    grabbed = GetFocus();
    dg = CreateDialog(GetModuleHandle(NULL),
        MAKEINTRESOURCE(ir), owner, lpDialogFunc);
    ShowWindow(dg, SW_SHOW);
    EnableWindow(owner, FALSE);
    
    memset(&messages, 0, sizeof(MSG));
    
    modals = ++global_modal_count;
    while (GetMessage(&messages, NULL, 0, 0) > 0) {
        if (global_modal_count < modals) {
            imout = 1;
        }
        
        // hack to let the dudes get the keys
        if (dvhack && messages.message == WM_KEYDOWN) {
            if (messages.wParam >= 'A' && messages.wParam <= 'Z') {
                if (edg.allow_control && GetFocus() != edg.allow_control) {
                    PostMessage(dg, EDM_DVKEY, messages.wParam, messages.lParam);
                    continue;
                }
            }
        }
            
        if (!imout && !IsDialogMessage(dg, &messages)) {
            TranslateMessage(&messages);
            DispatchMessage(&messages);
        } 
        if (imout) break;
    }
    EnableWindow(owner, TRUE);
    DestroyWindow(dg);
    SetFocus(grabbed);
    
    RETURN(0);
}

void ESB_go_up_one(HWND hwnd) {
    global_modal_count--;
    SendMessage(hwnd, WM_NULL, 0, 0);
} 


                        
