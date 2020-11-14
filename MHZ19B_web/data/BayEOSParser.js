const BayEOS_DataFrame = 0x1;
const BayEOS_Command = 0x2;
const BayEOS_CommandResponse = 0x3;
const BayEOS_Message = 0x4;
const BayEOS_ErrorMessage = 0x5;
const BayEOS_RoutedFrame = 0x6;
const BayEOS_DelayedFrame = 0x7;
const BayEOS_RoutedFrameRSSI = 0x8;
const BayEOS_TimestampFrame = 0x9;
const BayEOS_BinaryFrame = 0xa;
const BayEOS_OriginFrame = 0xb;
const BayEOS_MillisecondTimestampFrame = 0xc;
const BayEOS_RoutedOriginFrame = 0xd;
const BayEOS_ChecksumFrame = 0xf;
const BayEOS_DelayedSecondFrame = 0x10;
const BayEOS_WithoutOffset = 0x20;
const BayEOS_ChannelNumber = 0x40;
const BayEOS_ChannelLabel = 0x60;
const BayEOS_DATATYP_MASK = 0x0f;
const BayEOS_OFFSETTYP_MASK = 0xf0;
function parseBayEOSFrame(frame) {
	var buffer = base64js.toByteArray(frame);
	var x = new DataView(buffer.buffer, 0);
	var f = {
		"cks" : 0,
		"origin" : "",
		"ts" : Date.now()
	};
	function parseDataFrame(offset) {
		if (x.getUint8(offset) != 1) {
			return
		}
		f.data = {};
		offset++;
		var data_type = (x.getUint8(offset) & BayEOS_DATATYP_MASK);
		var channel_type = (x.getUint8(offset) & BayEOS_OFFSETTYP_MASK);
		var channel = 0;
		if (channel_type == 0) {
			offset++;
			channel = x.getUint8(offset)
		}
		offset++;
		while (offset < x.byteLength - f.cks) {
			ch_label = "";
			value = 0;
			if (channel_type == BayEOS_ChannelLabel) {
				channel = x.getUint8(offset) + offset + 1;
				offset++;
				while (offset < (x.byteLength - f.cks) && offset < channel) {
					ch_label += String.fromCharCode(x.getUint8(offset));
					offset++
				}
			} else {
				if (channel_type == BayEOS_ChannelNumber) {
					channel = x.getUint8(offset);
					offset++
				} else
					channel++;
				ch_label = "CH" + channel
			}
			switch (data_type) {
			case 0x1:
				value = x.getFloat32(offset, 1);
				offset += 4;
				break;
			case 0x2:
				value = x.getInt32(offset, 1);
				offset += 4;
				break;
			case 0x3:
				value = x.getInt16(offset, 1);
				offset += 2;
				break;
			case 0x4:
				value = x.getInt8(offset);
				offset++;
				break
			}
			f.data[ch_label] = value
		}
	}
	function parse(offset) {
		var current_offset;
		switch (x.getUint8(offset)) {
		case BayEOS_DataFrame:
			data = parseDataFrame(offset);
			break;
		case BayEOS_RoutedFrame:
			if (f.origin.length)
				f.origin += "/";
			f.origin += "XBee" + x.getInt16(offset + 1, 1) + "/"
					+ x.getInt16(offset + 3, 1);
			parse(offset + 5);
			break;
		case BayEOS_RoutedFrameRSSI:
			if (f.origin.length)
				f.origin += "/";
			f.origin += "XBee" + x.getInt16(offset + 1, 1) + "/"
					+ x.getInt16(offset + 3, 1);
			f.rssi = x.getUint8(offset + 5);
			parse(offset + 6);
			break;
		case BayEOS_OriginFrame:
		case BayEOS_RoutedOriginFrame:
			if (x.getUint8(offset) != BayEOS_RoutedOriginFrame)
				f.origin = "";
			if (f.origin.length)
				f.origin += "/";
			offset++;
			current_offset = x.getUint8(offset);
			offset++;
			while (current_offset > 0) {
				f.origin += String.fromCharCode(x.getUint8(offset));
				offset++;
				current_offset--
			}
			parse(offset);
			break;
		case BayEOS_DelayedFrame:
			f.ts -= x.getUint32(offset + 1, 1);
			parse(offset + 5);
			break;
		case BayEOS_DelayedSecondFrame:
			f.ts -= x.getUint32(offset + 1, 1)*1000;
			parse(offset + 5);
			break;
		case BayEOS_TimestampFrame:
			f.ts = Date.UTC(2000, 0, 1, 0, 0, 0).valueOf() + x.getUint32(offset + 1, 1)*1000;
			parse(offset + 5);
			break;
		case BayEOS_Message:
		case BayEOS_ErrorMessage:
			offset++;
			f.ms = "";
			while (offset < x.byteLength - f.cks) {
				f.ms += String.fromCharCode(x.getUint8(offset));
				offset++
			}
			break;
		case BayEOS_ChecksumFrame:
			f.cks = 2;
			offset++;
			parse(offset);
			break;
		default:
			break
		}
	}
	parse(0);
	return f
}