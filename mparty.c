#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "sound.h"
#include "global_data.h"
#include "champions.h"
#include "uproto.h"
#include "gproto.h"
#include "monster.h"
#include "istack.h"
#include "mparty.h"

extern struct global_data gd;
extern struct inventory_info gii;
extern struct inst *oinst[NUM_INST];
extern struct dungeon_level *dun;
extern istack istaparty;

extern FILE *errorlog;

unsigned int Gmparty_flags = 0;
int PPis_here[4] = {0, 0, 0, 0};

void change_partyview(int nap) {
    int cap = gd.a_party;
    
    onstack("change_partyview");
    
    if (!GMP_ENABLED())
        VOIDRETURN();

    if (cap == nap)
        VOIDRETURN();

    if (gd.p_lev[nap] < 0)
        VOIDRETURN();

    gd.a_party = nap;
    set_3d_soundcoords();
    if (gd.p_lev[cap] != gd.p_lev[nap])
        party_viewto_level(gd.p_lev[nap]);
        
    if (gd.who_method != (gd.leader+1))
        clear_method();

    if (gd.whose_spell != gd.leader)
        gd.whose_spell = gd.leader;
        
    mparty_who_is_here();
    
    lc_parm_int("sys_change_view", 1, nap);
    
    VOIDRETURN();
}

void mparty_who_is_here(void) {
    int xp, yp;
    int ap = gd.a_party;
    
    if (!GMP_ENABLED()) {
        PPis_here[0] = 1;
        PPis_here[1] = 1;
        PPis_here[2] = 1;
        PPis_here[3] = 1;   
    }     
    
    PPis_here[0] = 0;
    PPis_here[1] = 0;
    PPis_here[2] = 0;
    PPis_here[3] = 0;

    for (yp=0;yp<2;++yp) {
        for (xp=0;xp<2;xp++) {
            int gp = gd.guypos[yp][xp][ap];
            if (gp)
                PPis_here[gp - 1] = 1;
        }
    }
    
}

int mparty_containing(int ppos) {
    int xp, yp, ap;
    
    if (!GMP_ENABLED()) {
        return 0;
    }

    for (yp=0;yp<2;++yp) {
        for (xp=0;xp<2;xp++) {
            for (ap=0;ap<4;ap++) {
                int gp = gd.guypos[yp][xp][ap];
                if (gp == (ppos+1))
                    return ap;
            }
        }
    }

    return -1;
}

int mparty_split(int splitter, int op) {
    int n_party = -1;
    int i_t_here;
    int n;
    
    onstack("mparty_split");
    
    // one-member parties do not split
    i_t_here = PPis_here[0] + PPis_here[1] + PPis_here[2] + PPis_here[3];
    if (i_t_here <= 1)
        RETURN(op);
        
    // bad leader information = no split
    if (splitter < 0)
        RETURN(op);
    
    for (n=0;n<4;++n) {
        if (gd.p_lev[n] == LOC_LIMBO) {
            gd.p_face[n] = gd.p_face[op];
            mparty_transfer_member(splitter, op, n);
            n_party = n;
            break;
        }
    }
    
    if (n_party == -1) {
        recover_error("Cannot split: all parties in use");
        RETURN(-1);
    }
    
    gd.p_lev[n_party] = gd.p_lev[op];
    gd.p_x[n_party] = gd.p_x[op];
    gd.p_y[n_party] = gd.p_y[op];
    change_partyview(n_party);
    
    RETURN(n_party);
}

void mparty_merge(int alive) {
    int n;
    int lev, x, y;
    
    onstack("mparty_merge");
    
    lev = gd.p_lev[alive];
    x = gd.p_x[alive];
    y = gd.p_y[alive];
    
    for(n=0;n<4;++n) {
        if (alive == n)
            continue;
            
        if (gd.p_lev[n] == lev) {
            if (gd.p_x[n] == x && gd.p_y[n] == y) {
                mparty_transfer_all(n, alive);
                gd.p_lev[n] = LOC_LIMBO;
            }
        }
    }

    ist_push(&istaparty, alive);    
    lc_parm_int("sys_change_view", 1, alive);
    ist_pop(&istaparty);
    
    VOIDRETURN();
}

void mparty_integrity_check(int sp, int face) {
    int i_t_here;
    
    onstack("mparty_integrity_check");
    
    mparty_who_is_here();
    i_t_here = PPis_here[0] + PPis_here[1] + PPis_here[2] + PPis_here[3];
    if (i_t_here == 0) {
        change_partyview(sp);
        gd.p_face[sp] = face;
    }
    
    VOIDRETURN();
}

int mparty_destroy_empty(int cp) {
    int i, j;
    int nactive = 0;
    
    onstack("mparty_destroy_empty");
    
    for(i=0;i<2;++i) {
        for(j=0;j<2;++j) {
            if (gd.guypos[i][j][cp])
                ++nactive;
        }
    }
    
    if (!nactive) {
        gd.p_lev[cp] = LOC_LIMBO;
        RETURN(1);
    }
    
    RETURN(0);
}

void read_mparty_flags(PACKFILE *pf) {
    onstack("read_mparty_flags");
    Gmparty_flags = pack_igetl(pf);
    VOIDRETURN();
}

void write_mparty_flags(PACKFILE *pf) {
    onstack("write_mparty_flags");
    pack_iputl(Gmparty_flags, pf);
    VOIDRETURN();
}

void mparty_transfer(int from_p, int xx, int yy, int to_p, int x2, int y2) {
    gd.guypos[y2][x2][to_p] = gd.guypos[yy][xx][from_p];
    gd.guypos[yy][xx][from_p] = 0;
}

void mparty_transfer_member(int ppos, int from_p, int to_p) {
    int appos = ppos + 1;
    int i, j;
    
    onstack("mparty_transfer_member");
    
    for (i=0;i<2;++i) {
        for (j=0;j<2;++j) {
            if (gd.guypos[i][j][from_p] == appos) {
                if (!gd.guypos[i][j][to_p]) {
                    mparty_transfer(from_p, j, i, to_p, j, i);
                } else {
                    gd.guypos[i][j][from_p] = 0;
                    put_into_pos(to_p, appos);
                }
                
                break;
            }
        }
    }
    
    VOIDRETURN();
}

void mparty_transfer_all(int from_p, int to_p) {
    int i, j;
    
    onstack("mparty_transfer_all");
    
    for (i=0;i<2;++i) {
        for (j=0;j<2;++j) {
            if (gd.guypos[i][j][from_p]) {
                int appos = gd.guypos[i][j][from_p];
                if (!gd.guypos[i][j][to_p]) {
                    mparty_transfer(from_p, j, i, to_p, j, i);
                } else {
                    gd.guypos[i][j][from_p] = 0;
                    put_into_pos(to_p, appos);
                }
            }
        }
    }
    
    VOIDRETURN();
}

int party_ist_peek(int def) {
    return (ist_opeek(&istaparty, def));
}
