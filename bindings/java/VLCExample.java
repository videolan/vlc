import org.videolan.jvlc.JVLC;
import org.videolan.jvlc.VLCException;


public class VLCExample 
{

    public static void main( String[] args )
    {
    	boolean videoInput = false;
        JVLC jvlc = new JVLC(args);
        try {
        jvlc.playlist.add("file://" + System.getProperty( "user.dir" ) + "/a.avi", "a.avi");
    	jvlc.playlist.add("file://" + System.getProperty( "user.dir" ) + "/a.mp3", "a.mp3");
        jvlc.playlist.play( -1 , null );
        } catch (VLCException e) {
        	e.printStackTrace();
        }
        while (! jvlc.isInputPlaying()) ;
	while (! jvlc.hasVout() );        
        
	
	
	// testing vout functionalities

        try {
        	Thread.sleep(2500);
        	if (jvlc.hasVout()) videoInput = true;
		} catch (InterruptedException e) {
				e.printStackTrace();
		}

        if (videoInput) {
        	try {
        		System.out.print(jvlc.video.getWidth());
        		System.out.print("x");
        		System.out.println(jvlc.video.getHeight());
        	} catch (VLCException e) {
        		e.printStackTrace();
        	}
        }
        try 
        {
        	if (videoInput) {
        		System.out.print("Fullscreen... ");        	
        		jvlc.video.setFullscreen(true);
        		Thread.sleep(3000);
        		System.out.println("real size.");
                jvlc.video.setFullscreen(false);
            	System.out.print("Taking snapshot... ");
            	jvlc.video.getSnapshot( System.getProperty( "user.dir" ) + "/snap.png");
            	System.out.println("taken. (see " + System.getProperty( "user.dir" ) + "/snap.png )");
        		Thread.sleep(2000);
                System.out.println("Resizing to 300x300");
                jvlc.video.setSize(300, 300);
                                
        	}
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
        }
        
    	System.out.println("Everything fine ;)");
    	System.out.println("Playing next item");
    	try {
    		jvlc.playlist.next();
    	} catch (VLCException e) {
    		e.printStackTrace();
    	}
    	
    	try {
    		Thread.sleep(3000);
    	} catch (InterruptedException e) {
    		e.printStackTrace();
    	}
    	jvlc.destroy();
        return;
    }
}

