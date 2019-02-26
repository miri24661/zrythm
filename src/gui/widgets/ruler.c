/*
 * Copyright (C) 2018-2019 Alexandros Theodotou
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * \file
 *
 * Ruler widget.
 */

#include <math.h>

#include "actions/actions.h"
#include "audio/position.h"
#include "audio/transport.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/midi_arranger.h"
#include "gui/widgets/midi_modifier_arranger.h"
#include "gui/widgets/midi_ruler.h"
#include "gui/widgets/ruler.h"
#include "gui/widgets/ruler_playhead.h"
#include "gui/widgets/ruler_marker.h"
#include "gui/widgets/ruler_range.h"
#include "gui/widgets/timeline_arranger.h"
#include "gui/widgets/timeline_ruler.h"
#include "project.h"
#include "settings/settings.h"
#include "utils/ui.h"
#include "zrythm.h"

#include <gtk/gtk.h>

G_DEFINE_TYPE_WITH_PRIVATE (RulerWidget,
                            ruler_widget,
                            GTK_TYPE_OVERLAY)

#define Y_SPACING 5
#define FONT "Monospace"
#define FONT_SIZE 14

     /* FIXME delete these, see ruler_marker.h */
#define START_MARKER_TRIANGLE_HEIGHT 8
#define START_MARKER_TRIANGLE_WIDTH 8
#define Q_HEIGHT 12
#define Q_WIDTH 7

#define GET_PRIVATE RULER_WIDGET_GET_PRIVATE (self)

/**
 * Gets called to set the position/size of each overlayed widget.
 */
static gboolean
get_child_position (GtkOverlay   *overlay,
                    GtkWidget    *widget,
                    GdkRectangle *allocation,
                    gpointer      user_data)
{
  RulerWidget * self =
    Z_RULER_WIDGET (overlay);

  if (Z_IS_RULER_PLAYHEAD_WIDGET (widget))
    {
      if (Z_IS_TIMELINE_RULER_WIDGET (self))
        allocation->x =
          ui_pos_to_px_timeline (
            &TRANSPORT->playhead_pos,
            1) - (PLAYHEAD_TRIANGLE_WIDTH / 2);
      /*else*/
        /*allocation->x =*/
          /*ui_pos_to_px_piano_roll (*/
            /*&TRANSPORT->playhead_pos,*/
            /*1) - (PLAYHEAD_TRIANGLE_WIDTH / 2);*/
      allocation->y =
        gtk_widget_get_allocated_height (GTK_WIDGET (self)) -
          PLAYHEAD_TRIANGLE_HEIGHT;
      allocation->width = PLAYHEAD_TRIANGLE_WIDTH;
      allocation->height =
        PLAYHEAD_TRIANGLE_HEIGHT;
    }
  else if (Z_IS_RULER_RANGE_WIDGET (widget))
    {
      timeline_ruler_widget_set_ruler_range_position (
        Z_TIMELINE_RULER_WIDGET (self),
        Z_RULER_RANGE_WIDGET (widget),
        allocation);
    }
  else if (Z_IS_RULER_MARKER_WIDGET (widget))
    {
      if (Z_IS_TIMELINE_RULER_WIDGET (self))
        timeline_ruler_widget_set_ruler_marker_position (
          Z_TIMELINE_RULER_WIDGET (self),
          Z_RULER_MARKER_WIDGET (widget),
          allocation);
      else
        midi_ruler_widget_set_ruler_marker_position (
          Z_MIDI_RULER_WIDGET (self),
          Z_RULER_MARKER_WIDGET (widget),
          allocation);
    }

  return TRUE;
}

RulerWidgetPrivate *
ruler_widget_get_private (RulerWidget * self)
{
  return ruler_widget_get_instance_private (self);
}

