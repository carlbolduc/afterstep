/****************************************************************************
 *
 * Copyright (c) 1999 Sasha Vasko <sasha at aftercode.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.   See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 ****************************************************************************/

#include "../configure.h"

#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#include "../include/asapp.h"
#include "../include/afterstep.h"
#include "../include/parser.h"
#include "../include/screen.h"
#include "../include/functions.h"
#include "../include/session.h"

ASProgArgs    MyArgs = {0, NULL, 0, NULL, NULL, NULL, 0, 0};/* some typical progy cmd line options - set by SetMyArgs( argc, argv )*/
char         *MyName = NULL;                    /* name are we known by */
char          MyClass[MAX_MY_CLASS+1]="unknown";/* application Class name ( Pager, Wharf, etc. ) - set by SetMyClass(char *) */
void        (*MyVersionFunc)   (void) = NULL;
void        (*MyUsageFunc)   (void) = NULL;

char     *as_gnustep_dir_name   = GNUSTEP;
char     *as_gnusteplib_dir_name= GNUSTEP "/" GNUSTEPLIB;
char     *as_afterstep_dir_name = GNUSTEP "/" GNUSTEPLIB "/" AFTER_DIR;
char     *as_save_dir_name      = GNUSTEP "/" GNUSTEPLIB "/" AFTER_DIR "/" AFTER_SAVE;
char     *as_start_dir_name     = GNUSTEP "/" GNUSTEPLIB "/" AFTER_DIR "/" START_DIR;
char     *as_share_dir_name     = AFTER_SHAREDIR;

int       fd_width;

unsigned int  nonlock_mods = 0;				   /* a mask for non-locking modifiers */
unsigned int *lock_mods = NULL;				   /* all combinations of lock modifier masks */
/* Now for each display we may have one or several screens ; */
ScreenInfo    Scr;							   /* ScreenInfo for the default screen */
int SingleScreen = -1 ;						   /* if >= 0 then [points to the only ScreenInfo structure available */
int PointerScreen = 0;						   /* screen that currently has pointer */
unsigned int  NumberOfScreens = 0; 			   /* number of screens on display */
/* unused - future development : */
ScreenInfo  **all_screens = NULL ;             /* all ScreenInfo structures for NumberOfScreens screens */
ASHashTable  *screens_window_hash =NULL	;	   /* so we can easily track what window is on what screen */
/* end of: unused - future development : */

/* TODO: need to initialize Default* things to something sensible */
struct ASEnvironment *DefaultEnv = NULL;

struct ASFeel *DefaultFeel = NULL;/* unused - future development : */
struct MyLook *DefaultLook = NULL;/* unused - future development : */

/* Base config : */
char         *PixmapPath = NULL;
char         *CursorPath = NULL;
char         *IconPath   = NULL;
char         *ModulePath = AFTER_BIN_DIR;
char         *FontPath   = NULL;

struct ASSession *Session = NULL;          /* filenames of look, feel and background */

/* names of AS functions - used all over the place  :*/

#define FUNC_TERM(txt,len,func)         {TF_NO_MYNAME_PREPENDING,txt,len,TT_TEXT,func,NULL, NULL}
#define FUNC_TERM2(flags,txt,len,func)  {TF_NO_MYNAME_PREPENDING|(flags),txt,len,TT_TEXT,func,NULL, NULL}

