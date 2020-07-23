# design-challenge-the-bee-team
design-challenge-the-bee-team created by GitHub Classroom

Protocol Overview:
Keys are generated in the bl_build tool, and are stored in the secret_build_output.txt file. 

The firmware protect tool takes in firmware, version number, and the release message. It encrypts the firmware, and creates a firmware blob that contains the encrypted firmware, 
the IV, and the release message.
It creates a larger data blob that has the metadata, and hmac of the metadata, then it alternates with 32 byte chunks of data and their hmacs. An hmac of all of the data is 
added at the very end.

The firmware update tool sends the metadata and its hmac, then the large data blob in 64 byte frames, then the general hmac over to the bootloader.

The bootloader receives the metadata, verifies the hmac, then starts reading in frames, and verifying the hmac. (This part is still not completely done)
(basically if we can we'll fit all the data into memory, check w the big hmac, then decrypt, then write to flash in pages)
