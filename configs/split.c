#include <gst/gst.h>
#include <string.h>

typedef struct _CustomData {
  GstElement *source;
  GstElement *sink;
} CustomData;

typedef struct _PipelineAndLoop {
  GstElement *pipeline;
  gboolean is_live;
  GMainLoop *loop;
} PipelineAndLoop;

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
  }

  if(GST_PAD_LINK_FAILED(retval)) {
    g_print("Type is '%s', but linking failed\n", new_pad_type);
  }
  else {
    g_print("Link succeeded with type '%s'.\n", new_pad_type);
  }

 exit:
  if(new_pad_caps != NULL) {
    gst_caps_unref(new_pad_caps);
  }
  gst_object_unref(sink_pad);
}

static void cb_message (GstBus *bus, GstMessage *msg, PipelineAndLoop *data) {
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
  GstElement *source1, *source2, *sink;
  GstElement *left_videobox, *right_videobox, *mixer;
  GstElement *output_converter;
  GstBus *bus;
  GstCaps *caps;
  GstStateChangeReturn return_value;
  CustomData left_stream;
  CustomData right_stream;
  PipelineAndLoop data;
  GMainLoop *loop;

  gst_init(&argc, &argv);

  memset(&left_stream, 0, sizeof(left_stream));
  memset(&right_stream, 0, sizeof(right_stream));
  memset(&data, 0, sizeof(data));

  //static things
  source1 = gst_element_factory_make("rtmpsrc", "source1");
  source2 = gst_element_factory_make("rtmpsrc", "source2");
  caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "AYUV", NULL);
  left_videobox = gst_element_factory_make("videobox", "left_videobox");
  right_videobox = gst_element_factory_make("videobox", "right_videobox");
  mixer = gst_element_factory_make("videomixer", "mixer");
  output_converter = gst_element_factory_make("videoconvert", "output_converter");
  sink = gst_element_factory_make("ximagesink", "sink");
  
  //dynamic things
  left_stream.source = gst_element_factory_make("decodebin", "left_decoder");
  left_stream.sink = gst_element_factory_make("videoconvert", "left_converter");

  right_stream.source = gst_element_factory_make("decodebin", "right_decoder");
  right_stream.sink = gst_element_factory_make("videoconvert", "right_converter");

  data.pipeline = gst_pipeline_new("split");
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "afterparse");

  if(!data.pipeline || !source1 || !source2 || !left_stream.sink || !left_videobox || \
     !left_stream.source || !right_stream.sink || !right_stream.source || !right_videobox || \
     !mixer || !output_converter || !sink) {
    g_printerr("Could not build an element\n");
    return -1;
  }

  gst_bin_add_many(GST_BIN(data.pipeline), source1, left_stream.sink, left_videobox,\
		   left_stream.source, source2, right_stream.sink, right_stream.source, \
		   right_videobox,  mixer, output_converter, sink, NULL);

  g_object_set(source1, "location", "rtmp://192.168.1.124:1935/yanked/stream-climax", NULL);
  g_object_set(source2, "location", "rtmp://192.168.1.124:1935/yanked/stream-meu", NULL);

  g_object_set(left_videobox, "left", 160, "right", 160, "top", 0, "bottom", 0, NULL);
  g_object_set(right_videobox, "left", 160, "right", 160, "top", 0, "bottom", 0, NULL);

  if(!gst_element_link(source1, left_stream.source)) {
    g_printerr("Could not link left source to its decoder bin.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link_filtered(left_stream.sink, left_videobox, caps)) {
    g_printerr("Could not link left converter to its video box using the capabilities.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(source2, right_stream.source)) {
    g_printerr("Could not link right source to its decoder bin.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link_filtered(right_stream.sink, right_videobox, caps)) {
    g_printerr("Could not link right converter to its video box using the capabilities.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(mixer, output_converter)) {
    g_printerr("Could not connect the mixer to the output converter.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(output_converter, sink)) {
    g_printerr("Could not link output converter to the sink.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  /* Manually linking the videoboxes to the mixer */
  GstPadTemplate *mixer_sink_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(mixer), "sink_%u");

  if(mixer_sink_pad_template == NULL) {
    g_printerr("Could not get mixer pad template.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  GstPad *left_stream_video_pad = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "afterleftpadrequest");

  GstPad *right_stream_video_pad = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);

  if(!gst_element_link(left_videobox, mixer)) {
    g_printerr("Could not link the left videobox to the mixer\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(right_videobox, mixer)) {
    g_printerr("Could not link the right videobox to the mixer\n");
    gst_object_unref(data.pipeline);
    return -1;
  }
  g_object_set(right_stream_video_pad, "xpos", 320, NULL);
  g_object_set(right_stream_video_pad, "ypos", 0, NULL);
  g_object_set(right_stream_video_pad, "alpha", 1.0, NULL);
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "afterelementlink");

  g_signal_connect(left_stream.source, "pad-added", G_CALLBACK(pad_added_handler), &left_stream);
  g_signal_connect(right_stream.source, "pad-added", G_CALLBACK(pad_added_handler), &right_stream);

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
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "aftermainlooprun");
  g_main_loop_unref(loop);
  gst_object_unref(bus);
  gst_element_set_state(data.pipeline, GST_STATE_NULL);
  gst_object_unref(data.pipeline);
  return 0;
}
