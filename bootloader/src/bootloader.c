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


// Forward Declarations
void load_initial_firmware(void);
void load_firmware(void);
void boot_firmware(void);
long program_flash(uint32_t, unsigned char*, unsigned int);
int decrypt_firmware();


// Firmware Constants
#define METADATA_BASE 0xFC00  // base address of version and firmware size in Flash
#define FW_BASE 0x10000  // base address of firmware in Flash


// FLASH Constants
#define FLASH_PAGESIZE 1024
#define FLASH_WRITESIZE 4


// Protocol Constants
#define OK    ((unsigned char)0x00)
#define ERROR ((unsigned char)0x01)
#define UPDATE ((unsigned char)'U')
#define BOOT ((unsigned char)'B')


// Firmware v2 is embedded in bootloader
extern int _binary_firmware_bin_start;
extern int _binary_firmware_bin_size;


// Device metadata
uint16_t *fw_version_address = (uint16_t *) METADATA_BASE;
uint16_t *fw_size_address = (uint16_t *) (METADATA_BASE + 2);
uint8_t *fw_release_message_address;

// Firmware Buffer
unsigned char data[FLASH_PAGESIZE];


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

  decrypt_firmware();
    
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
  int size = (int)&_binary_firmware_bin_size;
  int *data = (int *)&_binary_firmware_bin_start;
    
  uint16_t version = 2;
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
  int frame_length = 0;
  int read = 0;
  uint32_t rcv = 0;
  
  uint32_t data_index = 0;
  uint32_t page_addr = FW_BASE;
  uint32_t version = 0;
  uint32_t size = 0;

  // Get version.
  rcv = uart_read(UART1, BLOCKING, &read);
  version = (uint32_t)rcv;
  rcv = uart_read(UART1, BLOCKING, &read);
  version |= (uint32_t)rcv << 8;

  uart_write_str(UART2, "Received Firmware Version: ");
  uart_write_hex(UART2, version);
  nl(UART2);

  // Get size.
  rcv = uart_read(UART1, BLOCKING, &read);
  size = (uint32_t)rcv;
  rcv = uart_read(UART1, BLOCKING, &read);
  size |= (uint32_t)rcv << 8;
  

  uart_write_str(UART2, "Received Firmware Size: ");
  uart_write_hex(UART2, size);
  nl(UART2);


  // Compare to old version and abort if older (note special case for version 0).
  uint16_t old_version = *fw_version_address;

  if (version != 0 && version < old_version) {
    uart_write(UART1, ERROR); // Reject the metadata.
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

  uart_write(UART1, OK); // Acknowledge the metadata.

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

    // Get the number of bytes specified
    for (int i = 0; i < frame_length; ++i){
        data[data_index] = uart_read(UART1, BLOCKING, &read);
        data_index += 1;
    } //for

    // If we filed our page buffer, program it
    if (data_index == FLASH_PAGESIZE || frame_length == 0) {
      // Try to write flash and check for error
      if (program_flash(page_addr, data, data_index)){
        uart_write(UART1, ERROR); // Reject the firmware
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
      data_index = 0;

      // If at end of firmware, go to main
      if (frame_length == 0) {
        uart_write(UART1, OK);
        break;
      }
    } // if

    uart_write(UART1, OK); // Acknowledge the frame.
  } // while(1)
}

int verify_hmac(uint32_t metadata, char data[]) {
    
    return 0;  
}


int decrypt_firmware(){ //(char* iv, char* key, unsigned short KEY_LEN, char* data, unsigned short DATA_LEN) {
    char iv[16] = {0x50,0xea,0x92,0xec,0xb5,0x1f,0x5c,0x20,0xd,0x13,0xa6,0x30,0xed,0x6c,0x6b,0xcd};
    unsigned short KEY_LEN =  0x10;
    char* key = "AAAAAAAAAAAAAAAA";
    char data[48] = {0xae,0xd,0x2f,0xe5,0x74,0x98,0xce,0xc5,0x63,0x17,0x69,0xa5,0x62,0xa1,0x8e,0x47,0x4d,0x37,0xb5,0xad,0x4d,0xda,0x97,0xd0,0xb3,0x51,0x2c,0xe4,0x8e,0x2f,0xa7,0x34,0x31,0xca,0x8c,0xaa,0xdb,0x5,0xe6,0x14,0x24,0x56,0x40,0xf7,0xb8,0x20,0x6,0x3e};
    unsigned short DATA_LEN = 0x30;

    
    //all the AES CBC stuff
    const br_block_cbcdec_class * vd = &br_aes_big_cbcdec_vtable;
    br_aes_gen_cbcdec_keys v_dc;
    const br_block_cbcdec_class **dc;
    dc = &v_dc.vtable;
    
    //decoding the stuff in place ???
    vd->init(dc, key, KEY_LEN);
    vd->run(dc, iv, data, DATA_LEN);
    
    //transmitting all the decoded data on UART2 for debugging purpuses
    data[47] = '\0';
    uart_write_str(UART2, data);
    /*int i = 0;
    while(data[i] != '\0') {
        uart_write_str(UART2, data[i]); //check wtf is this thing
        i += 1;
    }*/
    //Success!
    return 1;
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
  uart_write_str(UART2, (char *) fw_release_message_address);

  // Boot the firmware
    __asm(
    "LDR R0,=0x10001\n\t"
    "BX R0\n\t"
  );
}