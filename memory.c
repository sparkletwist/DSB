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
    
    onstack("dsbrealloc");
    
    ptr = realloc(iptr, size);
    if (ptr != NULL) RETURN(ptr);
    
    fprintf(errorlog, "Realloc of %d (at %p) failed\n", size, iptr);
    fflush(errorlog);
    
    program_puke("Realloc failed");
    RETURN(NULL);
}

void *dsbmalloc(size_t size) {
    void *ptr;
    
    onstack("dsbmalloc");
    
    ptr = DSB_ALLOCATOR(size);
    if (ptr != NULL) RETURN(ptr);
    
    fprintf(errorlog, "@@@ Malloc of %d failed\n", size);
    fflush(errorlog);
    
    program_puke("Malloc failed");
    RETURN(NULL);
}       

void *dsbcalloc(size_t nmemb, size_t size) {
    void *ptr;

    onstack("dsbcalloc");

    ptr = DSB_ALLOCATOR(nmemb * size);
    if (ptr != NULL) {
        memset(ptr, 0, nmemb * size);
        RETURN(ptr);
    }

    fprintf(errorlog, "@@@ Calloc of %d * %d failed\n", nmemb, size);
    fflush(errorlog);

    program_puke("Calloc failed");
    RETURN(NULL);
}

char *dsbstrdup(const char *str) {
    int len;
    char *ptr;
    
    onstack("dsbstrdup");
    
    if (str == NULL) {
        RETURN(NULL);
    }
    
    len = strlen(str) + 1;
    ptr = (char*)DSB_ALLOCATOR(len);

    if (ptr != NULL) {
        memcpy(ptr, str, len);
        RETURN(ptr);
    }
    
    fprintf(errorlog, "@@@ Strdup of [%s] failed\n", str);
    fflush(errorlog);
    
    program_puke("Strdup failed");
    RETURN(NULL);
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
    
    
