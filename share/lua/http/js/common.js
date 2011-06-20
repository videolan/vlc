var intv	=	0;
var ccmd	=	"";
function format_time( s ){
    var hours	= Math.floor(s/3600);
    var minutes = Math.floor((s/60)%60);
    var seconds = Math.floor(s%60);
    hours		=	hours<10 ? "0"+hours : hours;
	minutes		=	minutes<10 ? "0"+minutes : minutes;
	seconds		=	seconds<10 ? "0"+seconds : seconds;
    return hours+":"+minutes+":"+seconds;
}
function setIntv(){
	if(intv>0){
		intv++;
		setTimeout(setIntv,500);
	}else{
		intv=0
	}
	if(intv>5){
		var nt	=	0;
		switch(ccmd){
			case 'prev':
				nt	=	Math.max(0,$('#seekSlider').slider('value')-10);
				break;
			case 'next':
				nt	=	Math.max(0,$('#seekSlider').slider('value')+10);
				break;
		}
		switch(current_que){
			case 'main':
				sendCommand({'command':'seek','val':Math.round((nt/100)*$('#seekSlider').attr('totalLength')),plreload:false});
				break;
			case 'stream':
				sendVLMCmd('control Current seek '+nt);
				break;
		}
	}
}
