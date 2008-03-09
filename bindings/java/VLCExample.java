import org.videolan.jvlc.JVLC;
import org.videolan.jvlc.MediaDescriptor;
import org.videolan.jvlc.MediaInstance;
import org.videolan.jvlc.Playlist;
import org.videolan.jvlc.VLCException;
import org.videolan.jvlc.Video;
import org.videolan.jvlc.event.MediaInstanceListener;


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
        Playlist playlist = new Playlist(jvlc);
        System.out.println("... done.");
        
        MediaDescriptor mediaDescriptor = new MediaDescriptor(jvlc, "/home/carone/a.avi");
        MediaInstance mediaInstance = mediaDescriptor.getMediaInstance();

        mediaInstance.addListener(new MediaInstanceListener()
        {
            @Override
            public void endReached(MediaInstance mediaInstance)
            {
                System.out.println("Media instance end reached. MRL: " + mediaInstance.getMediaDescriptor().getMrl());
            }

            @Override
            public void paused(MediaInstance mediaInstance)
            {
                System.out.println("Media instance paused. MRL: " + mediaInstance.getMediaDescriptor().getMrl());
            }

            @Override
            public void played(MediaInstance mediaInstance)
            {
                System.out.println("Media instance played. MRL: " + mediaInstance.getMediaDescriptor().getMrl());
            }

            @Override
            public void positionChanged(MediaInstance mediaInstance)
            {
                // TODO Auto-generated method stub
            }

            @Override
            public void timeChanged(MediaInstance mediaInstance, long newTime)
            {
                System.out.println("new time: " + newTime);
            }
        });
        mediaInstance.play();
        
        // MediaInstance mediaInstance = playlist.getMediaInstance();
        //        
        // while (! mediaInstance. playlist.isPlaying())
        // {
        // Thread.sleep(100);
        // }
        // while (! jvlc.hasVout() )
        // {
        // Thread.sleep(100);
        // }

        // testing vout functionalities
        // Thread.sleep(2500);
        // if (jvlc.hasVout())
        // {
        // videoInput = true;
        // }
        //        
        // if (videoInput)
        // {
        try
        {
            Video video = new Video(jvlc);
            System.out.print(video.getWidth(mediaInstance));
            System.out.print("x");
            System.out.println(video.getHeight(mediaInstance));
            System.out.print("Fullscreen... ");
            video.setFullscreen(mediaInstance, true);
            Thread.sleep(3000);
            System.out.println("real size.");
            video.setFullscreen(mediaInstance, false);
            System.out.print("Taking snapshot... ");
            video.getSnapshot(mediaInstance, System.getProperty("user.dir") + "/snap.png", 0, 0);
            System.out.println("taken. (see " + System.getProperty("user.dir") + "/snap.png )");
            Thread.sleep(2000);
            System.out.println("Resizing to 300x300");
            video.setSize(300, 300);
        }
        catch (VLCException e)
        {
            e.printStackTrace();
        }

        // System.out.print("Muting...");
        // jvlc.audio.setMute(true);
        // Thread.sleep(3000);
        // System.out.println("unmuting.");
        // jvlc.audio.setMute(false);
        // Thread.sleep(3000);
        // System.out.println("Volume is: " + jvlc.audio.getVolume());
        // System.out.print("Setting volume to 150... ");
        // jvlc.audio.setVolume(150);
        // System.out.println("done");
        // System.out.println("== AUDIO INFO ==");
        // int currentChannel = jvlc.audio.getChannel();
        // System.out.println("Audio track number: " + jvlc.audio.getTrack());
        // System.out.println("Audio channel info: " + jvlc.audio.getChannel());
        // System.out.print("Setting left channel... ");
        // jvlc.audio.setChannel(AudioIntf.LEFT_CHANNEL);
        // System.out.println("done.");
        // Thread.sleep(3000);
        // System.out.print("Setting right channel... ");
        // jvlc.audio.setChannel(AudioIntf.RIGHT_CHANNEL);
        // System.out.println("done.");
        // Thread.sleep(3000);
        // System.out.print("Reverting to original channel");
        // jvlc.audio.setChannel(currentChannel);
        // System.out.println("done.");
        // Thread.sleep(3000);
        // System.out.println("INPUT INFORMATION");
        // System.out.println("-----------------");
        // System.out.println("Total length (ms) :\t" + jvlc.input.getLength());
        // System.out.println("Input time (ms) :\t" + jvlc.input.getTime());
        // System.out.println("Input position [0-1]:\t" + jvlc.input.getPosition());
        // if (videoInput)
        // System.out.println("Input FPS :\t" + jvlc.input.getFPS());
        //
        // }
        //
        // catch (Exception e)
        // {
        // System.out.println("Something was wrong. I die :(.");
        // jvlc.destroy();
        // e.printStackTrace();
        // System.exit(0);
        // }

        System.out.println("Everything fine ;)");
        System.out.println("Playing next item");
        // try
        // {
        // jvlc.playlist.next();
        // }
        // catch (VLCException e)
        // {
        // e.printStackTrace();
        // }
        //
        // Thread.sleep(3000);

        // jvlc.destroy();
        return;
    }
}
