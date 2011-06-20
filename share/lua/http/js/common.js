function format_time( s ){
    var hours	= Math.floor(s/3600);
    var minutes = Math.floor((s/60)%60);
    var seconds = Math.floor(s%60);
    hours		=	hours<10 ? "0"+hours : hours;
	minutes		=	minutes<10 ? "0"+minutes : minutes;
	seconds		=	seconds<10 ? "0"+seconds : seconds;
    return hours+":"+minutes+":"+seconds;
}
