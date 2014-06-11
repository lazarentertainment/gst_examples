#include <gst/gst.h>

int main(int argc, char *argv[]) {
  GstElement *pipeline, *source, *sink, *filter;
  GstBus *bus;
  GstMessage *message;
  GstStateChangeReturn return_value;

  gst_init(&argc, &argv);

  source = gst_element_factory_make("videotestsrc", "source");
  sink = gst_element_factory_make("ximagesink", "sink");
  filter = gst_element_factory_make("shagadelictv", "filter");
  pipeline = gst_pipeline_new("test-pipeline");

  if(!pipeline || !source || !sink || !filter) {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  gst_bin_add_many(GST_BIN(pipeline), source, filter, sink, NULL);
  if(gst_element_link(source,filter) != TRUE) {
    g_printerr("Could not link source to filter.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  if(gst_element_link(filter,sink) != TRUE) {
    g_printerr("Could not link filter to sink.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  g_object_set(source, "pattern", 0, NULL);
  return_value = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if(return_value == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to start playing the pipeline.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  bus = gst_element_get_bus(pipeline);
  message = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

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
      break;
    case GST_MESSAGE_EOS:
      g_print("End of stream reached.\n");
      break;
    default:
      g_printerr("THIS SHOULD NOT HAPPEN\n");
      break;
    }
    gst_message_unref(message);
  }

  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  return 0;
}
