/*
 * Copyright (c) 2000 Sasha Vasko <sasha at aftercode.net>
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
 */

#define LOCAL_DEBUG

#include "../configure.h"

#include "../include/asapp.h"
#include <X11/cursorfont.h>


#include "../include/afterstep.h"
#include "../include/parse.h"
#include "../include/loadimg.h"
#include "../include/parser.h"
#include "../include/confdefs.h"
#include "../include/mystyle.h"
#include "../libAfterImage/afterimage.h"
#include "../include/screen.h"
#include "../include/balloon.h"
#include "../include/asfeel.h"

/**************************************************************************************
 * ASFeel initialization and destruction code.
 *************************************************************************************/

void
init_asfeel( ASFeel *feel )
{
    int i ;
    memset( feel, 0x00, sizeof(ASFeel));
    feel->magic = MAGIC_ASFEEL ;
    feel->buttons2grab = 7;
    feel->AutoReverse = 0;
    feel->Xzap = 12;
    feel->Yzap = 12;
    feel->EdgeScrollX = feel->EdgeScrollY = -100000;
    feel->EdgeResistanceScroll = feel->EdgeResistanceMove = 0;
    feel->EdgeAttractionScreen = 20;
    feel->EdgeAttractionWindow = 10;
    feel->OpaqueMove = 5;
    feel->OpaqueResize = 5;
    feel->ClickTime = 150;
    feel->AutoRaiseDelay = 0;
    feel->RaiseButtons = 0;
    feel->flags = DoHandlePageing;
    feel->XorValue = (((unsigned long)1) << Scr.d_depth) - 1;

    feel->no_snaping_mod = ShiftMask|ControlMask ;

    feel->MouseButtonRoot = NULL;
    feel->FuncKeyRoot = NULL;
    feel->Popups = NULL;
    feel->ComplexFunctions = NULL;
    feel->ShadeAnimationSteps = 12;

    for( i = 0 ; i < MAX_CURSORS; ++i )
        if( feel->cursors[i] )
            feel->cursors[i] = Scr.standard_cursors[i] ;
}

ASFeel *
create_asfeel()
{
	ASFeel *feel ;

    feel = safemalloc( sizeof(ASFeel));
	init_asfeel( feel );
	return feel;
}

void
destroy_asfeel( ASFeel *feel, Bool reusable )
{
	if( feel )
    {
        if(feel->magic == MAGIC_ASFEEL )
        {
            register int i ;

            feel->magic = 0 ;

			while (feel->MouseButtonRoot != NULL)
			{
            	MouseButton  *mb = feel->MouseButtonRoot;

            	feel->MouseButtonRoot = mb->NextButton;
            	if (mb->fdata)
            	{
                	free_func_data( mb->fdata);
                	free (mb->fdata);
            	}
				free (mb);
			}
        	while (feel->FuncKeyRoot != NULL)
			{
            	FuncKey      *fk = feel->FuncKeyRoot;

            	feel->FuncKeyRoot = fk->next;
				if (fk->name != NULL)
					free (fk->name);
            	if (fk->fdata != NULL)
            	{
                	free_func_data(fk->fdata);
                	free (fk->fdata);
            	}
				free (fk);
			}
        	for( i = 0 ; i < MAX_CURSORS; ++i )
            	if( feel->cursors[i] && feel->cursors[i] != Scr.standard_cursors[i] )
            	{
                	XFreeCursor( dpy, feel->cursors[i] );
                	feel->cursors[i] = None ;
            	}
        	if( feel->Popups )
            	destroy_ashash( &feel->Popups );
        	if( feel->ComplexFunctions )
            	destroy_ashash( &feel->ComplexFunctions );
            if( feel->window_boxes )
            {
                i = feel->window_boxes_num;
                while( --i >= 0 )
                    destroy_aswindow_box( &(feel->window_boxes[i]), True );
                free( feel->window_boxes );
            }
        }
		if( !reusable )
			free(feel);
		else
			memset( feel, 0x00, sizeof(ASFeel) );
	}
}

void
apply_feel_cursor( Window w, ASFeel *feel, int cursor )
{
    if( feel && cursor >= 0 && cursor < MAX_CURSORS && w != None)
        XDefineCursor (dpy, w, feel->cursors[cursor]);
}

void
free_feel_cursors (ASFeel *feel)
{
    register int i ;

    if( dpy == NULL || feel == NULL ) return ;
    /* free cursors */
    for( i = 0 ; i < MAX_CURSORS ; i++ )
        if( feel->cursors[i] )
            XFreeCursor (dpy, feel->cursors[i] );
}

