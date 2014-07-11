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

int print_error_and_exit(const gchar *message, PipelineAndLoop *data) {
  g_printerr(message);
  gst_object_unref(data->pipeline);
  return -1;
}

int main(int argc, char *argv[]) {
  GstCaps *main_stream_caps, *judge_stream_caps;
  GstBus *bus;
  GstStateChangeReturn return_value;
  GMainLoop *loop;
  CustomData main_source_sink;
  CustomData judge1_source_sink;
  CustomData judge2_source_sink;
  CustomData judge3_source_sink;
  PipelineAndLoop data;

  GstElement *main_stream, *main_stream_videobox;
  GstElement *judge1_stream, *judge1_converter;
  GstElement *judge2_stream, *judge2_converter;
  GstElement *judge3_stream, *judge3_converter;
  GstElement *mixer, *output_converter, *output_sink;


  gst_init(&argc, &argv);
  memset(&data, 0, sizeof(data));
  memset(&main_source_sink, 0, sizeof(main_source_sink));
  memset(&judge2_source_sink, 0, sizeof(judge1_source_sink));
  memset(&judge2_source_sink, 0, sizeof(judge2_source_sink));
  memset(&judge3_source_sink, 0, sizeof(judge3_source_sink));

  main_stream_caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "AYUV", NULL);
  judge_stream_caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "AYUV", \
					  "width", G_TYPE_INT, 213, "height", G_TYPE_INT, 120, NULL);

  main_stream = gst_element_factory_make("rtmpsrc", "main_stream");
  main_stream_videobox = gst_element_factory_make("videobox", "main_stream_videobox");
  main_source_sink.source = gst_element_factory_make("decodebin", "main_stream_source");
  main_source_sink.sink = gst_element_factory_make("videoconvert", "main_stream_sink");

  judge1_stream = gst_element_factory_make("rtmpsrc", "judge1_stream");
  judge1_converter = gst_element_factory_make("videoconvert", "judge1_converter");
  judge1_source_sink.source = gst_element_factory_make("decodebin", "judge1_decoder");
  judge1_source_sink.sink = gst_element_factory_make("videoscale", "judge1_scaler");

  judge2_stream = gst_element_factory_make("rtmpsrc", "judge2_stream");
  judge2_converter = gst_element_factory_make("videoconvert", "judge2_converter");
  judge2_source_sink.source = gst_element_factory_make("decodebin", "judge2_decoder");
  judge2_source_sink.sink = gst_element_factory_make("videoscale", "judge2_scaler");

  judge3_stream = gst_element_factory_make("rtmpsrc", "judge3_stream");
  judge3_converter = gst_element_factory_make("videoconvert", "judge3_converter");
  judge3_source_sink.source = gst_element_factory_make("decodebin", "judge3_decoder");
  judge3_source_sink.sink = gst_element_factory_make("videoscale", "judge3_scaler");


  mixer = gst_element_factory_make("videomixer", "mixer");
  output_converter = gst_element_factory_make("videoconvert", "output_converter");
  output_sink = gst_element_factory_make("ximagesink", "output_sink");

  data.pipeline = gst_pipeline_new("quad");

  if(!data.pipeline || !main_stream || !main_stream_videobox || !main_source_sink.source \
     || !main_source_sink.sink || !mixer || !output_converter || !output_sink \
     || !judge1_stream || !judge1_converter || !judge1_source_sink.source || !judge1_source_sink.sink \
     || !judge2_stream || !judge2_converter || !judge2_source_sink.source || !judge2_source_sink.sink \
     || !judge3_stream || !judge3_converter || !judge3_source_sink.source || !judge3_source_sink.sink) {
    g_printerr("Could not make an element.\n");
    return -1;
  }

  gst_bin_add_many(GST_BIN(data.pipeline), main_stream, main_stream_videobox, main_source_sink.source, \
		   main_source_sink.sink, mixer, output_converter, output_sink, \
		   judge1_stream, judge1_converter, judge1_source_sink.source, judge1_source_sink.sink,\
		   judge2_stream, judge2_converter, judge2_source_sink.source, judge2_source_sink.sink,\
		   judge3_stream, judge3_converter, judge3_source_sink.source, judge3_source_sink.sink, NULL);

  g_object_set(main_stream, "location", "rtmp://192.168.1.124:1935/yanked/stream-climax", NULL);
  g_object_set(judge1_stream, "location", "rtmp://192.168.1.124:1935/yanked/stream-meu", NULL);
  g_object_set(judge2_stream, "location", "rtmp://192.168.1.124:1935/yanked/stream-kisser", NULL);
  g_object_set(judge3_stream, "location", "rtmp://192.168.1.124:1935/yanked/stream-countdown", NULL);

  g_object_set(main_stream_videobox, "left", 160, "right", 160, "top", 0, "bottom", 0, NULL);


  if(!gst_element_link(mixer, output_converter)) {
    return print_error_and_exit("Could not link the video mixer to the output converter.\n", &data);
  }

  if(!gst_element_link(output_converter, output_sink)) {
    return print_error_and_exit("Could not link the output converter to the output sink.\n", &data);
  }

  if(!gst_element_link(main_stream, main_source_sink.source)) {
    return print_error_and_exit("Could not link the RTMP source to its decoder bin.\n", &data);
  }

  if(!gst_element_link_filtered(main_source_sink.sink, main_stream_videobox, main_stream_caps)) {
    g_printerr("Could not link the main stream video converter to the videobox.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(judge1_stream, judge1_source_sink.source)) {
    return print_error_and_exit("Could not link judge1 RTMP source to its decoder bin.\n", &data);
  }

  if(!gst_element_link(judge1_source_sink.sink, judge1_converter)) {
    return print_error_and_exit("Could not link judge1 scaler to its converter.\n", &data);
  }

  if(!gst_element_link(judge2_stream, judge2_source_sink.source)) {
    return print_error_and_exit("Could not link judge2 RTMP source to its decoder bin.\n", &data);
  }

  if(!gst_element_link(judge2_source_sink.sink, judge2_converter)) {
    return print_error_and_exit("Could not link judge2 scaler to its converter.\n", &data);
  }

  if(!gst_element_link(judge3_stream, judge3_source_sink.source)) {
    return print_error_and_exit("Could not link judge3 RTMP source to its decoder bin.\n", &data);
  }

  if(!gst_element_link(judge3_source_sink.sink, judge3_converter)) {
    return print_error_and_exit("Could not link judge3 scaler to its converter.\n", &data);
  }


  /* Manually linking the videoboxes to the mixer */
  GstPadTemplate *mixer_sink_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(mixer), "sink_%u");

  if(mixer_sink_pad_template == NULL) {
    return print_error_and_exit("Could not get mixer pad template.\n", &data);
  }

  GstPad *main_stream_video_pad = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "aftermainpadrequest");

  if(!gst_element_link(main_stream_videobox, mixer)) {
    return print_error_and_exit("Could not link the main stream videobox to the mixer.\n", &data);
  }

  GstPad *judge1_stream_video_pad = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "afterjudge1padrequest");

  if(!gst_element_link_filtered(judge1_converter, mixer, judge_stream_caps)) {
    return print_error_and_exit("Could not link the judge1 converter to the mixer.\n", &data);
  }

  GstPad *judge2_stream_video_pad = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "afterjudge2padrequest");

  if(!gst_element_link_filtered(judge2_converter, mixer, judge_stream_caps)) {
    return print_error_and_exit("Could not link the judge2 converter to the mixer.\n", &data);
  }

  GstPad *judge3_stream_video_pad = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "afterjudge3padrequest");

  if(!gst_element_link_filtered(judge3_converter, mixer, judge_stream_caps)) {
    return print_error_and_exit("Could not link the judge3 converter to the mixer.\n", &data);
  }

  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "afterelementlink");

  /* Position the judges */
  g_object_set(judge1_stream_video_pad, "xpos", 320, "ypos", 0, NULL);
  g_object_set(judge2_stream_video_pad, "xpos", 320, "ypos", 120, NULL);
  g_object_set(judge3_stream_video_pad, "xpos", 320, "ypos", 240, NULL);

  g_signal_connect(main_source_sink.source, "pad-added", G_CALLBACK(pad_added_handler),\
		   &main_source_sink);
  g_signal_connect(judge1_source_sink.source, "pad-added", G_CALLBACK(pad_added_handler), \
		   &judge1_source_sink);
  g_signal_connect(judge2_source_sink.source, "pad-added", G_CALLBACK(pad_added_handler), \
		   &judge2_source_sink);
  g_signal_connect(judge3_source_sink.source, "pad-added", G_CALLBACK(pad_added_handler), \
		   &judge3_source_sink);

  bus = gst_element_get_bus(data.pipeline);
  return_value = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

  if(return_value == GST_STATE_CHANGE_FAILURE) {
    return print_error_and_exit("Unable to set the pipeline to the playing state.\n", &data);
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
