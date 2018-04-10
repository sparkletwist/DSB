#define ESBF_STATICFILL     -256

#define XW_NO_CLICKS    256

#define ESB_MESSAGE_DEBUG() \
    {\
        char debugstr[200];\
        stackbackc('(');\
        sprintf(debugstr, "%x, %d, %ld", message, wParam, lParam);\
        v_onstack(debugstr);\
        addstackc(')');\
    }

enum {
    EDM_VOID_PSEL = WM_APP,
    EDM_MAIN_TREE_WS_ADD,
    EDM_DVKEY = (WM_APP + 64),
    EDM_TARGETPICK,
    EDM_PARTY_REFRESH
};

void setup_objects(void);
void initialize_lua(void);

void init_exvars(void);
void setup_editor_objects(const char *fname);

void SetD(unsigned char dmode, char *s_etext, char *s_wtext);

void ESBsysmsg(char *c, int);
int editor_load_dungeon(char *fname);
void editor_export_dungeon(const char *fname);
void ed_tag_l(const char *s_add_me);
char *ed_makeselexvar(HWND hlist, int *p, char *dt);

void update_all_inventory_info(void);

INT_PTR CALLBACK ESBdproc (HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_build_container(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_monstergen_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_opby_ed_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_mirror_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESBsearchproc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_item_action_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_door_edit_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_counter_edit_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_trigger_controller_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
    
LRESULT CALLBACK ESBtoolproc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
    
LRESULT CALLBACK ed_sync_popups (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

int ESB_modaldb(int ir, HWND owner, DLGPROC lpDialogFunc, int dvh);
void ESB_go_up_one(HWND hwnd);
void recursive_inst_destroy(int inst, int queue);

int editor_create_new_gamedir();
void esb_clear_used_wallset_table(void);
void ed_reload_all_arch(void);
void ed_purge_treeview(HWND tv);
unsigned int wscomp(int lev, int x, int y, int t);
void ed_paint_wallset(BITMAP *scx, int l, int xx, int yy, int drx, int dry);
void ed_m_main_tree_ws_add(const char *lstr);
void ed_main_treeview_refresh_wallsets(void);
int ed_location_pick(int lev, int tx, int ty);

void float_toolbars(void);
void unfloat_toolbars(void);
void init_right_side_windows(HINSTANCE hThisInstance);
void setup_right_side_window_fonts(void);
