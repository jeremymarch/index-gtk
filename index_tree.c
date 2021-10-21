#include <gtk/gtk.h>
#include <mysql.h>
#include "mysql_index.h"
#include "generic_index_window.h"
#include "index_tree.h"

static gboolean
indexGetData(GtkWidget *sourceView, GdkDragContext *context, GtkSelectionData *seldata, 
             guint info, guint time, gpointer data)
{
  GtkTreeModel     *indexStore;
  GtkTreeIter       iter;
  GtkTreeSelection *indexSelection;

  indexStore = gtk_tree_view_get_model(GTK_TREE_VIEW(sourceView));

  indexSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sourceView));

  if (gtk_tree_selection_get_selected(indexSelection, &indexStore, &iter))
  {
/* this works because data_set copies the data so we're not referring to a local variable */
    gint index_id;

    gtk_tree_model_get (indexStore, &iter, 0, &index_id, -1);
    gtk_selection_data_set(seldata,
                         GDK_SELECTION_TYPE_INTEGER,
                         sizeof (gint32)*8,
                         (guchar *) &index_id,
                         sizeof (gint32));
  }
  return FALSE;
}

static gboolean
sigExpandIndex(GtkWidget *button, gpointer indexView)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(button)))
  {
    gtk_label_set_text(GTK_LABEL(GTK_BIN(button)->child), "Collapse All");
    /* doesn't show up till scroll over tree? */
    gtk_tree_view_expand_all (GTK_TREE_VIEW(indexView));
  }
  else
  {
    gtk_label_set_text(GTK_LABEL(GTK_BIN(button)->child), "Expand All");
    gtk_tree_view_collapse_all (GTK_TREE_VIEW(indexView));
  }
  return FALSE;
}

int
requeryTree(gpointer indexTree)
{
  const gchar *text;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(((Index_Tree_Window *)indexTree)->button)))
  {
    gtk_label_set_text(GTK_LABEL(GTK_BIN(((Index_Tree_Window *)indexTree)->button)->child), "Tree View");
    text = gtk_entry_get_text(GTK_ENTRY(((Index_Tree_Window *)indexTree)->entry));

    sauros_requery_index_list(conn, text, GTK_TREE_VIEW(((Index_Tree_Window *)indexTree)->indexView));
  }
  else
  {
    gtk_label_set_text(GTK_LABEL(GTK_BIN(((Index_Tree_Window *)indexTree)->button)->child), "Search");

    sauros_requery_index_tree (conn, GTK_TREE_VIEW(((Index_Tree_Window *)indexTree)->indexView), (gpointer) indexTree);
  }

  return 1;
}

static gboolean
sigToggleIndexView(GtkWidget *button, gpointer indexTree)
{
  requeryTree(indexTree);

  return FALSE;
}

static gboolean
sigCreateIndexWindow(GtkTreeView *indexView, GtkTreePath *path, GtkTreeViewColumn *col, gpointer indexTree)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gint              index_id;
  indexForm        *res;

  selection = gtk_tree_view_get_selection(indexView);
  model =     gtk_tree_view_get_model (indexView);

  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    gtk_tree_model_get (model, &iter, 0, &index_id, -1);

    if ((res = create_index_window(index_id, (gpointer) indexTree, requeryTree)) == NULL)
    {
      g_print("couldn't open edit index window\n");
    }
  }
  return FALSE;
}

static gboolean
sigAddIndexWindow(GtkWidget *button, gpointer indexTree)
{
  indexForm *res;

  if ((res = create_index_window(0, (gpointer) indexTree, requeryTree)) == NULL)
  {
    g_print("couldn't open index window\n");
  }

  return FALSE;
}

static gboolean
sigRequeryIndexTree(GtkEditable *entry, gpointer indexTree)
{
  const gchar *text;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(((Index_Tree_Window *)indexTree)->button)))
  {
    text = gtk_entry_get_text(GTK_ENTRY(entry));
    sauros_requery_index_list(conn, text, GTK_TREE_VIEW(((Index_Tree_Window *)indexTree)->indexView));
  }

  return FALSE;
}

gboolean
myexp(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer indexTree)
{
  gint  key;
  gint *oldKey;
  gint *val;

  gtk_tree_model_get(model, iter, 0, &key, -1);

  if (g_tree_lookup_extended(((Index_Tree_Window *)indexTree)->expandedNodes, (gconstpointer) &key, (gpointer) &oldKey, (gpointer) &val))
  {
    gtk_tree_view_expand_row(GTK_TREE_VIEW(((Index_Tree_Window *)indexTree)->indexView), path, FALSE);
  }
  return FALSE;
}

static gint
numcmp (int *num1, int *num2)
{
  if ((int) *num1 < (int) *num2)
    return -1;
  else if ((int) *num1 == (int) *num2)
    return 0;
  else
    return 1;
}

static gboolean
sigExpandIndexTree(GtkTreeView *treeView, GtkTreeIter *iter, GtkTreePath *path, gpointer indexTree)
{
  GtkTreeModel *model;
  gint *key;
  gint *oldKey;
  gint *val;

  model = gtk_tree_view_get_model(treeView);

  key = (gint *) g_malloc(sizeof(gint) * 1);

  gtk_tree_model_get(model, iter, 0, key, -1);
  /* we dont need to lookup */
  if (!g_tree_lookup_extended(((Index_Tree_Window *)indexTree)->expandedNodes, (gconstpointer) key, (gpointer) &oldKey, (gpointer) &val))
  {
    g_tree_insert (((Index_Tree_Window *)indexTree)->expandedNodes, (gpointer) key, NULL);
  }
  else
  {
    g_free(key);
  }
  return FALSE;
}

