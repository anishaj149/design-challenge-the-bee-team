// Hardware Imports
#include "inc/hw_memmap.h" // Peripheral Base Addresses
#include "inc/lm3s6965.h" // Peripheral Bit Masks and Registers
#include "inc/hw_types.h" // Boolean type
#include "inc/hw_ints.h" // Interrupt numbers

// Driver API Imports
#include "driverlib/flash.h" // FLASH API
#include "driverlib/sysctl.h" // System control API (clock/reset)
#include "driverlib/interrupt.h" // Interrupt API

// Application Imports
#include "uart.h"
// For CBC decryption
#include "bearssl.h" 
#include <stdlib.h>
#include <string.h>

// HMAC Imports
#include "bearssl_hmac.h"


// Forward Declarations
void load_initial_firmware(void);
void load_firmware(void);
void boot_firmware(void);
long program_flash(uint32_t, unsigned char*, unsigned int);
int verify_hmac(unsigned int hmac_index, unsigned int data_index, unsigned int data_size);
int is_same(unsigned char* hmac, unsigned char* tmp, unsigned int data_size);
int decrypt_firmware(unsigned char* iv, unsigned char* data, unsigned short DATA_LEN);

// Firmware Constants
#define METADATA_BASE 0xFC00  // base address of version and firmware size in Flash
#define FW_BASE 0x10000  // base address of firmware in Flash

// FLASH Constants
#define FLASH_PAGESIZE 1024
#define FLASH_WRITESIZE 4


// Protocol Constants
#define OK    ((unsigned char)0x00)
#define ERROR_META ((unsigned char)0x01)
#define ERROR_HMAC ((unsigned char)0x02)
#define ERROR_FLASH ((unsigned char)0x03)
#define UPDATE ((unsigned char)'U')
#define BOOT ((unsigned char)'B')

// define HMAC Constants
#define HMAC_SIZE 32

// define IV Constants
#define IV_SIZE 16

// define Key Constants
#define CBC_KEY_LEN 16


// Firmware v2 is embedded in bootloader
extern int _binary_firmware_bin_start;
extern int _binary_firmware_bin_size;


// Device metadata
uint16_t *fw_version_address = (uint16_t *) METADATA_BASE;
uint16_t *fw_size_address = (uint16_t *) (METADATA_BASE + 2);
uint8_t *fw_release_message_address;

// Firmware Buffer
unsigned char data[FLASH_PAGESIZE*30+32];

// HMAC Buffer?
unsigned char hmac[HMAC_SIZE];

// IV Buffer?
unsigned char iv [IV_SIZE];


//Define keys
char CBC_KEY[] = CBC;
char HMAC_KEY[] = HMAC;

int main(void) {

  // Initialize UART channels
  // 0: Reset
  // 1: Host Connection
  // 2: Debug
  uart_init(UART0);
  uart_init(UART1);
  uart_init(UART2);

  // Enable UART0 interrupt
  IntEnable(INT_UART0);
  IntMasterEnable();
  
  load_initial_firmware();

  uart_write_str(UART2, "Welcome to the BWSI Vehicle Update Service!\n");
  uart_write_str(UART2, "Send \"U\" to update, and \"B\" to run the firmware.\n");
  uart_write_str(UART2, "Writing 0x20 to UART0 will reset the device.\n");
        
  int resp;
  while (1){
    uint32_t instruction = uart_read(UART1, BLOCKING, &resp);
    if (instruction == UPDATE){
      uart_write_str(UART1, "U");
      load_firmware();
    } else if (instruction == BOOT){
      uart_write_str(UART1, "B");
      boot_firmware();
    }
  }
}



/*
 * Load initial firmware into flash
 */
