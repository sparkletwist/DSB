#define XF_PSEL     0x01

struct ext_hwnd_info {
    HWND h;
    int flags;
    int inst;
    int wtype;
    
    int zm; // zone mode
    
    union {
        int uzone; // uzone value for me
        int plevel; // or active level for a picker
    };
    
    const char *sel_str;
    
    BITMAP *b_buf;
    HWND b_buf_win;
    
    char data_prefix[12];
    
    char q_r_options;
    char q_UNUSED_2;
    char q_UNUSED_3;
    char q_UNUSED_4;
    
    int b_w;
    int b_h;
    int c_w;
    int c_h;
    
    struct ext_hwnd_info *n;
};

struct ext_hwnd_info *get_hwnd(HWND v);
struct ext_hwnd_info *add_hwnd(HWND v, int i_inst, int uzone);
struct ext_hwnd_info *add_hwnd_b(HWND v, BITMAP *b_buf, HWND b_buf_win);
void del_hwnd(HWND v);
void add_xinfo_coordinates(HWND hwnd, struct ext_hwnd_info *x, int b);
