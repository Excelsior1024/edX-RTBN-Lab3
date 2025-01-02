#ifndef _MAT_TEST_DATA_H
#define _MAT_TEXT_DATA_H

/** Interface used for reading test data from files */

#include "matmul.h"

/** struct to allow defining test matrices */
typedef struct TestData {
  const char *desc;
  int nRows, nCols;
  MatrixBaseType *data;      //pointer to matrix data
  struct TestData *next;
} TestData;

/** Append list of new test data from fileName to *link.  Will
 *  terminate program on most errors.
 */
void newTestData(const char *fileName, TestData **link);

/** Free previously created testData */
void freeTestData(TestData *testData);


#endif //ifndef _MAT_TEST_DATA_H
