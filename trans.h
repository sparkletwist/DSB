typedef struct _ttble {
    unsigned int id;
    char *str;
} t_tbl_entry;

struct translate_table {
    int last;
    int max;
    t_tbl_entry *e;
};

void blank_translation_table(void);
void translate_enlarge(struct translate_table *tr);

