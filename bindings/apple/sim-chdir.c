/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <stdio.h>
#include <unistd.h>

/* simctl spawn starts the child in / instead of the caller's folder, so the CLI is launched after an explicit chdir. */
int main(int argc, char **argv) {
  if (argc < 3) {
    fputs("usage: sim-chdir DIR PROGRAM [ARG...]\n", stderr);
    return 2;
  }
  if (chdir(argv[1]) != 0) {
    perror(argv[1]);
    return 2;
  }
  execv(argv[2], argv + 2);
  perror(argv[2]);
  return 2;
}
