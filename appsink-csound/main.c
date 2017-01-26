
#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#include <csound/csound.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

const gchar *audio_caps =
    "audio/x-raw,format=F64LE,channels=1,rate=44100";

typedef struct
{
  GMainLoop *loop;
  GstElement *source;
  GstElement *sink;
  GstElement *appsrc;
  CSOUND *csound;
  MYFLT *csoundbuf;
  char **csd;
  int args;
  GstSample *sample;
  gint num_sample;
  gint samples;
  gint user_ksmps;
  GstBuffer *buffer;
  GstFlowReturn ret;
} ProgramData;


static GstFlowReturn
on_new_sample_from_sink (GstElement * elt, ProgramData * data)
{
    GstMapInfo info;
    data->sample = gst_app_sink_pull_sample (GST_APP_SINK (elt));
    data->buffer = gst_sample_get_buffer (data->sample);
    gst_buffer_map(data->buffer, &info, GST_MAP_READ);
    process(info.data, info.size, data);
    gst_buffer_unmap(data->buffer, &info);
    gst_app_src_push_buffer(data->appsrc, data->buffer);
    return data->ret;
}

void process( gdouble *buf, int size, ProgramData * data)
{
    int num_samples = size/8;//8 bytes por sample(F64LE)
    data->num_sample=0;
    int i = 0;
    int j = 0;
    int ciclos=num_samples/data->user_ksmps;
    gdouble *out = csoundGetSpout (data->csound);
    for(i=0;i<ciclos;i++){
        memcpy(data->csoundbuf, buf, data->user_ksmps*sizeof(gdouble));
        csoundPerformKsmps(data->csound);
        memcpy(buf, out, data->user_ksmps*sizeof(gdouble));
        buf = buf + data->user_ksmps;
        data->num_sample = data->num_sample + data->user_ksmps;
     }
    memcpy(data->csoundbuf, buf, (num_samples-data->num_sample)*sizeof(MYFLT));
    csoundPerformKsmps(data->csound);
    memcpy(buf, out, (num_samples-data->num_sample)*sizeof(MYFLT));
}


static gboolean
on_source_message (GstBus * bus, GstMessage * message, ProgramData * data)
{

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      g_print ("The source got dry\n");
      csoundStop(data->csound);
      csoundCleanup(data->csound);
      g_main_loop_quit(data->loop);
      break;
    case GST_MESSAGE_ERROR:
      g_print ("Received error\n");
      g_main_loop_quit (data->loop);
      break;
    default:
      break;
  }
  return TRUE;
}

/* called when we get a GstMessage from the sink pipeline when we get EOS, we
 * exit the mainloop and this testapp. */
static gboolean
on_sink_message (GstBus * bus, GstMessage * message, ProgramData * data)
{
  /* nil */
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      g_print ("Finished playback\n");
      g_main_loop_quit (data->loop);
      break;
    case GST_MESSAGE_ERROR:
  {
      GError *err = NULL;
      gchar *dbg_info = NULL;
      gst_message_parse_error (message, &err, &dbg_info);
      g_printerr ("ERROR from element %s: %s\n",
          GST_OBJECT_NAME (message->src), err->message);
      g_printerr ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
      g_error_free (err);
      g_free (dbg_info);
      g_main_loop_quit (data->loop);
  }
      break;
    default:
      break;
  }
  return TRUE;
}


int main(int argc, char **argv)
{

    ProgramData *data = NULL;
    gchar *string = NULL;
    GstBus *bus = NULL;
    GstElement *testsink = NULL;
    gst_init (NULL, NULL);

    data = g_new0 (ProgramData, 1);
    data->loop = g_main_loop_new (NULL, FALSE);
    data->csd = argv;
    data->args = argc;


    /* config csound---------------------------------------*/
    data->csound = csoundCreate(NULL);
    //csoundSetHostImplementedAudioIO(data->csound, 0, -1);
    int result2 = csoundCompile(data->csound, data->args, data->csd);
    data->csoundbuf = csoundGetSpin(data->csound);
    data->user_ksmps = csoundGetKsmps(data->csound);
    gdouble *adata = (gdouble *)calloc(data->user_ksmps, sizeof(gdouble));
    if(result2) exit(-1);
    /*----------------------------------------------------------*/


    /* to be notified of messages from this pipeline, mostly EOS */
    string = g_strdup_printf("filesrc location=audio.mp3  ! decodebin ! audioconvert ! volume volume=2.0 ! audiorate !"
                                "audioconvert ! appsink caps=\"%s\" name=testsink", audio_caps);
    data->source = gst_parse_launch (string, NULL);
    if (data->source == NULL) {
      g_print ("Bad source\n");
      return -1;
    }
    bus = gst_element_get_bus (data->source);
    gst_bus_add_watch (bus, (GstBusFunc) on_source_message, data);
    gst_object_unref (bus);

    testsink = gst_bin_get_by_name (GST_BIN (data->source), "testsink");
    g_object_set (G_OBJECT (testsink), "emit-signals", TRUE, "sync", FALSE, NULL);
    gst_app_sink_set_drop(testsink, TRUE);
    /* setting the new audio samples callback-------------------*/
    g_signal_connect (testsink, "new-sample",
        G_CALLBACK (on_new_sample_from_sink), (void *)data);
    gst_object_unref (testsink);

    /* setting up sink pipeline, we push audio data into this pipeline that will
     * then play it back using the default audio sink. We have no blocking
     * behaviour on the src which means that we will push the entire file into
     * memory. */
    data->sink = gst_parse_launch ("appsrc name=src caps= audio/x-raw, format=F64LE, layout=(string)interleaved, "
                                   "channels=1, rate=44100 ! audioparse raw-format=F64LE channels=1 rate=44100 interleaved=true ! "
                                   " audioconvert ! audio/x-raw, format=S16LE, layout=(string)interleaved, channels=1, "
                                   "rate=44100 ! audioconvert ! queue ! lamemp3enc ! filesink location=hola.mp3", NULL);
    g_free (string);

    if (data->sink == NULL) {
      g_print ("Bad sink\n");
      return -1;
    }

    data->appsrc = gst_bin_get_by_name (GST_BIN (data->sink), "src");
    g_object_set (data->appsrc, "format", GST_FORMAT_TIME, NULL);
    bus = gst_element_get_bus (data->sink);
    gst_bus_add_watch (bus, (GstBusFunc) on_sink_message, data);
    gst_object_unref (bus);
    gst_element_set_state (data->source, GST_STATE_PLAYING);//appsink
    gst_element_set_state (data->sink, GST_STATE_PLAYING);//appsrc

    g_print ("Let's run!\n");
    g_main_loop_run (data->loop);
    g_print ("Going out\n");
    gst_element_set_state (data->source, GST_STATE_NULL);
    gst_element_set_state (data->sink, GST_STATE_NULL);
    gst_object_unref (data->sink);
    gst_object_unref (data->source);
    g_main_loop_unref (data->loop);
    g_free (data);
    return 0;
}
