/****************************************************************************
 * Copyright (c) 2001 Sasha Vasko <sasha at aftercode.net>
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
 *****************************************************************************/

/***********************************************************************
 * afterstep window move/resize code
 ***********************************************************************/

#include "../configure.h"

#define LOCAL_DEBUG
#include "asapp.h"
#include <signal.h>
#include "../libAfterImage/afterimage.h"
#include "afterstep.h"
#include "event.h"
#include "mystyle.h"
#include "screen.h"
#include "hints.h"
#ifdef NO_ASRENDER
#include "decor.h"
#endif
#include "moveresize.h"

/***********************************************************************
 * backported from dispatcher.c :
 ***********************************************************************/
/* if not NULL then we are in interactive move/resize mode : */
typedef int (*moveresize_event_func)(struct ASMoveResizeData*, struct ASEvent *event );

Bool (*_as_grab_screen_func)( struct ScreenInfo *scr, Cursor cursor ) = NULL;
void (*_as_ungrab_screen_func) () = NULL;

void move_func (struct ASMoveResizeData *data, int x, int y);


static ASMoveResizeData *_as_curr_moveresize_data = NULL ;
static moveresize_event_func    _as_curr_moveresize_handler = NULL ;
/* We need to track what widgets has received ButtonPress so that we can
 * send them ButtonRelease in case we need to ungrab the pointer. Another
 * problem here is there could be 5 buttons each generating its own
 * Button Press/Release.
 */
static ASEvent          _as_pressed_buttons[5] = {{0,0,0,0,0,0,NULL,NULL,NULL,0},
												  {0,0,0,0,0,0,NULL,NULL,NULL,0},
												  {0,0,0,0,0,0,NULL,NULL,NULL,0},
												  {0,0,0,0,0,0,NULL,NULL,NULL,0},
												  {0,0,0,0,0,0,NULL,NULL,NULL,0}};

/**********************************************************************/
/* Interactive Move/Resize :                                          */
/**********************************************************************/
Bool start_widget_moveresize( ASMoveResizeData * data, moveresize_event_func handler )
{
	if( _as_curr_moveresize_data || data == NULL || handler == NULL )
		return False;
	_as_curr_moveresize_data = data ;
	_as_curr_moveresize_handler = handler ;
	return True;
}

Bool
check_moveresize_event( ASEvent *event )
{
    LOCAL_DEBUG_OUT("data(%p)->handler(%p)", _as_curr_moveresize_data, _as_curr_moveresize_handler );
    if( _as_curr_moveresize_data )
		if( _as_curr_moveresize_handler( _as_curr_moveresize_data, event ) == ASE_Consumed )
            return True;
    return False;
}

Bool stop_widget_moveresize()
{
	if( _as_curr_moveresize_data == NULL )
		return False;
	_as_curr_moveresize_data = NULL ;
	_as_curr_moveresize_handler = NULL ;
	return True;
}

