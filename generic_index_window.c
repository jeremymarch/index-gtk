#include <gtk/gtk.h>
#include <mysql.h>
#include "mysql_index.h"
#include "generic_index_window.h"
#include <stdio.h>
#include <stdlib.h>

#define MAX_MESG_LEN 200

static char messageBuffer[MAX_MESG_LEN];

static void
printStatusMesg(GtkStatusbar *statusBar, gchar *mesg);

static int
sigInsertTerm(GtkWidget *button, gpointer index);

static gboolean
sigUpdateTerm(GtkWidget *button, gpointer index);

static gboolean
sigDeleteTerm(GtkWidget *button, gpointer index);

static gboolean
sigRemoveUseGroup(GtkWidget *button, gpointer index);

static gboolean
sigRemoveUse(GtkWidget *button, gpointer index);

static gboolean
sigAddRelated(GtkWidget *button, gpointer index);

static gboolean
sigRemoveRelated(GtkWidget *button, gpointer index);

static gboolean
sigAddUse(GtkWidget *button, gpointer index);

static gboolean
sigAddUseFor(GtkWidget *button, gpointer index);

static gboolean
sigRequeryCombo(GtkWidget *entry, GdkEventKey *event, gpointer combo);

static GtkWidget *
createComboBoxEntry();

static GtkTreeView *
create_related_list(char *colName);

static GtkTreeView *
create_use_list(char *colName);

static GtkTreeView *
create_use_and_list();

static gboolean
useAndCleanUp(GtkWidget *window, GdkEvent *event, gpointer index);

static GtkWidget *
createIndexAddUseGroup(gint id, gint group, indexForm *index);

static GtkWidget *
createIndexAddWindow(int use_related, indexForm *index);

static gboolean
sigCreateRelWindow(GtkWidget *button, gpointer index);

static gboolean
sigCreateUseForWindow(GtkWidget *button, gpointer index);

static gboolean
sigSelectCreateUseGroup(GtkTreeView *useView, GtkTreePath *path, GtkTreeViewColumn *col, gpointer index);

static gboolean
cleanUp(GtkWidget *window, GdkEvent *event, indexForm *index);

/**********END DECLARATIONS*************/

static void
printStatusMesg(GtkStatusbar *statusBar, gchar *mesg)
{
  guint context_id;

  context_id = gtk_statusbar_get_context_id(statusBar, "status example" );
  gtk_statusbar_push(statusBar, context_id, mesg );
}

/**
 * Insert a new term into the db based on the values in the form
 */
static int
sigInsertTerm(GtkWidget *button, gpointer index)
{
  const char    *term;
  const char    *scope_note;
  const char    *see_also;
  int            parent_id;
  int            preferred;
  GtkTextBuffer *scopeNoteBuffer;
  GtkTextIter    startIter, endIter;
  GtkTreeIter    parentIter;
  GtkTreeModel  *parentModel;

  term = gtk_entry_get_text(GTK_ENTRY(((indexForm *)index)->termText));

  scopeNoteBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(((indexForm *)index)->scopeTextView));
  gtk_text_buffer_get_start_iter(scopeNoteBuffer, &startIter);
  gtk_text_buffer_get_end_iter(scopeNoteBuffer, &endIter);
  scope_note = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(scopeNoteBuffer), 
                                        &startIter, &endIter, FALSE);

  see_also = gtk_entry_get_text(GTK_ENTRY(((indexForm *)index)->seeAlsoText));

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(((indexForm *)index)->preferredToggle)))
    preferred = 1;
  else
    preferred = 0;

  if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(((indexForm *)index)->parentCombo), &parentIter))
  {
    printStatusMesg(((indexForm *)index)->statusBar, "Insert failed: select a parent");
    return FALSE;
  }
  parentModel = gtk_combo_box_get_model(GTK_COMBO_BOX(((indexForm *)index)->parentCombo));
  gtk_tree_model_get(parentModel, &parentIter, 0, &parent_id, -1);

  if (sauros_insert_term(conn, term, scope_note, parent_id, see_also, preferred) < 1)
  {
    snprintf(messageBuffer, MAX_MESG_LEN, "Insert failed: %s", sauros_get_db_error());
    printStatusMesg(((indexForm *)index)->statusBar, messageBuffer);
  }
  else
  {
    printStatusMesg(((indexForm *)index)->statusBar, "Inserted!");

    if (((indexForm *)index)->cb)
    {
      ((indexForm *)index)->cb(((indexForm *)index)->cbData);
    }

    gtk_widget_destroy(((indexForm *)index)->window);
    g_free(index);
  }
  return FALSE;
}

/**
 *
 */
static gboolean
sigUpdateTerm(GtkWidget *button, gpointer index)
{
  const char    *term;
  const char    *scope_note;
  const char    *see_also;
  int            parent_id;
  int            preferred;
  int            res;
  GtkTextBuffer *scopeNoteBuffer;
  GtkTextIter    startIter, endIter;
  GtkTreeIter    parentIter;
  GtkTreeModel  *parentModel;

  term = gtk_entry_get_text(GTK_ENTRY(((indexForm *)index)->termText));

  scopeNoteBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(((indexForm *)index)->scopeTextView));
  gtk_text_buffer_get_start_iter(scopeNoteBuffer, &startIter);
  gtk_text_buffer_get_end_iter(scopeNoteBuffer, &endIter);
  scope_note = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(scopeNoteBuffer),
                                        &startIter, &endIter, FALSE);

  see_also = gtk_entry_get_text(GTK_ENTRY(((indexForm *)index)->seeAlsoText));

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(((indexForm *)index)->preferredToggle)))
    preferred = 1;
  else
    preferred = 0;

  if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(((indexForm *)index)->parentCombo), &parentIter))
  {
    printStatusMesg(((indexForm *)index)->statusBar, "Update failed: select a parent");
    return FALSE;
  }
  parentModel = gtk_combo_box_get_model(GTK_COMBO_BOX(((indexForm *)index)->parentCombo));
  gtk_tree_model_get(parentModel, &parentIter, 0, &parent_id, -1);

  if ((res = sauros_update_term(conn, ((indexForm *)index)->index_id, term, scope_note, parent_id, see_also, preferred)) < 0)
  {
    sauros_select_term(conn, ((indexForm *)index)->index_id, (indexForm *) index); /* reselect old data */

    snprintf(messageBuffer, MAX_MESG_LEN, "Update failed: %s", sauros_get_db_error());
    printStatusMesg(((indexForm *)index)->statusBar, messageBuffer);
  }
  else if (res == 0)
  {
    printStatusMesg(((indexForm *)index)->statusBar, "Updated 0 rows");
  }
  else
  {
    printStatusMesg(((indexForm *)index)->statusBar, "Updated!");

    if (!sauros_select_term(conn, ((indexForm *)index)->index_id, ((indexForm *)index)))
      printStatusMesg(((indexForm *)index)->statusBar, sauros_get_db_error());
  }
  return FALSE;
}

