struct eextchar {
    char *desg;
    int crap[5];
};

void destroy_hash(void);

int lgetarch(const char *cs_arch);

int inst_moveup(unsigned int inst);

void unreflua(int ref);

int ed_calculate_mon_hp(struct inst *p_inst);
void destroy_dungeon_level(struct dungeon_level *dd);

void ed_grab_exvars_for(int inst, HWND hwnd, int p);
void ed_grab_ch_exvars_for(int ch, HWND hwnd, int p);
