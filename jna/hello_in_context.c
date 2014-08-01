#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gst/gst.h>

typedef struct _Errors {
  /* char message[255]; */
  const char *message;
  int status_code;
} Errors;

typedef struct _GstContext {
  GstElement *pipeline;
  GstElement *source;
  GstElement *sink;
  gboolean is_live;
  GMainLoop *loop;
} GstContext;


static void cb_message(GstBus *bus, GstMessage *msg, GstContext *data) {
  switch (GST_MESSAGE_TYPE(msg)) {
  case GST_MESSAGE_ERROR: {
    GError *err;
    gchar *debug;
 
    gst_message_parse_error(msg, &err, &debug);
    g_print ("Error: %s\n", err->message);
    g_error_free (err);
    g_free (debug);

    gst_element_set_state(data->pipeline, GST_STATE_READY);
    g_main_loop_quit (data->loop);
    break;
  }
  case GST_MESSAGE_EOS:
    /* end-of-stream */
    gst_element_set_state (data->pipeline, GST_STATE_READY);
    g_main_loop_quit (data->loop);
    break;
  case GST_MESSAGE_BUFFERING: {
    gint percent = 0;

    /* If the stream is live, we do not care about buffering. */
    if (data->is_live) break;

    gst_message_parse_buffering (msg, &percent);
    g_print ("Buffering (%3d%%)\r", percent);
    /* Wait until buffering is complete before start/resume playing */
    if (percent < 100)
      gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
    else
      gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
    break;
  }
  case GST_MESSAGE_CLOCK_LOST:
    /* Get a new clock */
    gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
    gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
    break;
  case GST_MESSAGE_STATE_CHANGED:
    GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data->pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "playing");
    break;
  default:
    /* Unhandled message */
    break;
  }
}

static void pad_added_handler(GstElement *source, GstPad *pad, GstContext *data) {
  GstPad *sink_pad = gst_element_get_static_pad(data->sink, "sink");
  GstPadLinkReturn retval;
  GstCaps *new_pad_caps = NULL;
  GstStructure *new_structure = NULL;
  const gchar *new_pad_type = NULL;
  g_print("Received new pad '%s' from '%s'\n", GST_PAD_NAME(pad), GST_ELEMENT_NAME(source));
  new_pad_caps = gst_pad_query_caps(pad, NULL);
  g_print("Getting the capabilities\n");
  new_structure = gst_caps_get_structure(new_pad_caps, 0);
  g_print("Getting the structures of capabilities\n");
  new_pad_type = gst_structure_get_name(new_structure);
  g_print("Getting the name: %s\n", new_pad_type);

  g_print("g str has a prefix '%s': %d\n", new_pad_type, g_str_has_prefix(new_pad_type, "video/x-raw"));
  if(g_str_has_prefix(new_pad_type, "video/x-raw")) {
    g_print("Linking the video pad to the video converter sink\n");
    g_print("Is pad linked: %d\n", gst_pad_is_linked(sink_pad));
    if(gst_pad_is_linked(sink_pad)) {
      g_print("Video pad linked already\n");
      goto exit;
    }
    retval = gst_pad_link(pad, sink_pad);

    if(GST_PAD_LINK_FAILED(retval)) {
      g_print("Type is '%s', but linking failed\n", new_pad_type);
    }
    else {
      g_print("Link succeeded with type '%s'.\n", new_pad_type);
    }
  }

 exit:
  if(new_pad_caps != NULL) {
    gst_caps_unref(new_pad_caps);
  }
  gst_object_unref(sink_pad);
}

GstContext* allocate() {
  GstContext* context;
  gst_init(NULL, NULL);
  context = (GstContext *)malloc(sizeof(GstContext));
  printf("%s", "Set up the struct and returning the pointer.\n");
  return context;
}

Errors setup(const char* rtmp_source, const char* rtmp_sink, GstContext *context) {
  GstElement *source, *sink, *encoder, *muxer;
  Errors error_structure;

  source = gst_element_factory_make("rtmpsrc", "source");
  sink = gst_element_factory_make("rtmpsink", "sink");
  encoder = gst_element_factory_make("x264enc", "encoder");
  muxer = gst_element_factory_make("flvmux", "muxer");


  context->source = gst_element_factory_make("decodebin", "decoder");
  context->sink = gst_element_factory_make("videoconvert", "converter");
  context->pipeline = gst_pipeline_new("contextdriven");

  if(!context->pipeline || !source || !sink || !encoder || !muxer || !context->source || !context->sink) {
    error_structure.message = "Could not create elements.";
    error_structure.status_code = 500;
    return error_structure;
  }

  gst_bin_add_many(GST_BIN(context->pipeline), source, sink, encoder, muxer,\
		   context->source, context->sink, NULL);

  g_object_set(source, "location", rtmp_source, NULL);
  g_object_set(sink, "location", rtmp_sink, NULL);
  g_object_set(encoder, "bframes", 0, NULL);

  if(!gst_element_link(source, context->source)) {
    error_structure.message = "Could not link RTMP source to the decoder.";
    error_structure.status_code = 500;
    gst_object_unref(context->pipeline);
    return error_structure;
  }

  if(!gst_element_link(context->sink, encoder)) {
    error_structure.message = "Could not link the converter to the h264 encoder.";
    error_structure.status_code = 500;
    gst_object_unref(context->pipeline);
    return error_structure;
  }

  if(!gst_element_link(encoder, muxer)) {
    error_structure.message = "Could not link the h264 encoder to the FLV muxer.";
    error_structure.status_code = 500;
    gst_object_unref(context->pipeline);
    return error_structure;
  }

  if(!gst_element_link(muxer, sink)) {
    error_structure.message ="Could not link the FLV muxer to the RTMP sink.";
    error_structure.status_code = 500;
    gst_object_unref(context->pipeline);
    return error_structure;
  }

  g_signal_connect(context->source, "pad-added", G_CALLBACK(pad_added_handler), context);
  error_structure.status_code = 200;
  error_structure.message = "Success";
  printf("%s", "Returning successfully...\n");
  return error_structure;
}

void start(GstContext *context) {
  GstStateChangeReturn return_value;
  GstBus *bus;
  GstMessage *message;

  bus = gst_element_get_bus(context->pipeline);
  return_value = gst_element_set_state(context->pipeline, GST_STATE_PLAYING);

  if(return_value == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to start playing the pipeline.\n");
    gst_object_unref(context->pipeline);
  }
  else if(return_value == GST_STATE_CHANGE_NO_PREROLL) {
    context->is_live = TRUE;
  }

  context->loop = g_main_loop_new(NULL, FALSE);

  gst_bus_add_signal_watch(bus);
  g_signal_connect(bus, "message", G_CALLBACK(cb_message), context);
  g_main_loop_run(context->loop);
  g_main_loop_unref(context->loop);
  gst_object_unref(bus);
  gst_element_set_state(context->pipeline, GST_STATE_NULL);
  gst_object_unref(context->pipeline);
  free(context);

}

void stop(GstContext *context) {
  printf("%s", "Stopping...\n");
  g_main_loop_quit(context->loop);
}