/**
 *
 */
static gboolean
sigDeleteTerm(GtkWidget *button, gpointer index)
{
  if (sauros_delete_term (conn, ((indexForm *)index)->index_id, NULL, 0) > 0)
  {
    printStatusMesg(((indexForm *)index)->statusBar, "Deleted term!");

    if (((indexForm *)index)->cb)
    {
      ((indexForm *)index)->cb(((indexForm *)index)->cbData);
    }

    gtk_widget_destroy(((indexForm *)index)->window);
    g_free(index);
  }
  else
  {
    printStatusMesg(((indexForm *)index)->statusBar, sauros_get_db_error());
  }
  return FALSE;
}

/**
 * Problem:
 * NP term cheesecake--USE cheese AND cake.
 * When we select the term cheese we see that cheesecake is in its USE FOR
 * list, but we don't know that we are to use cheese AND cake for it.  So we 
 * don't have enough information to know if we should be able to remove the 
 * USE FOR cheesecake from cheese.  
 * 1. we could not show non-preferred terms in the USE FOR list if they point 
 * to multiple preferred terms.  They can only be accessed through themselves.
 * 2. we could not allow removing non-preferred terms from the preferred term's
 * USE FOR window.  You have to select the non-pref term and add or remove from there
 */
static gboolean
sigRemoveUseGroup(GtkWidget *button, gpointer index)
{
  int               selected_id;
  int               res;
  GtkTreeModel     *useModel;
  GtkTreeIter       iter;
  GtkTreeSelection *selection;

  useModel  = gtk_tree_view_get_model(GTK_TREE_VIEW(((indexForm *)index)->useTreeView));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(((indexForm *)index)->useTreeView));

  if (!gtk_tree_selection_get_selected(selection, &useModel, &iter))
  {
    printStatusMesg(((indexForm *)index)->statusBar, "Remove failed: No term selected");
    return FALSE;
  }

  gtk_tree_model_get (useModel, &iter, 0, &selected_id, -1);

  if (((indexForm *)index)->displayMode == PREFERRED_DISPLAY)
  {
    /* don't remove non-preferred term if preferred term is part of a USE AND group */
    if (sauros_num_use_group_members (conn, selected_id, ((indexForm *)index)->index_id) != 1)
    {
      printStatusMesg(((indexForm *)index)->statusBar, "Remove failed: term part of USE AND group");
      return FALSE;
    }

    if ((res = sauros_remove_use(conn, selected_id, ((indexForm *)index)->index_id)) < 0)
      printStatusMesg(((indexForm *)index)->statusBar, sauros_get_db_error());
    else if (res == 0)
      printStatusMesg(((indexForm *)index)->statusBar, "removed 0 rows");
    else
      printStatusMesg(((indexForm *)index)->statusBar, "Removed use/use for term!");

    /* requery regardless of result */
    sauros_select_use_for(conn, ((indexForm *)index)->index_id, GTK_LIST_STORE(useModel));
  }
  else
  {
    if ((res = sauros_remove_use_group(conn, ((indexForm *)index)->index_id, selected_id)) < 0)
      printStatusMesg(((indexForm *)index)->statusBar, sauros_get_db_error());
    else if (res == 0)
      printStatusMesg(((indexForm *)index)->statusBar, "removed 0 rows");
    else
      printStatusMesg(((indexForm *)index)->statusBar, "Removed use/use for term!");

    /* requery regardless of result */
    sauros_select_use(conn, ((indexForm *)index)->index_id, GTK_LIST_STORE(useModel));
  }
  return FALSE;
}

static gboolean
sigRemoveUse(GtkWidget *button, gpointer index)
{
  GtkTreeModel     *useModel;
  GtkTreeIter       iter;
  GtkTreeSelection *selection;
  int               preferred_id;

  useModel = gtk_tree_view_get_model(GTK_TREE_VIEW(((indexForm *)index)->addUseGroupList));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(((indexForm *)index)->addUseGroupList));

  if (!gtk_tree_selection_get_selected(selection, &useModel, &iter))
  {
    printStatusMesg(((indexForm *)index)->statusBar, "Remove failed: No term selected");
    return FALSE;
  }

  gtk_tree_model_get (useModel, &iter, 0, &preferred_id, -1);

  if (sauros_remove_use(conn, ((indexForm *)index)->index_id, preferred_id) < 1)
  {
    printStatusMesg(((indexForm *)index)->statusBar, sauros_get_db_error());
  }
  /* requery regardless */
  sauros_select_use_group(conn, ((indexForm *)index)->index_id, ((indexForm *)index)->group, GTK_LIST_STORE(useModel));

  return FALSE;
}

/**
 *
 */
static gboolean
sigAddRelated(GtkWidget *button, gpointer index)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  int           res;
  int           related_id;

  if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(((indexForm *)index)->addUseRelCombo), &iter))
    return FALSE;

  model = gtk_combo_box_get_model(GTK_COMBO_BOX(((indexForm *)index)->addUseRelCombo));
  gtk_tree_model_get(model, &iter, 0, &related_id, -1);

  if ((res = sauros_add_related(conn, ((indexForm *)index)->index_id, related_id)) < 0)
  {
    snprintf(messageBuffer, MAX_MESG_LEN, "Add Related failed: %s", sauros_get_db_error());
    printStatusMesg(((indexForm *)index)->statusBar, messageBuffer);
    return FALSE;
  }
  else if (res == 0)
  {
    snprintf(messageBuffer, MAX_MESG_LEN, "Added 0 Related terms");
    printStatusMesg(((indexForm *)index)->statusBar, messageBuffer);
    return FALSE;
  }

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(((indexForm *)index)->relatedTreeView));

  if (!sauros_select_related(conn, ((indexForm *)index)->index_id, GTK_LIST_STORE(model)))
  {
    printStatusMesg(((indexForm *)index)->statusBar, sauros_get_db_error());
  }
  return FALSE;
}

