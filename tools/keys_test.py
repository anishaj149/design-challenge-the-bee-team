import secrets

def gen_keys():  #Have to generate one CBC key and one HMAC key
    cbc_key = secrets.token_bytes(16)  #generates a key of 16 bytes for CBC
    hmac_key = secrets.token_bytes(64) #Generates a key of 64 bytes for HMAC
    print(type(cbc_key))
    print(type(hmac_key))
    with open('secret_build_output.txt','wb') as fp:  #Opens the file which stores keys
        fp.write(cbc_key)  #Writes the cbc key
        fp.write(b'\n')    #Writes a newline to separate the keys
        fp.write(hmac_key) #Writes the hmac key
        fp.write(b'\n')  #writes a newline to separate from the next cbc key
        fp.write(b'hi')
        
    with open('secret_build_output.txt','rb') as fp:
        print(fp.read())
    return 'finished'
      