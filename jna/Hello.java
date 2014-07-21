import com.sun.jna.Library;
import com.sun.jna.Native;

public class Hello {
    public interface TestHello extends Library {
	public void helloFromC();
	public void showGstVersion();
	public void parameterIn(String param);
	public String parameterOut(String param);
	public void videoTestSource();
    }

    public static void main(String argv[]) {
	TestHello ctest = (TestHello)Native.loadLibrary("hello", TestHello.class);
	ctest.helloFromC();
	ctest.showGstVersion();
	ctest.parameterIn("This is a test");
	System.out.println(ctest.parameterOut("This is a test"));
	ctest.videoTestSource();
    }
}
