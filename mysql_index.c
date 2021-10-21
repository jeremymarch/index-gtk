#include <gtk/gtk.h>
#include <mysql.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mysql_index.h"

#define USE_INNODB

extern int 
sauros_change_display (int state, indexForm *index);

extern gboolean
myexp(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer indexTree);

static char queryBuffer[MAX_QUERY_LEN];
static char errorBuffer[MAX_ERROR_LEN];

char *
sauros_get_db_error ()
{
  return errorBuffer;
}

#ifdef USE_INNODB
static int
sauros_begin_trans (MYSQL *conn)
{
  return !mysql_query(conn, "START TRANSACTION");
}

static int
sauros_commit_trans (MYSQL *conn)
{
  return !mysql_query(conn, "COMMIT");
}

static int
sauros_rollback_trans (MYSQL *conn)
{
  return !mysql_query(conn, "ROLLBACK");
}
#endif /* USE_INNODB */

static char *
strmov (register char *dst, register const char *src)
{
  while ((*dst++ = *src++)) ;
  return dst-1;
}

/************ALTER FUNCTIONS******************/

static int
sauros_real_remove_related (MYSQL *conn, int index_id, int related_id)
{
  int res; 

  if (!sauros_begin_trans (conn))
    return -1;

  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "DELETE FROM index_related "
          "WHERE (index_id = %i AND related_id = %i) OR "
          "(index_id = %i AND related_id = %i)",
           index_id, related_id, related_id, index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
            queryBuffer, mysql_error(conn));
    return -1;
  }

  if ((res = mysql_affected_rows (conn)) != 2)
    return -1;

  if (!sauros_commit_trans (conn))
    return -1;

  return res;
}

int
sauros_remove_related (MYSQL *conn, int index_id, int related_id)
{
  int res;

  if ((res = sauros_real_remove_related (conn, index_id, related_id)) < 0)
    sauros_rollback_trans (conn);

  return res;
}

static int
sauros_real_add_related (MYSQL *conn, int index_id, int related_id)
{
  MYSQL_RES *res_set;
  int res;

  if (!sauros_begin_trans (conn))
    return -1;

  /* Term cannot be related to itself */
  if (index_id == related_id)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "Term cannot be related to itself");
    return -1;
  }

  /* make sure both terms are preferred and lock them*/
  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "SELECT preferred "
          "FROM index_terms "
          "WHERE index_id IN (%i, %i) AND preferred = 1 "
          "LOCK IN SHARE MODE",
           index_id, related_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }
  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "store result failed");
    return -1;
  }
  if (mysql_num_rows(res_set) != 2)
  {
     snprintf(errorBuffer, MAX_ERROR_LEN, 
            "Both terms must be preferred");
    mysql_free_result(res_set);
    return -1;
  }
  mysql_free_result(res_set);

  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "INSERT INTO index_related VALUES ("
          "%i, %i),(%i, %i)",
           index_id, related_id, related_id, index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }

  res = mysql_affected_rows (conn);

  if (!sauros_commit_trans (conn))
    return -1;

  return res;
}

int
sauros_add_related (MYSQL *conn, int index_id, int related_id)
{
  int res;
  if ((res = sauros_real_add_related (conn, index_id, related_id)) < 0)
    sauros_rollback_trans (conn);

  return res;
}

/**
 * Remove a whole USE AND group
 */
static int
sauros_real_remove_use_group (MYSQL *conn, int non_preferred_id, int preferred_id)
{
  MYSQL_RES *res_set;
  MYSQL_ROW  row;
  int group;
  int res;

  if (!sauros_begin_trans (conn))
    return -1;

  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "SELECT use_and_group "
          "FROM index_use "
          "WHERE index_id = %i AND preferred_id = %i",
           non_preferred_id, preferred_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }
  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "store result failed");
    return -1;
  }
  if ((row = mysql_fetch_row(res_set)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "fetch row failed");
    return -1;
  }
  group = atoi(row[0]);
  mysql_free_result(res_set);

  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "DELETE FROM index_use "
          "WHERE index_id = %i AND use_and_group = %i",
           non_preferred_id, group);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
            queryBuffer, mysql_error(conn));
    return -1;
  }
  res = mysql_affected_rows(conn);

  if (!sauros_commit_trans (conn))
    return -1;

  return res;
}

int
sauros_remove_use_group (MYSQL *conn, int non_preferred_id, int preferred_id)
{
  int res;

  if ((res = sauros_real_remove_use_group (conn, non_preferred_id, preferred_id)) < 0)
    sauros_rollback_trans (conn);

  return res; 
}

