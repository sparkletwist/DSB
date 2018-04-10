struct ai_core {
    int state;
    
    struct timer_event *controller;
    
    int hp;
    int maxhp;
    
    unsigned char sface;
    unsigned char no_move;
    unsigned char see_party;
    unsigned char attack_anim;
    
    int fear;
    int i_V1;
    
    // currently unused, available for something
    short X_lcx;
    short X_lcy;
    
    // who is my boss? or am i the boss
    int boss;
    
    unsigned char d_hp; //dynamic hp
    unsigned char XXX3;
    unsigned char XXX4;
    unsigned char wallbelief;
    
    unsigned short action_delay;
    unsigned short UNUSED_delay;

    unsigned int ai_flags;
    
    int _FILL[13];
};

enum {
    MOVE_NORMAL,
    MOVE_PARTY,
    MOVE_FEAR
};

#define SIGHT_TARG          1
#define SIGHT_CLEAR_SHOT    2
#define SIGHT_DOOR          4
#define SIGHT_DOOR_BARS     8

#define AI_P_QUERY      -6000

#define AI_FEAR         1
#define AI_DELAY_ACTION 2
#define AI_DELAY_TIMER  12
#define AI_STUN         3
#define AI_HAZARD       4
#define AI_WALL_BELIEF  5
#define AI_ATTACK_BMP   6
#define AI_DELAY_EVERYTHING 7
#define AI_MOVE_NOW     16
#define AI_SEE_PARTY    32
#define AI_TARGET       33
#define AI_UNGROUP      34
#define AI_MOVE         512
#define AI_TURN         513
#define AI_TIMER        514

#define AIF_NOFASTMOVE      0x0001
#define AIF_TOOK_ATTACK     0x0002
#define AIF_HAZARD          0x0004
#define AIF_UNGROUPED       0x0008
#define AIF_PARTY_SEE       0x1000
#define AIF_MOVE_NOW        0x8000

#define _FIRST_PERM_AIF     0x010000
#define AIF_DEAD            0x010000

#define ROTATE_2PRE         0x1
#define ROTATE_POST         0x2
#define ROTATE_FULL         0x3

int get_slave_subtile(struct inst *p_inst);
void spawned_monster(unsigned int inst, struct inst *p_m_inst, int);
void destroyed_monster(struct inst *p_m_inst);

int monster_acts(unsigned int inst);
void monster_investigates(unsigned int inst);
int monster_telefrag(int lev, int xx, int yy);

void defer_kill_monster(unsigned int inst);
void kill_monster(unsigned int inst);
void kill_all_walking_dead_monsters();

int ai_msg_handler(unsigned int inst, int ai_msg, int parm, int);

struct inst_loc *monster_groupup(unsigned int l_inst, struct inst *leader,
    unsigned int *icount, int take_all);
    
int monvalidateboss(struct inst *p_m_inst, int bossnum);
void mon_move(unsigned int inst, int lev, int x, int y, int ndir, int isboss, int frozenalso);
int mon_rotate2wide(unsigned int inst, struct inst *pb, int rdir, int b_tele, int b_instantturn);
