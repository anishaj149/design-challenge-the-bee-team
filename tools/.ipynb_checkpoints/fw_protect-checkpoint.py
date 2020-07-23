"""
Firmware Bundle-and-Protect Tool

"""
import argparse
import struct

from crypto.Util.Padding import pad
from Crypto.Hash import HMAC, SHA256

def protect_firmware(infile, outfile, version, message):
    # Load firmware binary from infile
    with open(infile, 'rb') as fp:
        firmware = fp.read()

    #encrypts the firmware w cbc mode aes-128, adds an iv to the end
    enc_firmware_iv = cbc_encryption(firmware)
    
    # Pack version and size into two little-endian shorts
    metadata = struct.pack('<HH', version, len(firmware))
    
    # Append null-terminated release message to end of firmware
    firmware_iv_message = enc_firmware_iv + message.encode() + b'\00'

    firmware_iv_message = pad(firmware_iv_message, 32)

    # Start the firmware blob with the metadata and the metadata's hmac  
    firmware_blob = metadata + hmac_generation(metadata)
    
    
    #takes 32 bytes of data, generates an hmac, and appends both to firmware_blob  
    for i in range(0, len(firmware_iv_message), 32)
        firmware_blob += firmware_iv_message[i, i+32]
        firmware_blob += hmac_generation(firmware_iv_message[i, i+32])
    
    
    # Write firmware blob to outfile
    with open(outfile, 'wb+') as outfile:
        outfile.write(firmware_blob)

def cbc_encryption():
    pass
def hmac_generation(metadata, ciphertext):
    
    #reading in a key from the secret file
    with open("secret_build_output.txt", "rb") as f:
        key_list = f.readlines()
    key = key_list[1].rstrip()

    
    
    
    #generates a new hmac object
    h = HMAC.new(key, digestmod=SHA256)
    
    #makes an hmac for the unencrypted metadata and the encrypted firmware
    h.update(metadata)
    h.update(ciphertext)
    
    
    #returns that hmac
    return h.digest()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Firmware Update Tool')
    parser.add_argument("--infile", help="Path to the firmware image to protect.", required=True)
    parser.add_argument("--outfile", help="Filename for the output firmware.", required=True)
    parser.add_argument("--version", help="Version number of this firmware.", required=True)
    parser.add_argument("--message", help="Release message for this firmware.", required=True)
    args = parser.parse_args()

    protect_firmware(infile=args.infile, outfile=args.outfile, version=int(args.version), message=args.message)