static int
sauros_real_remove_use (MYSQL *conn, int non_preferred_id, int preferred_id)
{
  int res;

  if (!sauros_begin_trans (conn))
    return -1;

  snprintf(queryBuffer, MAX_QUERY_LEN,
          "DELETE FROM index_use "
          "WHERE index_id = %i AND preferred_id = %i",
           non_preferred_id, preferred_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
            queryBuffer, mysql_error(conn));
    return -1;
  }

  res = mysql_affected_rows(conn);

  if (!sauros_commit_trans (conn))
    return -1;

  return res;
}

int
sauros_remove_use (MYSQL *conn, int non_preferred_id, int preferred_id)
{
  int res;

  if ((res = sauros_real_remove_use (conn, non_preferred_id, preferred_id)) < 0)
   sauros_rollback_trans (conn);

  return res;
}

/**
 * group is a pointer so we can pass back the actual group assigned
 * if group < 1 then start new group else assign it the group specified by group
 */
int
sauros_real_add_use (MYSQL *conn, int non_pref_id, int pref_id, int *group)
{
  MYSQL_RES *res_set;
  MYSQL_ROW  row;
  int res;

  if (!sauros_begin_trans (conn))
    return -1;

  /* preferred term must not be non-preferred */
  snprintf(queryBuffer, MAX_QUERY_LEN, 
             "SELECT preferred "
             "FROM index_terms "
             "WHERE index_id = %i AND preferred = 1 "
             "LOCK IN SHARE MODE",
              pref_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }
  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "store result failed");
    return -1;
  }
  if (mysql_num_rows(res_set) != 1)
  {
    mysql_free_result(res_set);
    snprintf(errorBuffer, MAX_ERROR_LEN, "Can't USE non-preferred term");
    return -1;
  }
  /* non-preferred term must not be preferred */
  snprintf(queryBuffer, MAX_QUERY_LEN, 
             "SELECT preferred "
             "FROM index_terms "
             "WHERE index_id = %i AND preferred = 0 "
             "LOCK IN SHARE MODE",
              non_pref_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }
  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "store result failed");
    return -1;
  }
  if (mysql_num_rows(res_set) != 1)
  {
    mysql_free_result(res_set);
    snprintf(errorBuffer, MAX_ERROR_LEN, "Can't USE FOR preferred term");
    return -1;
  }

  if (*group == NEW_GROUP) /* start a new group */
  {
    snprintf(queryBuffer, MAX_QUERY_LEN, 
             "SELECT MAX(use_and_group) "
             "FROM index_use " /* for update ? */
             "WHERE index_id = %i",
              non_pref_id);

    if (mysql_query(conn, queryBuffer) != 0)
    {
      snprintf(errorBuffer, MAX_ERROR_LEN, 
              "query failed: %s\nmysql error: %s", 
               queryBuffer, mysql_error(conn));
      return -1;
    }
    if ((res_set = mysql_store_result (conn)) == NULL)
    {
      snprintf(errorBuffer, MAX_ERROR_LEN, "store result failed");
      return -1;
    }
    if ((row = mysql_fetch_row (res_set)) == NULL)
    {
      mysql_free_result(res_set);
      return -1;
    }
    if (row[0] == NULL) /* MAX SQL function returns NULL if no rows found */
      *group = 1; /* new group is the first one */
    else
      *group = atoi(row[0]) + 1; /* make new group 1 greater than max */

    mysql_free_result(res_set);
  }

  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "INSERT INTO index_use VALUES ("
          "%i, %i, %i)",
           non_pref_id, pref_id, *group);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }

  res = mysql_affected_rows(conn);

  if (!sauros_commit_trans (conn))
    return -1;

  return res;
}

int
sauros_add_use (MYSQL *conn, int non_pref_id, int pref_id, int *group)
{
  int res;

  if ((res = sauros_real_add_use (conn, non_pref_id, pref_id, group)) < 0)
    sauros_rollback_trans (conn);

  return res;
}

static int
sauros_real_insert_term (MYSQL *conn, const char *term, const char *scope_note, int parent_id, 
            const char *see_also, int preferred)
{
  char *end;
  int   res;

  if (!sauros_begin_trans (conn))
    return -1;

  if (strlen(term) < 1)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "Term cannot be zero length");
    return -1;
  }

