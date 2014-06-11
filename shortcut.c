#include <gst/gst.h>
#include <string.h>

#define CHUNK_SIZE 1024
#define SAMPLE_RATE 44100
#define AUDIO_CAPS "audio/x-raw,channels=1,rate=%d,signed=(boolean)true,width=16,depth=16,endianness=BYTE_ORDER"

typedef struct _CustomData {
  GstElement *pipeline, *app_source, *tee, *audio_queue, *audio_convert1, *audio_resample, *audio_sink;
  GstElement *video_queue, *audio_convert2, *visual, *video_convert, *video_sink;
  GstElement *app_queue, *app_sink;
  guint64 num_of_samples;
  gfloat a,b,c,d;
  guint source_id;
  GMainLoop *main_loop;
} CustomData;

static gboolean push_data(CustomData *data) {
  GstBuffer *buffer;
  GstFlowReturn return_flow;
  int i;
  /* gint16 *raw; */
  gint num_of_samples = CHUNK_SIZE / 2;
  gfloat frequency;
  buffer = gst_buffer_new_and_alloc(CHUNK_SIZE);
  GST_BUFFER_TIMESTAMP (buffer) = gst_util_uint64_scale(data->num_of_samples, GST_SECOND, SAMPLE_RATE);
  GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale(CHUNK_SIZE, GST_SECOND, SAMPLE_RATE);

  /* raw = (gint16 *)GST_BUFFER_DATA (buffer); */
  data->c += data->d;
  data->d -= data->c / 1000;
  frequency = 1100 + 1000 * data->d;

  for(i = 0; i < num_of_samples; i++) {
    data->a += data->b;
    data->b -= data->a / frequency;
    /* raw[i] = (gint16)(500 * data->a); */
  }
  data->num_of_samples += num_of_samples;

  g_signal_emit_by_name(data->app_source, "push-buffer", buffer, &return_flow);
  gst_buffer_unref(buffer);

  if(return_flow != GST_FLOW_OK) {
    return FALSE;
  }
  return TRUE;
}

static void start_feed(GstElement *src, guint size, CustomData *data) {
  if(data->source_id == 0) {
    g_print("Start the feed.\n");
    data->source_id = g_idle_add((GSourceFunc) push_data, data);
  }
}

static void stop_feed(GstElement *src, CustomData *data) {
  if(data->source_id != 0) {
    g_print("Stop the feed.\n");
    g_source_remove(data->source_id);
    data->source_id = 0;
  }
}

static void new_buffer(GstElement *sink, CustomData *data) {
  GstBuffer *buffer;
  g_signal_emit_by_name(sink, "pull-buffer", &buffer);
  if(buffer) {
    g_print("*");
    gst_buffer_unref(buffer);
  }
}

static void error_callback(GstBus *bus, GstMessage *message, CustomData *data) {
  GError *error;
  gchar *debug_info;

  gst_message_parse_error(message, &error, &debug_info);
  g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(message->src), error->message);
  g_printerr("Debug info: %s\n", debug_info ? debug_info : "none");
  g_clear_error(&error);
  g_free(debug_info);

  g_main_loop_quit(data->main_loop);
}