void load_initial_firmware(void) {

  if (*((uint32_t*)(METADATA_BASE+512)) != 0){
    /*
     * Default Flash startup state in QEMU is all zeros since it is
     * secretly a RAM region for emulation purposes. Only load initial
     * firmware when metadata page is all zeros. Do this by checking
     * 4 bytes at the half-way point, since the metadata page is filled
     * with 0xFF after an erase in this function (program_flash()).
     */
    return;
  }

  int size = (int)&_binary_firmware_bin_size;
  int *data = (int *)&_binary_firmware_bin_start;
    
  uint16_t version = 2; // is this supposed to be version 1?
  uint32_t metadata = (((uint16_t) size & 0xFFFF) << 16) | (version & 0xFFFF);
  program_flash(METADATA_BASE, (uint8_t*)(&metadata), 4);
  fw_release_message_address = (uint8_t *) "This is the initial release message.";
    
  int i = 0;
  for (; i < size / FLASH_PAGESIZE; i++){
       program_flash(FW_BASE + (i * FLASH_PAGESIZE), ((unsigned char *) data) + (i * FLASH_PAGESIZE), FLASH_PAGESIZE);
  }
  program_flash(FW_BASE + (i * FLASH_PAGESIZE), ((unsigned char *) data) + (i * FLASH_PAGESIZE), size % FLASH_PAGESIZE);
}

/*
 * Load the firmware into flash.
 */