/* non-preferred terms can't have parents or children */
  if (!preferred && parent_id)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "Non-preferred terms can't have parents");
    return -1;
  }

  end = strmov(queryBuffer, "INSERT INTO index_terms "
               "(index_id, term, scope_note, see_also, "
               "parent_id, preferred, updated_by, "
               "created, created_by) VALUES (NULL, '");
  end += mysql_real_escape_string(conn, end, term, strlen(term));
  end = strmov(end, "', '");
  end += mysql_real_escape_string(conn, end, scope_note, strlen(scope_note));
  end = strmov(end, "', '");
  end += mysql_real_escape_string(conn, end, see_also, strlen(see_also));
  end = strmov(end, "', ");
  end = int10_to_str(parent_id, end, 10);
  end = strmov(end, ", ");
  end = int10_to_str(preferred, end, 10);
  end = strmov(end, ", CURRENT_USER(), NOW(), CURRENT_USER())");

  if (mysql_real_query(conn, queryBuffer, end - queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query: %s\nError: %s", queryBuffer, mysql_error(conn));
    return -1;
  }

  res = mysql_affected_rows(conn);

  if (!sauros_commit_trans (conn))
    return -1;

  return res;
}

int
sauros_insert_term (MYSQL *conn, const char *term, const char *scope_note, int parent_id, 
            const char *see_also, int preferred)
{
  int res;

  if ((res = sauros_real_insert_term (conn, term, scope_note, parent_id, 
                                      see_also, preferred)) < 0)
    sauros_rollback_trans (conn);

  return res;
}

/* select parent's parent_id until we find 0 or the word_id of the word we're moving 
 * return 1 for OK to move 0 for not OK, -1 for error
 */
static int
sauros_validate_parent (MYSQL *conn, int new_parent_id, int index_id)
{
  MYSQL_RES *res_set;
  MYSQL_ROW  row;
  int depth = 0;

  while (depth < 500) /* set a maximum depth to prevent infinite loop */
  {
    if (new_parent_id == 0)
    {
      return 1; /* ok--term not in parent's path to root */
    }
    if (new_parent_id == index_id)
    {
      snprintf(errorBuffer, MAX_ERROR_LEN, "Recursion Error!");
      return 0; /* invalid--found term in parent's path to root */
    }

    snprintf(queryBuffer, MAX_QUERY_LEN, "SELECT parent_id "
                         "FROM index_terms "
                         "WHERE index_id = %i", new_parent_id);

    if (mysql_query (conn, queryBuffer) != 0)
    {
      snprintf(errorBuffer, MAX_ERROR_LEN,  
               "Query failed. Mysql Error: %s", mysql_error(conn));
      return -1;
    }

    if ((res_set = mysql_store_result (conn)) == NULL)
    {
      snprintf(errorBuffer, MAX_ERROR_LEN, "Couldn't create result set");
      return -1;
    }

    if ((row = mysql_fetch_row(res_set)) == NULL)
    {
      snprintf(errorBuffer, MAX_ERROR_LEN, "Couldn't fetch row");
      return -1;
    }
    new_parent_id = atoi(row[0]);

    mysql_free_result(res_set);
    depth++;
  }
  snprintf(errorBuffer, MAX_ERROR_LEN, 
           "Error: Reached maximum depth: %i. "
           "Your tree probably has a circular path to root.", depth);
  return -1; /* we should never get here */
}

/**
 * need to add code to check for parent-child consistency for update and 
 * insert and deleteTerm functions
 */
static int
sauros_real_update_term (MYSQL *conn, int index_id, const char *term, const char *scope_note, 
                         int parent_id, const char *see_also, int preferred)
{
  char *end;
  MYSQL_RES *res_set;
  int res;

  if (!sauros_begin_trans (conn))
    return -1;

  if (strlen(term) < 1)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "Term cannot be zero length");
    return -1;
  }

/* non-preferred terms can't be children */
  if (!preferred && parent_id)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "Non-preferred terms can't have parents");
    return -1;
  }

/* non-preferred terms can't have children */
  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "SELECT index_id "
          "FROM index_terms "
          "WHERE parent_id = %i LIMIT 1",
           index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
             "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }
  if ((res_set = mysql_store_result(conn)) == NULL)
    return -1;

  if (!preferred && mysql_num_rows(res_set) > 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "Non-preferred terms can't have children");
    mysql_free_result(res_set);
    return -1;
  }
  mysql_free_result(res_set);

/* check for recursion errors */
  if (sauros_validate_parent(conn, parent_id, index_id) < 1)
    return -1;

/* pref term can't be pointed to by pref terms */
  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "SELECT preferred_id "
          "FROM index_use "
          "WHERE index_id = %i LIMIT 1",
           index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
             "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }
  if ((res_set = mysql_store_result(conn)) == NULL)
    return -1;

  if (preferred && mysql_num_rows(res_set) > 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
             "references preferred terms");
    mysql_free_result(res_set);
    return -1;
  }
  mysql_free_result(res_set);

