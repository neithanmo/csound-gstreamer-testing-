#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>
#include <csound/csound.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/audio/audio.h>


const gchar *audio_caps =
    "audio/x-raw,format=S16LE,channels=1,rate=44100 ";
int result;
typedef struct
{
  GMainLoop *loop;
  GstElement *source;
  CSOUND *csound;
  float *input_channel;//array de samples de audio
  MYFLT *aflg;
  int num_sample;//control para llenar el array de audio
  GstSample *sample;
  int user_ksmps;
  GstBuffer *buffer;
  GstElement *app;
  GstFlowReturn ret;
  char **csd;
  int args;
  GstPad *pad;
} ProgramData;

static GstFlowReturn
on_new_sample_from_sink (GstElement * elt, void *data1)
{
    ProgramData *data = (ProgramData *)data1;
    data->sample = gst_app_sink_pull_sample (GST_APP_SINK (elt));
    data->buffer = gst_sample_get_buffer (data->sample);
    GstMapInfo info;
    gst_buffer_map(data->buffer, &info, GST_MAP_READ);
    guint16 a,b=0;
    data->num_sample=0;
    int i=0;

    //g_print("buffer size %d and data size %d and memory blocks are  %d \n", x, info.size,
            //gst_buffer_n_memory(data->buffer));
    while(data->num_sample < (info.size-1)){
        a = info.data[data->num_sample];
        b = info.data[data->num_sample+1];
        gint16 int16_sample = (gint16)(b<<8 | a&0x00FF);
        if(i<data->user_ksmps){
            data->input_channel[i]=int16_sample;
            //g_print("rot: %d  \n", int16_sample);
            //g_print("gst: %d\n", (gint16)info.data[data->num_sample]);
            i++;
        }
        if(i == data->user_ksmps) {
            i=0;
            csoundSetAudioChannel(data->csound, "ainput", data->input_channel);
            csoundPerformKsmps(data->csound);
        }
        data->num_sample=data->num_sample+2;
        
    }
    data->num_sample=0;
    //csoundSetAudioChannel(data->csound, "ainput", data->input_channel);
    gst_buffer_unmap(data->buffer, &info);
    gst_sample_unref (data->sample);
    return data->ret;
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
    gint16 *out = csoundGetSpout (data->csound);
    int i = 0;
    while(csoundPerformKsmps(data->csound) == 0){
           if(i< data->user_ksmps) {
               //g_print("output data %d\n", out[i]);
               i++;
           }
           else i=0;
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

    //string =g_strdup_printf("audiotestsrc wave=10 ! audioconvert ! appsink caps=\"%s\" name=testsink", audio_caps);
    string =g_strdup_printf("filesrc location=audio.mp3  ! decodebin ! audioconvert ! audiorate "
                            "! audio/x-raw,format=S16LE,channels=1,rate=44100 ! "
                            "audioconvert ! appsink caps=\"%s\" name=testsink sync=true", audio_caps);
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
    csoundSetOption(data->csound, "-s");
    csoundSetOption(data->csound, "-b1024");
    int result2 = csoundCompile(data->csound, data->args, data->csd);
    data->user_ksmps = csoundGetKsmps(data->csound);
    g_print("csound KSMPS is %u \n", data->user_ksmps);
    if((csoundGetChannelPtr(data->csound, &data->input_channel, "ainput",
                                      CSOUND_INPUT_CHANNEL | CSOUND_AUDIO_CHANNEL) != 0))
    {
        g_print("NO ainput channel, exiting \n");
        exit(-1);
    }
    g_print("puntero al canal listo \n");
    /* input_channel es dimensionado para ser un array de KSMPS audio samples, segun el valor
     * de ksmps que estipulo el usuario en el CDS*/
    csoundGetChannelPtr(data->csound, &data->aflg, "moog",
                                          CSOUND_INPUT_CHANNEL | CSOUND_CONTROL_CHANNEL);
    data->input_channel = malloc (sizeof (data->input_channel) * data->user_ksmps);
    data->num_sample = 0;
    *data->aflg = 1500;
    /*int i = 0;
    while(data->user_ksmps-i){
        data->input_channel[i] =(float)rand()/1.0;
        g_print("%g\n", data->input_channel[i]);
        i++;
    }*/
    if(result2)
    {
        g_print("not compiled the CSD file, exiting \n");
        exit(-1);
    }

    /*----------------------------------------------------------*/


    /* to be notified of messages from this pipeline, mostly EOS */
    bus = gst_element_get_bus (data->source);
    gst_bus_add_watch (bus, (GstBusFunc) on_source_message, data);
    gst_object_unref (bus);

    data->app = gst_bin_get_by_name (GST_BIN (data->source), "testsink");
    g_object_set (G_OBJECT (data->app), "emit-signals", TRUE, "sync", TRUE, NULL);
    //gst_app_sink_set_drop(data->app, TRUE);
    //gst_app_sink_set_max_buffers(data->app, 1);
    /* setting the new audio samples callback-------------------*/
   g_signal_connect (data->app, "new-sample",
        G_CALLBACK (on_new_sample_from_sink), (void *)data);

    /* running the pipeline before to start the csound preformance thread*/
    gst_element_set_state (data->source, GST_STATE_PLAYING);
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

