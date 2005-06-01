public class JVLCIntVariable extends JVLCVariable {


    public JVLCIntVariable(String name, int value) {
	super(name);
	this.value = new Integer(value);
    }

    public int getIntValue() {
	return ((Integer)value).intValue();
    }

}