/* non-pref term can't point to non-pref terms */
  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "SELECT preferred_id "
          "FROM index_use "
          "WHERE preferred_id = %i LIMIT 1",
           index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }
  if ((res_set = mysql_store_result(conn)) == NULL)
    return -1;

  if (!preferred && mysql_num_rows(res_set) > 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
             "references non-preferred terms");
    mysql_free_result(res_set);
    return -1;
  }
  mysql_free_result(res_set);

/* non-pref terms can't have related terms */
  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "SELECT related_id "
          "FROM index_related "
          "WHERE index_id = %i OR related_id = %i LIMIT 1",
           index_id, index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }
  if ((res_set = mysql_store_result(conn)) == NULL)
    return -1;

  if (!preferred && mysql_num_rows(res_set) > 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
             "references related terms");
    mysql_free_result(res_set);
    return -1;
  }
  mysql_free_result(res_set);

  end = strmov(queryBuffer, "UPDATE index_terms SET term='");
  end += mysql_real_escape_string(conn, end, term, strlen(term));
  end = strmov(end, "', scope_note='");
  end += mysql_real_escape_string(conn, end, scope_note, strlen(scope_note));
  end = strmov(end, "', see_also='");
  end += mysql_real_escape_string(conn, end, see_also, strlen(see_also));
  end = strmov(end, "', parent_id=");
  end = int10_to_str(parent_id, end, 10);
  end = strmov(end, ", preferred=");
  end = int10_to_str(preferred, end, 10);
  end = strmov(end, ", updated_by = CURRENT_USER() ");
  end = strmov(end, " WHERE index_id=");
  end = int10_to_str(index_id, end, 10);

  if (mysql_real_query(conn, queryBuffer, end - queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "update term query failed");
    return -1;
  }

  res = mysql_affected_rows(conn);

  if (!sauros_commit_trans (conn))
    return -1;

  return res;
}

int
sauros_update_term (MYSQL *conn, int index_id, const char *term, const char *scope_note, 
                    int parent_id, const char *see_also, int preferred)
{
  int res;

  if ((res = sauros_real_update_term (conn, index_id, term, scope_note, 
                                      parent_id, see_also, preferred)) < 0)
    sauros_rollback_trans (conn);

  return res;
}

static int
sauros_real_delete_term (MYSQL *conn, int index_id, char *foreignTable, int autoDelNonPref)
{
  MYSQL_RES *res_set;
  MYSQL_ROW  row;
  int res;

  if (!sauros_begin_trans (conn))
    return -1;

  /* optionally check that term isn't referred to elsewhere */
  if (foreignTable)
  {
    snprintf(queryBuffer, MAX_QUERY_LEN, 
            "SELECT index_id FROM %s "
            "WHERE index_id = %i LIMIT 1",
             foreignTable, index_id);

    if (mysql_query(conn, queryBuffer) != 0)
    {
      snprintf(errorBuffer, MAX_ERROR_LEN, 
              "query failed: %s\nmysql error: %s", 
               queryBuffer, mysql_error(conn));
      return -1;
    }

    if ((res_set = mysql_store_result (conn)) == NULL)
    {
      snprintf(errorBuffer, MAX_ERROR_LEN, "store result failed");
      return -1;
    }
    /* term referenced in a foreign table--abort */
    if (mysql_num_rows(res_set) > 0)
    {
      snprintf(errorBuffer, MAX_ERROR_LEN, "Delete failed: term referenced in %s", foreignTable);
      mysql_free_result(res_set);
      return -1;
    }
  }

  /* don't delete if has children */
  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "SELECT index_id FROM index_terms "
          "WHERE parent_id = %i LIMIT 1",
           index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }

  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "store result failed");
    return -1;
  }

  if (mysql_num_rows(res_set) > 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "Delete failed: term has children");
    mysql_free_result(res_set);
    return -1;
  }

/* don't delete if has related terms */
  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "SELECT related_id "
          "FROM index_related "
          "WHERE index_id = %i OR related_id = %i LIMIT 1",
           index_id, index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }
  if ((res_set = mysql_store_result(conn)) == NULL)
    return -1;

  if (mysql_num_rows(res_set) > 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
             "references related terms");
    mysql_free_result(res_set);
    return -1;
  }
  mysql_free_result(res_set);