static gboolean
ruler_draw_cb (GtkWidget * widget,
         cairo_t *cr,
         RulerWidget * self)
{
  /* engine is run only set after everything is set up
   * so this is a good way to decide if we should draw
   * or not */
  if (!AUDIO_ENGINE->run)
    return FALSE;

  GdkRectangle rect;
  gdk_cairo_get_clip_rectangle (cr,
                                &rect);

  GET_PRIVATE;

  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));

  /*width = gtk_widget_get_allocated_width (widget);*/
  guint height =
    gtk_widget_get_allocated_height (widget);

  gtk_render_background (context, cr,
                         rect.x, rect.y,
                         rect.width, rect.height);

  /* draw lines */
  for (int i = rect.x; i < rect.x + rect.width; i++)
    {
      int draw_pos = i + SPACE_BEFORE_START;

      if (i % rw_prv->px_per_bar == 0)
      {
        cairo_set_source_rgb (cr, 1, 1, 1);
        cairo_set_line_width (cr, 1);
        cairo_move_to (cr, draw_pos, 0);
        cairo_line_to (cr, draw_pos, height / 3);
        cairo_stroke (cr);
        cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);
        cairo_select_font_face(cr, FONT,
            CAIRO_FONT_SLANT_NORMAL,
            CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, FONT_SIZE);
        gchar * label =
          g_strdup_printf ("%d", i / rw_prv->px_per_bar + 1);
        static cairo_text_extents_t extents;
        cairo_text_extents(cr, label, &extents);
        cairo_move_to (cr,
                       (draw_pos ) - extents.width / 2,
                       (height / 2) + Y_SPACING);
        cairo_show_text(cr, label);
      }
      else if (i % rw_prv->px_per_beat == 0)
      {
          cairo_set_source_rgb (cr, 0.7, 0.7, 0.7);
          cairo_set_line_width (cr, 0.5);
          cairo_move_to (cr, draw_pos, 0);
          cairo_line_to (cr, draw_pos, height / 4);
          cairo_stroke (cr);
      }
  }

 return FALSE;
}

/**
 * TODO: for updating the global static variables
 * when needed
 */
void
reset_ruler ()
{

}

static void
on_motion (GtkDrawingArea * da,
           GdkEventMotion *event,
           RulerWidget *    self)
{
  if (self != MW_RULER)
    return;

  int height = gtk_widget_get_allocated_height (
    GTK_WIDGET (da));
  if (event->type == GDK_MOTION_NOTIFY)
    {
      /* if lower 3/4ths */
      if (event->y > (height * 1) / 4)
        {
          /* set cursor to normal */
          ui_set_cursor (GTK_WIDGET (self),
                         "default");
        }
      else /* upper 1/4th */
        {
          /* set cursor to range selection */
          ui_set_cursor (GTK_WIDGET (self),
                         "text");
        }
    }
}

void
ruler_widget_refresh (RulerWidget * self)
{
  if (Z_IS_TIMELINE_RULER_WIDGET (self))
    timeline_ruler_widget_refresh ();
  else
    midi_ruler_widget_refresh ();
}

/**
 * Sets zoom level and disables/enables buttons
 * accordingly.
 *
 * Returns if the zoom level was set or not.
 */
int
ruler_widget_set_zoom_level (RulerWidget * self,
                             float         zoom_level)
{
  if (zoom_level > MAX_ZOOM_LEVEL)
    {
      action_disable_window_action ("zoom-in");
    }
  else
    {
      action_enable_window_action ("zoom-in");
    }
  if (zoom_level < MIN_ZOOM_LEVEL)
    {
      action_enable_window_action ("zoom-out");
    }
  else
    {
      action_enable_window_action ("zoom-out");
    }

  int update = zoom_level >= MIN_ZOOM_LEVEL &&
    zoom_level <= MAX_ZOOM_LEVEL;

  if (update)
    {
      RULER_WIDGET_GET_PRIVATE (self);
      rw_prv->zoom_level = zoom_level;
      ruler_widget_refresh (self);
      return 1;
    }
  else
    {
      return 0;
    }
}

static void
ruler_widget_init (RulerWidget * self)
{
  g_message ("initing ruler...");
  GET_PRIVATE;

  rw_prv->zoom_level = DEFAULT_ZOOM_LEVEL;

  rw_prv->bg =
    GTK_DRAWING_AREA (gtk_drawing_area_new ());
  gtk_widget_set_visible (GTK_WIDGET (rw_prv->bg),
                          1);
  gtk_container_add (GTK_CONTAINER (self),
                     GTK_WIDGET (rw_prv->bg));
  gtk_widget_add_events (GTK_WIDGET (rw_prv->bg),
                         GDK_ALL_EVENTS_MASK);

  rw_prv->playhead =
    ruler_playhead_widget_new ();
  gtk_overlay_add_overlay (GTK_OVERLAY (self),
                           GTK_WIDGET (rw_prv->playhead));

  /* make it able to notify */
  gtk_widget_add_events (GTK_WIDGET (rw_prv->bg),
                         GDK_ALL_EVENTS_MASK);


  g_signal_connect (
    G_OBJECT (rw_prv->bg), "draw",
    G_CALLBACK (ruler_draw_cb), self);
  g_signal_connect (
    G_OBJECT (rw_prv->bg), "motion-notify-event",
    G_CALLBACK (on_motion),  self);
  g_signal_connect (
    G_OBJECT (self), "get-child-position",
    G_CALLBACK (get_child_position), NULL);
}

static void
ruler_widget_class_init (RulerWidgetClass * _klass)
{
  GtkWidgetClass * klass = GTK_WIDGET_CLASS (_klass);
  gtk_widget_class_set_css_name (klass,
                                 "ruler");
}
