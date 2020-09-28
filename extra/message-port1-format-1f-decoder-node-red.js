/*

Name:   message-port1-format-1f-decoder-node-red.js

Function:
    Decode port 0x01 format 0x1f messages for Node-RED.

Copyright and License:
    See accompanying LICENSE file at https://github.com/mcci-catena/MCCI-Catena-PMS7003/

Author:
    Terry Moore, MCCI Corporation   September 2020

*/


/*

Name:   message-port1-format-1f-decoder-ttn.js

Function:
    Decode port 0x01 format 0x1f messages for TTN console.

Copyright and License:
    See accompanying LICENSE file at https://github.com/mcci-catena/MCCI-Catena-PMS7003/

Author:
    Terry Moore, MCCI Corporation   September 2020

*/

function DecodeU16(Parse) {
    var i = Parse.i;
    var bytes = Parse.bytes;
    var Vraw = (bytes[i] << 8) + bytes[i + 1];
    Parse.i = i + 2;
    return Vraw;
}

function DecodeUflt16(Parse) {
    var rawUflt16 = DecodeU16(Parse);
    var exp1 = rawUflt16 >> 12;
    var mant1 = (rawUflt16 & 0xFFF) / 4096.0;
    var f_unscaled = mant1 * Math.pow(2, exp1 - 15);
    return f_unscaled;
}

function DecodeSflt16(Parse) {
    var rawSflt16 = DecodeU16(Parse);

    // special case minus zero:
    if (rawSflt16 == 0x8000)
        return -0.0;

    // extract the sign.
    var sSign = ((rawSflt16 & 0x8000) != 0) ? -1 : 1;

    // extract the exponent
    var exp1 = (rawSflt16 >> 11) & 0xF;

    // extract the "mantissa" (the fractional part)
    var mant1 = (rawSflt16 & 0x7FF) / 2048.0;

    // convert back to a floating point number. We hope
    // that Math.pow(2, k) is handled efficiently by
    // the JS interpreter! If this is time critical code,
    // you can replace by a suitable shift and divide.
    var f_unscaled = sSign * mant1 * Math.pow(2, exp1 - 15);

    return f_unscaled;
}

function DecodeDiffPressure(Parse) {
    return DecodeSflt16(Parse) * 32768.0 / 60.0;
}

function DecodeI16(Parse) {
    var i = Parse.i;
    var bytes = Parse.bytes;
    var Vraw = (bytes[i] << 8) + bytes[i + 1];
    Parse.i = i + 2;

    // interpret uint16 as an int16 instead.
    if (Vraw & 0x8000)
        Vraw += -0x10000;

    return Vraw;
}

function DecodeV(Parse) {
    return DecodeI16(Parse) / 4096.0;
}

function Decoder(bytes, port) {
    // Decode an uplink message from a buffer
    // (array) of bytes to an object of fields.
    var decoded = {};

    if (! (port === null || port === 1))
        return null;

    var uFormat = bytes[0];
    if (! (uFormat === 0x1F))
        return null;

    // an object to help us parse.
    var Parse = {};
    Parse.bytes = bytes;
    // i is used as the index into the message. Start with the flag byte.
    Parse.i = 1;

    // fetch the bitmap.
    var flags = bytes[Parse.i++];

    if (flags & 0x1) {
        decoded.Vbattery = DecodeV(Parse);
    }

    if (flags & 0x2) {
        decoded.Vsystem = DecodeV(Parse);
    }

    if (flags & 0x4) {
        var iBoot = bytes[Parse.i++];
        decoded.Boot = iBoot;
    }

    if (flags & 0x8) {
        // we have temperature
        decoded.TemperatureC = DecodeI16(Parse) / 200;
    }

    if (flags & 0x10) {
        // we have differential pressure
        decoded.DifferentialPressure = DecodeDiffPressure(Parse);
    }

    return decoded;
}


/*

Node-RED function body.

Input:
    msg     the object to be decoded.  
    
            msg.payload_raw is taken
            as the raw payload if present; otheriwse msg.payload
            is taken to be a raw payload.

            msg.port is taken to be the LoRaWAN port nubmer.


Returns:
    This function returns a message body. It's a mutation of the
    input msg; msg.payload is changed to the decoded data, and
    msg.local is set to additional application-specific information.

*/

var bytes;

if ("payload_raw" in msg) {
    // the console already decoded this
    bytes = msg.payload_raw;  // pick up data for convenience
    // msg.payload_fields still has the decoded data from ttn
} else {
    // no console decode
    bytes = msg.payload;  // pick up data for conveneince
}

// try to decode.
var result = Decoder(bytes, msg.port);

if (result === null) {
    // not one of ours: report an error, return without a value,
    // so that Node-RED doesn't propagate the message any further.
    var eMsg = "not port 1/fmt 0x1F! port=" + msg.port.toString();
    if (port === 1) {
        if (Buffer.byteLength(bytes) > 0) {
            eMsg = eMsg + " fmt=" + bytes[0].toString();
        } else {
            eMsg = eMsg + " <no fmt byte>"
        }
    }
    node.error(eMsg);
    return;
}

// now update msg with the new payload and new .local field
// the old msg.payload is overwritten.
msg.payload = result;
msg.local =
    {
        nodeType: "Catena 4801 M311",
        platformType: "Catena 4801",
        radioType: "Murata",
        applicationName: "Differential pressure sensor"
    };

return msg;
