#include "matrix.h"
#include "dense_matrix.h"
#include "smart_mul_matrix.h"

#include "errors.h"
#include "memalloc.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <getopt.h>
#include <sys/times.h>

/** struct to allow defining test matrices */
typedef struct {
  const char *desc;
  int nRows, nCols;
  int *data;      //pointer to matrix data
} TestData;

/** Function used for creating matrices */
typedef Matrix *(*NewFn)(int nRows, int nCols, int *err);

/** Define all NewFn's used for creating test matrices */
static  struct {
  const char *desc;
  NewFn new;
} newFns[] = {
  { .desc = "denseMatrix", .new = (NewFn)newDenseMatrix },
  { .desc = "smartMulMatrix", .new = (NewFn)newSmartMulMatrix },
};

/************************* Matrix Output Routines **********************/

#define TEST_CASE_DELIM "------------"

/** Output matrix on stream out preceded by space-separated labels.*/
static void
outMatrix(FILE *out, const Matrix *matrix, const char *labels[])
{
  int err = 0;
  int nRows = matrix->fns->getNRows(matrix, &err);
  int nCols = matrix->fns->getNCols(matrix, &err);
  if (err) {
    fprintf(out, "bad %s matrix: %s\n",
            (labels[0]) ? labels[0] : "", strerror(err));
    return;
  }
  for (const char **p = &labels[0]; *p != NULL; p++) {
    fprintf(out, "%s ", *p);
  }
  if (labels[0]) fprintf(out, "\n");
  for (int i = 0; i < nRows; i++) {
    for (int j = 0; j < nCols; j++) {
      int element = matrix->fns->getElement(matrix, i, j, &err);
      if (err) {
        fprintf(out, "cannot access entry [%d][%d]: %s\n", i, j, strerror(err));
      }
      else {
        fprintf(out, "%8d", element);
      }
    }
    fprintf(out, "\n");
  }
}

/** Output multiplicand * multiplier = product on out, outputting a
 *  product error if *productErr.
 */
static void
outMulTest(FILE *out, const Matrix *multiplicand, const char *multiplicandDesc,
           const Matrix *multiplier, const char *multiplierDesc,
           Matrix *product, int *productErr)
{
  outMatrix(out, multiplicand,
            (const char *[]) { "multiplicand", multiplicandDesc, NULL });
  outMatrix(out, multiplier,
            (const char *[]) { "multiplier", multiplierDesc, NULL });
  if (*productErr) {
    fprintf(out, "product error: %s\n", strerror(*productErr));
    return;
  }
  else {
    outMatrix(out, product,
              (const char *[]) { "product:", multiplicandDesc, " x ",
                                 multiplierDesc, NULL });
  }
  fprintf(out, TEST_CASE_DELIM "\n");
}

static void
outTransposeTest(FILE *out, const Matrix *matrix, const char *desc,
                 const Matrix *transpose)
{
  outMatrix(out, matrix,
            (const char *[]) { "input matrix", desc, NULL });
  outMatrix(out, transpose, (const char *[]) { "transpose matrix", NULL });
  fprintf(out, TEST_CASE_DELIM "\n");
}

/*********************** Multiplication Test Routines ******************/

/** Standard matrix multiplication using a plain (non-oo) dense matrix */
//a[][] and b[][] should be declared const int, but doing so results
//in warnings on gcc 4.9.2-10.
static void
goldMatrixMultiply(int n1, int n2, int n3,
                   int a[n1][n2], int b[n2][n3], int c[n1][n3])
{
  for (int i = 0; i < n1; i++) {
    for (int j = 0; j < n3; j++) {
      c[i][j] = 0;
      for (int k = 0; k < n2; k++) c[i][j] += a[i][k]*b[k][j];
    }
  }
}

/** Return pointer to dynamic array of int's containing entries from
 *  matrix ordered in row-major order.  The return'd array can be
 *  treated as a 2-D matrix when suitably cast.
 */
static int *
matrixToPlainMatrix(Matrix *matrix, const char *desc, int *nRows, int *nCols)
{
  int err = 0;
  int n1 = matrix->fns->getNRows(matrix, &err);
  int n2 = matrix->fns->getNCols(matrix, &err);
  if (err) {
    fatal("matrixToPlainMatrix(): cannot get matrix dimensions for %s: %s",
          desc, strerror(err));
  }
  int *plain = mallocChk(sizeof(MatrixBaseType) * n1 * n2);
  for (int i = 0; i < n1; i++) {
    for (int j = 0; j < n2; j++) {
      int element = matrix->fns->getElement(matrix, i, j, &err);
      if (err) {
        fatal("matrixToPlainMatrix(): cannot access element at %s[%d][%d]: %s",
              desc, i, j, strerror(err));
      }
      plain[i*n2 + j] = element;
    }
  }
  *nRows = n1; *nCols = n2;
  return plain;
}

