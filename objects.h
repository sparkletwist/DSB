#define NEW_ARCH 0
#define QUERY_ARCH 1
#define CLEAR_ALL_ARCH 128
#define INTEGRITY_CHECK 256

#define NUM_ARCHS 255

#define HASHMAX 1024

#define MAX_FOOD 3072

#define COL_DIFF_TYPES 0x00010000
#define COL_HAS_DOOR   0x00020000
#define COL_HAS_OBJ    0x00040000
#define COL_NO_MONS    0x00080000

enum {
    WST_BIGWALLS,
    WST_LR_SEP,
    WST_DM2_STYLE,
    _WST_UNUSED,
    WST_ERROR
};

enum {
    INSTMD_INST_MOVE,
    INSTMD_INST_DELETE,
    INSTMD_INST_FLYER,
    INSTMD_PARTY_PLACE,
    INSTMD_RELOCATE,
    INSTMD_KILL_MONSTER,
    INSTMD_PTREXCH,
    INSTMD_TROTATE
};

enum {
    OBJTYPE_INVALID,
    OBJTYPE_WALLITEM,
    OBJTYPE_FLOORITEM,
    OBJTYPE_UPRIGHT,
    OBJTYPE_MONSTER,
    OBJTYPE_THING,
    OBJTYPE_DOOR,
    OBJTYPE_CLOUD,
    OBJTYPE_HAZE,
    OBJTYPE_UNDEFINED,
    END_OF_OBJ_TYPES
};

enum {
    WALLBMP_FLOOR,
    WALLBMP_ROOF,
    WALLBMP_PERS0,
    WALLBMP_PERS0ALT,
    WALLBMP_PERS1,
    WALLBMP_PERS1ALT,
    WALLBMP_PERS2,
    WALLBMP_PERS2ALT,
    WALLBMP_PERS3,
    WALLBMP_PERS3ALT,
    WALLBMP_FARWALL3,
    WALLBMP_FARWALL3ALT,
    WALLBMP_FRONT1,
    WALLBMP_FRONT2,
    WALLBMP_FRONT3,
    MAX_WALLBMP
};

enum {
    XWALLBMP_LEFT1,
    XWALLBMP_LEFT1ALT,
    XWALLBMP_LEFT2,
    XWALLBMP_LEFT2ALT,
    XWALLBMP_LEFT3,
    XWALLBMP_LEFT3ALT,
    MAX_XWALLBMP
};

enum {
    WALLPATCH_1,
    WALLPATCH_2,
    WALLPATCH_3,
    MAXWALLPATCH
};

enum {
    INV_R_HAND,
    INV_L_HAND,
    INV_HEAD,
    INV_TORSO,
    INV_LEGS,
    INV_FEET,
    INV_NECK,
    INV_POUCH1,
    INV_POUCH2,
    INV_QUIV,
    INV_QUIV2,
    INV_QUIV3,
    INV_QUIV4,
    INV_PACK
};

#define INV_INVALID -878787

#define INVZMASK 0x80000000
    
enum {
    DIR_NORTH,
    DIR_EAST,
    DIR_SOUTH,
    DIR_WEST,
    DIR_CENTER,
    MAX_DIR
};
#define DIR_NORTHWEST DIR_NORTH
#define DIR_NORTHEAST DIR_EAST
#define DIR_SOUTHEAST DIR_SOUTH
#define DIR_SOUTHWEST DIR_WEST

#define RHACK_DOORFRAME         0x00001
#define RHACK_DOORBUTTON        0x00002
#define _RHACK_UNUSED1          0x00004
#define RHACK_MIRROR            0x00008
#define RHACK_BLUEHAZE          0x00010
#define RHACK_GOT_NAMECHANGER   0x00020
#define RHACK_WALLPATCH         0x00040
#define RHACK_FAKEWALL          0x00080
#define RHACK_CLICKABLE         0x00100 
#define RHACK_WRITING           0x00200
#define RHACK_ONLY_DEFINED      0x00400
#define RHACK_STAIRS            0x00800
#define _RHACK_UNUSED           0x01000
#define RHACK_2WIDE             0x02000
#define RHACK_NOSHADE           0x04000
#define RHACK_POWERSCALE        0x08000
#define _RHACK_UNUSED2          0x10000
#define RHACK_DYNCUT            0x20000
#define RHACK_INVISWALL         0x40000

