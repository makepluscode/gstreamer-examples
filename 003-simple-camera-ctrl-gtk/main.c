#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

static guintptr video_window_handle = 0;
 
static GstElement *pipeline, *src, *sink;
static GstBus *bus;

// Window and containers
GtkWidget *app_window, *video_window, *main_box, *top_box, *bottom_box;

// Misc
GtkWidget *top_radio1, *top_radio2, *bottom_label1;

#if 0
static GstBusSyncReply
sync_bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  const GstStructure *st;
  const GValue *image;
  GstBuffer *buf = NULL;
  guint8 *data_buf = NULL;
  gchar *caps_string;
  guint size = 0;
  gchar *preview_filename = NULL;
  FILE *f = NULL;
  size_t written;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ELEMENT:{
      st = gst_message_get_structure (message);
      if (st) {
        if (gst_structure_has_name (message->structure, "prepare-xwindow-id")) {
          if (!no_xwindow && window) {
            gst_x_overlay_set_window_handle (GST_X_OVERLAY (GST_MESSAGE_SRC
                    (message)), window);
            gst_message_unref (message);
            message = NULL;
            return GST_BUS_DROP;
          }
        } else if (gst_structure_has_name (st, "image-captured")) {
          GST_DEBUG ("image-captured");
        } else if (gst_structure_has_name (st, "preview-image")) {
          GST_DEBUG ("preview-image");
          //extract preview-image from msg
          image = gst_structure_get_value (st, "buffer");
          if (image) {
            buf = gst_value_get_buffer (image);
            data_buf = GST_BUFFER_DATA (buf);
            size = GST_BUFFER_SIZE (buf);
            preview_filename = g_strdup_printf ("test_vga.rgb");
            caps_string = gst_caps_to_string (GST_BUFFER_CAPS (buf));
            g_print ("writing buffer to %s, elapsed: %.2fs, buffer caps: %s\n",
                preview_filename, g_timer_elapsed (timer, NULL), caps_string);
            g_free (caps_string);
            f = g_fopen (preview_filename, "w");
            if (f) {
              written = fwrite (data_buf, size, 1, f);
              if (!written) {
                g_print ("error writing file\n");
              }
              fclose (f);
            } else {
              g_print ("error opening file for raw image writing\n");
            }
            g_free (preview_filename);
          }
        }
      }
      break;
    }
    default:
      /* unhandled message */
      break;
  }
  return GST_BUS_PASS;
}
#endif

static gboolean is_eos_done = FALSE;
static gboolean
bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  g_info ("bus_callback called! (%d)\n", GST_MESSAGE_TYPE (message));  
  switch (GST_MESSAGE_TYPE (message))
  {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *debug;

      gst_message_parse_error (message, &err, &debug);
      g_error ("bus_callback error : %s\n", err->message);
      g_error_free (err);
      g_free (debug);

      // g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_STATE_CHANGED:
      {
        GstState oldstate, newstate;

        gst_message_parse_state_changed (message, &oldstate, &newstate, NULL);
        GST_DEBUG_OBJECT (GST_MESSAGE_SRC (message), "state-changed: %s -> %s",
            gst_element_state_get_name (oldstate),
            gst_element_state_get_name (newstate));
      }
      break;
    case GST_MESSAGE_EOS:
      {
        g_warning("bus_callback : eos");
        is_eos_done = TRUE;
      }
      break;
    default:
      /* unhandled message */
      break;
  }
  return TRUE;
}

static GstBusSyncReply bus_sync_handler (GstBus * bus, GstMessage * message, gpointer user_data)
{
  g_print("bus_sync_handler called!\n");
  // ignore anything but 'prepare-window-handle' element messages
  if (!gst_is_video_overlay_prepare_window_handle_message (message))
  {
    // g_print("bus_sync_handler : video_overlay is not prepared yet!\n");
    return GST_BUS_PASS;
  }

  if (video_window_handle != 0)
  {
    GstVideoOverlay *overlay;

    // GST_MESSAGE_SRC (message) will be the video sink element
    overlay = GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (message));
    gst_video_overlay_set_window_handle (overlay, video_window_handle);

    gtk_label_set_text(GTK_LABEL (bottom_label1), "camera is previewing...");
  }
  else
  {
    g_warning ("Should have obtained video_window_handle by now!");
  }

  gst_message_unref (message);
  return GST_BUS_DROP;
}

static void video_widget_realize_cb (GtkWidget * widget, gpointer data)
{
  g_print("video_widget_realize_cb called!\n");
  gulong xid = GDK_WINDOW_XID (gtk_widget_get_window (video_window));
  video_window_handle = xid;

  g_print("video_widget_realize_cb : xid : %x \n", xid);
}

static gboolean b_mode = FALSE;
static void create_pipeline();
static void close_pipeline();
static void change_source_cb (GtkToggleButton *widget, gpointer data)
{
  g_print("change_source_cb called!\n");
  g_print ("change_source_cb active: %d\n", gtk_toggle_button_get_active (widget));
  if(b_mode) // camera from video
  {
    gtk_button_set_label(GTK_TOGGLE_BUTTON (widget), "Change to camera");
    b_mode = FALSE;
  }
  else // video from camera
  {
    gtk_button_set_label(GTK_TOGGLE_BUTTON (widget), "Change to video");
    b_mode = TRUE;
  }

  create_pipeline();
}