/** Compare matrix entries with plain entries; if they differ, then
 *  set (*diffRowN, *diffColN) to coordinates of first differing entry
 *  and return false; otherwise return true.
 */
static _Bool
compareMatrixToPlainMatrix(Matrix *matrix, const char *desc,
                           int nRows, int nCols, int plain[nRows][nCols],
                           int *diffRowN, int *diffColN)
{
  int err = 0;
  int n1 = matrix->fns->getNRows(matrix, &err);
  int n2 = matrix->fns->getNCols(matrix, &err);
  if (n1 != nRows || n2 != nCols) {
    fatal("compareMatrixToPlainMatrix(): matrix %s dimensions differ: "
          "matrix is %dx%d; plain is %dx%d", desc, n1, n2, nRows, nCols);
  }
  for (int i = 0; i < n1; i++) {
    for (int j = 0; j < n2; j++) {
      int element = matrix->fns->getElement(matrix, i, j, &err);
      if (err) {
        fatal("matrixToPlainMatrix(): cannot access element at "
              "[%d][%d] in matrix %s: %s", i, j, desc, strerror(err));
      }
      if (element != plain[i][j]) {
        *diffRowN = i; *diffColN = j;
        return false;
      }
    }
  }
  return true;
}

/** Return true iff product is m1 * m2.  If false, report erroneous
 *  first entry on stderr.
 */
static _Bool
doMulTestMatrix(Matrix *m1, const char *m1Desc,
                Matrix *m2, const char *m2Desc, Matrix *product)
{
  int m1NRows, m1NCols;
  int *plain1 = matrixToPlainMatrix(m1, m1Desc, &m1NRows, &m1NCols);
  int m2NRows, m2NCols;
  int *plain2 = matrixToPlainMatrix(m2, m2Desc, &m2NRows, &m2NCols);
  int *plainProduct  = mallocChk(sizeof(MatrixBaseType)*m1NRows*m2NCols);
  goldMatrixMultiply(m1NRows, m1NCols, m2NCols, (int (*)[m1NCols])plain1,
                     (int (*)[m2NCols])plain2, (int (*)[m2NCols])plainProduct);
  const char *mult = " x ";
  char *desc = mallocChk(strlen(m1Desc) + strlen(mult) + strlen(m2Desc) + 1);
  sprintf(desc, "%s%s%s", m1Desc, mult, m2Desc);
  int diffRowN, diffColN;
  _Bool isOk =
    compareMatrixToPlainMatrix(product, desc, m1NRows, m2NCols,
                               (int (*)[m2NCols])plainProduct,
                               &diffRowN, &diffColN);
  if (!isOk) {
    int expectedValue = plainProduct[diffRowN*m2NCols + diffColN];
    int err = 0;
    int testValue = product->fns->getElement(product, diffRowN, diffColN, &err);
    error("%s: differs at [%d][%d]; expected %d, got %d", desc,
          diffRowN, diffColN, expectedValue, testValue);
  }
  free(plain1);
  free(plain2);
  free(plainProduct);
  free(desc);
  return isOk;
}

/************************ Transpose Test Routines **********************/

static _Bool
testTranspose(const Matrix *matrix, const char *desc, const Matrix *transpose,
              int nRows, int nCols)
{
  int err = 0;
  for (int i = 0; i < nRows; i++) {
    for (int j = 0; j < nCols; j++) {
      int expected = matrix->fns->getElement(matrix, i, j, &err);
      if (err) {
        error("testTranspose(): cannot get matrix %s element [%d][%d]: %s",
              desc, i, j, strerror(err));
        return false;
      }
      int actual = transpose->fns->getElement(transpose, j, i, &err);
      if (err) {
        error("testTranspose(): cannot get transpose %s element [%d][%d]: %s",
              desc, j, i, strerror(err));
        return false;
      }
      if (expected != actual) {
        error("testTranspose(): "
              "(matrix[%d][%d] = %d) != (transpose[%d][%d] = %d)",
              i, j, expected, j, i, actual);
        return false;
      }
    }
  }
  return true;
}

