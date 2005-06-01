public class JVLCBoolVariable extends JVLCVariable {

    public JVLCBoolVariable(String name, int value) {
	super(name);
	if (value != 0) this.value = new Integer(1);
	else this.value = new Integer(0);
    }

    public int getBoolValue() {
	return ((Integer)value).intValue();
    }

}