enum {
    DTYPE_UPDOWN,
    DTYPE_LEFTRIGHT,
    DTYPE_MAGICDOOR,
    DTYPE_LEFT,
    DTYPE_RIGHT,
    DTYPE_NORTHWEST,
    DTYPE_SOUTHEAST
};

#define ARFLAG_FLY_ONLY         0x000001
#define _ARFLAG_NOTUSED_1       0x000002
#define ARFLAG_DROP_ZONE        0x000004
#define ARFLAG_DRAW_CONTENTS    0x000008
#define ARFLAG_NO_BUMP          0x000100
#define ARFLAG_IMMOBILE         0x000200
#define ARFLAG_INSTTURN         0x000400
#define ARFLAG_NOCZCLOBBER      0x000800
#define ARFLAG_SMOOTH           0x001000
#define _ARFLAG_NOTUSED_2       0x002000
#define ARFLAG_DZ_EXCHONLY      0x004000
#define ARFLAG_METHOD_FUNC      0x008000
#define _ARFLAG_PRIORITY_UNUSED 0x010000

#define LOC_PARTY -1
#define LOC_IN_OBJ -2
#define LOC_CHARACTER -3
#define LOC_MOUSE_HAND -4
#define LOC_LIMBO -5

// special! temporary party pushing area
#define LOC_PARTYPUSHLIMBO -17

// special! party controlled space for
// external control instances. whee!
#define LOC_PARCONSPC   -16

enum {
    EX_MESSAGE,
    EX_TARGET,
    EX_OPBY,
    EX_CHAMPION,
    MAX_EXVARS
};

/* animap flags */
#define AM_HASALPHA         0x01
#define __AM_UNUSED         0x02
#define AM_DYNGEN           0x04
#define AM_256COLOR         0x10
#define AM_VIRTUAL          0x20

/* object flags */
// (now entirely absorbed into gfxflags)
#define OF_INACTIVE 2

struct animap_dynscale {
    struct animap *v[6];
};

// load on demand images
#define ALOD_LOADED     0x01
#define ALOD_SCANNED    0x02
#define ALOD_KEEP       0x04
#define ALOD_ANIMATED   0x08
struct lod_data {
    unsigned int d;
    unsigned int lastuse;
    char *shortname;
    char *filename;
    
    short ani_num_frames;
    short ani_framedelay;
};

struct animap {
    union {
        BITMAP *b;
        BITMAP **c_b;
    };
    int numframes;
    
    short framedelay;
    short __UNUSED_SHORT;
    
    unsigned short clone_references;
    unsigned short ext_references;

    short mid_x_tweak;
    short mid_y_tweak;
    short far_x_tweak;
    short far_y_tweak;
    
    short mid_x_compress;
    short mid_y_compress;
    short far_x_compress;
    short far_y_compress;
    
    short xoff;
    short yoff;
    short w;
    short flags;
    union {
        RGB *pal;
        int vtype;      // type of virtual bitmap
        // (not currently used for anything, but RGB *pal is)
    };
    
    struct animap *cloneof;
    struct animap_dynscale *ad;
    struct lod_data *lod;
};
struct animap *create_vanimap(int w, int h);

struct mon_att_view {

    unsigned char n;
    unsigned char c_v2;
    unsigned char c_v3;
    unsigned char c_v4;
    
    struct animap **v;
};

struct obj_arch {
    
    char *luaname;
    char *dname;
    
    int type;
    
    // icon view
    struct animap *icon;
    
    // and an alt icon if i'm using one
    union {
        struct animap *alt_icon;
        FONT *w_font;
    };
    
