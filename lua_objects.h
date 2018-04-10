#define SYS_METHOD_OBJ      -1
#define SYS_METHOD_SEL      -2
#define SYS_METHOD_CLEAR    -3
#define SYS_MOVE_ARROW      -4

#define SYS_MAGIC_PPOS      -11
#define SYS_MAGIC_RUNE      -12
#define SYS_MAGIC_BACK      -13
#define SYS_MAGIC_CAST      -14
#define SYS_OBJ_PUT_AWAY    -20
#define SYS_LEADER_SET      -30

//struct animap *find_lua_bitmap(char *tstr, char *req);

void DSBLerror(lua_State *LUA, const char *fmt, ...);
void DSBLerror_nonfatal(lua_State *LUA, const char *fmt, ...);

void register_object(lua_State *LUA, int);
void assign_transation_table_ids(lua_State *LUA);
void pixmap2level(int lev, const char *dfilename, int lite, int xpm);
int call_member_func(unsigned int inst, const char *funcname, int);
int call_member_func2(unsigned int inst, const char *funcname, int ip, int ip2);
int call_member_func3(unsigned int inst, const char *funcname, int ip, int ip2, int);
char *call_member_func_s(unsigned int inst, const char *funcname, int i_parm);
void import_lua_bmp_vars(lua_State *LUA, struct animap *lbmp, int stackarg);

void textmap2level(lua_State *LUA, int lev,
    int xs, int ys, int lite, int xpm);

unsigned char colortable_convert(BITMAP *, int lev, int xx, int yy, unsigned char t);
int setup_colortable_convert(void);
void finish_colortable_convert(void);

void new_lua_cz(unsigned int zoneflags, struct clickzone **x, int *a,
    int, int, int, int, int, unsigned int, unsigned int);
    
void new_lua_extcz(unsigned int zoneflags, struct clickzone **x, int *a,
    int, int, int, int, int, unsigned int, unsigned int, unsigned int);

int ex_exvar(unsigned int inst, const char *vname, int *x);

char *exs_archvar(unsigned int archnum, char *vname);
char *exs_exvar(unsigned int inst, char *vname);
char *exs_alt_exvar(unsigned int inst, const char *vname, const char *alt);

void do_damage_popup(unsigned int ppos, unsigned int d_amt, unsigned char dtype);
void setbarval(unsigned int who, int which, int nval);

int to_tile(int lev, int xx, int yy, int dir, int inst, int onoff);
int i_to_tile(unsigned int inst, int onoff);
int p_to_tile(int ap, int onoff, int v);

struct att_method *import_attack_methods(const char *obj_name);
void destroy_attack_methods(struct att_method *meth);

void enable_inst(unsigned int inst, int onoff);

int queryluapresence(struct obj_arch *p_arch, char *chkstr);
int queryluabool(struct obj_arch *p_arch, char *chkstr);
int queryluaint(struct obj_arch *p_arch, char *chkstr);
int rqueryluaint(struct obj_arch *p_arch, char *chkstr);
char *queryluaglobalstring(const char *name);

void luastacksize(int lsize);
void lstacktop(void);

void setup_lua_bitmap(lua_State *LUA, struct animap *lua_bitmap);

int member_begin_call(unsigned int inst, const char *func);
void member_finish_call(int addargs, int);
int member_retrieve_stackparms(int count, int *dest);

int il_lua_gc(lua_State *LUA);
void inst_loc_to_luatable(lua_State *LUA, struct inst_loc *tteam, int frozenalso, int *adjp);
