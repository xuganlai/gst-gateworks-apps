#include <gst/gst.h>
 
/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData {
    GstElement *pipeline;
    GstElement *source;
    GstElement *depay;
    GstElement *parse;
    GstElement *avdec;
    GstElement *convert;
	GstElement *filter;
    GstElement *resample;
    GstElement *sink;
} CustomData;
 
/* Handler for the pad-added signal */
static void pad_added_handler (GstElement *src, GstPad *pad, CustomData *data);
 
int main(int argc, char *argv[]) {
    CustomData data;
    GstBus *bus;
    GstMessage *msg;
	GstCaps *Cap;
    GstStateChangeReturn ret;
    gboolean terminate = FALSE;
 printf("%s %s\r\n",__DATE__,__TIME__);
    /* Initialize GStreamer */
    //gst_init(&argc, &argv);
 	gst_init(NULL,NULL);
    /* Create the elements */
    data.source = gst_element_factory_make("rtspsrc", "source");
    g_object_set(G_OBJECT (data.source), "latency", 2000, NULL);
    data.depay = gst_element_factory_make("rtph265depay", "depay");
	
	//------
	Cap = gst_caps_from_string("video/x-h265,stream-format=byte-stream");
	data.filter = gst_element_factory_make("capsfilter", "filter");
	g_object_set(data.filter, "caps", Cap, NULL);

    data.parse = gst_element_factory_make ("h264parse", "parse");
    data.avdec = gst_element_factory_make ("avdec_h265", "avdec");
    data.convert = gst_element_factory_make ("videoconvert", "convert");
    //data.resample = gst_element_factory_make ("audioresample", "resample");
    
    data.sink = gst_element_factory_make ("filesink", "sink");
 	g_object_set(G_OBJECT(data.sink), "location", "/tmp/output.264", NULL);
    // g_object_set (G_OBJECT (data.sink), "sync", FALSE, NULL);
 
    /* Set the URI to play */
    g_object_set(data.source, "location", "rtsp://admin:123456@192.168.1.10:554/stream1", NULL);
    /* Connect to the pad-added signal */
    g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);
 
    /* Create the empty pipeline */
    data.pipeline = gst_pipeline_new("test-pipeline");
 #if 1
    if (!data.pipeline || !data.source || !data.depay || !data.filter || !data.sink) {
        g_printerr("Not all elements could be created.%p-%p-%p-%p-%p\n",data.pipeline,data.source,data.depay,data.filter,data.sink);
        return -1;
    }
 #endif
    /* Build the pipeline. Note that we are NOT linking the source at this
    * point. We will do it later. */
    #if 0
    gst_bin_add_many (GST_BIN(data.pipeline), data.source, data.depay, data.parse, data.avdec, data.convert, data.sink, NULL);
	#else
	gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.depay,data.filter,data.sink, NULL);//,data.avdec
	#endif
   // gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.depay,data.filter,data.sink, NULL);//,data.parse
    #if 0
	if(TRUE != gst_element_link_many(data.depay, data.parse, data.avdec, data.convert, data.sink, NULL))
	#else
	if(TRUE != gst_element_link_many(data.depay,data.filter,data.sink, NULL))//,data.avdec
	#endif
	//if(TRUE != gst_element_link_many(data.source,data.depay,data.filter, data.sink, NULL)) //,data.parse
	{
        g_printerr ("Elements could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
 
    /* Start playing */
    ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) 
	{
        g_printerr ("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
 
    /* Listen to the bus */
    bus = gst_element_get_bus(data.pipeline);
    do {
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
 		printf("while:%d-%d-%d-%d\r\n",GST_MESSAGE_TYPE (msg),GST_MESSAGE_ERROR,GST_MESSAGE_EOS,GST_MESSAGE_STATE_CHANGED);
        /* Parse message */
        if (msg != NULL) {
            GError *err;
            gchar *debug_info;
 
            switch (GST_MESSAGE_TYPE (msg)) {
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error(msg, &err, &debug_info);
                    g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
                    g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
                    g_clear_error (&err);
                    g_free (debug_info);
                    terminate = TRUE;
                    break;
                case GST_MESSAGE_EOS:
                    g_print ("End-Of-Stream reached.\n");
                    terminate = TRUE;
                    break;
                case GST_MESSAGE_STATE_CHANGED:
                    /* We are only interested in state-changed messages from the pipeline */
                    if (GST_MESSAGE_SRC(msg) == GST_OBJECT (data.pipeline)) {
                        GstState old_state, new_state, pending_state;
                        gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
                        g_print ("Pipeline state changed from %s to %s:\n",
                            gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));
                    }
                    break;
                default:
                    /* We should not reach here */
                    g_printerr ("Unexpected message received.\n");
                    break;
            }
            gst_message_unref (msg);
        }
    } while (!terminate);
 
    /* Free resources */
    gst_object_unref (bus);
    gst_element_set_state (data.pipeline, GST_STATE_NULL);
    gst_object_unref (data.pipeline);
    return 0;
}
 
/* This function will be called by the pad-added signal */
static void pad_added_handler (GstElement *src, GstPad *new_pad, CustomData *data) 
{
    GstPad *sink_pad = gst_element_get_static_pad (data->depay, "sink");
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;
 
    g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME (src));
 
    /* If our converter is already linked, we have nothing to do here */
    if (gst_pad_is_linked (sink_pad)) {
        g_print ("We are already linked. Ignoring.\n");
        goto exit;
    }
 
    /* Check the new pad's type */
    new_pad_caps = gst_pad_get_current_caps (new_pad);
    new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
    new_pad_type = gst_structure_get_name (new_pad_struct);
 
    /* Attempt the link */
    ret = gst_pad_link (new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED (ret)) {
        g_print ("Type is '%s' but link failed.\n", new_pad_type);
    } else {
        g_print ("Link succeeded (type '%s').\n", new_pad_type);
    }
 
exit:
    /* Unreference the new pad's caps, if we got them */
    if (new_pad_caps != NULL)
        gst_caps_unref (new_pad_caps);
 
    /* Unreference the sink pad */
    gst_object_unref (sink_pad);
}

