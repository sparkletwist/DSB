#define VIEWPORTW 448
#define VIEWPORTH 272

void setup_background(BITMAP *scx, int forceblack);

struct animap *get_fly_view(int o_type, struct obj_arch *p_obj_arch, 
    struct inst *p_inst, int sm, int vsm, int p_face, int flags, int *flip);
    
struct animap *get_rend_view(int o_type, struct obj_arch *p_obj_arch, 
    int flag, int sm, int vsm, int *p_xhack, int *info_flags);
    
struct animap *get_monster_view(struct inst *p_inst,
    struct obj_arch *p_arch, int sm, int p_face, int tflags, int *info_flags);
    
void adv_inst_and_cond_framecounters(void);
void adv_crmotion_with_missing(int m);
void call_lua_inventory_renderer(BITMAP *scx, int lwho, int bx, int by, int namedraw);
void call_lua_top_renderer(struct animap *rtarg, int cstate, int ppos, int lwho, int here, const char *port_name, int *p_x, int *p_y);
void call_lua_damage_renderer(struct animap *rtarg, int cstate, int ppos, int lwho);
int find_targeted_csr(struct animap *sbmp, int xx, int yy, struct clickzone ***p_lcz, int **p_lczn, int *p_cx, int *p_cy);
int find_associated_csr_ppos(struct animap *sbmp, int csr, int csr_found);
int find_valid_czn(struct clickzone *p_lcz, int lczn);
