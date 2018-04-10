enum {
    LOCK_ACTUATORS,
    LOCK_MOVEMENT,
    LOCK_MOUSE,
    LOCK_MAGIC,
    LOCK_INVENTORY,
    LOCK_ATTACK,
    LOCK_FLYERS,
    LOCK_CONDITIONS,
    LOCK_MESSAGES,
    LOCK_ALL_TIMERS,
    MAX_LOCKS
};

struct gamelock {
    unsigned char lockc[MAX_LOCKS];
};