void
check_feel_sanity(ASFeel *feel)
{
    int i ;

LOCAL_DEBUG_CALLER_OUT( "feel %p", feel);
    /* If no edge scroll line is provided in the setup file, use a default */
    if (feel->EdgeScrollX == -100000)
        feel->EdgeScrollX = 25;
    if (feel->EdgeScrollY == -100000)
        feel->EdgeScrollY = feel->EdgeScrollX;

    if (get_flags(feel->flags, ClickToRaise) && (feel->AutoRaiseDelay == 0))
        feel->AutoRaiseDelay = -1;

	/* if edgescroll >1000 and < 100000m
	 * wrap at edges of desktop (a "spherical" desktop) */
    feel->flags &= ~(EdgeWrapX|EdgeWrapY);
    if (feel->EdgeScrollX >= 1000)
	{
        feel->EdgeScrollX /= 1000;
        feel->flags |= EdgeWrapX;
	}
    if (feel->EdgeScrollY >= 1000)
	{
        feel->EdgeScrollY /= 1000;
        feel->flags |= EdgeWrapY;
	}

	feel->EdgeScrollX = feel->EdgeScrollX * Scr.MyDisplayWidth / 100;
    feel->EdgeScrollY = feel->EdgeScrollY * Scr.MyDisplayHeight / 100;

    if( feel->no_snaping_mod == 0 )
        feel->no_snaping_mod = ShiftMask ;

    if (Scr.VxMax == 0)
        clear_flags(feel->flags, EdgeWrapX);
    if (Scr.VyMax == 0)
        clear_flags(feel->flags, EdgeWrapY);

    i = feel->window_boxes_num ;
    while( --i >= 0 )
    {
        if( !get_flags(feel->window_boxes[i].area.flags, WidthValue) )
        {
            feel->window_boxes[i].area.width = Scr.MyDisplayWidth ;
            if( get_flags( feel->window_boxes[i].flags, ASA_Virtual ) )
                feel->window_boxes[i].area.width += Scr.VxMax ;
            feel->window_boxes[i].area.width -= feel->window_boxes[i].area.x ;
        }
        if( !get_flags(feel->window_boxes[i].area.flags, HeightValue) )
        {
            feel->window_boxes[i].area.height = Scr.MyDisplayHeight ;
            if( get_flags( feel->window_boxes[i].flags, ASA_Virtual ) )
                feel->window_boxes[i].area.width += Scr.VyMax ;
            feel->window_boxes[i].area.width -= feel->window_boxes[i].area.y ;
        }
        if( feel->default_window_box_name != NULL )
        {
            if( feel->window_boxes[i].name &&
                mystrcasecmp( feel->window_boxes[i].name, feel->default_window_box_name ) == 0 )
                feel->default_window_box = &(feel->window_boxes[i]);
        }
#if !defined(LOCAL_DEBUG) || defined(NO_DEBUG_OUTPUT)
        if( get_output_threshold() >= OUTPUT_LEVEL_DEBUG )
#endif
           print_window_box (&(feel->window_boxes[i]), i);
    }

#if 0
    if( feel->default_window_box == NULL )
    { /* build new default windowbox */
        i = feel->window_boxes_num ;
        feel->window_boxes = realloc( feel->window_boxes, sizeof(ASWindowBox)*(i+1) );
        ++(feel->window_boxes);
        feel->default_window_box = &(feel->window_boxes[i]);
        memset( feel->default_window_box, 0x00, sizeof(ASWindowBox));
        feel->default_window_box->name = mystrdup("default");
        feel->default_window_box->area.width = Scr.MyDisplayWidth ;
        feel->default_window_box->area.height = Scr.MyDisplayHeight ;
        feel->default_window_box->main_strategy = ASP_Manual ;
        feel->default_window_box->backup_strategy = ASP_Manual ;
        if( get_flags( feel->flags, SMART_PLACEMENT )  )
        {
            feel->default_window_box->main_strategy = ASP_SmartPlacement ;
            if( get_flags( feel->flags, RandomPlacement )  )
                feel->default_window_box->backup_strategy = ASP_RandomPlacement ;
        }else if( get_flags( feel->flags, RandomPlacement )  )
            feel->default_window_box->main_strategy = ASP_RandomPlacement ;
#if !defined(LOCAL_DEBUG) || defined(NO_DEBUG_OUTPUT)
        if( get_output_threshold() >= OUTPUT_LEVEL_DEBUG )
#endif
           print_window_box (&(feel->window_boxes[i]), i);
    }
#endif
}

/*************************************************************************
 * WindowBox utility functions
 *************************************************************************/
ASWindowBox *create_aswindow_box( const char *name )
{
	ASWindowBox *aswbox = safecalloc( 1, sizeof(ASWindowBox) );
	aswbox->name = mystrdup( name );
	return aswbox;
}

void
destroy_aswindow_box( ASWindowBox *aswbox, Bool reusable )
{
    if( aswbox )
    {
        if( aswbox->name )
            free( aswbox->name );
        if( !reusable )
            free(aswbox);
        else
            memset( aswbox, 0x00, sizeof(ASWindowBox) );
    }
}

void
print_window_box (ASWindowBox *aswbox, int index)
{
	if (aswbox)
	{
        if (aswbox->name)
            fprintf (stderr, "WindowBox[%d].name = \"%s\";\n", index, aswbox->name);
        fprintf (stderr, "WindowBox[%d].set_flags = 0x%lX;\n", index, aswbox->set_flags);
        fprintf (stderr, "WindowBox[%d].flags = 0x%lX;\n", index, aswbox->flags);
		fprintf (stderr, "WindowBox[%d].area.flags = 0x%X;\n", index, aswbox->area.flags);
		fprintf (stderr, "WindowBox[%d].area.geometry = %dx%d%+d%+d;\n", index, aswbox->area.width, aswbox->area.height, aswbox->area.x, aswbox->area.y);
		fprintf (stderr, "WindowBox[%d].min_size = %dx%d;\n", index, aswbox->min_width, aswbox->min_height);
		fprintf (stderr, "WindowBox[%d].max_size = %dx%d;\n", index, aswbox->max_width, aswbox->max_height);
		fprintf (stderr, "WindowBox[%d].main_strategy = %d;\n", index, aswbox->main_strategy);
		fprintf (stderr, "WindowBox[%d].backup_strategy = %d;\n", index, aswbox->backup_strategy);
	}
}

/*************************************************************************
 * Menus :
 *************************************************************************/
void
init_list_of_menus(ASHashTable **list, Bool force)
{
    if( list == NULL ) return ;

    if( force && *list != NULL )
        destroy_ashash( list );

    if( *list == NULL )
    {
        *list = create_ashash( 0, casestring_hash_value,
                                  casestring_compare,
                                  menu_data_destroy);
        LOCAL_DEBUG_OUT( "created the list of Popups %p", *list );
    }
}




