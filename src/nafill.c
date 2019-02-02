#include "data.table.h"
#include <Rdefines.h>

// ans_t *ans is not used here but can be used to track timings, messages, etc.
void setnafillDouble(double *x, uint_fast64_t nx, unsigned int type, double fill, ans_t *ans) {
  if (type==0) { // const
    for (uint_fast64_t i=0; i<nx; i++) {
      if (ISNA(x[i])) x[i] = fill;
    }
  } else if (type==1) { // locf
    for (uint_fast64_t i=1; i<nx; i++) {
      if (ISNA(x[i])) x[i] = x[i-1];
    }
  } else if (type==2) { // nocb
    for (int_fast64_t i=nx-2; i>=0; i--) {
      if (ISNA(x[i])) x[i] = x[i+1];
    }
  }
}

void nafillDouble(double *x, uint_fast64_t nx, unsigned int type, double fill, ans_t *ans) {
  if (type==0) { // const
    for (uint_fast64_t i=0; i<nx; i++) {
      ans->dbl_v[i] = ISNA(x[i]) ? fill : x[i];
    }
  } else if (type==1) { // locf
    ans->dbl_v[0] = x[0];
    for (uint_fast64_t i=1; i<nx; i++) {
      ans->dbl_v[i] = ISNA(x[i]) ? ans->dbl_v[i-1] : x[i];
    }
  } else if (type==2) { // nocb
    ans->dbl_v[nx-1] = x[nx-1];
    for (int_fast64_t i=nx-2; i>=0; i--) {
      ans->dbl_v[i] = ISNA(x[i]) ? ans->dbl_v[i+1] : x[i];
    }
  }
}

// ans_t *ans is not used here but can be used to track timings, messages, etc.
void setnafillInteger(int32_t *x, uint_fast64_t nx, unsigned int type, int32_t fill, ans_t *ans) {
  if (type==0) { // const
    for (uint_fast64_t i=0; i<nx; i++) {
      if (x[i]==NA_INTEGER) x[i] = fill;
    }
  } else if (type==1) { // locf
    for (uint_fast64_t i=1; i<nx; i++) {
      if (x[i]==NA_INTEGER) x[i] = x[i-1];
    }
  } else if (type==2) { // nocb
    for (int_fast64_t i=nx-2; i>=0; i--) {
      if (x[i]==NA_INTEGER) x[i] = x[i+1];
    }
  }
}

void nafillInteger(int32_t *x, uint_fast64_t nx, unsigned int type, int32_t fill, ans_t *ans) {
  if (type==0) { // const
    for (uint_fast64_t i=0; i<nx; i++) {
      ans->int_v[i] = x[i]==NA_INTEGER ? fill : x[i];
    }
  } else if (type==1) { // locf
    ans->int_v[0] = x[0];
    for (uint_fast64_t i=1; i<nx; i++) {
      ans->int_v[i] = x[i]==NA_INTEGER ? ans->int_v[i-1] : x[i];
    }
  } else if (type==2) { // nocb
    ans->int_v[nx-1] = x[nx-1];
    for (int_fast64_t i=nx-2; i>=0; i--) {
      ans->int_v[i] = x[i]==NA_INTEGER ? ans->int_v[i+1] : x[i];
    }
  }
}

