enum {
    AC_BASE = 0,
    AC_WALLITEM,
    AC_FLOORITEM,
    AC_UPRIGHT,
    AC_MONSTER,
    AC_THING,
    AC_DOOR,
    AC_CLOUD,
    AC_HAZE,
    AC___RESERVED_LIB,
    AC___RESERVED_EXT,
    ARCH_CLASSES
};

struct S_arches {
    struct obj_arch *ac[ARCH_CLASSES];
};

struct obj_arch *Arch(unsigned int anum);

int objarchctl(int req, int ac);
struct animap *getluaoptbitmap(char *xname, char *nam);
