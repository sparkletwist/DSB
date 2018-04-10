#define onstack(V) int I_STACK_LEVEL = cstacklev();v_onstack(V)
#define upstack(V) v_upstack(V)
#define luaonstack(V) int I_STACK_LEVEL = cstacklev();v_luaonstack(V)

#define RETURN(x)   {CHECKOUT(I_STACK_LEVEL);upstack();return(x);}
#define VOIDRETURN()    {CHECKOUT(I_STACK_LEVEL);upstack();return;}

# define LIKELY(x)	__builtin_expect (!!(x), 1)
# define UNLIKELY(x)	__builtin_expect (!!(x), 0)

int cstacklev();
void init_stack();
void v_onstack(const char *on);
void v_luaonstack(const char *on_str);
void v_upstack();
void stackbackc(char c);
void addstackc(char c);

void fix_inst_int(void);

void CHECKOUT(int sl);
