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

  GstElement *rtmp_source1, *rtmp_source2, *output_converter, *output_sink;
  GstElement *encoder, *muxer;
  GstElement *midstream_converter, *mixer;
  PipelineAndLoop data;
  CustomData main_stream;
  CustomData inset_stream;
  GMainLoop *loop;

  gst_init(&argc, &argv);
  memset(&data, 0, sizeof(data));
  memset(&main_stream, 0, sizeof(main_stream));
  memset(&inset_stream, 0, sizeof(inset_stream));

  /* static things */
  capabilities = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, 200, \
				     "height", G_TYPE_INT, 150, NULL);
  rtmp_source1 = gst_element_factory_make("rtmpsrc", "source1");
  rtmp_source2 = gst_element_factory_make("rtmpsrc", "source2");
  midstream_converter = gst_element_factory_make("videoconvert", "midstream");
  mixer = gst_element_factory_make("videomixer", "mixer");
  output_converter = gst_element_factory_make("videoconvert", "output_converter");
  encoder = gst_element_factory_make("x264enc", "encoder");
  muxer = gst_element_factory_make("flvmux", "muxer");
  output_sink = gst_element_factory_make("rtmpsink", "output_sink");

  /*dynamic things */
  main_stream.source = gst_element_factory_make("decodebin", "main_decoder");
  main_stream.sink = gst_element_factory_make("videoconvert", "converter");

  inset_stream.source = gst_element_factory_make("decodebin", "inset_decoder");
  inset_stream.sink = gst_element_factory_make("videoscale", "scaler");

  data.pipeline = gst_pipeline_new("pip");
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "afterinit");

  if(!data.pipeline || !rtmp_source1 || !rtmp_source2 || !midstream_converter || !mixer || \
     !output_converter || !output_sink || !main_stream.source || !main_stream.sink || \
     !inset_stream.source || !inset_stream.sink || !encoder || !muxer) {
    g_printerr("Could not build an element\n");
    return -1;
  }

  gst_bin_add_many(GST_BIN(data.pipeline), rtmp_source1, rtmp_source2, midstream_converter, \
		   mixer, output_converter, output_sink, main_stream.source, main_stream.sink, \
		   inset_stream.source, inset_stream.sink, muxer, encoder, NULL);

  g_object_set(rtmp_source1, "location", "rtmp://192.168.1.124:1935/yanked/stream-counter", NULL);
  g_object_set(rtmp_source2, "location", "rtmp://192.168.1.124:1935/yanked/stream-meu", NULL);
  g_object_set(encoder, "bframes", 0, NULL);
  g_object_set(output_sink, "location", "rtmp://192.168.1.124:1935/yanked/stream-videotestsrc", NULL);

  if(!gst_element_link(rtmp_source1, main_stream.source)) {
    g_printerr("Could not link the main RTMP source to the decoder bin.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(rtmp_source2, inset_stream.source)) {
    g_printerr("Could not link the inset RTMP source to the decoder bin.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(inset_stream.sink, midstream_converter)) {
    g_printerr("Could not link the inset stream sink (scaler) to the mid-stream video converter.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(mixer, output_converter)) {
    g_printerr("Could not link the video mixer to the output video converter.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(output_converter, encoder)) {
    g_printerr("Could not link the output converter to the h264 encoder.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(encoder, muxer)) {
    g_printerr("Could not link the h264 encoder to the FLV muxer.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(muxer, output_sink)) {
    g_printerr("Could not link the FLV muxer to the RTMP sink.\n");
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

  GstPad *main_stream_video_pad = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "aftermainpadrequest");

  GstPad *inset_stream_video_pad = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);

  if(!gst_element_link(main_stream.sink, mixer)) {
    g_printerr("Could not link the main stream video converter to the mixer\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link_filtered(midstream_converter, mixer, capabilities)) {
    g_printerr("Could not link the midstream video converter to the mixer\n");
    gst_object_unref(data.pipeline);
    return -1;
  }
  g_object_set(inset_stream_video_pad, "xpos", 438, NULL);
  g_object_set(inset_stream_video_pad, "ypos", 210, NULL);
  /* g_object_set(inset_stream_video_pad, "alpha", 1.0, NULL); */
  g_object_set(inset_stream_video_pad, "zorder", 100, NULL);

  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "afterelementlink");

  g_signal_connect(main_stream.source, "pad-added", G_CALLBACK(pad_added_handler), &main_stream);
  g_signal_connect(inset_stream.source, "pad-added", G_CALLBACK(pad_added_handler), &inset_stream);

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