void load_firmware(void)
{
    // declaring variables    
  int frame_length = 0;
  int read = 0;
  uint32_t rcv = 0;
  
  uint32_t data_index = 0;
  uint32_t page_addr = FW_BASE;
  uint32_t version = 0;
  uint32_t size = 0;

  // Get version.
  data[0] = uart_read(UART1, BLOCKING, &read);
  version = (uint32_t)data[0];
  data[1] = uart_read(UART1, BLOCKING, &read);
  version |= (uint32_t)data[1] << 8;

  // Get size.
  data[2] = uart_read(UART1, BLOCKING, &read);
  size = (uint32_t)data[2];
  data[3] = uart_read(UART1, BLOCKING, &read);
  size |= (uint32_t)data[3] << 8;

            
  // confirmation messages
  uart_write_str(UART2, "Received Firmware Version: ");
  uart_write_hex(UART2, version);
  nl(UART2);
  uart_write_str(UART2, "Received Firmware Size: ");
  uart_write_hex(UART2, size);
  nl(UART2);
    
  uart_write(UART1, OK); // send ok to update tool after receiving metadata
    
  // get HMAC of metadata
  // Get the number of bytes specified 
  data_index = 4; //need to start at 4 because of the metadata len
  for (int i = 0; i < HMAC_SIZE; ++i){
    data[data_index] = uart_read(UART1, BLOCKING, &read);
    data_index += 1;
  }

  //HMAC verification for metadata
  if(!verify_hmac(4, 0, 4)){
        uart_write(UART1, ERROR_HMAC); // Reject the firmware
        SysCtlReset(); // Reset device
        return;
    }

  // Compare to old version and abort if older (note special case for version 0).
  uint16_t old_version = *fw_version_address;  
  if (version != 0 && version < old_version) {
    uart_write(UART1, ERROR_META); // Reject the metadata.
    SysCtlReset(); // Reset device
    return;
  } else if (version == 0) {
    // If debug firmware, don't change version
    version = old_version;
  }
    
  // Write new firmware size and version to Flash
  // Create 32 bit word for flash programming, version is at lower address, size is at higher address
  uint32_t metadata = ((size & 0xFFFF) << 16) | (version & 0xFFFF);
  program_flash(METADATA_BASE, (uint8_t*)(&metadata), 4);
  fw_release_message_address = (uint8_t *) (FW_BASE + size);

  uart_write_str(UART2, (char *) fw_release_message_address);
    
  uart_write(UART1, OK); // Acknowledge the metadata's HMAC

  data_index = 0; //resetting data[]

  /* Loop here until you can get all your characters and stuff */
  while (1) {

    // Get two bytes for the length.
    rcv = uart_read(UART1, BLOCKING, &read);
    frame_length = (int)rcv << 8;
    rcv = uart_read(UART1, BLOCKING, &read);
    frame_length += (int)rcv;

    // Write length debug message
    uart_write_hex(UART2,(unsigned char)rcv);
    nl(UART2);

    //if the done message is sent, finish
    if (frame_length == 0) {
        uart_write(UART1, OK);
        
        //get HMACs Part 1 and 2 of entire data 
        for (int i = 0; i < HMAC_SIZE*2; ++i){
        data[data_index] = uart_read(UART1, BLOCKING, &read);
        data_index += 1;
        }
        
        // get the halfway point index
        int half = (data_index - HMAC_SIZE*2) / 2;
        
        // verify hmac 1 and hmac 2
        if(!verify_hmac(data_index - HMAC_SIZE*2, 0, half) || !verify_hmac(data_index - HMAC_SIZE, half, data_index - HMAC_SIZE*2 - half)){ //beginning of data, beginning of hmac
            uart_write(UART1, ERROR_HMAC); // Reject the firmware
            SysCtlReset(); // Reset device
            return;
        }
        uart_write(UART1, OK); // Acknowledge the frame.        
        
        // get HMAC of HMACs Part1 and Part2
        for (int i = 0; i < HMAC_SIZE; ++i){
        data[data_index] = uart_read(UART1, BLOCKING, &read);
        data_index += 1;
        }
        
        if(!verify_hmac(data_index - HMAC_SIZE, data_index - HMAC_SIZE*3, HMAC_SIZE*2)) {
            uart_write(UART1, ERROR_HMAC); // Reject the firmware
            SysCtlReset(); // Reset device
            return;
        }
        uart_write(UART1, OK); // Acknowledge the frame.
        
        data_index -= HMAC_SIZE*3; // we don't want to include these hmacs in the data, either. 
        
        break;
      }
     // if
      
    // Get the number of bytes specified
    for (int i = 0; i < frame_length; i++){
        data[data_index] = uart_read(UART1, BLOCKING, &read);
        data_index += 1;
    } //for
      
      //             first index of HMAC       first index of data          length of data
    if(!verify_hmac(data_index - HMAC_SIZE, data_index - frame_length, frame_length - HMAC_SIZE)){ //beginning of data, beginning of hmac
        uart_write(UART1, ERROR_HMAC); // Reject the firmware
        SysCtlReset(); // Reset device
        return;
    }
    //discard hmac so only fw will go to flash (this is the end of the IV frame)
    data_index -= HMAC_SIZE;
    
    uart_write(UART1, OK); // Acknowledge the frame.
  }
//get all parts necessary for decrypt firmware
//decrypt_firmware

    unsigned char just_data[size]; // its JUST DATA!!!! D:<
    // separate data and IV from data[]
    int j = 0;
    for (int i = 0; i < size + IV_SIZE; i++) {
        if (i >= size) {
            iv[j] = data[i];
            j++;
        } else {
            just_data[i] = data[i];
        }
    }
    decrypt_firmware(iv, just_data, size); //IV, key, key len, data, data len

//Now, data has all of the firmware, so we are going to put it all into flash. 
    // If we filed our page buffer, program it
    int remainder = size % FLASH_PAGESIZE;
    for(int i = 0; i < size - remainder; i += FLASH_PAGESIZE){
      if (program_flash(page_addr, just_data + i, FLASH_PAGESIZE)){
        uart_write(UART1, ERROR_FLASH); // Reject the firmware
        SysCtlReset(); // Reset device
        return;
      }
#if 1
      // Write debugging messages to UART2.
      uart_write_str(UART2, "Page successfully programmed\nAddress: ");
      uart_write_hex(UART2, page_addr);
      uart_write_str(UART2, "\nBytes: ");
      uart_write_hex(UART2, data_index);
      nl(UART2);
#endif
      // Update to next page
      page_addr += FLASH_PAGESIZE;


    } // for
    
    if(remainder){ //remainder in case there wasnt exactly % 1000
        if (program_flash(page_addr, just_data + (size - remainder), remainder)){
            uart_write(UART1, ERROR_FLASH); // Reject the firmware
            SysCtlReset(); // Reset device
            return;
        }
#if 1
      // Write debugging messages to UART2.
      uart_write_str(UART2, "Page successfully programmed\nAddress: ");
      uart_write_hex(UART2, page_addr);
      uart_write_str(UART2, "\nBytes: ");
      uart_write_hex(UART2, data_index);
      nl(UART2);
#endif
    }
      
    uart_write_str(UART2, "Finished Load Firmware");
}


