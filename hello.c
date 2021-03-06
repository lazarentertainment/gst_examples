#include <gst/gst.h>

int main(int argc, char *argv[]) {
  GstElement *pipeline;
  GstBus *bus;
  GstMessage *message;

  gst_init(&argc, &argv);

  pipeline = gst_parse_launch("playbin uri=file:///code/ra.mp4 video-sink=ximagesink", NULL);
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  bus = gst_element_get_bus(pipeline);
  message = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

  if(message != NULL) {
    gst_message_unref(message);
  }
  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  return 0;
}