/**
 *
 */
static gboolean
sigRemoveRelated(GtkWidget *button, gpointer index)
{
  int               related_id;
  GtkTreeModel     *relModel;
  GtkTreeIter       iter;
  GtkTreeSelection *selection;

  relModel = gtk_tree_view_get_model(GTK_TREE_VIEW(((indexForm *)index)->relatedTreeView));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(((indexForm *)index)->relatedTreeView));

  if (!gtk_tree_selection_get_selected(selection, &relModel, &iter))
  {
    printStatusMesg(((indexForm *)index)->statusBar, "Remove failed: No term selected");
    return FALSE;
  }

  gtk_tree_model_get (relModel, &iter, 0, &related_id, -1);

  if (!sauros_remove_related(conn, ((indexForm *)index)->index_id, related_id))
    printStatusMesg(((indexForm *)index)->statusBar, "Remove related term failed");
  else
  {
    sauros_select_related(conn, ((indexForm *)index)->index_id, GTK_LIST_STORE(relModel));
    printStatusMesg(((indexForm *)index)->statusBar, "Removed related term!");
  }
  return FALSE;
}

/**
 *
 */
static gboolean
sigAddUse(GtkWidget *button, gpointer index)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  int           preferred_id;
  int           res;
  int           localGroup;

  /* hack : can't figure out how to pass pointer to struct member? */
  localGroup = ((indexForm *)index)->group;

  if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(((indexForm *)index)->addUseRelCombo), &iter))
    return FALSE;

  model = gtk_combo_box_get_model(GTK_COMBO_BOX(((indexForm *)index)->addUseRelCombo));
  gtk_tree_model_get(model, &iter, 0, &preferred_id, -1);

  if ((res = sauros_add_use(conn, ((indexForm *)index)->index_id, preferred_id, &localGroup)) < 1)
  {
    snprintf(messageBuffer, MAX_MESG_LEN, "Add Use failed: %s", sauros_get_db_error());
    printStatusMesg(((indexForm *)index)->statusBar, messageBuffer);
  }
  /* need to reset group in case we add two terms in one go */
  ((indexForm *)index)->group = localGroup;

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(((indexForm *)index)->addUseGroupList));

  if (!sauros_select_use_group(conn, ((indexForm *)index)->index_id, localGroup, GTK_LIST_STORE(model)))
  {
    printStatusMesg(((indexForm *)index)->statusBar, sauros_get_db_error());
  }
  return FALSE;
}

/**
 *
 */
static gboolean
sigAddUseFor(GtkWidget *button, gpointer index)
{
  int preferred_id;
  GtkTreeModel *model;
  GtkTreeIter   iter;
  int res;
  int group;

  group = 0;

  if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(((indexForm *)index)->addUseRelCombo), &iter))
    return FALSE;

  model = gtk_combo_box_get_model(GTK_COMBO_BOX(((indexForm *)index)->addUseRelCombo));
  gtk_tree_model_get(model, &iter, 0, &preferred_id, -1);

  if ((res = sauros_add_use(conn, preferred_id, ((indexForm *)index)->index_id, &group)) < 1)
  {
    snprintf(messageBuffer, MAX_MESG_LEN, "Add Use failed: %s", sauros_get_db_error());
    printStatusMesg(((indexForm *)index)->statusBar, messageBuffer);
  }
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(((indexForm *)index)->useTreeView));

  if (!sauros_select_use_for(conn, ((indexForm *)index)->index_id, GTK_LIST_STORE(model)))
    printStatusMesg( ((indexForm *) index)->statusBar, sauros_get_db_error());

  return FALSE;
}

static gboolean
sigRequeryCombo(GtkWidget *entry, GdkEventKey *event, gpointer combo)
{
  const gchar *text;
  gint         num_rows;

  text = gtk_entry_get_text (GTK_ENTRY(entry));

  num_rows = sauros_requery_combo(conn, text, GTK_COMBO_BOX(combo));

  if (num_rows > 0)
  {
    gtk_combo_box_popup (GTK_COMBO_BOX(combo));
/*
    gtk_widget_grab_focus(GTK_WIDGET(entry));
    gtk_editable_set_position(GTK_EDITABLE(entry), -1);
*/
  }
  return FALSE;
}

/**
 * create a combobox with 1 int col and 1 string col
 * return the pointer to it.

static GtkWidget *
createComboBox()
{
  GtkWidget       *comboBox;
  GtkListStore    *model;
  GtkCellRenderer *renderer;

  model = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
  comboBox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(model));

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (comboBox), renderer, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (comboBox), renderer, "text", 1);

  return comboBox;
}
*/
static GtkWidget *
createComboBoxEntry()
{
  GtkWidget    *comboBox;
  GtkListStore *model;

  model = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
  comboBox = gtk_combo_box_entry_new_with_model(GTK_TREE_MODEL(model), 1);

  return comboBox;
}

/**
 *
 */
static GtkTreeView *
create_related_list(char *colName)
{
  GtkListStore    *model;
  GtkCellRenderer *renderer;
  GtkTreeView     *treeView;

  model = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
  treeView = (GtkTreeView *) gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeView),
                                               -1,
                                               colName,  
                                               renderer,
                                               "text", 1,
                                               NULL);
  gtk_widget_set_size_request (GTK_WIDGET(treeView), 200, 75);
  return treeView;
}

static GtkTreeView *
create_use_list(char *colName)
{
  GtkListStore    *model;
  GtkCellRenderer *renderer;
  GtkTreeView     *treeView;

  model = gtk_list_store_new (3, G_TYPE_INT, G_TYPE_STRING, G_TYPE_INT);
  treeView = (GtkTreeView *) gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeView),
                                               -1,
                                               colName,  
                                               renderer,
                                               "text", 1,
                                               NULL);
/*
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeView),
                                               -1,
                                               "id",  
                                               renderer,
                                               "text", 0,
                                               NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeView),
                                               -1,
                                               "group",  
                                               renderer,
                                               "text", 2,
                                               NULL);
*/
  gtk_widget_set_size_request (GTK_WIDGET(treeView), 200, 75);
  return treeView;
}

/**
 *
 */
static GtkTreeView *
create_use_and_list()
{
  GtkListStore    *model;
  GtkCellRenderer *renderer;
  GtkTreeView     *treeView;

  model = gtk_list_store_new (3, G_TYPE_INT, G_TYPE_STRING, G_TYPE_BOOLEAN);
  treeView = (GtkTreeView *) gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeView),
                                               -1,
                                               "Use... and",  
                                               renderer,
                                               "text", 1,
                                               NULL);