TermDef       FuncTerms[F_FUNCTIONS_NUM + 1] = {
	FUNC_TERM2 (NEED_NAME, "Nop", 3, F_NOP),   /* Nop      "name"|"" */
	FUNC_TERM2 (NEED_NAME, "Title", 5, F_TITLE),	/* Title    "name"    */
	FUNC_TERM ("Beep", 4, F_BEEP),			   /* Beep               */
	FUNC_TERM ("Quit", 4, F_QUIT),			   /* Quit     ["name"] */
	FUNC_TERM2 (NEED_NAME | NEED_CMD, "Restart", 7, F_RESTART),	/* Restart "name" WindowManagerName */
	FUNC_TERM ("Refresh", 7, F_REFRESH),	   /* Refresh  ["name"] */
#ifndef NO_VIRTUAL
	FUNC_TERM ("Scroll", 6, F_SCROLL),		   /* Scroll     horiz vert */
	FUNC_TERM ("GotoPage", 8, F_GOTO_PAGE),	   /* GotoPage   x     y    */
	FUNC_TERM ("TogglePage", 10, F_TOGGLE_PAGE),	/* TogglePage ["name"]   */
#endif
	FUNC_TERM ("CursorMove", 10, F_MOVECURSOR),	/* CursorMove horiz vert */
	FUNC_TERM2 (NEED_WINIFNAME, "WarpFore", 8, F_WARP_F),	/* WarpFore ["name" window_name] */
	FUNC_TERM2 (NEED_WINIFNAME, "WarpBack", 8, F_WARP_B),	/* WarpBack ["name" window_name] */
	FUNC_TERM2 (NEED_NAME | NEED_WINDOW, "Wait", 4, F_WAIT),	/* Wait      "name" window_name  */
	FUNC_TERM ("Desk", 4, F_DESK),			   /* Desk arg1 [arg2] */
#ifndef NO_WINDOWLIST
	FUNC_TERM ("WindowList", 10, F_WINDOWLIST),	/* WindowList [arg1 arg2] */
#endif
	FUNC_TERM2 (NEED_NAME, "PopUp", 5, F_POPUP),	/* PopUp    "popup_name" [popup_name] */
	FUNC_TERM2 (NEED_NAME, "Function", 8, F_FUNCTION),	/* Function "function_name" [function_name] */
#ifndef NO_TEXTURE
	FUNC_TERM ("MiniPixmap", 10, F_MINIPIXMAP),	/* MiniPixmap "name" */
#endif
	FUNC_TERM2 (NEED_NAME | NEED_CMD, "Exec", 4, F_EXEC),	/* Exec   "name" command */
	FUNC_TERM2 (NEED_NAME | NEED_CMD, "Module", 6, F_MODULE),	/* Module "name" command */
	FUNC_TERM2 (NEED_NAME | NEED_CMD, "KillModuleByName", 16, F_KILLMODULEBYNAME),	/* KillModuleByName "name" module */
	FUNC_TERM2 (NEED_NAME | NEED_CMD, "QuickRestart", 12, F_QUICKRESTART),	/* QuickRestart "name" what */
	FUNC_TERM2 (NEED_NAME | NEED_CMD, "Background", 10, F_CHANGE_BACKGROUND),	/* Background "name" file_name */
	FUNC_TERM2 (NEED_NAME | NEED_CMD, "ChangeLook", 10, F_CHANGE_LOOK),	/* ChangeLook "name" file_name */
	FUNC_TERM2 (NEED_NAME | NEED_CMD, "ChangeFeel", 10, F_CHANGE_FEEL),	/* ChangeFeel "name" file_name */
    FUNC_TERM2 (NEED_NAME | NEED_CMD, "ChangeTheme", 11, F_CHANGE_THEME), /* ChangeTheme "name" file_name */
	FUNC_TERM2 (TF_SYNTAX_TERMINATOR, "EndFunction", 11, F_ENDFUNC),
	FUNC_TERM2 (TF_SYNTAX_TERMINATOR, "EndPopup", 8, F_ENDPOPUP),
    FUNC_TERM2 (NEED_NAME | NEED_CMD, "Test", 4, F_Test),

	/* this functions require window as aparameter */
	FUNC_TERM ("&nonsense&", 10, F_WINDOW_FUNC_START),	/* not really a command */
	FUNC_TERM ("Move", 4, F_MOVE),			   /* Move     ["name"] */
	FUNC_TERM ("Resize", 6, F_RESIZE),		   /* Resize   ["name"] */
	FUNC_TERM ("Raise", 5, F_RAISE),		   /* Raise    ["name"] */
	FUNC_TERM ("Lower", 5, F_LOWER),		   /* Lower    ["name"] */
	FUNC_TERM ("RaiseLower", 10, F_RAISELOWER),	/* RaiseLower ["name"] */
	FUNC_TERM ("PutOnTop", 8, F_PUTONTOP),	   /* PutOnTop  */
	FUNC_TERM ("PutOnBack", 9, F_PUTONBACK),   /* PutOnBack */
	FUNC_TERM ("SetLayer", 8, F_SETLAYER),	   /* SetLayer    layer */
	FUNC_TERM ("ToggleLayer", 11, F_TOGGLELAYER),	/* ToggleLayer layer1 layer2 */
	FUNC_TERM ("Shade", 5, F_SHADE),		   /* Shade    ["name"] */
	FUNC_TERM ("Delete", 6, F_DELETE),		   /* Delete   ["name"] */
	FUNC_TERM ("Destroy", 7, F_DESTROY),	   /* Destroy  ["name"] */
	FUNC_TERM ("Close", 5, F_CLOSE),		   /* Close    ["name"] */
	FUNC_TERM ("Iconify", 7, F_ICONIFY),	   /* Iconify  ["name"] value */
	FUNC_TERM ("Maximize", 8, F_MAXIMIZE),	   /* Maximize ["name"] [hori vert] */
	FUNC_TERM ("Stick", 5, F_STICK),		   /* Stick    ["name"] */
	FUNC_TERM ("Focus", 5, F_FOCUS),		   /* Focus */
	FUNC_TERM2 (NEED_WINIFNAME, "ChangeWindowUp", 14, F_CHANGEWINDOW_UP),	/* ChangeWindowUp   ["name" window_name ] */
	FUNC_TERM2 (NEED_WINIFNAME, "ChangeWindowDown", 16, F_CHANGEWINDOW_DOWN),	/* ChangeWindowDown ["name" window_name ] */
    FUNC_TERM2 (NEED_WINIFNAME, "GoToBookmark", 12, F_GOTO_BOOKMARK),   /* GoToBookmark ["name" window_bookmark ] */
	FUNC_TERM ("GetHelp", 7, F_GETHELP),	   /* */
	FUNC_TERM ("PasteSelection", 14, F_PASTE_SELECTION),	/* */
	FUNC_TERM ("WindowsDesk", 11, F_CHANGE_WINDOWS_DESK),	/* WindowDesk "name" new_desk */
    FUNC_TERM ("BookmarkWindow", 14, F_BOOKMARK_WINDOW),    /* BookmarkWindow "name" new_bookmark */
    FUNC_TERM ("PinMenu", 7, F_PIN_MENU),    /* PinMenu ["name"] */
        /* end of window functions */
	/* these are commands  to be used only by modules */
	FUNC_TERM ("&nonsense&", 10, F_MODULE_FUNC_START),	/* not really a command */
	FUNC_TERM ("Send_WindowList", 15, F_SEND_WINDOW_LIST),	/* */
	FUNC_TERM ("SET_MASK", 8, F_SET_MASK),	   /* SET_MASK  mask */
    FUNC_TERM2 (NEED_NAME, "SET_NAME", 8, F_SET_NAME),   /* SET_NAME  name */
	FUNC_TERM ("UNLOCK", 6, F_UNLOCK),		   /* UNLOCK    1  */
	FUNC_TERM ("SET_FLAGS", 9, F_SET_FLAGS),   /* SET_FLAGS flags */
	/* these are internal commands */
	FUNC_TERM ("&nonsense&", 10, F_INTERNAL_FUNC_START),	/* not really a command */
	FUNC_TERM ("&raise_it&", 10, F_RAISE_IT),  /* should not be used by user */
    /* wharf functions : */
    {TF_NO_MYNAME_PREPENDING, "Folder", 6, TT_TEXT, F_Folder, NULL},
    {TF_NO_MYNAME_PREPENDING | NEED_NAME | NEED_CMD, "Swallow", 7, TT_TEXT, F_Swallow, NULL},
    {TF_NO_MYNAME_PREPENDING | NEED_NAME | NEED_CMD, "MaxSwallow", 10, TT_TEXT, F_MaxSwallow, NULL},
    {TF_NO_MYNAME_PREPENDING | NEED_NAME | NEED_CMD, "SwallowModule", 13, TT_TEXT, F_Swallow, NULL},
    {TF_NO_MYNAME_PREPENDING | NEED_NAME | NEED_CMD, "MaxSwallowModule", 16, TT_TEXT, F_MaxSwallow, NULL},
	FUNC_TERM2 (NEED_NAME | NEED_CMD, "DropExec", 8, F_DropExec),	/* DropExec   "name" command */
    {TF_NO_MYNAME_PREPENDING, "Size", 4, TT_TEXT, F_Size, NULL},
    {TF_NO_MYNAME_PREPENDING, "Transient", 9, TT_TEXT, F_Transient, NULL},

	{0, NULL, 0, 0, 0}
};

