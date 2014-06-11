#include <gst/gst.h>

int main(int argc, char *argv[]) {
  GstElement *pipeline, *audio_source, *tee, *audio_queue, *audio_convert, *audio_resample, *audio_sink;
  GstElement *video_queue, *visual, *video_convert, *video_sink;
  GstBus *bus;
  GstMessage *message;
  GstPadTemplate *tee_src_pad_template;
  GstPad *tee_audio_pad, *tee_video_pad;
  GstPad *audio_queue_pad, *video_queue_pad;

  gst_init(&argc, &argv);

  audio_source = gst_element_factory_make("audiotestsrc", "audio_source");
  tee = gst_element_factory_make("tee", "tee");
  audio_queue = gst_element_factory_make("queue", "audio_queue");
  audio_convert = gst_element_factory_make("audioconvert", "audio_convert");
  audio_resample = gst_element_factory_make("audioresample", "audio_resample");
  audio_sink = gst_element_factory_make("autoaudiosink", "audio_sink");

  video_queue = gst_element_factory_make("queue", "video_queue");
  visual = gst_element_factory_make("wavescope", "visual");
  video_convert = gst_element_factory_make("videoconvert", "csp");
  video_sink = gst_element_factory_make("ximagesink", "video_sink");

  pipeline = gst_pipeline_new("multi-stuff");

  if(!pipeline || !audio_source || !tee || !audio_queue || !audio_convert || !audio_resample || !audio_sink || !video_queue || !visual || !video_convert || !video_sink) {
    g_printerr("One of these weird elements wasn't created.\n");
    return -1;
  }

  g_object_set(audio_source, "freq", 215.0f, NULL);
  g_object_set(visual, "shader", 0, "style", 1, NULL);

  gst_bin_add_many(GST_BIN(pipeline), audio_source, tee, audio_queue, audio_convert, audio_resample, audio_sink, video_queue, visual, video_convert, video_sink, NULL);
  if(gst_element_link_many(audio_source, tee, NULL) != TRUE || gst_element_link_many(audio_queue, audio_convert, audio_resample, audio_sink, NULL) != TRUE || gst_element_link_many(video_queue, visual, video_convert, video_sink, NULL) != TRUE) {
    g_printerr("Elements could not be linked.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  tee_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(tee), "src_%u");
  tee_audio_pad = gst_element_request_pad(tee, tee_src_pad_template, NULL, NULL);
  g_print("Obtained a request for pad %s for audio branch.\n", gst_pad_get_name(tee_audio_pad));
  audio_queue_pad = gst_element_get_static_pad(audio_queue, "sink");

  tee_video_pad = gst_element_request_pad(tee, tee_src_pad_template, NULL, NULL);
  g_print("Obtained a request for pad %s for video branch.\n", gst_pad_get_name(tee_video_pad));
  video_queue_pad = gst_element_get_static_pad(video_queue, "sink");

  if(gst_pad_link(tee_audio_pad, audio_queue_pad) != GST_PAD_LINK_OK || gst_pad_link(tee_video_pad, video_queue_pad) != GST_PAD_LINK_OK) {
    g_printerr("Tee could not be linked.\n");
    gst_object_unref(pipeline);
    return -1;
  }
  gst_object_unref(audio_queue_pad);
  gst_object_unref(video_queue_pad);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  bus = gst_element_get_bus(pipeline);
  message = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

  gst_element_release_request_pad(tee, tee_audio_pad);
  gst_element_release_request_pad(tee, tee_video_pad);
  gst_object_unref(tee_audio_pad);
  gst_object_unref(tee_video_pad);

  if(message != NULL) {
    gst_object_unref(message);
  }
  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  return 0;
}
