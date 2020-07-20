"""
Firmware Bundle-and-Protect Tool

"""
import argparse
import struct
from Crypto.Cipher import AES


def protect_firmware(infile, outfile, version, message):
    # Load firmware binary from infile
    with open(infile, 'rb') as fp:
        firmware = fp.read()

    #encrypts the firmware w cbc mode aes-128
    encrypted_firmware = cbc_encryption(firmware)
    
    # Pack version and size into two little-endian shorts
    metadata = struct.pack('<HH', version, len(firmware))
    
    
    #generates an hmac from the unencrypted metadata and the encrypted firmware
    hmac = hmac_generation(metadata, encrypted_firmware)
    
    
    # Append null-terminated message to end of firmware
    firmware_and_message = encrypted_firmware + message.encode() + b'\00'

   

    # Append firmware and message to metadata
    firmware_blob = metadata + firmware_and_message

    # Write firmware blob to outfile
    with open(outfile, 'wb+') as outfile:
        outfile.write(firmware_blob)

def cbc_encryption(firmware):
    #Firmware format is just the firmware. The size and release message are to be added later. 
    cipher = AES.new(key, AES.MODE_CBC)
    
    
    
def hmac_generation():
    pass

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Firmware Update Tool')
    parser.add_argument("--infile", help="Path to the firmware image to protect.", required=True)
    parser.add_argument("--outfile", help="Filename for the output firmware.", required=True)
    parser.add_argument("--version", help="Version number of this firmware.", required=True)
    parser.add_argument("--message", help="Release message for this firmware.", required=True)
    args = parser.parse_args()

    protect_firmware(infile=args.infile, outfile=args.outfile, version=int(args.version), message=args.message)
