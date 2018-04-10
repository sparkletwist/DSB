enum {
    PRESS_MOVEKEY,
    PRESS_INVKEY,
    PRESS_MAGICKEY,
    PRESS_METHODKEY,
    PRESS_INTERFACEKEY,
    _UNUSED_B,
    PRESS_MOUSELEFT,
    PRESS_MOUSERIGHT,
    PRESS_MOUSEMIDDLE,
    MAX_PRESS
};

enum {
    MOVE_UP,
    MOVE_RIGHT,
    MOVE_BACK,
    MOVE_LEFT,
    MOVE_TURNLEFT,
    MOVE_TURNRIGHT,
    MAX_MOVE
};

struct movequeue {
    int type;
    int x;
    int y;
    struct movequeue *n;
};

#define KEYQUEUE_SIZE 200

void initmovequeue(void);
void addmovequeue(int t, int m, int y);
void addmovequeuefront(int t, int m, int y);
int popmovequeue(int *t, int *m, int *y);
void destroymovequeue(void);
