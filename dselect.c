#include <allegro.h>
#include <winalleg.h>
#include <objbase.h>
#include <shlobj.h>
#include "global_data.h"
#include "uproto.h"

extern struct global_data gd;

// start in the current directory rather than in the root
int CALLBACK selectcallback(HWND hwnd, UINT u_msg, LPARAM lp, LPARAM data) {
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

int selectdungeondir(void) {
    HRESULT hr_c;
    BROWSEINFO brinfo;
    LPITEMIDLIST p_dpl;
    int r_v = 0;

    onstack("selectdungeondir");

    hr_c = CoInitialize(NULL);
    if (hr_c != S_OK) {
        if (hr_c == S_FALSE)
            CoUninitialize();
        else
            poop_out("Unable to initialize dungeon selection dialog.\nYou will have to set a dungeon= line in dsb.ini.");
    }

    memset(&brinfo, 0, sizeof(BROWSEINFO));
    brinfo.lpszTitle = "Choose Dungeon Location";
    brinfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT;// | BIF_NEWDIALOGSTYLE;
    brinfo.lpfn = selectcallback;
    
    p_dpl = SHBrowseForFolder(&brinfo);
    
    if (p_dpl) {
        TCHAR ts_path[MAX_PATH];
        LPMALLOC p_malloc;
        
        if (SHGetPathFromIDList(p_dpl, ts_path)) {
            gd.dprefix = dsbstrdup(ts_path);
            r_v = 1;
        }
        
        if (SUCCEEDED(SHGetMalloc(&p_malloc))) {
            p_malloc->lpVtbl->Free(p_malloc, p_dpl);
            p_malloc->lpVtbl->Release(p_malloc);
        }
    }
    
    CoUninitialize();
    
    RETURN(r_v);
}