SEXP nafillR(SEXP obj, SEXP type, SEXP fill, SEXP inplace) {
  int protecti=0;
  
  if (!xlength(obj)) return(obj);
  bool binplace = LOGICAL(inplace)[0];
  SEXP x;
  if (isVectorAtomic(obj)) {
    if (binplace) {
      error("'x' argument is atomic vector, in-place update is supported only for list/data.table");
    } else if (!(isReal(obj) || isInteger(obj))) {
      error("'x' argument must be numeric type, or list/data.table of numeric types");
    }
    x = PROTECT(allocVector(VECSXP, 1)); protecti++; // wrap into list
    SET_VECTOR_ELT(x, 0, obj);
  } else {
    R_len_t nobj = length(obj);
    for (R_len_t i=0; i<nobj; i++) {
      if (!(isReal(VECTOR_ELT(obj, i)) || isInteger(VECTOR_ELT(obj, i)))) {
        error("'x' argument must be numeric type, or list/data.table of numeric types");
      }
    }
    x = obj;
  }
  R_len_t nx=length(x);
  
  double* dx[nx];
  int32_t* ix[nx];
  uint_fast64_t inx[nx];
  SEXP ans = R_NilValue;
  ans_t vans[nx];
  for (R_len_t i=0; i<nx; i++) {
    inx[i] = xlength(VECTOR_ELT(x, i));
    dx[i] = REAL(VECTOR_ELT(x, i));
    ix[i] = INTEGER(VECTOR_ELT(x, i));
  }
  if (!binplace) {
    ans = PROTECT(allocVector(VECSXP, nx)); protecti++;
    for (R_len_t i=0; i<nx; i++) {
      SET_VECTOR_ELT(ans, i, allocVector(TYPEOF(VECTOR_ELT(x, i)), inx[i]));
      vans[i] = ((ans_t) { .dbl_v=REAL(VECTOR_ELT(ans, i)), .int_v=INTEGER(VECTOR_ELT(ans, i)), .status=0, .message={"\0","\0","\0","\0"} });
    }
  } else {
    for (R_len_t i=0; i<nx; i++) {
      vans[i] = ((ans_t) { .dbl_v=(double *)R_NilValue, .int_v=(int32_t *)R_NilValue, .status=0, .message={"\0","\0","\0","\0"} });
    }
  }
  
  unsigned int itype;
  if (!strcmp(CHAR(STRING_ELT(type, 0)), "const")) itype = 0;
  else if (!strcmp(CHAR(STRING_ELT(type, 0)), "locf")) itype = 1;
  else if (!strcmp(CHAR(STRING_ELT(type, 0)), "nocb")) itype = 2;
  else error("Internal error: invalid type argument in nafillR function, should have been caught before. please report to data.table issue tracker."); // # nocov
  
  if (itype==0 && length(fill)!=1)
    error("fill must be a vector of length 1");
  
  double dfill=NA_REAL;
  int32_t ifill=NA_INTEGER;
  if (itype==0) {
    if (isInteger(fill)) {
      ifill = INTEGER(fill)[0];
      dfill = INTEGER(fill)[0]==NA_LOGICAL ? NA_REAL : (double)INTEGER(fill)[0];
    } else if (isReal(fill)) {
      ifill = ISNA(REAL(fill)[0]) ? NA_INTEGER : (int32_t)REAL(fill)[0];
      dfill = REAL(fill)[0];
    } else if (isLogical(fill) && LOGICAL(fill)[0]==NA_LOGICAL){
      ifill = NA_INTEGER;
      dfill = NA_REAL;
    } else {
      error("fill must be numeric");
    }
  }
  
  #pragma omp parallel for if (nx>1) num_threads(getDTthreads())
  for (R_len_t i=0; i<nx; i++) {
    switch (TYPEOF(VECTOR_ELT(x, i))) {
    case REALSXP :
      binplace ? setnafillDouble(dx[i], inx[i], itype, dfill, &vans[i]) : nafillDouble(dx[i], inx[i], itype, dfill, &vans[i]);
      break;
    case INTSXP :
      binplace ? setnafillInteger(ix[i], inx[i], itype, ifill, &vans[i]) : nafillInteger(ix[i], inx[i], itype, ifill, &vans[i]);
      break;
    }
  }
  
  for (R_len_t i=0; i<nx; i++) { // # nocov start
    if (vans[i].status == 3) {
      error(vans[i].message[3]);
    } else if (vans[i].status == 2) {
      warning(vans[i].message[2]);
    } else if (vans[i].status == 1) {
      //message(vans[i].message[1]);
    } else if (vans[i].status == 1) {
      //Rprintf(vans[i].message[0]);
    }
  } // # nocov end
  
  UNPROTECT(protecti);
  if (binplace) {
    return obj;
  } else {
    return isVectorAtomic(obj) && length(ans) == 1 ? VECTOR_ELT(ans, 0) : ans;
  }
}