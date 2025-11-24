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
  printf("   -c             Dump as a C header instead of a simple list\n");
  printf("   -o <path>      Dump roots to file instead of stdout\n");
  printf("   -h             Show this usage information and exit\n");
  exit(exit_code);
}


/**
 * @brief Databento live data client.
 */
static dbn_t dbn;


/**
 * @brief Flag used to stop the program on SIGINT.
 */
static volatile bool siginted = false;


/**
 * @brief Optionable root symbols, sorted.
 */
static char **roots;


/**
 * @brief Number of elements in the roots array.
 */
static size_t roots_count = 0;


/**
 * @brief Add a root symbol to the roots array, maintaining sort order.
 *
 * @param root Root symbol to add (will be copied).
 */
static void add_root(char *root)
{
  /*
   * Binary search for insertion point.
   */
  size_t insertion_point = 0;
  if (roots_count)
  {
    size_t last_index = 0;
    size_t index = roots_count / 2;
    size_t step = roots_count / 2;
    while(true)
    {
      int d = strcmp(root, roots[index]);
      if (!d) return; // Duplicate symbol
      else if (d < 0) // Want to step left
      {
        if (index == 0) // But at the start of the array
        {
          insertion_point = 0;
          break;
        }
        else if (last_index == index - 1) // But just stepped right by 1
        {
          insertion_point = index;
          break;
        }
        else
        {
          last_index = index;
          step /= 2;
          if (!step) step = 1;
          if (step > index) index = 0;
          else index -= step;
        }
      }
      else // Want to step right
      {
        if (index == roots_count - 1) // But at the end of the array
        {
          insertion_point = roots_count;
          break;
        }
        else if (last_index == index + 1) // But just stepped left by 1
        {
          insertion_point = index + 1;
          break;
        }
        else
        {
          last_index = index;
          step /= 2;
          if (!step) step = 1;
          index += step;
          if (index >= roots_count) index = roots_count - 1;
        }
      }
    }
  }


  /*
   * Insert.
   */
  int n = strlen(root);
  char *copy = calloc(1, 1 + n);
  if (!copy)
  {
    perror("calloc");
    abort();
  }
  memcpy(copy, root, n);

  roots_count++;
  roots = realloc(roots, roots_count * sizeof(char *));
  if (!roots)
  {
    perror("realloc");
    abort();
  }

  memmove(&roots[insertion_point + 1], &roots[insertion_point], (roots_count - insertion_point - 1) * sizeof(char *));
  roots[insertion_point] = copy;
}


/**
 * @brief Set when a system message indicates that definition replay
 * is finished.
 */
static volatile bool sdef_done = false;


/**
 * @brief Number of options seen.
 */
static volatile uint64_t num_options = 0;


/**
 * @brief SIGINT handler.
 */
static void on_sigint(int signum)
{
  static int num_sigints = 0;

  siginted = true;
  num_sigints++;

  if (num_sigints == 2)
  {
    dbn_close(&dbn);
  }
  else if (num_sigints > 2)
    abort();
}


/**
 * @brief Handle client errors and warnings by printing to stdout.
 */
static void on_error(dbn_t *dbn, bool fatal, char *msg, void *arg)
{
  if (fatal)
  {
    printf("Client error: %s\n", msg);
    exit(EXIT_FAILURE);
  }
  else
    printf("Client warning: %s\n", msg);
}


/**
 * @brief Handle server errors by printing to stdout.
 */
static void on_emsg(dbn_t *dbn, dbn_emsg_t *emsg, void *arg)
{
  printf("Server error: %s\n", emsg->msg);
}


/**
 * @brief Handle system messages, checking for the special "Finished
 * definition replay" message and ignoring all others.
 */
static void on_smsg(dbn_t *dbn, dbn_smsg_t *smsg, void *arg)
{
  if (!strcmp(smsg->msg, "Finished definition replay"))
    sdef_done = true;
}


/**
 * @brief Handle symbol mapping messages by attempting to decode the
 * symbol as an OSI symbol and then adding the root symbol to the sorted
 * list.
 */
static void on_smap(dbn_t *dbn, dbn_smap_t *smap, void *arg)
{
  osi_t osi;
  if (!osi_parse(smap->stype_out_symbol, &osi)) return; // Not an option contract

  add_root(osi.root);
  num_options++;
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
   * Create a client and register handlers. We're interested mainly in smap
   * messages, which will include the OSI symbol for each option, and smsg
   * messages, because there will be a system message once all definitions
   * have been replayed. Even though we're going to subscribe to symbol
   * definition messages, we don't actually care about them.
   */
  dbn_t dbn;
  dbn_init(&dbn);
  dbn.on_error = on_error;
  dbn.on_emsg = on_emsg;
  dbn.on_smsg = on_smsg;
  dbn.on_smap = on_smap;


  /*
   * Connect.
   */
  printf("Connecting to Databento... ");
  fflush(stdout);

  if (dbn_connect(
    &dbn,
    api_key,
    "OPRA.PILLAR",
    false,
    NULL)) exit(EXIT_FAILURE);

  printf("OK\n");


  /*
   * Subscribe to symbol definitions in intra-day replay mode.
   */
  printf("Subscribing to ALL_SYMBOLS from OPRA.PILLAR dataset, definition schema, intra-day replay... ");
  fflush(stdout);

  const char * const symbols[] = { "ALL_SYMBOLS" };
  if (dbn_start(
    &dbn,
    "definition",
    "parent",
    1, symbols,
    "",
    true,
    NULL)) exit(EXIT_FAILURE);

  printf("OK\n");


  /*
   * Run until sigint or we get a system message saying that definition replay
   * is complete.
   */
  printf("Collecting roots... \x1B[s"); // ANSI save
  fflush(stdout);

  uint64_t num_messages_0 = 0;
  uint64_t num_messages_1 = 0;
  while (!siginted && !sdef_done)
  {
    num_messages_1 += dbn_get(&dbn, NULL);
    if (num_messages_1 > num_messages_0 + 10000)
    {
      printf("\x1B[u%ld (%ld options)", roots_count, num_options); // ANSI restore
      fflush(stdout);
      num_messages_0 = num_messages_1;
    }
  }
  printf("\x1B[u%ld (%ld options)\n", roots_count, num_options); // ANSI restore


  /*
   * Disconnect and free.
   */
  dbn_close(&dbn);


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

  if (as_header)
  {
    write_all(fd, "// Generated by dbn_roots\n"); //
    write_all(fd, "#pragma once\n");
    write_all(fd, "const char * const dbn_roots[] =\n{\n");
  }

  for (size_t i = 0; i < roots_count; i++)
  {
    char *root = roots[i];

    if (as_header) write_all(fd, "  \"");

    write_all(fd, root);
    write_all(fd, ".OPT");

    if (as_header)
    {
      if (i < roots_count - 1) write_all(fd, "\",\n");
      else write_all(fd, "\"\n};\n");
    }
    else write_all(fd, "\n");
  }

  if (path)
  {
    close(fd);
    printf("OK\n");
  }
  return EXIT_SUCCESS;
}