struct SyntaxDef FuncSyntax = {
	' ',
	'\0',
	FuncTerms,
	0,										   /* use default hash size */
	NULL
};

struct SyntaxDef     *pFuncSyntax = &FuncSyntax;

/************************************************************************************/
/* Command Line Processing/ App initialization here :                               */
/************************************************************************************/
void
SetMyClass (const char *app_class)
{
    if( app_class != NULL )
    {
        strncpy( MyClass, app_class, MAX_MY_CLASS );
        MyClass[MAX_MY_CLASS] = '\0' ;
    }
}

void
SetMyName (char *argv0)
{
	char         *temp = strrchr (argv0, '/');

	/* Save our program name - for error messages */
	MyName = temp ? temp + 1 : argv0;
	set_application_name(argv0);
}

/* If you change/add options please change InitMyApp below and option flags in aftersteplib.h */

static CommandLineOpts as_cmdl_options[] =
{
#define  SHOW_VERSION   0
#define  SHOW_CONFIG    1
#define  SHOW_USAGE     2
/* 0*/{"v", "version", "Display version information and stop", NULL, handler_show_info, NULL, SHOW_VERSION  },
/* 1*/{"c", "config",  "Display Config information and stop",  NULL, handler_show_info, NULL, SHOW_CONFIG},
/* 2*/{"h", "help",    "Display uasge information and stop",   NULL, handler_show_info, NULL, SHOW_USAGE},
/* 3*/{NULL,"debug",   "Debugging: Run in Synchronous mode.",  NULL, handler_set_flag, &(MyArgs.flags), ASS_Debugging},
/* 4*/{"s", "single",  "Run on single screen only", NULL, handler_set_flag, &(MyArgs.flags), ASS_SingleScreen},
/* 5*/{"r", "restart", "Run as if it was restarted","same as regular startup, only runs RestartFunction\ninstead of InitFunction.",
                                                           handler_set_flag, &(MyArgs.flags), ASS_Restarting},
#define OPTION_HAS_ARGS     6
/* 6*/{"d", "display", "Specify what X display we should connect to","Overrides $DISPLAY environment variable.",
                                                           handler_set_string, &(MyArgs.display_name), 0, CMO_HasArgs },
/* 7*/{"f", "config-file", "Read all config from requested file","Use it if you want to use .steprc\ninstead of standard config files.",
                                                           handler_set_string, &(MyArgs.override_config), 0, CMO_HasArgs },
/* 8*/{"p", "user-dir","Read all the config from requested dir","Use it to override config location\nrequested in compile time." ,
                                                           handler_set_string, &(MyArgs.override_home), 0, CMO_HasArgs },
/* 9*/{"g", "global-dir","Use requested dir as a shared config dir","Use it to override shared config location\nrequested in compile time." ,
                                                           handler_set_string, &(MyArgs.override_share), 0, CMO_HasArgs },
/*10*/{"V", "verbosity-level","Change verbosity of the AfterStep output","0 - will disable any output;\n1 - will allow only error messages;\n5 - both errors and warnings(default).",
                                                           handler_set_int, &(MyArgs.verbosity_level), 0, CMO_HasArgs },
/*11*/{NULL, "window", "Internal Use: Window in which action occured", "interface part which has triggered our startup.",
                                                           handler_set_int, &(MyArgs.src_window), 0, CMO_HasArgs },
/*12*/{NULL, "context","Internal Use: Context in which action occured", "interface part which has triggered our startup.",
                                                           handler_set_int, &(MyArgs.src_context), 0, CMO_HasArgs },
/*13*/{NULL, "look","Read look config from requested file","Use it if you want to use different look\ninstead of what was selected from the menu.",
                                                           handler_set_string, &(MyArgs.override_look), 0, CMO_HasArgs },
/*14*/{NULL, "feel","Read feel config from requested file","Use it if you want to use different feel\ninstead of what was selected from the menu.",
                                                           handler_set_string, &(MyArgs.override_feel), 0, CMO_HasArgs },
/*15*/{NULL, "theme","Read theme config from requested file","Use it if you want to use different theme\ninstead of what was selected from the menu.",
                                                           handler_set_string, &(MyArgs.override_feel), 0, CMO_HasArgs },
#ifdef DEBUG_TRACE_X
/*16*/{NULL, "trace-func","Debugging: Trace calls to a function with requested name", NULL,
                                                           handler_set_string, &(MyArgs.trace_calls), 0, CMO_HasArgs },
#endif
      {NULL, NULL, NULL, NULL, NULL, NULL, 0 }
};

