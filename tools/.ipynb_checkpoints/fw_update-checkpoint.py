#!/usr/bin/env python
"""
Firmware Updater Tool

A frame consists of two sections:
1. Two bytes for the length of the data section
2. A data section of length defined in the length section

[ 0x02 ]  [ variable ]
--------------------
| Length | Data... |
--------------------

In our case, the data is from one line of the Intel Hex formated .hex file

We write a frame to the bootloader, then wait for it to respond with an
OK message so we can write the next frame. The OK message in this case is
just a zero
"""

import argparse
import struct
import time

from serial import Serial

RESP_OK = b'\x00'
HMAC_SIZE = 32
FRAME_SIZE = 64 #firmware frame size plus indiv hmac for each frame

error_dct = {b'\x01':'Version is wrong', b'\x02': 'HMAC is not verifiable', b'\x03':'Not enough space in flash', b'\x04':'Release Message is too big'}

def send_metadata(ser, metadata, metadata_HMAC, debug=False):
    version, size, message = struct.unpack_from('<HHH', metadata)
    print(f'Version: {version}\nSize: {size} bytes\nRelease Message Size: {message}\n')

    # Handshake for update
    ser.write(b'U')
    
    print('Waiting for bootloader to enter update mode...')
    while ser.read(1).decode() != 'U':
        pass

    # Send size and version to bootloader.
    if debug:
        print(metadata)

    ser.write(metadata)
    
    # Wait for an OK from the bootloader.
    resp = ser.read()
    if resp != RESP_OK:
        raise RuntimeError("ERROR: Bootloader responded with {}".format(error_dct[resp]))
    
    ser.write(metadata_HMAC)
        
    # Wait for an OK from the bootloader.
    resp = ser.read()
    if resp != RESP_OK:
        raise RuntimeError("ERROR: Bootloader responded with {}".format(error_dct[resp]))
              
                           
def send_frame(ser, frame, debug=False):
    ser.write(frame)  # Write the frame...
        
    if debug:
        print(frame)

    resp = ser.read()  # Wait for an OK from the bootloader that hmac did verify      
    if resp != RESP_OK:
        raise RuntimeError("ERROR: Bootloader responded with {}".format(error_dct[resp]))#repr(resp)))
    

        
            
def main(ser, infile, debug):
    # Open serial port. Set baudrate to 115200. Set timeout to 2 seconds.
    with open(infile, 'rb') as fp:
        firmware_blob = fp.read()

    #splitting data up
    metadata = firmware_blob[:6] #contains version, size of data, and size of release message
    metadata_HMAC = firmware_blob[6: 6+ HMAC_SIZE] #need to send the hmac for the version for InTeGrItY for the metadata
    firmware_and_hmacs = firmware_blob[HMAC_SIZE + 6:-96] #each frame has its own hmac
    hmacs = firmware_blob[-96:]# hmac of half one, hmac of half two, hmac for the two hmacs --> to make sure order isn't messed with
    
    #sending metadata
    send_metadata(ser, metadata, metadata_HMAC, debug=debug)
              
    #sending the rest of the data
    for idx, frame_start in enumerate(range(0, len(firmware_and_hmacs), FRAME_SIZE)):
        data = firmware_and_hmacs[frame_start: frame_start + FRAME_SIZE]

        # Get length of data (frame).
        length = len(data)
        frame_fmt = '>H{}s'.format(length)

        # Construct frame.
        frame = struct.pack(frame_fmt, length, data)

        if debug:
            print("Writing frame {} ({} bytes)...".format(idx, len(frame)))

        send_frame(ser, frame, debug=debug)
        
        
    # Send a zero length payload to tell the bootloader to finish writing its page.
    ser.write(struct.pack('>H', 0x0000))
    
    resp = ser.read()
    if resp != RESP_OK:
        raise RuntimeError("ERROR: Bootloader responded with {}".format(error_dct[resp]))#repr(resp)))

    #Send hmacs for half 1 and 2 
    ser.write(hmacs[0:64])
    
    resp = ser.read()  # Wait for an OK from the bootloader that it was able to go to flash or that hmac did verify    
    time.sleep(0.1)
    
    if resp != RESP_OK:
        raise RuntimeError("ERROR: Bootloader responded with {}".format(error_dct[resp]))#repr(resp)))
    
    # send the HMAC of hmacs
    ser.write(hmacs[64:])
    
    resp = ser.read()  # Wait for an OK from the bootloader that it was able to go to flash or that hmac did verify    
    time.sleep(0.1)
    
    if resp != RESP_OK:
        raise RuntimeError("ERROR: Bootloader responded with {}".format(error_dct[resp]))#repr(resp)))
    
    print("Done writing firmware.")
    
    return ser


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Firmware Update Tool')

    parser.add_argument("--port", help="Serial port to send update over.",
                        required=True)
    parser.add_argument("--firmware", help="Path to firmware image to load.",
                        required=True)
    parser.add_argument("--debug", help="Enable debugging messages.",
                        action='store_true')
    args = parser.parse_args()

    print('Opening serial port...')
    ser = Serial(args.port, baudrate=115200, timeout=2)
    main(ser=ser, infile=args.firmware, debug=args.debug)


