#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

typedef int fixed_t;

#define F (1 << 14) // 17.14 fixed-point representation

/* Convert integer to fixed-point */
fixed_t int_to_fp(int n);

/* Convert fixed-point to integer (rounding toward zero) */
int fp_to_int_zero(fixed_t x);

/* Convert fixed-point to integer (rounding to nearest) */
int fp_to_int_nearest(fixed_t x);

/* Add two fixed-point numbers */
fixed_t add_fp(fixed_t x, fixed_t y);

/* Subtract y from x (fixed-point) */
fixed_t sub_fp(fixed_t x, fixed_t y);

/* Add fixed-point and int */
fixed_t add_mixed(fixed_t x, int n);

/* Subtract int from fixed-point */
fixed_t sub_mixed(fixed_t x, int n);

/* Multiply two fixed-point numbers */
fixed_t mul_fp(fixed_t x, fixed_t y);

/* Multiply fixed-point by int */
fixed_t mul_mixed(fixed_t x, int n);

/* Divide x by y (both fixed-point) */
fixed_t div_fp(fixed_t x, fixed_t y);

/* Divide fixed-point by int */
fixed_t div_mixed(fixed_t x, int n);

#endif /* THREADS_FIXED_POINT_H */