void
standard_version (void)
{
    if( MyVersionFunc )
        MyVersionFunc();
    else
	    printf ("%s version %s\n", MyClass, VERSION);
}

void
print_command_line_opt(const char *prompt, CommandLineOpts *options, ASFlagType mask)
{
    register int i ;
    ASFlagType bit = 0x01;

    printf ("%s:\n", prompt);

    for( i = 0 ; options[i].handler != NULL ; i++ )
    {
        if( !get_flags( bit, mask ) )
        {
            if( options[i].short_opt )
                printf( OPTION_SHORT_FORMAT, options[i].short_opt );
            else
                printf( OPTION_NOSHORT_FORMAT);

            if( !get_flags( options[i].flags, CMO_HasArgs ) )
                printf( OPTION_DESCR1_FORMAT_NOVAL, options[i].long_opt, options[i].descr1 );
            else
                printf( OPTION_DESCR1_FORMAT_VAL, options[i].long_opt, options[i].descr1 );

            if( options[i].descr2 )
			{
                register char *start = options[i].descr2 ;
				register char *end ;
				do
				{
					end = strchr( start, '\n' );
					if( end == NULL )
						printf(OPTION_DESCR2_FORMAT, start );
					else
					{
						static char buffer[81];
						register int len = (end > start+80 )? 80 : end-start ;
						strncpy( buffer, start, len );
						buffer[len] = '\0' ;
						printf(OPTION_DESCR2_FORMAT, buffer );
		  				start = end+1 ;
					}
				}while( end != NULL );
			}
        }
        bit = bit << 1 ;
    }
}

