
#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#include <csound/csound.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

const gchar *audio_caps =
    "audio/x-raw,format=S16LE,channels=1,rate=44100";
/*const gchar *audio_caps =
    "audio/x-raw,format=F32LE,channels=1,rate=44100";*/
int result;
typedef struct
{
  GMainLoop *loop;
  GstElement *source;
  CSOUND *csound;
  MYFLT *csoundbuf;
  char **csd;
  int args;
  GstSample *sample;
  gint num_sample;
  gint user_ksmps;
  GstBuffer *buffer;
  GstFlowReturn ret;
  GstMapInfo info;
  GstPad *pad;
} ProgramData;


static GstFlowReturn
on_new_sample_from_sink (GstElement * elt, void *data1)
{

    ProgramData *data = (ProgramData *)data1;
    GstMapInfo info;
    data->sample = gst_app_sink_pull_sample (GST_APP_SINK (elt));
    data->buffer = gst_sample_get_buffer (data->sample);
    gst_buffer_map(data->buffer, &info, GST_MAP_READ);
    guint16 a,b;
    MYFLT CSsample;
    data->num_sample=0;
    int i=0;
    //g_print("data info size %u \n", info.size);
    while(data->num_sample < (info.size-1)){
        a = info.data[data->num_sample];
        b = info.data[data->num_sample+1];
       CSsample  = (MYFLT)(b<<8 | a&0x00FF);
        if(i<data->user_ksmps){
            data->csoundbuf[i]=CSsample;
            i++;
            //g_print("in buffer %g \n", CSsample);
        }
        if(i==data->user_ksmps){
            i=0;
            csoundPerformKsmps(data->csound);
        }
        data->num_sample=data->num_sample+2;
    }
    gst_sample_unref (data->sample);
    //gst_buffer_unmap(data->buffer);
  return data->ret;
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


uintptr_t performance_function(void *data1)
{
    ProgramData *data = (ProgramData *)data1;
    MYFLT *out1 = csoundGetSpout (data->csound);
    MYFLT *out2 = csoundGetOutputBuffer(data->csound);
    int j = 0;
    while(csoundPerformKsmps(data->csound) == 0){
        if(j<data->user_ksmps){
            //g_print("out buffer %g \n", out1[j]);
            j++;
        }
        else j=0;
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
    //string = g_strdup_printf("audiotestsrc wave=6 ! audioconvert volume volume=0.5 ! appsink caps=\"%s\" name=testsink",audio_caps);
    string = g_strdup_printf("filesrc location=audio.mp3  ! decodebin ! audioconvert ! audiorate "
                                "! audio/x-raw,format=S16LE,channels=1,rate=44100 ! "
                                "audioconvert ! appsink caps=\"%s\" name=testsink", audio_caps);
    //string =g_strdup_printf("audiotestsrc wave=6 num-buffers=1 ! audioconvert ! appsink caps=\"%s\" name=testsink",audio_caps);
    data->source = gst_parse_launch (string, NULL);
    g_free (string);

    if (data->source == NULL) {
      g_print ("Bad source\n");
      return -1;
    }
    /* config csound---------------------------------------*/
    data->csound = csoundCreate(NULL);
    int result2 = csoundCompile(data->csound, data->args, data->csd);
    data->csoundbuf = csoundGetSpin(data->csound);
    data->user_ksmps = csoundGetKsmps(data->csound);
    if(result2) exit(-1);
    /*----------------------------------------------------------*/


    /* to be notified of messages from this pipeline, mostly EOS */
    bus = gst_element_get_bus (data->source);
    gst_bus_add_watch (bus, (GstBusFunc) on_source_message, data);
    gst_object_unref (bus);
    testsink = gst_bin_get_by_name (GST_BIN (data->source), "testsink");
    g_object_set (G_OBJECT (testsink), "emit-signals", TRUE, "sync", TRUE, NULL);
    //gst_app_sink_set_drop(testsink, TRUE);
    //gst_app_sink_set_max_buffers(testsink, 1);
    /* setting the new audio samples callback-------------------*/
    g_signal_connect (testsink, "new-sample",
        G_CALLBACK (on_new_sample_from_sink), (void *)data);
    gst_object_unref (testsink);


    /* running the pipeline before to start the csound preformance thread
      and fill the csound input buffer with audio data  */
    gst_element_set_state (data->source, GST_STATE_PLAYING);

    /*csound performance thread, it is necesary because gstreamer need its own thread and running
     * the csound performance functions will  locking all application*/
    //void *thread = csoundCreateThread(&performance_function,(void*)data);

    g_print ("Let's run!\n");
    g_main_loop_run (data->loop);
    g_print ("Going out\n");
    gst_element_set_state (data->source, GST_STATE_NULL);
    gst_object_unref (data->source);
    g_main_loop_unref (data->loop);
    g_free (data);
    return 0;
}