static gboolean
sigCollapseIndexTree(GtkTreeView *treeView, GtkTreeIter *iter, GtkTreePath *path, gpointer indexTree)
{
  GtkTreeModel *model;
  gint          index_id;
  gint         *oldKey;
  gpointer     *val;

  model = gtk_tree_view_get_model(treeView);

  gtk_tree_model_get(model, iter, 0, &index_id, -1);

  if (g_tree_lookup_extended(((Index_Tree_Window *)indexTree)->expandedNodes, (gconstpointer) &index_id, (gpointer) &oldKey, (gpointer) &val))
  {
    g_tree_remove(((Index_Tree_Window *)indexTree)->expandedNodes, (gconstpointer) &index_id);
    g_free(oldKey);
  }
  return FALSE;
}

static gboolean
cleanUpIndexTree(GtkWidget *window, GdkEvent *event, Index_Tree_Window *indexTree)
{
/** 
 * can't destroy this until all keys are freed 
 * g_tree_destroy(((Index_Tree_Window *)indexTree)->expandedNodes); 
 */
  g_free(indexTree);
  return FALSE;
}

GtkWidget *
createIndexTreeWindow()
{
  GtkTreeStore      *indexTreeStore;
  GtkWidget         *indexScrolledWindow;
  GtkCellRenderer   *renderer;
  GtkWidget         *buttonAdd, *buttonHbox, *vbox;
  Index_Tree_Window *indexTree;

  indexTree = g_malloc(sizeof(Index_Tree_Window) * 1);

  indexTree->expandedNodes = g_tree_new((GCompareFunc)numcmp);

  indexTree->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW(indexTree->window), 500, 700);
  g_signal_connect (indexTree->window, "delete_event", G_CALLBACK(cleanUpIndexTree), NULL);

  gtk_window_set_title (GTK_WINDOW(indexTree->window), "Index Tree");
  gtk_window_set_position(GTK_WINDOW(indexTree->window), GTK_WIN_POS_CENTER_ALWAYS);

  indexTree->button = gtk_toggle_button_new_with_mnemonic("_Search");

  indexTree->entry = gtk_entry_new();

  /* --- tree --- */
  indexTree->indexView = gtk_tree_view_new ();

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (indexTree->indexView),
                                               -1,
                                               "Index Term",  
                                               renderer,
                                               "text", 1,
                                               NULL);

  indexTreeStore = gtk_tree_store_new (2, G_TYPE_INT, G_TYPE_STRING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (indexTree->indexView), GTK_TREE_MODEL(indexTreeStore));

  indexScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (indexScrolledWindow),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_container_add(GTK_CONTAINER(indexScrolledWindow), GTK_WIDGET(indexTree->indexView));

/*******drag********/

  static GtkTargetEntry targetentries[] =
  {
    { "index id", GTK_TARGET_SAME_APP, TARGET_INDEX_ID }
  };

  gtk_drag_source_set (GTK_WIDGET(indexTree->indexView), GDK_BUTTON1_MASK, targetentries, 1, GDK_ACTION_COPY);

  g_signal_connect (GTK_WIDGET(indexTree->indexView), "drag_data_get", G_CALLBACK(indexGetData), NULL);

/*******drag********/

  buttonAdd = gtk_button_new_with_mnemonic("_Add New Term");
  indexTree->buttonExpand = gtk_toggle_button_new_with_mnemonic("_Expand All");

  buttonHbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(buttonHbox), buttonAdd, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(buttonHbox), indexTree->buttonExpand, TRUE, TRUE, 0);

  vbox = gtk_vbox_new(FALSE, 5);

  gtk_box_pack_start(GTK_BOX(vbox), indexTree->button, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), indexTree->entry, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), indexScrolledWindow, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), buttonHbox, FALSE, TRUE, 0);

  gtk_container_add(GTK_CONTAINER(indexTree->window), GTK_WIDGET(vbox));

  sauros_requery_index_tree (conn, GTK_TREE_VIEW(indexTree->indexView), indexTree);

  g_signal_connect (GTK_OBJECT(indexTree->button), "toggled", G_CALLBACK(sigToggleIndexView), (gpointer) indexTree);

  g_signal_connect (GTK_OBJECT(indexTree->buttonExpand), "toggled", G_CALLBACK(sigExpandIndex), (gpointer) indexTree->indexView);

  g_signal_connect (GTK_OBJECT(buttonAdd), "clicked", G_CALLBACK(sigAddIndexWindow), (gpointer) indexTree);

  g_signal_connect (GTK_OBJECT(indexTree->entry), "changed", G_CALLBACK(sigRequeryIndexTree), (gpointer) indexTree);

  g_signal_connect (GTK_OBJECT(indexTree->indexView), "row-activated", G_CALLBACK(sigCreateIndexWindow), (gpointer) indexTree);

  g_signal_connect (GTK_OBJECT(indexTree->indexView), "row-expanded", G_CALLBACK(sigExpandIndexTree), (gpointer) indexTree);

  g_signal_connect (GTK_OBJECT(indexTree->indexView), "row-collapsed", G_CALLBACK(sigCollapseIndexTree), (gpointer) indexTree);

  gtk_widget_show_all(indexTree->window);

  return indexTree->window;
}