/** 
 * optionally return error if this term references 
 * non-preferred terms--must delete them first. 
 * otherwise automatically delete them after 
 * deleting preferred term.
 */
  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "SELECT preferred_id "
          "FROM index_use "
          "WHERE index_id = %i "
          "UNION SELECT index_id "
          "FROM index_use "
          "WHERE preferred_id = %i",
           index_id, index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }
  if ((res_set = mysql_store_result(conn)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "Delete failed: store results failed");
    return -1;
  }
  if (!autoDelNonPref && mysql_num_rows(res_set) > 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "Delete failed: references another term");
    mysql_free_result(res_set);
    return -1;
  }

  /* DELETE term from all 3 tables */
  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "DELETE FROM index_related "
          "WHERE index_id = %i",
           index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }

  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "DELETE FROM index_use "
          "WHERE index_id = %i",
           index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }

  snprintf(queryBuffer, MAX_QUERY_LEN, 
          "DELETE FROM index_use "
          "WHERE preferred_id = %i",
           index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
             "query failed: %s\nmysql error: %s", 
              queryBuffer, mysql_error(conn));
    return -1;
  }

  snprintf(queryBuffer, MAX_QUERY_LEN,
          "DELETE FROM index_terms "
          "WHERE index_id = %i",
           index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }

  /*now delete all non-preferred terms if autoDelNonPref is true.
    if autoDelNonPref is false row will be Null and loop won't execute
   */
  while ((row = mysql_fetch_row (res_set)) != NULL)
  {
    if (sauros_delete_term (conn, atoi(row[0]), foreignTable, autoDelNonPref) < 1)
      return -1;
  }
  res = mysql_affected_rows(conn);

  if (!sauros_commit_trans (conn))
    return -1;

  return res;
}

int
sauros_delete_term (MYSQL *conn, int index_id, char *foreignTable, int autoDelNonPref)
{
  int res;

  if ((res = sauros_real_delete_term (conn, index_id, foreignTable, autoDelNonPref)) < 0)
    sauros_rollback_trans (conn);

  return res;
}

/**
 * returns how many terms are part of its USE AND group
 */
int
sauros_num_use_group_members (MYSQL *conn, int index_id, int preferred_id)
{
  MYSQL_RES *res_set;
  MYSQL_ROW  row;
  int group_members;

  snprintf(queryBuffer, MAX_QUERY_LEN, 
           "SELECT COUNT(*) "
           "FROM index_use "
           "WHERE index_id = %i AND use_and_group = "
           "(SELECT use_and_group "
           "FROM index_use "
           "WHERE index_id = %i AND preferred_id = %i)", 
           index_id, index_id, preferred_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }

  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "store result failed");
    return -1;
  }
  if ((row = mysql_fetch_row (res_set)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "fetch row failed");
    return -1;
  }
  group_members = atoi(row[0]);
  mysql_free_result(res_set);

  return group_members;
}

/**
 * return:
 *  1 = preferred
 *  0 = non-preferred
 * -1 = error
 */
int
sauros_is_preferred (MYSQL *conn, int index_id)
{
  MYSQL_RES *res_set;
  MYSQL_ROW  row;
  int preferred;

  snprintf(queryBuffer, MAX_QUERY_LEN, 
           "SELECT preferred "
           "FROM index_terms "
           "WHERE index_id = %i", 
           index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return -1;
  }

  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "store result failed");
    return -1;
  }
  if ((row = mysql_fetch_row (res_set)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "fetch row failed");
    return -1;
  }
  preferred = atoi(row[0]);
  mysql_free_result(res_set);

  return preferred;
}

/*************SELECT FUNCTIONS***********************/
/*********INDEX TREE FUNCTIONS**********************/

static void
sauros_print_row (int wordId, char *word, int parent, 
                  GtkTreeIter *parentIter, GtkTreeStore *indexTreeStore, 
                  indexTree *row, int num_rows)
{
  GtkTreeIter iter;
  int i;

  gtk_tree_store_append (indexTreeStore, &iter, parentIter);
  gtk_tree_store_set (indexTreeStore, &iter,
                      0, wordId,
                      1, word,
                      -1);

  for (i = 0; i < num_rows; i++)
  {
    if (row[i].parent == wordId)
    {
      sauros_print_row(row[i].id, row[i].indexTerm, row[i].parent, 
                    &iter, indexTreeStore, row, num_rows);
    }
  }
}

