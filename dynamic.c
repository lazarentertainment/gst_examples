#include <gst/gst.h>
#include <stdio.h>

//information struct for callbacks
typedef struct _CustomData {
  GstElement *pipeline;
  GstElement *source;
  GstElement *audio_converter;
  GstElement *audio_sink;
  GstElement *video_converter;
  GstElement *video_sink;
} CustomData;

static void pad_added_handler(GstElement *source, GstPad *pad, CustomData *data);

int main(int argc, char *argv[]) {
  CustomData data;
  GstBus *bus;
  GstMessage *message;
  GstStateChangeReturn return_value;
  gboolean terminate = FALSE;

  gst_init(&argc, &argv);

  data.source = gst_element_factory_make("uridecodebin", "source");
  data.audio_converter = gst_element_factory_make("audioconvert", "audio_converter");
  data.audio_sink = gst_element_factory_make("autoaudiosink", "audio_sink");
  data.video_converter = gst_element_factory_make("videoconvert", "video_converter");
  data.video_sink = gst_element_factory_make("ximagesink", "video_sink");

  data.pipeline = gst_pipeline_new("test-pipeline");
  if(!data.pipeline || !data.source || !data.audio_converter || !data.audio_sink ||
     !data.video_converter || !data.video_sink) {
    g_printerr("One or more elements couldn't be converted.\n");
    return -1;
  }

  gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.audio_converter, data.video_converter, data.audio_sink, data.video_sink, NULL);
  if(!gst_element_link(data.audio_converter, data.audio_sink)) {
    g_printerr("Could not link the audio converter to the audio sink.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  if(!gst_element_link(data.video_converter, data.video_sink)) {
    g_printerr("Could not link video converter to the video sink\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  g_object_set(data.source, "uri", "file:///home/vagrant/sintel_trailer-480p.webm", NULL);
  g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);
  return_value = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

  if(return_value == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  bus = gst_element_get_bus(data.pipeline);

  do {
    message = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
    if(message != NULL) {
      GError *error;
      gchar *debug_info;


      switch(GST_MESSAGE_TYPE(message)) {
      case GST_MESSAGE_ERROR:
	gst_message_parse_error(message, &error, &debug_info);
	g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(message->src), error->message);
	g_printerr("Debug info: %s\n", debug_info ? debug_info : "none");
	g_clear_error(&error);
	g_free(debug_info);
	terminate = TRUE;
	break;
      case GST_MESSAGE_EOS:
	g_print("EOS reached\n");
	terminate = TRUE;
	break;
      case GST_MESSAGE_STATE_CHANGED:
	if(GST_MESSAGE_SRC(message) == GST_OBJECT(data.pipeline)) {
	  GstState old_state, new_state, pending_state;
	  gst_message_parse_state_changed(message, &old_state, &new_state, &pending_state);
	  g_print("State changed from %s to %s\n", gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
	}
	  break;
	default:
	  g_printerr("Should not happen\n");
	  break;
      }
      gst_message_unref(message);
    }
  } while(!terminate);

  gst_object_unref(bus);
  gst_element_set_state(data.pipeline, GST_STATE_NULL);
  gst_object_unref(data.pipeline);
  return 0;
}

static void pad_added_handler(GstElement *source, GstPad *new_pad, CustomData *data) {
  GstPad *audio_sink_pad = gst_element_get_static_pad(data->audio_converter, "sink");
  GstPad *video_sink_pad = gst_element_get_static_pad(data->video_converter, "sink");
  GstPadLinkReturn return_value;
  GstCaps *new_pad_caps = NULL;
  GstStructure *new_pad_structure = NULL;
  const gchar *new_pad_type = NULL;
  g_print("Received new pad '%s' from '%s'\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(source));
  new_pad_caps = gst_pad_query_caps(new_pad, NULL);
  g_print("Getting the capabilities\n");
  new_pad_structure = gst_caps_get_structure(new_pad_caps, 0);
  g_print("Getting the structures of capabilities\n");
  new_pad_type = gst_structure_get_name(new_pad_structure);
  g_print("Getting the name: %s\n", new_pad_type);

  g_print("g str has a prefix '%s': %d\n", new_pad_type, g_str_has_prefix(new_pad_type, "video/x-raw"));
  if(g_str_has_prefix(new_pad_type, "audio/x-raw")) {
    if(gst_pad_is_linked(audio_sink_pad)) {
      g_print("Audio pad linked already\n");
      goto exit;
    }
    return_value = gst_pad_link(new_pad, audio_sink_pad);
  }
  else if(g_str_has_prefix(new_pad_type, "video/x-raw")) {
    g_print("Linking the video pad to the video converter sink\n");
    g_print("Is pad linked: %d\n", gst_pad_is_linked(video_sink_pad));
    if(gst_pad_is_linked(video_sink_pad)) {
      g_print("Video pad linked already\n");
      goto exit;
    }
    return_value = gst_pad_link(new_pad, video_sink_pad);
  }

  if(GST_PAD_LINK_FAILED(return_value)) {
    g_print("Type is '%s', but linking failed\n", new_pad_type);
  }
  else {
    g_print("Link succeeded with type '%s'.\n", new_pad_type);
  }

 exit:
  if(new_pad_caps != NULL) {
    gst_caps_unref(new_pad_caps);
  }
  gst_object_unref(audio_sink_pad);
  gst_object_unref(video_sink_pad);
}
