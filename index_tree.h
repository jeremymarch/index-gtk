GtkWidget *
createIndexTreeWindow();

typedef struct {
  GtkWidget *window;
  GtkWidget *indexView;
  GtkWidget *entry;
  GtkWidget *button;
  GTree     *expandedNodes;
  GtkWidget *buttonExpand;
} Index_Tree_Window;