void
standard_usage()
{
    standard_version();
    if( MyUsageFunc )
        MyUsageFunc();
    else
        printf (OPTION_USAGE_FORMAT "\n", MyName );
    print_command_line_opt("standard_options are :", as_cmdl_options, MyArgs.mask);
}

void  handler_show_info( char *argv, void *trg, long param )
{
    switch( param )
    {
        case SHOW_VERSION :
            standard_version();
            break;
        case SHOW_CONFIG  :
            standard_version();
            printf ("BinDir            %s\n", AFTER_BIN_DIR);
            printf ("ManDir            %s\n", AFTER_MAN_DIR);
            printf ("DocDir            %s\n", AFTER_DOC_DIR);
            printf ("ShareDir          %s\n", AFTER_SHAREDIR);
            printf ("GNUstep           %s\n", GNUSTEP);
            printf ("GNUstepLib        %s\n", GNUSTEPLIB);
            printf ("AfterDir          %s\n", AFTER_DIR);
            break;
        case SHOW_USAGE   :
            standard_usage();
            break;
    }
    exit (0);
}
void  handler_set_flag( char *argv, void *trg, long param )
{
    register ASFlagType *f = trg ;
    set_flags( *f, param );
}
void  handler_set_string( char *argv, void *trg, long param )
{
    register char **s = trg;
    if( argv )
        *s = argv;
}
void  handler_set_int( char *argv, void *trg, long param )
{
    register int *i = trg ;
    if( argv )
        *i = atoi(argv);
}

