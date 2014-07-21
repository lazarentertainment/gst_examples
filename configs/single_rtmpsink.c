#include <gst/gst.h>
#include <string.h>

typedef struct _CustomData {
  GstElement *pipeline;
  GstElement *source;
  GstElement *sink;
  gboolean is_live;
  GMainLoop *loop;
} CustomData;

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

static void cb_message (GstBus *bus, GstMessage *msg, CustomData *data) {
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

int main(int argc, char *argv[]) {
  GstElement *source1;
  GstElement *converter2, *encoder, *muxer, *sink;
  GstBus *bus;
  GstCaps *caps;
  GstStateChangeReturn return_value;
  CustomData data;
  GMainLoop *loop;

  gst_init(&argc, &argv);

  memset(&data, 0, sizeof(data));
  source1 = gst_element_factory_make("rtmpsrc", "source1");
  converter2 = gst_element_factory_make("videoconvert", "converter2");
  encoder = gst_element_factory_make("x264enc", "encoder");
  muxer = gst_element_factory_make("flvmux", "muxer");
  sink = gst_element_factory_make("rtmpsink", "sink");
  caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "AYUV", NULL);
  
  data.source = gst_element_factory_make("decodebin", "decoder1");
  data.sink = gst_element_factory_make("videoconvert", "converter1");
  data.pipeline = gst_pipeline_new("single");
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "afterparse");

  if(!data.pipeline || !source1 || !data.sink || !converter2 || !data.source || !encoder || !muxer || !sink) {
    g_printerr("Could not build an element\n");
    return -1;
  }

  gst_bin_add_many(GST_BIN(data.pipeline), source1, data.sink, converter2, data.source, encoder, muxer, sink, NULL);
  g_object_set(source1, "location", "rtmp://192.168.1.124:1935/yanked/rabbit", NULL);

  if(!gst_element_link(source1, data.source)) {
    g_printerr("Could not link source to the decode bin.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link_filtered(data.sink, converter2, caps)) {
    g_printerr("Could not link converter 1 to converter 2 using the capabilities.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(converter2, encoder)) {
    g_printerr("Could not link converter 2 to the x264 encoder.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(encoder, muxer)) {
    g_printerr("Could not link encoder to the FLV muxer.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(muxer, sink)) {
    g_printerr("Could not link FLV muxer to the RTMP sink.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  g_object_set(encoder, "bframes", 0, NULL);
  g_object_set(muxer, "streamable", TRUE, NULL);
  g_object_set(sink, "location", "rtmp://192.168.1.124:1935/yanked/rabbit-echo", NULL);

  g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);
  bus = gst_element_get_bus(data.pipeline);
  return_value = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

  if(return_value == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }
  else if(return_value == GST_STATE_CHANGE_NO_PREROLL) {
    data.is_live = TRUE;
  }

  loop = g_main_loop_new(NULL, FALSE);
  data.loop = loop;

  gst_bus_add_signal_watch(bus);
  g_signal_connect(bus, "message", G_CALLBACK(cb_message), &data);

  g_main_loop_run(loop);
  g_main_loop_unref(loop);
  gst_object_unref(bus);
  gst_element_set_state(data.pipeline, GST_STATE_NULL);
  gst_object_unref(data.pipeline);
  return 0;
}
