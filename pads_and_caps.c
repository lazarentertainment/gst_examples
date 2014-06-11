#include <gst/gst.h>

static gboolean print_field(GQuark field, const GValue *value, gpointer pfx) {
  gchar *str = gst_value_serialize(value);
  g_print("%s %15s: %s\n", (gchar *)pfx, g_quark_to_string(field), str);
  g_free(str);
  return TRUE;
}

static void print_caps(const GstCaps *caps, const gchar *pfx) {
  guint i;
  g_return_if_fail(caps != NULL);
  if(gst_caps_is_any(caps)) {
    g_print("%sANY\n", pfx);
    return;
  }

  if(gst_caps_is_empty(caps)) {
    g_print("%sEMPTY\n", pfx);
    return;
  }

  for(i = 0; i < gst_caps_get_size(caps); i++) {
    GstStructure *structure = gst_caps_get_structure(caps, i);
    g_print("%s%s\n", pfx, gst_structure_get_name(structure));
    gst_structure_foreach(structure, print_field, (gpointer) pfx);
  }
}

static void print_pad_templates_information(GstElementFactory *factory) {
  const GList *pads;
  GstStaticPadTemplate *pad_template;
  g_print("Pad Templates for %s:\n", gst_element_factory_get_metadata(factory, "long-name"));
  if(gst_element_factory_get_num_pad_templates(factory) == 0) {
    g_print(" none\n");
    return;
  }

  pads = gst_element_factory_get_static_pad_templates(factory);
  while(pads) {
    pad_template = (GstStaticPadTemplate *)(pads->data);
    pads = g_list_next(pads);

    if(pad_template->direction == GST_PAD_SRC) {
      g_print(" SRC template: '%s'\n", pad_template->name_template);
    }
    else if(pad_template->direction == GST_PAD_SINK) {
      g_print(" SINK template: '%s'\n", pad_template->name_template);
    }
    else {
      g_print(" UNKNOWN! template: '%s'\n", pad_template->name_template);
    }

    if(pad_template->presence == GST_PAD_ALWAYS) {
      g_print(" Availability: Always\n");
    }
    else if(pad_template->presence == GST_PAD_SOMETIMES) {
      g_print(" Availability: Sometimes\n");
    }
    else if(pad_template->presence == GST_PAD_REQUEST) {
      g_print(" Availability: On Request\n");
    }
    else {
      g_print(" Availability: Unknown\n");
    }

    if(pad_template->static_caps.string) {
      g_print(" Capabilities:\n");
      print_caps(gst_static_caps_get(&pad_template->static_caps), "        ");
    }
    g_print("\n");
  }
}

static void print_pad_capabilities(GstElement *element, gchar *pad_name) {
  GstPad *pad = NULL;
  GstCaps *caps = NULL;

  pad = gst_element_get_static_pad(element, pad_name);
  if(!pad) {
    g_print("Could not retrieve pad '%s'\n", pad_name);
    return;
  }

  caps = gst_pad_get_current_caps(pad);
  if(caps) {
    g_print("Caps for the %s pad:\n", pad_name);
    print_caps(caps, "      ");
    gst_caps_unref(caps);
  }
  gst_object_unref(pad);
}

int main(int argc, char *argv[]) {
  GstElement *pipeline, *source, *sink;
  GstElementFactory *source_factory, *sink_factory;
  GstBus *bus;
  GstMessage *message;
  GstStateChangeReturn return_value;
  gboolean terminate = FALSE;

  gst_init(&argc, &argv);

  source_factory = gst_element_factory_find("videotestsrc");
  sink_factory = gst_element_factory_find("ximagesink");

  if(!source_factory || !sink_factory) {
    g_printerr("Could not find either a source or a sink factory.\n");
    return -1;
  }

  print_pad_templates_information(source_factory);
  print_pad_templates_information(sink_factory);

  source = gst_element_factory_create(source_factory, "source");
  sink = gst_element_factory_create(sink_factory, "sink");
  pipeline = gst_pipeline_new("test-pads-caps");

  if(!pipeline || !source || !sink) {
    g_printerr("Could not create a pipeline or a source or a sink.\n");
    return -1;
  }

  gst_bin_add_many(GST_BIN(pipeline), source, sink, NULL);
  if(gst_element_link(source, sink) != TRUE) {
    g_printerr("Could not link source to sink.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  g_print("In NULL state:\n");
  print_pad_capabilities(sink, "sink");

  return_value = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if(return_value == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set pipeline to playing state\n");
  }

  bus = gst_element_get_bus(pipeline);

  do {
    message = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED);

    if(message != NULL) {
      GError *error;
      gchar *debug_info;
      switch(GST_MESSAGE_TYPE(message)) {
      case GST_MESSAGE_ERROR:
	gst_message_parse_error(message, &error, &debug_info);
	g_printerr("Error received from element '%s': %s\n", GST_OBJECT_NAME(message->src), error->message);
	g_printerr("Debug info: %s\n", debug_info ? debug_info : "none");
	g_clear_error(&error);
	g_free(debug_info);
	terminate = TRUE;
	break;
      case GST_MESSAGE_EOS:
	g_print("EOS reached.\n");
	terminate = TRUE;
	break;
      case GST_MESSAGE_STATE_CHANGED:
	if(GST_MESSAGE_SRC(message) == GST_OBJECT(pipeline)) {
	  GstState old_state, new_state, pending_state;
	  gst_message_parse_state_changed(message, &old_state, &new_state, &pending_state);
	  g_print("State changed from %s to %s\n", gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
	  print_pad_capabilities(sink, "sink");
	}
	break;
      default:
	g_printerr("Should not happen.\n");
	break;
      }
      gst_message_unref(message);
    }
  } while(!terminate);

  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  gst_object_unref(source_factory);
  gst_object_unref(sink_factory);
  return 0;
}