/*
  renderer = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeView),
                                               -1,
                                               "Remove",  
                                               renderer,
                                               "active", 2,
                                               "activatable", 2,
                                               NULL);
*/
  gtk_widget_set_size_request (GTK_WIDGET(treeView), 50, 75);

  return treeView;
}

static gboolean
useAndCleanUp(GtkWidget *window, GdkEvent *event, gpointer index)
{
  GtkListStore *list;
  ((indexForm *) index)->group = 0; /* reset to zero */
  list = (GtkListStore *) gtk_tree_view_get_model(GTK_TREE_VIEW(((indexForm *) index)->useTreeView));
  sauros_select_use(conn, ((indexForm *) index)->index_id, list);
  return FALSE;
}

static GtkWidget *
createIndexAddUseGroup(gint id, gint group, indexForm *index)
{
  GtkWidget *window;
  GtkWidget *buttonAdd, *buttonRemove;
  GtkWidget *vbox, *hbox, *listScrolledWindow;
  GtkTreeModel *model;

  index->group = group;

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "delete_event", G_CALLBACK(useAndCleanUp), (gpointer) index);
  gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW(window), GTK_WINDOW(index->window));

  gtk_window_set_modal(GTK_WINDOW(window), TRUE);

  vbox = gtk_vbox_new(FALSE, 3);

  index->addUseGroupList = (GtkWidget *) create_use_and_list();
  gtk_widget_set_size_request (index->addUseGroupList, 200, 100);

  listScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (listScrolledWindow),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_container_add(GTK_CONTAINER(listScrolledWindow), 
                    GTK_WIDGET(index->addUseGroupList));

  gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(listScrolledWindow), TRUE, TRUE, 0);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW(index->addUseGroupList));
  if (!sauros_select_use_group(conn, id, group, GTK_LIST_STORE(model)))
  {
    printStatusMesg(index->statusBar, sauros_get_db_error());
    gtk_widget_destroy(GTK_WIDGET(window));
    return NULL;
  }

  index->addUseRelCombo = createComboBoxEntry();
  gtk_combo_box_set_wrap_width (GTK_COMBO_BOX(index->addUseRelCombo), 2);
/*  GTK_COMBO_BOX(index->addUseRelCombo)->priv->has_frame = TRUE; */

/* HERE IS TEST COMBO BOX ENTRY */
  gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX(index->addUseRelCombo), FALSE);

  g_signal_connect_after(G_OBJECT(GTK_BIN(index->addUseRelCombo)->child), "key-release-event", G_CALLBACK(sigRequeryCombo), (gpointer) index->addUseRelCombo); 

  buttonAdd = gtk_button_new_with_mnemonic("_Add");
  buttonRemove = gtk_button_new_with_mnemonic("_Remove");

  hbox = gtk_hbox_new(TRUE, 3);
  gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(buttonAdd), FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(buttonRemove), FALSE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(index->addUseRelCombo), FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), FALSE, TRUE, 0);

  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(vbox));

  if (index->displayMode == PREFERRED_DISPLAY)
  {
    gtk_window_set_title (GTK_WINDOW(window), "Add Use For");

    if (!sauros_fill_combo_box(conn, GTK_COMBO_BOX(index->addUseRelCombo), -1, SELECT_NON_PREF))
    {
      printStatusMesg(index->statusBar, sauros_get_db_error());
      gtk_widget_destroy(GTK_WIDGET(window));
      return NULL;
    }

    g_signal_connect (G_OBJECT(buttonAdd), "clicked", G_CALLBACK(sigAddUseFor), (gpointer) index);
  }
  else
  {
    gtk_window_set_title (GTK_WINDOW(window), "Add Use");

    if (!sauros_fill_combo_box(conn, GTK_COMBO_BOX(index->addUseRelCombo), -1, SELECT_PREF))
    {
      printStatusMesg(index->statusBar, sauros_get_db_error());
      gtk_widget_destroy(GTK_WIDGET(window));
      return NULL;
    }

    g_signal_connect (G_OBJECT(buttonAdd), "clicked", G_CALLBACK(sigAddUse), (gpointer) index);
    g_signal_connect (G_OBJECT(buttonRemove), "clicked", G_CALLBACK(sigRemoveUse), (gpointer) index);
  }
  gtk_widget_show_all(window);
  return window;
}

static GtkWidget *
createIndexAddWindow(int window_type, indexForm *index)
{
  GtkWidget *window;
  GtkWidget *buttonAdd, *buttonCancel;
  GtkWidget *vbox, *hbox;

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size(GTK_WINDOW(window), 200, 0);
  gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW(window), GTK_WINDOW(index->window));
  gtk_window_set_modal(GTK_WINDOW(window), TRUE);

  vbox = gtk_vbox_new(TRUE, 3);

  index->addUseRelCombo = createComboBoxEntry();

  buttonAdd = gtk_button_new_with_mnemonic("_Add");
  buttonCancel = gtk_button_new_with_mnemonic("_Cancel");

  hbox = gtk_hbox_new(TRUE, 3);
  gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(buttonAdd), FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(buttonCancel), FALSE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(index->addUseRelCombo), FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), FALSE, TRUE, 0);

  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(vbox));

  if (window_type == ADD_USE_FOR_WINDOW)
  {
    gtk_window_set_title (GTK_WINDOW(window), "Add Use For");

    if (!sauros_fill_combo_box(conn, GTK_COMBO_BOX(index->addUseRelCombo), -1, SELECT_NON_PREF))
    {
      gtk_widget_destroy(GTK_WIDGET(window));
      return NULL;
    }

    g_signal_connect (G_OBJECT(buttonAdd), "clicked", G_CALLBACK(sigAddUseFor), (gpointer) index);
  }
  else
  {
    gtk_window_set_title (GTK_WINDOW(window), "Add Related");

    if (!sauros_fill_combo_box(conn, GTK_COMBO_BOX(index->addUseRelCombo), -1, SELECT_PREF))
    {
      gtk_widget_destroy(GTK_WIDGET(window));
      return NULL;
    }

    g_signal_connect (G_OBJECT(buttonAdd), "clicked", G_CALLBACK(sigAddRelated), (gpointer) index);
  }
  gtk_widget_show_all(window);
  return window;
}