static void
doTransposeTestMatrix(FILE *out, _Bool doOutput,
                      const Matrix *matrix, const char *desc)
{
  int err = 0;
  int nRows = matrix->fns->getNRows(matrix, &err);
  int nCols = matrix->fns->getNCols(matrix, &err);
  if (err) {
    error("doTransposeTestMatrix(): cannot get dimensions for %s: %s",
          desc, strerror(errno));
    return;
  }
  Matrix *transpose =
            (Matrix *)newDenseMatrix(nCols, nRows, &err);
  if (err) {
    error("doTransposeTestMatrix(): cannot create transpose matrix for %s: %s",
          desc, strerror(errno));
    return;
  }
  matrix->fns->transpose(matrix, transpose, &err);
  if (err) {
    error("doTransposeTestMatrix(): cannot transpose %s: %s\n",
          desc, strerror(errno));
    return;
  }
  testTranspose(matrix, desc, transpose, nRows, nCols);
  if (doOutput) {
    outTransposeTest(out, matrix, desc, transpose);
  }
  transpose->fns->free(transpose, &err);
  if (err) {
    fprintf(stderr, "doTransposeMatrix(): cannot free transpose: %s\n",
            strerror(errno));
    return;
  }
}

/************************* Test Data to Matrix *************************/

/** Initialize matrix from init. */
static void
initMatrix(int n1, int n2, int init[n1][n2], Matrix *matrix, int *err)
{
  if (n1 != matrix->fns->getNRows(matrix, err) ||
      n2 != matrix->fns->getNCols(matrix, err)) {
    *err = EDOM;
  }
  if (*err) return;
  for (int i = 0; i < n1; i++) {
    for (int j = 0; j < n2; j++) {
      matrix->fns->setElement(matrix, i, j, init[i][j], err);
      if (*err) return;
    }
  }
}

static Matrix *
createMatrix(const TestData *dataP, NewFn newMatrix, int *err)
{
  int nRows = dataP->nRows;
  int nCols = dataP->nCols;
  Matrix *matrix = newMatrix(nRows, nCols, err);
  if (*err) return NULL;
  initMatrix(nRows, nCols, (int (*)[])dataP->data, matrix, err);
  if (*err) return NULL;
  return matrix;
}

/*********************** Matrix Tests **********************/

/** Test transpose for data for all possible newFns. */
static void
doTransposeTestData(FILE *out, _Bool doOutput, const TestData *data)
{
  int err = 0;
  int nNewFns = sizeof(newFns)/sizeof(newFns[0]);
  for (int i = 0; i < nNewFns; i++) {
    NewFn newFnI = newFns[i].new;
    Matrix *matrix = createMatrix(data, newFnI, &err);
    const char *useStr = " using ";
    char *desc = mallocChk(strlen(data->desc) + strlen(useStr) +
                            strlen(newFns[i].desc) + 1);
    sprintf(desc, "%s%s%s", data->desc, useStr, newFns[i].desc);
    doTransposeTestMatrix(out, doOutput, matrix, desc);
    matrix->fns->free(matrix, &err);
    if (err) {
      error("cannot free matrix %s: %s\n", data->desc, strerror(err));
    }
    free(desc);
  }
}

static void
outTimes(const char *desc1, const char *desc2,
         const struct tms *start, const struct tms *end)
{
  long utime = end->tms_utime - start->tms_utime;
  long stime = end->tms_stime - start->tms_stime;
  fprintf(stderr, "%s x %s: utime: %ld, stime: %ld, total: %ld\n",
          desc1, desc2, utime, stime, utime + stime);
}

/** Test multiplication for data1 and data2 for all possible newFns.
 */
