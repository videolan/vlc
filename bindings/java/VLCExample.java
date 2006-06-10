import org.videolan.jvlc.JVLC;


public class VLCExample 
{

    public static void main( String[] args )
    {
    	boolean videoInput = false;
        JVLC jvlc = new JVLC();

        jvlc.playlist.add("file://" + System.getProperty( "user.dir" ) + "/a.avi", "a.avi");
    	jvlc.playlist.add("file://" + System.getProperty( "user.dir" ) + "/a.mp3", "a.mp3");
        jvlc.playlist.play( -1 , null );
        
        while (! jvlc.isInputPlaying()) ;
        
        // testing vout functionalities

        try {
        	Thread.sleep(500);
        	if (jvlc.hasVout()) videoInput = true;
		} catch (InterruptedException e) {
				e.printStackTrace();
		}

        if (videoInput) {
        	System.out.print(jvlc.getVideoWidth());
        	System.out.print("x");
        	System.out.println(jvlc.getVideoHeight());
        }
        try 
        {
        	if (videoInput) {
        		System.out.print("Fullscreen... ");        	
        		jvlc.setFullscreen(true);
        		Thread.sleep(3000);
        		System.out.println("real size.");
        		jvlc.setFullscreen(false);
            	System.out.print("Taking snapshot... ");
            	jvlc.getSnapshot( System.getProperty( "user.dir" ) + "/snap.png");
            	System.out.println("taken. (see " + System.getProperty( "user.dir" ) + "/snap.png )");
        	}
            System.out.print("Muting...");
            jvlc.setMute(true);
            Thread.sleep(3000);
            System.out.println("unmuting.");
            jvlc.setMute(false);
            Thread.sleep(3000);
            System.out.println("Volume is: " + jvlc.getVolume());
            System.out.print("Setting volume to 150... ");
            jvlc.setVolume(150);
            System.out.println("done");
            Thread.sleep(3000);
            System.out.println("INPUT INFORMATION");
            System.out.println("-----------------");
            System.out.println("Total length   (ms) :\t" + jvlc.getInputLength());
            System.out.println("Input time     (ms) :\t" + jvlc.getInputTime());
            System.out.println("Input position [0-1]:\t" + jvlc.getInputPosition());
            if (videoInput)
            	System.out.println("Input FPS          :\t" + jvlc.getInputFPS());
           
            
        }
        
        catch (Exception e) 
        {
        	System.out.println("Something was wrong. I die :(.");
            jvlc.destroy();
        }
        
    	System.out.println("Everything fine ;)");
    	System.out.println("Playing next item");
    	jvlc.playlist.next();
    	
    	try {
    		Thread.sleep(3000);
    	} catch (InterruptedException e) {
    		e.printStackTrace();
    	}
    	jvlc.destroy();
        return;
    }
}

