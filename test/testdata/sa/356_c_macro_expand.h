int get(int n);
struct box { int v[4]; };
#define SCALE(x) (get(x) * 2)
#define CAST(x) ((unsigned char) (x))
#define BOX1(p) ((p)->v[1])