/**********************************************************************/
/* Pointer grabbing :                                                 */
/**********************************************************************/
Bool grab_widget_pointer( ASWidget *widget, ASEvent *trigger,
						  unsigned int event_mask,
	                      int *x_return, int *y_return,
						  int *root_x_return, int *root_y_return,
						  unsigned int *mask_return)
{
    int i ;
    Window wjunk;
    Time ttime = CurrentTime ;
    ScreenInfo *scr = &Scr ;

    if( widget == NULL )
		return False;
    ASQueryPointerWinXYMask(AS_WIDGET_WINDOW(widget),root_x_return, root_y_return, x_return, y_return, mask_return);
LOCAL_DEBUG_OUT("grabbing pointer at window(%+d%+d) root(%+d%+d), mask = 0x%X, window(%lX)", *x_return, *y_return, *root_x_return, *root_y_return, *mask_return, AS_WIDGET_WINDOW(widget) );
    if( trigger )
    {
        ttime = trigger->event_time ;;
        if( trigger->scr )
            scr =  trigger->scr ;
    }

/*	XUngrabPointer( dpy, trigger->event_time ); */
#ifdef AS_WIDGET_H_HEADER_INCLUDED
    if( XGrabPointer( dpy, AS_WIDGET_WINDOW(widget),
		              False,
					  event_mask,
					  GrabModeAsync, GrabModeAsync,
					  None,
					  None,
                      ttime ) == 0 )
#else
    if( _as_grab_screen_func == NULL ||
    	_as_grab_screen_func( scr, scr->Feel.cursors[ASCUR_Move]))
#endif
    {
SHOW_CHECKPOINT;
		for( i = 0 ; i < 5 ; ++i )
		{
SHOW_CHECKPOINT;
LOCAL_DEBUG_OUT("comparing mask = 0x%X and 0x%X", *mask_return, (Button1Mask<<i) );
			if( get_flags( *mask_return, (Button1Mask<<i) ) )
			{
LOCAL_DEBUG_OUT("pressed widget[%d] = %p and widget = %p", i, _as_pressed_buttons[i].widget, widget );
				if( _as_pressed_buttons[i].widget != NULL &&
			    	_as_pressed_buttons[i].widget != widget )
				{
					ASEvent tmp = _as_pressed_buttons[i] ;
SHOW_CHECKPOINT;
					tmp.x.type = ButtonRelease ;
					tmp.x.xbutton.button = i+1 ;
					tmp.x.xbutton.x_root = *root_x_return ;
					tmp.x.xbutton.y_root = *root_y_return ;
					XTranslateCoordinates( dpy, AS_WIDGET_SCREEN(tmp.widget)->Root,
		    			               	AS_WIDGET_WINDOW(tmp.widget),
									   	*x_return, *y_return,
									   	&(tmp.x.xbutton.x), &(tmp.x.xbutton.y), &wjunk );
					tmp.x.xbutton.state = *mask_return ;
#ifdef AS_DISPATCHER_H_HEADER_INCLUDED         /* in as-devel */
					handle_widgets_event ( &tmp );
#else
                    XSendEvent( dpy, tmp.x.xany.window, False, ButtonReleaseMask, &(tmp.x));
#endif
				}
			}
		}
    }else
        return False;
    return True;
}


/***********************************************************************
 * end of code backported from dispatcher.c.
 ***********************************************************************/
/***********************************************************************
 *  MoveResize miscelanea :
 ***********************************************************************/
