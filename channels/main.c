#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>
#include <csound/csound.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

// compile with
    // gcc main.c -Wall `pkg-config --cflags --libs gstreamer-1.0 ` -lgstapp-1.0 -DUSE_DOUBLE -lcsound64
 /*      run with:
                    ./a.out 2.csd
       */


const gchar *audio_caps =
    "audio/x-raw,format=S8,channels=1,rate=44100 ";
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

void s16le_to_myflt(GstMapInfo *info, ProgramData *data)
{
    int a,b=0;
    data->num_sample=0;
    while(data->num_sample < (data->user_ksmps-1)){
        b = info->data[data->num_sample];
        a= info->data[data->num_sample+1];
        //data->input_channel[data->num_sample]= (MYFLT)(b<<8 | a&0x00FF)/1.0;
        data->input_channel[data->num_sample]=(MYFLT)info->data[data->num_sample]/1.0;
        printf("s16le audio to csound is: %d \n", info->size);
        data->num_sample++;
    }
    csoundSetAudioChannel(data->csound, "ainput", data->input_channel);
}

static GstFlowReturn
on_new_sample_from_sink (GstElement * elt, void *data1)
{
  ProgramData *data = (ProgramData *)data1;
  data->sample = gst_app_sink_pull_sample (GST_APP_SINK (elt));
  data->buffer = gst_sample_get_buffer (data->sample);
  GstMapInfo info;
  gst_buffer_map(data->buffer, &info, GST_MAP_READ);
  //GstCaps *c = gst_sample_get_caps(data->sample);
  //g_print("audio caps %s\n", gst_caps_to_string(c));

  /*if(data->num_sample<data->user_ksmps){
      data->input_channel[data->num_sample]=(float)info->data[data->num_sample]/1.0;
      //g_print("adata %g \n", data->input_channel[data->num_sample]);
      data->num_sample++;
  }*/
  //g_print("2 buffers has %d samples \n", info.size);
  //s16le_to_myflt(&info, data);
  //if(data->num_sample == data->user_ksmps) {
      //csoundSetAudioChannel(data->csound, "ainput", data->input_channel);
      //int info_data = info.size;
      /*while(info_data){
          //g_print("info.data value in position %u is %u \n", info_data, info.data[info_data]);
          //info_data--;
          *data->aflg=*data->aflg+1000;

      }*/

      data->num_sample++;
      //*data->aflg=*data->aflg+1000;
 // }
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
    //long *out1 = csoundGetSpout (data->csound);
    while(csoundPerformKsmps(data->csound) == 0){
        //*data->aflg = 2000;
        data->sample = gst_app_sink_pull_sample (GST_APP_SINK (data->app));
        data->buffer = gst_sample_get_buffer (data->sample);
        GstMapInfo info;
        gst_buffer_map(data->buffer, &info, GST_MAP_READ);
        s16le_to_myflt(&info, data);
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
    string =g_strdup_printf("audiotestsrc wave=10  ! audioconvert ! appsink caps=\"%s\" name=testsink", audio_caps);
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
    g_object_set (G_OBJECT (data->app), "emit-signals", TRUE, "sync", FALSE, NULL);
    //gst_app_sink_set_drop(data->app, TRUE);
    //gst_app_sink_set_max_buffers(data->app, 1);
    /* setting the new audio samples callback-------------------*/
   /* g_signal_connect (data->app, "new-sample",
        G_CALLBACK (on_new_sample_from_sink), (void *)data);*/

    /* running the pipeline before to start the csound preformance thread*/
    gst_element_set_state (data->source, GST_STATE_PLAYING);
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

