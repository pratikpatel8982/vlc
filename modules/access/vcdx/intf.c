/*****************************************************************************
 * intf.c: Video CD interface to handle user interaction and still time
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: intf.c,v 1.1 2003/10/04 18:55:13 gbazin Exp $
 *
 * Authors: St�phane Borel <stef@via.ecp.fr>
 *          Current modification and breakage for VCD by rocky.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>
#include <unistd.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "vcd.h"

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    input_thread_t *    p_input;
    vcd_data_t *        p_vcd;

    vlc_bool_t          b_still;
    vlc_bool_t          b_inf_still;
    mtime_t             m_still_time;

#if FINISHED
    vcdplay_ctrl_t      control;
#else 
    int                 control;
#endif
    vlc_bool_t          b_click, b_move, b_key_pressed;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  InitThread     ( intf_thread_t *p_intf );
static int  MouseEvent     ( vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void * );
static int  KeyEvent       ( vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void * );

/* Exported functions */
static void RunIntf        ( intf_thread_t *p_intf );

/*****************************************************************************
 * OpenIntf: initialize dummy interface
 *****************************************************************************/
int E_(VCDOpenIntf) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    printf("+++++Called VCDOpenIntf\n");
    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( 1 );
    };

    p_intf->pf_run = RunIntf;

    p_intf->p_sys->m_still_time = 0;
    p_intf->p_sys->b_inf_still = 0;
    p_intf->p_sys->b_still = 0;

    return( 0 );
}

/*****************************************************************************
 * CloseIntf: destroy dummy interface
 *****************************************************************************/
void E_(VCDCloseIntf) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Destroy structure */
    free( p_intf->p_sys );
}


/*****************************************************************************
 * RunIntf: main loop
 *****************************************************************************/
static void RunIntf( intf_thread_t *p_intf )
{
    vlc_object_t *      p_vout = NULL;
    printf("+++++Called RunIntf\n");

    if( InitThread( p_intf ) < 0 )
    {
        msg_Err( p_intf, "can't initialize intf" );
        return;
    }
    msg_Dbg( p_intf, "intf initialized" );

    /* Main loop */
    while( !p_intf->b_die )
    {
      vlc_mutex_lock( &p_intf->change_lock );

        /*
         * keyboard event
         */
        if( p_vout && p_intf->p_sys->b_key_pressed )
        {
            p_intf->p_sys->b_key_pressed = VLC_FALSE;
            
            printf("++++key pressed...\n");
        }
                
        vlc_mutex_unlock( &p_intf->change_lock );

        if( p_vout == NULL )
        {
            p_vout = vlc_object_find( p_intf->p_sys->p_input,
                                      VLC_OBJECT_VOUT, FIND_CHILD );
            if( p_vout )
            {
                var_AddCallback( p_vout, "mouse-moved", MouseEvent, p_intf );
                var_AddCallback( p_vout, "mouse-clicked", MouseEvent, p_intf );
                var_AddCallback( p_vout, "key-pressed", KeyEvent, p_intf );
            }
        }


        /* Wait a bit */
        msleep( INTF_IDLE_SLEEP );
    }

    vlc_object_release( p_intf->p_sys->p_input );
}

/*****************************************************************************
 * InitThread:
 *****************************************************************************/
static int InitThread( intf_thread_t * p_intf )
{
    /* We might need some locking here */
    if( !p_intf->b_die )
    {
        input_thread_t * p_input;
        vcd_data_t * p_vcd;

        p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_PARENT );

        /* Maybe the input just died */
        if( p_input == NULL )
        {
            return VLC_EGENERIC;
        }

        p_vcd = (vcd_data_t*)p_input->p_access_data;
        p_vcd->p_intf = p_intf;

        vlc_mutex_lock( &p_intf->change_lock );

        p_intf->p_sys->p_input = p_input;
        p_intf->p_sys->p_vcd = p_vcd;

        p_intf->p_sys->b_move = VLC_FALSE;
        p_intf->p_sys->b_click = VLC_FALSE;
        p_intf->p_sys->b_key_pressed = VLC_FALSE;

        vlc_mutex_unlock( &p_intf->change_lock );

        return VLC_SUCCESS;
    }
    else
    {
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * MouseEvent: callback for mouse events
 *****************************************************************************/
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;

    vlc_mutex_lock( &p_intf->change_lock );

    if( psz_var[6] == 'c' ) /* "mouse-clicked" */
    {
        p_intf->p_sys->b_click = VLC_TRUE;
    }
    else if( psz_var[6] == 'm' ) /* "mouse-moved" */
    {
        p_intf->p_sys->b_move = VLC_TRUE;
    }

    vlc_mutex_unlock( &p_intf->change_lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * KeyEvent: callback for keyboard events
 *****************************************************************************/
static int KeyEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    vlc_mutex_lock( &p_intf->change_lock );

    p_intf->p_sys->b_key_pressed = VLC_TRUE;
    
    vlc_mutex_unlock( &p_intf->change_lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * dvdIntfStillTime: function provided to demux plugin to request
 * still images
 *****************************************************************************/
int vcdIntfStillTime( intf_thread_t *p_intf, int i_sec )
{
    vlc_mutex_lock( &p_intf->change_lock );

    if( i_sec == 0xff )
    {
        p_intf->p_sys->b_still = 1;
        p_intf->p_sys->b_inf_still = 1;
    }
    else if( i_sec > 0 )
    {
        p_intf->p_sys->b_still = 1;
        p_intf->p_sys->m_still_time = 1000000 * i_sec;
    }
    vlc_mutex_unlock( &p_intf->change_lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * vcdIntfStillTime: function provided to reset still image
 *****************************************************************************/
int vcdIntfResetStillTime( intf_thread_t *p_intf )
{
    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->m_still_time = 0;
    input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );
    vlc_mutex_unlock( &p_intf->change_lock );

    return VLC_SUCCESS;
}