static void
prepare_move_resize_data( ASMoveResizeData *data, ASWidget *parent, ASWidget *mr,
						  ASEvent *trigger,
                          as_interactive_apply_handler    apply_func,
						  as_interactive_complete_handler complete_func,
						  int opaque_size )
{
	XSetWindowAttributes attr;
	int x = 0, y = 0;
	ScreenInfo *scr = AS_WIDGET_SCREEN(parent);
	MyLook *look ;
	int root_x, root_y;
    int width, height ;

	data->parent= parent;
	data->mr 	= mr ;
    data->feel  = AS_WIDGET_FEEL(mr);
    data->look  = look = AS_WIDGET_LOOK(mr);

	if( apply_func != NULL  &&
		AS_WIDGET_WIDTH(mr)*AS_WIDGET_HEIGHT(mr) < (opaque_size*AS_WIDGET_WIDTH(parent)*AS_WIDGET_HEIGHT(parent)) / 100)
		data->apply_func = apply_func ;
	else
	{
		data->apply_func = move_outline;
		data->outline = make_outline_segments( parent, look );
	}
	data->complete_func = complete_func;

    data->start.x = data->curr.x = data->last.x = AS_WIDGET_X(mr) - AS_WIDGET_X(parent);
    data->start.y = data->curr.y = data->last.y = AS_WIDGET_Y(mr) - AS_WIDGET_Y(parent);
    data->start.width  = data->curr.width  = data->last.width  = AS_WIDGET_WIDTH(mr);
    data->start.height = data->curr.height = data->last.height = AS_WIDGET_HEIGHT(mr);
	data->bw = AS_WIDGET_BW(mr);

	grab_widget_pointer( parent, trigger,
						 ButtonPressMask|ButtonReleaseMask|PointerMotionMask|EnterWindowMask|LeaveWindowMask,
	                     &(data->last_x), &(data->last_y),
						 &root_x, &root_y,
						 &(data->pointer_state));

    data->stop_on_button_press = ((data->pointer_state&ButtonAnyMask) == 0 );

    data->origin_x = parent->root_x + data->last_x - mr->root_x;
    data->origin_y = parent->root_y + data->last_y - mr->root_y;
    /* we should be using this methinks: */
    /* data->last_x = root_x ;
     * data->last_y = root_y ;
	 */

	/* " %u x %u %+d %+d " */
	data->geometry_string = safemalloc( 1+6+3+6+2+6+2+6+1+1 +30/*for the heck of it*/);
#ifndef NO_ASRENDER
    width = look->resize_move_geometry.width ;
    height = look->resize_move_geometry.height ;
#else
    data->geom_bar = create_astbar();
    set_astbar_style(data->geom_bar, BAR_STATE_UNFOCUSED, look->MSWindow[BACK_FOCUSED]->name );

    add_astbar_label( data->geom_bar, 0, 0, 0, ALIGN_CENTER, DEFAULT_TBAR_SPACING, DEFAULT_TBAR_SPACING, "88888 x 88888 +88888 +88888", AS_Text_ASCII );
    width = calculate_astbar_width( data->geom_bar );
    height = calculate_astbar_height( data->geom_bar );
    if( width < look->resize_move_geometry.width )
        width = look->resize_move_geometry.width;
    if( height < look->resize_move_geometry.height )
        height = look->resize_move_geometry.height ;
#endif
    data->geometry_window_width = width ;
    data->geometry_window_height = height ;

	if( get_flags( look->resize_move_geometry.flags, XValue ) )
	{
		x = look->resize_move_geometry.x ;
		if( get_flags( look->resize_move_geometry.flags, XNegative) )
            x += scr->MyDisplayWidth - width ;
	}
	if( get_flags( look->resize_move_geometry.flags, YValue ) )
	{
		y = look->resize_move_geometry.y ;
		if( get_flags( look->resize_move_geometry.flags, YNegative) )
            y += scr->MyDisplayHeight - height ;
	}

	attr.override_redirect = True ;
	attr.save_under = True ;
    data->geometry_display = create_screen_window( AS_WIDGET_SCREEN(parent), None,
                                x, y, width, height,
					  			1, InputOutput,
					  			CWOverrideRedirect|CWSaveUnder, &attr );
#ifndef NO_ASRENDER
    XMapRaised( dpy, data->geometry_display );
    RendAddWindow( scr, data->geometry_display, REND_Visible, REND_Visible );
	RendSetWindowStyles( scr, data->geometry_display, look->MSWindow[BACK_FOCUSED], look->MSWindow[BACK_FOCUSED]);
	RendAddLabel( scr, data->geometry_display, 1, REND_Visible, 0, NULL );
	RendMaskWindowState( scr, data->geometry_display, 0, REND_Visible );
#else
    set_astbar_size( data->geom_bar, width, height );
    data->geom_canvas = create_ascanvas(data->geometry_display);
    render_astbar( data->geom_bar, data->geom_canvas);
    update_canvas_display( data->geom_canvas );
    XMapRaised( dpy, data->geometry_display );
    data->below_sibling = data->geometry_display ;
#endif
	LOCAL_DEBUG_OUT( "curr(%+d%+d)->last(%+d%+d)", data->curr.x, data->curr.y, data->last_x, data->last_y );

	if( data->pointer_func != NULL )
		data->pointer_func( data, data->last_x, data->last_y );
}

static void
update_geometry_display( ASMoveResizeData *data )
{
    int display_width = (int)data->curr.width-(int)data->frame_width ;
    int display_height = (int)data->curr.height-(int)data->frame_height ;
    int display_x = data->curr.x - data->geom_x_origin;
    int display_y = data->curr.y - data->geom_y_origin;
	
	if( display_height < 0 ) 
		display_height = 0 ;
	if( display_width < 0 ) 
		display_width = 0 ;

    if( data->geom_x_mult > 0 && data->geom_x_div > 0 )
    {
        display_x = (display_x*data->geom_x_mult)/data->geom_x_div ;
        display_width = (display_width*data->geom_x_mult)/data->geom_x_div ;
    }
    if( data->geom_y_mult > 0 && data->geom_y_div > 0 )
    {
        display_y = (display_y*data->geom_y_mult)/data->geom_y_div ;
        display_height = (display_height*data->geom_y_mult)/data->geom_y_div ;
    }
	if( data->width_inc != 0 )
        display_width /= (int)data->width_inc ;
    if( data->height_inc != 0 )
        display_height /= (int)data->height_inc ;
    if( display_width == 0 && display_height != 0 ) 
		sprintf (data->geometry_string, "x %d %+d %+d", display_height, display_x, display_y );
	else if( display_width != 0 && display_height == 0 ) 
		sprintf (data->geometry_string, "%d x  %+d %+d", display_width, display_x, display_y );
	else if( display_width != 0 && display_height != 0 ) 
		sprintf (data->geometry_string, "%d x %d %+d %+d", display_width, display_height, display_x, display_y );
	else
		sprintf (data->geometry_string, "%+d %+d",display_x, display_y );
#ifndef NO_ASRENDER
    RendChangeLabel( AS_WIDGET_SCREEN(data->parent), data->geometry_display, 1, data->geometry_string );
#else
    change_astbar_first_label( data->geom_bar, data->geometry_string, AS_Text_ASCII );
    render_astbar( data->geom_bar, data->geom_canvas);
    update_canvas_display( data->geom_canvas );
    XRaiseWindow( dpy, data->geometry_display );
    ASSync(False);
#endif
	if( !get_flags( data->look->resize_move_geometry.flags, XValue|YValue ) )
	{
		int x, y ;
        int width = data->geometry_window_width ;
        int height = data->geometry_window_height ;
		x = data->last_x + 10;
		y = data->last_y + 10;
		if( x + width > AS_WIDGET_SCREEN(data->parent)->MyDisplayWidth )
			x = data->last_x - (width+5) ;
		if( y + height > AS_WIDGET_SCREEN(data->parent)->MyDisplayHeight )
			y = data->last_y - (height+5) ;
		XMoveWindow( dpy, data->geometry_display, x, y );
		XSync( dpy, False );
	}
}