    // base (front) dungeon views
    struct animap *dview[6]; 
        
    // 0 = special view for in-same-square flooritems
    // also flying towards views for things
    // attack view for monster
    union {
        struct animap *inview[6];
        struct mon_att_view iv;
    };
    
    // side angle views for flooritems
    // side views for monsters
    // sideways flying views for things
    // perspective views for wallitems
    struct animap *sideview[6];
    
    // rear views for monsters, flying away views for things
    // optional flipped side view for directionally oriented wallitems
    struct animap *outview[6];
    
    // monster's other side, and whatever else...
    struct animap *extraview[6];
    
    // renderer hacks
    unsigned int rhack;
    
    // attack methods (for renderer)
    struct att_method *method;
    
    // object's mass for load calculations
    // -- or monster's size
    union {
        unsigned int mass;
        unsigned int msize;
    };
    
    // only seen from certain sides?
    unsigned char viewangle;
    unsigned char side_flip_ok;
    unsigned char unused_3;
    unsigned char unused_4;
    
    unsigned short cropmax;
    unsigned short UNUSED_SHORT;
    
    // modify cloud size etc?
    short sizetweak;
    short __UNUSED_SHORT_1;
    
    // misc flags  
    unsigned int arch_flags;
    
    unsigned int def_charge; 
    
    unsigned char UNUSED_C_1;
    unsigned char UNUSED_C_2;
    unsigned char grouptype;
    unsigned char ytweak;
    
    unsigned int lua_reg_key;
    
    union {
        short s_parm1;
        short dtype;
        short base_flyreps;
    };
    short s_parm2;
    
    struct arch_shadectl *shade;
    int UNUSED;
};

struct wallset {
    struct animap *wall[MAX_WALLBMP];
    struct animap *wpatch[MAXWALLPATCH];
    struct animap *sidepatch;
    struct animap *window;
    
    struct animap *xwall[MAX_XWALLBMP];
    
    struct animap *alt_floor;
    struct animap *alt_roof;
    
    // cache is pretty useless
    int UNUSED[11];
    
    unsigned char type;
    unsigned char flipflags;
    unsigned char c_v3;
    unsigned char c_v4;
    
    unsigned int flags;
};

struct dtile {
    
    // wall or not
    unsigned char w;
    // CURRENTLY UNUSED: vertical skew
    // (could be for tables and whatever?)
    unsigned char ___vskew;    
    // external party representation
    // on this tile
    unsigned char exparrep;   
    
    // don't do anything with this
    // the renderer code needs to mess with it
    unsigned char SCRATCH;
    
    // alternate wallset (5 bytes)
    unsigned char altws[MAX_DIR];
    
    struct inst_loc *il[MAX_DIR];
    
};

#define DLF_ITERATIVE_CEIL      0x01
#define DLF_START_INACTIVE      0x02
struct dungeon_level {
    
    int lightlevel;
    int xp_multiplier;
    
    int xsiz;
    int ysiz;
    
    // tiles
    struct dtile **t;
    
    // which wallset in the wallset table to use
    int wall_num;
    
    // shade distant objects with...?
    unsigned int tint;
    unsigned int tint_intensity;
    
    // flags
    unsigned int level_flags;
};

struct obj_after_from_q {
    int lev_parm;
    int x_parm;
    int y_parm;
    int dir_parm;
    int lh1_parm;
    int lh2_parm;    
};

#define DSB_CHECKVALUE 0xDEADDEAD
struct inst {   
    // what am i?
    int arch;
    
    // where am i?
    int level;
    int x;
    int y;
    int tile;
    
    // and where was i?
    struct {
        int level;
        int x;
        int y;
        int tile;
    } prev;
    
    // direction of travel, monster facing, etc.
    int facedir;
    
    // for debugging
    unsigned int CHECKVALUE;
    
    // graphical variation
    int x_tweak;
    int y_tweak;
    
    // object flying through the air
    unsigned short flytimer;
    unsigned short max_flytimer;
    struct timer_event *flycontroller;
    
