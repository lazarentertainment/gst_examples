#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gst/gst.h>

typedef struct _CustomData {
  GstElement *pipeline;
  GstElement *source;
  GstElement *sink;
  gboolean is_live;
  GMainLoop *loop;
} CustomData;

typedef void (*java_callback)(GMainLoop *);

static void pad_added_handler(GstElement *source, GstPad *pad, CustomData *data) {
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

static void cb_message(GstBus *bus, GstMessage *msg, CustomData *data) {
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

void videoTestSource(const char* rtmp_source, const char* rtmp_sink, java_callback stop_stream_cb) {
  GstElement *test_source, *sink,*encoder, *muxer;
  GstBus *bus;
  GstMessage *message;
  GstStateChangeReturn return_value;
  CustomData data;
  GMainLoop *loop;

  gst_init(NULL, NULL);
  memset(&data, 0, sizeof(data));

  test_source = gst_element_factory_make("rtmpsrc", "source");
  encoder = gst_element_factory_make("x264enc", "encoder");
  muxer = gst_element_factory_make("flvmux", "muxer");
  sink = gst_element_factory_make("rtmpsink", "sink");

  data.source = gst_element_factory_make("decodebin", "decoder");
  data.sink = gst_element_factory_make("videoconvert", "converter");
  data.pipeline = gst_pipeline_new("testsourcefromjavatortmp");

  g_object_set(encoder, "bframes", 0, NULL);
  g_object_set(sink, "location", rtmp_sink, NULL);
  g_object_set(test_source, "location", rtmp_source, NULL);

  if(!data.pipeline || !test_source || !sink || !encoder || !muxer || !data.source || !data.sink) {
    g_printerr("Could create any elements\n");
  }

  gst_bin_add_many(GST_BIN(data.pipeline), test_source, encoder, muxer, sink, data.source, data.sink, NULL);

  if(gst_element_link(test_source, data.source) != TRUE) {
    g_printerr("Could not connect the test source to the decoder.\n");
    gst_object_unref(data.pipeline);
  }

  if(gst_element_link(data.sink, encoder) != TRUE) {
    g_printerr("Could not connect the converter to the x264 encoder.\n");
    gst_object_unref(data.pipeline);
  }

  if(gst_element_link(encoder, muxer) != TRUE) {
    g_printerr("Could not connect the encoder to the FLV muxer.\n");
    gst_object_unref(data.pipeline);
  }

  if(gst_element_link(muxer, sink) != TRUE) {
    g_printerr("Could not link the converter to the ximage sink.\n");
    gst_object_unref(data.pipeline);
  }

  g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);
  bus = gst_element_get_bus(data.pipeline);
  return_value = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

  if(return_value == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to start playing the pipeline.\n");
    gst_object_unref(data.pipeline);
  }
  else if(return_value == GST_STATE_CHANGE_NO_PREROLL) {
    data.is_live = TRUE;
  }

  loop = g_main_loop_new(NULL, FALSE);
  data.loop = loop;

  gst_bus_add_signal_watch(bus);
  g_signal_connect(bus, "message", G_CALLBACK(cb_message), &data);

  stop_stream_cb(data.loop);

  g_main_loop_run(loop);
  printf("%s", "Main loop stopped running.\n");
  g_main_loop_unref(loop);
  gst_object_unref(bus);
  gst_element_set_state(data.pipeline, GST_STATE_READY);
  gst_element_set_state(data.pipeline, GST_STATE_NULL);
  gst_object_unref(data.pipeline);
  printf("%s", "Everything cleaned up.\n");
}

void stopStream(GMainLoop *loop) {
  g_main_loop_quit(loop);
}

const char* showGstVersion() {
  const gchar *nano_str;
  guint major, minor, micro, nano;
  gst_version(&major, &minor, &micro, &nano);

  if(nano == 1) {
    nano_str = "(CVS)";
  }
  else if(nano == 2) {
    nano_str = "(Prerelease)";
  }
  else {
    nano_str = "";
  }

  char* message = (char *)malloc(100);

  sprintf(message,\
	  "This program is linked against gstreamer version %d.%d.%d %s\n", \
	 major, minor, micro, nano_str);
  return message;
}

void parameterIn(const char *param) {
  printf("The passed in parameter is: '%s'\n", param);
}

const char* parameterOut(const char *param) {
  printf("The result of string compare is %d\n", strcmp(param, "This is a test"));
  if(strcmp(param, "This is a test") == 0) {
    return "Is it really?";
  }
  return "Nothing to see here";
}
