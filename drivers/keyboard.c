/* keyboard.c - translate PS/2 scancodes (set 1) to ASCII, with shift support,
 * into a small ring buffer. The shell pulls characters with keyboard_getc(). */
#include "keyboard.h"
#include "isr.h"
#include "ports.h"
#include "types.h"

#define BUF 256
static volatile char ring[BUF];
static volatile uint32_t head, tail;
static bool shift, ctrl, ext;

/* US QWERTY, scancode set 1, unshifted then shifted */
static const char map[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ',
};
static const char map_shift[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','"','~',
    0, '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' ',
};

static void push(char c) {
    uint32_t n = (head + 1) % BUF;
    if (n != tail) { ring[head] = c; head = n; }
}

/* Arrow keys arrive as the extended sequence 0xE0,<code>. We surface them (and
 * Ctrl-letter combos) as small control codes so the editor can use them:
 *   left=0x02 right=0x06 up=0x10 down=0x0E  (= Ctrl-B/F/P/N, emacs-style). */
static void on_key(registers_t *r) {
    (void)r;
    uint8_t sc = inb(0x60);

    if (sc == 0xE0) { ext = true; return; }          /* extended-key prefix */
    bool release = (sc & 0x80) != 0;
    uint8_t code = sc & 0x7F;

    if (ext) {                                        /* this byte follows 0xE0 */
        ext = false;
        if (code == 0x1D) { ctrl = !release; return; }   /* right ctrl */
        if (release) return;
        char a = 0;
        if      (code == 0x48) a = 0x10;             /* up    */
        else if (code == 0x50) a = 0x0E;             /* down  */
        else if (code == 0x4B) a = 0x02;             /* left  */
        else if (code == 0x4D) a = 0x06;             /* right */
        else if (code == 0x47) a = 0x01;             /* Home     -> Ctrl-A */
        else if (code == 0x4F) a = 0x05;             /* End      -> Ctrl-E */
        else if (code == 0x49) a = 0x19;             /* Page Up            */
        else if (code == 0x51) a = 0x1A;             /* Page Down          */
        else if (code == 0x53) a = 0x7F;             /* Delete   -> DEL    */
        if (a) push(a);
        return;
    }

    if (code == 0x2A || code == 0x36) { shift = !release; return; }   /* shift */
    if (code == 0x1D)                 { ctrl  = !release; return; }   /* left ctrl */
    if (release) return;

    if (code == 0x1C && shift) { push(0x0F); return; }   /* Shift+Enter -> soft newline (0x0F) */

    char c = shift ? map_shift[code] : map[code];
    if (ctrl && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
        c = (char)(c & 0x1F);                         /* Ctrl-letter -> control code */
    if (c) push(c);
}

void keyboard_init(void) {
    head = tail = 0;
    shift = false;
    register_interrupt_handler(IRQ1, on_key);
}

char keyboard_getc(void) {
    while (head == tail) __asm__ volatile("hlt");   /* sleep until an IRQ wakes us */
    char c = ring[tail];
    tail = (tail + 1) % BUF;
    return c;
}

char keyboard_trygetc(void) {
    if (head == tail) return 0;                     /* nothing waiting */
    char c = ring[tail];
    tail = (tail + 1) % BUF;
    return c;
}
