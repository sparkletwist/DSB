enum {
    FT_UNUSED,
    FT_FLOOR,
    FT_NEGATIVELEVEL
};

struct burst_obj {

    unsigned char f_type;
    unsigned char c_v2;
    unsigned char c_v3;
    unsigned char c_v4;
    
    unsigned int inst;
    
    struct burst_obj *n;
};

void terminate_spawnburst(void);
void execute_spawn_events(void);
void sb_to_queue(unsigned int inst, int level);
