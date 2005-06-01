public class JVLCTimeVariable extends JVLCVariable {

    public JVLCTimeVariable(String name, long value) {
	super(name);
	this.value = new Long(value);
    }

    public long getTimeValue() {
	return ((Long)value).longValue();
    }

}