    // for opening doors (and other things?)
    short crop;
    // graphics flags
    unsigned short gfxflags;
    
    // crop motion, for animated doors
    short crmotion;

    // not currently used, but, 2d crop?
    short _UNUSED_ycrop;
    
    //make this an int to avoid stupid issues with > 255 charges
    int charge;
    
    // not used, formerly a wonky way of managing animation
    unsigned char UNUSED_fd;
    // not used, formerly charge
    unsigned char Unused_char;
    // internal variable
    unsigned short i_var;
    
    // we just give instances a frame counter now
    unsigned int frame;
    
    // number of things inside
    unsigned char inside_n;
    // for flying objects
    unsigned char openshot;    // hack so shooters aren't smacked by own stuff
    unsigned short damagepower;

    // color hack
    unsigned int tint;
    unsigned int tint_intensity;

    // chests and monsters carry things
    unsigned int *inside;
    
    // message chaining (every object is a relay, essentially)
    struct inst_loc *chaintarg;
    int uplink;
    
    // for monsters
    struct ai_core *ai;
    
    // for moving stuff around
    struct obj_after_from_q *after_from_q;
};

struct inst_loc {
    unsigned int i;
    union {
        unsigned char slave;
        unsigned char delink;
    };
    union {
        unsigned char link_unusedv2;
        unsigned char chain_reps;
    };
    unsigned char unusedv3;
    unsigned char unusedv4;
    struct inst_loc *n;
};

#define ZF_SYSTEM_MSGZONE       0x01
#define ZF_SYSTEM_OBJZONE       0x02
struct clickzone {
    int x;
    int y;
    int w;
    int h;
    
    unsigned int zoneflags;
    
    union {
        unsigned int inst;
        int sys_msg;
    };
    
    union {
        unsigned int ext_data;
        unsigned short sys_data[2];
    };
};

struct att_method {
    char *name;
    unsigned short minlev;
    unsigned short reqclass;
    unsigned short reqclass_sub;
    unsigned short UNUSED_SHORT;
    int luafunc;
};

struct exports {
    char *s;
    struct exports *n;
};

/*
struct obj_hash_entry {
    int o_n;
    struct obj_hash_entry *n;
};
*/

struct obj_aff {
    int op;
    
    union {
        unsigned int inst;
        int x2;
    };

    int lev;

    union {
        int x;
        int fdist;
    };
    
    union {
        int y;
        int dpower;
    };
    
    union {
        int dir;
        int lev2;
    };
    
    union {
        int data;
        int y2;
    };
    
    struct obj_aff *n;
};

struct iidesc {
    short x;
    short y;
    
    struct animap *img;
    struct animap *img_ex;
};
    
enum {
    TOP_HANDS = 0,
    TOP_PORT,
    TOP_DEAD,
    MAX_TOP_SCR
};
    
struct inventory_info {
    struct animap *main_background;
    struct animap *inventory_background;
    struct animap *topscr[MAX_TOP_SCR];
    
    struct animap *sel_box;
    struct animap *boost_box;
    struct animap *hurt_box;
    
    short srx;
    short sry;
    short topx;
    short topy;
    
    unsigned int max_invslots;
    unsigned int max_injury;
    
    struct iidesc *inv;
    
    struct iidesc eye;
    struct iidesc mouth;
    struct iidesc save;
    struct iidesc sleep;
    struct iidesc exit;
    
    unsigned short stat_maximum;
    unsigned short stat_minimum;
    
    unsigned char filled;
    unsigned char c_v2;
    unsigned char c_v3;
    unsigned char c_v4;
    
    unsigned int lslot[4];
    unsigned int rslot[4];
};

int integerlookup(const char *s_name, int default_value);
int boollookup(const char *s_name, int default_value);
int tablecheckpresence(const char *s_name);

