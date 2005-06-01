public class JVLCVarValue {
    
    String name;
    int OID;
    
    public JVLCVarValue(String name, int OID) {
	this.name = name;
	this.OID = OID;
    }

    public String getName() {
	return name;
    }

    public int getOID() {
	return OID;
    }

}
