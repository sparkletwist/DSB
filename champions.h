#define COND_MOUTH_RED      0x01
#define COND_MOUTH_GREEN    0x02
#define COND_EYE_RED        0x04
#define COND_EYE_GREEN      0x08

enum {
    BAR_HEALTH,
    BAR_STAMINA,
    BAR_MANA,
    MAX_BARS
};

enum {
    STAT_STRENGTH,
    STAT_DEXTERITY,
    STAT_WISDOM,
    STAT_VITALITY,
    STAT_ANTIMAGIC,
    STAT_ANTIFIRE,
    STAT_LUCK,
    MAX_STATS
};

enum {
    CLASS_FIGHTER,
    CLASS_NINJA,
    CLASS_PRIEST,
    CLASS_WIZARD,
    MAX_CLASSES
};

#define MAX_LEVELS 16

#define IsNotDead(C) (gd.champs[gd.party[C]-1].bar[0] > 0)

#define _CUSV_DEPRECATED_1  0x01

// flags for inventory_queue_data
// (leave the lower two bytes open)
#define IQD_FILLED      0x00010000
#define IQD_VACATED     0x00020000

struct champion {
    char name[8];
    char lastname[20];
    
    // portrait data
    char *port_name;
    struct animap *portrait;
    
    // customizable stuff:
    char *hand_name;
    struct animap *hand;
    char *invscr_name;
    struct animap *invscr;
    
    // overrides
    char *top_hands_name;
    char *top_port_name;
    char *top_dead_name;
    struct animap *topscr_override[MAX_TOP_SCR];
    
    // bar stats
    int bar[MAX_BARS];
    int maxbar[MAX_BARS];
    
    // other stats
    int stat[MAX_STATS];
    int maxstat[MAX_STATS];
    
    // xp levels for each class
    int *xp;
    int **sub_xp;
    
    // temporary bonuses
    int **xp_temp;
    int **lev_bonus;
    
    // miscellaneous information
    int food;
    int water;
    
    // strength level of the various state changes
    int *condition;
    unsigned int *cond_animtimer;
    
    // weight of crap i'm carrying around
    int load;
    int maxload;
    
    // inventory
    unsigned int *inv;
    unsigned int *inv_queue_data;
    
    // injuries
    unsigned int *injury;
    
    // attack methods
    struct att_method *method;
    char *method_name;              // for reloadability

    // the inst that shows my 3rd person view
    unsigned int exinst;
    
    // custom values
    unsigned char custom_v;
    unsigned char _X2;
    unsigned char _X3;
    unsigned char _X4;
    
    unsigned int unused_value;
};

void calculate_maxload(int lcidx);

void enter_inventory_view(int ch_ppos);
void exit_inventory_view(void);

void enter_champion_preview(int lua_charid);
void exit_champion_preview(int lua_charid, int taken);

void validate_xpclass(int what);
void validate_xplevel(int what);

void party_update(void);
void exec_party_place(int ap, int lev, int xx, int yy, int facedir);

int determine_mastery(int rawsublevel, int who, int maincl, int subcl, int usebonus);
void timer_smash(int who);

void go_to_sleep(void);

void allocate_champion_xp_memory(struct champion *me);
void destroy_champion_xp_memory(struct champion *me);

int forbid_magic(int ppos, int who);
