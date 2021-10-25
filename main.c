#include <gtk/gtk.h>
#include <mysql.h>
#include "mysql_index.h"
#include "generic_index_window.h"
#include "index_tree.h"
#include <stdio.h>

#define _GNU_SOURCE
#include <getopt.h>

MYSQL *conn;

#define MAX_USER_LEN     16 /* did "DESC user" in "mysql" database */
#define MAX_PASSWORD_LEN 20
#define MAX_DB_LEN       64 /* http://dev.mysql.com/doc/mysql/en/Legal_names.html */
#define MAX_HOST_LEN     60 /* did "DESC user" in "mysql" database */
#define MAX_PORT_LEN     5
#define MAX_SOCKET_LEN   100

typedef struct {
  GtkWidget *window;
  GtkWidget *userText;
  GtkWidget *passText;
  GtkWidget *hostText;
  GtkWidget *portText;
  GtkWidget *socketText;
  GtkWidget *dbText;
} Login_st;

struct login_params {
  char *host_name;
  char *user_name;
  char *password;
  unsigned int port_num;
  char *socket_name;
  char *db_name;
};

static void
print_error (MYSQL *conn, char *message)
{
  fprintf (stderr, "%s\n", message);
  if (conn != NULL)
  {
    fprintf (stderr, "Error %u (%s)\n",
             mysql_errno (conn), mysql_error (conn));
  }
}

static MYSQL *
do_connect (char *host_name, char *user_name, char *password, char *db_name,
            unsigned int port_num, char *socket_name, unsigned int flags)
{
  MYSQL *conn;

  if (db_name == NULL)
    return NULL;  /* db name is required */

  conn = mysql_init (NULL);
  if (conn == NULL)
  {
    print_error (NULL, "mysql_init() failed (probably out of memory)");
    return (NULL);
  }

#if defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 32200 /* 3.22 and up */
  if (mysql_real_connect (conn, host_name, user_name, password,
          db_name, port_num, socket_name, flags) == NULL)
  {
    print_error (conn, "mysql_real_connect() failed");
    return (NULL);
  }
#else              /* pre-3.22 */
  if (mysql_real_connect (conn, host_name, user_name, password,
                          port_num, socket_name, flags) == NULL)
  {
    print_error (conn, "mysql_real_connect() failed");
    return (NULL);
  }
  if (db_name != NULL)    /* simulate effect of db_name parameter */
  {
    if (mysql_select_db (conn, db_name) != 0)
    {
      print_error (conn, "mysql_select_db() failed");
      mysql_close (conn);
      return (NULL);
    }
  }
#endif
#if defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 4100 /* 4.1.0 and up */
  if (mysql_query(conn, "SET CHARACTER SET utf8") != 0)
    return (NULL);
#endif
  return (conn);
}

static void
do_disconnect (MYSQL *conn)
{
  mysql_close (conn);
}

static gboolean
tryLogin(GtkWidget *button, gpointer login)
{
  const gchar *user;
  const gchar *pass;
  const gchar *host;
  const gchar *db;
  const gchar *port;
  guint        portNum;
  const gchar *sock;
  GtkWidget   *indexTree;

  user = gtk_entry_get_text(GTK_ENTRY(((Login_st *) login)->userText));
  pass = gtk_entry_get_text(GTK_ENTRY(((Login_st *) login)->passText));
  host = gtk_entry_get_text(GTK_ENTRY(((Login_st *) login)->hostText));
  port = gtk_entry_get_text(GTK_ENTRY(((Login_st *) login)->portText));
  sock = gtk_entry_get_text(GTK_ENTRY(((Login_st *) login)->socketText));
  db   = gtk_entry_get_text(GTK_ENTRY(((Login_st *) login)->dbText));

  if (*host == '\0')
    host = NULL;
  if (*sock == '\0')
    sock = NULL;
  if (*port == '\0')
    portNum = 0;
  else
    portNum = atoi(port);

  if ((conn = do_connect ((char *) host, (char *) user, (char *) pass, 
                          (char *) db, (unsigned int) portNum, (char *) sock, 0)) != NULL)
  {
    indexTree = createIndexTreeWindow();
    g_signal_connect (indexTree, "destroy", gtk_main_quit, NULL);
    gtk_widget_destroy(((Login_st *) login)->window);
    g_free(login);
  }

  return FALSE;
}

