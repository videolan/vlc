import java.time.LocalTime;
import java.time.format.DateTimeFormatter;

public class AlarmClock {

    public static void main(String[] args) {
        // Set your alarm time here (HH:mm:ss format)
        String alarmTimeStr = "07:30:00";

        DateTimeFormatter formatter = DateTimeFormatter.ofPattern("HH:mm:ss");
        LocalTime alarmTime = LocalTime.parse(alarmTimeStr, formatter);

        System.out.println("Alarm set for: " + alarmTime);

        while (true) {
            LocalTime now = LocalTime.now();

            if (now.getHour() == alarmTime.getHour() &&
                now.getMinute() == alarmTime.getMinute() &&
                now.getSecond() == alarmTime.getSecond()) {

                System.out.println("⏰ Alarm ringing!");
                playSound(); // optional sound
                break;
            }

            try {
                Thread.sleep(1000); // check every second
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    // Optional sound method (works with .wav file)
    public static void playSound() {
        try {
            java.io.File soundFile = new java.io.File("alarm.wav");
            javax.sound.sampled.AudioInputStream audio =
                    javax.sound.sampled.AudioSystem.getAudioInputStream(soundFile);

            javax.sound.sampled.Clip clip =
                    javax.sound.sampled.AudioSystem.getClip();

            clip.open(audio);
            clip.start();

        } catch (Exception e) {
            System.out.println("Sound failed, but alarm triggered!");
        }
    }
}
