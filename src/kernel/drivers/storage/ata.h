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

#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include <stddef.h>

// ide channel definitions
#define ATA_PRIMARY     0
#define ATA_SECONDARY   1

// ide drive definitions
#define ATA_MASTER      0
#define ATA_SLAVE       1

// ata commands
#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_IDENTIFY_DRIVE  0xEC

// ata status flags
#define ATA_SR_BSY 0x80    // busy
#define ATA_SR_DRDY 0x40   // device ready
#define ATA_SR_DF 0x20     // device fault
#define ATA_SR_DSC 0x10    // device seek complete
#define ATA_SR_DRQ 0x08    // data request ready
#define ATA_SR_CORR 0x04   // corrected data
#define ATA_SR_IDX 0x02    // index
#define ATA_SR_ERR 0x01    // error

// ata error flags
#define ATA_ER_BBK  0x80   // bad block
#define ATA_ER_UNC  0x40   // uncorrectable data
#define ATA_ER_MC   0x20   // media changed
#define ATA_ER_IDNF 0x10   // id not found
#define ATA_ER_MCR  0x08   // media change request
#define ATA_ER_ABRT 0x04   // abort
#define ATA_ER_TK0NF 0x02  // track 0 not found
#define ATA_ER_AMNF 0x01   // address mark not found

// ata device structure
typedef struct {
    uint8_t channel;        // ata_primary or ata_secondary
    uint8_t drive;          // ata_master or ata_slave
    uint8_t type;           // device type
    uint16_t signature;     // device signature
    uint16_t capabilities;  // capabilities
    uint32_t command_sets;  // command sets
    uint32_t size;          // size in sectors
    char model[41];         // model string
    char serial[21];        // serial number
} ata_device_t;

// initialize the ata subsystem
void ata_init(void);

// detect an ata device
int ata_detect_device(uint8_t channel, uint8_t drive, ata_device_t *device);

// read sectors from an ata device
int ata_read_sectors(uint8_t channel, uint8_t drive, uint32_t lba, uint16_t count, void *buffer);

// write sectors to an ata device
int ata_write_sectors(uint8_t channel, uint8_t drive, uint32_t lba, uint16_t count, const void *buffer);

// get the number of detected devices
int ata_get_device_count(void);

// get device info by index
ata_device_t* ata_get_device(int index);

#endif