static int
gui_login(struct login_params *plp)
{
  GtkWidget *loginTable, *userLabel, *passLabel, *button;
  GtkWidget *hostLabel, *portLabel, *socketLabel, *dbLabel;
  Login_st *login;

  login = (Login_st *) g_malloc(sizeof(Login_st) * 1);

  login->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(login->window), "Login");
  gtk_window_set_position(GTK_WINDOW(login->window), GTK_WIN_POS_CENTER_ALWAYS);
  g_signal_connect (login->window, "delete-event", G_CALLBACK(gtk_main_quit), (gpointer) NULL);

  loginTable = gtk_table_new(7, 2, FALSE);

  userLabel = gtk_label_new("User:");
  gtk_misc_set_alignment(GTK_MISC(userLabel), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(userLabel), 5, 5);
  gtk_table_attach(GTK_TABLE(loginTable), userLabel, 0, 1, 0, 1, 
                   GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 3, 3);
  login->userText = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(login->userText), MAX_USER_LEN);
  gtk_table_attach(GTK_TABLE(loginTable), login->userText, 1, 2, 0, 1, 
                   GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 3, 3);

  passLabel = gtk_label_new("Password:");
  gtk_misc_set_alignment(GTK_MISC(passLabel), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(passLabel), 5, 5);
  gtk_table_attach(GTK_TABLE(loginTable), passLabel, 0, 1, 1, 2, 
                   GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 3, 3);
  login->passText = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(login->passText), FALSE);
  gtk_entry_set_max_length(GTK_ENTRY(login->passText), MAX_PASSWORD_LEN);
  gtk_table_attach(GTK_TABLE(loginTable), login->passText, 1, 2, 1, 2, 
                   GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 3, 3);

  dbLabel = gtk_label_new("Database:");
  gtk_misc_set_alignment(GTK_MISC(dbLabel), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(dbLabel), 5, 5);
  gtk_table_attach(GTK_TABLE(loginTable), dbLabel, 0, 1, 2, 3, 
                   GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 3, 3);
  login->dbText = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(login->dbText), MAX_DB_LEN);
  gtk_table_attach(GTK_TABLE(loginTable), login->dbText, 1, 2, 2, 3, 
                   GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 3, 3);

  hostLabel = gtk_label_new("Host:");
  gtk_misc_set_alignment(GTK_MISC(hostLabel), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(hostLabel), 5, 5);
  gtk_table_attach(GTK_TABLE(loginTable), hostLabel, 0, 1, 3, 4, 
                   GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 3, 3);
  login->hostText = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(login->hostText), MAX_HOST_LEN);
  gtk_table_attach(GTK_TABLE(loginTable), login->hostText, 1, 2, 3, 4, 
                   GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 3, 3);

  portLabel = gtk_label_new("Port:");
  gtk_misc_set_alignment(GTK_MISC(portLabel), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(portLabel), 5, 5);
  gtk_table_attach(GTK_TABLE(loginTable), portLabel, 0, 1, 4, 5, 
                   GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 3, 3);
  login->portText = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(login->portText), MAX_PORT_LEN);
  gtk_table_attach(GTK_TABLE(loginTable), login->portText, 1, 2, 4, 5, 
                   GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 3, 3);

  socketLabel = gtk_label_new("Socket:");
  gtk_misc_set_alignment(GTK_MISC(socketLabel), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(socketLabel), 5, 5);
  gtk_table_attach(GTK_TABLE(loginTable), socketLabel, 0, 1, 5, 6, 
                   GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 3, 3);
  login->socketText = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(login->socketText), MAX_SOCKET_LEN);
  gtk_table_attach(GTK_TABLE(loginTable), login->socketText, 1, 2, 5, 6, 
                   GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 3, 3);

  button = gtk_button_new_with_mnemonic("_Login");
  gtk_table_attach(GTK_TABLE(loginTable), button, 0, 2, 6, 7, 
                   GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 3, 3);

  gtk_container_add(GTK_CONTAINER(login->window), 
                    GTK_WIDGET(loginTable));

  if (plp->user_name == NULL)
    plp->user_name = "";
  if (plp->password == NULL)
    plp->password = "";
  if (plp->db_name == NULL)
    plp->db_name = "";
  if (plp->socket_name == NULL)
    plp->socket_name = "";
  if (plp->host_name == NULL)
    plp->host_name = "";
  char port[6];
  snprintf(port, 6, "%i", plp->port_num);

  gtk_entry_set_text(GTK_ENTRY(login->userText), plp->user_name);
  gtk_entry_set_text(GTK_ENTRY(login->passText), plp->password);
  gtk_entry_set_text(GTK_ENTRY(login->dbText), plp->db_name);
  gtk_entry_set_text(GTK_ENTRY(login->hostText), plp->host_name);
  gtk_entry_set_text(GTK_ENTRY(login->portText), port);
  gtk_entry_set_text(GTK_ENTRY(login->socketText), plp->socket_name);

  g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(tryLogin), (gpointer) login);

  gtk_widget_show_all(login->window);