static void create_gui()
{
  /*
  ** Windows
  */

  app_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(app_window), "Camera Preview");
  gtk_window_set_default_size(GTK_WINDOW(app_window), 960, 640);
  gtk_window_set_position(GTK_WINDOW(app_window), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width (GTK_CONTAINER (app_window), 8);
  g_signal_connect(G_OBJECT(app_window), "destroy", G_CALLBACK(gtk_main_quit), NULL);

  /*
  ** Main Widget
  */

  main_box = gtk_box_new(TRUE, 1);
  gtk_container_set_border_width(GTK_CONTAINER(main_box), 1);
  gtk_container_add(GTK_CONTAINER(app_window), main_box);
  
  /*
  ** Top box Widget
  */

  top_box = gtk_box_new(FALSE, 8);
  gtk_box_pack_start (GTK_BOX (main_box), top_box, TRUE, TRUE, 0);

  GtkWidget *frame = gtk_frame_new ("Usage of Player");
  gtk_widget_set_size_request(frame, 300, 60);
  gtk_frame_set_label_align(frame, 0.04, 0.5);
  GtkWidget *label = gtk_label_new ("You can select a source between video and camera.");
  gtk_misc_set_alignment (GTK_MISC (label), 0.1, 0.5);
  gtk_container_add(GTK_CONTAINER(frame), label);
  gtk_box_pack_start (GTK_BOX (top_box), frame, TRUE, TRUE, 8);

#if 0
  top_radio1 = gtk_radio_button_new_with_label (NULL, "Camera");
  gtk_box_pack_start (GTK_BOX (radio_box), top_radio1, TRUE, TRUE, 4);
  GSList *group = gtk_radio_button_get_group (GTK_RADIO_BUTTON(top_radio1));
  top_radio2 = gtk_radio_button_new_with_label (group, "Video");
  gtk_box_pack_start (GTK_BOX (radio_box), top_radio2, TRUE, TRUE, 8);

  g_signal_connect(G_OBJECT(top_radio1), "camera", G_CALLBACK(change_source_cb), NULL);
  g_signal_connect(G_OBJECT(top_radio2), "video", G_CALLBACK(change_source_cb), NULL);
#else
  GtkWidget *box = gtk_box_new (FALSE, 8);
  // gtk_widget_set_size_request(box, 300, 60);
  // gtk_frame_set_label_align(box, 0.04, 0.5);

  GtkWidget *toggle1 = gtk_toggle_button_new_with_label ("Camera");
  // Makes this toggle button invisible
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (toggle1), TRUE);

  gtk_misc_set_alignment (GTK_MISC (box), 0.05, 0.5);
  gtk_widget_set_size_request(toggle1, 200, 16);

  gtk_container_add(GTK_CONTAINER(box), toggle1);
  gtk_box_pack_start (GTK_BOX (top_box), box, TRUE, TRUE, 8);

  g_signal_connect (toggle1, "toggled", G_CALLBACK (change_source_cb), NULL);
#endif

  /*
  ** Camera Video Widget
  */
  video_window = gtk_drawing_area_new ();
  gtk_widget_set_size_request(video_window, 960, 540);
  gtk_box_pack_start (GTK_BOX (main_box), video_window, TRUE, TRUE, 0);
  g_signal_connect (video_window, "realize", G_CALLBACK (video_widget_realize_cb), NULL);
  gtk_widget_set_double_buffered (video_window, FALSE);

  /*
  ** Bottom box Widget
  */
  bottom_box = gtk_box_new(FALSE, 1);
  gtk_box_set_homogeneous(bottom_box, TRUE);
  gtk_box_pack_start (GTK_BOX (main_box), bottom_box, TRUE, TRUE, 0);
  
  // Label
  bottom_label1 = gtk_label_new ("Camera is not initilzed...");
  gtk_misc_set_alignment (GTK_MISC (bottom_label1), 0.05, 0.5);
  gtk_box_pack_start (GTK_BOX (bottom_box), bottom_label1, TRUE, TRUE, 0);
}

static void create_pipeline()
{
  close_pipeline();

  // gst pipeline for camera
	pipeline = gst_pipeline_new(NULL);

  if(b_mode)
  {
    src = gst_element_factory_make("v4l2src", "src");
  }
  else
  {
    src = gst_element_factory_make("videotestsrc", "src");
  }
  sink = gst_element_factory_make("xvimagesink", "sink");
  gst_bin_add_many(GST_BIN(pipeline), src, sink, NULL);
  if(gst_element_link(src, sink) != TRUE)
  {
    g_printerr("Elements could not be linked.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  // set up sync handler for setting the xid once the pipeline is started
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_set_sync_handler (bus, (GstBusSyncHandler) bus_sync_handler, NULL, NULL);
  gst_bus_add_watch (bus, bus_callback, NULL);
  gst_object_unref (bus);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
}

static void close_pipeline()
{
  if (pipeline != NULL)
  {
    gboolean res = gst_element_send_event(pipeline, gst_event_new_eos());

    // do {
    //   g_usleep (1000*1000);
    //   g_warning("waiting until EOS done!");
    // } while (!is_eos_done);

    is_eos_done = FALSE;

    g_usleep (10*1000*1000);

    g_print("pipeline is removing...\n\r");
    gst_object_unref(pipeline);

    if(!res) {
      g_error("Error occurred! EOS signal cannot be sent!\n\r");
    }
    else {
      g_print("EOS signal was sent!\n\r");
    }
  }
}

int main (int argc, char **argv)
{
  gtk_init(&argc, &argv);
	gst_init (&argc, &argv);

  create_gui();

  // show the GUI
  gtk_widget_show_all (app_window);

  // to be realized if its corresponding GdkWindow has been created. 
  gtk_widget_realize (video_window);

  create_pipeline();

  // we should have the XID/HWND now
  g_assert (video_window_handle != 0);

  gtk_main ();
}