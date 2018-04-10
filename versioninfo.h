// the loader is hardcoded to reject all nonzero values
#define CURRENT_FILESPEC    0

#define DXC_TINTVALUE           0x01
#define DXC_VARIABLECONSOLE     0x02
#define DXC_DUNGEON_FLAGS       0x04

#define DSB_CURRENT_EXTENDED_CAPABS     0x07

// this was previously always 64, each greater
// value breaks binary backward compatibility
#define BACKCOMPAT_REVISION 65

// This must be 0 in release versions!
#define CURRENT_BETA 0
// dsbtest2         1
// dsbtest3         2
// dsbtest4         3
// dsbtest5         4
// dsbtest6         5
// dsbtest7         6
// dsbtest8         7
// dsbtest8a        8
// dsbtest9         9
// dsbtest10        10
// dsbtest11/12     11
// dsbtest13/14     12
// dsbtest15/16     13
// dsbtest17        14
// dsbtest18        15
// dsbtest19        16
// dsbtest20        17
// dsbtest21        18
// dsbtest22        19
// dsbtest23        20
// dsbtest24/25     21
// dsbtest26        22
// dsbtest27        23
// dsbtest28        24
// dsbtest29/30     25
// dsbtest31        26
// dsbtest32        0   - trying to start compatibility!
// dsbdebug37       27
// dsbdebug42       28  - WAS SET TO THIS FOR FAR TOO LONG
// dsbdebug49       29
