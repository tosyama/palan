typedef unsigned short ushort_t;
struct box { int v[4]; struct box *next; };
int get(int n);
enum { RED = 3 };

#define MASK 0x7f
#define INNER(s) ((s) & MASK)
#define OUTER(s) (INNER(s) == 0)
#define TYPED(c) ((ushort_t) (c) << 2)
#define FIELDS(p, i) ((p)->next->v[i] + (*(p)).v[0])
#define CALLS(n) (get(n) && !get(RED))
#define CHARS(c) ((c) == 'a' || (c) == '\n' || (c) == '\x41')
#define NEGATE(x) (-1 - (x))
#define NOPARAM() (get(0))

#define TERNARY(a, b) ((a) < (b) ? (a) : (b))
#define ASSIGN(x) ((x) |= 1)
#define COMMA(x) ((void) (x), 0)
#define STRING(x) (x, "s")
#define STRINGIZE(x) #x
#define PASTE(a, b) a ## b
#define VARIADIC(...) get(__VA_ARGS__)
#define ADDRESS(x) (&(x))
#define SIZEOF_EXPR(x) sizeof (x)
#define INCREMENT(x) ((x)++)
#define PRE_DECREMENT(x) (--(x))
#define CALL_PTR(f) ((f)(0))
#define FLOATING(x) ((x) * 1.5)
#define NOT_EXPR(x) int x;

#define CHAR_CONST 'z'
#define NOT_A_CAST (x) - 1