int
match_command_line_opt( char *argvi, CommandLineOpts *options )
{
    register char *ptr = argvi;
    register int opt ;

    if( ptr == NULL )
        return -1;
    if( *ptr == '-' )
    {
        ptr++;
        if( *ptr == '-' )
        {
            ptr++;
            for( opt = 0 ; options[opt].handler ; opt++ )
                if( strcmp(options[opt].long_opt, ptr ) == 0 )
                    break;

        }else
        {
            for( opt = 0 ; options[opt].handler ; opt++ )
                if( options[opt].short_opt )
                    if( strcmp(options[opt].short_opt, ptr ) == 0 )
                        break;
        }
    }else
        opt = -1;
    if( options[opt].handler == NULL )
        opt = -1;
    return opt;
}

void
InitMyApp (  const char *app_class, int argc, char **argv, void (*version_func) (void), void (*custom_usage_func) (void), ASFlagType opt_mask )
{
    /* first of all let's set us some nice signal handlers */
#ifdef HAVE_SIGSEGV_HANDLING
    set_signal_handler (SIGSEGV);
#endif

	set_signal_handler (SIGUSR2);
    signal (SIGPIPE, DeadPipe);        /* don't forget DeadPipe should be provided by the app */

#ifdef I18N
	if (setlocale (LC_CTYPE, AFTER_LOCALE) == NULL)
        show_error ("unable to set locale");
#endif

    SetMyClass( app_class );
    MyVersionFunc = version_func ;
    MyUsageFunc = custom_usage_func ;

    memset( &MyArgs, 0x00, sizeof(ASProgArgs) );
    MyArgs.mask = opt_mask ;
    MyArgs.verbosity_level = OUTPUT_VERBOSE_THRESHOLD ;


    memset( &Scr, 0x00, sizeof(ScreenInfo) );

    if( argc > 0 && argv )
    {
        int i ;

		MyArgs.saved_argc = argc ;
		MyArgs.saved_argv = safecalloc( argc, sizeof(char*));
		for( i = 0 ; i < argc ; ++i )
			MyArgs.saved_argv[i] = argv[i] ;

        SetMyName( argv[0] );

        for( i = 1 ; i < argc ; i++ )
        {
            register int opt ;

            if( (opt = match_command_line_opt( &(argv[i][0]), as_cmdl_options )) < 0 )
                continue;
            if( get_flags( (0x01<<opt), MyArgs.mask) )
                continue;
            if( get_flags( as_cmdl_options[opt].flags, CMO_HasArgs ) )
            {
                if( ++i >= argc )
                    continue;
                else
                    as_cmdl_options[opt].handler( argv[i], as_cmdl_options[opt].trg, as_cmdl_options[opt].param );
                argv[i-1] = NULL ;
            }else
                as_cmdl_options[opt].handler( NULL, as_cmdl_options[opt].trg, as_cmdl_options[opt].param );
            argv[i] = NULL ;
        }
    }

    fd_width = get_fd_width ();

    if (FuncSyntax.term_hash == NULL)
    {
        InitHash (&FuncSyntax);
		BuildHash (&FuncSyntax);
    }
    set_output_threshold( MyArgs.verbosity_level );
#ifdef DEBUG_TRACE_X
    trace_enable_function(MyArgs.trace_calls);
#endif

}

