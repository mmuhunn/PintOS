#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#define F (1 << 14) // dh. 17.14 fixed-point

/* dh. integer ↔ fixed-point translation */
#define INT_TO_FP(n) ((n) * F)               // integer → fixed point
#define FP_TO_INT_ZERO(x) ((x) / F)          // fixed point → integer (remove)
#define FP_TO_INT_NEAR(x) ((x) >= 0 ? ((x) + F / 2) / F : ((x) - F / 2) / F)

/* dh. fixed point caculation */
#define ADD_FP(x, y) ((x) + (y))
#define SUB_FP(x, y) ((x) - (y))
#define ADD_MIX(x, n) ((x) + (n) * F)
#define SUB_MIX(x, n) ((x) - (n) * F)
#define MUL_FP(x, y) (((int64_t)(x)) * (y) / F)
#define MUL_MIX(x, n) ((x) * (n))
#define DIV_FP(x, y) (((int64_t)(x)) * F / (y))
#define DIV_MIX(x, n) ((x) / (n))

#endif /* FIXED_POINT_H */
