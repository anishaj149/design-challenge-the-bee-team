"""
Firmware Bundle-and-Protect Tool

"""
import argparse
import struct
import pathlib

from Crypto.Cipher import AES
from Crypto.Util.Padding import pad
from Crypto.Hash import HMAC, SHA256


FILE_DIR = pathlib.Path(__file__).parent.absolute()
bootloader = FILE_DIR / '..' / 'bootloader'

def protect_firmware(infile, outfile, version, message):
    # Load firmware binary from infile
    with open(infile, 'rb') as fp:
        firmware = fp.read()

    #encrypts the firmware w cbc mode aes-128, adds an iv to the end
    enc_firmware_iv = cbc_encryption(firmware)
    
    # Pack version, size of encrypted firmware, and length of the release message into three little-endian shorts
    metadata = struct.pack('<HHH', version, len(enc_firmware_iv) - 16, len(message))
    
    # Append null-terminated release message to end of firmware
    firmware_iv_message = enc_firmware_iv + message.encode() + b'\x00'

    firmware_iv_message = pad(firmware_iv_message, 32)

    # Start the firmware blob with the metadata and the metadata's hmac 
    firmware_blob = metadata + hmac_generation(metadata)
    
    #takes 32 bytes of data, generates an hmac, and appends both to firmware_blob  
    for i in range(0, len(firmware_iv_message), 32):
        firmware_blob += firmware_iv_message[i: i+32]
        firmware_blob += hmac_generation(firmware_iv_message[i: i+32])

        
    half = int(len(firmware_iv_message)/2)
    
    # Generate HMAC from first half of data
    hmac_1_2 = hmac_generation(firmware_iv_message[0:half])
    #generate HMAC from second half of data
    hmac_1_2 += hmac_generation(firmware_iv_message[half:])
    
    # Add part 1 and 2 HMACs to data
    firmware_blob += hmac_1_2
    
    # Generate an HMAC of the part 1 and 2 HMACs
    firmware_blob += hmac_generation(hmac_1_2)
    
    # Write firmware blob to outfile
    with open(outfile, 'wb+') as outfile:
        outfile.write(firmware_blob)
    
def cbc_encryption(firmware):
    #mode is aes-128
    
    #read in the cbc key
    with open(bootloader/'secret_build_output.txt','rb') as fp:
        key = fp.read()  
        key = key[0:16]   
        
        
    #makes a cipher object with a random IV
    cipher = AES.new(key, AES.MODE_CBC)  
    
    
    #encrypts the firmware and pads it so the firmware length is a multiple of 16 bytes
    ciphertext = cipher.encrypt(pad(firmware, AES.block_size)) 
    
    
    #concatenate ciphertext and iv together
    ciphertext = bytearray(ciphertext)
    iv = bytearray(cipher.iv)
    
    final_encrypt = ciphertext + iv  
    
    
    return final_encrypt 
    
    
def hmac_generation(input_thing):
    
    #reading in a key from the secret file
    with open(bootloader/'secret_build_output.txt', "rb") as f:
        key_list = f.read()
    key = key_list[16:48]
    
    #generates a new hmac object
    h = HMAC.new(key, digestmod=SHA256)
    
    #makes an hmac for the input
    h.update(input_thing)
    
    
    
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