int
sauros_requery_index_tree (MYSQL *conn, GtkTreeView *indexTreeView, gpointer myindexTree)
{
  GtkTreeModel *indexTreeStore;
  MYSQL_RES    *res_set;
  MYSQL_ROW     row;
  int           num_rows;

  indexTreeStore = gtk_tree_view_get_model (GTK_TREE_VIEW(indexTreeView));
  gtk_tree_store_clear(GTK_TREE_STORE(indexTreeStore));

  gchar query[] = "SELECT index_id, term, parent_id "
                  "FROM index_terms "
                  "WHERE preferred = 1 "
                  "ORDER BY term";

  if (mysql_query (conn, query) != 0)
  {
    g_print("Query failed:");
    return 1;
  }

  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    g_print("Couldn't create result set");
    return 2;
  }

  num_rows = mysql_num_rows(res_set);
  indexTree indexTerms[num_rows]; /* variable length array--new in C99! */

  int i = 0;
  while ((row = mysql_fetch_row (res_set)) != NULL)
  {
    if (row[2]) /* ommit NULL used for root node */
    {
      indexTerms[i].id = atoi(row[0]);
      indexTerms[i].indexTerm = row[1];
      indexTerms[i].parent = atoi(row[2]);
      i++;
    }
  }

  for (i = 0; i < num_rows; i++)
  {
    if (indexTerms[i].parent == 0)
    {
      sauros_print_row (indexTerms[i].id, indexTerms[i].indexTerm, 
                    indexTerms[i].parent, NULL, GTK_TREE_STORE(indexTreeStore), 
                    indexTerms, num_rows);
    }
  }
  mysql_free_result (res_set);

  gtk_tree_model_foreach(indexTreeStore, myexp, (gpointer) myindexTree);

  return 0;
}

int
sauros_requery_index_list(MYSQL *conn, const char *text, GtkTreeView *indexView)
{
  GtkTreeStore *indexTreeStore;
  MYSQL_RES    *res_set;
  MYSQL_ROW     row;
  GtkTreeIter   iter;

  indexTreeStore = (GtkTreeStore *) gtk_tree_view_get_model (GTK_TREE_VIEW(indexView));
  gtk_tree_store_clear(GTK_TREE_STORE(indexTreeStore));

  snprintf(queryBuffer, MAX_QUERY_LEN, 
           "SELECT index_id, term "
           "FROM index_terms "
           "WHERE term LIKE '%s%%' "
           "ORDER BY term "
           "LIMIT 25",
            text);

  if (mysql_query (conn, queryBuffer) != 0)
  {
    g_print("Query failed:");
    return -1;
  }

  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    g_print("Couldn't create result set");
    return -1;
  }

  while ((row = mysql_fetch_row(res_set)) != NULL)
  {
    gtk_tree_store_append (indexTreeStore, &iter, NULL);
    gtk_tree_store_set (indexTreeStore, &iter,
                        0, atoi(row[0]),
                        1, row[1],
                        -1);
  }
  return 1;
}

/******END INDEX TREE FUNCTIONS*********************/

/**
 * Select up to 25 terms which are LIKE text and load them into combo
 */
int
sauros_requery_combo (MYSQL *conn, const gchar *text, GtkComboBox *combo)
{
  MYSQL_RES     *res_set;
  MYSQL_ROW      row;
  GtkListStore  *list;
  GtkTreeIter    iter;
  int            num_rows;

  list = (GtkListStore *) gtk_combo_box_get_model(combo);
  gtk_list_store_clear(list);

  snprintf(queryBuffer, MAX_QUERY_LEN, 
           "SELECT index_id, term "
           "FROM index_terms "
           "WHERE term LIKE '%s%%' AND preferred = 1 "
           "ORDER BY term LIMIT 25",
            text);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
             "query failed: %s\nmysql error: %s", 
              queryBuffer, mysql_error(conn));
    return 0;
  }

  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    g_print("Couldn't create result set");
    return 0;
  }

  num_rows = mysql_num_rows(res_set);

/* 
  This is only needed when adding Parents not Use terms
  or we could add "None" as a real term
  gtk_list_store_append(list, &iter);
  gtk_list_store_set(list, &iter, 0, 0, 1, "None", -1);
*/
  while ((row = mysql_fetch_row(res_set)) != NULL)
  {
    int index_id;
    index_id = atoi(row[0]);

    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter, 0, index_id, 1, row[1], -1);
  }
  mysql_free_result(res_set);

  return num_rows;
}

/**
 * fill the combo box with all preferred or non-preferred terms as indicated by
 * pref.  Select the term which matches selected_id if one matches it.
 */ 
int
sauros_fill_combo_box (MYSQL *conn, GtkComboBox *comboBox, int selected_id, int pref)
{
  MYSQL_RES     *res_set;
  MYSQL_ROW      row;
  GtkListStore  *list;
  GtkTreeIter    iter;

  list = (GtkListStore *) gtk_combo_box_get_model(comboBox);
  gtk_list_store_clear(list);

  snprintf(queryBuffer, MAX_QUERY_LEN, 
           "SELECT index_id, term "
           "FROM index_terms "
           "WHERE preferred = %i "
           "ORDER BY term",
            pref);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
             "query failed: %s\nmysql error: %s", 
              queryBuffer, mysql_error(conn));
    return 0;
  }

  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    g_print("Couldn't create result set");
    return 0;
  }

