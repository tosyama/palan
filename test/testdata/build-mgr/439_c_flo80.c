#include <stdio.h>
#include "439_c_flo80.h"

long double ld_late(long a, long b, long c, long d, long e, long f, long g, long double x, int h, long double y)
{
	printf("late %ld %ld %.2Lf %d %.2Lf\n", a, g, x, h, y);
	return a + g + x + h + y;
}

struct LdMix ld_mix(struct LdMix m, long double add)
{
	printf("mix %d %.2Lf\n", m.n, m.v);
	m.n += 1;
	m.v += add;
	return m;
}

struct LdOne ld_one(long double x) { return (struct LdOne){x / 4}; }

long double ld_call_back(void) { return ld_scale(1.0L / 3, 2); }
