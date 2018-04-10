
#define XDF_PARTYPOS        0x01
#define XDF_WSPAINT         0x02

enum {
    ESBM_NEW = 20000,
    ESBM_OPEN,
    ESBM_SAVE,
    ESBM_SAVEAS,
    ESBM_EXIT,
    ESBM_MKGAMEDIR,
    ESBM_TESTGAME,
    
    DRM_ESBM_MODEBASE = 20099,
    ESBM_MODE_DD,
    ESBM_MODE_AI, // add insts
    ESBM_MODE_WSG, // paint wall
    ESBM_MODE_PRP, // party place
    ESBM_MODE_CUT, // cut n paste
    
    ESBM_DO_CUT = 20115,
    ESBM_DO_COPY,
    ESBM_DO_PASTE,
    ESBM_MODESWAP_DO_CUT,
    ESBM_MODESWAP_DO_COPY,
    ESBM_MODESWAP_DO_PASTE,
    
    ESBM_RELOADARCH = 20125,
    ESBM_LEVELINFO,
    ESBM_CHAMPS,
    ESBM_PREFS,
    ESBM_GRAPHICS,
    ESBM_GLOBALINFO,
    
    ESBM_PREVL = 20200,
    ESBM_NEXTL,
    ESBM_DOUBLEZOOM,
    ESBM_TARGREVERSE,
    ESBM_FLOATWIN,
    ESBM_SEARCHFIND = 20250,
    
    ESBA_MODE1 = 21000,
    ESBA_MODE2,
    ESBA_MODE3,
    ESBA_MODE4,
    ESBA_MODE5,
    ESBA_EDITLAST
};

void ed_menu_setup(HWND ehwnd);
void ed_menucommand(HWND hwnd, unsigned short cmd_id);
void ed_quit_out(void);

INT_PTR CALLBACK ESB_edit_exvar_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_add_exvar_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESBoptionsproc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_level_info_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_global_info_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
