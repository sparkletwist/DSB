#define UDEC(V, D) (V=(V>D)? V-D : 0)

#define XRES 640
#define YRES 480

#define LHTRUE  90998
#define LHFALSE  90999

#define NUM_INST 32768

#define CRAZY_UNDEFINED_VALUE   -98762

// changing this will break a lot of code!!!
#define DSB_DEFAULT_TINT_INTENSITY  64

#define MUSIC_HANDLES   8

#define ATTACK_METHOD_TABLESIZE     16

enum {
    STATE_LOADING,
    STATE_OBJPARSE,
    STATE_DUNGEONLOAD,
    STATE_ISTARTUP,
    STATE_GAMEPLAY,
    STATE_FRONTDOOR,
    STATE_RESUME,
    STATE_QUITTING,
    STATE_CUSTOMPARSE,
    STATE_KEYCONFIG,
    STATE_EDITOR,
    MAX_STATES
};

enum {
    T_FROM_TILE = 0,
    T_TO_TILE,
    T_TRY_MOVE,
    T_ON_TURN,
    T_OFF_TURN,
    T_LOCATION_PICKUP,
    T_LOCATION_DROP
};

#ifndef COLORCONV_KEEP_ALPHA
#define COLORCONV_KEEP_ALPHA \
    COLORCONV_TOTAL & ~( \
        COLORCONV_32A_TO_8  |\
        COLORCONV_32A_TO_15 |\
        COLORCONV_32A_TO_16 |\
        COLORCONV_32A_TO_24)
#endif

#define GP_WINDOW_ON    0x0001
#define GP_NO_PASS      0x0002
#define __DEPRECATED_GP_FREEZELIFE   0x0002
#define GP_PARTYINVIS   0x0004
#define GP_CSB_REINCAR  0x0020
#define GP_END_EMPTY    0x0040
#define GP_NOCLOBBER_DROP   0x0080
#define GP_FAST_ADVANCE 0x0100
#define GP_ONE_WALLITEM 0x0200
#define GP_NO_LAUNCH_TELEPORT   0x0400
#define GP_ZERO_EXP_MOD 0x0800
#define GP_MAX_STAT_BONUS 0x1000
#define GP_GLOWING_CYAN 0x01000000

#define INIOPT_CUDAM    0x01
#define INIOPT_SEMA     0x02
#define INIOPT_STMOUSE  0x04

#define ENFLAG_SPAWNBURST   0x0001
#define __DEPRECATED_ENFLAG_QUEUEOBJAFF  0x1000

#define MAX_LIGHTS 10

#define RUNE_DISABLED       0x01

#define OV_FLOOR    0x0002
#define OV_ROOF     0x0004

#define OVST_ALTFLOOR   0x0020
#define OVST_ALTROOF    0x0040

#define WSF_ALWAYS_DRAW  0x0001

/*
struct sysicons {
    struct animap *eye;
    struct animap *eye_look;
    struct animap *mouth;
    struct animap *r_hand;
    struct animap *r_hand_hurt;
    struct animap *l_hand;
    struct animap *l_hand_hurt;
    struct animap *head;
    struct animap *head_hurt;
    struct animap *torso;
    struct animap *torso_hurt;
    struct animap *legs;
    struct animap *legs_hurt;
    struct animap *feet;
    struct animap *feet_hurt;
    struct animap *neck;
    struct animap *pouch;
    struct animap *quiver;
    struct animap *pack;
    struct animap *blank_attack;
};
*/

#define     DSUB_DO_SUB     0x01
#define     DSUB_USE_BACK   0x02

#define SRF_ACTIVE      0x01
#define SRF_HANDLED     0x02
typedef struct _system_renderer {
    unsigned int flags;
    
    unsigned short rtx;
    unsigned short rty;
    unsigned short sizex;
    unsigned short sizey;
    
    char *renderer_name;
    struct animap *rtarg;
    struct clickzone *cz;
    int cz_n;
} sys_rend;

enum {
    SR_METHODS,
    SR_MAGIC,
    SR_MOVE,
    SR_USER1,
    SR_USER2,
    SR_USER3,
    SR_USER4,
    SR_USER5,
    NUM_SR
};

enum {
    SHO_WALLITEM,
    SHO_FLOORITEM,
    SHO_UPRIGHT,
    SHO_MONSTER,
    SHO_THING,
    SHO_DOOR,
    SHO_CLOUD,
    MAX_SHOTYPES
};

enum {
    SHDIR_FRONT,
    SHDIR_SIDE,
    MAX_SHDIRS
};

enum {
    TICK_NONE,
    TICK_TIMED,
    TICK_FORCED
};