static void
flush_move_resize_data( ASMoveResizeData *data )
{

#ifdef AS_WIDGET_H_HEADER_INCLUDED
    XUngrabPointer( dpy, CurrentTime );
#else
	if( _as_ungrab_screen_func )
    	_as_ungrab_screen_func();
#endif
#ifdef NO_ASRENDER
    if( data->geom_bar )
        destroy_astbar( &(data->geom_bar ) );
    if( data->geom_canvas )
        destroy_ascanvas( &(data->geom_canvas ) );
#endif
    if( data->geometry_string )
		free( data->geometry_string );
	if( data->geometry_display )
		XDestroyWindow( dpy, data->geometry_display );
	if( data->outline )
		destroy_outline_segments( &(data->outline) );
	if( data->grid )
		destroy_asgrid( data->grid, False );
}



/***********************************************************************
 * Main move-resize loop :
 * Move the rubberband around, return with the new window location
 ***********************************************************************/
void complete_interactive_action( ASMoveResizeData *data, Bool cancel )
{
	XEvent client_event ;

	if( data->outline )
		destroy_outline_segments( &(data->outline) );

	if( data->complete_func )
		data->complete_func(data, cancel);
	if( !cancel && data->pointer_func == move_func )
	{
	    client_event.type = ConfigureNotify;
  		client_event.xconfigure.display = dpy;
	    client_event.xconfigure.event  = AS_WIDGET_WINDOW(data->mr);
	    client_event.xconfigure.window = client_event.xconfigure.event ;

  		client_event.xconfigure.x = data->curr.x ;
	    client_event.xconfigure.y = data->curr.y ;
  		client_event.xconfigure.width = data->curr.width ;
	    client_event.xconfigure.height = data->curr.height ;

  		client_event.xconfigure.border_width = 0;
	    /* Real ConfigureNotify events say we're above title window, so ... */
  		/* what if we don't have a title ????? */
	    client_event.xconfigure.above = AS_WIDGET_SCREEN(data->mr)->Root;
  		client_event.xconfigure.override_redirect = False;
		/* only send it ourselves */
	    XSendEvent (dpy, client_event.xconfigure.event, False, 0, &client_event);
	}

	/* its a good idea to send out a syntetic ConfigureNotify */

	stop_widget_moveresize();
	flush_move_resize_data( data );
	free( data );
}