int main(int argc, char *argv[]) {
  CustomData data;
  GstPadTemplate *tee_src_pad_template;
  GstPad *tee_audio_pad, *tee_video_pad, *tee_app_pad;
  GstPad *audio_queue_pad, *video_queue_pad, *app_queue_pad;
  gchar *audio_caps_text;
  GstCaps *audio_caps;
  GstBus *bus;

  memset(&data, 0, sizeof(data));
  data.b = 1;
  data.d = 1;

  gst_init(&argc, &argv);

  data.app_source = gst_element_factory_make("appsrc", "app_source");
  data.tee = gst_element_factory_make("tee", "tee");
  data.audio_queue = gst_element_factory_make("queue", "audio_queue");
  data.audio_convert1 = gst_element_factory_make("audioconvert", "audio_convert1");
  data.audio_resample = gst_element_factory_make("audioresample", "audio_resample");
  data.audio_sink = gst_element_factory_make("autoaudiosink", "audio_sink");

  data.video_queue = gst_element_factory_make("queue", "video_queue");
  data.audio_convert2 = gst_element_factory_make("audioconvert", "audio_convert2");
  data.visual = gst_element_factory_make("wavescope", "visual");
  data.video_convert = gst_element_factory_make("videoconvert", "csp");
  data.video_sink = gst_element_factory_make("ximagesink", "video_sink");

  data.app_queue = gst_element_factory_make("queue", "app_queue");
  data.app_sink = gst_element_factory_make("appsink", "app_sink");

  data.pipeline = gst_pipeline_new("shorty-stuff");

  if(!data.pipeline || !data.app_source || !data.tee || !data.audio_queue || !data.audio_convert1 || !data.audio_resample || !data.audio_sink || !data.video_queue || !data.audio_convert2 || !data.visual || !data.video_convert || !data.video_sink || !data.app_queue || !data.app_sink) {
    g_printerr("One of these weird elements wasn't created.\n");
    return -1;
  }

  g_object_set(data.visual, "shader", 0, "style", 0, NULL);

  audio_caps_text = g_strdup_printf(AUDIO_CAPS, SAMPLE_RATE);
  audio_caps = gst_caps_from_string(audio_caps_text);
  g_object_set(data.app_source, "caps", audio_caps, NULL);

  g_signal_connect(data.app_source, "need-data", G_CALLBACK(start_feed), &data);
  g_signal_connect(data.app_source, "enough-data", G_CALLBACK(stop_feed), &data);

  g_object_set(data.app_sink, "emit-signals", TRUE, "caps", audio_caps, NULL);
  g_signal_connect(data.app_sink, "new-buffer", G_CALLBACK(new_buffer), &data);
  gst_caps_unref(audio_caps);
  g_free(audio_caps_text);

  gst_bin_add_many(GST_BIN(data.pipeline), data.app_source, data.tee, data.audio_queue, data.audio_convert1, data.audio_resample, data.audio_sink, data.video_queue, data.audio_convert2, data.visual, data.video_convert, data.video_sink, data.app_queue, data.app_sink, NULL);

  if(gst_element_link_many(data.app_source, data.tee, NULL) != TRUE || gst_element_link_many(data.audio_queue, data.audio_convert1, data.audio_resample, data.audio_sink, NULL) != TRUE || gst_element_link_many(data.video_queue, data.audio_convert2, data.visual, data.video_convert, data.video_sink, NULL) != TRUE || gst_element_link_many(data.app_queue, data.app_sink, NULL) != TRUE) {
    g_printerr("Elements could not be linked.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  tee_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(data.tee), "src_%u");
  tee_audio_pad = gst_element_request_pad(data.tee, tee_src_pad_template, NULL, NULL);
  g_print("Obtained a request for pad %s for audio branch.\n", gst_pad_get_name(tee_audio_pad));
  audio_queue_pad = gst_element_get_static_pad(data.audio_queue, "sink");

  tee_video_pad = gst_element_request_pad(data.tee, tee_src_pad_template, NULL, NULL);
  g_print("Obtained a request for pad %s for video branch.\n", gst_pad_get_name(tee_video_pad));
  video_queue_pad = gst_element_get_static_pad(data.video_queue, "sink");

  tee_app_pad = gst_element_request_pad(data.tee, tee_src_pad_template, NULL, NULL);
  g_print("Obtained a request for pad %s for app branch.\n", gst_pad_get_name(tee_app_pad));

  app_queue_pad = gst_element_get_static_pad(data.app_queue, "sink");

  if(gst_pad_link(tee_audio_pad, audio_queue_pad) != GST_PAD_LINK_OK || gst_pad_link(tee_video_pad, video_queue_pad) != GST_PAD_LINK_OK || gst_pad_link(tee_app_pad, app_queue_pad) != GST_PAD_LINK_OK) {
    g_printerr("Tee could not be linked.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }
  gst_object_unref(audio_queue_pad);
  gst_object_unref(video_queue_pad);
  gst_object_unref(app_queue_pad);

  bus = gst_element_get_bus(data.pipeline);
  gst_bus_add_signal_watch(bus);
  g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)error_callback, &data);
  gst_object_unref(bus);

  gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

  data.main_loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(data.main_loop);

  gst_element_release_request_pad(data.tee, tee_audio_pad);
  gst_element_release_request_pad(data.tee, tee_video_pad);
  gst_element_release_request_pad(data.tee, tee_app_pad);
  gst_object_unref(tee_audio_pad);
  gst_object_unref(tee_video_pad);
  gst_object_unref(tee_app_pad);

  gst_element_set_state(data.pipeline, GST_STATE_NULL);
  gst_object_unref(data.pipeline);
  return 0;
}
