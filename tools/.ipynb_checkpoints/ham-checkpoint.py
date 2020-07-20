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

metadata = b'\x01\x11\x12\x03'
print(hmac_generation(metadata, ciphertext = b'thebeeteam'))
print(len('2ee889c96ecfceb65d4e5e4e487d76bf2efd372f77b8f5017e5b675be71b5262'))
