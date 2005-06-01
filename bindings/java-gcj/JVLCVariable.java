public abstract class JVLCVariable {
    String name;
    Object value;

    public JVLCVariable(String name) {
	this.name = name;
    }

    public String getName() {
	return name;
    }

    public Object getValue() {
	return value;
    }

    public void setValue(Object value) {
	this.value = value;
    }
	
}
