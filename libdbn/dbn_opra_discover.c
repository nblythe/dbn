/**
 * @file dbn_opra_discover.c
 * @brief Databento client wrapper that discovers options and optionable roots
 * @author Nathan Blythe
 * @copyright Copyright 2025 Nathan Blythe
 * @copyright Released under the Apache-2.0 license, see LICENSE
 *
 * See README.md for example usage.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include <pthread.h>

#include "dbn.h"
#include "osi.h"
#include "dbn_opra_discover.h"


/**
 * @brief On client error, save the provided error message and transition to
 * the ERROR state.
 */
static void on_error(dbn_t *dbn, bool fatal, char *msg)
{
  dbn_opra_discover_t *discover = dbn->ctx;
  if (fatal)
  {
    if (discover->error) free(discover->error);
    int n = 1 + strlen(msg);
    discover->error = calloc(1, n);
    if (!discover->error)
    {
      perror("calloc");
      abort();
    }

    memcpy(discover->error, msg, n);
    discover->state = DBN_OPRA_DISCOVER_STATE_ERROR;
  }
}


/**
 * @brief Databento message handler.
 */
static void on_msg(dbn_t *dbn, dbn_hdr_t *msg)
{
  dbn_opra_discover_t *discover = dbn->ctx;


  /*
   * On symbol mapping message for an option, find the option's root
   * in the root list (or add it if not listed yet) and add the option
   * to the root.
   */
  if (msg->rtype == DBN_RTYPE_SMAP)
  {
    /*
     * Decode the symbol.
     */
    dbn_smap_t *smap = (void *)msg;
    osi_t osi;
    if (!osi_parse(smap->stype_out_symbol, &osi)) return; // Not an option contract


    /*
     * Binary search the roots array for the insertion point.
     */
    size_t insertion_point = 0;
    bool insertion_needed = true;
    if (discover->num_roots)
    {
      size_t last_index = 0;
      size_t index = discover->num_roots / 2;
      size_t step = discover->num_roots / 2;
      while(true)
      {
        int d = strcmp(osi.root, discover->roots[index].root);
        if (!d) // Root already listed
        {
          insertion_point = index;
          insertion_needed = false;
          break;
        }
        else if (d < 0) // Want to step left
        {
          if (index == 0) // But at the start of the array
          {
            insertion_point = 0;
            insertion_needed = true;
            break;
          }
          else if (last_index == index - 1) // But just stepped right by 1
          {
            insertion_point = index;
            insertion_needed = true;
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
          if (index == discover->num_roots - 1) // But at the end of the array
          {
            insertion_point = discover->num_roots;
            insertion_needed = true;
            break;
          }
          else if (last_index == index + 1) // But just stepped left by 1
          {
            insertion_point = index + 1;
            insertion_needed = true;
            break;
          }
          else
          {
            last_index = index;
            step /= 2;
            if (!step) step = 1;
            index += step;
            if (index >= discover->num_roots) index = discover->num_roots - 1;
          }
        }
      }
    }


    /*
     * Insert new root if needed.
     */
    if (insertion_needed)
    {
      int n = strlen(osi.root);
      char *copy = calloc(1, 1 + n);
      if (!copy)
      {
        perror("calloc");
        abort();
      }
      memcpy(copy, osi.root, n);

      discover->num_roots++;
      discover->roots = realloc(discover->roots, discover->num_roots * sizeof(dbn_opra_discover_root_t));
      if (!discover->roots)
      {
        perror("realloc");
        abort();
      }

      memmove(&discover->roots[insertion_point + 1], &discover->roots[insertion_point], (discover->num_roots - insertion_point - 1) * sizeof(dbn_opra_discover_root_t));
      memset(&discover->roots[insertion_point], 0, sizeof(dbn_opra_discover_root_t));
      discover->roots[insertion_point].root = copy;
    }


    /*
     * Add option to this root.
     */
    dbn_opra_discover_root_t *root = &discover->roots[insertion_point];
    if (root->num_options == root->cap_options)
    {
      root->cap_options = root->cap_options ? 2 * root->cap_options : 64;
      root->options = realloc(root->options, root->cap_options * sizeof(dbn_opra_discover_option_t));
      if (!root->options)
      {
        perror("realloc");
        abort();
      }
    }

    dbn_opra_discover_option_t *option = &root->options[root->num_options++];
    memset(option, 0, sizeof(dbn_opra_discover_option_t));
    option->instrument_id = smap->hdr.instrument_id;
    option->symbol = osi;

    discover->num_options++;
  }


  /*
   * On security definition message, add the security definition to the
   * security definition map.
   */
  else if (msg->rtype == DBN_RTYPE_SDEF)
  {
    dbn_sdef_t *sdef = (void *)msg;

    /*
     * Find the bucket for this security definition, by instrument ID.
     */
    size_t bindex = sdef->hdr.instrument_id % DBN_OPRA_DISCOVER_NUM_SDEF_BUCKETS;
    dbn_opra_discover_sdef_bucket_t *bucket = &discover->sdefs[bindex];


    /*
     * Add the security definition to the bucket.
     */
    if (bucket->count == bucket->capacity)
    {
      bucket->capacity = bucket->capacity ? 2 * bucket->capacity : 4;
      bucket->sdefs = realloc(bucket->sdefs, bucket->capacity * sizeof(dbn_sdef_t));
      if (!bucket->sdefs)
      {
        perror("realloc");
        abort();
      }
    }

    memcpy(&bucket->sdefs[bucket->count], sdef, sizeof(dbn_sdef_t));
    bucket->count++;

    discover->num_sdefs++;
  }


  /*
   * The special "Finished definition replay" message indicates that intra-day
   * replay of instrument definitions is complete, and so discovery can move
   * to cross-referencing of security definitions and options.
   */
  else if (msg->rtype == DBN_RTYPE_SMSG)
  {
    dbn_smsg_t *smsg = (void *)msg;
    if (!strcmp(smsg->msg, "Finished definition replay"))
      discover->state = DBN_OPRA_DISCOVER_STATE_XREF;
  }


  /*
   * On error message, transition to the ERROR state.
   */
  else if (msg->rtype == DBN_RTYPE_EMSG)
  {
    dbn_emsg_t *emsg = (void *)msg;
    if (discover->error) free(discover->error);
    int n = 1 + strlen(emsg->msg);
    discover->error = calloc(1, n);
    if (!discover->error)
    {
      perror("calloc");
      abort();
    }

    memcpy(discover->error, emsg->msg, n);
    discover->state = DBN_OPRA_DISCOVER_STATE_ERROR;
  }
}


/**
 * @brief Worker thread entry point.
 */
static void *worker(void *arg)
{
  dbn_opra_discover_t *discover = arg;


  /*
   * Subscribe to symbol definitions in intra-day replay mode.
   */
  const char * const symbols[] = { "ALL_SYMBOLS" };
  if (dbn_start(
    &discover->dbn,
    "definition",
    "parent",
    1, symbols,
    "",
    true)) return NULL;

  discover->state = DBN_OPRA_DISCOVER_STATE_SUBSCRIBED;


  /*
   * Process messages until stopped, errored out, or done receiving messages.
   */
  while (!discover->stop && discover->state == DBN_OPRA_DISCOVER_STATE_SUBSCRIBED)
  {
    dbn_get(&discover->dbn);
  }


  /*
   * If we finished receiving messages normally and are now in the cross-
   * reference state, cross-reference sdefs to instruments for easy access
   * later.
   */
  for (size_t i = 0; i < discover->num_roots; i++)
  {
    dbn_opra_discover_root_t *root = &discover->roots[i];
    for (size_t j = 0; j < root->num_options; j++)
    {
      dbn_opra_discover_option_t *option = &root->options[j];
      size_t bindex = option->instrument_id % DBN_OPRA_DISCOVER_NUM_SDEF_BUCKETS;
      dbn_opra_discover_sdef_bucket_t *bucket = &discover->sdefs[bindex];
      for (int k = 0; k < bucket->count; k++)
      {
        dbn_sdef_t *sdef = &bucket->sdefs[k];
        if (sdef->hdr.instrument_id == option->instrument_id)
        {
          option->sdef = sdef;
          break;
        }
      }
    }
  }


  /*
   * Now we're actually done.
   */
  discover->state = DBN_OPRA_DISCOVER_STATE_DONE;
  return NULL;
}


void dbn_opra_discover_init(
  dbn_opra_discover_t *discover)
{
  memset(discover, 0, sizeof(dbn_opra_discover_t));
  dbn_init(&discover->dbn, on_error, on_msg, discover);
}


int dbn_opra_discover_start(
  dbn_opra_discover_t *discover,
  const char *api_key)
{
  /*
   * Connect. Everything else we'll do in a different thread.
   */
  int r = dbn_connect(
    &discover->dbn,
    api_key,
    "OPRA.PILLAR",
    false);
  if (r) return r;

  discover->state = DBN_OPRA_DISCOVER_STATE_CONNECTED;


  /*
   * Start the worker thread.
   */
  if (pthread_create(
    &discover->thread,
    NULL,
    worker,
    discover))
  {
    perror("pthread_create");
    abort();
  }

  return 0;
}


void dbn_opra_discover_destroy(dbn_opra_discover_t *discover)
{
  if (discover->state != DBN_OPRA_DISCOVER_STATE_NOT_STARTED)
  {
    discover->stop = true;
    pthread_join(discover->thread, NULL);
    dbn_close(&discover->dbn);

    if (discover->roots)
    {
      for (size_t i = 0; i < discover->num_roots; i++)
        free(discover->roots[i].options);
      free(discover->roots);

      for (size_t i = 0; i < DBN_OPRA_DISCOVER_NUM_SDEF_BUCKETS; i++)
      {
        if (discover->sdefs[i].sdefs)
          free(discover->sdefs[i].sdefs);
      }
    }

    memset(discover, 0, sizeof(dbn_opra_discover_t));
  }
}

