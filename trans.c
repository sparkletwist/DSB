#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uproto.h"
#include "trans.h"

struct translate_table Translate;
extern int ZID;

void blank_translation_table(void) {
    memset(&Translate, 0, sizeof(struct translate_table));
}

void purge_translation_table(struct translate_table *tr) {
    int n;
    for (n=0;n<tr->last;++n) {
        dsbfree(tr->e[n].str);
    }
    dsbfree(tr->e);
    tr->e = NULL;
    tr->max = 0;
    tr->last = 0;
}

void translate_enlarge(struct translate_table *tr) {
    tr->max += 16;
    tr->e = dsbrealloc(tr->e, sizeof(t_tbl_entry) * tr->max);
}

void purge_object_translation_table(void) {
    purge_translation_table(&Translate);
}
    