static void
doMulTestData(FILE *out, _Bool doOutput, int perfCount,
              const TestData *data1, const TestData *data2)
{
  int err = 0;
  int nNewFns = sizeof(newFns)/sizeof(newFns[0]);
  for (int i = 0; i < nNewFns; i++) {
    
    NewFn newFnI = newFns[i].new;
    Matrix *multiplicand = createMatrix(data1, newFnI, &err);
    const char *useStr = " using ";
    char *desc1 = mallocChk(strlen(data1->desc) + strlen(useStr) +
                            strlen(newFns[i].desc) + 1);
    sprintf(desc1, "%s%s%s", data1->desc, useStr, newFns[i].desc);
    if (err) {
       fprintf(stderr, "cannot make multiplicand for %s: %s\n",
              desc1, strerror(err));
       continue;
     }

    for (int j = 0; j < nNewFns; j++) {
      NewFn newFnJ = newFns[j].new;
      err = 0;
      Matrix *multiplier = createMatrix(data2, newFnJ, &err);
      char *desc2 = mallocChk(strlen(data2->desc) + strlen(useStr) +
                              strlen(newFns[j].desc) + 1);
      sprintf(desc2, "%s%s%s", data2->desc, useStr, newFns[j].desc);
      if (err) {
        fprintf(stderr, "cannot make multiplier for %s: %s",
                desc2, strerror(err));
        continue;
      }
      int productNRows = multiplicand->fns->getNRows(multiplicand, &err);
      int productNCols = multiplier->fns->getNCols(multiplier, &err);
      if (err) {
        fprintf(stderr, "cannot get dimensions for product for %s x %s: %s\n",
                desc1, desc2, strerror(err));
        continue;
      }
      Matrix *product =
        (Matrix *)newDenseMatrix(productNRows, productNCols, &err);
      if (err) {
        fprintf(stderr, "cannot create product for %s x %s: %s\n",
                desc1, desc2, strerror(err));
        continue;
      }
      struct tms start, end;
      if (times(&start) < 0) {
        fatal("cannot get start time for %s x %s:", desc1, desc2);
      }
      int nPerfIters = 0;
      do {
        multiplicand->fns->mul(multiplicand, multiplier, product, &err);
        nPerfIters++;
      } while (nPerfIters < perfCount);
      if (times(&end) < 0) {
        fatal("cannot get start time for %s x %s:", desc1, desc2);
      }
      if (!err && perfCount >= 0) {
        outTimes(newFns[i].desc, newFns[j].desc, &start, &end);
      }
      if (!err && perfCount < 0) {
        doMulTestMatrix(multiplicand, data1->desc, multiplier, data2->desc,
                        product);
      }
      if (doOutput) {
        outMulTest(out, multiplicand, desc1, multiplier, desc2,
                   product, &err);
      }
      err = 0;
      product->fns->free(product, &err);
      if (err) {
        fprintf(stderr, "cannot free product %s x %s: %s\n",
                desc1, desc2, strerror(err));
      }
      err = 0;
      multiplier->fns->free(multiplier, &err);
      if (err) {
      fprintf(stderr, "cannot free multiplier %s: %s\n",
              desc2, strerror(err));
      }
      free(desc2);
    } //for (int j = 0; ...)
    err = 0;
    multiplicand->fns->free(multiplicand, &err);
    if (err) {
      fprintf(stderr, "cannot free multiplicand %s: %s\n",
              desc1, strerror(err));
    }
    free(desc1);
  } //for (int i = 0; ...)
}

/****************** Tests with Predefined Matrix Data ******************/

static void
doTransposeTests(FILE *out, _Bool doOutput,
                 const TestData *data, int nData)
{
  for (int i = 0; i < nData; i++) {
    doTransposeTestData(out, doOutput, &data[i]);
  }
}

static void
doMulTests(FILE *out, _Bool doOutput, int perfCount,
           const TestData *data, int nData)
{
  for (int i = 0; i < nData; i++) {
    for (int j = 0; j < nData; j++) {
      doMulTestData(out, doOutput, perfCount, &data[i], &data[j]);
    }
  }
}

static void
doTests(FILE *out, _Bool doOutput, const TestData *data, int nData)
{
  doTransposeTests(out, doOutput, data, nData);
  doMulTests(out, doOutput, -1, data, nData);
}


/**************************** Random Test Data *************************/

typedef struct {
  const char *desc;
  int nRows, nCols;
  int max;
} RandSpec;

static const TestData
createRandomTestData(const RandSpec *spec)
{
  TestData data;
  data.desc = spec->desc;
  data.nRows = spec->nRows; data.nCols = spec->nCols;
  data.data = mallocChk(spec->nRows * spec->nCols * sizeof(MatrixBaseType));
  for (int i = 0; i < spec->nRows * spec->nCols; i++) {
    //not uniformly distributes
    data.data[i] =  (rand() % (2*spec->max - 1)) - (spec->max - 1);
  }
  return data;
}

static void
freeRandomTestData(const TestData *data)
{
  free(data->data);
}


/*************************** Tests with Random Data ********************/

static const RandSpec randSpecs[] = {
  { .desc = "rand(5x5)", .nRows = 5, .nCols = 5, .max = 10 },
  { .desc = "rand(5x6)", .nRows = 5, .nCols = 6, .max = 10 }
};

static void doRandomTests(FILE *out, _Bool doOutput) {
  int nSpecs = sizeof(randSpecs)/sizeof(randSpecs[0]);
  TestData data[nSpecs];
  for (int i = 0; i < nSpecs; i++) {
    data[i] = createRandomTestData(&randSpecs[i]);
  }
  doTests(out, doOutput, data, nSpecs);
  for (int i = 0; i < nSpecs; i++) {
    freeRandomTestData(&data[i]);
  }
}

