import org.videolan.jvlc.AudioIntf;
import org.videolan.jvlc.JVLC;
import org.videolan.jvlc.VLCException;
import org.videolan.jvlc.listener.VolumeListener;


public class VLCExample
{

    public static void main(String[] args) throws InterruptedException
    {
        System.out.println("== Starting VLCExample ==");
        boolean videoInput = false;
        if (args.length == 0)
        {
            System.out.print("Creating a JVLC instance without args");
        }
        else
        {
            System.out.println("Creating a JVLC instance with args: ");
            for (int i = 0; i < args.length; i++)
            {
                System.out.println(i + ") " + args[i]);
            }
        }
        JVLC jvlc = new JVLC(args);
        System.out.println("... done.");

        try
        {
            // jvlc.playlist.add("file://" + System.getProperty( "user.dir" ) + "/a.avi", "a.avi");
            jvlc.playlist.add("file:///home/little/a.avi", "a.avi");
            // jvlc.playlist.add("file://" + System.getProperty( "user.dir" ) + "/a.mp3", "a.mp3");
            jvlc.playlist.play(-1, null);
        }
        catch (VLCException e)
        {
            e.printStackTrace();
        }

        while (! jvlc.isInputPlaying())
        {
            Thread.sleep(100);
        }
        while (! jvlc.hasVout() )
        {
            Thread.sleep(100);
        }

        // testing vout functionalities
        Thread.sleep(2500);
        if (jvlc.hasVout())
        {
            videoInput = true;
        }
        
        if (videoInput)
        {
            try
            {
                System.out.print(jvlc.video.getWidth());
                System.out.print("x");
                System.out.println(jvlc.video.getHeight());
            }
            catch (VLCException e)
            {
                e.printStackTrace();
            }
        }
        try
        {
            if (videoInput)
            {
                System.out.print("Fullscreen... ");
                jvlc.video.setFullscreen(true);
                Thread.sleep(3000);
                System.out.println("real size.");
                jvlc.video.setFullscreen(false);
                System.out.print("Taking snapshot... ");
                jvlc.video.getSnapshot(System.getProperty("user.dir") + "/snap.png",0,0);
                System.out.println("taken. (see " + System.getProperty("user.dir") + "/snap.png )");
                Thread.sleep(2000);
                System.out.println("Resizing to 300x300");
                jvlc.video.setSize(300, 300);

            }
            jvlc.audio.addVolumeListener(new VolumeListener()
            {
				public void volumeChanged() {
					System.out.println("====> From the listener: volume changed.");
				}
            });

            System.out.print("Muting...");
            jvlc.audio.setMute(true);
            Thread.sleep(3000);
            System.out.println("unmuting.");
            jvlc.audio.setMute(false);
            Thread.sleep(3000);
            System.out.println("Volume is: " + jvlc.audio.getVolume());
            System.out.print("Setting volume to 150... ");
            jvlc.audio.setVolume(150);
            System.out.println("done");
            System.out.println("== AUDIO INFO ==");
            int currentChannel = jvlc.audio.getChannel();
            System.out.println("Audio track number: " + jvlc.audio.getTrack());
            System.out.println("Audio channel info: " + jvlc.audio.getChannel());
            System.out.print("Setting left channel... ");
            jvlc.audio.setChannel(AudioIntf.LEFT_CHANNEL);
            System.out.println("done.");
            Thread.sleep(3000);
            System.out.print("Setting right channel... ");
            jvlc.audio.setChannel(AudioIntf.RIGHT_CHANNEL);
            System.out.println("done.");
            Thread.sleep(3000);
            System.out.print("Reverting to original channel");
            jvlc.audio.setChannel(currentChannel);
            System.out.println("done.");
            Thread.sleep(3000);
            System.out.println("INPUT INFORMATION");
            System.out.println("-----------------");
            System.out.println("Total length   (ms) :\t" + jvlc.input.getLength());
            System.out.println("Input time     (ms) :\t" + jvlc.input.getTime());
            System.out.println("Input position [0-1]:\t" + jvlc.input.getPosition());
            if (videoInput)
                System.out.println("Input FPS          :\t" + jvlc.input.getFPS());

        }

        catch (Exception e)
        {
            System.out.println("Something was wrong. I die :(.");
            jvlc.destroy();
            e.printStackTrace();
            System.exit(0);
        }

        System.out.println("Everything fine ;)");
        System.out.println("Playing next item");
        try
        {
            jvlc.playlist.next();
        }
        catch (VLCException e)
        {
            e.printStackTrace();
        }

        Thread.sleep(3000);

        jvlc.destroy();
        return;
    }
}
