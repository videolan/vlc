public class JVLCVarVariable extends JVLCVariable {

    public JVLCVarVariable(String name, JVLCVarValue value) {
	super(name);
	this.value = value;
    }

    public JVLCVarValue getVarValue() {
	return (JVLCVarValue)value;
    }

}
