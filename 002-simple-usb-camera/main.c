#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

static guintptr video_window_handle = 0;
 
static GstElement *pipeline, *src, *sink;
static GstBus *bus;

GtkWidget *app_window, *video_window, *main_box, *label1;

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

    gtk_label_set_text(GTK_LABEL (label1), "camera is previewing...");
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

static void create_gui()
{
  // Window
  app_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(app_window), "Camera Preview");
  gtk_window_set_default_size(GTK_WINDOW(app_window), 960, 600);
  gtk_window_set_position(GTK_WINDOW(app_window), GTK_WIN_POS_CENTER);
  g_signal_connect(G_OBJECT(app_window), "destroy", G_CALLBACK(gtk_main_quit), NULL);

  // Main Widget (Label + Video)
  main_box = gtk_box_new(TRUE, 1);
  gtk_container_set_border_width(GTK_CONTAINER(main_box), 1);
  gtk_container_add(GTK_CONTAINER(app_window), main_box);
  
  // Label Widget (Left)
  label1 = gtk_label_new ("Camera is not initilzed...");
  //gtk_widget_set_size_request(label, 100, 50);
  gtk_box_pack_start (GTK_BOX (main_box), label1, TRUE, TRUE, 0);
  
  // Camera Video Widget (Right)  
  video_window = gtk_drawing_area_new ();
  gtk_widget_set_size_request(video_window, 960, 540);
  gtk_box_pack_start (GTK_BOX (main_box), video_window, TRUE, TRUE, 0);
  g_signal_connect (video_window, "realize", G_CALLBACK (video_widget_realize_cb), NULL);
  gtk_widget_set_double_buffered (video_window, FALSE);
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
  src = gst_element_factory_make("v4l2src", "src");          
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