/* or we could add "None" as a real term */
  gtk_list_store_append(list, &iter);
  gtk_list_store_set(list, &iter, 0, 0, 1, "None", -1);

  if (selected_id == 0)
    gtk_combo_box_set_active_iter (comboBox, &iter);

  while ((row = mysql_fetch_row(res_set)) != NULL)
  {
    int index_id;
    index_id = atoi(row[0]);

    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter, 0, index_id, 1, row[1], -1);

    if (selected_id == index_id)
      gtk_combo_box_set_active_iter (comboBox, &iter);
  }
  mysql_free_result(res_set);

  return 1;
}

int
sauros_select_related (MYSQL *conn, int index_id, GtkListStore *list)
{
  MYSQL_RES  *res_set;
  MYSQL_ROW   row;
  GtkTreeIter iter;

  gtk_list_store_clear(list);

  snprintf(queryBuffer, MAX_QUERY_LEN, 
           "SELECT a.index_id, a.term "
           "FROM index_terms AS a INNER JOIN index_related AS b "
           "ON a.index_id = b.related_id "
           "WHERE b.index_id = %i "
           "ORDER BY a.term", 
           index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return 0;
  }

  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "store result failed");
    return 0;
  }

  while ((row = mysql_fetch_row (res_set)) != NULL)
  {
    gtk_list_store_append (list, &iter);
    gtk_list_store_set (list, &iter,
                        0, atoi(row[0]),
                        1, row[1],
                        -1);
  }
  mysql_free_result(res_set);
  return 1;
}

int
sauros_select_use_for (MYSQL *conn, int preferred_id, GtkListStore *list)
{
  MYSQL_RES *res_set;
  MYSQL_ROW  row;
  GtkTreeIter iter;

  gtk_list_store_clear(list);

/**
 * The dependent subquery excludes terms involved in USE AND groups so only
 * terms that are the only member of their use_and_group are selected.
 * WHERE ... AND (SELECT COUNT(*) FROM index_use as c WHERE 
 * c.index_id = a.index_id AND c.use_and_group = a.use_and_group) < 2
 *
 * The problem is that then you don't see that it does reference another term
 * and don't understand why you can't delete it!
 *
 * Maybe I should make the terms visible but only allow removing them from 
 * the non-preferred term.

  snprintf(queryBuffer, MAX_QUERY_LEN, 
           "SELECT b.index_id, b.term "
           "FROM index_use AS a INNER JOIN index_terms AS b "
           "ON a.index_id = b.index_id "
           "WHERE a.preferred_id = %i "
           "AND (SELECT COUNT(*) FROM index_use AS c WHERE c.index_id = a.index_id AND c.use_and_group = a.use_and_group) < 2 "
           "ORDER BY b.term", 
           preferred_id);
*/
  snprintf(queryBuffer, MAX_QUERY_LEN, 
           "SELECT b.index_id, b.term "
           "FROM index_use AS a "
           "INNER JOIN index_terms AS b "
           "ON a.index_id = b.index_id "
           "WHERE a.preferred_id = %i "
           "ORDER BY b.term", 
           preferred_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return 0;
  }

  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "store result failed");
    return 0;
  }

  while ((row = mysql_fetch_row (res_set)) != NULL)
  {
    gtk_list_store_append (list, &iter);
    gtk_list_store_set (list, &iter,
                        0, atoi(row[0]),
                        1, row[1],
                        -1);
  }
  return 1;
}

int
sauros_select_use (MYSQL *conn, int non_pref_id, GtkListStore *list)
{
  MYSQL_RES  *res_set;
  MYSQL_ROW   row;
  int num_rows;
  GtkTreeIter iter;

  gtk_list_store_clear(list);

  snprintf(queryBuffer, MAX_QUERY_LEN, 
           "SELECT b.index_id, b.term, a.use_and_group "
           "FROM index_use AS a INNER JOIN index_terms AS b "
           "ON a.preferred_id = b.index_id "
           "WHERE a.index_id = %i "
           "ORDER BY a.use_and_group, b.term", 
           non_pref_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return 0;
  }

  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "store result failed");
    return 0;
  }
  num_rows = mysql_num_rows(res_set);

  if ((row = mysql_fetch_row (res_set)) == NULL)
    return 0;

  GString *str;
  int      lastId;
  int      lastGroup;
  int      newGroup;
  int      i;

  str = g_string_new(row[1]);
  lastId    = atoi(row[0]);
  lastGroup = atoi(row[2]);

  for (i = 0; i < num_rows; i++)
  {
    /* assign newGroup here so we only have to atoi() row[2] once */
    for ( ; ((row = mysql_fetch_row (res_set)) != NULL) && lastGroup == (newGroup = atoi(row[2])) ; i++ )
    {
      g_string_append(str, " AND ");
      g_string_append(str, row[1]);
    }

    gtk_list_store_append (list, &iter);
    gtk_list_store_set (list, &iter,
                        0, lastId,
                        1, str->str,
                        2, lastGroup,
                        -1);

    if (row != NULL)
    {
      g_string_truncate(str, 0);

      g_string_append(str, row[1]);
      lastId    = atoi(row[0]);
      lastGroup = newGroup;
    }
  }
  mysql_free_result(res_set);
  g_string_free (str, TRUE);
  return 1;
}

