// WebSocket.js
// Define all Websocket stuff and command functions
// 
// Browser sends messages like "{"command":"save",...}
// ESP send messages like "{"event":"error", ... }

var t2000=new Date("2000-01-01T00:00:00.000Z")
var esp_time=0; //in UTC seconds since 2000
var esp_time_update = Date.now(); //JS-Time
var limit_low=1000;
var limit_high=1400;
var socket_id=-1;
var sampling_int=10;
var co2_array_len=10;
var frame_size=10;

var connection = new WebSocket('ws://' + location.hostname + ':81/',
		[ 'arduino' ]);

connection.onopen = function() {
	//on open we could send the time to the ESP.
	//here we do nothing
	var msg = {
			command : "getAll",
			time : Math.floor((Date.now()-t2000.getTime())/1000)
	};
	console.log(msg);
	connection.send(JSON.stringify(msg))
};


connection.onerror = function(error) {
	console.log('WebSocket Error ', error);
};


connection.onmessage = function(e) {
	console.log('Server:'+e.data);
	var msg = JSON.parse(''+e.data);
	switch(msg.event){
	case "error":
		$('#error').html(msg.text)
		$('#error_tr').show();
		setTimeout(function(){
		    $('#error_tr').fadeOut(500)
		}, 3000)
		$(".config").prop('disabled', false); //enable inputs
		break
	case "msg":
		$('#msg').html(msg.text)
		$('#msg_tr').show();
		setTimeout(function(){
		    $('#msg_tr').fadeOut(500)
		}, 3000)
		break
	case "conf":
		$( "#zerocal" ).prop( "checked", false );
		if(msg.socket_id>=0) socket_id=msg.socket_id;
		$('#ssid_h1').html(msg.name)
		$('title').html(msg.ssid)
		$('#esp_version').html(msg.esp_version)
		$('#name').val(msg.name)
		$('#ssid').val(msg.ssid)
		$('#password').val(msg.password)
		$('#ampel_mode').val(msg.ampel_mode)
		$('#mode').val(msg.mode)
		if(msg.mode) $(".bayeos").show()
		else $(".bayeos").hide()
		$('#autocalibration').val(msg.autocalibration)
		$('#low').val(msg.low)
		limit_low=msg.low
		$('#high').val(msg.high)
		limit_high=msg.high
		sampling_int=msg.sampling_int;
		co2_array_len=msg.co2_array_len;
		frame_size=msg.frame_size;
		$('#buffer_tag').html(Math.round(3600*24/sampling_int/co2_array_len))
		$('#blink').val(msg.blink)
		$('#ampel_start').val(msg.ampel_start)
		$('#ampel_end').val(msg.ampel_end)
		$('#brightness').val(msg.brightness)
		$('#bayeos_name').val(msg.bayeos_name)
		$('#bayeos_gateway').val(msg.bayeos_gateway)
		$('#bayeos_user').val(msg.bayeos_user)
		$('#bayeos_pw').val(msg.bayeos_pw)
		$('#admin_pw1').val("")
		$('#admin_pw2').val("")
		$(".config").prop('disabled', false); //enable inputs
		break;
	case "frame":
		for(var f in msg.frames){
			parseFrame(msg.frames[f])
		}
		chart.redraw()
		break;
	case "buffer":
		$('#buffer_max').html(msg.length/frame_size/2)
		var l=msg.write-msg.read
		if(l<0) l+=msg.length
		$('#buffer_neu').html(l/frame_size)
		l=msg.write-msg.end
		if(l<0) l+=msg.length
		$('#buffer_total').html(l/frame_size)
		break;
	case "data":
		shift = chart.series[0].data.length > 2000;
		chart.series[0].addPoint([ Date.now(), msg.co2 ], true, shift)
		var t;
		var diff=msg.co2-msg.co2_single;
		if(diff< -20) t="⇑";
		else if(diff<-10) t="⇗";
		else if(diff<10) t="⇒";
		else if(diff<20) t="⇘";
		else t="⇓";
		$('#co2').html(msg.co2);
		$('#trend').html(t);
		if(msg.co2<limit_low) set_col(0,0,1);
		else if(msg.co2>limit_high) set_col(1,0,0);
		else set_col(0,1,0);
		break;
	case "blink":
		if(msg.off) set_col(0,0,0);
		else set_col(1,0,0);
		break;
	//TODO: add further events to handle
	}
};

connection.onclose = function() {
	console.log('WebSocket connection closed');
	if(confirm('Verbindung unterbrochen! Soll die Seite neu geladen werden?'))
	   location.reload(); 
};

//Start Download
function download(){
	var fileFormat=parseInt($('#dl_format option:selected').val())
	var dlSize= parseInt($("input[name='dl_size']:checked").val())
	if(dlSize>0){
		dlSize=frame_size*Math.round(3600*parseInt($('#dl_size_s option:selected').val())*
		parseInt($('#dl_size_u option:selected').val())/sampling_int/co2_array_len)
		if(isNaN(dlSize) || dlSize<=0){
			alert('Invalid download size:' +dlSize)
			return;
		}
	}
	
	var url='/download?f='+fileFormat+'&s='+dlSize;
	//$("#dl").val("Download progress: 0%");
	
	var _iframe_dl = $('<iframe />')
	       .attr('src', url)
	       .hide()
	       .appendTo('body');	
}
//Save config to EEPROM
function saveConf() {
	var low = parseInt($('#low').val())
	var high = parseInt($('#high').val())
	var blink = parseInt($('#blink').val())

	if (isNaN(low) || isNaN(high) || isNaN(blink) ) {
		alert("Achtung: Die Grenzen müssen Ganzzahlen sein!")
		return
	}
	var msg = {
			command : "setConf",
			zerocal : (	$( "#zerocal" ).prop( "checked" )?1:0),
			name : $('#name').val(),
			ssid : $('#ssid').val(),
			password : $('#password').val(),
			mode : parseInt($('#mode').val()),
			autocalibration : parseInt($('#autocalibration').val()),
			low: low,
			high: high,
			blink: blink,
			ampel_start : parseInt($('#ampel_start').val()),
			ampel_end : parseInt($('#ampel_end').val()),
			ampel_mode : parseInt($('#ampel_mode').val()),
			brightness : parseInt($('#brightness').val()),
			bayeos_name : $('#bayeos_name').val(),
			bayeos_gateway : $('#bayeos_gateway').val(),
			bayeos_user : $('#bayeos_user').val(),
			bayeos_pw : $('#bayeos_pw').val(),
			admin_pw1 : $('#admin_pw1').val(),
			admin_pw2 : $('#admin_pw2').val(),
			admin_pw : $('#admin_pw').val(),
			
		};
	console.log(msg);
	connection.send(JSON.stringify(msg));
	$(".config").prop('disabled', true);		
}
