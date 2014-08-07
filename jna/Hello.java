import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.Callback;

// import java.util.Timer;
// import java.util.TimerTask;

import java.util.ArrayList;

public class Hello {
    private static Pointer loop = null;
    // private static Timer timer = new Timer();

    public interface TestHello extends Library {
    	public Pointer showGstVersion();
    	public void videoTestSource(String source_url, String sink_url, TestCallback cb);
    	public void stopStream(Pointer gLoop);
	public void sendArray(int size, String[] listy);
    }

    public interface TestCallback extends Callback {
    	public void invoke(Pointer gLoop);
    }

    public static void main(String argv[]) {
	final TestHello ctest = (TestHello)Native.loadLibrary("hello", TestHello.class);

	// String source = "rtmp://192.168.1.114:1935/yanked/stream-fancy";
	// String sink = "rtmp://192.168.1.114:1935/yanked/stream-videotestsrc";
	// TestCallback bs = new TestCallback() {
	// 	public void invoke(Pointer gLoop) {
	// 	    // System.out.println("The message is: " + message);
	// 	    System.out.println("Am invoked");
	// 	    loop = gLoop;
	// 	    timer.schedule(new GstTimerTask(ctest), 30000);
	// 	}
	//     };
 	// ctest.videoTestSource(source, sink, bs);
	// timer.cancel();
	ArrayList<String> testList = new ArrayList<String>();
	testList.add("test1");
	testList.add("test2");

	ctest.sendArray(testList.size(), testList.toArray(new String[testList.size()]));
	System.out.println("Returned from C");
    }

    // public static class GstTimerTask extends TimerTask {
    // 	private TestHello ctest;
    // 	public GstTimerTask(TestHello ctest) {
    // 	    this.ctest = ctest;
    // 	}
    // 	public void run() {
    // 	    System.out.println("Running the timer");
    // 	    this.ctest.stopStream(loop);
    // 	    System.out.println("Came out, didn't freeze");
    // 	}
    // }
}