void place_instance(unsigned int inst, int lev, int xx, int yy, int dir, int);
unsigned int create_instance(unsigned int objarch, int lev, int xx, int yy, int dir, int bfi);
unsigned int next_object_inst(unsigned int archnum, int last_alloc);
void purge_dirty_list(void);
void tweak_thing(struct inst *o_i);
void pointerize(unsigned int inst);
void depointerize(unsigned int inst);

void queued_inst_destroy(unsigned int inst);

void deliver_szmsg(int msg, unsigned short parm1, unsigned short parm2);

int on_click_func(unsigned int targ, unsigned int putdown, int, int);

void launch_object(int inst, int lev, int x, int y, 
    int travdir, int tilepos, unsigned short flydist, 
    unsigned short, short, short);
    
void inst_putinside(unsigned int inwhat, unsigned int put_in, int z);
void limbo_instance(unsigned int inst);
void resize_inside(struct inst *in_inst);

int obj_collision(int, int, int, int, unsigned int, int*);
int party_objblock(int ap, int lev, int x, int y);

int check_slave_subtile(int, int);

struct inst_loc *party_extcollision_forward(int ap, int oface, int tx, int ty);
struct inst_loc *party_extcollision_back(int ap, int oface, int tx, int ty);
struct inst_loc *party_extcollision_left(int ap, int oface, int tx, int ty);
struct inst_loc *party_extcollision_right(int ap, int oface, int tx, int ty);
int d_ppos_collision(int ap, unsigned int inst, struct inst *p_inst);

struct inst_loc *fast_il_pushcol(struct inst_loc *PSH,
    struct inst_loc *il, int tx, int ty, int tl, int ddir, int ch);
struct inst_loc *fast_il_genlist(struct inst_loc *PSH,
    struct inst_loc *il, int ddir, int ch);
    
void remove_openshot_on_pushdest(struct dungeon_level *dd, int tx, int ty, int tl);

unsigned int ObjMass(unsigned int I);

void destroy_lua_exvars(char *exvi, unsigned int inst);
void coord_inst_r_error(const char *s_error, unsigned int inst, int);

void clear_method(void);
int determine_method(int ppos, int isl);
void destroy_inst_loc_list(struct inst_loc *il, int umk, int destall);
void regular_swap(unsigned int inst, unsigned int objarch);

int calc_total_light(struct dungeon_level *dd);

unsigned int scanfor_all_owned(int ppos, int arch);

void instmd_queue(int op, int lev, int xx, int yy, int dir,
    int inst, int data);
void inv_instmd_queue(int op, int lev, int xx, int yy, int dir,
    int inst, int data);
    
void exec_flyinst(unsigned int inst, struct inst *p_inst,
    int travdir, int flydist, int damagepower, short delta, short, int instant);
void set_fdelta_to_begin_now(struct inst *p_obj_inst);

int scan_inventory_click(int xx, int yy);

int inst_relocate(unsigned int inst, int level);
void inv_pickup_all(int li_who);
void inv_putdown_all(int li_who);

void initialize_objects(void);

#define LOAD_OBJ_SYSTEM     0x01
#define LOAD_OBJ_USER       0x02
#define LOAD_OBJ_REGISTER   0x04
#define LOAD_OBJ_ALL        0x07
void load_system_objects(int loadparm);

void exchange_pointers(int srcl, int sx, int sy, int desl, int dx, int dy);
void tile_actuator_rotate(int lev, int x, int y, int pos, unsigned int inst);

void flush_instmd_queue(void);
void flush_instmd_queue_with_max(int qmax);
void flush_inv_instmd_queue(void);

int iidxlookup(int integer_index);

void determine_load_color(int who);

void DSB_aa_scale_blit(int doit, BITMAP *source, BITMAP *dest,
    int s_x, int s_y, int s_w, int s_h,
    int d_x, int d_y, int d_w, int d_h);
    
struct animap *setup_animation(int, struct animap *lbmp, struct animap *bmp_table[256], int num_frames, int framedelay);
void inventory_clickzone(int z, int who_look, int subrend);
void process_queued_after_from(unsigned int inst);

