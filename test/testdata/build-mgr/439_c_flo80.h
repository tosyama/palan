struct LdMix { int n; long double v; };
struct LdOne { long double v; };
long double ld_late(long a, long b, long c, long d, long e, long f, long g, long double x, int h, long double y);
struct LdMix ld_mix(struct LdMix m, long double add);
struct LdOne ld_one(long double x);
long double ld_scale(long double x, int k);
long double ld_call_back(void);
