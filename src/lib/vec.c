#include "rlang.h"
#include <math.h>

static bool has_correct_length(sexp* x, r_ssize_t n) {
  return n < 0 || r_length(x) == n;
}

bool r_is_atomic(sexp* x, r_ssize_t n) {
  switch(r_typeof(x)) {
  case LGLSXP:
  case INTSXP:
  case REALSXP:
  case CPLXSXP:
  case STRSXP:
  case RAWSXP:
    return has_correct_length(x, n);
  default:
    return false;
  }
}
bool r_is_scalar_atomic(sexp* x) {
  return r_is_atomic(x, 1);
}

bool r_is_vector(sexp* x, r_ssize_t n) {
  switch(r_typeof(x)) {
  case LGLSXP:
  case INTSXP:
  case REALSXP:
  case CPLXSXP:
  case STRSXP:
  case RAWSXP:
  case VECSXP:
    return has_correct_length(x, n);
  default:
    return false;
  }
}

bool r_is_integerish(sexp* x) {
  static sexp* predicate = NULL;
  if (!predicate) {
    predicate = rlang_ns_get("is_integerish");
  }
  sexp* call = KEEP(r_build_call1(predicate, x));
  sexp* out = r_eval(call, r_empty_env);
  FREE(1);
  return r_lgl_get(out, 0);
}

bool r_is_integer(sexp* x, r_ssize_t n) {
  return r_typeof(x) == r_type_integer && has_correct_length(x, n);
}

bool r_is_finite(sexp* x) {
  size_t n = r_length(x);

  switch(r_typeof(x)) {
  case r_type_integer: {
    int* ptr = r_int_deref(x);
    for (size_t i = 0; i < n; ++i, ++ptr) {
      if (*ptr == NA_INTEGER) {
        return false;
      }
    }
    break;
  }
  case r_type_double: {
    double* ptr = r_dbl_deref(x);
    for (size_t i = 0; i < n; ++i, ++ptr) {
      if (!isfinite(*ptr)) {
        return false;
      }
    }
    break;
  }
  case r_type_complex: {
    r_complex_t* ptr = r_cpl_deref(x);
    for (size_t i = 0; i < n; ++i, ++ptr) {
      if (!isfinite(ptr->r) || !isfinite(ptr->i)) {
        return false;
      }
    }
    break;
  }
  default:
    r_abort("Internal error: expected a numeric vector");
  }

  return true;
}

r_ssize_t r_vec_length(sexp* x) {
  switch(r_typeof(x)) {
  case r_type_null:
    return 0;
  case r_type_logical:
  case r_type_integer:
  case r_type_double:
  case r_type_complex:
  case r_type_character:
  case r_type_raw:
  case r_type_list:
  case r_type_string:
    return LENGTH(x);
  default:
    r_abort("Internal error: expected a vector");
  }
}

sexp* r_vec_get(sexp* vec, r_ssize_t i) {
  switch (r_typeof(vec)) {
  case r_type_character:
    return r_chr_get(vec, i);
  case r_type_list:
    return r_list_get(vec, i);
  default:
    r_abort("Internal error: Unimplemented type in `r_vec_get()`");
  }
}

bool r_vec_find_first_identical_any(sexp* x, sexp* y, r_long_ssize_t* index) {
  if (r_typeof(x) != r_type_list && r_typeof(x) != r_type_character) {
    r_abort("Internal error: `x` must be a list or character vector in `r_vec_find_first_identical_any()`");
  }
  if (r_typeof(y) != r_type_list && r_typeof(y) != r_type_character) {
    r_abort("Internal error: `y` must be a list or character vector in `r_vec_find_first_identical_any()`");
  }
  r_ssize_t n = r_length(x);
  r_ssize_t n_comparisons = r_length(y);

  for (r_ssize_t i = 0; i < n; ++i) {
    sexp* elt = r_vec_get(x, i);

    for (r_ssize_t j = 0; j < n_comparisons; ++j) {
      if (r_is_identical(elt, r_vec_get(y, j))) {
        if (index) {
          *index = i;
        }
        return true;
      }
    }
  }

  return false;
}


