#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "callstack.h"
#include "istack.h"
#include "memory.h"

#define HEAPSIZE 120000
char Ex_Heap[HEAPSIZE];

extern FILE *errorlog;

static void *DSB_ALLOCATOR(size_t size) {
    return malloc(size);
}

void *dsbrealloc(void *iptr, size_t size) {
    void *ptr;
    
    ptr = realloc(iptr, size);
    if (ptr != NULL) {
        return ptr;
    }
    
    fprintf(errorlog, "Realloc of %d (at %p) failed\n", size, iptr);
    fflush(errorlog);
    
    program_puke("Realloc failed");
    return NULL;
}

void *dsbmalloc(size_t size) {
    void *ptr;

    ptr = DSB_ALLOCATOR(size);
    if (ptr != NULL) {
        return ptr;
    }
    
    fprintf(errorlog, "@@@ Malloc of %d failed\n", size);
    fflush(errorlog);
    
    program_puke("Malloc failed");
    return NULL;
}       

void *dsbcalloc(size_t nmemb, size_t size) {
    void *ptr;

    ptr = DSB_ALLOCATOR(nmemb * size);
    if (ptr != NULL) {
        memset(ptr, 0, nmemb * size);
        return ptr;
    }

    fprintf(errorlog, "@@@ Calloc of %d * %d failed\n", nmemb, size);
    fflush(errorlog);

    program_puke("Calloc failed");
    return NULL;
}

char *dsbstrdup_nocallstack(const char *str) {
    int len;
    char *ptr;
    
    if (str == NULL) {
        return NULL;
    }
    
    len = strlen(str) + 1;
    ptr = (char*)DSB_ALLOCATOR(len);

    if (ptr != NULL) {
        memcpy(ptr, str, len);
        return ptr;
    }
    
    fprintf(errorlog, "@@@ Strdup of [%s] failed\n", str);
    fflush(errorlog);
    
    program_puke("Strdup failed");
    return NULL;
}

char *dsbstrdup(const char *str) {
    char *rv;
    onstack("dsbstrdup");
    rv = dsbstrdup_nocallstack(str);
    RETURN(rv);
}


void dsbfree(void *mem) {
    free(mem);
}

/* Experimental memory heap */
// do not use this on anything that you
// expect to stick around outside of your
// one function call
void *dsbfastalloc(size_t size) {
    static char *baseptr = Ex_Heap;
    static char *hptr = Ex_Heap;
    char *t;
    
    t = hptr;
    hptr += (size+1);
    
    if (hptr >= baseptr+HEAPSIZE) {
        t = baseptr;
        hptr = (baseptr+size+1);
    }
    
    return (void*)t;
}

// not necessary
void condfree(void *ptr) {
    dsbfree(ptr);
}
    
    
