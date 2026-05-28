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

#include "shell.h"
#include "../lib/printf.h"
#include "../drivers/display/vga.h"
#include "../drivers/bus/pci.h"
#include "../drivers/storage/ata.h"
#include "../fs/fat32.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../sched/scheduler.h"
#include "../drivers/timer/pit.h"
#include "../drivers/input/keyboard.h"
#include "../drivers/serial/serial.h"
#include "../lib/debuglog.h"
#include "../arch/usermode.h"
#include "../arch/syscall.h"
#include "../arch/gdt.h"
#include "../arch/elf.h"
#include "../drivers/network/net.h"
#include <stdint.h>
#include <stddef.h>

#define SHELL_BUF_SIZE  256
#define MAX_ARGS        16
#define HISTORY_SIZE    16

// default prompt colors (foreground, background)
static uint8_t shell_prompt_fg = 2; // green
static uint8_t shell_prompt_bg = 0; // black

// command history ring buffer
static char history[HISTORY_SIZE][SHELL_BUF_SIZE];
static int history_count = 0;
static int history_pos = 0;

// vga color names for convenience
static const char *vga_color_names[16] = {
    "black", "blue", "green", "cyan",
    "red", "magenta", "brown", "light gray",
    "dark gray", "light blue", "light green", "light cyan",
    "light red", "light magenta", "yellow", "white"
};

// tiny string helpers so we don't need libc
static int sh_strlen(const char *s) {
    int n = 0; while (s[n]) n++; return n;
}
static int sh_strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}
static void sh_strcpy(char *dst, const char *src) {
    while ((*dst++ = *src++));
}
static int sh_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
        if (ca != cb) return (unsigned char)ca - (unsigned char)cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}
