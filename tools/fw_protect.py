"""
Firmware Bundle-and-Protect Tool

"""
import argparse
import struct

from Crypto.Cipher import AES
from Crypto.Util.Padding import pad
from Crypto.Hash import HMAC, SHA256

def protect_firmware(infile, outfile, version, message):
    # Load firmware binary from infile
    with open(infile, 'rb') as fp:
        firmware = fp.read()

    #encrypts the firmware w cbc mode aes-128, adds an iv to the end
    enc_firmware_iv = cbc_encryption(firmware)
    
    # Pack version and size (of only the firmware!) into two little-endian shorts
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
    
    #appends an hmac of all the *data* onto the very end of the entire firmware blob
    firmware_blob += hmac_generation(firmware_iv_message)
    
    # Write firmware blob to outfile
    with open(outfile, 'wb+') as outfile:
        outfile.write(firmware_blob)
    
    

def cbc_encryption(firmware):
    #Firmware format is just the firmware. The size and release message are to be added later. 
    #mode is aes-128
    #How do i read the key from the text file? Which key will it be?
    #Append IVs to the end of the ciphertext
    #128 bit IV
    with open('secret_build_output.txt','rb') as fp:
        key = fp.readlines()  #Returns a list. Each line is an index in the list.
        key = key[-2]  #key should be 16 bytes long. 
        #The keys are generated with one line being CBC and the next line being HMAC
        #The CBC key will be the second to last item in the list.
        key = key.rstrip()  #removes any possible newlines 
        
    cipher = AES.new(key, AES.MODE_CBC)  #makes a cipher object with a random IV
    
    ciphertext = cipher.encrypt(pad(firmware, AES.block_size)) 
    #encrypts the firmware and pads it so the firmware length is a multiple of 16 bytes
    
    ciphertext = bytearray(ciphertext)
    iv = bytearray(cipher.iv)
    #makes these into bytearrays so they can be concatenated together
    
    final_encrypt = ciphertext + iv  
    #The final encryption to be returned
    #The firmware sends the metadata size, so the IV will be easily distinguished
    
    return final_encrypt  #returns CBC encryption
    
def hmac_generation(input):
    
    #reading in a key from the secret file
    with open("secret_build_output.txt", "rb") as f:
        key_list = f.readlines()
    key = key_list[1].rstrip()
    
    #generates a new hmac object
    h = HMAC.new(key, digestmod=SHA256)
    
    #makes an hmac for the unencrypted metadata and the encrypted firmware
    h.update(input)
    
    
    
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