struct graphics_control {
    unsigned char do_subrend;
    unsigned char UNUSED_subrend_bkg; // draw background as a subrenderer, maybe?
    unsigned char XX3;
    unsigned char XX4;
    
    int UNUSED_v[2];
    
    struct animap *l_viewport_rtarg;
    struct alt_targ *l_alt_targ;
    
    unsigned char console_lines;
    unsigned char itemname_drawzone;
    unsigned char UNUSED_c3;
    unsigned char override_flip;
    
    BITMAP *i_subrend;
    struct animap **subrend_targ;
    struct animap *subrend;
    struct animap *statspanel;
    struct animap *food_water;
    struct animap *objlook;
    
    unsigned short curobj_x;
    unsigned short curobj_y;
    unsigned short port_x;
    unsigned short port_y;
    unsigned short con_x;
    unsigned short con_y;
    unsigned short guy_x;
    unsigned short guy_y;
    
    sys_rend SR[NUM_SR];
    sys_rend ppos_r[4];
        
    char *floor_override_name;
    struct animap *floor_override;
    char *roof_override_name;
    struct animap *roof_override;
    
    char *floor_alt_override_name;
    struct animap *floor_alt_override;
    char *roof_alt_override_name;
    struct animap *roof_alt_override;
    
    struct animap *background_image;
    
    unsigned int shade[MAX_SHOTYPES][MAX_SHDIRS][3];
};

struct arch_shadectl {
    unsigned int d[MAX_SHDIRS][3];    
};

struct current_music {
    char *uid;
    char *filename;
    int chan;
    int preserve;
    int XXX_1;
    int XXX_2;
};

#define FRAMECOUNTER_MAX 479001600
#define FRAMECOUNTER_NORUN 0xFFFFFFFF
struct global_data {
    DATAFILE *rdat;
    
    int exestate;

    unsigned char compile;
    unsigned char edit_mode;
    unsigned char double_size;
    unsigned char lua_take_neg_one;
    
    unsigned int session_seed;
    unsigned int rng_index;
    int color_depth;
    
    // global framecounter
    // maxes out at 479001600
    unsigned int framecounter;
    
    // for determing when to load/unload resources
    unsigned int lod_timer;
    
    // still updated when the game is frozen
    unsigned int gui_framecounter;
    
    int scr_show;
    int scr_blit;
    int trip_buffer;     
    
    int uprefix;        // use the same prefix value? (don't src everything 2x)
    char *dprefix;
    char *bprefix;
    
    short vxo;
    short vyo;
    
    short vyo_off;
    short __UNUSED_Y;

    unsigned char coerce_alpha;
    unsigned char loaded_tga;
    unsigned char party_see;
    unsigned char curs_pos;
    
    char *reload_option;

    unsigned char lua_nonfatal;
    unsigned char lua_bool_return;
    unsigned char lua_return_done;
    unsigned char lua_bool_hack;
    
    unsigned char lua_init;
    unsigned char dungeon_loading;
    unsigned char process_tick_mode;
    unsigned char file_lod;
    
    int softframecnt;
    
    unsigned short ini_options;
    unsigned short engine_flags;
    
    unsigned short soundfade;
    unsigned char __NOT_USED_1;
    unsigned char __NOT_USED_2;
    
    int tickclock;
    
    // hack-- temporary stringbuffer
    char *tmpstr;           
    
    int dungeon_levels;
    
    // party data
    int a_party;
    int p_lev[4];
    int p_x[4];
    int p_y[4];
    int p_face[4];
    
    int lightlevel[MAX_LIGHTS];
    
    unsigned short move_lock;
    unsigned char forbid_move_dir;
    unsigned char forbid_move_timer;
    
    // hack to edit the viewstate globally
    // (no longer needed but old code still uses it)
    int *gl_viewstate;
    
    int need_cz;
    
    int viewmode;
    int who_look;
    int whose_spell;
    
    int *glowmask;
    
    struct {
        struct animap *mouse;
        struct animap *mousehand;
        struct animap *mouse_override;
        
        struct animap *UNUSED_full_dmg;
         
        struct animap *control_bkg;
        struct animap *control_long;
        struct animap *control_short;
        
        // special
        BITMAP *no_use;
        
        struct animap *guy_icons;
        struct animap *guy_icons_overlay;
        BITMAP *guy_icons_8;
        
        BITMAP *door_window;
        
        RGB guy_palette[256];
        RGB window_palette[256];
        
        // autogen
        BITMAP *guy[4];
    } simg;
    
    struct current_music cur_mus[MUSIC_HANDLES];
    struct frozen_chan *frozen_sounds;
    
    unsigned int active_szone;
    unsigned int active_czone;
    
    unsigned int lua_error_found;
    
