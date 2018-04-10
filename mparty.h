#define MPF_SPLITMOVE       0x0001
#define MPF_COLLIDE         0x0002
#define MPF_ENABLED         0x0100

#define GMP_ENABLED() (Gmparty_flags & MPF_ENABLED)

int mparty_containing(int ppos);
void mparty_who_is_here(void);
void read_mparty_flags(PACKFILE *pf);
void write_mparty_flags(PACKFILE *pf);
void mparty_merge(int alive);
int mparty_split(int splitter, int op);
void mparty_integrity_check(int sp, int face);
int mparty_destroy_empty(int cp);

void mparty_transfer_member(int ppos, int from_p, int to_p);
void mparty_transfer_all(int from_p, int to_p);

int party_ist_peek(int def);
