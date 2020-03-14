#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "callstack.h"
#include "htext.h"

extern int debug;
extern FILE *errorlog;

Htextbuf *read_and_break(int directread, const char *fb, int maxlinelen) {
    Htextbuf *txtlines = NULL;
    Htext *basetl = NULL;
    Htext *cln = NULL;
    int bufptr = 0;
    int buflen = 4096;
    char *buf;
    FILE *rf;
    
    onstack("read_and_break");
    
    if (!directread) {
        const char *filename = fb;
        rf = fopen(filename, "r");
        if (!rf) {
            if (debug) {
                fprintf(errorlog, "LOCALTEXT: Could not open %s\n", filename);
            }
            
            RETURN(NULL);
        }
    }

    buf = (char*)malloc(buflen);

    if (directread) {
        int iptr = 0;
        while (fb[iptr] != '\0') {
            buf[bufptr] = fb[iptr];
            ++bufptr;
            ++iptr;
            if (bufptr == buflen) {
                buflen *= 2;
                buf = (char*)realloc(buf, buflen);
            }
        }
    } else {
        char c;
        while ((c = fgetc(rf)) != EOF) {
            int repeats = 1;

            while (repeats > 0) {
                buf[bufptr] = c;
                ++bufptr;
                if (bufptr == buflen) {
                    buflen *= 2;
                    buf = (char*)realloc(buf, buflen);
                }
                repeats--;
            }

        }
    }
    buf[bufptr] = '\0';

    if (maxlinelen <= 0)
        maxlinelen = buflen;

    int i = 0;
    int clinelen = 0;
    int linestart_i = 0;
    int last_i = 0;
    int numlines = 0;
    while (i < buflen) {
        char cc = buf[i];

        if (cc == ' ' || cc == '\n' || cc == '\0') last_i = i;

        if (cc == '\n' || cc == '\0' || clinelen == maxlinelen) {
            int eat = 1;
            Htext *ntxt = malloc(sizeof(Htext));
            memset(ntxt, 0, sizeof(Htext));

            if (clinelen == maxlinelen) {
                if (last_i == -1) {
                    eat = 0;
                } else {
                    clinelen = last_i - linestart_i;
                }
            }

            ntxt->t = malloc(sizeof(char)*(clinelen+1));
            memcpy(ntxt->t, &(buf[linestart_i]), clinelen+1);
            ntxt->t[clinelen] = '\0';
            
            if (!basetl) {
                basetl = cln = ntxt;
            } else {
                cln->n = ntxt;
                cln = ntxt;
            }
            ++numlines;

            if (cc == '\0')
                break;

            i = linestart_i + clinelen + eat;
            last_i = -1;
            linestart_i = i;
            clinelen = 0;

        } else {
            clinelen++;
            i++;
        }
    }
    free(buf);

    if (!directread)
        fclose(rf);

    txtlines = calloc(1, sizeof(Htextbuf));
    txtlines->numlines = numlines;
    txtlines->tl = basetl;
   
    RETURN(txtlines);
}

void destroy_textbuf(Htextbuf *d) {
    onstack("destroy_textbuf");
    
    while (d->tl) {
        Htext *d_tl_n = d->tl->n;
        free(d->tl->t);
        free(d->tl);
        d->tl = d_tl_n;
    }
    free(d);
        
    VOIDRETURN(); 
}