/**
 * Select all the preferred terms that belong to a USE group for the indicated
 * non-preferred term, non_pref_id, and load them into the list store
 */
int
sauros_select_use_group (MYSQL *conn, int non_pref_id, int group, GtkListStore *list)
{
  MYSQL_RES  *res_set;
  MYSQL_ROW   row;
  GtkTreeIter iter;

  gtk_list_store_clear(list);

  snprintf(queryBuffer, MAX_QUERY_LEN,
           "SELECT b.index_id, b.term "
           "FROM index_use AS a INNER JOIN index_terms AS b "
           "ON a.preferred_id = b.index_id "
           "WHERE a.index_id = %i AND a.use_and_group = %i "
           "ORDER BY b.term", 
           non_pref_id, group);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return 0;
  }

  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "store result failed");
    return 0;
  }

  while ((row = mysql_fetch_row (res_set)) != NULL)
  {
    gtk_list_store_append (list, &iter);
    gtk_list_store_set (list, &iter,
                        0, atoi(row[0]),
                        1, row[1],
                        -1);
  }
  return 1;
}

int
sauros_select_term (MYSQL *conn, int index_id, indexForm *index)
{
  MYSQL_RES         *res_set;
  MYSQL_ROW          row;
  int                newPreferred;
  gboolean           boolPreferred;
  GtkTextBuffer     *textBuf;
  gchar              title[50];
  GtkTreeModel      *model;

  snprintf(queryBuffer, MAX_QUERY_LEN, 
           "SELECT index_id, term, see_also, scope_note, updated, "
           "created, preferred, parent_id "
           "FROM index_terms "
           "WHERE index_id = %i", 
           index_id);

  if (mysql_query(conn, queryBuffer) != 0)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, 
            "query failed: %s\nmysql error: %s", 
             queryBuffer, mysql_error(conn));
    return 0;
  }

  if ((res_set = mysql_store_result (conn)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "store result failed");
    return 0;
  }
  if ((row = mysql_fetch_row (res_set)) == NULL)
  {
    snprintf(errorBuffer, MAX_ERROR_LEN, "fetch row failed");
    mysql_free_result(res_set);
    return 0;
  }

  textBuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(index->scopeTextView));

  index->index_id = atoi(row[0]);
  
  newPreferred = atoi(row[6]);

  if (newPreferred == 0)
    boolPreferred = FALSE;
  else
    boolPreferred = TRUE;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(index->preferredToggle), boolPreferred);

  gtk_entry_set_text(GTK_ENTRY(index->termText), row[1]);
  gtk_text_buffer_set_text(textBuf, row[3], strlen(row[3]));
  gtk_entry_set_text(GTK_ENTRY(index->seeAlsoText), row[2]);
  gtk_label_set_text(GTK_LABEL(index->updated), row[4]);
  gtk_label_set_text(GTK_LABEL(index->created), row[5]);

  if (!sauros_fill_combo_box (conn, GTK_COMBO_BOX(index->parentCombo), atoi(row[7]), SELECT_PREF))
    return 0;

  snprintf(title, 50, "Index Term: %s", row[1]);
  gtk_window_set_title(GTK_WINDOW(index->window), title);

  mysql_free_result(res_set);

  /* compare preferred to display mode to determine if we need to change display */
  if (index->displayMode != NON_PREFERRED_DISPLAY && newPreferred == 0)
  {
    sauros_change_display (NON_PREFERRED_DISPLAY, index);
  }
  else if (index->displayMode != PREFERRED_DISPLAY && newPreferred == 1)
  {
    sauros_change_display (PREFERRED_DISPLAY, index);
  }

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(index->useTreeView));
  if (index->displayMode == PREFERRED_DISPLAY)
  {
    sauros_select_use_for(conn, index_id, GTK_LIST_STORE(model));
  }
  else
  {
    sauros_select_use(conn, index_id, GTK_LIST_STORE(model));
  }

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(index->relatedTreeView));
  if (!sauros_select_related(conn, index_id, GTK_LIST_STORE(model)))
    g_print("Error: %s\n", sauros_get_db_error());

  return 1;
}
