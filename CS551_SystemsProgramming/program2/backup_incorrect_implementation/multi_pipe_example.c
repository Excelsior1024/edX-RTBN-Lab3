#include "errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <unistd.h>

/** This simple demo program illustrates the use of multiple pipes
 *  to write data from a parent to multiple children.  The
 *  parent forks n (given by a command-line argument) children
 *  with each child set up to read from a pipe.  The parent reads
 *  stdin and sends successive inputs round-robin to successive
 *  children.  The children read their pipes and echo to stdout
 *  preceeded by identifying info.  When the parent gets EOF
 *  on stdin, it closes the child pipes which then terminate.
 *
 *  Just a play program; not up to expected standards:
 *  fixed-size buffer, quits on errors, does not check errors
 *  properly.
 */

enum { MAX_LINE = 100 }; //fixed size only for demo program

/** Read input from fd and echo to stdout preceeded by id and pid */
static void
doChild(int id, int fd)
{
  FILE *out = stdout;
  char line[MAX_LINE];
  int n;
  while ((n = read(fd, line, MAX_LINE)) > 0) {
    fprintf(out, "%d (%ld): %.*s", id, (long)getpid(), n, line);
  }
  if (n < 0) {
    fatal("%d (%ld): read error:", id, (long)getpid());
  }
}

/** Read input from stdin and write into successive fds[nProcesses]
 *  in a round-robin fashion.  On EOF on stdin, close fds[] and
 *  return.
 */
static void
doParent(int nProcesses, int fds[])
{
  FILE *in = stdin;
  char line[MAX_LINE];
  int childId = 0;
  while (fgets(line, MAX_LINE, in) != NULL) { //no checking for line too long
    int n = strlen(line);
    if (write(fds[childId], line, n) != n) { //imperfect
      fatal("doParent(%d): write():", childId);
    }
    childId = (childId + 1)%nProcesses;
  }
  if (!feof(in)) fatal("doParent(): stdin read error:");
  for (int i = 0; i < nProcesses; i++) {
    if (close(fds[i]) < 0) fatal("doParent(): cannot close fds[%d]:", i);
  }
}

int
main(int argc, const char *argv[])
{
  int nProcesses;
  if (argc != 2 || (nProcesses = atoi(argv[1])) <= 0) {
    fatal("usage: %s <n_processes>", argv[0]);
  }
  int fds[nProcesses]; //remember write fd's from parent to children
  for (int i = 0; i < nProcesses; i++) {
    int fd[2];
    if (pipe(fd) < 0) fatal("cannot create pipe:");
    pid_t pid = fork();
    if (pid < 0) {
      fatal("cannot create child %d:", i);
    }
    else if (pid == 0) { //child
      if (close(fd[1]) < 0) {
        fatal("child %d, pid %ld: close(fd[1]):", i, (long)getpid());
      }
      fprintf(stderr, "created child %d (%ld)\n", i, (long)getpid());
      doChild(i, fd[0]);
      fprintf(stderr, "child %d (%ld) terminating\n", i, (long)getpid());
      exit(0);
    }
    else { //parent
      if (close(fd[0]) < 0) fatal("parent i %d; close(fd(0)):", i);
      fds[i] = fd[1];
    }
    //parent goes around loop once again
  }
  doParent(nProcesses, fds);
  return 0;
}