static gboolean
sigCreateRelWindow(GtkWidget *button, gpointer index)
{
  createIndexAddWindow(ADD_RELATED_WINDOW, (indexForm *) index);
  return FALSE;
}

/**
 * Add a new USE or new USE FOR term
 */
static gboolean
sigCreateUseForWindow(GtkWidget *button, gpointer index)
{
  if (((indexForm *)index)->displayMode == PREFERRED_DISPLAY)
  {
    createIndexAddWindow(ADD_USE_FOR_WINDOW, (indexForm *) index);
  }
  else
  {
    /* -1 because nothing to select for list */
    createIndexAddUseGroup(-1, NEW_GROUP, ((indexForm *) index));
  }
  return FALSE;
}

/**
 * select use term and open window to add or edit other terms to its USE AND group
 * QUESTION: should I select by id and group or id and pref_id?
 */
static gboolean
sigSelectCreateUseGroup(GtkTreeView *useView, GtkTreePath *path, GtkTreeViewColumn *col, gpointer index)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gint              useId;
  gint              useAndGroup;

  if (((indexForm *) index)->displayMode == NON_PREFERRED_DISPLAY)
  {
    selection = gtk_tree_view_get_selection (useView);
    model =     gtk_tree_view_get_model (useView);

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, 0, &useId, 2, &useAndGroup, -1);

      createIndexAddUseGroup (((indexForm *)index)->index_id, useAndGroup, (indexForm *) index);
    }
  }
  return FALSE;
}

gboolean
relDragDataReceived(GtkWidget *destTreeView, GdkDragContext *context, int x, int y,
                        GtkSelectionData *seldata, guint info, guint time,
                        gpointer index)
{
  gint          affected_rows;
  guint         rel_id;
  GtkListStore *relList;

/* is there a prettier way to get an guint out of a pointer to guchar? */

  memcpy(&rel_id, seldata->data, sizeof(guint32));

  gchar *str, *str2;
  relList = (GtkListStore *) gtk_tree_view_get_model(GTK_TREE_VIEW(destTreeView));

  GtkTreeIter iter;
  GtkTreePath *path;

  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW(destTreeView), x, y, &path, NULL, NULL, NULL))
  {
    gtk_tree_model_get_iter(GTK_TREE_MODEL(relList), &iter, path);
    gtk_tree_model_get(GTK_TREE_MODEL(relList), &iter, 1, &str, -1);
    g_print("drag target1: %s\n", str);
    gtk_tree_path_free(path);
  }

/*  it might be safer to get dest row from the selection rather than the position */

  GtkTreeSelection *sel;
  GtkTreeIter iter2;
  sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(destTreeView));
  if (gtk_tree_selection_get_selected (sel, &relList, &iter2))
  {
    gtk_tree_model_get (GTK_TREE_MODEL(relList), &iter2, 1, &str2, -1);
    g_print("drag target2: %s\n", str2);
  }

  if ((affected_rows = sauros_add_related(conn, ((indexForm *) index)->index_id, rel_id)) < 0)
  {
    printStatusMesg(GTK_STATUSBAR(((indexForm *) index)->statusBar), sauros_get_db_error());
  }
  else if (affected_rows == 0)
  {
    printStatusMesg(GTK_STATUSBAR(((indexForm *) index)->statusBar), "Added 0 rows");
  }
  else if (affected_rows > 0)
  {
    relList = (GtkListStore *) gtk_tree_view_get_model(GTK_TREE_VIEW(destTreeView));
    sauros_select_related(conn, ((indexForm *) index)->index_id, relList);
    printStatusMesg(GTK_STATUSBAR(((indexForm *) index)->statusBar), "Added Related");
  }
  return FALSE;
}

/* test */

static gboolean
highlight_expose (GtkWidget *widget,
		  GdkEventExpose *event,
		  gpointer data)
{
	GdkWindow *bin_window;
	int width;
	int height;

	if (GTK_WIDGET_DRAWABLE (widget)) {
		bin_window = 
			gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget));
		
		gdk_drawable_get_size (bin_window, &width, &height);
		
		gtk_paint_focus (widget->style,
				 bin_window,
				 GTK_WIDGET_STATE (widget),
				 NULL,
				 widget,
				 "treeview-drop-indicator",
				 0, 0, width, height);
	}
	
	return FALSE;
}

/* /test */

gboolean
relDragDataMotion(GtkWidget *destTreeView, GdkDragContext *drag_context,
                                            gint x,
                                            gint y,
                                            guint time,
                                            gpointer user_data)
{
  GtkTreePath *path;

  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW(destTreeView), x, y, &path, NULL, NULL, NULL))
  {
/*    gtk_tree_view_set_cursor (GTK_TREE_VIEW(destTreeView), path, NULL, FALSE); */

highlight_expose (GTK_WIDGET(destTreeView),
		  NULL,
		  NULL);

    gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW(destTreeView), path, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);

    gtk_tree_path_free(path);
  }

  return FALSE;
}

gboolean
useDragDataReceived(GtkWidget *destTreeView, GdkDragContext *context, int x, int y,
                        GtkSelectionData *seldata, guint info, guint time,
                        gpointer index)
{
  gint          affected_rows;
  guint         use_id;
  GtkListStore *useList;
  gint          group = 0;

/* is there a prettier way to get an guint out of a pointer to guchar? */
  memcpy(&use_id, seldata->data, sizeof(guint32));

  if (((indexForm *) index)->displayMode == PREFERRED_DISPLAY)
  {
    if ((affected_rows = sauros_add_use(conn, use_id, ((indexForm *) index)->index_id, &group)) < 0)
    {
      printStatusMesg(GTK_STATUSBAR(((indexForm *) index)->statusBar), sauros_get_db_error());
    }
    else if (affected_rows == 0)
    {
      printStatusMesg(GTK_STATUSBAR(((indexForm *) index)->statusBar), "Added 0 rows");
    }
    else if (affected_rows > 0)
    {
      useList = (GtkListStore *) gtk_tree_view_get_model(GTK_TREE_VIEW(destTreeView));
      sauros_select_use_for(conn, ((indexForm *) index)->index_id, useList);
      printStatusMesg(GTK_STATUSBAR(((indexForm *) index)->statusBar), "Added USE FOR");
    }
  }
  else if (((indexForm *) index)->displayMode == NON_PREFERRED_DISPLAY)
  {
    if ((affected_rows = sauros_add_use(conn, ((indexForm *) index)->index_id, use_id, &group)) < 0)
    {
      printStatusMesg(GTK_STATUSBAR(((indexForm *) index)->statusBar), sauros_get_db_error());
    }
    else if (affected_rows == 0)
    {
      printStatusMesg(GTK_STATUSBAR(((indexForm *) index)->statusBar), "Added 0 rows");
    }
    else if (affected_rows > 0)
    {
      useList = (GtkListStore *) gtk_tree_view_get_model(GTK_TREE_VIEW(destTreeView));
      sauros_select_use(conn, ((indexForm *) index)->index_id, useList);
      printStatusMesg(GTK_STATUSBAR(((indexForm *) index)->statusBar), "Added USE");
    }
  }
  return FALSE;
}

