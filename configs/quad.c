#include <gst/gst.h>
#include <string.h>

typedef struct _CustomData {
  GstElement *sink;
  GstElement *source;
} CustomData;

typedef struct _PipelineAndLoop {
  GstElement *pipeline;
  gboolean is_live;
  GMainLoop *loop;
} PipelineAndLoop;

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

int main(int argc, char *argv[]) {
  GstCaps *capabilities;
  GstBus *bus;
  GstStateChangeReturn return_value;
  GMainLoop *loop;

  GstElement *rtmp_source1, *rtmp_source2, *rtmp_source3, *rtmp_source4;
  GstElement *output_converter, *output_sink;
  GstElement *stream1_converter, *stream2_converter, *stream3_converter, *stream4_converter, *mixer;
  GstElement *stream1_scaler, *stream2_scaler, *stream3_scaler, *stream4_scaler;
  PipelineAndLoop data;
  CustomData top_left;
  CustomData top_right;
  CustomData bottom_left;
  CustomData bottom_right;

  gst_init(&argc, &argv);
  memset(&data, 0, sizeof(data));
  memset(&top_left, 0, sizeof(top_left));
  memset(&top_right, 0, sizeof(top_right));
  memset(&bottom_left, 0, sizeof(bottom_left));
  memset(&bottom_right, 0, sizeof(bottom_right));

  /* static things */
  capabilities = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "AYUV", \
				     "width", G_TYPE_INT, 320, "height", G_TYPE_INT, 180, NULL);

  rtmp_source1 = gst_element_factory_make("rtmpsrc", "source1");
  rtmp_source2 = gst_element_factory_make("rtmpsrc", "source2");
  rtmp_source3 = gst_element_factory_make("rtmpsrc", "source3");
  rtmp_source4 = gst_element_factory_make("rtmpsrc", "source4");

  stream1_converter = gst_element_factory_make("videoconvert", "stream1_converter");
  stream2_converter = gst_element_factory_make("videoconvert", "stream2_converter");
  stream3_converter = gst_element_factory_make("videoconvert", "stream3_converter");
  stream4_converter = gst_element_factory_make("videoconvert", "stream4_converter");

  mixer = gst_element_factory_make("videomixer", "mixer");
  output_converter = gst_element_factory_make("videoconvert", "output_converter");
  output_sink = gst_element_factory_make("ximagesink", "output_sink");

  /* dynamic things */
  top_left.source = gst_element_factory_make("decodebin", "tl_decoder");
  top_left.sink = gst_element_factory_make("videoscale", "stream1_scaler");

  top_right.source = gst_element_factory_make("decodebin", "tr_decoder");
  top_right.sink = gst_element_factory_make("videoscale", "stream2_scaler");

  bottom_left.source = gst_element_factory_make("decodebin", "bl_decoder");
  bottom_left.sink = gst_element_factory_make("videoscale", "stream3_scaler");

  bottom_right.source = gst_element_factory_make("decodebin", "br_decoder");
  bottom_right.sink = gst_element_factory_make("videoscale", "stream4_scaler");

  data.pipeline = gst_pipeline_new("quad");
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "afterinit");

  if(!data.pipeline || !rtmp_source1 || !rtmp_source2 || !rtmp_source3 || !rtmp_source4 || \
     !stream1_converter || !stream2_converter || !stream3_converter || !stream4_converter || \
     !mixer || !output_converter || !output_sink || !top_left.source || !top_left.sink || \
     !top_right.source || !top_right.sink || !bottom_left.source || !bottom_left.sink || \
     !bottom_right.source || !bottom_right.sink) {
    g_printerr("Could not build an element\n");
    return -1;
  }

  gst_bin_add_many(GST_BIN(data.pipeline), rtmp_source1, rtmp_source2, rtmp_source3, rtmp_source4, \
		   stream1_converter, stream2_converter, stream3_converter, stream4_converter, \
		   mixer, output_converter, output_sink, top_left.source, top_left.sink, \
		   top_right.source, top_right.sink, bottom_left.source, bottom_left.sink, \
		   bottom_right.source, bottom_right.sink, NULL);

  g_object_set(rtmp_source1, "location", "rtmp://192.168.1.124:1935/yanked/stream-climax", NULL);
  g_object_set(rtmp_source2, "location", "rtmp://192.168.1.124:1935/yanked/stream-countdown", NULL);
  g_object_set(rtmp_source3, "location", "rtmp://192.168.1.124:1935/yanked/stream-meu", NULL);
  g_object_set(rtmp_source4, "location", "rtmp://192.168.1.124:1935/yanked/stream-kisser", NULL);

  if(!gst_element_link(rtmp_source1, top_left.source)) {
    g_printerr("Could not link the top left RTMP source to its decoder bin.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(top_left.sink, stream1_converter)) {
    g_printerr("Could not link top left scaler to its converter.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(rtmp_source2, top_right.source)) {
    g_printerr("Could not link the top right RTMP source to its decoder bin.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(top_right.sink, stream2_converter)) {
    g_printerr("Could not link top right scaler to its converter.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(rtmp_source3, bottom_left.source)) {
    g_printerr("Could not link the bottom left RTMP source to its decoder bin.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(bottom_left.sink, stream3_converter)) {
    g_printerr("Could not link bottom left scaler to its converter.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(rtmp_source4, bottom_right.source)) {
    g_printerr("Could not link the bottom right RTMP source to its decoder bin.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(bottom_right.sink, stream4_converter)) {
    g_printerr("Could not link bottom right scaler to its converter.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }


  if(!gst_element_link(mixer, output_converter)) {
    g_printerr("Could not link the video mixer to the output video converter.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(output_converter, output_sink)) {
    g_printerr("Could not link the output converter to the output sink.\n");
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

  GstPad *top_left_stream_video_pad = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "aftertopleftpadrequest");

  if(!gst_element_link_filtered(stream1_converter, mixer, capabilities)) {
    g_printerr("Could not link the top left video scaler to the mixer\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  GstPad *top_right_stream_video_pad = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "aftertoprightpadrequest");

  if(!gst_element_link_filtered(stream2_converter, mixer, capabilities)) {
    g_printerr("Could not link the top right video scaler to the mixer\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  GstPad *bottom_left_stream_video_pad = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "afterbottomleftpadrequest");

  if(!gst_element_link_filtered(stream3_converter, mixer, capabilities)) {
    g_printerr("Could not link the bottom left video scaler to the mixer\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  GstPad *bottom_right_stream_video_pad = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "afterbottomrightpadrequest");

  if(!gst_element_link_filtered(stream4_converter, mixer, capabilities)) {
    g_printerr("Could not link the bottom right video scaler to the mixer\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  g_object_set(top_left_stream_video_pad, "xpos", 0, "ypos", 0, NULL);
  g_object_set(top_right_stream_video_pad, "xpos", 320, "ypos", 0, NULL);
  g_object_set(bottom_left_stream_video_pad, "xpos", 0, "ypos", 180, NULL);
  g_object_set(bottom_right_stream_video_pad, "xpos", 320, "ypos", 180, NULL);

  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "afterelementlink");

  g_signal_connect(top_left.source, "pad-added", G_CALLBACK(pad_added_handler), &top_left);
  g_signal_connect(top_right.source, "pad-added", G_CALLBACK(pad_added_handler), &top_right);
  g_signal_connect(bottom_left.source, "pad-added", G_CALLBACK(pad_added_handler), &bottom_left);
  g_signal_connect(bottom_right.source, "pad-added", G_CALLBACK(pad_added_handler), &bottom_right);

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
