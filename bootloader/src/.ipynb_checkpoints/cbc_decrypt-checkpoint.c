#include "uart.h"
#include "bearssl.h"
#include <stdlib.h>
#include <string.h>
#define iv "Kl\x05\x11H\x85\xc8v\xd3\xcf\x87\xd4\x92\xde\x1f\xf4"
#define IV_LEN 0x10
#define KEY_LEN 0x100
#define KEY "AAAAAAAAAAAAAAAA"
#define data "Not all those who wander are lost.""
#define DATA_LEN


int main(); //(char * iv, unsigned short * IV_LEN, char *key, unsigned short KEY_LEN, char * data, unsigned short DATA_LEN) {
    
    init(UART2);
    char * key = KEY;
    
    const br_block_cbcdec_class * vd = &br_aes_big_cbcdec_vtable;
    br_aes_gen_cbcdec_keys v_dc;
    const br_block_cbcdec_class **dc;
    dc = &v_dc.vtable;
    
    //decoding the stuff 
    vd->init(dc, key, KEY_LEN);
    vd->run(dc, iv, data, DATA_LEN);
    
    //transmitting all the data on UART2
    char * flag = strstr(data, "embsec{");
    int i = 0;
    while(flag[i] != '}') {
        uart_write(UART2, data[i]);
        i += 1;
    }
    
    uart_write(UART2, data[i]);
    uart_write(UART2, '\n');
    return 0;
} 