int
sauros_change_display (int displayMode, indexForm *index)
{
  GtkTreeViewColumn *col;

  col = gtk_tree_view_get_column (GTK_TREE_VIEW(index->useTreeView), 0);

  switch (displayMode)
  {
    case NON_PREFERRED_DISPLAY:
    gtk_tree_view_column_set_title (col, "Use... or");

    gtk_widget_hide(GTK_WIDGET(index->insertButton));
    gtk_widget_show(GTK_WIDGET(index->updateButton));
    gtk_widget_show(GTK_WIDGET(index->deleteButton));

    gtk_widget_show(GTK_WIDGET(index->useScrolledWindow));
    gtk_widget_show(GTK_WIDGET(index->useTreeView));
    gtk_widget_show(GTK_WIDGET(index->useAddButton));
    gtk_widget_show(GTK_WIDGET(index->useDeleteButton));

    gtk_widget_show(GTK_WIDGET(index->updated));
    gtk_widget_show(GTK_WIDGET(index->updatedLabel));
    gtk_widget_show(GTK_WIDGET(index->created));
    gtk_widget_show(GTK_WIDGET(index->createdLabel));

    gtk_widget_hide(GTK_WIDGET(index->scopeLabel));
    gtk_widget_hide(GTK_WIDGET(index->scopeScrolledWindow));
    gtk_widget_hide(GTK_WIDGET(index->scopeTextView));
    gtk_widget_hide(GTK_WIDGET(index->parentCombo));
    gtk_widget_hide(GTK_WIDGET(index->parentLabel));
    gtk_widget_hide(GTK_WIDGET(index->seeAlsoText));
    gtk_widget_hide(GTK_WIDGET(index->seeAlsoLabel));
    gtk_widget_hide(GTK_WIDGET(index->relatedScrolledWindow));
    gtk_widget_hide(GTK_WIDGET(index->relatedTreeView));
    gtk_widget_hide(GTK_WIDGET(index->relAddButton));
    gtk_widget_hide(GTK_WIDGET(index->relDeleteButton));

    gtk_window_resize(GTK_WINDOW(index->window), 1, 1);

    index->displayMode = NON_PREFERRED_DISPLAY;
    break;

    case PREFERRED_DISPLAY:
    gtk_tree_view_column_set_title (col, "Use For");

    gtk_widget_hide(GTK_WIDGET(index->insertButton));
    gtk_widget_show(GTK_WIDGET(index->updateButton));
    gtk_widget_show(GTK_WIDGET(index->deleteButton));

    gtk_widget_show(GTK_WIDGET(index->scopeLabel));
    gtk_widget_show(GTK_WIDGET(index->scopeScrolledWindow));
    gtk_widget_show(GTK_WIDGET(index->scopeTextView));
    gtk_widget_show(GTK_WIDGET(index->parentCombo));
    gtk_widget_show(GTK_WIDGET(index->parentLabel));
    gtk_widget_show(GTK_WIDGET(index->seeAlsoText));
    gtk_widget_show(GTK_WIDGET(index->seeAlsoLabel));

    gtk_widget_show(GTK_WIDGET(index->updated));
    gtk_widget_show(GTK_WIDGET(index->updatedLabel));
    gtk_widget_show(GTK_WIDGET(index->created));
    gtk_widget_show(GTK_WIDGET(index->createdLabel));

    gtk_widget_show(GTK_WIDGET(index->useScrolledWindow));
    gtk_widget_show(GTK_WIDGET(index->useTreeView));
    gtk_widget_show(GTK_WIDGET(index->useAddButton));
    gtk_widget_show(GTK_WIDGET(index->useDeleteButton));

    gtk_widget_show(GTK_WIDGET(index->relatedScrolledWindow));
    gtk_widget_show(GTK_WIDGET(index->relatedTreeView));
    gtk_widget_show(GTK_WIDGET(index->relAddButton));
    gtk_widget_show(GTK_WIDGET(index->relDeleteButton));

    gtk_window_resize(GTK_WINDOW(index->window), 1, 1);

    index->displayMode = PREFERRED_DISPLAY;
    break;

    case NEW_TERM_DISPLAY:
    gtk_widget_show(GTK_WIDGET(index->insertButton));
    gtk_widget_hide(GTK_WIDGET(index->updateButton));
    gtk_widget_hide(GTK_WIDGET(index->deleteButton));

    gtk_widget_show(GTK_WIDGET(index->scopeLabel));
    gtk_widget_show(GTK_WIDGET(index->scopeScrolledWindow));
    gtk_widget_show(GTK_WIDGET(index->scopeTextView));
    gtk_widget_show(GTK_WIDGET(index->parentCombo));
    gtk_widget_show(GTK_WIDGET(index->parentLabel));
    gtk_widget_show(GTK_WIDGET(index->seeAlsoText));
    gtk_widget_show(GTK_WIDGET(index->seeAlsoLabel));

    gtk_widget_hide(GTK_WIDGET(index->updated));
    gtk_widget_hide(GTK_WIDGET(index->updatedLabel));
    gtk_widget_hide(GTK_WIDGET(index->created));
    gtk_widget_hide(GTK_WIDGET(index->createdLabel));

    gtk_widget_hide(GTK_WIDGET(index->useScrolledWindow));
    gtk_widget_hide(GTK_WIDGET(index->useTreeView));
    gtk_widget_hide(GTK_WIDGET(index->useAddButton));
    gtk_widget_hide(GTK_WIDGET(index->useDeleteButton));

    gtk_widget_hide(GTK_WIDGET(index->relatedScrolledWindow));
    gtk_widget_hide(GTK_WIDGET(index->relatedTreeView));
    gtk_widget_hide(GTK_WIDGET(index->relAddButton));
    gtk_widget_hide(GTK_WIDGET(index->relDeleteButton));

    gtk_window_set_title(GTK_WINDOW(index->window), "Add New Term");
    gtk_window_resize(GTK_WINDOW(index->window), 1, 1);

    sauros_fill_combo_box (conn, GTK_COMBO_BOX(index->parentCombo), 1, SELECT_PREF);

    index->displayMode = NEW_TERM_DISPLAY;
    break;

    default:
    index->displayMode = INVALID_DISPLAY;
    return 0;
    break;
  }
  return 1;
}