Bool
move_resize_loop (ASMoveResizeData *data, ASEvent *event )
{
	Bool          finished = False;
	int           new_x, new_y;
SHOW_CHECKPOINT;
	/* discard any extra motion events before a logical release */
	if( AS_ASSERT(event) || AS_ASSERT(data))
		return False;
LOCAL_DEBUG_OUT("widget(%p)->parent(%p)", event->widget, data->parent );
    if( event->widget != data->parent )
		return False;

	if( event->eclass&ASE_POINTER_EVENTS )
	{
SHOW_CHECKPOINT;
		data->pointer_state = event->x.xbutton.state ;
SHOW_CHECKPOINT;
		if( data->subwindow_func &&
			event->x.xbutton.subwindow != data->curr_subwindow )
		{
SHOW_CHECKPOINT;
			data->subwindow_func( data, event );
SHOW_CHECKPOINT;
			data->curr_subwindow = event->x.xbutton.subwindow ;
		}
SHOW_CHECKPOINT;
		if (event->x.type == MotionNotify)
		{
			while (ASCheckMaskEvent(PointerMotionMask | ButtonMotionMask |
									ButtonPressMask | ButtonRelease, &(event->x)))
            {
                if( data->stop_on_button_press )
                {
                    if (event->x.type == ButtonRelease)
                        break;
                }else if (event->x.type == ButtonPress)
                    break;
            }
SHOW_CHECKPOINT;
            if( data->stop_on_button_press )
            {
                if( get_flags(event->x.xmotion.state, ButtonAnyMask) )
                {
                    /* some button is pressed !!! */
                    complete_interactive_action( data, True );
                    return 0;
                }
SHOW_CHECKPOINT;
            }else if( !get_flags(event->x.xmotion.state, AllButtonMask) )
			{/* all the buttons are depressed !!! */
				complete_interactive_action( data, False );
				return 0;
			}
SHOW_CHECKPOINT;
		}
	}
SHOW_CHECKPOINT;
	switch (event->x.type)
	{   /* Handle a limited number of key press events to allow mouseless
		 * operation */
		case ConfigureNotify :
			return (data->pointer_func==move_func)?ASE_Consumed:0 ;
		case KeyPress:
		/* Keyboard_shortcuts (&Event, ButtonRelease, 20); */
			break;
        case ButtonPress:
        case ButtonRelease:
SHOW_CHECKPOINT;
            if( (data->stop_on_button_press?1:0) == (event->x.type==ButtonPress?1:0))
                finished = True;
        case MotionNotify:
			/* update location of the pager_view window */
			{
				/* must get current pointer position as we may get late while
				   doing all the redrawiong stuff */
				if( !ASQueryPointerRootXY( &new_x, &new_y ) )
				{
					new_x = event->x.xmotion.x_root ;
					new_y = event->x.xmotion.y_root ;
				}
          		new_x -= data->parent->root_x;
          		new_y -= data->parent->root_y;
LOCAL_DEBUG_OUT("new = %+d%+d, finished = %d", new_x, new_y, finished );
			}
			data->pointer_func (data, new_x, new_y);
			break;
        default:
SHOW_CHECKPOINT;
			return 0;
	}
	data->last = data->curr;
	if( finished )
		complete_interactive_action( data, False );
SHOW_CHECKPOINT;
	return ASE_Consumed;
}

/***********************************************************************
 * Snapping to the grid :
 **********************************************************************/
#define ATTRACT_SIDE_ABOVE(t,b,band,grav,size) \
	if( (grav) > 0 ) \
	{	if( (t) > (band)-(grav) && (t) < (band) )	(t) = (band) ; \
		else if( size > 0 && (b) > (band)-(grav) && (b) < (band)) (t) = (band)-(size);} \

#define ATTRACT_SIDE_BELOW(t,b,band,grav,size) \
	if( (grav) > 0 ) \
	{ 	if( (t) < (band)+(grav) && (t) > (band) )	(t) = (band) ; \
	  	else if( size > 0 && (b) < (band)+(grav) && (b) > (band)) (t) = (band)-(size);} \

int
attract_side( register ASGridLine *l, int pos, int size, int lim1, int lim2 )
{
	int head = pos;
	int tail = head+size ;

	while( l != NULL )
	{
		if( lim2 >= l->start && lim1 <= l->end )
		{
			ATTRACT_SIDE_ABOVE(head,tail,l->band,l->gravity_above,size);
			ATTRACT_SIDE_BELOW(head,tail,l->band,l->gravity_below,size);
			if( head != pos )
			{
LOCAL_DEBUG_OUT( "attracted by %d, %d-%d, %d,%d", l->band, l->start, l->end, l->gravity_above, l->gravity_below );
				return head;
			}
		}
		l = l->next ;
	}
	return pos;
}

int
resist_west_side( register ASGridLine *l, int pos, int new_pos, int lim1,  int lim2 )
{
	while( l != NULL )
	{
/*LOCAL_DEBUG_OUT( "lim = %+d%+d, l = (%d,%d), pos = (%d,%d), band = %d, grav = %d", lim1, lim2, l->start, l->end, pos, new_pos, l->band, l->gravity_above );*/
		if( lim2 >= l->start && lim1 <= l->end && l->gravity_above < 0 &&
            l->band <= pos && l->band >= new_pos )
            new_pos = l->band ;
		l = l->next ;
	}
	return MIN(pos, new_pos);
}

