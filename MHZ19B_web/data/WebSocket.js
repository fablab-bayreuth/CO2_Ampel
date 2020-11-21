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
		$('#esp_version').html(msg.esp_version)
		$('#name').val(msg.name)
		$('#ssid').val(msg.ssid)
		$('#password').val(msg.password)
		$('#mode').val(msg.mode)
		$('#autocalibration').val(msg.autocalibration)
		$('#low').val(msg.low)
		limit_low=msg.low
		$('#high').val(msg.high)
		limit_high=msg.high
		$('#blink').val(msg.blink)
		$('#ampel_start').val(msg.ampel_start)
		$('#ampel_end').val(msg.ampel_end)
		$('#bayeos_name').val(msg.bayeos_name)
		$('#bayeos_gateway').val(msg.bayeos_gateway)
		$('#bayeos_user').val(msg.bayeos_user)
		$('#bayeos_pw').val(msg.bayeos_pw)
		$(".config").prop('disabled', false); //enable inputs
		break;
	case "frame":
		for(var f in msg.frames){
			parseFrame(msg.frames[f])
		}
		$('#buffer_max').html(msg.length/10)
		var l=msg.write-msg.read
		if(l<0) l+=msg.length
		$('#buffer_neu').html(l/10)
		l=msg.write-msg.end
		if(l<0) l+=msg.length
		$('#buffer_total').html(l/10)

		break;
	case "data":
		$('#co2').html(msg.co2);
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
		dlSize=10*Math.round(3600*parseInt($('#dl_size_s option:selected').val())*
		parseInt($('#dl_size_u option:selected').val())/100)
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
			bayeos_name : $('#bayeos_name').val(),
			bayeos_gateway : $('#bayeos_gateway').val(),
			bayeos_user : $('#bayeos_user').val(),
			bayeos_pw : $('#bayeos_pw').val(),
			admin_pw : $('#admin_pw').val(),
			
		};
	console.log(msg);
	connection.send(JSON.stringify(msg));
	$(".config").prop('disabled', true);		
}
