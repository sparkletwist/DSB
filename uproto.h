enum {
    PCC_ACTUALMOVE = 0,
    PCC_MOVEOFF,
    PCC_MOVEON
};

char *fixname(const char *file_name);
char *bfixname(const char *file_name);
void scfix(char);

void DSBallegshutdown(void);
void DSBkeyboard_name_input(void);
void DSBkeyboard_gamesave_input(void);

void init_all_objects(void);

void show_introduction(void);
int show_the_end_screen(int);
int show_ending(int);
int show_front_door(void);
void show_load_ready(int rtp_only);

char *pick_savefile_zone(int z);
int pick_inamei_zone(int z);

void init_fonts(void);

void initlua (char *filename);
int src_lua_file(char *f_x, int opt);
//void literal_luacmd(char *lcmd);
void literal_pcall(char *lcmd);

void program_puke(char *reason);
void recover_error(char *reason);

void recursion_puke(void);
void puke_bad_subscript(char *var, int badsub);
void poop_out(char *errmsg);
void sigcrash(int signal);

void *dsbcalloc(size_t num, size_t size);
void *dsbmalloc(int size);
void *dsbrealloc(void *p, size_t size); 
void *dsbfastalloc(size_t size);
char *dsbstrdup(const char *str);
void condfree(void *ptr);

void face2delta(int face, int *dx, int *dy);
int check_for_wall(int lev, int x, int y);

int lc_parm_int(const char *fname, int parms, ...);
int lc_call_topstack(int parms, const char *fname); 
void lc_call_topstack_two_results(int parms, const char *fname, int *p1, int *p2); 

void party_enter_level(int ap, int lev);
void party_viewto_level(int lev);
void change_partyview(int nap);

void got_mouseclick(int b, int xx, int yy, int viewstate);
void got_mkeypress(int type, int key);
void obj_at(int inst, int lev, int xx, int yy, int dir);
void exec_lua_rearrange(int mg, int ox, int oy, int nx, int ny);
void ppos_reassignment(int ap, int oppos, int nppos, int ox, int oy, int nx, int ny);

void import_lua_constants(void);
void import_interface_constants(void);
void activate_gui_zone(char *dstr, int zid, int x, int y, int w, int h, int flags);
void internal_gui_command(const char cmd, int val);
void internal_shadeinfo(unsigned int arch, int otype, int odir, int d1, int d2, int d3);
void destroy_dynalloc_constants(void);

void clear_lua_cz(void);
void init_t_events(void);

void inst_destroy(unsigned int inst, int);

void init_console(void);
void destroy_console(void);

void console_system_message(const char *textmsg, int col);
void update_console(void);
void update_all_inventory_info(void);
void update_all(void);

void manifest_files(int, int comp);

int sum_load(unsigned int);
void put_into_pos(int ap, int i);
void remove_from_pos(int ap, int i);
int remove_from_all_pos(int i);
void find_remove_from_pos(int ap, int i, int *rx, int *ry);
int find_remove_from_pos_tolerant(int ap, int i, int *rx, int *ry);
int change_leader_to(int nleader);

void party_moveto(int ap, int lev, int xx, int yy, int lk, int pcc);
void give_xp(unsigned int cnum, unsigned int wclass, unsigned int, unsigned int xpamt); 

void change_facing(int ap, int olddir, int newdir, int);
int tileshift(int in_tile, int shift_by, int *chg);
int shiftdirto(int from_t, int to_t);

int levelxp(int level);

void add_export_gvar(char *s_vname);
void register_lua_objects();
void import_shading_info(void);

int systemtimer(void);
int getsleeptime(void);

int check_for_savefile(const char *filename, char **fidpt, char *writeinto);
int load_savefile(const char *fname);
void save_savefile(const char *fname, const char *gname);
void destroy_dungeon();

void lockflags_change(unsigned int lockflags, int b_set);
int lockflags_get(unsigned int bitflag);

unsigned int DSBrand();
int DSBtrivialrand(void);
int DSBprand(int, int);
int DSBsrand(unsigned int);
int srand_to_session_seed(void);
void init_lua_soundtable(void);
void destroy_all_sound_handles(void);
void destroy_all_music_handles(void);
void memset_sound(void);

void freeze_sound_channels(void);
void unfreeze_sound_channels(void);
void check_sound_channels(void);
void halt_all_sound_channels(void);

void interface_click(void);
void init_interface_click(void);

void psxf(const char *upd);
int selectdungeondir(void);

int is_game_exportable(void);
void store_exported_character_data(void);
void retrieve_exported_character_data(void);
void load_export_target(void);

void collect_lua_garbage(void);
void collect_lua_incremental_garbage(int steps);
void dsb_mem_maintenance(void);
void global_stack_dump_request(void);

int decode_character_value(const char *buf, short num);

void party_composition_changed(int ap, int pcc, int real_lev);

const char *make_noslash(const char *l_str);

void dsb_clear_keybuf(void);
void dsb_clear_immed_key_queue(void);
void dsb_shift_immed_key_queue(void);
