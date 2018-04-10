#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "callstack.h"
#include "istack.h"
#include "memory.h"

extern char Ex_Heap[HEAPSIZE];
extern istack istaparty;
extern FILE *errorlog;

void dsb_mem_maintenance(void) {
    int i;

    onstack("dsb_mem_maintenance");

    for (i = 0; i < HEAPSIZE; i += sizeof(int)) {
        int *Z = (int*)&(Ex_Heap[i]);
        *Z = 0xDDEEFF82;
    }

    ist_cleanup(&istaparty);
    full_lod_use_check();

    VOIDRETURN();
}
