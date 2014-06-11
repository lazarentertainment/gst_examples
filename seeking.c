#include <gst/gst.h>

typedef struct _CustomData {
  GstElement *playbin;
  gboolean playing;
  gboolean terminate;
  gboolean seek_enabled;
  gboolean seek_done;
  gint64 duration;
} CustomData;

static void handle_message(CustomData *data, GstMessage *message);

int main(int argc, char *argv[]) {
  CustomData data;
  GstBus *bus;
  GstMessage *message;
  GstStateChangeReturn return_value;

  data.playing = FALSE;
  data.terminate = FALSE;
  data.seek_enabled = FALSE;
  data.seek_done = FALSE;
  data.duration = GST_CLOCK_TIME_NONE;

  gst_init(&argc, &argv);

  data.playbin = gst_element_factory_make("playbin", "playbin");
  GstElement *video_sink = gst_element_factory_make("ximagesink", "videosink");
  if(!data.playbin) {
    g_printerr("Couldn't create the playbin with all the elements needed\n");
    return -1;
  }

  g_object_set(data.playbin, "uri", "file:///code/ra.mp4", NULL);
  g_object_set(data.playbin, "video-sink", video_sink, NULL);

  return_value = gst_element_set_state(data.playbin, GST_STATE_PLAYING);
  if(return_value == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to playing state.\n");
    gst_object_unref(data.playbin);
    return -1;
  }

  bus = gst_element_get_bus(data.playbin);
  do {
    message = gst_bus_timed_pop_filtered(bus, 100 * GST_MSECOND, GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_DURATION);
    if(message != NULL) {
      handle_message(&data, message);
    }
    else {
      if(data.playing) {
	GstFormat fmt =  GST_FORMAT_TIME;
	gint64 current = -1;
	if(!gst_element_query_position(data.playbin, fmt, &current)) {
	  g_printerr("Could not query position.\n");
	}

	if(!GST_CLOCK_TIME_IS_VALID(data.duration)) {
	  if(!gst_element_query_duration(data.playbin, fmt, &data.duration)) {
	    g_printerr("Could not query duration.\n");
	  }
	}
	g_print("Position %" GST_TIME_FORMAT " of %" GST_TIME_FORMAT "\r", GST_TIME_ARGS(current), GST_TIME_ARGS(data.duration));
	if(data.seek_enabled && !data.seek_done && current > 10 * GST_SECOND) {
	  g_print("\nReached 10s, performing seek...\n");
	  gst_element_seek_simple(data.playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, 0 * GST_SECOND);
	  data.seek_done = TRUE;
	}
      }
    }
  } while(!data.terminate);

  gst_object_unref(bus);
  gst_element_set_state(data.playbin, GST_STATE_NULL);
  gst_object_unref(data.playbin);
  return 0;
}

static void handle_message(CustomData *data, GstMessage *message) {
  GError *error;
  gchar *debug_info;

  switch(GST_MESSAGE_TYPE(message)) {
  case GST_MESSAGE_ERROR:
    gst_message_parse_error(message, &error, &debug_info);
    g_printerr("Error received from element '%s': %s\n", GST_OBJECT_NAME(message->src), error->message);
    g_printerr("Debug info: %s\n", debug_info ? debug_info : "none");
    g_clear_error(&error);
    g_free(debug_info);
    data->terminate = TRUE;
    break;
  case GST_MESSAGE_EOS:
    g_print("EOS reached.\n");
    data->terminate = TRUE;
    break;
  case GST_MESSAGE_DURATION:
    data->duration = GST_CLOCK_TIME_NONE;
    break;
  case GST_MESSAGE_STATE_CHANGED: {
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(message, &old_state, &new_state, &pending_state);
    if(GST_MESSAGE_SRC(message) == GST_OBJECT(data->playbin)){
      g_print("Pipeline state change: %s ===> %s\n", gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
      data->playing = (new_state == GST_STATE_PLAYING);
      if(data->playing) {
	GstQuery *query;
	gint64 start, end;
	query = gst_query_new_seeking(GST_FORMAT_TIME);
	if(gst_element_query(data->playbin, query)) {
	  gst_query_parse_seeking(query, NULL, &data->seek_enabled, &start, &end);
	  if(data->seek_enabled) {
	    g_print("Seeking is ENABLED from %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT "\n", GST_TIME_ARGS(start), GST_TIME_ARGS(end));
	  }
	  else {
	    g_print("Seek DISABLED for this stream.\n");
	  }
	}
	else {
	  g_printerr("Seeking query failed.\n");
	}
	gst_query_unref(query);
      }
    }
  }
    break;
  default:
    g_printerr("This should not happen.\n");
    break;
  }
  gst_message_unref(message);
}
