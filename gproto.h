enum {
    VIEWSTATE_DUNGEON,      // past here you can't move
    VIEWSTATE_INVENTORY,    // past here no magic or attacks
    VIEWSTATE_SLEEPING,     // past here no time passes
    VIEWSTATE_FROZEN,
    VIEWSTATE_CHAMPION,
    VIEWSTATE_GUI,
    MAX_VIEWSTATES
};

#define GUI_NO_GUI              0x00
#define GUI_WANT_SAVE           0x01
#define GUI_PICK_FILE           0x02
#define GUI_ENTER_SAVENAME      0x08

#define GUI_NAME_ENTRY 0x010000

#define REINCARNATE 2
#define RESURRECT 1
#define RES_REI 3

#define NUM_ZONES 110

#define ZONE_EYE 31
#define ZONE_MOUTH 32
#define ZONE_MUSIC 33
#define ZONE_DISK 34
#define ZONE_ZZZ 35
#define ZONE_EXITBOX 36
#define ZONE_RES 37
#define ZONE_REI 38
#define ZONE_CANCEL 39

enum {
    MM_NOTHING,
    MM_MOUTHLOOK,
    __MM_UNUSED1,
    __MM_UNUSED2,
    __MM_UNUSED3,
    __MM_UNUSED4,
    __MM_UNUSED5,
    __MM_UNUSED6,
    __MM_UNUSED7,
    __MM_UNUSED8,
    MM_EYELOOK,
    MM_EYELOOKOBJ
};


#define G_ALT_ICON  0x0001
// duplicates OF_INACTIVE
#define G_INACTIVE  0x0002
#define G_BASHED    0x0004
#define G_ATTACKING 0x0008
#define G_UNMOVED   0x0010
#define G_FLIP      0x0020
#define G_FREEZE    0x0040
#define G_LAUNCHED  0x0080

#define TINTFLAG_NORED      0x01
#define TINTFLAG_NOGREEN    0x02
#define TINTFLAG_NOBLUE     0x04

struct alt_targ {
    BITMAP *targ;
    int lev;
    int px;
    int py;
    int dir;
    int tlight;
};

void define_alt_targ(int lev, int x, int y, int dir, int tlight);
void destroy_alt_targ(void); 

void get_sysgfx(void);
int check_subgfx_state(int vs);
int render_dungeon_view(int softmode, int);
int draw_interface(int softmode, int viewstate, int real);
void do_gui_options(int softmode);
void draw_gui_controls(BITMAP *scx, int bx, int by, int modesel);
void openclip(int softmode);
BITMAP *pcxload(const char *p_str, const char *longname);
BITMAP *pcxload8(const char *p_str, const char *longname, RGB *use);
FONT *fontload(const char *l_str, const char *longname);
BITMAP *find_rendtarg(int);

void destroy_animap(struct animap *lbmp);
void conddestroy_dynscaler(struct animap_dynscale *ad);
void conddestroy_animap(struct animap *ani);

void setup_res_rei_cz(void);
void setup_rei_name_entry_cz(void);

void draw_name_entry(BITMAP *scx);

void setup_misc_inv(BITMAP *scx, int, int, int);

void make_cz(int cznum, int mx, int my, int w, int h, int inum);
int scan_clickzones(struct clickzone *cz_a, int xx, int yy, int max);

struct animap *scaleimage(struct animap *base_map, int sm);
struct animap *scaledoor(struct animap *base_map, int sm);

struct animap *grab_from_gfxtable(const char *nam);

BITMAP *animap_subframe(struct animap *amap, int d);
BITMAP *monster_animap_subframe(struct animap *amap, int curframe);
BITMAP *fz_animap_subframe(struct animap *amap, struct inst *p_inst);
int n_animap_subframe(struct animap *amap, int d, int fc);

void draw_file_names(BITMAP *scx, int bx, int by);
void draw_file_names_csave(BITMAP *scx, int bx, int by);

int draw_wall_writing(int softmode, BITMAP *scx, unsigned int inst,
    int mx, int my);
BITMAP *create_lua_sub_hack(unsigned int inst, BITMAP *b_base,
    int *xsk, int *ysk, int sideview);

void masked_copy(BITMAP *src, BITMAP *dest, int sx, int sy, 
    int dx, int dy, int w, int h, BITMAP *masker, 
    int mrx, int mry, int mcol, int mcol2, int rev, int mrev);

/*    
void remask_blit(BITMAP *src, BITMAP *dest, int sx, int sy, 
    int dx, int dy, int w, int h, int, int, int);
*/
    
void render_console(BITMAP *scx);
void init_colordepth(int cd);

void draw_lua_errors(BITMAP *scx);

int draw_objicon(BITMAP *scx, unsigned int inst, int mx, int my, int lit);
int draw_gfxicon(BITMAP *scx, struct animap *ani, int mx, int my, int lit, struct inst *p);
int draw_gfxicon_af(BITMAP *scx, struct animap *ani, int mx, int my, int lit,
    struct inst *p_inst, int alphafix);
void set_dsb_fixed_alpha_blender(int alphafix);
void draw_inv_statsbox(BITMAP *scx, int px, int py, int pt);
void draw_obj_infobox(BITMAP *scx, int px, int py, int pt);

void draw_eye_border(BITMAP *scx, struct champion *w, int xt, int yt);
void draw_mouth_border(BITMAP *scx, struct champion *w, int xt, int yt);

void draw_guy_icons(BITMAP *scx, int px, int py, int fd);
void draw_spell_interface(BITMAP *scx, int i_index, int vs);

void draw_mouseptr(BITMAP *targ, struct animap *special_ptr, int allow_override);
void set_mouse_override(struct animap *override_image);
void clear_mouse_override(void);

int mline_breaktext(BITMAP *scx, FONT *font, int bxc, int byc,
    int color, int bg, char *str, int cpl, int maxlines, int ypl, int flags);

char *determine_name(unsigned int inst, int flags, int *del);
void show_name(BITMAP *scx, unsigned int inst, int x, int y, int col, int flags);

unsigned long cyan_darkdun24(unsigned long x, unsigned long y, unsigned long n);
unsigned long nocyan_darkdun24(unsigned long x, unsigned long y, unsigned long n);

void purge_defer_list(void);

int initialize_wallset(struct wallset *newset);

void copy_alpha_channel(BITMAP *src, BITMAP *dest);
void reduce_alpha_channel(BITMAP *src, int mp);

int trapblit(int drawmode, BITMAP *scx, int bx, int by, struct dtile *ct,
    int xc, int yc, int oo, int flags, struct animap *floor, struct animap *roof);
    
int bmp_alpha_scan(BITMAP *ibmp);
struct animap *prepare_animap_from_bmp(BITMAP *in_bmp, int targa);
struct animap *prepare_lod_animap(const char *name, const char *longname);
void lod_use(struct animap *lani);
void purge_from_lod(int mode, struct animap *lani);
void destroy_lod_list(void);
void full_lod_use_check(void);
void frame_lod_use_check(void);

struct animap **get_view(struct obj_arch *p_arch, int view);
void change_gfxflag(int inst, int gflag, int operation);

void thick_line(BITMAP *bmp, float x, float y, float x_, float y_,
    float thickness, int color);

void distort_viewport(int process, BITMAP *dest, int bx, int by, int t_val);
void pushtxt_cons_bmp(const char *textmsg, int col, struct animap *bgcol, int ttl);
