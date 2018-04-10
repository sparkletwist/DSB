typedef struct __ISTACK {
    int max;
    int top;
    struct __ISTACK *istack_cap_top;
    int *e;
} istack;

void ist_init(istack *ist);
void ist_push(istack *ist, int ne);
int ist_opeek(istack *ist, int idefv);
int ist_pop(istack *ist);
void ist_cleanup(istack *ist);
void ist_cap_top(istack *ist);
void ist_check_top(istack *ist);
void init_all_ist(void);
void flush_all_ist(void);