void
InitSession()
{
    /* initializing our dirs names */
    Session = GetNCASSession(&Scr, MyArgs.override_home, MyArgs.override_share);
    if( MyArgs.override_config )
        set_session_override( Session, MyArgs.override_config, 0 );
    if( MyArgs.override_look )
        set_session_override( Session, MyArgs.override_look, F_CHANGE_LOOK );
    if( MyArgs.override_feel )
        set_session_override( Session, MyArgs.override_feel, F_CHANGE_FEEL );
}

/*********** end command line parsing **************************/
/************ child process spawning ***************************/
/***********************************************************************
 *  Procedure:
 *  general purpose child launcher - spawn child.
 *  Pass along all the cmd line args if needed.
 *  returns PID of the spawned process
 ************************************************************************/
static int  	as_singletons[MAX_SINGLETONS_NUM];
static Bool 	as_init_singletons = True;

void
as_sigchild_handler (int signum)
{
	int           pid;
	int status ;

	signal (SIGCHLD, as_sigchild_handler);
	DEBUG_OUT("Entering SigChild_handler(%lu)", time (NULL));
	if( as_init_singletons )
		return;
	while ((pid = WAIT_CHILDREN (&status)) > 0 )
	{
		register int i ;
		for( i = 0; i < MAX_SINGLETONS_NUM ; i++ )
			if( pid == as_singletons[i] )
			{
				as_singletons[i] = 0 ;
				break;
			}
	}
	DEBUG_OUT("Exiting SigChild_handler(%lu)", time (NULL));
}

/*
 * This should return 0 if process of running external app to draw background completed or killed.
 * otherwise it returns > 0
 */
int
check_singleton_child (int singleton_id, Bool kill_it_to_death)
{
	int           i;
	int 		  pid, status;

	if( as_init_singletons || singleton_id < 0 )
		return -1 ;

	if( singleton_id >= MAX_SINGLETONS_NUM )
		singleton_id = MAX_SINGLETONS_NUM-1;

	DEBUG_OUT("CheckingForDrawChild(%lu)....",time (NULL));
	if ( (pid = as_singletons[singleton_id]) > 0)
	{
		DEBUG_OUT("checking on singleton child #%d started with PID (%d)", singleton_id, pid);
		if (kill_it_to_death)
		{
			kill (pid, SIGTERM);
			for (i = 0; i < 100; i++)		   /* give it 10 sec to terminate */
			{
				sleep_a_little (100);
				if ( WAIT_CHILDREN (&status) == pid || as_singletons[singleton_id] <= 0 )
					break;
			}
			if (i >= 100)
				kill (pid, SIGKILL);  /* no more mercy */
			as_singletons[singleton_id] = 0 ;
		}
	} else if (as_singletons[singleton_id] < 0)
		as_singletons[singleton_id] = 0;

	DEBUG_OUT ("Done(%lu). Child PID on exit = %d.", time (NULL), as_singletons[singleton_id]);
	return as_singletons[singleton_id];
}

