/*
 * Initial main.c file generated by Glade. Edit as required.
 * Glade will not overwrite this file.
 */

#include "../../../../configure.h"
#include "../../../../libAfterStep/asapp.h"
#include "../../../../libAfterStep/module.h"
#include "../../../../libAfterConf/afterconf.h"


#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "interface.h"
#include "support.h"


ASWallpaperState WallpaperState ;

int
main (int argc, char *argv[])
{
	GdkDisplay *gdk_display ;
	int i ; 
	static char *deleted_arg = "_deleted_arg_" ;
  	
	InitMyApp (CLASS_ASCP, argc, argv, NULL, NULL, 0 );
	for( i = 1 ; i < argc ; ++i ) 
		if( argv[i] == NULL ) 
			argv[i] = strdup(deleted_arg) ;
  	LinkAfterStepConfig();
  	InitSession();

#ifdef ENABLE_NLS
  	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  	textdomain (GETTEXT_PACKAGE);
#endif

  	gtk_set_locale ();
  	gtk_init (&argc, &argv);

	gdk_display = gdk_display_get_default();
	
	LoadColorScheme();

	ConnectXDisplay (gdk_x11_display_get_xdisplay(gdk_display), NULL, False);
	ReloadASEnvironment( NULL, NULL, NULL, False, True );
	/*MyArgs.display_name*/

  add_pixmap_directory (DEFAULT_PIXMAP_DIR);

  /*
   * The following code was added by Glade to create one of each component
   * (except popup menus), just so that you see something after building
   * the project. Delete any components that you don't want shown initially.
   */
  init_ASWallpaper();

  gtk_main ();
  return 0;
}

