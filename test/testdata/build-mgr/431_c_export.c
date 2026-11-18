#include "431_c_export.h"

int32_t twice_scaled(int32_t v)
{
	return 2 * scaled(v);
}

int64_t total_of_three(void)
{
	int64_t xs[3] = {1, 2, 3};
	return total(xs, 3);
}
