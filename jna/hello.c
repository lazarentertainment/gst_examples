#include <stdio.h>
#include <gst/gst.h>

void helloFromC() {
  printf("Hello from C.\n");
}

void videoTestSource() {
  GstElement *test_source, *converter, *sink, *pipeline;
  GstElement *encoder, *muxer;
  GstBus *bus;
  GstMessage *message;
  GstStateChangeReturn return_value;

  gst_init(NULL, NULL);
  test_source = gst_element_factory_make("videotestsrc", "source");
  converter = gst_element_factory_make("videoconvert", "converter");
  encoder = gst_element_factory_make("x264enc", "encoder");
  muxer = gst_element_factory_make("flvmux", "muxer");
  sink = gst_element_factory_make("rtmpsink", "sink");
  pipeline = gst_pipeline_new("testsourcefromjavatortmp");

  if(!pipeline || !test_source || !converter || !sink || !encoder || !muxer) {
    g_printerr("Could create any elements\n");
  }

  gst_bin_add_many(GST_BIN(pipeline), test_source, converter, encoder, muxer, sink, NULL);

  if(gst_element_link(test_source, converter) != TRUE) {
    g_printerr("Could not connect the test source to the converter.\n");
    gst_object_unref(pipeline);
  }

  if(gst_element_link(converter, encoder) != TRUE) {
    g_printerr("Could not connect the converter to the x264 encoder.\n");
    gst_object_unref(pipeline);
  }

  if(gst_element_link(encoder, muxer) != TRUE) {
    g_printerr("Could not connect the encoder to the FLV muxer.\n");
    gst_object_unref(pipeline);
  }

  if(gst_element_link(muxer, sink) != TRUE) {
    g_printerr("Could not link the converter to the ximage sink.\n");
    gst_object_unref(pipeline);
  }

  g_object_set(encoder, "bframes", 0, NULL);
  g_object_set(muxer, "streamable", TRUE, NULL);
  g_object_set(sink, "location", "rtmp://192.168.1.124:1935/yanked/stream-videotestsrc", NULL);

  return_value = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if(return_value == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to start playing the pipeline.\n");
    gst_object_unref(pipeline);
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
}

/* void initPipeline() { */
/*   pthread_create(&gst_app_thread, NULL, &videoTestSource, NULL); */
/* } */

/* void finalizePipeline() { */
/*   pthread_join(gst_app_thread, NULL); */
/* } */

void showGstVersion() {
  const gchar *nano_str;
  guint major, minor, micro, nano;
  //  gst_init(&argc, &argv);
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

  printf("This program is linked against gstreamer version %d.%d.%d %s\n", major, minor, micro, nano_str);
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
