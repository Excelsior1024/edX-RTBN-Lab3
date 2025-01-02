#ifndef _MAT_MUL_H
#define _MAT_MUL_H

/** Declarations for client */

#include "mat_base.h"

#include <stdbool.h>
#include <stdio.h>

//opaque type to be defined by implementation.
typedef struct MatrixMul MatrixMul;

/** Return an interface to the client end of a client-server matrix
 *  multiplier set up to multiply using multiplication module
 *  specified by modulePath with server daemon running in directory
 *  serverDir.
 *
 *  If modulePath is relative, then it must be found on the server's
 *  LD_LIBRARY_PATH interpreted relative to serverDir.  The name of
 *  the multiplication function in the loaded module is the last
 *  component of the modulePath with the extension (if any) removed.
 *
 *  If trace is non-NULL, then turn on tracing for all subsequent
 *  calls to mulMatrixMul() which use the returned MatrixMul.
 *  Specifically, after completing each matrix multiplication, the
 *  client should log a single line on trace in the format:
 *
 *  utime: UTIME, stime: STIME, wall: WALL
 *
 *  where UTIME, STIME and WALL gives the amount of user time, system
 *  time and wall time in times() clock ticks needed within the server
 *  to perform only the multiplication function provided by the
 *  module. The spacing must be exactly as shown above and all the
 *  clock tick values must be output in decimal with no leading zeros
 *  or redundant + signs.
 *
 *  Set *err to an appropriate error number (documented in errno(3))
 *  on error.
 *
 *  This call should result in the creation of a new worker process on
 *  the server, spawned using the double-fork technique.  The worker
 *  process must load and link the specified module.  All future
 *  multiplication requests on the returned MatrixMul must be
 *  performed using the specified module within this worker process.
 *  All IPC between the client and server processes must be performed
 *  using only named pipes (FIFO's).
 */
MatrixMul *newMatrixMul(const char *serverDir, const char *modulePath,
                        FILE *trace, int *err);

/** Free all resources used by matMul.  Specifically, free all memory
 *  used by matMul, remove all FIFO's created specifically for matMul
 *  and set up the worker process on the server to terminate.  Set
 *  *err appropriately (as documented in errno(3)) on error.
 */
void freeMatrixMul(MatrixMul *matMul, int *err);

/** Set matrix c[n1][n3] to a[n1][n2] * b[n2][n3].  It is assumed that
 *  the caller has allocated c[][] appropriately.  Set *err to an
 *  appropriate error number (documented in errno(3)) on error.  If
 *  *err is returned as non-zero, then the matMul object may no longer
 *  be valid and future calls to mulMatrixMul() may have unpredictable
 *  behavior.  It is the responsibility of the caller to call
 *  freeMatrixMul() after an error.
 *
 *  The multiplication must be performed entirely on the server using
 *  the specified module by the worker process which was spawned when
 *  matMul was created.  Note that a single matMul instance may be
 *  used for performing multiple multiplications.  All IPC must be
 *  handled using FIFOs.
 */
void mulMatrixMul(const MatrixMul *matMul, int n1, int n2, int n3,
                  CONST MatrixBaseType a[n1][n2],
                  CONST MatrixBaseType b[n2][n3],
                  MatrixBaseType c[n1][n3], int *err);


#endif //ifndef _MAT_MUL_H
