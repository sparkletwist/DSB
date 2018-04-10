#define V_DVIEW     0x0000
#define V_INVIEW    0x0010
#define V_OUTVIEW   0x0020
#define V_SIDEVIEW  0x0030
#define V_EXTVIEW   0x0040
#define V_ATTVIEW   0x0100

struct animap *do_dynamic_scale(int sm, struct obj_arch *p_arch,
    int view, int blocker);

void invalidate_scale_cache(struct obj_arch *p_arch);