// Copy --------------------------------------------------------------

void r_vec_poke_n(sexp* x, r_ssize_t offset,
                  sexp* y, r_ssize_t from, r_ssize_t n) {

  if ((r_length(x) - offset) < n) {
    r_abort("Can't copy data to `x` because it is too small");
  }
  if ((r_length(y) - from) < n) {
    r_abort("Can't copy data from `y` because it is too small");
  }

  switch (r_typeof(x)) {
  case LGLSXP: {
    int* src_data = LOGICAL(y);
    int* dest_data = LOGICAL(x);
    for (r_ssize_t i = 0; i != n; ++i)
      dest_data[i + offset] = src_data[i + from];
    break;
  }
  case INTSXP: {
    int* src_data = INTEGER(y);
    int* dest_data = INTEGER(x);
    for (r_ssize_t i = 0; i != n; ++i)
      dest_data[i + offset] = src_data[i + from];
    break;
  }
  case REALSXP: {
    double* src_data = REAL(y);
    double* dest_data = REAL(x);
    for (r_ssize_t i = 0; i != n; ++i)
      dest_data[i + offset] = src_data[i + from];
    break;
  }
  case CPLXSXP: {
    r_complex_t* src_data = COMPLEX(y);
    r_complex_t* dest_data = COMPLEX(x);
    for (r_ssize_t i = 0; i != n; ++i)
      dest_data[i + offset] = src_data[i + from];
    break;
  }
  case RAWSXP: {
    r_byte_t* src_data = RAW(y);
    r_byte_t* dest_data = RAW(x);
    for (r_ssize_t i = 0; i != n; ++i)
      dest_data[i + offset] = src_data[i + from];
    break;
  }
  case STRSXP: {
    sexp* elt;
    for (r_ssize_t i = 0; i != n; ++i) {
      elt = STRING_ELT(y, i + from);
      SET_STRING_ELT(x, i + offset, elt);
    }
    break;
  }
  case VECSXP: {
    sexp* elt;
    for (r_ssize_t i = 0; i != n; ++i) {
      elt = VECTOR_ELT(y, i + from);
      SET_VECTOR_ELT(x, i + offset, elt);
    }
    break;
  }
  default:
    r_abort("Copy requires vectors");
  }
}

void r_vec_poke_range(sexp* x, r_ssize_t offset,
                      sexp* y, r_ssize_t from, r_ssize_t to) {
  r_vec_poke_n(x, offset, y, from, to - from + 1);
}


// Coercion ----------------------------------------------------------

sexp* rlang_vec_coercer(sexp* dest) {
  switch(r_typeof(dest)) {
  case LGLSXP: return rlang_ns_get("as_logical");
  case INTSXP: return rlang_ns_get("as_integer");
  case REALSXP: return rlang_ns_get("as_double");
  case CPLXSXP: return rlang_ns_get("as_complex");
  case STRSXP: return rlang_ns_get("as_character");
  case RAWSXP: return rlang_ns_get("as_bytes");
  default: r_abort("No coercion implemented for `%s`", Rf_type2str(r_typeof(dest)));
  }
}

void r_vec_poke_coerce_n(sexp* x, r_ssize_t offset,
                         sexp* y, r_ssize_t from, r_ssize_t n) {
  if (r_typeof(y) == r_typeof(x)) {
    r_vec_poke_n(x, offset, y, from, n);
    return ;
  }
  if (r_is_object(y)) {
    r_abort("Can't splice S3 objects");
  }

  // FIXME: This callbacks to rlang R coercers with an extra copy.
  sexp* coercer = rlang_vec_coercer(x);
  sexp* call = KEEP(Rf_lang2(coercer, y));
  sexp* coerced = KEEP(r_eval(call, R_BaseEnv));

  r_vec_poke_n(x, offset, coerced, from, n);
  FREE(2);
}

void r_vec_poke_coerce_range(sexp* x, r_ssize_t offset,
                             sexp* y, r_ssize_t from, r_ssize_t to) {
  r_vec_poke_coerce_n(x, offset, y, from, to - from + 1);
}
