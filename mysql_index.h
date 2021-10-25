#define MAX_QUERY_LEN 1024
#define MAX_ERROR_LEN 2048

#define NEW_GROUP 0

#define TARGET_INDEX_ID 1

extern char *int10_to_str(long val,char *dst,int radix);

typedef struct {
  GtkWidget *window;
  GtkWidget *termText;
  GtkWidget *parentLabel;
  GtkWidget *parentCombo;
  GtkWidget *scopeLabel;
  GtkWidget *scopeTextView;
  GtkWidget *seeAlsoLabel;
  GtkWidget *seeAlsoText;
  GtkWidget *useTreeView;
  GtkWidget *useAddButton;
  GtkWidget *useDeleteButton;
  GtkWidget *updateButton;
  GtkWidget *deleteButton;
  GtkWidget *insertButton;
  GtkWidget *relatedTreeView;
  GtkWidget *preferredToggle;
  GtkWidget *updatedLabel;
  GtkWidget *updated;
  GtkWidget *createdLabel;
  GtkWidget *created;
  GtkWidget *useScrolledWindow;
  GtkWidget *relatedScrolledWindow;
  GtkWidget *scopeScrolledWindow;
  GtkWidget *relAddButton;
  GtkWidget *relDeleteButton;
  gpointer   cbData; /* pointer to listView in parent window to requery */
  GtkWidget *addUseRelCombo;
  GtkWidget *addUseGroupList;
  GtkStatusbar *statusBar;
  int group;              /* used when adding a USE term--a temporary hack */
  int displayMode;
  int index_id;           /* only member to hold a value from the db */
  int (*cb)(gpointer);
} indexForm;

typedef struct {
  int   id;
  char *indexTerm;
  int   parent;
} indexTree;

enum display_modes {
  INVALID_DISPLAY       = -1,
  NON_PREFERRED_DISPLAY = 0,
  PREFERRED_DISPLAY     = 1,
  NEW_TERM_DISPLAY      = 2
};

enum add_windows {
  ADD_USE_FOR_WINDOW = 0,
  ADD_RELATED_WINDOW
};

enum select_modes {
  SELECT_NON_PREF = 0,
  SELECT_PREF
};

char *
sauros_get_db_error();

int
sauros_requery_index_tree (MYSQL *conn, GtkTreeView *indexTreeView, gpointer indexTree);

int
sauros_requery_index_list(MYSQL *conn, const char *text, GtkTreeView *indexView);

int
sauros_fill_combo_box (MYSQL *conn, GtkComboBox *comboBox, int selected_id, int pref);

int
sauros_requery_combo (MYSQL *conn, const gchar *text, GtkComboBox *combo);

int
sauros_remove_related (MYSQL *conn, int index_id, int related_id);

int
sauros_add_related (MYSQL *conn, int index_id, int related_id);

int
sauros_remove_use_group (MYSQL *conn, int non_pref_id, int preferred_id);

int
sauros_remove_use (MYSQL *conn, int non_pref_id, int preferred_id);

int
sauros_add_use (MYSQL *conn, int non_pref_id, int preferred_id, int *group);

int
sauros_delete_term (MYSQL *conn, int index_id, char *foreignTable, int audoDelNonPref);

int
sauros_update_term (MYSQL *conn, int index_id, const char *term, 
            const char *scope_note, int parent_id, const char *see_also, 
            int preferred);

int
sauros_insert_term (MYSQL *conn, const char *term, const char *scope_note, 
            int parent_id, const char *see_also, int preferred);

int
sauros_num_use_group_members (MYSQL *conn, int index_id, int preferred_id);

int
sauros_select_related (MYSQL *conn, int index_id, GtkListStore *list);

int
sauros_select_use_for (MYSQL *conn, int preferred_id, GtkListStore *list);

int
sauros_select_use (MYSQL *conn, int non_pref_id, GtkListStore *list);

int
sauros_select_use_group (MYSQL *conn, int non_pref_id, int group, 
                   GtkListStore *list);

int
sauros_select_term (MYSQL *conn, int index_id, indexForm *index);

