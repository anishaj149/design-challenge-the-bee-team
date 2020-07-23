#!/usr/bin/env python
#Hope this creates a conflict
"""
Bootloader Build Tool

This tool is responsible for building the bootloader from source and copying
the build outputs into the host tools directory for programming.
"""
import argparse
import os
import pathlib
import shutil
import subprocess
import secrets

FILE_DIR = pathlib.Path(__file__).parent.absolute()


def copy_initial_firmware(binary_path):
    """
    Copy the initial firmware binary to the bootloader build directory
    Return:
        None
    """
    # Change into directory containing tools
    os.chdir(FILE_DIR)
    bootloader = FILE_DIR / '..' / 'bootloader'
    shutil.copy(binary_path, bootloader / 'src' / 'firmware.bin')


def make_bootloader():
    """
    Build the bootloader from source.

    Return:
        True if successful, False otherwise.
    """
    # Change into directory containing bootloader.
    bootloader = FILE_DIR / '..' / 'bootloader'
    os.chdir(bootloader)

    subprocess.call('make clean', shell=True)
   # status = subprocess.call('make',shell=True)  #Makes us able to pass in commands
    status = subprocess.call(f'make KEY={to_c_array(key)}', shell=True) #Makes us able to pass in commands make the key command anything you want. 
    
    # Return True if make returned 0, otherwise return False.
    return (status == 0)

def gen_keys():  #Have to generate one CBC key and one HMAC key
    cbc_key = secrets.token_bytes(16)  #generates a key of 16 bytes for CBC
    hmac_key = secrets.token_bytes(64) #Generates a key of 64 bytes for HMAC, 32 bytes?
    with open('secret_build_output.txt','wb') as fp:  #Opens the file which stores keys
        fp.write(cbc_key)  #Writes the cbc key
        fp.write(b'\n')    #Writes a newline to separate the keys
        fp.write(hmac_key) #Writes the hmac key
        fp.write(b'\n')  #writes a newline to separate from the next cbc key
        
        
def to_c_array(binary_string): #Converts binary string to readable form
    return "{" + ",".join([hex(c) for c in binary_string]) + "}"

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Bootloader Build Tool')
    parser.add_argument("--initial-firmware", help="Path to the the firmware binary.", default=None)
    args = parser.parse_args()
    if args.initial_firmware is None:
        binary_path = FILE_DIR / '..' / 'firmware' / 'firmware' / 'gcc' / 'main.bin'
    else:
        binary_path = os.path.abspath(pathlib.Path(args.initial_firmware))

    if not os.path.isfile(binary_path):
        raise FileNotFoundError(
            "ERROR: {} does not exist or is not a file. You may have to call \"make\" in the firmware directory.".format(
                binary_path))

    copy_initial_firmware(binary_path)
    make_bootloader()
    gen_keys()
