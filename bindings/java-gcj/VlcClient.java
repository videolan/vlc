public class VlcClient {

    public static void main(String[] args) {

	JVLC vlc = new JVLC(args);
	System.out.println(vlc.getVersion());
	vlc.addInterface(true, true);
	vlc.die();
	vlc.cleanUp();
    }

}
