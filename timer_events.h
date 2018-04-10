enum {
    EVENT_NULL,
    EVENT_MONSTER,
    EVENT_MOVE_FLYER,
    EVENT_COND_UPDATE, 
    EVENT_SEND_MESSAGE, 
    EVENT_DO_FUNCTION,
    EVENT_DO_FUNCTION_SAVED, // currently unimplemented  
    MAX_TIMER_EVENTS
};

enum {
    OPENSHOT_NOTHING,
    OPENSHOT_GENERIC_OBJECT,
    OPENSHOT_MONSTER_OBJECT, // currently unused
    OPENSHOT_PARTY_OBJECT
};

#define DELAY_EOT_TIMER     -5999

#define TE_DECMODE      0x02
#define TE_RUNMODE      0x04

#define TE_FULLMODE     0x06

struct timer_event {    
    int type;
    
    // timer clock
    int time_until;
    int maxtime;
    
    // the instance/target involved
    int inst;
    
    // message type or other misc. data
    // for monsters-- shift timer
    union {
        int data;
        int shift;
    };
    
    // monsters flip occasionally
    // also used as alt data
    union {
        int flip;
        int altdata;
    };
    
    // sender information
    // also used as repetitions for fast fliers
    union {
        unsigned int sender;
        unsigned int flyreps;
    };
    
    // should i delete myself?
    unsigned char delme;  
    // currently unused
    unsigned char pstate;
    unsigned char xdata;
    unsigned char investigate; // for monsters

    struct timer_event *n;    
};


struct condition_desc {
    struct animap *bmp;
    struct animap *pbmp;
    unsigned short baseint;
    unsigned short flags;
    int lua_reg;
};

void reset_monster_ext(struct inst *p_inst);

struct timer_event *add_timer(int inst, int type, int first_time, int do_time,
     int datafield, int flipfield, unsigned int id_sender);
struct timer_event *add_timer_ex(int inst, int type, int first_time, int do_time,
     int datafield, int flipfield, unsigned int id_sender, int ap, int xdata);
     
void delay_all_timers(int inst, int type, int delay);

void run_timers(struct timer_event *cte, struct timer_event **pr, int);
void cleanup_timers(struct timer_event **pr);

void deliver_msg(int inst, int msgtype,
    int datafield, unsigned int id_sender, int ps, int xd);
void deliver_message_buffer(void);
    
void deliver_chained_msgs(struct inst *p_inst, int msgtype, int datafield,
    unsigned int sender, int rc, int ap, int xd);
void msg_chain_to(unsigned int base_id, unsigned int fwd_to, unsigned int chain_reps);
void msg_chain_delink(unsigned int base_id, unsigned int fwd_to);
void destroy_assoc_timer(unsigned int inst, int, int);
void set_inst_flycontroller(int inst, struct inst *p_inst);
void drop_flying_inst(int inst, struct inst *p_inst);
void append_new_queue_te(struct timer_event *ote, struct timer_event *nte);

void set_animtimers_unused(int *cond, int num);