static inline void outb_sh(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb_sh(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// print a string padded to 'width' chars with a single flush at the end
static void sh_print_padded(const char *s, int width) {
    extern int debug_log_is_enabled(void);
    int len = sh_strlen(s);
    // write string to vga raw (no flush per char)
    for (int i = 0; s[i]; i++) {
        vga_putchar_raw(s[i]);
        debug_log_putchar(s[i]);
        if (debug_log_is_enabled()) {
            serial_putchar(s[i]);
        }
    }
    // pad with spaces
    for (int i = len; i < width; i++) {
        vga_putchar_raw(' ');
        debug_log_putchar(' ');
        if (debug_log_is_enabled()) {
            serial_putchar(' ');
        }
    }
    // no flush here — caller flushes after the full line
}

// ── commands ──────────────────────────────────────────────────────────────────

// show the help text listing all available commands
static void cmd_help(void) {
    printf("\n");
    printf("  XOS Shell Commands\n");
    printf("  ------------------------------------\n");
    printf("  help              Show this help\n");
    printf("  clear             Clear the screen\n");
    printf("  ls                List files\n");
    printf("  cat <file>        Print file contents\n");
    printf("  meminfo           Memory stats\n");
    printf("  tasks             Running tasks\n");
    printf("  pci               PCI devices\n");
    printf("  uptime            Time since boot\n");
    printf("  xfetch            System info\n");
    printf("  touch <file>      Create empty file\n");
    printf("  echo <text> [>] <file>  Print text or write file\n");
    printf("  rm <file>         Delete file\n");
    printf("  mkdir <dir>       Create directory\n");
    printf("  cd <dir>          Change directory\n");
    printf("  pwd               Print working directory\n");
    printf("  log on|off|status Toggle verbose logging\n");
    printf("  exec <file>       Load ELF64 and run in Ring-3\n");
    printf("  usermode          Launch Ring-3 test task\n");
    printf("  reboot            Reboot\n");
    printf("  ------------------------------------\n");
    printf("\n");
}

// clear the screen
static void cmd_clear(void) {
    vga_clear();
}

// show the vga color palette
static void cmd_colors(void) {
    printf("\nVGA color palette:\n");
    uint8_t prev = vga_get_color();
    for (int bg = 0; bg < 1; bg++) {
        for (int fg = 0; fg < 16; fg++) {
            vga_set_color(fg, bg);
            printf(" %02x", fg);
        }
        printf("\n");
    }
    vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    printf("\nUse: promptcolor <fg> <bg> (0-15) or named color\n");
}

// parse a color argument (number 0-15 or named color)
static int parse_color_arg(const char *s) {
    if (!s) return -1;
    // try numeric first
    int v = 0;
    int i = 0; int isnum = 1;
    while (s[i]) { if (s[i] < '0' || s[i] > '9') { isnum = 0; break; } i++; }
    if (isnum) {
        for (i = 0; s[i]; i++) v = v * 10 + (s[i] - '0');
        if (v >= 0 && v < 16) return v;
        return -1;
    }
    // fall back to color name lookup
    for (int j = 0; j < 16; j++) {
        if (sh_strcasecmp(s, vga_color_names[j]) == 0) return j;
    }
    return -1;
}

// set the prompt foreground and background colors
static void cmd_promptcolor(int argc, char **argv) {
    if (argc == 1) {
        printf("Current prompt color: fg=%d bg=%d\n", shell_prompt_fg, shell_prompt_bg);
        return;
    }
    if (argc < 3) {
        printf("Usage: promptcolor <fg> <bg>\n");
        return;
    }
    int fg = parse_color_arg(argv[1]);
    int bg = parse_color_arg(argv[2]);
    if (fg < 0 || bg < 0) {
        printf("Invalid color. Use 0-15 or named colors.\n");
        return;
    }
    shell_prompt_fg = (uint8_t)fg;
    shell_prompt_bg = (uint8_t)bg;
    printf("Prompt color set: fg=%d bg=%d\n", fg, bg);
}

// parse an ipv4 address string into four octets
static int parse_ip4(const char *s, uint8_t *octets) {
    int i = 0, num = 0;
    while (*s && i < 4) {
        if (*s >= '0' && *s <= '9') {
            num = num * 10 + (*s - '0');
            if (num > 255) return -1;
        } else if (*s == '.') {
            octets[i++] = num & 0xFF;
            num = 0;
        } else {
            return -1;
        }
        s++;
    }
    if (i == 3) octets[3] = num & 0xFF;
    return (i == 3 && num <= 255) ? 0 : -1;
}

// send icmp echo requests to a target
static void cmd_ping(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: ping [-c count] <host>\n");
        return;
    }
    int count = 4;
    const char *host = NULL;
    if (argc >= 3 && sh_strcmp(argv[1], "-c") == 0) {
        const char *c_str = argv[2];
        count = 0;
        while (*c_str >= '0' && *c_str <= '9') {
            count = count * 10 + (*c_str - '0');
            c_str++;
        }
        host = argv[3];
    } else {
        host = argv[1];
    }
    if (!host) {
        printf("Usage: ping [-c count] <host>\n");
        return;
    }
    uint8_t octets[4] = {0};
    if (parse_ip4(host, octets) != 0) {
        printf("ping: invalid address\n");
        return;
    }
    uint32_t target_ip = ((uint32_t)octets[0] << 24) | 
                        ((uint32_t)octets[1] << 16) | 
                        ((uint32_t)octets[2] << 8) | 
                        ((uint32_t)octets[3]);
    printf("PING %d.%d.%d.%d (%d.%d.%d.%d) 56(84) bytes of data.\n",
           octets[0], octets[1], octets[2], octets[3],
           octets[0], octets[1], octets[2], octets[3]);
    for (int seq = 1; seq <= count; seq++) {
        net_send_icmp_echo(target_ip, seq);
        printf("Sent ICMP echo request %d\n", seq);
        vga_flush();
        if (seq < count) {
            pit_sleep_ms(1000);
        }
    }
}

// send an arp request for a target ip
static void cmd_arp(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: arp <host>\n");
        return;
    }
    uint8_t octets[4] = {0};
    if (parse_ip4(argv[1], octets) != 0) {
        printf("arp: invalid address\n");
        return;
    }
    uint32_t target_ip = ((uint32_t)octets[0] << 24) | 
                        ((uint32_t)octets[1] << 16) | 
                        ((uint32_t)octets[2] << 8) | 
                        ((uint32_t)octets[3]);
    printf("ARPING %d.%d.%d.%d from 127.0.0.1\n",
           octets[0], octets[1], octets[2], octets[3]);
    net_send_arp_request(target_ip);
}