    char ending_boring;
    char ending_v2;
    char ending_v3;
    // hack to prevent duplicate runes
    unsigned char magic_this_frame;
    
    int arch_select;
    unsigned int gameplay_flags;
    
    int *partycond;
    unsigned int *partycond_animtimer;
    int num_pconds;
    int num_conds;
    
    char *attack_msg;
    int attack_dmg_msg;
    int attack_msg_ttl;
    
    int idle_t[4];
    
    // hack for monster/throw clickzones
    int click38;
    
    // used if a certain inst (or -1, if the party)
    // gets affected by a move, even a queued one
    // (Currently set but not actually used)
    int watch_inst;
    unsigned char watch_inst_out_of_position;
    unsigned char WI_2;
    unsigned char WI_3;
    unsigned char WI_4;
    
    char zero_int_result;
    char z_c2;
    char run_everywhere_count;
    char a_tickclock;
    
    char *s_tmpluastr;
    
    int tmp_champ;
    
    int arrowon;
    int litarrow;
    
    int num_champs;
    struct champion *champs;
    
    unsigned char max_class;
    unsigned char max_lvl;
    unsigned char max_UNUSED;
    unsigned char max_nstat;
    
    unsigned char *max_subsk;
    
    void *queued_pointer;
    
    char gui_down_button;
    unsigned char gui_button_wait;
    unsigned char gui_next_mode;
    char gui_v4;
    
    char *barname[3];
    
    unsigned short lastmethod[4][3];
    
    // which champions from "champs" are in the party
    int party[4];
    
    unsigned int playercol[4]; 
    struct animap *playerbitmap[4];
    unsigned int name_draw_col[4];
    unsigned int sys_col;
    
    // who's standing where
    unsigned char guypos[2][2][4];
    int g_facing[4];
    
    int leader;
    int mouseobj;
    
    unsigned char farcloudhack;
    unsigned char noresizehack;
    unsigned char ungrouphack;
    unsigned char hack3;
       
    int mouse_mode;
    int mouse_guy;
    
    unsigned int depointerized_inst;
    
    int infobox;
    
    unsigned char UNUSED_c4[4];
    
    unsigned char max_total_light;
    unsigned char mcv_V2;
    unsigned char mcv_V3;
    unsigned char distort_func;
    
    unsigned short showdam[4];
    unsigned char showdam_t[4]; 
    unsigned char showdam_ttl[4];
    
    int everyone_dead;
    
    unsigned char mouse_hidden;
    unsigned char _mouse_unused_2;
    unsigned char _mouse_unused_3;
    unsigned char _mouse_unused_4;
    
    unsigned short mouse_hcx;
    unsigned short mouse_hcy;
    
    struct gamelock *gl;
    
    int in_handler;
    
    int who_method;
    int num_methods;
    
    unsigned int num_sysarch;
    
    unsigned int max_dtypes;
    
    unsigned short atk_popup;
    unsigned short queue_monster_deaths;
    
    unsigned char t_ypl;
    unsigned char t_cpl;
    unsigned char t_maxlines;
    unsigned char t_UNUSED_Cv_1;
    
    int force_id;
    
    struct obj_aff *queued_obj_aff;    
    int queue_rc;
    int always_queue_inst;

    struct obj_aff *queued_inv_obj_aff;    
    int queue_inv_rc;
    
    int offering_ppos;
    int monster_move_ok;
    int RESERVED_XXX[2];
    int last_alloc;
    
    int method_loc;
    int method_obj;

    struct att_method *cached_lua_method;
    
    int stored_rcv;
    
    int gui_mode;
    
    int filecount;
    
    unsigned int updateclock;
    
    char i_spell[4][8];
    
    unsigned char UNUSEDR3[18][6];

    unsigned char UNUSEDCHAR50[50];
    
    struct att_method *c_method[ATTACK_METHOD_TABLESIZE];
};

#include "callstack.h"
#define DSB_FLAG_FONT       1
#define DSB_FLAG_INTERNAL   2
#define DSB_FLAG_COLLECTION 4
#define DSB_FLAG_VIRTUAL    8

#define DSB_LUA_BITMAP 32
#define DSB_LUA_FONT 33
#define DSB_LUA_I_FONT 35
#define DSB_LUA_WALLSET 36
#define DSB_LUA_VIRTUAL_BITMAP 40

#define DSB_LUA_SOUND 64

#define MAJOR_VERSION 0
#define MINOR_VERSION 69
//#define SUB_VERSION 'C'

enum {
    HIDEMOUSEMODE_SHOW = 0,
    HIDEMOUSEMODE_HIDE,
    HIDEMOUSEMODE_HIDE_OVERRIDE
};
