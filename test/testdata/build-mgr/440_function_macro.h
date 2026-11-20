#include <string.h>
#include <stdlib.h>
struct box { int v[4]; };
#define SECOND(p) ((p)->v[1])
#define FIRST_CHAR(s) (*(s))
#define LEN_PLUS(s, n) (strlen(s) + (n))
#define TWICE(x) ((x) + (x))
#define MASKED(c) ((unsigned char) (c) & 0x0f)
#define BOTH(a, b) ((a) && !(b))
#define NESTED(x) (TWICE(x) * 2)
#define atoi(s) (42)
#define abs(x) ((x) < 0 ? -(x) : (x))
