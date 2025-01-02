#include "matmul.h"
#include "mat_test_data.h"

//#define DO_TRACE 1
#include "trace.h"

#include "errors.h"
#include "memalloc.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <getopt.h>



/************************* Matrix Output Routines **********************/

#define TEST_CASE_DELIM "------------"

/** Output matrix on stream out preceded by space-separated labels.*/
static void
outMatrix(FILE *out, int nRows, int nCols,
          CONST MatrixBaseType matrix[nRows][nCols], const char *labels[])
{
  for (const char **p = &labels[0]; *p != NULL; p++) {
    fprintf(out, "%s ", *p);
  }
  if (labels[0]) fprintf(out, "\n");
  for (int i = 0; i < nRows; i++) {
    for (int j = 0; j < nCols; j++) {
      int element = matrix[i][j];
      fprintf(out, "%8d", element);
    }
    fprintf(out, "\n");
  }
}

/** Output multiplicand * multiplier = product on out, outputting a
 *  product error if *productErr.
 */
static void
outMulTest(FILE *out, int n1, int n2, int n3,
           CONST MatrixBaseType multiplicand[n1][n2],
           const char *multiplicandDesc, int multiplierNRows,
           CONST MatrixBaseType multiplier[multiplierNRows][n3],
           const char *multiplierDesc,
           MatrixBaseType product[n1][n3], int *productErr)
{
  outMatrix(out, n1, n2, multiplicand,
            (const char *[]) { "multiplicand", multiplicandDesc, NULL });
  outMatrix(out, multiplierNRows, n3, multiplier,
            (const char *[]) { "multiplier", multiplierDesc, NULL });
  if (*productErr) {
    fprintf(out, "product error: %s\n", strerror(*productErr));
    return;
  }
  else {
    outMatrix(out, n1, n3, product,
              (const char *[]) { "product:", multiplicandDesc, " x ",
                                 multiplierDesc, NULL });
  }
  fprintf(out, TEST_CASE_DELIM "\n");
}

/*********************** Multiplication Test Routines ******************/

/** Standard matrix multiplication using regular 2-dimensional matrices */
static void
goldMatrixMultiply(int n1, int n2, int n3,
                   CONST MatrixBaseType a[n1][n2],
                   CONST MatrixBaseType b[n2][n3], MatrixBaseType c[n1][n3])
{
  for (int i = 0; i < n1; i++) {
    for (int j = 0; j < n3; j++) {
      c[i][j] = 0;
      for (int k = 0; k < n2; k++) c[i][j] += a[i][k]*b[k][j];
    }
  }
}

/** Compare matrix entries with gold entries; if they differ, then
 *  set (*diffRowN, *diffColN) to coordinates of first differing entry
 *  and return false; otherwise return true.
 */
