public class JVLCFloatVariable extends JVLCVariable {

    public JVLCFloatVariable(String name, float value) {
	super(name);
	this.value = new Float(value);
    }

    public float getFloatValue() {
	return ((Float)value).floatValue();
    }

}
