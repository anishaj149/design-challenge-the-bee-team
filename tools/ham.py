from Crypto.Hash import HMAC, SHA256

def hmac_generation(metadata, ciphertext):
#     with open("secret_build_output.txt", "rb") as f:
#         key_list = f.readlines()
#     key = key_list[1]

    
    key = b'0123456789012345678901234567890123456789012345678901234567890123'
    
    #generates a new hmac object
    h = HMAC.new(key, digestmod=SHA256)
    
    #makes an hmac for the unencrypted metadata and the encrypted firmware
    h.update(metadata)
    h.update(ciphertext)
    
    
    #returns that hmac
    return h.hexdigest()

meta = b'\x01\x02\x03\x04'
data = b'thebeeteam'
print(hmac_generation(meta, data))