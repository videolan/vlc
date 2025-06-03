// A simple alarm clock for JAVA beginners
 
import java.util.*; 
import java.time.format.DateTimeFormatter;  
import java.time.LocalDateTime;
import java.text.*;
import java.lang.*;
public class Alarm_C{  
	public static void main(String[] args){  
		try{
		    Mythread t1=new Mythread();        	      
		    Scanner sc= new Scanner(System.in);
		    System.out.println("Enter the time that you want to get up in HH:mm format");
		    String s= sc.next();
		    System.out.println("Your alarm is now set for "+s+" !!");  
		    while(1==1){	//An always true condition.
			    String currentTime = new SimpleDateFormat("HH:mm").format(new Date());	//Fetching the current system time.
			    boolean x = currentTime.equals(s);	//Equating the correct time to the time entered by the user.
			    if(x==true){
				    System.out.println("Wake up..Wake up. Its a brand new Day.!!");
				    break;	//Using break to jump out of the loop as soon as the alarm rings.
			    }
			    else
				    continue;  //To keep the program running until the desired time is reached.
				
    		    }	
        }
        catch(Exception e){
            System.out.println("Ohh.. believe me, something's wrong");
           }
    }
}