// list the current directory
static void cmd_ls(void) {
    printf("\n");
    fat32_list_directory(NULL);  // null = list current directory
    printf("\n");
}

// print the contents of a file
static void cmd_cat(const char *filename) {
    if (!filename || sh_strlen(filename) == 0) {
        printf("Usage: cat <filename>\n");
        return;
    }
    
    // pass the filename as-is — fat32_open handles current directory
    fat32_file_t *f = fat32_open(filename);
    if (!f) { printf("cat: %s: not found\n", filename); return; }
    uint8_t buf[128];
    int total = 0, n;
    printf("\n");
    while ((n = fat32_read(f, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", (char *)buf);
        total += n;
    }
    fat32_close(f);
    if (total == 0) printf("(empty)\n");
    printf("\n");
}

// show physical memory usage stats
static void cmd_meminfo(void) {
    pmm_stats_t s;
    pmm_get_stats(&s);
    size_t free_mb  = (s.free_pages  * s.page_size) / (1024*1024);
    size_t used_mb  = (s.used_pages  * s.page_size) / (1024*1024);
    size_t total_mb = ((s.total_pages - s.reserved_pages) * s.page_size) / (1024*1024);
    printf("\n");
    printf("  Physical Memory\n");
    printf("  ------------------------------------\n");
    printf("  Page size : %d bytes\n",   (int)s.page_size);
    printf("  Reserved  : %d MB\n",      (int)(s.reserved_pages * s.page_size / 1024 / 1024));
    printf("  Total     : %d MB\n",      (int)total_mb);
    printf("  Used      : %d MB\n",      (int)used_mb);
    printf("  Free      : %d MB\n",      (int)free_mb);
    printf("\n");
}

// show running tasks
static void cmd_tasks(void) {
    printf("\n");
    scheduler_dump();
}

// scan and list pci devices
static void cmd_pci(void) {
    pci_init();
}

// show system uptime
static void cmd_uptime(void) {
    uint64_t ticks = pit_get_ticks();
    printf("Uptime: %d.%02d s (%d ticks)\n",
           (int)(ticks/100), (int)(ticks%100), (int)ticks);
}

// show the xfetch system info splash
static void cmd_xfetch(void) {
    pmm_stats_t mem;
    pmm_get_stats(&mem);
    uint64_t uptime  = pit_get_ticks() / 100;
    uint32_t free_mb = (uint32_t)((mem.free_pages * mem.page_size) / (1024*1024));
    uint32_t used_mb = (uint32_t)((mem.used_pages * mem.page_size) / (1024*1024));
    uint32_t tot_mb  = (uint32_t)(((mem.total_pages - mem.reserved_pages) * mem.page_size) / (1024*1024));

    // art lines exactly as in xos.txt (backslash = \\, no other escapes)
    static const char *art[10] = {
        "$\\   $\\  $$$\\   $$$\\  ",
        "$ |  $ |$  __$\\ $  __$\\",
        "\\$\\ $  |$ /  $ |$ /  \\__",
        " \\$$  / $ |  $ |\\$$$\\  ",
        " $  $<  $ |  $ | \\____$\\",
        "$  /\\$\\ $ |  $ |$\\   $ |",
        "$ /  $ | $$$  |\\$$$  |  ",
        "\\__|  \\__|\\______/ \\______/",
        "                           ",
        "                           ",
    };

    // info lines matching each art row
    printf("\n");

    sh_print_padded("  ", 2);
    sh_print_padded(art[0], 30); printf("   XOS\n");

    sh_print_padded("  ", 2);
    sh_print_padded(art[1], 30); printf("   -------------------\n");

    sh_print_padded("  ", 2);
    sh_print_padded(art[2], 30); printf("   OS     : XOS 64-bit\n");

    sh_print_padded("  ", 2);
    sh_print_padded(art[3], 30); printf("   Arch   : x86_64\n");

    sh_print_padded("  ", 2);
    sh_print_padded(art[4], 30); printf("   Kernel : Phobos\n");

    sh_print_padded("  ", 2);
    sh_print_padded(art[5], 30);
    printf("   Uptime : %d seconds\n", (int)uptime);

    sh_print_padded("  ", 2);
    sh_print_padded(art[6], 30);
    printf("   Memory : %d MB / %d MB\n", (int)used_mb, (int)tot_mb);

    sh_print_padded("  ", 2);
    sh_print_padded(art[7], 30);
    printf("   Free   : %d MB\n", (int)free_mb);

    sh_print_padded("  ", 2);
    sh_print_padded(art[8], 30);
    printf("   Tasks  : %d\n", scheduler_current_id() + 1);

    sh_print_padded("  ", 2);
    sh_print_padded(art[9], 30);
    printf("   Shell  : xos-shell\n");

    printf("\n");
}

// create an empty file
static void cmd_touch(const char *filename) {
    if (!filename || sh_strlen(filename) == 0) {
        printf("Usage: touch <file>\n");
        return;
    }

    // pass the filename as-is — fat32_create handles current directory
    if (fat32_create(filename) == 0) {
        printf("touch: created %s\n", filename);
    } else {
        printf("touch: failed to create %s\n", filename);
    }
}

// delete a file
static void cmd_rm(const char *filename) {
    if (!filename || sh_strlen(filename) == 0) {
        printf("Usage: rm <file>\n");
        return;
    }

    // pass the filename as-is — fat32_delete handles current directory
    if (fat32_delete(filename) == 0) {
        printf("rm: deleted %s\n", filename);
    } else {
        printf("rm: failed to delete %s\n", filename);
    }
}

// create a directory
static void cmd_mkdir(const char *dirname) {
    if (!dirname || sh_strlen(dirname) == 0) {
        printf("Usage: mkdir <directory>\n");
        return;
    }

    // pass the path as-is — fat32_mkdir handles absolute and relative
    if (fat32_mkdir(dirname) == 0) {
        printf("mkdir: created %s\n", dirname);
    } else {
        printf("mkdir: failed to create %s\n", dirname);
    }
}

// change the current directory
static void cmd_cd(const char *dirname) {
    if (!dirname || sh_strlen(dirname) == 0) {
        // cd with no args goes to root
        if (fat32_chdir("/") != 0) {
            printf("cd: failed to change to root directory\n");
        }
        return;
    }

    // pass the path as-is — fat32_chdir handles absolute and relative
    if (fat32_chdir(dirname) != 0) {
        printf("cd: failed to change directory to %s\n", dirname);
    }
}

// print the current working directory
static void cmd_pwd(void) {
    const char *cwd = fat32_getcwd();
    if (cwd) {
        printf("%s\n", cwd);
    } else {
        printf("/\n");
    }
}

// toggle debug logging on/off or show status
static void cmd_log(int argc, char **argv) {
    extern void debug_log_set_enabled(int enabled);
    extern int debug_log_is_enabled(void);
    extern void debug_print_set_enabled(int enabled);
    extern int debug_print_is_enabled(void);

    if (argc < 2) {
        printf("Usage: log on|off|status\n");
        return;
    }

    if (sh_strcasecmp(argv[1], "on") == 0) {
        debug_log_set_enabled(1);
        debug_print_set_enabled(1);
        printf("[LOG] enabled (print+buffer)\n");
    } else if (sh_strcasecmp(argv[1], "off") == 0) {
        debug_log_set_enabled(0);
        debug_print_set_enabled(0);
        printf("[LOG] disabled (print+buffer)\n");
    } else if (sh_strcasecmp(argv[1], "status") == 0) {
        printf("[LOG] print=%s buffer=%s\n",
               debug_print_is_enabled() ? "on" : "off",
               debug_log_is_enabled() ? "on" : "off");
    } else {
        printf("Usage: log on|off|status\n");
    }
}

// echo text or redirect it to a file
static void cmd_echo(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: echo <text> [>] <file>\n");
        return;
    }

    int redirect = -1;
    for (int i = 1; i < argc; i++) {
        if (sh_strcmp(argv[i], ">") == 0) {
            redirect = i;
            break;
        }
    }

    if (redirect < 0) {
        for (int i = 1; i < argc; i++) {
            if (i > 1) printf(" ");
            printf("%s", argv[i]);
        }
        printf("\n");
        return;
    }

    if (redirect == 1 || redirect + 1 >= argc) {
        printf("Usage: echo <text> > <file>\n");
        return;
    }

    char text[256];
    int pos = 0;
    for (int i = 1; i < redirect; i++) {
        for (int j = 0; argv[i][j] && pos < (int)sizeof(text) - 1; j++) {
            text[pos++] = argv[i][j];
        }
        if (i + 1 < redirect && pos < (int)sizeof(text) - 1) {
            text[pos++] = ' ';
        }
    }
    text[pos] = '\0';

    const char *filename = argv[redirect + 1];
    
    // pass the filename as-is — fat32_write handles current directory
    if (fat32_write(filename, text, sh_strlen(text)) == 0) {
        printf("echo: wrote %s\n", filename);
    } else {
        printf("echo: failed to write %s\n", filename);
    }
}

// launch a ring-3 user mode test task
static void cmd_usermode(void) {
    extern void user_test_entry(void);  // defined in user_test.asm

    if (debug_print_is_enabled()) {
        printf("[USERMODE] cmd_usermode() called\n");
    }
    int user_task = scheduler_create_user_task("user-test", user_test_entry);
    if (user_task < 0) {
        printf("[USERMODE] Failed to create user task\n");
        return;
    }

    if (debug_print_is_enabled()) {
        printf("[USERMODE] Created user task %d, yielding to scheduler\n", user_task);
    }
    scheduler_yield();
}

// load and execute an elf binary in ring-3
static void cmd_exec(const char *filename) {
    if (!filename || sh_strlen(filename) == 0) {
        printf("Usage: exec <file>\n");
        return;
    }

    __asm__ volatile("cli");

    if (debug_print_is_enabled()) {
        printf("[EXEC] Loading user program: %s\n", filename);
    }

    char path[64];
    if (filename[0] == '/') {
        sh_strcpy(path, filename);
    } else {
        path[0] = '/';
        sh_strcpy(path + 1, filename);
    }

    uint64_t entry = 0;
    if (debug_print_is_enabled()) {
        printf("[EXEC] Attempting to load ELF from %s\n", path);
    }
    if (elf_load_user_program(path, &entry) != 0) {
        printf("[EXEC] Error: failed to load %s\n", filename);
        __asm__ volatile("sti");
        return;
    }
    if (debug_print_is_enabled()) {
        printf("[EXEC] ELF loaded successfully, entry point: %p\n", (void *)(uintptr_t)entry);
    }

    if (debug_print_is_enabled()) {
        printf("[EXEC] Creating user task...\n");
    }
    int task_id = scheduler_create_user_task(filename, (void (*)(void))(uintptr_t)entry);
    if (task_id < 0) {
        printf("[EXEC] Error: failed to create user task\n");
        __asm__ volatile("sti");
        return;
    }

    if (debug_print_is_enabled()) {
        printf("[EXEC] Task %d started (%s @ %p), yielding to scheduler\n", task_id, filename, (void *)(uintptr_t)entry);
    }
    __asm__ volatile("sti");
    scheduler_yield();
}

// reboot the system via the keyboard controller
static void cmd_reboot(void) {
    printf("Rebooting...\n");
    uint8_t val;
    do { val = inb_sh(0x64); } while (val & 0x02);
    outb_sh(0x64, 0xFE);
    __asm__ volatile("cli; hlt");
}

// ── input ─────────────────────────────────────────────────────────────────────

// clear the current line by printing backspace characters
static void clear_line(int pos) {
    for (int i = 0; i < pos; i++) {
        printf("\b \b");
    }
}

// print a line of text (used for history recall)
static void print_line(const char *line) {
    for (int i = 0; line[i]; i++) {
        printf("%c", line[i]);
    }
}

// read one line of input from the keyboard, with history support
static int sh_readline(char *buf, int max) {
    int pos = 0;
    int browse_pos = history_count;
    char temp[SHELL_BUF_SIZE] = {0};
    
    while (1) {
        unsigned char c = (unsigned char)keyboard_getchar();
        
        // handle page up/down for scrollback
        if (c == (unsigned char)KEY_PGUP) {
            vga_scrollback_up(5);
            continue;
        }
        if (c == (unsigned char)KEY_PGDN) {
            vga_scrollback_down(5);
            continue;
        }
        
        // any other key resets scrollback
        if (vga_scrollback_is_active() && c != (unsigned char)KEY_PGUP && c != (unsigned char)KEY_PGDN) {
            vga_scrollback_reset();
        }
        
        // handle up/down arrow keys for history browsing
        if (c == (unsigned char)KEY_UP) {
            if (browse_pos > 0) {
                // save current input if at bottom of history
                if (browse_pos == history_count) {
                    for (int i = 0; i < pos; i++) temp[i] = buf[i];
                    temp[pos] = '\0';
                }
                
                browse_pos--;
                clear_line(pos);
                pos = 0;
                for (int i = 0; history[browse_pos][i]; i++) {
                    buf[pos++] = history[browse_pos][i];
                }
                buf[pos] = '\0';
                print_line(buf);
                vga_flush();
            }
            continue;
        }
        
        if (c == (unsigned char)KEY_DOWN) {
            if (browse_pos < history_count) {
                browse_pos++;
                clear_line(pos);
                pos = 0;
                
                if (browse_pos == history_count) {
                    // restore saved input
                    for (int i = 0; temp[i]; i++) {
                        buf[pos++] = temp[i];
                    }
                } else {
                    for (int i = 0; history[browse_pos][i]; i++) {
                        buf[pos++] = history[browse_pos][i];
                    }
                }
                buf[pos] = '\0';
                print_line(buf);
                vga_flush();
            }
            continue;
        }
        
        if (c == '\n' || c == '\r') {
            printf("\n");
            vga_flush();
            buf[pos] = '\0';
            
            // add to history if non-empty and different from last entry
            if (pos > 0) {
                int add_to_history = 1;
                if (history_count > 0) {
                    // check if this is the same as the last command
                    int same = 1;
                    for (int i = 0; i < pos; i++) {
                        if (buf[i] != history[history_count - 1][i]) {
                            same = 0;
                            break;
                        }
                    }
                    if (same && history[history_count - 1][pos] == '\0') {
                        add_to_history = 0;
                    }
                }
                
                if (add_to_history) {
                    int idx = history_count % HISTORY_SIZE;
                    for (int i = 0; i < pos; i++) {
                        history[idx][i] = buf[i];
                    }
                    history[idx][pos] = '\0';
                    if (history_count < HISTORY_SIZE) history_count++;
                }
            }
            
            return pos;
        }
        
        if ((c == '\b' || c == 127) && pos > 0) {
            pos--;
            printf("\b \b");
            vga_flush();
        if (c == 3) {
            printf("^C\n");
            vga_flush();
            buf[0] = '\0';
            return 0;
        }
        if (c == 24) {
            while (pos > 0) {
                printf("\\b \\b");
                printf("\b \b");
            }
            buf[0] = '\0';
            vga_flush();
            continue;
        }
            continue;
        }
        
        if (c < 32 || c > 126) continue;
        
        if (pos < max - 1) {
            buf[pos++] = (char)c;
            printf("%c", (char)c);
            vga_flush();
        }
    }
}

// ── parser ────────────────────────────────────────────────────────────────────

// parse a command line and dispatch to the right handler
static void sh_parse_and_run(char *line) {
    while (*line == ' ') line++;
    if (*line == '\0') return;

    char *argv[MAX_ARGS];
    int argc = 0;
    char *p = line;
    while (*p && argc < MAX_ARGS) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p == ' ') *p++ = '\0';
    }
    if (argc == 0) return;

    const char *cmd = argv[0];
    if      (sh_strcmp(cmd, "help")    == 0) cmd_help();
    else if (sh_strcmp(cmd, "clear")   == 0) cmd_clear();
    else if (sh_strcmp(cmd, "ls")      == 0) cmd_ls();
    else if (sh_strcmp(cmd, "cat")     == 0) cmd_cat(argc > 1 ? argv[1] : "");
    else if (sh_strcmp(cmd, "meminfo") == 0) cmd_meminfo();
    else if (sh_strcmp(cmd, "tasks")   == 0) cmd_tasks();
    else if (sh_strcmp(cmd, "pci")     == 0) cmd_pci();
    else if (sh_strcmp(cmd, "uptime")  == 0) cmd_uptime();
    else if (sh_strcmp(cmd, "xfetch")  == 0) cmd_xfetch();
    else if (sh_strcmp(cmd, "touch")   == 0) cmd_touch(argc > 1 ? argv[1] : "");
    else if (sh_strcmp(cmd, "echo")    == 0) cmd_echo(argc, argv);
    else if (sh_strcmp(cmd, "rm")      == 0) cmd_rm(argc > 1 ? argv[1] : "");
    else if (sh_strcmp(cmd, "log")     == 0) cmd_log(argc, argv);
    else if (sh_strcmp(cmd, "exec")    == 0) cmd_exec(argc > 1 ? argv[1] : "");
    else if (sh_strcmp(cmd, "ping")    == 0) cmd_ping(argc, argv);
    else if (sh_strcmp(cmd, "arp")     == 0) cmd_arp(argc, argv);
    else if (sh_strcmp(cmd, "usermode")== 0) cmd_usermode();
    else if (sh_strcmp(cmd, "reboot")  == 0) cmd_reboot();
    else if (sh_strcmp(cmd, "colors")  == 0) cmd_colors();
    else if (sh_strcmp(cmd, "promptcolor") == 0) cmd_promptcolor(argc, argv);
    else if (sh_strcmp(cmd, "mkdir")   == 0) cmd_mkdir(argc > 1 ? argv[1] : "");
    else if (sh_strcmp(cmd, "cd")      == 0) cmd_cd(argc > 1 ? argv[1] : "");
    else if (sh_strcmp(cmd, "pwd")     == 0) cmd_pwd();
    else printf("%s: command not found  (type 'help')\n", cmd);
}

// ── entry ─────────────────────────────────────────────────────────────────────

// the main shell entry point — runs forever processing commands
void shell_run(void) {
    pit_init(100);
    printf("\n");
    printf("================================================\n");
    printf("  XOS Shell  |  type 'help' for commands\n");
    printf("================================================\n");
    printf("\n");

    char line[SHELL_BUF_SIZE];
    while (1) {
        // print prompt with configured colors
        uint8_t prev = vga_get_color();
        vga_set_color(shell_prompt_fg, shell_prompt_bg);
        printf("xos> ");
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
        vga_flush();   // show cursor at prompt position
        sh_readline(line, sizeof(line));
        sh_parse_and_run(line);
        vga_flush();   // update cursor after command output
    }
}
