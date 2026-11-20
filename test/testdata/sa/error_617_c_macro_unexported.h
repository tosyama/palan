#define FMAC(x) ((x) ? 1 : 0)
#define NOFOLD  (1 < 2)
#define UNRES(x) (nowhere(x))
#define ONE(x) ((x) + 1)
#define NOBODY(x) int x;
#define GLOBAL() (stdout)
typedef _Complex float cf_t;
#define BADCAST(x) ((cf_t) (x))
