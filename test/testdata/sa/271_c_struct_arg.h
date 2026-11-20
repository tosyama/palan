struct I8 { int a; int b; };
struct Big { long a, b, c; };
struct Odd { char c[3]; };
struct LD { int n; long double v; };
int take_i8(struct I8 v);
long take_big(struct Big v);
int take_odd(struct Odd v);
int take_ld(struct LD v);
