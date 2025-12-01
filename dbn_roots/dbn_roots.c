/**
 * @file dbn_roots.c
 * @brief Collect optionable root symbols from Databento
 * @author Nathan Blythe
 * @copyright Copyright 2025 Nathan Blythe
 * @copyright Released under the Apache-2.0 license, see LICENSE
 *
 * See README.md for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#include <dbn.h>
#include <osi.h>
#include <dbn_opra_discover.h>


/**
 * @brief Show usage and exit.
 *
 * @param exit_code Exit code with which to call exit().
 *
 * Does not return.
 */
static void usage(int exit_code)
{
  printf("Usage: dbn_roots -k <key> [-h] [-c] [-o <path>]\n");
  printf("\n");
  printf("Options:\n");
  printf("   -k <key>       Databento API key\n");
  printf("   -c             Dump as C header instead of simple list\n");
  printf("   -o <path>      Dump to file instead of stdout\n");
  printf("   -h             Show this usage information and exit\n");
  exit(exit_code);
}


/**
 * @brief Flag used to stop the program on SIGINT.
 */
static volatile bool siginted = false;


/**
 * @brief SIGINT handler.
 */
static void on_sigint(int signum)
{
  if (siginted) abort();
  else siginted = true;
}


/**
 * @brief Write a null-terminated string to a file descriptor, looping as
 * needed to write the full string.
 *
 * @param fd File descriptor
 * @param string Pointer to null-terminated string
 *
 * @return 0 on success or -1 on failure with errno set
 */
static int write_all(int fd, const char *string)
{
  int n = strlen(string);
  int d = 0;
  while (d < n)
  {
    int r = write(fd, string + d, n - d);
    if (r <= 0) return -1;
    else d += r;
  }
  return 0;
}


int main(int argc, char **argv)
{
  /*
   * Parse args.
   */
  char *api_key = NULL;
  bool as_header = false;
  char *path = NULL;
  char c;
  while ((c = getopt(argc, argv, "hk:co:")) != -1)
  {
    switch(c)
    {
      case 'h':
        usage(EXIT_SUCCESS);
      case 'k':
        api_key = optarg;
        break;
      case 'c':
        as_header = true;
        break;
      case 'o':
        path = optarg;
        break;
      case '?':
      default:
        usage(EXIT_FAILURE);
    }
  }

  if (!api_key)
    usage(EXIT_FAILURE);


  /*
   * Register sigint handler.
   */
  signal(SIGINT, on_sigint);


  /*
   * Create a client and connect.
   */
  dbn_opra_discover_t discover;
  dbn_opra_discover_init(&discover);

  printf("Connecting to Databento... ");
  fflush(stdout);

  if (dbn_opra_discover_start(&discover, api_key))
    exit(EXIT_FAILURE);

  printf("OK\n");


  /*
   * Wait to be subscribed.
   */
  printf("Subscribing to OPRA security definitions... ");
  fflush(stdout);
  while (true)
  {
    if (siginted)
    {
      printf("\n");
      printf("Stopping (interrupted)... ");
      fflush(stdout);
      dbn_opra_discover_destroy(&discover);
      printf("OK\n");
      exit(EXIT_SUCCESS);
    }
    else if (discover.state == DBN_OPRA_DISCOVER_STATE_ERROR)
    {
      printf("Failed, %s\n", discover.error);
      printf("Stopping... ");
      fflush(stdout);
      dbn_opra_discover_destroy(&discover);
      printf("OK\n");
      exit(EXIT_FAILURE);
    }
    else if (discover.state == DBN_OPRA_DISCOVER_STATE_SUBSCRIBED)
    {
      printf("OK\n");
       break;
    }
    else usleep(100000); // 100 ms
  }


  /*
   * Wait for all security definitions to be received.
   */
  printf("Discovered \x1B[s"); // ANSI save
  printf("0 roots, 0 options, and 0 definitions... ");
  fflush(stdout);
  while (true)
  {
    if (siginted)
    {
      printf("\nStopping (interrupted)... ");
      fflush(stdout);
      dbn_opra_discover_destroy(&discover);
      printf("OK\n");
      exit(EXIT_SUCCESS);
    }
    else if (discover.state == DBN_OPRA_DISCOVER_STATE_ERROR)
    {
      printf("\nFailed, %s\n", discover.error);
      printf("Stopping... ");
      fflush(stdout);
      dbn_opra_discover_destroy(&discover);
      printf("OK\n");
      exit(EXIT_FAILURE);
    }
    else if (discover.state == DBN_OPRA_DISCOVER_STATE_XREF || discover.state == DBN_OPRA_DISCOVER_STATE_DONE)
    {
      printf("\x1B[u%ld roots, %ld options, and %ld definitions... OK\n", discover.num_roots, discover.num_options, discover.num_sdefs);
      break;
    }
    else
    {
      printf("\x1B[u%ld roots, %ld options, and %ld definitions... ", discover.num_roots, discover.num_options, discover.num_sdefs);
      fflush(stdout);
      usleep(100000); // 100 ms
    }
  }


  /*
   * Wait for cross-referencing to finish.
   */
  printf("Cross-referencing definitions... ");
  fflush(stdout);
  while (true)
  {
    if (siginted)
    {
      printf("\nStopping (interrupted)... ");
      fflush(stdout);
      dbn_opra_discover_destroy(&discover);
      printf("OK\n");
      exit(EXIT_SUCCESS);
    }
    else if (discover.state == DBN_OPRA_DISCOVER_STATE_DONE)
    {
      printf("OK\n");
      break;
    }
    else usleep(100000); // 100 ms
  }


  /*
   * Open the target output file and dump roots.
   */
  int fd;
  if (path != NULL)
  {
    printf("Writing roots to %s... ", path);
    fflush(stdout);
    fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0)
    {
      printf("Failed to open or create %s : %s\n", path, strerror(errno));
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    printf("Writing roots to stdout:\n");
    fd = STDOUT_FILENO;
  }

  for (size_t i = 0; i < discover.num_roots; i++)
  {
    if (as_header) write_all(fd, "  \"");

    write_all(fd, discover.roots[i].root);
    write_all(fd, ".OPT");

    if (as_header)
    {
      if (i < discover.num_roots - 1) write_all(fd, "\",\n");
      else write_all(fd, "\"\n};\n");
    }
    else write_all(fd, "\n");
  }

  if (path)
  {
    close(fd);
    printf("OK\n");
  }


  /*
   * Disconnect / clean up before we go.
   */
  dbn_opra_discover_destroy(&discover);
  return EXIT_SUCCESS;
}
