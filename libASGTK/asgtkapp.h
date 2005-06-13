#ifndef ASGTKAPP_HEADER_FILE_INCLUDED
#define ASGTKAPP_HEADER_FILE_INCLUDED


#ifdef __cplusplus
extern "C" {
#endif

void 
init_asgtkapp( int argc, char *argv[], const char *module_name, void (*custom_usage_func) (void), ASFlagType opt_mask);

GdkColor* color_name2GdkColor( const char *name, GdkColor* color );
GtkStyle* get_colorschemed_style_normal();
GtkStyle* get_colorschemed_style_button();

void colorize_gtk_window( GtkWidget *window );
void colorize_gtk_tree_view( GtkCellRenderer *cell, GtkTreeViewColumn *column );
void colorize_gtk_widget(GtkWidget *widget, gpointer data);

#ifdef __cplusplus
}
#endif


#endif /* #ifndef ASGTKAPP_HEADER_FILE_INCLUDED */