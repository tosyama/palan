#if __has_include("055_struct_typedef_name.h") && __has_include(<linux/types.h>)
int hi_found(void);
#endif
#if __has_include("no_such_header.h") || __has_include(<no_such_dir/no_such.h>)
int hi_missing(void);
#endif
#ifdef __has_include
int hi_defined(void);
#endif
