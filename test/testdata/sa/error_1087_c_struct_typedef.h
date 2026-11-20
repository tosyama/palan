struct cs_a { int x; };
struct cs_b { int y; };
typedef struct cs_a cs_a_t;
typedef struct cs_b cs_b_t;
struct cs_c { int z; };
void take_b(cs_b_t *p);
