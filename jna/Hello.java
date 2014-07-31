import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.Pointer;

public class Hello {
    public interface TestHello extends Library {
	public Pointer showGstVersion();
	public void videoTestSource(String source_url, String sink_url);
    }

    public static void main(String argv[]) {
	TestHello ctest = (TestHello)Native.loadLibrary("hello", TestHello.class);
	// for(int i = 0; i < 1000000; i++) {
	//     Pointer addressOfSomething = ctest.showGstVersion();
	//     String message = addressOfSomething.getString(0);
	//     Native.free(Pointer.nativeValue(addressOfSomething));
	//     System.out.println(i);
	// }
	String source = "rtmp://192.168.1.114:1935/yanked/stream-fancy";
	String sink = "rtmp://192.168.1.114:1935/yanked/stream-videotestsrc";
	ctest.videoTestSource(source, sink);
    }
}
