import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.Callback;
import com.sun.jna.Structure;

import java.util.Timer;
import java.util.TimerTask;

public class HelloInContext {
    private static Timer timer = new Timer();
    private static Pointer context = null;

    public interface GstTest extends Library {
	public Pointer allocate();
	public int setup(String source, String sink, Pointer context);
	public int start(Pointer context);
	public int stop(Pointer context);
    }

    public static void main(String argv[]) {
	GstTest gst = (GstTest)Native.loadLibrary("hello_in_context", GstTest.class);
	context = gst.allocate();
	if(context != null) {
	    String source = "rtmp://192.168.1.114:1935/yanked/stream-fancy";
	    String sink = "rtmp://192.168.1.114:1935/yanked/stream-videotestsrc";
	    System.out.println("Got the context pointer");
	    int status = gst.setup(source, sink, context);
	    System.out.println("Returned successfully");
	    if(status == 0) {
		System.out.println("Scheduling a timer");
		timer.schedule(new GstTimerTask(gst), 30000);
		status = gst.start(context);
	    }
	}
	else {
	    System.out.println("Didn't get it");
	}
	timer.cancel();
	System.out.println("Exiting");
    }

    public static class GstTimerTask extends TimerTask {
    	private GstTest gst;

    	public GstTimerTask(GstTest test) {
    	    this.gst = test;
    	}
    	public void run() {
    	    System.out.println("Running the timer");
    	    int status = this.gst.stop(context);
    	    System.out.println("Came out, didn't freeze");
    	}
    }
}
