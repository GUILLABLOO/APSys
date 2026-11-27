/* SPDX-License-Identifier: GPL-3.0-or-later
Copyright (c) 2026 GUILLABLOO
kernel - APSys 00.01
Забыл сказать кое что, изначально APSys называлась как AES|AsyaEmbeddedSystem, но она переросла Embedded.
Поэтому вы видите то что есть, зачем это знать? Да потому что в некоторых частях
называется как aes|aes_kernel и тд. */

#include "key.h"
#include "asyxbox.h"
#include "sound.h"
#include "mouse.h"
#include "shell.h"
#include "diskmgr.h"
#include "roomfs.h"
#include "colors.h"
#include "cube.h"
#include "jaro.h"
#include "calc.h"
#include "chess.h"
#include "balls.h"
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;
typedef unsigned long       size_t;
#define NULL ((void*)0)
#define LINE_MAX 128
typedef struct {
uint32_t flags;
uint32_t mem_lower, mem_upper, boot_device, cmdline, mods_count, mods_addr;
uint32_t syms1, syms2, syms3, syms4;
uint32_t mmap_length, mmap_addr, drives_length, drives_addr;
uint32_t config_table, boot_loader_name, apm_table;
uint32_t vbe_control_info, vbe_mode_info;
uint16_t vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
uint64_t framebuffer_addr;
uint32_t framebuffer_pitch, framebuffer_width, framebuffer_height;
uint8_t  framebuffer_bpp, framebuffer_type;
} __attribute__((packed)) multiboot_info_t;
static volatile uint8_t* hw_vram   = NULL;
static uint32_t          hw_pitch  = 0;
static uint32_t          hw_width  = 0;
static uint32_t          hw_height = 0;
static uint8_t           hw_bpp    = 0;
static uint8_t          *fb        = 0;
static uint32_t virt_width  = 1024;
static uint32_t virt_height = 768;
static int cli_cy = 16;
static int cli_cx = 16;
static inline uint64_t rdtsc(void) {
uint32_t lo, hi;
__asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
return ((uint64_t)hi << 32) | lo;
}
static void delay_10ms(void) {
uint64_t start = rdtsc();
while ((rdtsc() - start) < 20000000ULL) {
__asm__ volatile ("pause");
}
}
typedef struct {
uint8_t r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15;
} glyph_t;
static glyph_t get_glyph(uint8_t ch) {
if (ch >= 'a' && ch <= 'z') ch -= 32;
static const glyph_t g_empty = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const glyph_t g_dash  = {0,0,0,0,0x7E,0x7E,0,0,0,0,0,0,0,0,0};
static const glyph_t g_plus  = {0,0,0,0x18,0x18,0x7E,0x7E,0x18,0x18,0,0,0,0,0};
static const glyph_t g_eq    = {0,0,0,0,0x7E,0,0x7E,0,0,0,0,0,0,0,0};
static const glyph_t g_dot   = {0,0,0,0,0,0,0,0,0,0x18,0x18,0,0,0,0,0};
static const glyph_t g_comma = {0,0,0,0,0,0,0,0,0x18,0x18,0x30,0,0,0,0};
static const glyph_t g_slash = {0,0,0x02,0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0,0,0,0,0};
static const glyph_t g_colon = {0,0,0,0x18,0x18,0,0,0x18,0x18,0,0,0,0,0};
static const glyph_t g_lt    = {0,0,0x06,0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x06,0,0,0,0,0};
static const glyph_t g_gt    = {0,0,0x60,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x60,0,0,0,0,0};
static const glyph_t g_lbr   = {0,0,0x3C,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3C,0,0,0,0,0};
static const glyph_t g_rbr   = {0,0,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0,0,0,0,0};
static const glyph_t g_lpar  = {0,0,0x0C,0x18,0x30,0x30,0x30,0x30,0x30,0x18,0x0C,0,0,0,0,0};
static const glyph_t g_rpar  = {0,0,0x30,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x18,0x30,0,0,0,0,0};
static const glyph_t g_und   = {0,0,0,0,0,0,0,0,0,0,0xFF,0,0,0,0,0};
static const glyph_t g_excl  = {0,0,0x18,0x18,0x18,0x18,0x18,0,0,0x18,0x18,0,0,0,0,0};
static const glyph_t g_qmark = {0,0,0x3C,0x66,0x06,0x0C,0x18,0x18,0,0x18,0x18,0,0,0,0,0};
static const glyph_t g_quot  = {0,0,0x66,0x66,0x66,0x22,0,0,0,0,0,0,0,0,0,0};
static const glyph_t g_apos  = {0,0,0x18,0x18,0x18,0x08,0,0,0,0,0,0,0,0,0,0};
static const glyph_t g_star  = {0,0,0,0x66,0x3C,0xFF,0x3C,0x66,0,0,0,0,0,0,0};
static const glyph_t g_pct   = {0,0,0xC2,0xC6,0x0C,0x18,0x30,0x66,0x46,0,0,0,0,0,0,0};
static const glyph_t g_amp   = {0,0,0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0,0,0,0,0,0,0};
static const glyph_t g_pipe  = {0,0,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0,0,0,0,0,0,0};
static const glyph_t g_semi  = {0,0,0,0,0x18,0x18,0,0,0x18,0x18,0x30,0,0,0,0,0};
static const glyph_t g_lbrace= {0,0,0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0,0,0,0,0,0,0};
static const glyph_t g_rbrace= {0,0,0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0,0,0,0,0,0,0};
static const glyph_t g_hash  = {0,0,0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0,0,0,0,0,0,0};
static const glyph_t g_at    = {0,0,0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x78,0,0,0,0,0,0,0};
static const glyph_t g_dollar= {0,0,0x18,0x7E,0xC0,0x7C,0x06,0xFC,0x18,0,0,0,0,0,0,0};
static const glyph_t g_caret = {0,0,0x10,0x38,0x6C,0xC6,0,0,0,0,0,0,0,0,0,0};
static const glyph_t g_tilde = {0,0,0,0,0x76,0xDC,0,0,0,0,0,0,0,0,0,0};
static const glyph_t g_bslash= {0,0,0x80,0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0,0,0,0,0};
static const glyph_t digits[] = {
{0,0,0x3C,0x66,0xC6,0xCE,0xD6,0xE6,0xC6,0x66,0x3C,0,0,0,0,0},
{0,0,0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x7E,0,0,0,0,0},
{0,0,0x7C,0xC6,0x06,0x0C,0x18,0x30,0x60,0xC6,0xFE,0,0,0,0,0},
{0,0,0x7C,0xC6,0x06,0x0C,0x38,0x0C,0x06,0xC6,0x7C,0,0,0,0,0},
{0,0,0x0C,0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x1E,0,0,0,0,0},
{0,0,0xFE,0x60,0x60,0x7C,0xC6,0x06,0x06,0xC6,0x7C,0,0,0,0,0},
{0,0,0x3C,0x60,0xC0,0x7C,0xC6,0xC6,0xC6,0xC6,0x3C,0,0,0,0,0},
{0,0,0xFE,0xC6,0x06,0x0C,0x18,0x30,0x30,0x30,0x30,0,0,0,0,0},
{0,0,0x3C,0x66,0xC6,0x6C,0x38,0x6C,0xC6,0x66,0x3C,0,0,0,0,0},
{0,0,0x3C,0x66,0xC6,0xC6,0x7E,0x06,0x06,0xC6,0x3C,0,0,0,0,0}
};
static const glyph_t letters[] = {
{0,0,0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x66,0x66,0,0,0,0,0},
{0,0,0xFC,0x66,0x66,0x66,0x7C,0x66,0x66,0x66,0xFC,0,0,0,0,0},
{0,0,0x3C,0x66,0xC0,0xC0,0xC0,0xC0,0xC0,0x66,0x3C,0,0,0,0,0},
{0,0,0xF8,0x6C,0x66,0x66,0x66,0x66,0x66,0x6C,0xF8,0,0,0,0,0},
{0,0,0xFE,0x66,0x60,0x60,0x7C,0x60,0x60,0x66,0xFE,0,0,0,0,0},
{0,0,0xFE,0x66,0x60,0x60,0x7C,0x60,0x60,0x60,0xF0,0,0,0,0,0},
{0,0,0x3C,0x66,0xC0,0xC0,0xCE,0xC6,0xC6,0x66,0x3E,0,0,0,0,0},
{0,0,0x66,0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x66,0,0,0,0,0},
{0,0,0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0,0,0,0,0},
{0,0,0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0,0,0,0,0},
{0,0,0xE6,0x66,0x6C,0x78,0x78,0x6C,0x66,0x66,0xE6,0,0,0,0,0},
{0,0,0xF0,0x60,0x60,0x60,0x60,0x60,0x62,0x66,0xFE,0,0,0,0,0},
{0,0,0xC6,0xEE,0xFE,0xD6,0xC6,0xC6,0xC6,0xC6,0xC6,0,0,0,0,0},
{0,0,0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,0xC6,0xC6,0xC6,0,0,0,0,0},
{0,0,0x3C,0x66,0xC6,0xC6,0xC6,0xC6,0xC6,0x66,0x3C,0,0,0,0,0},
{0,0,0xFC,0x66,0x66,0x66,0x7C,0x60,0x60,0x60,0xF0,0,0,0,0,0},
{0,0,0x3C,0x66,0xC6,0xC6,0xC6,0xC6,0xD6,0x7C,0x0E,0,0,0,0,0},
{0,0,0xFC,0x66,0x66,0x66,0x7C,0x6C,0x66,0x66,0xE6,0,0,0,0,0},
{0,0,0x3C,0x66,0xC0,0x60,0x3C,0x06,0xC6,0x66,0x3C,0,0,0,0,0},
{0,0,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0,0,0,0,0},
{0,0,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0,0,0,0,0},
{0,0,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x18,0,0,0,0,0},
{0,0,0xC6,0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0xC6,0,0,0,0,0},
{0,0,0xC6,0x6C,0x38,0x18,0x38,0x6C,0xC6,0xC6,0xC6,0,0,0,0,0},
{0,0,0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x18,0x3C,0,0,0,0,0},
{0,0,0xFE,0xC6,0x0C,0x18,0x30,0x60,0xC0,0xC6,0xFE,0,0,0,0,0}
};
if (ch >= '0' && ch <= '9') return digits[ch - '0'];
if (ch >= 'A' && ch <= 'Z') return letters[ch - 'A'];
if (ch == '-') return g_dash;
if (ch == '+') return g_plus;
if (ch == '=') return g_eq;
if (ch == '.') return g_dot;
if (ch == ',') return g_comma;
if (ch == '/') return g_slash;
if (ch == ':') return g_colon;
if (ch == '<') return g_lt;
if (ch == '>') return g_gt;
if (ch == '[') return g_lbr;
if (ch == ']') return g_rbr;
if (ch == '(') return g_lpar;
if (ch == ')') return g_rpar;
if (ch == '_') return g_und;
if (ch == '!') return g_excl;
if (ch == '?') return g_qmark;
if (ch == '"')  return g_quot;
if (ch == '\'') return g_apos;
if (ch == '*')  return g_star;
if (ch == '%')  return g_pct;
if (ch == '&')  return g_amp;
if (ch == '|')  return g_pipe;
if (ch == ';')  return g_semi;
if (ch == '{')  return g_lbrace;
if (ch == '}')  return g_rbrace;
if (ch == '#')  return g_hash;
if (ch == '@')  return g_at;
if (ch == '$')  return g_dollar;
if (ch == '^')  return g_caret;
if (ch == '~')  return g_tilde;
if (ch == '\\') return g_bslash;
return g_empty;
}
static void set_dynamic_resolution(uint32_t w, uint32_t h) {
if (w > hw_width) w = hw_width;
if (h > hw_height) h = hw_height;
virt_width = w;
virt_height = h;
}
static void draw_rect_raw(int rx, int ry, int rw, int rh, uint8_t r, uint8_t g, uint8_t b) {
if (!fb) return;
uint32_t bytes_pp = hw_bpp / 8;
for (int row = ry; row < ry + rh; ++row) {
if (row < 0 || (uint32_t)row >= hw_height) continue;
for (int col = rx; col < rx + rw; ++col) {
if (col < 0 || (uint32_t)col >= hw_width) continue;
uint32_t offset = row * hw_pitch + col * bytes_pp;
fb[offset + 0] = b;
fb[offset + 1] = g;
fb[offset + 2] = r;
}
}
}
static void draw_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
int rx = (x * (int)hw_width) / (int)virt_width;
int ry = (y * (int)hw_height) / (int)virt_height;
int rw = (w * (int)hw_width) / (int)virt_width;
int rh = (h * (int)hw_height) / (int)virt_height;
if (rw < 1 && w > 0) rw = 1;
if (rh < 1 && h > 0) rh = 1;
draw_rect_raw(rx, ry, rw, rh, r, g, b);
}
static void draw_checkerboard_wallpaper(void) {
if (!fb) return;
uint32_t bytes_pp = hw_bpp / 8;
int tile = 4;
for (uint32_t row = 0; row < hw_height; ++row) {
for (uint32_t col = 0; col < hw_width; ++col) {
int is_white = (((row / tile) + (col / tile)) & 1) == 0;
uint8_t c = is_white ? 235 : 165;
uint32_t offset = row * hw_pitch + col * bytes_pp;
fb[offset + 0] = c;
fb[offset + 1] = c;
fb[offset + 2] = c;
}
}
}
#define FB_MAX_BYTES (1920*1080*4)
static uint8_t back_fb[FB_MAX_BYTES];
static int back_on = 0;
static void gfx_enable_backbuffer(void){
if((uint32_t)hw_height * hw_pitch <= FB_MAX_BYTES){ back_on = 1; fb = back_fb; }
}
static void gfx_present(void){
if(!back_on) return;
uint32_t bytes = hw_height * hw_pitch;
uint8_t *dst = (uint8_t*)(unsigned long)hw_vram;
if(hw_pitch % 4 == 0){
uint32_t words = bytes / 4;
uint32_t *d = (uint32_t*)dst;
uint32_t *s = (uint32_t*)back_fb;
for(uint32_t i=0;i<words;++i) d[i]=s[i];
} else {
for(uint32_t i=0;i<bytes;++i) dst[i]=back_fb[i];
}
}
static void draw_char(uint8_t ch, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
mouse_note_char(ch, x, y, r, g, b);
glyph_t g_data = get_glyph(ch);
const uint8_t* rows = (const uint8_t*)&g_data;
for (int row = 0; row < 16; ++row) {
uint8_t bits = rows[row];
for (int col = 0; col < 8; ++col) {
if (bits & (0x80 >> col)) {
draw_rect(x + col, y + row, 1, 1, r, g, b);
}
}
}
}
static void print_str(const char* str, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
int cx = x;
while (*str) {
if (*str == '\n') { cx = x; y += 18; }
else { draw_char((uint8_t)(*str), cx, y, r, g, b); cx += 8; }
++str;
}
}
static void print_hex(uint32_t val, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
const char* hex = "0123456789ABCDEF";
draw_char('0', x, y, r, g, b);
draw_char('X', x + 8, y, r, g, b);
for (int i = 0; i < 8; ++i) {
uint8_t nib = (val >> ((7 - i) * 4)) & 0xF;
draw_char((uint8_t)hex[nib], x + 16 + i * 8, y, r, g, b);
}
}
static int print_dec(uint32_t val, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
if (val == 0) { draw_char('0', x, y, r, g, b); return x + 8; }
uint32_t div = 1000000000U;
int started = 0, cx = x;
while (div > 0) {
uint32_t digit = val / div;
if (digit > 0 || started) {
draw_char((uint8_t)('0' + digit), cx, y, r, g, b);
cx += 8;
started = 1;
}
val %= div;
div /= 10;
}
return cx;
}
static void cli_newline_check(void) {
cli_cy += 18;
if (cli_cy > (int)virt_height - 28) {
draw_rect(0, 0, virt_width, virt_height, 10, 12, 16);
mouse_note_clear(0, 0, (int)virt_width, (int)virt_height);
cli_cy = 16;
cli_cx = 16;
}
}
static void ab_print_line(const char* text, uint8_t r, uint8_t g, uint8_t b) {
cli_newline_check();
print_str(text, 16, cli_cy, r, g, b);
int n=0; while(text[n])n++;
cli_cx = 16 + n*8;
}
static void ab_print_raw(const char* text, uint8_t r, uint8_t g, uint8_t b) {
while(*text){
if(cli_cx > (int)virt_width-24) break;
draw_char((uint8_t)*text, cli_cx, cli_cy, r, g, b);
cli_cx += 8; text++;
}
}
static void ab_pump(void){ }
static void ab_print_num(const char* prefix, uint32_t val, const char* suffix, uint8_t r, uint8_t g, uint8_t b) {
cli_newline_check();
int cx = 16;
while (*prefix) {
draw_char((uint8_t)(*prefix), cx, cli_cy, r, g, b);
cx += 8;
prefix++;
}
cx = print_dec(val, cx, cli_cy, r, g, b);
print_str(suffix, cx, cli_cy, r, g, b);
}
static void render_retro_boot_menu(int cursor_row, int boot_mode, int res_idx, int sec_left, int full_redraw) {
if (full_redraw) draw_checkerboard_wallpaper();
int ww = 560, wh = 270;
int wx = ((int)hw_width - ww) / 2;
int wy = ((int)hw_height - wh) / 2;
if (full_redraw) {
draw_rect_raw(wx + 10, wy + 10, ww, wh, 45, 45, 45);
draw_rect_raw(wx, wy, ww, wh, 60, 60, 60);
draw_rect_raw(wx + 3, wy + 3, ww - 6, wh - 6, 255, 255, 255);
draw_rect_raw(wx + 3, wy + 3, ww - 6, 28, 60, 60, 60);
print_str("APSYS (ASYA OPERATING SYSTEM) - BOOT SETUP", wx + 16, wy + 9, 255, 255, 255);
print_str("UP/DOWN: SELECT   LEFT/RIGHT: CHANGE   ENTER: START", wx + 22, wy + 44, 90, 90, 90);
}
int row0_y = wy + 78;
if (cursor_row == 0) {
draw_rect_raw(wx + 20, row0_y, ww - 40, 34, 60, 60, 60);
print_str("BOOT AS :", wx + 32, row0_y + 9, 255, 255, 255);
if (boot_mode == 0) print_str("< FULL TERMINAL >", wx + 160, row0_y + 9, 255, 255, 255);
else if (boot_mode == 1) print_str("< ARTHE DESKTOP >", wx + 160, row0_y + 9, 255, 255, 255);
else print_str("< VBE TEST >", wx + 160, row0_y + 9, 255, 255, 255);
} else {
draw_rect_raw(wx + 20, row0_y, ww - 40, 34, 255, 255, 255);
print_str("BOOT AS :", wx + 32, row0_y + 9, 60, 60, 60);
if (boot_mode == 0) print_str("< FULL TERMINAL >", wx + 160, row0_y + 9, 60, 60, 60);
else if (boot_mode == 1) print_str("< ARTHE DESKTOP >", wx + 160, row0_y + 9, 60, 60, 60);
else print_str("< VBE TEST >", wx + 160, row0_y + 9, 60, 60, 60);
}
int row1_y = wy + 122;
const char* res_label = "< NATIVE MAX >";
if (res_idx == 0) res_label = "< 640 X 480 >";
else if (res_idx == 1) res_label = "< 800 X 600 >";
else if (res_idx == 2) res_label = "< 1024 X 768 >";
if (cursor_row == 1) {
draw_rect_raw(wx + 20, row1_y, ww - 40, 34, 60, 60, 60);
print_str("RESOLUTION :", wx + 32, row1_y + 9, 255, 255, 255);
print_str(res_label, wx + 160, row1_y + 9, 255, 255, 255);
} else {
draw_rect_raw(wx + 20, row1_y, ww - 40, 34, 255, 255, 255);
print_str("RESOLUTION :", wx + 32, row1_y + 9, 60, 60, 60);
print_str(res_label, wx + 160, row1_y + 9, 60, 60, 60);
}
int bar_y = wy + 185;
draw_rect_raw(wx + 20, bar_y, ww - 40, 16, 60, 60, 60);
draw_rect_raw(wx + 22, bar_y + 2, ww - 44, 12, 255, 255, 255);
int fill_w = ((ww - 44) * sec_left) / 15;
draw_rect_raw(wx + 22, bar_y + 2, fill_w, 12, 60, 60, 60);
draw_rect_raw(wx + 20, wy + 214, 260, 20, 255, 255, 255);
print_str("STARTING IN:", wx + 20, wy + 216, 60, 60, 60);
print_dec((uint32_t)sec_left, wx + 124, wy + 216, 60, 60, 60);
print_str("SEC", wx + 148, wy + 216, 60, 60, 60);
}
static void render_cli_line(int start_x, int cy, char* buf, int len, int cursor) {
draw_rect(start_x, cy, virt_width - start_x, 18, 10, 12, 16);
mouse_note_clear(start_x, cy, (int)virt_width - start_x, 18);
for (int i = 0; i < len; ++i) {
int px = start_x + i * 8;
uint8_t ch = (uint8_t)buf[i];
if (i == cursor) {
draw_rect(px, cy, 8, 16, 230, 230, 230);
draw_char(ch, px, cy, 10, 12, 16);
} else {
draw_char(ch, px, cy, 255, 255, 255);
}
}
if (cursor == len) {
draw_char('_', start_x + cursor * 8, cy, 255, 255, 255);
}
}
static void cli_resume(void) {
back_on = 0;
fb = (uint8_t*)(unsigned long)hw_vram;
draw_rect(0, 0, virt_width, virt_height, 10, 12, 16);
mouse_note_clear(0, 0, (int)virt_width, (int)virt_height);
cli_cy = 16;
cli_cx = 16;
}
static int launch_gui_mode(multiboot_info_t* mboot);
static void cli_run_demo(int sig) {
if (sig != ASYXBOX_SIGNAL_OPEN_COLORS &&
sig != ASYXBOX_SIGNAL_OPEN_CUBE &&
sig != ASYXBOX_SIGNAL_OPEN_CUBE_4D &&
sig != ASYXBOX_SIGNAL_OPEN_CALC) return;
gui_gfx_t cg;
cg.vram=hw_vram; cg.pitch=hw_pitch;
cg.hw_w=hw_width; cg.hw_h=hw_height; cg.bpp=hw_bpp;
cg.virt_w=virt_width; cg.virt_h=virt_height;
cg.rect=draw_rect; cg.ch=draw_char; cg.str=print_str;
cg.wallpaper=draw_checkerboard_wallpaper;
cg.enable_bb=gfx_enable_backbuffer; cg.present=gfx_present;
if      (sig==ASYXBOX_SIGNAL_OPEN_COLORS)  colors_run_cli(&cg);
else if (sig==ASYXBOX_SIGNAL_OPEN_CUBE)    cube_run_cli(&cg);
else if (sig==ASYXBOX_SIGNAL_OPEN_CUBE_4D) cube_run_cli_4d(&cg);
else if (sig==ASYXBOX_SIGNAL_OPEN_CALC)    calc_run_cli(&cg);
back_on = 0;
fb = (uint8_t*)(unsigned long)hw_vram;
draw_rect(0, 0, virt_width, virt_height, 10, 12, 16);
mouse_note_clear(0, 0, (int)virt_width, (int)virt_height);
cli_cy = 16;
cli_cx = 16;
}
static void cli_run_balls(asyxbox_ctx_t *cx) {
(void)cx;
gui_gfx_t cg;
cg.vram=hw_vram; cg.pitch=hw_pitch; cg.hw_w=hw_width; cg.hw_h=hw_height; cg.bpp=hw_bpp;
cg.virt_w=virt_width; cg.virt_h=virt_height;
cg.rect=draw_rect; cg.ch=draw_char; cg.str=print_str; cg.wallpaper=draw_checkerboard_wallpaper;
cg.enable_bb=gfx_enable_backbuffer; cg.present=gfx_present;
balls_run_cli(&cg);
cli_resume();
}
static void cli_run_chess(asyxbox_ctx_t *cx) {
(void)cx;
gui_gfx_t cg;
cg.vram=hw_vram; cg.pitch=hw_pitch; cg.hw_w=hw_width; cg.hw_h=hw_height; cg.bpp=hw_bpp;
cg.virt_w=virt_width; cg.virt_h=virt_height;
cg.rect=draw_rect; cg.ch=draw_char; cg.str=print_str; cg.wallpaper=draw_checkerboard_wallpaper;
cg.enable_bb=gfx_enable_backbuffer; cg.present=gfx_present;
chess_run_cli(&cg);
cli_resume();
}
static void cli_run_editor(asyxbox_ctx_t *cx) {
gui_gfx_t cg;
cg.vram=hw_vram; cg.pitch=hw_pitch; cg.hw_w=hw_width; cg.hw_h=hw_height; cg.bpp=hw_bpp;
cg.virt_w=virt_width; cg.virt_h=virt_height;
cg.rect=draw_rect; cg.ch=draw_char; cg.str=print_str; cg.wallpaper=draw_checkerboard_wallpaper;
cg.enable_bb=gfx_enable_backbuffer; cg.present=gfx_present;
jaro_run_cli(&cg, cx->edit_vol, cx->edit_path, cx->edit_name);
back_on = 0;
fb = (uint8_t*)(unsigned long)hw_vram;
draw_rect(0, 0, virt_width, virt_height, 10, 12, 16);
mouse_note_clear(0, 0, (int)virt_width, (int)virt_height);
cli_cy = 16;
cli_cx = 16;
}
static int launch_cli_mode(multiboot_info_t* mboot) {
back_on = 0;
fb = (uint8_t*)(unsigned long)hw_vram;
draw_rect(0, 0, virt_width, virt_height, 10, 12, 16);
asyxbox_ctx_t ab_ctx;
ab_ctx.mem_lower_kb=mboot->mem_lower;
ab_ctx.mem_upper_kb=mboot->mem_upper;
ab_ctx.virt_w=virt_width; ab_ctx.virt_h=virt_height;
ab_ctx.hw_w=hw_width;     ab_ctx.hw_h=hw_height;
ab_ctx.bpp=hw_bpp;
ab_ctx.print_line=ab_print_line;
ab_ctx.print_num =ab_print_num;
ab_ctx.print_raw=ab_print_raw;
ab_ctx.pump=ab_pump;
ab_ctx.signal=0;
mouse_video_t mv;
mv.vram=hw_vram; mv.pitch=hw_pitch;
mv.hw_w=hw_width; mv.hw_h=hw_height; mv.bpp=hw_bpp;
mv.virt_w=virt_width; mv.virt_h=virt_height;
mv.bg_r=10; mv.bg_g=12; mv.bg_b=16;
mv.rect=draw_rect; mv.glyph=draw_char;
int have_mouse=mouse_init(&mv);
mouse_cons_setup(16,8,18);
mouse_enable_buttons(MOUSE_B1);
int start_x=64;
cli_cy=16; cli_cx=16;
int max_cols=((int)virt_width-start_x-24)/8;
if (max_cols>LINE_MAX-2) max_cols=LINE_MAX-2;
char line[LINE_MAX];
int len=0, cursor=0;
print_str("asya->", 16, cli_cy, 100,230,140);
render_cli_line(start_x, cli_cy, line, len, cursor);
if (!have_mouse) {
cli_newline_check();
print_str("MOUSE: PS/2 NOT DETECTED - KEYBOARD ONLY", 16, cli_cy, 240,110,110);
cli_newline_check();
print_str("asya->", 16, cli_cy, 100,230,140);
render_cli_line(start_x, cli_cy, line, len, cursor);
}
mouse_show();
for (;;) {
int k=key_poll();
mouse_frame();
char selbuf[LINE_MAX];
int seln=mouse_take_exec(selbuf, LINE_MAX);
if (seln>0) {
mouse_hide();
cli_newline_check();
print_str("asya->", 16, cli_cy, 100,230,140);
print_str(selbuf, start_x, cli_cy, 255,255,255);
ab_ctx.signal=0;
asyxbox_exec(selbuf, seln, &ab_ctx);
int sig=ab_ctx.signal; ab_ctx.signal=0;
if (sig==ASYXBOX_SIGNAL_SWITCH_ARTHE) { mouse_hide(); return ASYXBOX_SIGNAL_SWITCH_ARTHE; }
if (sig==ASYXBOX_SIGNAL_OPEN_EDITOR) cli_run_editor(&ab_ctx);
else if (sig==ASYXBOX_SIGNAL_OPEN_CHESS) cli_run_chess(&ab_ctx);
else if (sig==ASYXBOX_SIGNAL_OPEN_BALLS) cli_run_balls(&ab_ctx);
else cli_run_demo(sig);
if (sig==ASYXBOX_SIGNAL_CLEAR || sig==ASYXBOX_SIGNAL_SWITCH_TERMINAL) {
draw_rect(0,0,virt_width,virt_height,10,12,16);
mouse_note_clear(0,0,(int)virt_width,(int)virt_height);
cli_cy=16; cli_cx=16;
}
len=0; cursor=0;
cli_newline_check();
print_str("asya->", 16, cli_cy, 100,230,140);
render_cli_line(start_x, cli_cy, line, len, cursor);
mouse_show();
}
if (k != KEY_NONE) {
mouse_hide();
key_modifiers_t m; key_get_modifiers(&m);
draw_rect(virt_width-260, 16, 250, 18, 10,12,16);
if (m.shift)    print_str("[SHF]", virt_width-250, 16, 255,220,90);
if (m.ctrl)     print_str("[CTL]", virt_width-200, 16, 255,120,120);
if (m.alt)      print_str("[ALT]", virt_width-150, 16, 120,200,255);
if (m.capslock) print_str("[CAP]", virt_width-100, 16, 120,240,140);
if (k==KEY_ENTER) {
render_cli_line(start_x, cli_cy, line, len, -1);
line[len]=0;
ab_ctx.signal=0;
asyxbox_exec(line, len, &ab_ctx);
int sig=ab_ctx.signal; ab_ctx.signal=0;
if (sig==ASYXBOX_SIGNAL_SWITCH_ARTHE) { mouse_hide(); return ASYXBOX_SIGNAL_SWITCH_ARTHE; }
if (sig==ASYXBOX_SIGNAL_OPEN_EDITOR) cli_run_editor(&ab_ctx);
else if (sig==ASYXBOX_SIGNAL_OPEN_CHESS) cli_run_chess(&ab_ctx);
else if (sig==ASYXBOX_SIGNAL_OPEN_BALLS) cli_run_balls(&ab_ctx);
else cli_run_demo(sig);
if (sig==ASYXBOX_SIGNAL_CLEAR || sig==ASYXBOX_SIGNAL_SWITCH_TERMINAL) {
draw_rect(0,0,virt_width,virt_height,10,12,16);
mouse_note_clear(0,0,(int)virt_width,(int)virt_height);
cli_cy=16; cli_cx=16;
}
cli_newline_check();
len=0; cursor=0;
print_str("asya->", 16, cli_cy, 100,230,140);
render_cli_line(start_x, cli_cy, line, len, cursor);
} else if (k==KEY_BACKSPACE) {
if (cursor>0) {
for (int i=cursor-1;i<len-1;++i) line[i]=line[i+1];
len--; cursor--;
render_cli_line(start_x, cli_cy, line, len, cursor);
}
} else if (k==KEY_LEFT) {
if (cursor>0) { cursor--; render_cli_line(start_x, cli_cy, line, len, cursor); }
} else if (k==KEY_RIGHT) {
if (cursor<len) { cursor++; render_cli_line(start_x, cli_cy, line, len, cursor); }
} else if (k==KEY_TAB) {
for (int s=0;s<4 && len<max_cols;++s) {
for (int i=len;i>cursor;--i) line[i]=line[i-1];
line[cursor]=' '; len++; cursor++;
}
render_cli_line(start_x, cli_cy, line, len, cursor);
} else if (k>=32 && k<=126) {
if (len<max_cols && !m.ctrl) {
for (int i=len;i>cursor;--i) line[i]=line[i-1];
line[cursor]=(char)k; len++; cursor++;
render_cli_line(start_x, cli_cy, line, len, cursor);
}
}
mouse_show();
}
__asm__ volatile("pause");
}
return 0;
}
static int launch_gui_mode(multiboot_info_t* mboot) {
gui_gfx_t g;
g.vram=hw_vram; g.pitch=hw_pitch; g.hw_w=hw_width; g.hw_h=hw_height; g.bpp=hw_bpp;
g.virt_w=virt_width; g.virt_h=virt_height;
g.rect=draw_rect; g.ch=draw_char; g.str=print_str; g.wallpaper=draw_checkerboard_wallpaper;
g.enable_bb=gfx_enable_backbuffer;
g.present=gfx_present;
return shell_run(&g, mboot->mem_lower, mboot->mem_upper);
}
static void launch_vbe_mode(multiboot_info_t* mboot) {
draw_rect(0, 0, virt_width, virt_height, 14, 14, 18);
print_str("APSYS - VBE HARDWARE INSPECTOR", 16, 16, 255, 255, 255);
print_str("------------------------------", 16, 34, 90, 90, 90);
print_str("VRAM PHYS :", 16, 60, 180, 180, 180);
print_hex((uint32_t)(mboot->framebuffer_addr), 120, 60, 255, 255, 255);
print_str("VBE CTRL  :", 16, 82, 180, 180, 180);
print_hex(mboot->vbe_control_info, 120, 82, 255, 255, 255);
print_str("VBE MODE  :", 16, 104, 180, 180, 180);
print_hex(mboot->vbe_mode_info, 120, 104, 255, 255, 255);
print_str("PITCH     :", 16, 126, 180, 180, 180);
print_dec(hw_pitch, 120, 126, 255, 255, 255);
}
void aes_kernel_main(uint32_t magic, multiboot_info_t* mboot) {
if (magic != 0x2BADB002) {
while (1) { asm volatile("hlt"); }
}
if ((mboot->flags & (1 << 12)) && mboot->framebuffer_addr != 0) {
hw_vram   = (volatile uint8_t*)(uint32_t)(mboot->framebuffer_addr);
fb        = (uint8_t*)(unsigned long)hw_vram;
hw_pitch  = mboot->framebuffer_pitch;
hw_width  = mboot->framebuffer_width;
hw_height = mboot->framebuffer_height;
hw_bpp    = mboot->framebuffer_bpp;
key_init();
sound_init();
sound_boot_chime();
diskmgr_ensure();
{
int bios = (int)((mboot->boot_device >> 24) & 0xFF);
roomfs_set_boot_disk(bios >= 0x80 ? bios - 0x80 : -1);
}
int cursor_row = 0;
int boot_mode  = 0;
int res_idx    = 3;
int ticks      = 1500;
int last_sec   = -1;
render_retro_boot_menu(cursor_row, boot_mode, res_idx, 15, 1);
while (ticks > 0) {
int cur_sec = (ticks + 99) / 100;
int k = key_poll();
if (k == KEY_UP || k == KEY_DOWN || k == KEY_TAB) {
cursor_row = !cursor_row;
render_retro_boot_menu(cursor_row, boot_mode, res_idx, cur_sec, 0);
} else if (k == KEY_LEFT) {
if (cursor_row == 0) boot_mode = (boot_mode + 2) % 3;
else res_idx = (res_idx + 3) % 4;
render_retro_boot_menu(cursor_row, boot_mode, res_idx, cur_sec, 0);
} else if (k == KEY_RIGHT) {
if (cursor_row == 0) boot_mode = (boot_mode + 1) % 3;
else res_idx = (res_idx + 1) % 4;
render_retro_boot_menu(cursor_row, boot_mode, res_idx, cur_sec, 0);
} else if (k == KEY_ENTER) {
break;
}
if (cur_sec != last_sec) {
last_sec = cur_sec;
render_retro_boot_menu(cursor_row, boot_mode, res_idx, cur_sec, 0);
}
delay_10ms();
ticks--;
}
if (res_idx == 0) set_dynamic_resolution(640, 480);
else if (res_idx == 1) set_dynamic_resolution(800, 600);
else if (res_idx == 2) set_dynamic_resolution(1024, 768);
else set_dynamic_resolution(hw_width, hw_height);
int mode = boot_mode;
while (1) {
if (mode == 0) {
int ret = launch_cli_mode(mboot);
if (ret == ASYXBOX_SIGNAL_SWITCH_ARTHE) mode = 1;
else break;
} else if (mode == 1) {
int ret = launch_gui_mode(mboot);
if (ret == ASYXBOX_SIGNAL_SWITCH_TERMINAL) mode = 0;
else break;
} else break;
}
}
while (1) {
asm volatile("hlt");
}
}