/* why doesn't this work
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(button);
  gtk_window_set_default(GTK_WINDOW(login->window), button);
*/
  return 1;
}

int
main (int argc, char **argv)
{
  int opt, option_index;

  struct login_params lp = { NULL, NULL, NULL, 0, NULL, NULL };

  //const char *groups[] = { "client", "index", NULL };

  struct option long_options[] = {
    {"host",     1, NULL, 'h'},
    {"user",     1, NULL, 'u'},
    {"password", 1, NULL, 'p'},
    {"port",     1, NULL, 'P'},
    {"socket",   1, NULL, 'S'},
    {0,0,0,0}
  };

  gtk_init(&argc, &argv);

  //my_init(); /* initialize load_defaults() */
  //load_defaults ("my", groups, &argc, &argv);

  while ((opt = getopt_long (argc, argv, "h:p::u:P:S:", long_options,
                             &option_index)) != EOF)
  {
    switch (opt)
    {
    case 'h':
      lp.host_name = optarg;
      break;
    case 'u':
      lp.user_name = optarg;
      break;
    case 'p':
      lp.password = optarg;
      break;
    case 'P':
      lp.port_num = (unsigned int) atoi (optarg);
      break;
    case 'S':
      lp.socket_name = optarg;
      break;
    }
  }

  argc -= optind; /* advance past the arguments that were processed */
  argv += optind; /* by getopt_long() */

  if (argc > 0)
  {
    lp.db_name = argv[0];
    --argc; ++argv;
  }

  if ((conn = do_connect ((char *) lp.host_name, (char *) lp.user_name, (char *) lp.password, 
                          (char *) lp.db_name, (unsigned int) lp.port_num, (char *) lp.socket_name, 0)) == NULL)
  {
    /* if can't login from config file or cli opts then load gui login window */
    printf("host: %s\n", lp.host_name);
    printf("pass: %s\n", lp.password);
    printf("user: %s\n", lp.user_name);
    printf("port: %i\n", lp.port_num);
    printf("socket: %s\n", lp.socket_name);
    printf("db: %s\n", lp.db_name);

    gui_login(&lp);
  }
  else
  {
    GtkWidget *indexTree;

    indexTree = createIndexTreeWindow();
    g_signal_connect (indexTree, "destroy", gtk_main_quit, NULL);
  }

  gtk_main();

  do_disconnect(conn);

  return 0;
}