/*************************** Predefined Tests **************************/

#include "test.data"

static void
doPredefinedTests(FILE *out, _Bool doOutput)
{
  doTests(out, doOutput, testData, sizeof(testData)/sizeof(testData[0]));
}


/************************** Performance Tests **************************/

static void
doPerformanceTests(int n)
{
  enum { N_ITER = 1 };
  RandSpec randSpec = {
    .desc = "randPerfMatrix", .nRows = n, .nCols = n, .max = 100,
  };
  TestData data = createRandomTestData(&randSpec);
  doMulTests(NULL, false, N_ITER, &data, 1);
  freeRandomTestData(&data);
}

/***************************** Main Program ****************************/

#define OUTPUT_LONG_OPT            "output"
#define OUTPUT_SHORT_OPT           'o'
#define PREDEF_TESTS_LONG_OPT      "predefined-tests"
#define PREDEF_TESTS_SHORT_OPT     't'
#define RAND_TESTS_LONG_OPT        "random-tests"
#define RAND_TESTS_SHORT_OPT       'r'
#define PERF_MATRIX_SIZE_LONG_OPT  "perf-matrix-size"
#define PERF_MATRIX_SIZE_SHORT_OPT 's'

#define SHORT_OPTS {     \
  PREDEF_TESTS_SHORT_OPT, \
  RAND_TESTS_SHORT_OPT, \
  OUTPUT_SHORT_OPT, \
  PERF_MATRIX_SIZE_SHORT_OPT, ':', \
  '\0' \
  }

static struct option options[] = {
  { .name = OUTPUT_LONG_OPT, .has_arg = 0, .flag = 0,
    .val = OUTPUT_SHORT_OPT
  },
  { .name = PREDEF_TESTS_LONG_OPT, .has_arg = 0, .flag = 0,
    .val = PREDEF_TESTS_SHORT_OPT
  },
  { .name = RAND_TESTS_LONG_OPT, .has_arg = 0, .flag = 0,
    .val = RAND_TESTS_SHORT_OPT
  },
  { .name = PERF_MATRIX_SIZE_LONG_OPT, .has_arg = 1, .flag = 0,
    .val = PERF_MATRIX_SIZE_SHORT_OPT
  },

};

typedef struct {
  _Bool isErr;
  _Bool doOutput;
  _Bool doPredefTests;
  _Bool doRandomTests;
  int perfMatrixSize;
} Opts;

static void
usage(const char *prog)
{
  fatal("usage: %s ( (--%s | -%c) | (--%s | -%c) | (--%s | -%c) | "
        "(--%s S | -%c S) )+", prog,
        OUTPUT_LONG_OPT, OUTPUT_SHORT_OPT,
        PREDEF_TESTS_LONG_OPT, PREDEF_TESTS_SHORT_OPT,
        RAND_TESTS_LONG_OPT, RAND_TESTS_SHORT_OPT,
        PERF_MATRIX_SIZE_LONG_OPT, PERF_MATRIX_SIZE_SHORT_OPT);
}

static Opts
getOpts(int argc, const char *argv[])
{
  const char shortOpts[] = SHORT_OPTS;
  const char *prog = argv[0];
  Opts opts = {};
  int c;
  while (true) {
    int optIndex = 0;
    c = getopt_long(argc, (char **)argv, shortOpts, options, &optIndex);
    if (c < 0) break;
    switch (c) {
    case OUTPUT_SHORT_OPT:
      opts.doOutput = true;
      break;
    case PREDEF_TESTS_SHORT_OPT:
      opts.doPredefTests = true;
      break;
    case RAND_TESTS_SHORT_OPT:
      opts.doRandomTests = true;
      break;
    case  PERF_MATRIX_SIZE_SHORT_OPT:
      opts.perfMatrixSize = atoi(optarg);
      break;
    case '?':
      opts.isErr = true;
      break;
    }
  }
  if (optind < argc) usage(prog);
  return opts;
}

int
main(int argc, const char *argv[])
{
  srand(initSeed); //ensure reproducible results
  if (argc == 1) usage(argv[0]);
  Opts opts = getOpts(argc, argv);
  if (opts.isErr) {
    usage(argv[0]);
  }
  else {
    if (opts.doPredefTests) doPredefinedTests(stdout, opts.doOutput);
    if (opts.doRandomTests) doRandomTests(stdout, opts.doOutput);
    if (opts.perfMatrixSize > 0) doPerformanceTests(opts.perfMatrixSize);
  }
  exit(getErrorCount() > 0);
}
