int f_const_after(char const *s, unsigned char const volatile *u);
int f_unnamed_restrict(const char *restrict, void *__restrict__);
int f_arr_qualified(char *p[restrict], int q[static 4], int r[const 2]);
__signed__ char f_signed(__signed short x);
__const char *f_const_alt(void);
double _Complex f_complex(float _Complex a, _Complex double b, long double __complex__ c);
unsigned __int128 f_int128(__int128 a, __signed__ __int128 b);
long double f_long_double(long double x);
static __inline__ int f_static_inline(int x) { __asm__ ("nop" : : "r" (x)); { return x; } }
__inline int f_inline(void) { return 0; }
enum {
	sz_cf = sizeof(float _Complex), al_cf = _Alignof(float _Complex),
	sz_cd = sizeof(double _Complex), sz_cld = sizeof(long double _Complex),
	al_cld = _Alignof(long double _Complex), sz_i128 = sizeof(unsigned __int128)
};