int
resist_east_side( register ASGridLine *l, int pos, int new_pos, int lim1,  int lim2 )
{
	while( l != NULL )
	{
		if( lim2 >= l->start && lim1 <= l->end && l->gravity_below < 0 &&
            l->band >= pos && l->band <= new_pos )
            new_pos = l->band ;
		l = l->next ;
	}
	return MAX(pos, new_pos);
}

Bool
attract_corner( ASGrid *grid, int *x_inout, int *y_inout, struct MRRectangle *curr, int bw )
{
	int new_left ;
	int new_top ;
	Bool res = False ;

	if( grid )
	{
		int bw_addon = bw*2 ;
		new_left = attract_side( grid->v_lines, *x_inout, curr->width+bw_addon,  *y_inout, *y_inout+curr->height+bw_addon);
		new_top  = attract_side( grid->h_lines, *y_inout, curr->height+bw_addon, *x_inout, *x_inout+curr->width+bw_addon );
LOCAL_DEBUG_OUT( "attracted(%+d%+d) orinal(%+d%+d)", new_left, new_top, curr->x, curr->y );
		if( new_left > curr->x )  /* moving eastwards : */
			new_left = resist_east_side( grid->v_lines, curr->x+curr->width+bw_addon, new_left+curr->width+bw_addon, new_top, new_top+curr->height+bw_addon )-(curr->width+bw_addon);
		else if( new_left != curr->x )
			new_left = resist_west_side( grid->v_lines, curr->x, new_left, new_top, new_top+curr->height+bw_addon );
		if( new_top > curr->y )  /* moving southwards : */
			new_top = resist_east_side( grid->h_lines, curr->y+curr->height+bw_addon, new_top+curr->height+bw_addon, new_left, new_left+curr->width+bw_addon )-(curr->height+bw_addon);
		else if( new_top != curr->y )
			new_top = resist_west_side( grid->h_lines, curr->y, new_top, new_left, new_left+curr->width+bw_addon );
		res = (new_top != *y_inout || new_left != *x_inout );
		*x_inout = new_left ;
		*y_inout = new_top ;
	}
	return res ;
}

int adjust_west_side( ASGridLine *gridlines, int dpos, int *pos_inout, int *size_inout, int lim1, int lim2 )
{
	int pos = *pos_inout, new_pos = pos+dpos ;
	int adjusted_dpos = dpos;

	if( gridlines )
	{
		new_pos = attract_side( gridlines, new_pos, 0, lim1, lim2 );
		if( new_pos < pos )
		{ /* we need to resist move if we are offending any negative gridline */
			new_pos = resist_west_side( gridlines, pos, new_pos, lim1, lim2 );
		}
	}
	adjusted_dpos = MIN(new_pos-pos,*size_inout-1) ;

	*pos_inout += adjusted_dpos ;
	*size_inout -= adjusted_dpos ;
LOCAL_DEBUG_OUT( "pos = %d, new_pos = %d, lim1 = %d, lim2 = %d, dpos = %d, adjusted_dpos = %d", pos, new_pos, lim1, lim2, dpos, adjusted_dpos );
	return adjusted_dpos;
}

int adjust_east_side( ASGridLine *gridlines, int dpos, int pos, int *size_inout, int lim1, int lim2 )
{
	int adjusted_dpos = dpos;
	int new_pos = pos;

	if( gridlines )
	{
		pos += *size_inout ;
		new_pos = pos+dpos ;
		new_pos = attract_side( gridlines, new_pos, 0, lim1, lim2 );
		if( new_pos > pos )
		{ /* we need to resist move if we are offending any negative gridline */
			new_pos = resist_east_side( gridlines, pos, new_pos, lim1, lim2 );
		}
		adjusted_dpos = new_pos-pos ;
	}
	if( adjusted_dpos + *size_inout <= 0 )
		adjusted_dpos = 1-(int)(*size_inout) ;

	*size_inout += adjusted_dpos ;
LOCAL_DEBUG_OUT( "pos = %d, new_pos = %d, lim1 = %d, lim2 = %d, dpos = %d, adjusted_dpos = %d", pos, new_pos, lim1, lim2, dpos, adjusted_dpos );
	return adjusted_dpos;
}