static _Bool
compareMatrixToGoldMatrix(int nRows, int nCols,
                          CONST MatrixBaseType matrix[nRows][nCols],
                          const char *desc,
                          CONST MatrixBaseType gold[nRows][nCols],
                          int *diffRowN, int *diffColN)
{
  for (int i = 0; i < nRows; i++) {
    for (int j = 0; j < nCols; j++) {
      MatrixBaseType element = matrix[i][j];
      if (element != gold[i][j]) {
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
checkMulTest(int n1, int n2, int n3,
             CONST MatrixBaseType m1[n1][n2], const char *m1Desc,
             CONST MatrixBaseType m2[n2][n3], const char *m2Desc,
             MatrixBaseType product[n1][n3])
{
  int *goldProduct  = mallocChk(sizeof(MatrixBaseType)*n1*n3);
  goldMatrixMultiply(n1, n2, n3, m1, m2,
                     (MatrixBaseType (*)[n3])goldProduct);
  const char *mult = " x ";
  char *desc = mallocChk(strlen(m1Desc) + strlen(mult) + strlen(m2Desc) + 1);
  sprintf(desc, "%s%s%s", m1Desc, mult, m2Desc);
  int diffRowN, diffColN;
  _Bool isOk = compareMatrixToGoldMatrix(n1, n3, product, desc,
                                         (MatrixBaseType (*)[n3])goldProduct,
                                         &diffRowN, &diffColN);
  if (!isOk) {
    int goldValue = goldProduct[diffRowN*n3 + diffColN];
    int testValue = product[diffRowN][diffColN];
    error("%s: differs at [%d][%d]; expected %d, got %d", desc,
          diffRowN, diffColN, goldValue, testValue);
  }
  free(goldProduct);
  free(desc);
  return isOk;
}

/** Test multiplication for data1 and data2 for all possible newFns.
 */
static void
doMulTestData(const MatrixMul *matMul, _Bool doGold, FILE *out, _Bool doOutput,
              const TestData *data1, const TestData *data2, int *err)
{
  int n1 = data1->nRows;
  int n2 = data1->nCols;
  int n3 = data2->nCols;
  //should be const MatrixBaseType (gcc
  CONST MatrixBaseType (*multiplicand)[n1][n2] =
    (MatrixBaseType(*)[][n2])data1->data;
  CONST MatrixBaseType (*multiplier)[n2][n3] =
    (MatrixBaseType(*)[][n3])data2->data;
  MatrixBaseType (*product)[n1][n3] =
    (MatrixBaseType(*)[][n3])mallocChk(n1 * n3 * sizeof(MatrixBaseType));
  if (data1->nCols != data2->nRows) {
    *err = EDOM;
  }
  else if (doGold) {
    goldMatrixMultiply(n1, n2, n3, *multiplicand, *multiplier, *product);
  }
  else {
    TRACE("multiplying %p x %p to %p", multiplicand, multiplier, product);
    mulMatrixMul(matMul, n1, n2, n3, *multiplicand, *multiplier, *product,
                 err);
  }
  if (doOutput) { //print output even if *err, as output may aid diagnosis
    outMulTest(out, n1, n2, n3, *multiplicand, data1->desc,
               data2->nRows, *multiplier, data2->desc, *product, err);
  }
  if (!*err) {
    checkMulTest(n1, n2, n3, *multiplicand, data1->desc,
                 *multiplier, data2->desc, *product);
  }
  free(product);
}

static void
doTests(const MatrixMul *matMul, const TestData *data,
        _Bool doGold, FILE *out, _Bool doOutput, int *err)
{
  for (const TestData *p1 = data; p1 != NULL; p1 = p1->next) {
    for (const TestData *p2 = data; p2 != NULL; p2 = p2->next) {
      doMulTestData(matMul, doGold, out, doOutput, p1, p2, err);
      if (*err && *err != EDOM) return; //continue tests if *err == EDOM
      *err = 0;
    }
  }
}


/**************************** Random Test Data *************************/

typedef struct {
  const char *desc;
  int dims[2];
} RandSpec;

static _Bool
parseRandomSpec(const char *specString, RandSpec *randSpec)
{
  randSpec->desc = specString;
  const char *specEndP = specString + strlen(specString);
  const char *colonP = strchr(specString, ':');
  const char *xP = (colonP) ? strchr(colonP, 'x') : NULL;
  _Bool isOk =
    colonP && colonP > specString && xP && xP > colonP && specEndP > xP;
  const char *p = colonP + 1;
  for (int i = 0; isOk && i < 2; i++) {
    char *endP;
    randSpec->dims[i] = strtol(p, &endP, 10);
    isOk = (endP == ((i == 0) ? xP : specEndP) && randSpec->dims[i] > 0);
    p = endP + 1;
  }
  return isOk;
}

static void
addRandomTest(const RandSpec *spec, TestData **link)
{
  enum { MAX = 100 };
  TestData *data = mallocChk(sizeof(TestData));
  data->desc = spec->desc;
  data->nRows = spec->dims[0]; data->nCols = spec->dims[1];
  data->data = mallocChk(data->nRows * data->nCols * sizeof(MatrixBaseType));
  for (int i = 0; i < data->nRows * data->nCols; i++) {
    //not uniformly distributes
    data->data[i] =  (rand() % (2*MAX - 1)) - (MAX - 1);
  }
  data->next = *link;
  *link = data;
}

static void
freeRandomTestData(TestData *data)
{
  TestData *p = data;
  while (p) {
    TestData *next = p->next;
    free(p->data);
    free(p);
    p = next;
  }
}


/***************************** Main Program ****************************/

#define GOLD_LONG_OPT              "gold"
#define GOLD_SHORT_OPT             'g'
#define OUTPUT_LONG_OPT            "output"
#define OUTPUT_SHORT_OPT           'o'
#define RAND_LONG_OPT              "random"
#define RAND_SHORT_OPT             'r'
#define SEED_LONG_OPT              "seed"
#define SEED_SHORT_OPT             's'
#define TRACE_LONG_OPT             "trace"
#define TRACE_SHORT_OPT            't'

#define SHORT_OPTS {     \
  GOLD_SHORT_OPT, \
  OUTPUT_SHORT_OPT, \
  RAND_SHORT_OPT, ':', \
  SEED_SHORT_OPT, ':', \
  TRACE_SHORT_OPT, \
  '\0' \
  }

typedef struct {
  struct option option;      /** from getopt_long() */
  const char *doc;           /** documentation for option */
  const char *arg;           /** option argument description (NULL if no arg) */
} Option;

const static Option OPTIONS[] = {
  { .option =
    { .name = GOLD_LONG_OPT, .has_arg = 0, .flag = 0,
      .val = GOLD_SHORT_OPT
    },
    .doc = "\tOnly use guaranteed correct gold multiplication;"
           "\tdo not use test multiplication",
  },
  { .option =
    { .name = OUTPUT_LONG_OPT, .has_arg = 0, .flag = 0,
      .val = OUTPUT_SHORT_OPT
    },
    .doc = "\tfor each test show multiplicand, multiplier and product matrices",
  },
  { .option =
    { .name = RAND_LONG_OPT, .has_arg = 1, .flag = 0,
      .val = RAND_SHORT_OPT
    },
    .arg = "NAME:NROWSxNCOLS",
    .doc = "\tgenerate random matrix with description \"NAME:NROWSxNCOLS\""
           "\twith NROWS rows and NCOLS columns"
  },
  { .option =
    { .name = SEED_LONG_OPT, .has_arg = 1, .flag = 0,
      .val = SEED_SHORT_OPT
    },
    .arg = "SEED",
    .doc = "\tSet seed of random number generator to SEED",
  },
  { .option =
    { .name = TRACE_LONG_OPT, .has_arg = 0, .flag = 0,
      .val = TRACE_SHORT_OPT
    },
    .doc = "\tGenerate trace of test matrix multiplication on stderr",
  },
  { },  //dummy empty entry as required by getopts_long()
};

enum { N_OPTIONS = sizeof(OPTIONS)/sizeof(OPTIONS[0]) };

/** Output reasonably formatted descriptions of options[nOptions]
 *  on out.
 */
static void
outOptions(const Option *options, int nOptions, FILE *out)
{
  for (int i = 0; i < nOptions; i++) {
    const Option *optP = &options[i];
    if (optP->arg) {
      fprintf(out, "    --%s %s | -%c %s\n", optP->option.name, optP->arg,
              optP->option.val, optP->arg);
    }
    else {
      fprintf(out, "    --%s | -%c\n", optP->option.name, optP->option.val);
    }
    const char *p = optP->doc;
    do { //output option documentation, splitting lines on '\t'
      assert(*p == '\t');
      const char *q = strchr(p + 1, '\t'); //point q to next '\t'
      int n = (q) ? q - p : strlen(p);
      fprintf(out, "%.*s\n", n, p);
      p = q;
    } while (p);
  }
}

/** Gathers all command-line info */
typedef struct {
  _Bool isErr;       /** true if command-line error */
  _Bool doGold;      /** true iff --gold */
  _Bool doOutput;    /** true iff --output */
  _Bool doTrace;     /** true iff --trace */
  TestData *datas;   /** dynamically alloc data from command-line data files */
  TestData *rands;   /** dynamically alloc data from --random options */
  const char *serverDir;/** dir used by server */
  const char *module;/** server path to module */
} Opts;

/** Map options[] to std[] using options[*].option; i.e. pull out
 *  struct option format getopt_long() options from options[].
 */
static void
toStandardOptions(const Option *options, int nOptions, struct option *std)
{
  for (int i = 0; i < nOptions; i++) {
    std[i] = options[i].option;
  }
}

/** Output usage message to stderr */
static void
usage(const char *prog)
{
  fprintf(stderr,
          "usage %s [<options>] <server-dir> <module> [<filename>...]\n"
          "  <server-dir> gives the path to the directory used by the\n"
          "               previously started server\n"
          "  <module>     is the path to the matrix multiplication module\n"
          "               used by the worker process created on the server.\n"
          "               It must be on the server's LD_LIBRARY_PATH.\n"
          "  A test file <filename> consists of one or more matrices with\n"
          "  each matrix consisting of whitespace separated DESC NROWS NCOLS\n"
          "  ENTRY... If no --random or <filename>, then read test from stdin\n"
          "  Allows <options> are\n",
          prog);
  outOptions(OPTIONS, N_OPTIONS - 1, stderr);
  exit(1);
}

/* Options are gotten in 2 passes:
 *
 *   1.  Processes --seed, --trace options and gets N_PROCESSES argument.
 *
 *   2.  Process all remaining options.
 *
 * This is done for 2 reasons:
 *
 *   a) Processing the seed earlier allows it to affect the creation of
 *      any random data (which may be requested in an earlier option).
 *
 *   b) Deferring creation of dynamic data until after calling newMatrixMul()
 *      avoids memory leaks in any worker processes created by the call to
 *      newMatrixMul() (the worker processes do not know anything about
 *      these dynamically allocated data and hence it's impossible for
 *      them to free before exit).
 */

static void
getOptsPass1(struct option options[], int argc, const char *argv[], Opts *optsP)
{
  const char shortOpts[] = SHORT_OPTS;
  int c;
  optind = 1;
  while (true) { //look for seed arg first
    c = getopt_long(argc, (char **)argv, shortOpts, options, NULL);
    if (c < 0) break;
    switch (c) {
    case SEED_SHORT_OPT: {
      char *endP;
      int seed = (int) strtol(optarg, &endP, 10);
      if (*endP != '\0') {
        error("bad seed %s: must be an integer", optarg);
        optsP->isErr = true;
      }
      srand(seed);
      break;
    }
    case TRACE_SHORT_OPT:
      optsP->doTrace = true;
      break;
    default:
      break;
    } //switch
  } //while (true)
  optsP->isErr = optsP->isErr || argc < optind + 2;
  if (!optsP->isErr) {
    optsP->serverDir = argv[optind];
    optsP->module = argv[optind + 1];
  }
}

static void
getOptsPass2(struct option options[], int argc, const char *argv[], Opts *optsP)
{
  const char shortOpts[] = SHORT_OPTS;
  int c;
  optind = 1;
  while (true) {
    int optIndex = 0;
    c = getopt_long(argc, (char **)argv, shortOpts, options, &optIndex);
    if (c < 0) break;
    switch (c) {
    case GOLD_SHORT_OPT:
      optsP->doGold = true;
      break;
    case OUTPUT_SHORT_OPT:
      optsP->doOutput = true;
      break;
    case RAND_SHORT_OPT: {
      RandSpec randSpec;
      if (parseRandomSpec(optarg, &randSpec)) {
        addRandomTest(&randSpec, &optsP->rands);
      }
      else {
        error("bad random matrix spec: %s; must be DESC:NROWSxNCOLS", optarg);
        optsP->isErr = true;
      }
      break;
    }
    case  SEED_SHORT_OPT:
      //processed in getNewMatrixOpts()
      break;
    case TRACE_SHORT_OPT:
      //processed in getNewMatrixOpts()
      break;
    case '?':
      optsP->isErr = true;
      break;
    }
  } //while (true)
  if (!(optsP->isErr = optsP->isErr || (optind >= argc + 1))) {
    //skip serverDir and module which were read in getOptsPass1()
    for (int i = optind + 2; i < argc; i++) {
      newTestData(argv[i], &optsP->datas);
    }
  }
  //if no test inputs, then read from stdin
  if (!optsP->rands && !optsP->datas) newTestData("-", &optsP->datas);
}

int
main(int argc, const char *argv[])
{
  struct option options[N_OPTIONS];
  toStandardOptions(OPTIONS, N_OPTIONS, options);
  Opts opts = {};
  getOptsPass1(options, argc, argv, &opts);
  if (opts.isErr) usage(argv[0]);
  int err = 0;
  FILE *trace = (opts.doTrace) ? stderr : NULL;
  MatrixMul *matMul = newMatrixMul(opts.serverDir, opts.module, trace, &err);
  if (err) fatal("newMatrixMul(): %s", strerror(err));
  getOptsPass2(options, argc, argv, &opts);
  if (opts.isErr) {
    freeMatrixMul(matMul, &err);
    if (err) fatal("freeMatrixMul(): %s", strerror(err));
    usage(argv[0]);
  }
  doTests(matMul, opts.datas, opts.doGold, stdout, opts.doOutput, &err);
  if (!err) {
    doTests(matMul, opts.rands, opts.doGold, stdout, opts.doOutput, &err);
  }
  if (err) {
    error("matMul(): %s", strerror(err));
  }
  freeTestData(opts.datas);
  freeRandomTestData(opts.rands);
  err = 0;
  freeMatrixMul(matMul, &err);
  if (err) fatal("freeMatrixMul(): %s", strerror(err));
  exit(getErrorCount() > 0);
}
