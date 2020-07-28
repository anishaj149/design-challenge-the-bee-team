# design-challenge-the-bee-team
design-challenge-the-bee-team created by GitHub Classroom

Protocol Overview:
Keys are generated in the bl_build tool, and are stored in the secret_build_output.txt file. 

The firmware protect tool takes in firmware, version number, and the release message. It encrypts the firmware, and creates a firmware blob that contains the encrypted firmware, 
the IV, and the release message.
<<<<<<< HEAD
It creates a larger data blob that has the metadata, and hmac of the metadata, then it alternates with 32 byte chunks of data and their hmacs. An hmac of all of the data is 
added at the very end.

The firmware update tool sends the metadata and its hmac, then the large data blob in 64 byte frames, then the general hmac over to the bootloader.

The bootloader receives the metadata, verifies the hmac, then starts reading in frames, and verifying the hmac. (This part is still not completely done)
(basically if we can we'll fit all the data into memory, check w the big hmac, then decrypt, then write to flash in pages)
=======
It creates a larger data blob that has the metadata, and hmac of the metadata. Then it alternates with 32 byte chunks of firmware blob and its hmac. 
Finally, the data is split into two parts and HMACS are generated for them. These final HMACS are appended to the end of the data blob. The HMACS are concatenated to preserve the correct order of the data frames, and one final HMAC is generated from the last two and appended to the end of the large data blob.

The firmware update tool sends the metadata and its hmac, then the large data blob in 64 byte frames, and finally the two HMACS over to the bootloader.

The bootloader receives the metadata, verifies the hmac, then starts reading in frames, and verifying the hmac of the frames. The data is loaded into memory, and the bootloader checks the three HMACS at the end. Finally, it decrypts the firmware, then writes it to flash in pages.
>>>>>>> 6265d956e4f288dda7b009a79830265d35b58d62