int restrain_east_side( int dpos, int *size_inout, int min_val, int incr, int max_val )
{
	int adjusted_dpos = dpos;
    int size = *size_inout;

    if( size < min_val )
        size = min_val ;
    else if ( max_val > 0 && size > max_val )
        size = max_val ;
    else if( incr > 0 )
        size = min_val+((size-min_val)/incr)*incr ;

    adjusted_dpos += size - (*size_inout);
LOCAL_DEBUG_OUT( "in_size = %d, out_size = %d, min_val = %d, incr = %d, max_val = %d, dpos = %d, adjusted_dpos = %d", *size_inout, size, min_val, incr, max_val, dpos, adjusted_dpos );
    *size_inout = size ;
	return adjusted_dpos;
}

/**********************************************************************/
/* Actions : **********************************************************/
/**********************************************************************/
void
move_func (struct ASMoveResizeData *data, int x, int y)
{
	int dx, dy ;
	int new_x, new_y ;

LOCAL_DEBUG_CALLER_OUT( "data = %p, pos = (%+d%+d)", data, x, y );
	dx = x-(data->last_x+data->lag_x) ;
	dy = y-(data->last_y+data->lag_y) ;

	new_x = data->curr.x + dx ;
	new_y = data->curr.y + dy ;
LOCAL_DEBUG_OUT( "pointer_state = %X, no_snap_mod = %X", data->pointer_state&AllModifierMask, data->feel->no_snaping_mod );
LOCAL_DEBUG_OUT( "pos(%+d%+d)->delta(%+d%+d)->new(%+d%+d)->lag(%+d%+d)->last(%+d%+d)", x, y, dx, dy, new_x, new_y, data->lag_x, data->lag_y, data->last_x, data->last_y );
    if( data->grid && (data->pointer_state&AllModifierMask) != data->feel->no_snaping_mod )
	{
		attract_corner( data->grid, &new_x, &new_y, &(data->curr), data->bw );
		dx = new_x-data->curr.x ;
		dy = new_y-data->curr.y ;
	}
    data->lag_x  = -(x - dx - data->last_x - data->lag_x) ;
	data->lag_y  = -(y - dy - data->last_y - data->lag_y) ;
	data->curr.x = new_x ;
	data->curr.y = new_y ;
	data->last_x = x ;
	data->last_y = y ;
LOCAL_DEBUG_OUT( "last_geom(%+d%+d)->curr_geom(%+d%+d)->delta(%+d%+d)->lag(%+d%+d)->last(%+d%+d)", data->last.x, data->last.y, data->curr.x, data->curr.y, dx, dy, data->lag_x, data->lag_y, data->last_x, data->last_y );
 
/*  fprintf( stderr, "move_func: (x,y) =(%d,%d) to %+d%+d\n", x, y, pdata->new_x, pdata->new_y );
*/
/*	resist_move (pdata); */
	if( data->curr.x != data->last.x || data->curr.y != data->last.y )
	{
		data->apply_func( data );
	}
	update_geometry_display( data );
}

void
resize_func (struct ASMoveResizeData *data, int x, int y)
{
	int dx, dy, real_dx, real_dy ;
	ASGridLine *h_lines = NULL, *v_lines = NULL;
	int bw_addon = data->bw*2 ;

	real_dx = dx = x-(data->last_x+data->lag_x) ;
	real_dy = dy = y-(data->last_y+data->lag_y) ;

	if( data->grid && (data->pointer_state&AllModifierMask) != data->feel->no_snaping_mod )
	{
		h_lines = data->grid->h_lines ;
		v_lines = data->grid->v_lines ;
	}

	switch( data->side )
	{
		case FR_N :
		case FR_NE :
		case FR_NW :
			real_dy = adjust_west_side( h_lines, dy, &(data->curr.y), &(data->curr.height), data->curr.x+dx, data->curr.x+data->curr.width+dx+bw_addon );
            break ;

		case FR_S :
		case FR_SE:
		case FR_SW:
			real_dy = adjust_east_side( h_lines, dy, data->curr.y, &(data->curr.height), data->curr.x+dx, data->curr.x+data->curr.width+dx+bw_addon );
            real_dy = restrain_east_side( real_dy, &(data->curr.height), data->min_height, data->height_inc, data->max_height );
            break ;
	}
	switch( data->side )
	{
		case FR_E :
		case FR_NE :
		case FR_SE :
			real_dx = adjust_east_side( v_lines, dx, data->curr.x, &(data->curr.width), data->curr.y, data->curr.y+data->curr.height+bw_addon );
            real_dx = restrain_east_side( real_dx, &(data->curr.width), data->min_width, data->width_inc, data->max_width );
			break ;
		case FR_W :
		case FR_SW :
		case FR_NW :
			real_dx = adjust_west_side( v_lines, dx, &(data->curr.x), &(data->curr.width), data->curr.y, data->curr.y+data->curr.height+bw_addon );
			break ;
	}

	data->lag_x = real_dx - dx ;
	data->lag_y = real_dy - dy ;
	data->last_x = x ;
	data->last_y = y ;

	if( data->curr.x != data->last.x || data->curr.y != data->last.y ||
		data->curr.width != data->last.width || data->curr.height != data->last.height )
		data->apply_func( data );
	update_geometry_display( data );

LOCAL_DEBUG_OUT( " dx = %d, width  = %d, x = %d, lag_x = %d", dx, data->curr.width, data->curr.x, data->lag_x );
LOCAL_DEBUG_OUT( " dy = %d, height = %d, y = %d, lag_y = %d", dy, data->curr.height, data->curr.y, data->lag_y );
/*  fprintf( stderr, "move_func: (x,y) =(%d,%d) to %+d%+d\n", x, y, pdata->new_x, pdata->new_y );
*/
/*	resist_move (pdata); */
}


