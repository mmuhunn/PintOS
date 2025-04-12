#include "fixed_point.h"
#include <stdint.h>

/* Convert integer to fixed-point */
fixed_t int_to_fp(int n) {
  return n * F;
}

/* Convert fixed-point to integer (toward zero) */
int fp_to_int_zero(fixed_t x) {
  return x / F;
}

/* Convert fixed-point to integer (to nearest) */
int fp_to_int_nearest(fixed_t x) {
  if (x >= 0)
    return (x + F / 2) / F;
  else
    return (x - F / 2) / F;
}

/* Add two fixed-point numbers */
fixed_t add_fp(fixed_t x, fixed_t y) {
  return x + y;
}

/* Subtract two fixed-point numbers */
fixed_t sub_fp(fixed_t x, fixed_t y) {
  return x - y;
}

/* Add fixed-point and int */
fixed_t add_mixed(fixed_t x, int n) {
  return x + n * F;
}

/* Subtract int from fixed-point */
fixed_t sub_mixed(fixed_t x, int n) {
  return x - n * F;
}

/* Multiply two fixed-point numbers */
fixed_t mul_fp(fixed_t x, fixed_t y) {
  return ((int64_t)x) * y / F;
}

/* Multiply fixed-point by int */
fixed_t mul_mixed(fixed_t x, int n) {
  return x * n;
}

/* Divide x by y (both fixed-point) */
fixed_t div_fp(fixed_t x, fixed_t y) {
  return ((int64_t)x) * F / y;
}

/* Divide fixed-point by int */
fixed_t div_mixed(fixed_t x, int n) {
  return x / n;
}