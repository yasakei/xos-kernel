#ifndef SERIAL_H
#define SERIAL_H

void serial_init(void);
void serial_putchar(char c);
void serial_write(const char *str);
void serial_flush(void);       // drain TX buffer — call from PIT or idle
int  serial_read_ready(void);  // 1 if RX has data
char serial_getchar(void);     // read one byte (call only if read_ready)

#endif
