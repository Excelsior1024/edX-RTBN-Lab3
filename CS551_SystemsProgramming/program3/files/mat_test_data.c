#include "mat_test_data.h"

#include "errors.h"
#include "memalloc.h"

//#define DO_TRACE 1
#include "trace.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/************************** Test Data Input Routines *******************/

typedef enum { NULL_T, STRING_T, INTEGER_T, EOF_T } TokenType;

typedef struct {
  TokenType type;
  int len;
  union {
    struct {
      const char *string; //not NUL terminated
    };
    long integer;
  };
} Token;

typedef struct {
  FILE *in;
  const char *fileName;
  int lineNumber;
  char *line;
  size_t lineSize;
  ssize_t lineLen;
  int lineIndex;
} ScanState;

static Token
nextToken(ScanState *scan)
{
  Token t = { .type = NULL_T };
  while (t.type == NULL_T) {
    if (scan->lineIndex >= scan->lineLen) {
      scan->lineLen = getline(&scan->line, &scan->lineSize, scan->in);
      TRACE("%s:%d: read %s", scan->fileName, scan->lineNumber, scan->line);
      if (scan->lineLen < 0) {
        t.type = EOF_T;
        break;
      }
      scan->lineIndex = 0;
      scan->lineNumber++;
    }
    char *p = &scan->line[scan->lineIndex];
    while (isspace(*p)) p++;  //will hit '\0' on eol
    if (*p != '\0') {
      const char *tokenStart = p;
      t.type =
        (isdigit(*p) || (*p == '-' && isdigit(p[1]))) ? INTEGER_T : STRING_T;
      while (!isspace(*p) && p - scan->line < scan->lineLen) p++;
      t.len = p - tokenStart;
      if (t.type == STRING_T) {
        t.string = tokenStart;
      }
      else {
        char *endP;
        t.integer = strtol(tokenStart, &endP, 10);
        if (endP - tokenStart != t.len) {
          fatal("%s:%d: bad integer %.*s", scan->fileName, scan->lineNumber,
                t.len, tokenStart);
        }
      }
    }
    scan->lineIndex = (p - scan->line);
  } //while (t.type == NULL_T)
  return t;
}

static TestData *
linkData(TestData *data, TestData **link)
{
  *link = data;
  return data;
}

static TestData *
readNextTestData(ScanState *scan, TestData **next)
{
  typedef enum { DESC_S, NROWS_S, NCOLS_S, DATA_S } State;
  TestData *data = NULL;
  State state = DESC_S;
  int dataIndex = 0;
  int nEntries = -1;
  while (true) {
    Token t = nextToken(scan);
    TRACE("token=%d, state=%d, lineIndex=%d", t.type, state, scan->lineIndex);
    switch (t.type) {
    case EOF_T:
      if (state == DESC_S) return linkData(data, next);
      free(data);
      fatal("%s:%d: unexpected EOF", scan->fileName, scan->lineNumber);
      break;
    case STRING_T: {
      if (state != DESC_S) {
        fatal("%s:%d: unexpected description %s", scan->fileName,
              scan->lineNumber, t.string);
      }
      data = mallocChk(sizeof(TestData));
      char *desc = mallocChk(t.len + 1);
      strncpy(desc, t.string, t.len);
      desc[t.len] = '\0';
      data->desc = desc;
      break;
    }
    case INTEGER_T:
      if (state == DESC_S) {
        fatal("%s:%d: unexpected token %ld; expected matrix description",
              scan->fileName, scan->lineNumber, t.integer);
      }
      break;
    default:
      break;
    } //switch (t.type)

    switch (state) {
    case DESC_S:
      state = NROWS_S;
      break;
    case NROWS_S:
      data->nRows = t.integer;
      state = NCOLS_S;
      break;
    case NCOLS_S:
      data->nCols = t.integer;
      nEntries = data->nRows * data->nCols;
      data->data = mallocChk(nEntries * sizeof(MatrixBaseType));
      state = DATA_S;
      break;
    case DATA_S:
      TRACE("data[%d]=%ld", dataIndex, t.integer);
      data->data[dataIndex++] = t.integer;
      if (dataIndex >= nEntries) return linkData(data, next);
      break;
    } //switch (state)
  } //while (true)
  assert(false); //should never get here since loop has direct return's.
}


/** Append list of new test data from fileName to *link.  Will
 *  terminate program on most errors.
 */
void
newTestData(const char *fileName, TestData **link)
{
  _Bool isStdin = (strcmp(fileName, "-") == 0);
  FILE *in = (isStdin) ? stdin : fopen(fileName, "r");
  if (!in) {
    fatal("cannot read %s:", fileName);
  }
  ScanState scan = {
    .in = in,
    .fileName = (isStdin) ? "<stdin>" : fileName,
  };
  for (TestData *data = readNextTestData(&scan, link); data != NULL;
       data = readNextTestData(&scan, &data->next)) {
  }
  free(scan.line);
  if (!isStdin) fclose(in);
}

/** Free previously created testData */
void
freeTestData(TestData *datas)
{
  TestData *data = datas;
  while (data != NULL) {
    TestData *next = data->next;
    free((void *)data->desc);
    free(data->data);
    free(data);
    data = next;
  }
}

#ifdef TEST_MAT_TEST_DATA

int
main(int argc, const char *argv[])
{
  if (argc == 1) {
    fatal("usage: %s MATRIX_DATA_FILE...", argv[0]);
  }
  FILE *out = stdout;
  for (int a = 1; a < argc; a++) {
    TestData *datas = NULL;
    newTestData(argv[a], &datas);
    for (const TestData *p = datas; p != NULL; p = p->next) {
      fprintf(out, "**** %s ****\n", p->desc);
      for (int i = 0; i < p->nRows; i++) {
        for (int j = 0; j < p->nCols; j++) {
          fprintf(out, "%8ld ", (long)p->data[i*p->nCols + j]);
        }
        fprintf(out, "\n");
      }
    }
    freeTestData((TestData *)datas);
  } //for (int i = 1; i < argc; i++)
}

#endif //#ifdef TEST_MAT_TEST_DATA
