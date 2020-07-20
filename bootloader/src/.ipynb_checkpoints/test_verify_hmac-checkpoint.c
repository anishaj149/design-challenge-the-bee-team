#include "inc/bearssl_hmac.h"
#include "uart.h"
#include "inc/hw_memmap.h" // Peripheral Base Addresses
#include "inc/lm3s6965.h" // Peripheral Bit Masks and Registers
#include "inc/hw_types.h" // Boolean type
#include "inc/hw_ints.h" // Interrupt numbers
#include "string.h"

int main(void) {
    
    uart_init(UART2);
    
    char* hmac = "\x0b\xad\xbebOZ\x93>\xd2\xe9?;\xe8\xd5\x7f\x06FO\xf3\x93\x13;0K\xd5B\x8c\x16\xda\xe0\x1e\x88"
    char* data = "c\xfe\xf4\x97\x81\xe2\xc4~\x1a\xad\x0e\x90\x98\xfdz4\x18\x96F;I`.\x943\xb4N,\xd7\xdb\xd5\x96t\xd2p\x80\x86f\x12\xadd\xafw\xb0\x0e\x7f\x7fEp\xb2\x18\x9e\x1f\xdbU\xae\x06\x99\xbf,n\x99\x82\xb3"
    // given code to set up a new hmac using the key, algorithm, and data
    unsigned char tmp[32];
    const br_hash_class *digest_class = &br_sha256_vtable;
    br_hmac_key_context kc;
    br_hmac_context ctx;
    
    // KEY is currently hard-coded into br_hmac_key_init
    // br_hmac_key_init(&kc, digest_class, KEY, KEY_LEN); // non-hard-coded code
    char* key = "AAAAAAAAAAAAAAAA"; // hard-coded key
    br_hmac_key_init(&kc, digest_class, key, 32);
    br_hamc_init(&ctx, &kc, 0);
    br_hmac_update(&ctx, data, data.length());
    uint64_t test_hmac = br_hmac_out(&ctx, tmp);
    
    // test the hmac to make sure the data was not altered (Integrity/Authenticity)
    if (hmac == test_hmac) {
        uart_write_str(UART2, 0x01);
        return 1;
    }
    
    uart_write_str(UART2, 0x00);
    return 0;  
}