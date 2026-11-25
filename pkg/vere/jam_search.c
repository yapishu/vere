/// @file

#include "serial.h"
#include "lmdb/lmdb.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static void
_usage(FILE* out)
{
  fputs("usage: jam-search EPOCH PATTERN...\n"
        "Search the EVENTS database of one log epoch, e.g. ship/.urb/log/0i0.\n"
        "Patterns match anywhere in the noun. Use _ for any noun, @ for any atom,\n"
        "%term or an unsigned 64-bit decimal for a literal, and [head tail] for a cell.\n"
        "Print the event number and matching pattern, separated by a tab.\n", out);
}

static int
_search(MDB_cursor* cursor, const u3_jam_query* query, char* const* patterns)
{
  int rc;
  MDB_val key, record;
  while ( MDB_SUCCESS == (rc = mdb_cursor_get(cursor, &key, &record, MDB_NEXT)) ) {
    c3_d event, matches;
    if ( key.mv_size != sizeof(event) ) {
      fputs("jam-search: invalid event key\n", stderr);
      return 1;
    }
    memcpy(&event, key.mv_data, sizeof(event));
    // u3_disk_etch writes a four-byte mug followed by the jammed event.
    const c3_y* bytes = record.mv_data;
    if ( record.mv_size <= 4 ||
         c3n == u3s_jam_query_scan(query, record.mv_size - 4, bytes + 4, &matches) ) {
      fprintf(stderr, "jam-search: invalid jam at event %" PRIu64 "\n", event);
      return 1;
    }
    while ( matches ) {
      unsigned i = __builtin_ctzll(matches);
      if ( printf("%" PRIu64 "\t%s\n", event, patterns[i]) < 0 ) {
        perror("jam-search: stdout");
        return 1;
      }
      matches &= matches - 1;
    }
  }
  if ( rc != MDB_NOTFOUND ) {
    fprintf(stderr, "jam-search: EVENTS: %s\n", mdb_strerror(rc));
    return 1;
  }
  if ( fflush(stdout) == EOF ) {
    perror("jam-search: stdout");
    return 1;
  }
  return 0;
}

int
main(int argc, char** argv)
{
  if ( argc == 2 && !strcmp(argv[1], "--help") ) {
    _usage(stdout);
    return 0;
  }
  if ( argc < 3 ) {
    _usage(stderr);
    return 2;
  }
  char** patterns = argv + 2;
  // Keep each result on one tab-separated line.
  for ( int i = 0; i < argc - 2; i++ ) {
    for ( char* p = patterns[i]; *p; p++ ) {
      if ( *p == '\t' || *p == '\r' || *p == '\n' ) *p = ' ';
    }
  }
  const char* error;
  u3_jam_query* query = u3s_jam_query_compile(argc - 2, (const char* const*)patterns, &error);
  if ( !query ) {
    fprintf(stderr, "jam-search: %s\n", error);
    return 2;
  }

  MDB_env* env = NULL;
  MDB_txn* txn = NULL;
  MDB_cursor* cursor = NULL;
  MDB_dbi db;
  int rc, status = 1;
  if ( (rc = mdb_env_create(&env)) ||
       (rc = mdb_env_set_maxdbs(env, 2)) ||
       (rc = mdb_env_open(env, argv[1], MDB_RDONLY, 0664)) ||
       (rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn)) ||
       (rc = mdb_dbi_open(txn, "EVENTS", 0, &db)) ||
       (rc = mdb_cursor_open(txn, db, &cursor)) ) {
    fprintf(stderr, "jam-search: %s: %s\n", argv[1], mdb_strerror(rc));
  }
  else {
    status = _search(cursor, query, patterns);
    mdb_cursor_close(cursor);
  }
  if ( txn ) mdb_txn_abort(txn);
  if ( env ) mdb_env_close(env);
  u3s_jam_query_free(query);
  return status;
}
