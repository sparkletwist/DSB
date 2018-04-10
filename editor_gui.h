enum {
    EDITOR_LOADING,
    EDITOR_PARSING,
    _EDITOR_RESERVED_1,
    _EDITOR_RESERVED_2,
    EDITOR_RUNNING
};

enum {
    DRM_DONOTHING,
    DRM_DRAWDUN,
    DRM_DRAWOBJ,
    DRM_WALLSET,
    DRM_PARTYSTART,
    DRM_CUTPASTE,
    MAX_DRM_VAL
};

#define EEF_NOLIMBOSAVE     0x001
#define EEF_FLOORLOCK       0x002
#define EEF_ALCOVE          0x004
#define EEF_CEILPIT         0x008
#define EEF_DOORFRAME       0x010

#define EMF_DOUBLEZOOM  0x0001
#define EMF_TARGREVERSE 0x0002

#define INVALID_UZONE_VALUE 0x0F

typedef struct __cut_paste_sel {
    int level;
    int x1;
    int UNUSED_x2;
    int y1;
    int UNUSED_y2; 
} cut_paste_sel;

typedef struct __cut_paste_cb {
    int dup;
    int UNUSED_xsiz;
    int UNUSED_ysiz;
    struct inst_loc *t[MAX_DIR];   
} cut_paste_cb;

struct editor_global {
    HWND subwin;
    HWND infobar;
    HWND loc;
    HWND toolbar;
    HWND treeview;
    HWND coordview;
    HWND tile_list;
    
    HWND f_tool_win;
    HWND f_item_win;
    
    HACCEL haccel;
    
    BITMAP *cells;
    BITMAP *zones;
    BITMAP *marked_cell;
    BITMAP *true_cells;
    BITMAP *true_marked_cell;
    
    BITMAP *sbmp;
    
    int runmode;
    int op_inst;
    unsigned int op_arch;
    unsigned int last_inst;
    
    unsigned char draw_mode;
    unsigned char modeflags;
    char f_d_dir;
    unsigned char uzone;
    
    unsigned char ldown;
    unsigned char rdown;
    unsigned char d_dir;
    unsigned char dmode;

    unsigned char force_zone;
    unsigned char completion_flag;
    unsigned char dirty;
    unsigned char autosel; // how to determine tilepos automatically
    
    unsigned char op_picker_val;
    unsigned char q_r_options;
    unsigned char q_sp_options;
    unsigned char in_purge;
    
    unsigned char toolbars_floated;
    unsigned char ch_ex_mode;
    unsigned char __X2;
    unsigned char __X3;
    unsigned char __X4;
    
    short global_mark_level;
    unsigned short global_mark_x;
    unsigned short global_mark_y;
    unsigned short __UNUSED_GLOBAL_MARK;
    
    // last wallset draw
    short wsd_x;
    short wsd_y;
    
    // last selected location
    short lsx;
    short lsy;
    
    // absolute last mousemove location
    int lmm_lparam;
    
    // mouse over tile location
    short o_tx;
    short o_ty;
    
    struct eextchar *c_ext;
    int i_v2;
    const char *w_t_string;
    const char *exvn_string;
    const char *op_string;
    const char *op_base_sel_str;
    const char *data_prefix;
    const char *valid_savename;
    
    int ed_lev;
    int op_p_lev;
    
    short op_x;
    short op_y;
    
    char *curdir;
    char *draw_arch;
    char *draw_ws;
    
    unsigned int editor_flags;
    int op_b_id;
    HWND op_hwnd;
    int op_winmsg;
    int op_hwnd_extent;
    void *op_data_ptr;
    
    HWND allow_control;
    
    cut_paste_sel *cp;
    cut_paste_cb *cb;
};

struct edraw_list {
    unsigned int num;
    unsigned int pri;
    short dest_x;
    short dest_y;
    struct edraw_list *n;
};

#define BXS 780
#define BYS 580
#define TREEWIDTH 180

#define CTRLHEIGHT 72
#define CTRLREALSIZE 65
#define DRAW_OFFSET 32

void ed_resizesubs(HWND ehwnd);

void force_redraw(HWND ehwnd, int);

void ESB_dungeon_windowpaint(HWND ehwnd, BITMAP *backbuffer);
void load_editor_bitmaps(void);

int ed_picker_click(int type, HWND ehwnd, int i, int l, int x, int y);
void ed_window_mouseclick(HWND ehwnd, int x, int y, int inh);
void ed_get_uzonetype(HWND ehwnd, int x, int y);

int ed_fillboxfromtile(HWND ehwnd, int id, int lev, int tx, int ty, int dcurs);
void ed_reselect_inst(HWND hwnd, int item, int cinst);

int ed_obj_add(const char *arch_name, int level, int x, int y, int dir);

void ed_build_tree(HWND tree, int ct, const char *base_sel);

void ed_get_drawnumber(struct obj_arch *p_arch, int dir, int inact, int cell,
    int *num, int *ord);
int ed_edit_exvar(HWND owner, int inst, const char *dt, const char *exvn, char dt_fast);

void update_draw_mode(HWND ehwnd);
int locw_update(HWND hwnd, struct ext_hwnd_info *exi);

void clbr_sel(LPARAM lParam);
void clbr_del(LPARAM lParam);

int locw_draw(HWND ehwnd, HDC hdc, struct ext_hwnd_info *exi);

void ed_init_level_bmp(int lev, HWND hwnd);

void ed_move_tile_position(int inst, int zmt);
void ed_scrollset(int p, int olev, HWND sub, int i_w, int i_h);

BITMAP *ESBpcxload(char *afilename);

INT_PTR CALLBACK ESB_picker_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_target_picker_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
    
INT_PTR CALLBACK ESB_archpick_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_archpick_swap_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_spinedit_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_shooter_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESB_function_caller_proc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ESBpartyproc(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam);
        
#define QO_MOUSEOVER()\
{ edg.o_tx = -1;\
edg.o_ty = -1;\
PostMessage(edg.subwin, WM_MOUSEMOVE, 0, edg.lmm_lparam); }