ASMoveResizeData*
move_widget_interactively( ASWidget *parent, ASWidget *mr, ASEvent *trigger,
						   as_interactive_apply_handler    apply_func,
						   as_interactive_complete_handler complete_func )
{
	ASMoveResizeData *data = safecalloc( 1, sizeof(ASMoveResizeData));

	if( !start_widget_moveresize( data, move_resize_loop ) )
	{
		free( data );
		data = NULL ;
	}else
	{
		data->pointer_func = move_func ;
		prepare_move_resize_data( data, parent, mr, trigger, apply_func, complete_func, AS_WIDGET_FEEL(mr)->OpaqueMove );
	}
	return data;
}

ASMoveResizeData*
resize_widget_interactively( ASWidget *parent, ASWidget *mr, ASEvent *trigger,
	  						 as_interactive_apply_handler    apply_func,
							 as_interactive_complete_handler complete_func,
							 int side )
{
	ASMoveResizeData *data = safecalloc( 1, sizeof(ASMoveResizeData));

	if( !start_widget_moveresize( data, move_resize_loop ) )
	{
		free( data );
		data = NULL ;
	}else
	{
		data->pointer_func = resize_func ;
		prepare_move_resize_data( data, parent, mr, trigger, apply_func, complete_func, AS_WIDGET_FEEL(mr)->OpaqueResize );
        data->side = (side >= FR_N && side < FRAME_PARTS)?side:FR_SW ;
LOCAL_DEBUG_OUT( "requested side = %d, using side = %d", side, data->side );
    }
	return data;
}

void
set_moveresize_restrains( ASMoveResizeData *data, ASHints *hints, ASStatusHints *status )
{
    if( data )
    {
        Bool ignore_minmax_size = False ;
        if( status )
        {
            data->frame_width = status->frame_size[FR_W]+status->frame_size[FR_E];
            data->frame_height = status->frame_size[FR_N]+status->frame_size[FR_S];
            ignore_minmax_size = get_flags( status->flags, AS_Shaded);
        }

        if( hints )
        {
            if( !ignore_minmax_size )
            {
                if( hints->min_width > 0 )
                    data->min_width = hints->min_width+data->frame_width ;
                if( hints->max_width > 0 )
                    data->max_width = hints->max_width+data->frame_width ;
                if( hints->min_height > 0 )
                    data->min_height = hints->min_height+data->frame_height ;
                if( hints->max_height > 0 )
                    data->max_height = hints->max_height+data->frame_height ;
            }
            data->width_inc = hints->width_inc ;
            data->height_inc = hints->height_inc ;
        }

        if( data->height_inc != 0 || data->width_inc != 0 ||
            data->frame_width != 0 || data->frame_height != 0 )
            update_geometry_display( data );
    }
}

void
set_moveresize_aspect( ASMoveResizeData *data, int x_mult, int x_div, int y_mult, int y_div, int x_origin, int y_origin  )
{
    if( data )
    {
        data->geom_x_mult = x_mult ;
        data->geom_x_div  = x_div ;
        data->geom_y_mult = y_mult ;
        data->geom_y_div  = y_div ;
        data->geom_x_origin = x_origin ;
        data->geom_y_origin = y_origin ;
        update_geometry_display( data );
    }
}


