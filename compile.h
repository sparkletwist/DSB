/* CHECKING FOR:
---DSB ESB--
*/
#define KEY_VAL_1 0x2D2D2D44
#define KEY_VAL_2 0x53422045
#define KEY_VAL_3 0x53422D2D


struct inames {
    char *path;
    char *name;
    char *sym_name;
    char *ext;
    char *longname;
    struct inames *n;
};

void compile_dsbgraphics(const char *fname, int num, struct inames *names);
void splitformatdup(const char *aname, char **rname, char **ext);
void write_dsbgraphics(void);
void add_iname(const char *name, const char *symbolname, const char *ext, const char *longname);