int
spawn_child( const char *cmd, int singleton_id, int screen, Window w, int context, Bool do_fork, Bool pass_args, ... )
{
    int pid = 0;

    if( cmd == NULL )
        return 0;
	if( as_init_singletons )
	{
		register int i ;
		for( i = 0; i < MAX_SINGLETONS_NUM ; i++ )
			as_singletons[i] = 0 ;
		signal (SIGCHLD, as_sigchild_handler);
		as_init_singletons = False ;
	}

	if( singleton_id >= 0 )
	{
		if( singleton_id >= MAX_SINGLETONS_NUM )
			singleton_id = MAX_SINGLETONS_NUM-1;
		if( as_singletons[singleton_id] > 0 )
			check_singleton_child( singleton_id, True );
	}

    if( do_fork )
        pid = fork();

    if( pid != 0 )
	{
		/* there is a possibility of the race condition here
		 * but it really is not worse the trouble to try and avoid it.
		 */
    	if( singleton_id >= 0 )
			as_singletons[singleton_id] = pid ;
  	    return pid;
	}else
    {/* we get here only in child process. We now need to spawn new proggy here: */
        int len;
        char *display = XDisplayString (dpy);
        register char *ptr ;
        char *cmdl;
        char *arg, *screen_str = NULL, *w_str = NULL, *context_str = NULL ;
        va_list ap;

        len = strlen(cmd);
        if( pass_args )
        {
            if( screen >= 0 )
                screen_str = string_from_int( screen );
            if( w != None )
                w_str = string_from_int( w );
            if( context != C_NO_CONTEXT )
                context_str = string_from_int( context );

            len += 1+2+1+strlen( display );
            if( screen_str )
                len += strlen(screen_str);
            if ( get_flags( MyArgs.flags, ASS_Debugging) )
                len += 8 ;
            if ( get_flags( MyArgs.flags, ASS_Restarting) )
                len += 3 ;
            if ( MyArgs.override_config )
                len += 4+strlen(MyArgs.override_config);
            if ( MyArgs.override_home )
                len += 4+strlen(MyArgs.override_home);
            if ( MyArgs.override_share )
                len += 4+strlen(MyArgs.override_share);

            if( MyArgs.verbosity_level != OUTPUT_DEFAULT_THRESHOLD )
                len += 4+32 ;
#ifdef DEBUG_TRACE_X
            if( MyArgs.trace_calls )
                len += 13+strlen( MyArgs.trace_calls );
#endif
            if( w_str )
                len += 1+8+1+strlen(w_str);
            if( context_str )
                len += 1+9+1+strlen(context_str);
        }
        /* now we want to append arbitrary number of arguments to the end of command line : */
        va_start( ap, pass_args );
        while( (arg = va_arg(ap,char*)) != NULL )
            len += 1+strlen(arg);
        va_end(ap);

        len+=3;

        ptr = cmdl = safemalloc( len );
        strcpy( cmdl, cmd );
        while(*ptr) ptr++;
        if( pass_args )
        {
            ptr += sprintf( ptr, " -d %s%s -s", display, screen_str?screen_str:"" );
            if ( get_flags( MyArgs.flags, ASS_Debugging) )
            {
                strcpy( ptr, " --debug");
                ptr+=8 ;
            }
            if ( get_flags( MyArgs.flags, ASS_Restarting) )
            {
                strcpy( ptr, " -r");
                ptr += 3 ;
            }
            if ( MyArgs.override_config )
                ptr += sprintf( ptr, " -f %s", MyArgs.override_config );
            if ( MyArgs.override_home )
                ptr += sprintf( ptr, " -p %s", MyArgs.override_home );
            if ( MyArgs.override_share )
                ptr += sprintf( ptr, " -g %s", MyArgs.override_share );
            if( MyArgs.verbosity_level != OUTPUT_DEFAULT_THRESHOLD )
                ptr += sprintf( ptr, " -V %d", MyArgs.verbosity_level );
#ifdef DEBUG_TRACE_X
            if( MyArgs.trace_calls )
                ptr += sprintf( ptr, " --trace-func %s", MyArgs.trace_calls );
#endif
            if( w_str )
                ptr += sprintf( ptr, " --window %s", w_str );
            if( context_str )
                ptr += sprintf( ptr, " --context %s", context_str );
        }
        va_start( ap, pass_args );
        while( (arg = va_arg(ap,char*)) != NULL )
        {
            *(ptr++) = ' ';
            strcpy( ptr, arg );
            while(*ptr) ptr++;
        }
        va_end(ap);
        strcpy (ptr, do_fork?" &\n":"\n");

        execl ("/bin/sh", "sh", "-c", cmdl, (char *)0);
        if( screen >= 0 )
            show_error( "failed to start %s on the screen %d", cmd, screen );
        else
            show_error( "failed to start %s", cmd );
        show_system_error( " complete command line: \"%s\"\n", cmdl );
        exit(128);
    }
}
/************ end child process spawning ***************************/
