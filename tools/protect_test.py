
FILE_DIR = pathlib.Path(__file__).parent.absolute()
bootloader = FILE_DIR / '..' / 'bootloader'

def cbc_encryption(firmware):
    #Firmware format is just the firmware. The size and release message are to be added later. 
    #mode is aes-128
    #How do i read the key from the text file? Which key will it be?
    #Append IVs to the end of the ciphertext
    #128 bit IV
    with open(bootloader/'secret_build_output.txt','rb') as fp:
        key = fp.readlines()  #Returns a list. Each line is an index in the list.
        print(key, "\n")
        print(key[-2], "\n")
        print(key[-1], "\n")
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