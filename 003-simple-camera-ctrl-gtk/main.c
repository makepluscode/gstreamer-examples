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

static GstBusSyncReply bus_sync_handler (GstBus * bus, GstMessage * message, gpointer user_data)
{
  g_print("bus_sync_handler called!\n");
  // ignore anything but 'prepare-window-handle' element messages
  if (!gst_is_video_overlay_prepare_window_handle_message (message))
  {
    g_print("bus_sync_handler : video_overlay is not prepared yet!\n");
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

static void change_source_cb (GtkWidget * widget, gpointer data)
{
  g_print("change_source_cb called!\n");
}

static void create_gui()
{
  /*
  ** Windows
  */

  app_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(app_window), "Camera Preview");
  gtk_window_set_default_size(GTK_WINDOW(app_window), 960, 700);
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
  GtkWidget *label = gtk_label_new ("You can select a source between video and camera.");
  gtk_misc_set_alignment (GTK_MISC (label), 0.1, 0.5);
  gtk_box_pack_start (GTK_BOX (top_box), label, TRUE, TRUE, 0);
  
  GtkWidget *radio_box = gtk_box_new(FALSE, 5);
  GtkWidget *frame = gtk_frame_new ("Select source");
  gtk_frame_set_label_align(frame, 0.04, 0.5);

  top_radio1 = gtk_radio_button_new_with_label (NULL, "Camera");
  gtk_box_pack_start (GTK_BOX (radio_box), top_radio1, TRUE, TRUE, 4);
  GSList *group = gtk_radio_button_get_group (GTK_RADIO_BUTTON(top_radio1));
  top_radio2 = gtk_radio_button_new_with_label (group, "Video");
  gtk_box_pack_start (GTK_BOX (radio_box), top_radio2, TRUE, TRUE, 8);

  gtk_container_add (GTK_CONTAINER (frame), radio_box);

  gtk_box_pack_start (GTK_BOX (top_box), frame, TRUE, TRUE, 8);
  gtk_widget_set_size_request(frame, 0, 80);

  g_signal_connect(G_OBJECT(top_radio1), "camera", G_CALLBACK(change_source_cb), NULL);
  g_signal_connect(G_OBJECT(top_radio2), "video", G_CALLBACK(change_source_cb), NULL);

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

int main (int argc, char **argv)
{
  gtk_init(&argc, &argv);
	gst_init (&argc, &argv);

  create_gui();

  // show the GUI
  gtk_widget_show_all (app_window);

  // to be realized if its corresponding GdkWindow has been created. 
  gtk_widget_realize (video_window);

  // we should have the XID/HWND now
  g_assert (video_window_handle != 0);

  // gst pipeline for camera
	pipeline = gst_pipeline_new(NULL);
  src = gst_element_factory_make("videotestsrc", "src");          
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
  gst_object_unref (bus);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  gtk_main ();
}