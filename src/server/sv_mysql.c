#include "server.h"
#include <mysql/mysql.h>

MYSQL *connection = NULL;
MYSQL_RES *results = NULL;
MYSQL_ROW row = NULL;

static cvar_t *sv_mysql;
static cvar_t *sv_mysqlhost;
static cvar_t *sv_mysqldatabase;
static cvar_t *sv_mysqlusername;
static cvar_t *sv_mysqlpassword;

void
sv_mysql_init(void)
{
  qint timeout = 2;

  sv_mysql = Cvar_GetAndDescribe("sv_mysql", "0", CVAR_SERVERINFO, "Toggles whether or not to enable MySQL support.");
  sv_mysqlhost = Cvar_GetAndDescribe("sv_mysqlhost", "localhost", CVAR_ARCHIVE, "Specifies the MySQL host via IP address.");
  sv_mysqldatabase = Cvar_GetAndDescribe("sv_mysqldatabase", "tremulous", CVAR_ARCHIVE, "Specifies the name of the MySQL database.");
  sv_mysqlusername = Cvar_GetAndDescribe("sv_mysqlusername", "root", CVAR_ARCHIVE, "Specifies which MySQL user to use for this database.");
  sv_mysqlpassword = Cvar_GetAndDescribe("sv_mysqlpassword", "", CVAR_ARCHIVE, "Match this with the password of the MySQL user specified by sv_mysqlusername.");

  connection = mysql_init(NULL);
  mysql_options(connection, MYSQL_OPT_CONNECT_TIMEOUT, (qchar *) & (timeout));
  Com_Printf("Connecting to server %s timeout in %d seconds\n", sv_mysqlhost->string, timeout);
  
  if (!mysql_real_connect(connection, sv_mysqlhost->string, sv_mysqlusername->string, sv_mysqlpassword->string, sv_mysqldatabase->string, 0, NULL, 0))
  {
    Com_Printf("^3WARNING:^7 MySQL loading failed: %s\n", mysql_error(connection));
    Cvar_Set("sv_mysql", "0");
  }

  Com_Printf("MySQL loaded version: %s\n", mysql_get_client_info());
  Cvar_Set("sv_mysql", "1");
}


void
sv_mysql_shutdown(void)
{
  mysql_close(connection);
  Com_Printf("MySQL Closed\n");
  Cvar_Set("sv_mysql", "0");
}

void
sv_mysql_reconnect(void)
{
  sv_mysql_shutdown();
  sv_mysql_init();
}

qbool
sv_mysql_runquery(const qchar *query)
{
  if (sv_mysql->integer == 1)
  {
    if (mysql_query(connection, query))
    {
      Com_Printf("^3WARNING:^7 MySQL Query failed: %s. Attempting to reconnect.\n", mysql_error(connection));

      //attempt to reconnect mysql if it failed
      sv_mysql_reconnect();

      if (sv_mysql->integer == 1)
      {
        if (mysql_query(connection, query))
        {
          Com_Printf("^3WARNING:^7 MySQL Query failed: %s\n", mysql_error(connection));
          return qfalse;
        }

        results = mysql_store_result(connection);
        return qtrue;
      }

      return qfalse;
    }

    results = mysql_store_result(connection);
    return qtrue;
  }

  return qfalse;
}

void
sv_mysql_finishquery(void)
{
  if (sv_mysql->integer == 1 && results)
  {
    mysql_free_result(results);
    results = NULL;
  }
}

qbool
sv_mysql_fetchrow(void)
{
  if (sv_mysql->integer == 1)
  {
    row = mysql_fetch_row(results);

    if(!row)
    {
      return qfalse;
    }

    return qtrue;
  }

  return qfalse;
}

void
sv_mysql_fetchfieldbyID(qint id, qchar *buffer, qint len)
{
  if (sv_mysql->integer == 1)
  {
    if (row[id])
    {
      Q_strncpyz(buffer, row[id], len);
    }

    //otherwise do nothing
  }
}

void
sv_mysql_fetchfieldbyName(const qchar *name, qchar *buffer, qint len)
{
  MYSQL_FIELD *fields;
  qint num_fields;
  qint i;

  if (sv_mysql->integer == 1)
  {
    num_fields = mysql_num_fields(results);
    fields = mysql_fetch_fields(results);

    for(i = 0;i < num_fields;i++)
    {
      if(!strcmp(fields[i].name, name))
      {
        Q_strncpyz( buffer, row[i], len );
        return;
      }
    }
  }
}
