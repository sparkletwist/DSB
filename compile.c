#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include "objects.h"
#include "global_data.h"
#include "champions.h"
#include "uproto.h"
#include "compile.h"

extern struct global_data gd;

extern FILE *errorlog;
struct inames *iname_list = NULL;
int iname_entries = 0;

static int write_bin(const char *data_str, 
    void *data, unsigned int dsize, PACKFILE *pf) 
{
    int bindebug = 0;
    
    onstack("write_bin");
    if (bindebug) {
        char compstr[120];        
        snprintf(compstr, 120, "%x, %ld, %x",
            data, dsize, pf);
        stackbackc('(');
        v_onstack(data_str);
        addstackc(')');
        v_onstack("write_bin_data");
        stackbackc('(');
        v_onstack(compstr);
        addstackc(')');
    }
    
    if (pack_fwrite(data, dsize, pf) < dsize) {
        if (bindebug) v_upstack();
        RETURN(0);
    }
    
    if (bindebug) v_upstack();
    RETURN(1);
}

static void *load_bin(const char *filename, unsigned int *fsize) {
    void *mem;
    unsigned int sz;
    PACKFILE *f;

    onstack("load_bin");
    stackbackc('(');
    v_onstack(filename);
    addstackc(')');

    sz = file_size(filename);
    if (sz <= 0) {
        program_puke("Null size");
        RETURN(NULL);
    }

    mem = dsbmalloc(sz);
    f = pack_fopen(filename, F_READ);
    if (!f) program_puke("Open failed");

    if (pack_fread(mem, sz, f) < sz) {
        pack_fclose(f);
        dsbfree(mem);
        program_puke("Read failed");
    }
    pack_fclose(f);
    *fsize = sz;
    
    RETURN(mem);
}

void write_dsbgraphics(void) {
    onstack("write_dsbgraphics");
    compile_dsbgraphics("graphics.dsb", iname_entries, iname_list);
    VOIDRETURN();
}

void add_iname(const char *name, const char *symbolname, const char *ext, const char *longname)
{
    struct inames *n_iname;
    
    onstack("add_iname");
    
    n_iname = dsbcalloc(1, sizeof(struct inames));
    n_iname->path = gd.dprefix;
    n_iname->name = dsbstrdup(name);
    n_iname->sym_name = dsbstrdup(symbolname);
    n_iname->ext = dsbstrdup(ext);
    if (longname)
        n_iname->longname = dsbstrdup(longname);
    else
        n_iname->longname = NULL;
        
    n_iname->n = iname_list;
    iname_list = n_iname;
    
    ++iname_entries;
    
    VOIDRETURN();
}

void compile_dsbgraphics(const char *fname, int num, struct inames *names) {
    char crap[MAX_PATH];
    PACKFILE *f;
    
    onstack("compile_dsbgraphics");
    
    f = pack_fopen(fixname(fname), F_WRITE_NOPACK);
    if (!f) {
        program_puke("Disk write error");
        VOIDRETURN();
    }
        
    pack_mputl(DAT_MAGIC, f);
    pack_mputl(num, f);
    
    while (names != NULL) {
        PACKFILE *sf;
        PACKFILE *cf;
        void *bf;
        int isize;
        char *bin_name;
        struct inames *nextnames;
        int len;
        
        nextnames = names->n;
        
        // debug
        v_onstack("write_file");
        stackbackc('(');
        v_onstack(names->name);
        addstackc(')');
        //
        
        pack_mputl(DAT_PROPERTY, f);
        pack_mputl(DAT_NAME, f);       
        len = strlen(names->sym_name);
        pack_mputl(len, f);
        pack_fwrite(names->sym_name, len, f);
        
        if (names->path) {
            if (names->longname) {
                snprintf(crap, sizeof(crap), "%s%s%s",
                    names->path, "/", names->longname);
            } else {
                snprintf(crap, sizeof(crap), "%s%s%s%s%s",
                    names->path, "/", names->name, ".", names->ext);
            }
            bin_name = crap;
        } else {
            snprintf(crap, sizeof(crap), "%s%s%s",
                names->name, ".", names->ext);
            bin_name = fixname(crap);
        }
               
        pack_mputl(DAT_ID('D','S','B',' '), f);
        bf = load_bin(bin_name, &isize);
        sf = pack_fopen_chunk(f, 1);
        if (!sf) {
            fprintf(errorlog, "@@@ Write chunk fopen FAILED for %s (retrying)\n", names->name);
            sf = pack_fopen_chunk(f, 1);
            if (!sf) {
                poop_out("Chunk write failed for graphics.dsb.\n\n"
                    "This is most likely due to DSB being unable to create\n"
                    "a temporary file. Check if your disk and/or temporary\n"
                    "files directory are full or write protected.");
            }
        }                
        write_bin(bin_name, bf, isize, sf);
        cf = pack_fclose_chunk(sf); 
        if (cf != f) {
            fprintf(errorlog, "??? Save error? %s returned %x != base %x\n",
                names->name, cf, f);
        }
        
        // debug
        v_upstack();
        //
        
        //dsbfree(names->path);
        dsbfree(names->name);
        dsbfree(names->ext);
        dsbfree(names->longname);
        dsbfree(names);
        names = nextnames;    
    }
    
    pack_fclose(f);
    
    VOIDRETURN();
}   

void splitformatdup(const char *aname, char **rname, char **ext) {
    int i = 0;
    int dots = 0;
    const char *inname;
    char *outname;
    
    onstack("splitformatdup");
    
    inname = aname;
    while (*inname != '/') {
        ++inname;
    }
    ++inname;
    outname = dsbstrdup(inname);
  
    while (outname[i] != '\0') {
        unsigned char cx = toupper(outname[i]);
        
        if (outname[i] == '.') {
            dots++;
            if (UNLIKELY(dots > 1)) {
                char errx[256];
                snprintf(errx, sizeof(errx),
                    "Too many dots in %s", aname);
                poop_out(errx);
                VOIDRETURN();
            }
            
            outname[i] = '\0';
            *ext = dsbstrdup(&(outname[i+1]));
            
        } else
            outname[i] = cx;
        
        i++;
    }
    
    *rname = outname;    
    VOIDRETURN();
}
