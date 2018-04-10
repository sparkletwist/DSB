enum {
    FS_NOMOUSE,
    FS_MOUSEPTR,
    FS_MOUSECUSTOM
};

#define FS_B_DRAW_GAME      0x01

struct fullscreen {
    BITMAP *fs_scrbuf;
    
    struct animap *bkg;
    struct animap *mouse;
    int lr_bkg_draw;
    int lr_on_click;
    int lr_update;
    
    unsigned char fs_mousemode;
    unsigned char fs_fademode;
    unsigned char fs_bflags;
    unsigned char fs_c_v4;
};

int fullscreen_mode(struct fullscreen *fsd);
    
