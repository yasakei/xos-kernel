// -------------------------------------------------------------------
// mit license
// 
// copyright (c) 2026 xos
// 
// permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "software"), to deal in the software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the software, and to permit persons to whom the
// software is furnished to do so, subject to the following
// conditions:
// 
// the above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the software.
// 
// the software is provided "as is", without warranty of any kind,
// express or implied, including but not limited to the warranties
// of merchantability, fitness for a particular purpose and
// noninfringement. in no event shall the authors or copyright
// holders be liable for any claim, damages or other liability,
// whether in an action of contract, tort or otherwise, arising
// from, out of or in connection with the software or the use or
// other dealings in the software.
// -------------------------------------------------------------------

#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>

// rtl8139 pci ids
#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

// rtl8139 register offsets
#define RTL8139_MAC0        0x00  // mac address register 0
#define RTL8139_MAC1        0x01
#define RTL8139_MAC2        0x02
#define RTL8139_MAC3        0x03
#define RTL8139_MAC4        0x04
#define RTL8139_MAC5        0x05

#define RTL8139_RBSTART     0x30  // receive buffer start address
#define RTL8139_ERBCR       0x34  // early rx byte count register
#define RTL8139_ERSR        0x36  // early rx status register

#define RTL8139_CR          0x37  // command register
#define RTL8139_CAPR        0x38  // current address of packet read
#define RTL8139_CBR         0x3A  // current buffer address
#define RTL8139_IMR         0x3C  // interrupt mask register
#define RTL8139_ISR         0x3E  // interrupt status register

#define RTL8139_TCR         0x40  // tx config register
#define RTL8139_RCR         0x44  // rx config register

#define RTL8139_TSAD0       0x20  // tx status of descriptor 0
#define RTL8139_TSD0        0x10  // tx status descriptor 0
#define RTL8139_TBUF0       0x40  // tx buffer 0 physical address

#define RTL8139_TXADDR0     0x20  // tx descriptor 0 address
#define RTL8139_TXADDR1     0x24
#define RTL8139_TXADDR2     0x28
#define RTL8139_TXADDR3     0x2C

#define RTL8139_TXSTATUS0   0x10  // tx status descriptor 0
#define RTL8139_TXSTATUS1   0x14
#define RTL8139_TXSTATUS2   0x18
#define RTL8139_TXSTATUS3   0x1C

// command register (0x37) bits
#define RTL8139_CR_BUFE     0x01  // buffer empty
#define RTL8139_CR_TE       0x04  // transmit enable
#define RTL8139_CR_RE       0x08  // receive enable
#define RTL8139_CR_RST      0x10  // reset

// tx status bits (tsd)
#define RTL8139_TSD_OWN     0x80000000  // own bit (1=nic owns, 0=driver owns)
#define RTL8139_TSD_TABT    0x40000000  // transmit aborted
#define RTL8139_TSD_TUN     0x20000000  // transmit underrun
#define RTL8139_TSD_TOK     0x00008000  // transmit ok
#define RTL8139_TSD_SIZE    0x00001FFF  // transmit size (bits 12:0)

// rx status bits
#define RTL8139_RSR_ROK     0x0001      // receive ok
#define RTL8139_RSR_FAE     0x0002      // frame alignment error
#define RTL8139_RSR_CRC     0x0004      // crc error
#define RTL8139_RSR_LONG    0x0008      // packet too long
#define RTL8139_RSR_RUNT    0x0010      // packet too short
#define RTL8139_RSR_ISE     0x0020      // invalid symbol error
#define RTL8139_RSR_FOVW    0x0040      // rx fifo overflow
#define RTL8139_RSR_BOVW    0x0080      // rx buffer overflow
#define RTL8139_RSR_BAR     0x2000      // bad alignment
#define RTL8139_RSR_PAM     0x4000      // physical address matched
#define RTL8139_RSR_MAR     0x8000      // multicast address matched

// interrupt bits
#define RTL8139_INT_ROK     0x0001      // receive ok
#define RTL8139_INT_RER     0x0002      // receive error
#define RTL8139_INT_TOK     0x0004      // transmit ok
#define RTL8139_INT_TER     0x0008      // transmit error
#define RTL8139_INT_RXOVW   0x0010      // rx buffer overflow
#define RTL8139_INT_PUN     0x0020      // packet underrun
#define RTL8139_INT_FOVW    0x0040      // rx fifo overflow
#define RTL8139_INT_LINKCHG 0x0080      // link change
#define RTL8139_INT_RXFIFO  0x0100      // rx fifo underrun
#define RTL8139_INT_FESERR  0x0200      // fes error
#define RTL8139_INT_SERR    0x8000      // system error

// buffer sizes
#define RTL8139_RX_BUF_SIZE 0x2000      // 8kb rx buffer
#define RTL8139_RX_WRAP     0x1FFF      // wrap mask for rx buffer

// tx descriptor sizes
#define RTL8139_TX_BUF_SIZE 0x800       // 2kb per tx buffer
#define RTL8139_NUM_TX_DESC 4

typedef struct {
    uint16_t status;
    uint16_t vlan_tag;
    uint32_t buf_low;
    uint32_t buf_high;
} rtl8139_rx_desc_t;

// main rtl8139 device structure
typedef struct {
    uint8_t *rx_buffer;
    uintptr_t rx_buffer_pa;    // physical address
    uint16_t rx_read_offset;
    
    uint8_t *tx_buffers[RTL8139_NUM_TX_DESC];
    uintptr_t tx_buffer_pa[RTL8139_NUM_TX_DESC];
    uint8_t tx_current_desc;
    
    uint16_t io_base;         // io base if using port i/o
    uintptr_t mmio_base;      // mmio base if using memory i/o
    uint8_t is_mmio;          // 1 if mmio, 0 if port i/o
    
    uint8_t mac_address[6];
    uint8_t irq_line;
} rtl8139_device_t;

// function declarations
void rtl8139_init(uint8_t bus, uint8_t device, uint8_t func, uint16_t vendor_id, uint16_t device_id);
void rtl8139_detect_and_init(void);
void rtl8139_send_packet(const uint8_t *data, uint16_t length);
void rtl8139_handle_interrupt(void);

// get the device instance (for network layer access)
rtl8139_device_t* rtl8139_get_device(void);

#endif // RTL8139_H
