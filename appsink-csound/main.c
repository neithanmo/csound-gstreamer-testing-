#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#include <csound/csound.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

// compile with
    // gcc main.c -Wall `pkg-config --cflags --libs gstreamer-1.0 ` -lgstapp-1.0 -DUSE_DOUBLE -lcsound64


const gchar *audio_caps =
    "audio/x-raw,format=F64LE,channels=2,rate=44100, layout=interleaved";
int result;

typedef struct
{
  GMainLoop *loop;
  GstElement *source;
  CSOUND *csound;
  MYFLT* csoundbuf;
  char **csd;
  int args;
} ProgramData;



static GstFlowReturn
on_new_sample_from_sink (GstElement * elt, void *data1)
{
  GstSample *sample;
  GstBuffer *buffer;
  GstFlowReturn ret;
  ProgramData *data = (ProgramData *)data1;
  sample = gst_app_sink_pull_sample (GST_APP_SINK (elt));
  buffer = gst_sample_get_buffer (sample);
  /*gst_buffer_extract (GstBuffer *buffer,gsize offset,gpointer dest,gsize size);
   * Here copying data from gstreamer new sample into csound workins input buffer
   *
   * */
  g_print("appsink \n");
  gst_buffer_extract(buffer, 0, (gpointer *)data->csoundbuf, 8);
  gst_sample_unref (sample);
  return ret;
}


static gboolean
on_source_message (GstBus * bus, GstMessage * message, ProgramData * data)
{

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      g_print ("The source got dry\n");
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


uintptr_t performance_function(void *data1)
{
    ProgramData *data = (ProgramData *)data1;
    while(csoundPerformKsmps(data->csound) == 0){
        g_print("csound \n");
        result = 1;
    }
    csoundStop(data->csound);
    csoundCleanup(data->csound);
    g_main_loop_quit(data->loop);
    return 0;
}



int main(int argc, char **argv)
{

    ProgramData *data = NULL;
    gchar *string = NULL;
    GstBus *bus = NULL;
    GstElement *testsink = NULL;

    gst_init (NULL, NULL);

    data = g_new0 (ProgramData, 1);
    result = 1;
    data->loop = g_main_loop_new (NULL, FALSE);
    data->csd = argv;
    data->args = argc;
    gchar *name = "salidadeAudio";
    string =g_strdup_printf("audiotestsrc ! audioconvert ! appsink caps=\"%s\" name=testsink",audio_caps);
    //string =g_strdup_printf("audiotestsrc wave=6 ! audioconvert ! autoaudiosink name=\"%s\"", name);
    //string =g_strdup_printf("audiotestsrc wave=6 ! audioconvert ! filesink location=\"%s\"", name);
    data->source = gst_parse_launch (string, NULL);
    g_free (string);

    if (data->source == NULL) {
      g_print ("Bad source\n");
      return -1;
    }

    /* configuring csound---------------------------------------*/
    data->csound = csoundCreate(NULL);
    int result2 = csoundCompile(data->csound, data->args, data->csd);
    csoundSetHostImplementedAudioIO(data->csound, 1, 2048);
    csoundSetOutput(data->csound, "dac", NULL, NULL);
    data->csoundbuf = csoundGetSpin(data->csound);
    g_print("HHHHHHHHEXX %u \n", data->csoundbuf);
    if(result2) exit(-1);
    /*----------------------------------------------------------*/


    /* to be notified of messages from this pipeline, mostly EOS */
    bus = gst_element_get_bus (data->source);
    gst_bus_add_watch (bus, (GstBusFunc) on_source_message, data);
    gst_object_unref (bus);

    testsink = gst_bin_get_by_name (GST_BIN (data->source), "testsink");
    g_object_set (G_OBJECT (testsink), "emit-signals", TRUE, "sync", FALSE, NULL);
    gst_app_sink_set_drop(testsink, TRUE);
    gst_app_sink_set_max_buffers(testsink, 1);
    /* setting the new audio samples callback-------------------*/
    g_signal_connect (testsink, "new-sample",
        G_CALLBACK (on_new_sample_from_sink), (void *)data);
    gst_object_unref (testsink);

    /* running the pipeline before to start the csound preformance thread*/
    gst_element_set_state (data->source, GST_STATE_PLAYING);

    /*csound performance thread, it is necesary because gstreamer need its own thread and running
     * the csound performance functions will  locking all application*/
    void *thread = csoundCreateThread(&performance_function,(void*)data);

    g_print ("Let's run!\n");
    g_main_loop_run (data->loop);
    g_print ("Going out\n");
    gst_element_set_state (data->source, GST_STATE_NULL);
    gst_object_unref (data->source);
    g_main_loop_unref (data->loop);
    g_free (data);
    return 0;
}

