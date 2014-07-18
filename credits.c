/*
 * text-color-example.c
 *
 * Builds a pipeline with [videotestsrc ! textoverlay ! ximagesink] and
 * modulates color, text and text pos.
 *
 * Needs gst-plugin-base installed.
 */

#include <gst/gst.h>
#include <gst/controller/gstinterpolationcontrolsource.h>
#include <gst/controller/gstlfocontrolsource.h>
#include <gst/controller/gstargbcontrolbinding.h>
#include <gst/controller/gstdirectcontrolbinding.h>

gchar *credit_pages[20];

gint trans_duration = 1;
gint hold_duration = 3;

typedef struct{
	gchar **pages;
	gint current;
	gint total;
	GstElement *text;
} CreditPagesContext;

gboolean nextPage(GstClock *clock, GstClockTime time, GstClockID id, gpointer user_data) {
	CreditPagesContext* ctx = (CreditPagesContext *)user_data; 
	
  g_object_set (ctx->text,
  		"text", ctx->pages[ctx->current],
  		NULL);
  ctx->current += 1;
  if (ctx->current >= ctx->total) {
  	gst_clock_id_unschedule(id);
	}
  return 1; //ctx->current < ctx->total;
}

gint
main (gint argc, gchar ** argv)
{
  gint res = 1;
  GstElement *src, *text, *sink;
  GstElement *bin;
  GstControlSource *cs;
  GstControlSource *cs_a;
  GstClock *clock;
  GstClockID clock_id, periodic_id;
  GstClockReturn wait_ret;

	/* initialize the pages*/
	credit_pages[0] = "Siva's Power Hour";
	credit_pages[1] = "Produced by\nJJ";
	credit_pages[2] = "Directed by\nMP";
	credit_pages[3] = "Something by\nDL";
	credit_pages[4] = "Starring \nSiva";

  gst_init (&argc, &argv);

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  clock = gst_pipeline_get_clock (GST_PIPELINE (bin));
  src = gst_element_factory_make ("videotestsrc", NULL);
  if (!src) {
    GST_WARNING ("need videotestsrc from gst-plugins-base");
    goto Error;
  }
  g_object_set (src, "pattern", /* checkers-8 */ 2,
      NULL);
  text = gst_element_factory_make ("textoverlay", NULL);
  if (!text) {
    GST_WARNING ("need textoverlay from gst-plugins-base");
    goto Error;
  }
  g_object_set (text,
      "text", "Siva's Power Hour",
      "font-desc", "Sans, 30", "halignment", /* position */ 1,
      "valignment", /* position */ 3,
      "color", 0xffffffff,
      NULL);
  sink = gst_element_factory_make ("ximagesink", NULL);
  if (!sink) {
    GST_WARNING ("need ximagesink from gst-plugins-base");
    goto Error;
  }

  gst_bin_add_many (GST_BIN (bin), src, text, sink, NULL);
  if (!gst_element_link_many (src, text, sink, NULL)) {
    GST_WARNING ("can't link elements");
    goto Error;
  }

  /* setup control sources */
  gint length= sizeof(credit_pages)/sizeof(gchar*);
  cs = gst_interpolation_control_source_new ();
  cs_a = gst_interpolation_control_source_new ();
  periodic_id = gst_clock_new_periodic_id(clock,
  		gst_clock_get_time (clock),
  		(trans_duration * 2 + hold_duration) * GST_SECOND);
  CreditPagesContext ctx = {credit_pages, 0, length, text};

  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_NONE, NULL);
  g_object_set (cs_a, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  gst_object_add_control_binding (GST_OBJECT_CAST (text), gst_argb_control_binding_new (GST_OBJECT_CAST (text), "color", cs_a, cs, cs, cs));

  GstTimedValueControlSource *tvcs = (GstTimedValueControlSource *)cs;
	GstTimedValueControlSource *tvcsa = (GstTimedValueControlSource *)cs_a;
  
  gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 1.0);
 	gint i;
  gint time = 0;
  for (i=0; i < length; i++) {
		gst_timed_value_control_source_set (tvcsa, time * GST_SECOND, 0.0);
		time += trans_duration;
		gst_timed_value_control_source_set (tvcsa, time * GST_SECOND, 1.0);
		time += hold_duration;
		gst_timed_value_control_source_set (tvcsa, time * GST_SECOND, 1.0);
		time += trans_duration;
	}
	gst_object_unref (cs);
	gst_object_unref (cs_a);

  /* run for 10 seconds */
  clock_id =
      gst_clock_new_single_shot_id (clock,
      gst_clock_get_time (clock) + (60 * GST_SECOND));

  gst_clock_id_wait_async(periodic_id, nextPage, &ctx, NULL);
  
  if (gst_element_set_state (bin, GST_STATE_PLAYING)) {
    if ((wait_ret = gst_clock_id_wait (clock_id, NULL)) != GST_CLOCK_OK) {
      GST_WARNING ("clock_id_wait returned: %d", wait_ret);
    }
    gst_element_set_state (bin, GST_STATE_NULL);
  }

  /* cleanup */
  gst_clock_id_unref (clock_id);
  gst_object_unref (G_OBJECT (clock));
  gst_object_unref (G_OBJECT (bin));
  res = 0;
Error:
  return (res);
}
