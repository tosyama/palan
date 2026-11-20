#define EI(name, value) name = value,
enum ei {
#include "058_enum_include.def"
	ei_last
};