static gboolean
cleanUp(GtkWidget *window, GdkEvent *event, indexForm *index)
{
  if (index->cb)
  {
    index->cb(index->cbData);
  }
  gtk_widget_destroy(index->window);
  g_free(index);

  return FALSE;
}

indexForm *
create_index_window(int index_id, gpointer cbData, int (*cb)(gpointer))
{
  GtkWidget *indexTable;
  GtkWidget *termLabel;
  GtkWidget *buttonHBox;
  GtkWidget *relbuttonHBox;
  GtkWidget *usebuttonHBox;
  GtkWidget *preferredLabel;
  GtkWidget *vbox, *hbox, *rightColBox;
  GtkTextBuffer *scopeBuffer;
  indexForm *index;

  index = (indexForm *) g_malloc(sizeof(indexForm) * 1);

  index->displayMode = INVALID_DISPLAY; /* start with impossible value--will be changed in sauros_select_term */
  index->addUseRelCombo = NULL;
  index->addUseGroupList = NULL;
  index->group = -1;
  index->cbData = cbData;
  index->cb = cb;

  static GtkTargetEntry targetentries[] =
  {
    { "index id", GTK_TARGET_SAME_APP, TARGET_INDEX_ID }
  };

  index->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (index->window, "delete_event", G_CALLBACK(cleanUp), (gpointer) index);
  indexTable = gtk_table_new(9, 2, FALSE);

  termLabel = gtk_label_new("Term:");
  gtk_misc_set_alignment(GTK_MISC(termLabel), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(termLabel), 5, 5);
  gtk_table_attach(GTK_TABLE(indexTable), termLabel, 0, 1, 0, 1, 
                   GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  index->termText = gtk_entry_new();
  gtk_table_attach(GTK_TABLE(indexTable), index->termText, 1, 2, 0, 1, 
                   GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);

  index->scopeLabel = gtk_label_new("Scope Note:");
  gtk_widget_set_no_show_all (index->scopeLabel, TRUE);
  gtk_misc_set_alignment(GTK_MISC(index->scopeLabel), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(index->scopeLabel), 5, 5);
  gtk_table_attach(GTK_TABLE(indexTable), index->scopeLabel, 0, 1, 1, 2, 
                   GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);

  scopeBuffer = gtk_text_buffer_new(NULL);
  index->scopeTextView = gtk_text_view_new_with_buffer (scopeBuffer);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(index->scopeTextView), GTK_WRAP_WORD_CHAR);
  gtk_widget_set_size_request (index->scopeTextView, 300, 80);
  index->scopeScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_no_show_all (index->scopeScrolledWindow, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (index->scopeScrolledWindow),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(index->scopeScrolledWindow), 
                    GTK_WIDGET(index->scopeTextView));
  gtk_table_attach(GTK_TABLE(indexTable), index->scopeScrolledWindow, 0, 2, 2, 3, 
                   GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

  index->parentLabel = gtk_label_new("Broader Term:");
  gtk_widget_set_no_show_all (index->parentLabel, TRUE);
  gtk_misc_set_alignment(GTK_MISC(index->parentLabel), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(index->parentLabel), 5, 5);
  gtk_table_attach(GTK_TABLE(indexTable), index->parentLabel, 0, 1, 3, 4, 
                   GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);

  index->parentCombo = createComboBoxEntry();
  gtk_widget_set_no_show_all (index->parentCombo, TRUE);

  gtk_table_attach(GTK_TABLE(indexTable), index->parentCombo, 1, 2, 3, 4,
                   GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);

  index->seeAlsoLabel = gtk_label_new("See also:");
  gtk_widget_set_no_show_all (index->seeAlsoLabel, TRUE);

  gtk_misc_set_alignment(GTK_MISC(index->seeAlsoLabel), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(index->seeAlsoLabel), 5, 5);
  gtk_table_attach(GTK_TABLE(indexTable), index->seeAlsoLabel, 0, 1, 4, 5,
                   GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
  index->seeAlsoText = gtk_entry_new();
  gtk_widget_set_no_show_all (index->seeAlsoText, TRUE);

  gtk_table_attach(GTK_TABLE(indexTable), index->seeAlsoText, 1, 2, 4, 5, 
                   GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);

  preferredLabel = gtk_label_new("Preferred:");
  gtk_misc_set_alignment(GTK_MISC(preferredLabel), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(preferredLabel), 5, 5);
  gtk_table_attach(GTK_TABLE(indexTable), preferredLabel, 0, 1, 5, 6,
                   GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);

  index->preferredToggle = gtk_check_button_new();
  gtk_table_attach(GTK_TABLE(indexTable), index->preferredToggle, 1, 2, 5, 6,
                   GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);

/******updated created*****************/

  index->updatedLabel = gtk_label_new("Updated:");
  gtk_widget_set_no_show_all (index->updatedLabel, TRUE);
  gtk_misc_set_alignment(GTK_MISC(index->updatedLabel), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(index->updatedLabel), 5, 5);
  gtk_table_attach(GTK_TABLE(indexTable), index->updatedLabel, 0, 1, 6, 7, 
                   GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);

  index->updated = gtk_label_new(NULL);
  gtk_widget_set_no_show_all (index->updated, TRUE);
  gtk_misc_set_alignment(GTK_MISC(index->updated), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(index->updated), 5, 5);
  gtk_table_attach(GTK_TABLE(indexTable), index->updated, 1, 2, 6, 7, 
                   GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);

  index->createdLabel = gtk_label_new("Created:");
  gtk_widget_set_no_show_all (index->createdLabel, TRUE);
  gtk_misc_set_alignment(GTK_MISC(index->createdLabel), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(index->createdLabel), 5, 5);
  gtk_table_attach(GTK_TABLE(indexTable), index->createdLabel, 0, 1, 7, 8,
                   GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);

  index->created = gtk_label_new(NULL);
  gtk_widget_set_no_show_all (index->created, TRUE);
  gtk_misc_set_alignment(GTK_MISC(index->created), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC(index->created), 5, 5);
  gtk_table_attach(GTK_TABLE(indexTable), index->created, 1, 2, 7, 8,
                   GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);

/***********main buttons**************/

  buttonHBox = gtk_hbox_new(TRUE, 3);

  index->insertButton = gtk_button_new_with_mnemonic("_Insert");
  index->updateButton = gtk_button_new_with_mnemonic("_Update");
  index->deleteButton = gtk_button_new_with_mnemonic("_Delete");

  gtk_widget_set_no_show_all (index->insertButton, TRUE);
  gtk_widget_set_no_show_all (index->updateButton, TRUE);
  gtk_widget_set_no_show_all (index->deleteButton, TRUE);

  gtk_box_pack_start(GTK_BOX(buttonHBox), GTK_WIDGET(index->insertButton), FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(buttonHBox), GTK_WIDGET(index->updateButton), FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(buttonHBox), GTK_WIDGET(index->deleteButton), FALSE, TRUE, 0);

  gtk_table_attach(GTK_TABLE(indexTable), buttonHBox, 0, 2, 8, 9, 
                   GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);

/*************use***********/

  index->useTreeView = (GtkWidget *) create_use_list("Use... or");
  gtk_widget_set_no_show_all (index->useTreeView, TRUE);

  index->useScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_no_show_all (index->useScrolledWindow, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (index->useScrolledWindow),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_container_add(GTK_CONTAINER(index->useScrolledWindow), 
                    GTK_WIDGET(index->useTreeView));

  rightColBox = gtk_vbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(rightColBox), index->useScrolledWindow, TRUE, TRUE, 0);

  usebuttonHBox = gtk_hbox_new(TRUE, 3);
  index->useAddButton = gtk_button_new_with_mnemonic("_Add");
  index->useDeleteButton = gtk_button_new_with_mnemonic("_Remove");

  gtk_widget_set_no_show_all (index->useAddButton, TRUE);
  gtk_widget_set_no_show_all (index->useDeleteButton, TRUE);

  gtk_box_pack_start(GTK_BOX(usebuttonHBox), GTK_WIDGET(index->useAddButton), FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(usebuttonHBox), GTK_WIDGET(index->useDeleteButton), FALSE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(rightColBox), usebuttonHBox, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT(index->useTreeView), "drag-data-received", G_CALLBACK(useDragDataReceived), (gpointer) index);

  gtk_drag_dest_set (GTK_WIDGET(index->useTreeView), 
                     GTK_DEST_DEFAULT_ALL, targetentries, 1, GDK_ACTION_COPY);
/*
  gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW(index->useTreeView), 
                     targetentries, 1, GDK_ACTION_COPY);
*/
/******related**************/

  index->relatedTreeView = (GtkWidget *) create_related_list("Related Terms");

  index->relatedScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_no_show_all (index->relatedScrolledWindow, TRUE);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (index->relatedScrolledWindow),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_container_add(GTK_CONTAINER(index->relatedScrolledWindow), 
                    GTK_WIDGET(index->relatedTreeView));

  gtk_box_pack_start(GTK_BOX(rightColBox), index->relatedScrolledWindow, TRUE, TRUE, 0);

  relbuttonHBox = gtk_hbox_new(TRUE, 3);
  index->relAddButton = gtk_button_new_with_mnemonic("_Add");
  index->relDeleteButton = gtk_button_new_with_mnemonic("_Remove");
  gtk_widget_set_no_show_all (index->relAddButton, TRUE);
  gtk_widget_set_no_show_all (index->relDeleteButton, TRUE);

  gtk_box_pack_start(GTK_BOX(relbuttonHBox), GTK_WIDGET(index->relAddButton), FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(relbuttonHBox), GTK_WIDGET(index->relDeleteButton), FALSE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(rightColBox), relbuttonHBox, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT(index->relatedTreeView), "drag-data-received", G_CALLBACK(relDragDataReceived), (gpointer) index);
  g_signal_connect (G_OBJECT(index->relatedTreeView), "drag-motion", G_CALLBACK(relDragDataMotion), (gpointer) index);


  gtk_drag_dest_set (GTK_WIDGET(index->relatedTreeView), 
                     GTK_DEST_DEFAULT_ALL, targetentries, 1, GDK_ACTION_COPY);

/**********************/

  index->statusBar = (GtkStatusbar *) gtk_statusbar_new();
  gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR(index->statusBar), FALSE);

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), indexTable, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), rightColBox, FALSE, TRUE, 0);

  vbox = gtk_vbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(index->statusBar), FALSE, TRUE, 0);

  gtk_container_add(GTK_CONTAINER(index->window), vbox);

  if (index_id > 0)
  {
    if (!sauros_select_term(conn, index_id, index))
    {
      g_print("error: %s\n", sauros_get_db_error());

      gtk_widget_destroy(index->window);
      g_free(index);
      return NULL;
    }
  }
  else
  {
    sauros_change_display (NEW_TERM_DISPLAY, index);
  }

  g_signal_connect (G_OBJECT(index->updateButton), "clicked", G_CALLBACK(sigUpdateTerm), (gpointer) index);
  g_signal_connect (G_OBJECT(index->deleteButton), "clicked", G_CALLBACK(sigDeleteTerm), (gpointer) index);
  g_signal_connect (G_OBJECT(index->relAddButton), "clicked", G_CALLBACK(sigCreateRelWindow), (gpointer) index);
  g_signal_connect (G_OBJECT(index->relDeleteButton), "clicked", G_CALLBACK(sigRemoveRelated), (gpointer) index);

  g_signal_connect (G_OBJECT(index->useAddButton), "clicked", G_CALLBACK(sigCreateUseForWindow), (gpointer) index);
  g_signal_connect (G_OBJECT(index->useDeleteButton), "clicked", G_CALLBACK(sigRemoveUseGroup), (gpointer) index);
  g_signal_connect (G_OBJECT(index->useTreeView), "row-activated", G_CALLBACK(sigSelectCreateUseGroup), (gpointer) index);

  g_signal_connect (G_OBJECT(index->insertButton), "clicked", G_CALLBACK(sigInsertTerm), (gpointer) index);

  gtk_widget_show_all (index->window);

  return index;
}
