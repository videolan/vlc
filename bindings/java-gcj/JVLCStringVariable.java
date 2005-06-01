public class JVLCStringVariable extends JVLCVariable {

    public JVLCStringVariable(String name, String value) {
	super(name);
	this.value = value;
    }

    public String getStringValue() {
	return (String)value;
    }

}
