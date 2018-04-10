#define KEY_STRING  0x44534241
#define KEY_TR_STRING  0x445452FF


#define D_ISINT     1
#define D_ISSTR     2
#define D_ISBOOL    8

#define D_ISSTBL    32
#define D_ENDSTBL   128

#define D_INVALID   254

#define CUST_EXTEND     0x01    // use if we need an extended spec
#define CUST_HAND     0x02
#define CUST_INVSCR   0x04
#define CUST_TOP_HANDS  0x10
#define CUST_TOP_PORT   0x20
#define CUST_TOP_DEAD   0x40
#define EMPTY_CHAMPION   0x80

#define EXD_CHAINTARG   128
#define EXD_AI_CORE     64

#define wrl(X)  pack_iputl(X, pf)
#define wrc(X)  pack_putc(X, pf)
#define rdl(X)  (X = pack_igetl(pf))
#define rdc(X)  (X = pack_getc(pf))

#define Mwrl(X)  pack_mputl(X, pf)
#define Mrdl(X)  (X = pack_mgetl(pf))