// verify if the data was modified by calculating a new hmac and comparing it to the given hmac (Integrity/Authenticity)
// everything is currently hard-coded
int verify_hmac(unsigned int hmac_index, unsigned int data_f_index, unsigned int data_size) {    
    // declare variables used to create HMAC

    unsigned char tmp[32];
    const br_hash_class *digest_class = &br_sha256_vtable;
    br_hmac_key_context kc;
    br_hmac_context ctx;
            
    unsigned char key[32];
    memcpy(key, HMAC_KEY, 32);
    
    // Initialize the HMAC key and HMAC
    br_hmac_key_init(&kc, digest_class, key, HMAC_SIZE);
    br_hmac_init(&ctx, &kc, 0);
    
    // gets data used in HMAC
    unsigned char hmac_data[data_size];
    for (int i = 0; i < data_size; i++) {
        hmac_data[i] = data[data_f_index];   
        data_f_index++;
    }

    // gets the provided HMAC
    unsigned char hmac[HMAC_SIZE];
    for(int i = 0; i < HMAC_SIZE; i++){
        hmac[i] = data[hmac_index];
        hmac_index++;
    }

    // add data to be inside the HMAC
    br_hmac_update(&ctx, hmac_data, data_size);
    
    // write the calculated HMAC into tmp
    br_hmac_out(&ctx, tmp);
    
    // loop through each element of hmac and tmp and test if they are the same
    if (is_same(hmac, tmp, data_size)) {
        uart_write_str(UART2, "\nHMAC is Valid\n");
        return 1;
    } else {
        uart_write_str(UART2, "\nHMAC is Invalid\n");
    }
    return 0;  
}

int decrypt_firmware(unsigned char* iv, unsigned char* data, unsigned short DATA_LEN) {
    
    //all the AES CBC stuff
    const br_block_cbcdec_class * vd = &br_aes_big_cbcdec_vtable;
    br_aes_gen_cbcdec_keys v_dc;
    const br_block_cbcdec_class **dc;
    dc = &v_dc.vtable;
    
    //decoding the stuff in place ???
    vd->init(dc, CBC_KEY, CBC_KEY_LEN);
    vd->run(dc, iv, data, DATA_LEN);
    
    //transmitting all the decoded data on UART2 for debugging purpuses
    uart_write_str(UART2, "Unencrypted Data: ");
    uint32_t a;
    for(int i = 0; i < sizeof(data); i+=4) {
        a = data[i] << 24;
        a |= data[i+1] << 16;
        a |= data[i+2] << 8;
        a |= data[i+3];
        uart_write_hex(UART2, a);
    }
    nl(UART2);
    
    //Success!
    return 1;
}

// method checks if the given and calculated HMACs are the same
int is_same(unsigned char* hmac, unsigned char* tmp, unsigned int data_size) {
    int size = 0;
    // get the HMAC with the greater size (tho they should be the same length ya never know)
    if (HMAC_SIZE < data_size) {
        size = HMAC_SIZE;
    } else {
        size = data_size;
    }
    
    int equal = 1; // equals 0 when hmac and tmp are equal
    int useless = 0;
    for (int i = 0; i < size; i++) {
        if (*(hmac + i) == *(tmp + i)) {
            useless = 1; // set to a number for security --> time/power
        } else {
            equal = 0;
        }
    }
    
    return equal;
}

/*
 * Program a stream of bytes to the flash.
 * This function takes the starting address of a 1KB page, a pointer to the
 * data to write, and the number of byets to write.
 *
 * This functions performs an erase of the specified flash page before writing
 * the data.
 */
long program_flash(uint32_t page_addr, unsigned char *data, unsigned int data_len)
{
  unsigned int padded_data_len;

  // Erase next FLASH page
  FlashErase(page_addr);

  // Clear potentially unused bytes in last word
  if (data_len % FLASH_WRITESIZE){
    // Get number unused
    int rem = data_len % FLASH_WRITESIZE;
    int i;
    // Set to 0
    for (i = 0; i < rem; i++){
      data[data_len-1-i] = 0x00;
    }
    // Pad to 4-byte word
    padded_data_len = data_len+(FLASH_WRITESIZE-rem);
  } else {
    padded_data_len = data_len;
  }

  // Write full buffer of 4-byte words
  return FlashProgram((unsigned long *)data, page_addr, padded_data_len);
}


void boot_firmware(void)
{
  nl(UART2);
  uart_write_str(UART2, (char *) fw_release_message_address);

  // Boot the firmware
    __asm(
    "LDR R0,=0x10001\n\t"
    "BX R0\n\t"
  );
}