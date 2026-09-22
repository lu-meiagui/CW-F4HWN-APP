#pragma GCC optimize ("Os", "no-tree-loop-distribute-patterns")
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../app_api.h"

#define TICK_MS     50
#define RX_TICK     8
#define MSG_MAX     64
#define TAP_MS      900
#define UNIT_DEF    150
#define UNIT_MIN    15
#define UNIT_MAX    300
#define UNIT_STEP   5
#define AF_BEEP     3
#define HOLD_MS     1500
#define REPEAT_MS   3000
#define CFG_MAGIC   0xD0

#define F_RUN   (1<<0)
#define F_RX    (1<<1)
#define F_CARR  (1<<2)
#define F_AUTO  (1<<3)
#define F_FDOWN (1<<4)
#define F_MARK  (1<<5)
#define F_PEND  (1<<6)

void *memset(void *s, int c, size_t n){
    uint8_t *p = (uint8_t*)s;
    while(n--) *p++ = (uint8_t)c;
    return s;
}

static const uint8_t M_LET[26]={0x05,0x18,0x1A,0x0C,0x02,0x12,0x0E,0x10,0x04,0x17,0x0D,0x14,0x07,0x06,0x0F,0x16,0x1D,0x0A,0x08,0x03,0x09,0x11,0x0B,0x19,0x1B,0x1C};
static const uint8_t M_DIG[10]={0x3F,0x2F,0x27,0x23,0x21,0x20,0x30,0x38,0x3C,0x3E};
static const char KEYMAP_FLAT[]="\0""1.0/""\0""ABC2""\0""DEF3""\0""GHI4""\0""JKL5""\0""MNO6""\0""PQRS7""\0""TUV8""\0""WXYZ9";
static const char PROSIGN_FLAT[]=" \0""SK\0""AR\0""BT\0""KN\0""K\0""CQ\0""DE\0""73";
static const uint8_t BMP_TX[16]={0x1c,0x22,0x41,0x1c,0x22,0x00,0x08,0x1c,0x1c,0x08,0x00,0x22,0x1c,0x41,0x22,0x1c};
static const app_api_t *A;

static struct {
    uint16_t tapMs, fTapMs, unitMs, fHoldMs, pttHoldMs, rxRun, toneHz;
    int16_t  rxFloor, rxPeak, smooth_rssi;
    uint8_t  flags, msgLen, rxLen, lastKey, tapIdx, prevKey, rxCode, rxAdapt, batTimer, txPauseAt;
    bool     txPaused;
    char     msg[MSG_MAX+1];
    char     rxBuf[MSG_MAX+1];
} G;

static uint32_t udiv(uint32_t n, uint32_t d, uint32_t *rem){
    uint32_t q = 0, r = 0;
    for(int8_t i = 31; i >= 0; i--){
        r = (r << 1) | ((n >> i) & 1u);
        if(r >= d){ r -= d; q |= (1u << i); }
    }
    if(rem) *rem = r;
    return q;
}

static const char *getFlatStr(const char *buf, uint8_t idx){
    while(idx--) while(*buf++) {}
    return buf;
}

static char *pstr(char *d, const char *s){
    while((*d = *s++)) d++;
    return d;
}

static char *pnum(char *d, uint32_t v, uint8_t pad){
    char t[10]; int n = 0;
    do { uint32_t r; v = udiv(v, 10, &r); t[n++] = (char)('0' + r); } while(v);
    while(n < pad) t[n++] = '0';
    while(n--) *d++ = t[n];
    *d = 0;
    return d;
}

static uint8_t morseByte(char c){
    if(c >= 'a' && c <= 'z') c -= 32;
    if(c >= 'A' && c <= 'Z') return M_LET[c - 'A'];
    if(c >= '0' && c <= '9') return M_DIG[c - '0'];
    if(c == '/') return 0x32;
    if(c == '.') return 0x55;
    return 0;
}

static char morseChar(uint8_t code){
    if(code < 2) return 0;
    for(uint8_t i=0; i<26; i++) if(M_LET[i] == code) return (char)('A' + i);
    for(uint8_t i=0; i<10; i++) if(M_DIG[i] == code) return (char)('0' + i);
    if(code == 0x32) return '/';
    if(code == 0x55) return '.';
    return '?';
}

static void beepAlert(uint16_t ms){
    A->set_af(AF_BEEP); A->audio_path(true);
    A->tx_tone(800); A->tx_mute(false);
    A->delay_ms(ms);
    A->tx_mute(true); A->audio_path(false);
}

static bool rxEndsWith(const char *s, uint8_t n){
    if(G.rxLen < n) return false;
    for(uint8_t i=0; i<n; i++) if(G.rxBuf[G.rxLen - n + i] != s[i]) return false;
    return true;
}

static void rxCheckAlert(void){
    const char *id = getFlatStr(PROSIGN_FLAT, 0);
    uint8_t idLen = 0;
    while(id[idLen]) idLen++;
    if(idLen > 0 && rxEndsWith(id, idLen)){
        for(uint8_t i=0; i<3; i++){ A->led(true); beepAlert(80); A->led(false); A->delay_ms(80); }
        return;
    }
    if(G.rxLen >= 2 && G.rxBuf[G.rxLen-2] == 'C' && G.rxBuf[G.rxLen-1] == 'Q'){
        A->led(true); beepAlert(300); A->led(false);
    }
}

static void blitAll(void){ 
    A->blit_status(); A->blit_full(); 
}

static void chrome(void){
    char b[24], *p;
    A->display_clear(); A->status_clear();
    A->print_inverse((G.flags & F_RX) ? "CW RX" : "CW TX", 2, 0, true, true, 24);
    A->draw_battery();
    uint32_t rem, f = udiv((G.flags & F_RX) ? A->rx_freq() : A->tx_freq(), 100000u, &rem);
    p = pnum(b, f, 1); *p++ = '.'; p = pnum(p, rem, 5);
    A->print_normal(b, (uint8_t)(126 - (p - b) * 7), 0, 6);
}

static void drawText(const char *s, uint8_t len){
    char b[17];
    for(int r = 0; r < 4; r++){
        int n = 0;
        for(int i = r * 16; i < (r * 16 + 16) && i < len; i++) b[n++] = s[i];
        b[n] = 0;
        if(n) A->print_normal(b, 0, 127, r + 1);
        if(n < 16) break;
    }
}

static void draw(void){
    char b[24], *p;
    chrome();
    if(G.flags & F_RX){
        drawText(G.rxBuf, G.rxLen);
        p = b; uint32_t bit = 0x80;
        while(bit > 1 && !(G.rxCode & bit)) bit >>= 1;
        for(bit >>= 1; bit; bit >>= 1) *p++ = (G.rxCode & bit) ? '-' : '.';
        *p = 0; A->print_normal(b, 0, 127, 4);
        int16_t r = A->rssi_dbm();
        for(int i = 0; i < 16; i++) A->status_line[28 + i] = (r >= (-120 + i * 3)) ? 0x3C : 0;
    } else {
        drawText(G.msg, G.msgLen);
    }
    
    if(G.txPaused && G.txPauseAt < G.msgLen){
        p = pstr(b, "PAUSE "); p = pnum(p, G.txPauseAt + 1, 1);
        *p++ = '/'; p = pnum(p, G.msgLen, 1);
    } else {
        p = pstr(b, "WPM "); p = pnum(p, udiv(1200u, G.unitMs, NULL), 1);
        if(!(G.flags & F_RX)){ *p++ = ' '; p = pnum(p, G.msgLen, 1); *p++ = '/'; p = pnum(p, MSG_MAX, 1); }
        if(G.flags & F_CARR){ p = pstr(p, " CARR"); }
        else{ *p++ = ' '; p = pnum(p, G.toneHz, 1); p = pstr(p, "Hz"); }
        if(G.flags & F_AUTO) p = pstr(p, " AUTO");
    }
    A->print_normal(b, 2, 0, 5);
}

static void drawAndBlit(void){ draw(); blitAll(); }

static void keyOn(void){
    A->tx_carrier(true);
    if(G.flags & F_CARR){ A->tx_tone(G.toneHz); A->set_af(AF_BEEP); A->audio_path(true); }
    A->tx_mute(false);
}

static void keyOff(void){
    A->tx_mute(true);
    if(G.flags & F_CARR) A->tx_carrier(false);
}

static uint8_t sendDelay(uint16_t ms, bool anyKeyStops){
    while(ms){
        uint16_t sl = (ms > 10u) ? 10u : ms;
        A->delay_ms(sl); ms -= sl;
        A->backlight_update();
        uint8_t k = A->get_key();
        
        if(k == APP_KEY_EXIT) return 1;
        if(k == APP_KEY_PTT && !anyKeyStops) { G.txPaused = true; return 1; }
        if(anyKeyStops && k != APP_KEY_INVALID) return 2;
    }
    return 0;
}

static bool sendChar(char c){
    uint8_t code = morseByte(c);
    if(code == 0) return sendDelay((uint16_t)(G.unitMs * 4u), false) != 0;
    uint32_t bit = 0x80;
    while(!(code & bit)) bit >>= 1;
    for(bit >>= 1; bit; bit >>= 1){
        keyOn();
        if(sendDelay((code & bit) ? (uint16_t)(G.unitMs * 3u) : G.unitMs, false)){ keyOff(); return true; }
        keyOff();
        if(sendDelay(G.unitMs, false)) return true;
    }
    return sendDelay((uint16_t)(G.unitMs * 2u), false) != 0;
}

static void drainKeys(void){
    while(A->get_key() != APP_KEY_INVALID) A->delay_ms(10);
    G.prevKey = APP_KEY_INVALID;
}

static bool sendOnce(void){
    char b[16], *p;
    chrome();
    for(int i = 0; i < 16; i++) A->status_line[48 + i] = BMP_TX[i];
    drawText(G.msg, G.msgLen);
    blitAll();
    
    A->tx_set_params();
    A->tx_tone(G.toneHz); A->set_af(AF_BEEP); A->audio_path(true); A->tx_mute(true);
    bool aborted = false;
    
    G.txPaused = false;

    for(uint8_t i = G.txPauseAt; i < G.msgLen; i++){
        p = pstr(b, "TX "); p = pnum(p, i + 1, 1);
        *p++ = '/'; p = pnum(p, G.msgLen, 1);
        A->print_string(b, 0, 127, 5, 8);
        blitAll();
        
        if(sendChar(G.msg[i])){
            aborted = true;
            if(G.txPaused) G.txPauseAt = i;
            break; 
        }
    }
    
    if(G.txPaused){
        p = pstr(b, "PAUSE "); p = pnum(p, G.txPauseAt + 1, 1);
        *p++ = '/'; p = pnum(p, G.msgLen, 1);
        A->print_string(b, 0, 127, 5, 8);
        blitAll();
    }
    
    keyOff(); A->audio_path(false); A->tx_end();
    if(!G.txPaused) G.txPauseAt = 0;
    return !aborted;
}

static void sendMessage(void){
    if(G.msgLen == 0) return;
    if(!G.txPaused) G.txPauseAt = 0;
    bool ok = sendOnce();
    drainKeys(); 
    if(G.txPaused) return;
    
    while(ok && (G.flags & F_AUTO) && (G.flags & F_RUN)){
        uint8_t r = sendDelay(REPEAT_MS, true);
        if(r == 1){ G.flags &= ~F_AUTO; drainKeys(); drawAndBlit(); break; }
        if(r == 2) break;
        G.txPauseAt = 0; G.txPaused = false; ok = sendOnce(); drainKeys();
        if(G.txPaused) return;
    }
}

static void rxPush(char c){
    if(!c) return;
    if(G.rxLen >= MSG_MAX - 1){
        for(int i = 0; i < MSG_MAX - 1; i++) G.rxBuf[i] = G.rxBuf[i + 1];
        G.rxLen = MSG_MAX - 2;
    }
    G.rxBuf[G.rxLen++] = c; G.rxBuf[G.rxLen] = 0;
    rxCheckAlert();
}

static void rxFlush(void){
    if(G.flags & F_PEND){ rxPush(morseChar(G.rxCode)); G.rxCode = 1; G.flags &= ~F_PEND; }
}

static void rxReset(void){
    G.rxLen = 0; G.rxBuf[0] = 0; G.rxCode = 1;
    G.flags &= ~(F_PEND | F_MARK);
    G.rxRun = 0; G.rxAdapt = 0; G.rxFloor = -130; G.rxPeak = -120;
    G.smooth_rssi = -130;
}

static void rxSample(void){
    int16_t raw = A->rssi_dbm();
    G.smooth_rssi = (int16_t)(G.smooth_rssi + ((raw - G.smooth_rssi) >> 1));
    int16_t r = G.smooth_rssi;

    if(r < G.rxFloor) G.rxFloor = r;
    if(r > G.rxPeak) G.rxPeak = r;
    if(++G.rxAdapt >= 16){
        G.rxAdapt = 0;
        if(G.rxFloor < r) G.rxFloor++;
        if(G.rxPeak > r) G.rxPeak--;
    }
    
    if(G.rxPeak < (int16_t)(G.rxFloor + 14)) G.rxPeak = (int16_t)(G.rxFloor + 14);

    int16_t range = G.rxPeak - G.rxFloor;
    int16_t third = (int16_t)udiv((uint32_t)range, 3u, NULL);
    int16_t th_high = G.rxFloor + (third << 1); 
    int16_t th_low = G.rxFloor + third;         

    bool curr_mark = ((G.flags & F_MARK) != 0);
    bool mark = curr_mark;

    if (!curr_mark && r > th_high) mark = true;
    else if (curr_mark && r < th_low) mark = false;

    if(mark == curr_mark){
        if(G.rxRun < 60000u) G.rxRun += RX_TICK;
        else if(G.flags & F_PEND) rxFlush();
        return;
    }

    A->led(mark);
    uint16_t d = G.rxRun; G.rxRun = 0;
    
    if(curr_mark){
        G.rxCode = (uint8_t)((G.rxCode << 1) | ((d >= (uint16_t)(G.unitMs * 2u)) ? 1u : 0u));
        G.flags |= F_PEND;
        if(G.rxCode > 0x7F){ rxPush('?'); G.rxCode = 1; G.flags &= ~F_PEND; }
    } else {
        if(d >= (uint16_t)(G.unitMs * 5u)){ rxFlush(); rxPush(' '); }
        else if(d >= (uint16_t)(G.unitMs * 2u)) rxFlush();
    }
    
    if(mark) G.flags |= F_MARK; else G.flags &= ~F_MARK;
}

static void setMode(bool rx){
    if(rx){ G.flags |= F_RX; rxReset(); A->set_af(APP_AF_FM); A->audio_path(true); }
    else{ G.flags &= ~F_RX; A->audio_path(false); A->led(false); }
}

static void handleTap(uint8_t key){
    if(G.flags & F_RX) return; 
    if(key == APP_KEY_PTT){ sendMessage(); return; }
    
    G.txPaused = false; G.txPauseAt = 0;
    
    if(key == APP_KEY_0){
        if(G.msgLen > 0) G.msg[--G.msgLen] = 0;
        G.lastKey = APP_KEY_INVALID; G.tapMs = 0;
    } else if(key == APP_KEY_STAR){
        if(G.msgLen < MSG_MAX){ G.msg[G.msgLen++] = ' '; G.msg[G.msgLen] = 0; }
        G.lastKey = APP_KEY_INVALID; G.tapMs = 0;
    } else if(key <= APP_KEY_9){
        const char *set = getFlatStr(KEYMAP_FLAT, key);
        if(set[0]){
            if(key == G.lastKey && G.tapMs > 0 && G.msgLen > 0){
                G.tapIdx++; if(!set[G.tapIdx]) G.tapIdx = 0;
                G.msg[G.msgLen - 1] = set[G.tapIdx];
            } else if(G.msgLen < MSG_MAX){
                G.tapIdx = 0; G.msg[G.msgLen++] = set[0]; G.msg[G.msgLen] = 0;
            }
            G.lastKey = key; G.tapMs = TAP_MS;
        }
    }
}

static void holdTick(void){
    uint8_t key = A->get_key();
    if(key == APP_KEY_F){
        if(G.fHoldMs < HOLD_MS + TICK_MS) G.fHoldMs += TICK_MS;
        if(G.fHoldMs == HOLD_MS){ G.flags ^= F_CARR; G.flags &= ~F_FDOWN; G.fTapMs = 0; drawAndBlit(); }
    } else {
        if(G.fHoldMs > 0){
            if(G.fHoldMs < HOLD_MS){
                if(G.fTapMs > 0){
                    if(G.flags & F_RX) rxReset(); else { G.msgLen = 0; G.msg[0] = 0; }
                    G.flags &= ~F_FDOWN; G.fTapMs = 0; G.lastKey = APP_KEY_INVALID; G.tapMs = 0;
                } else { G.flags ^= F_FDOWN; G.fTapMs = 400; } 
                drawAndBlit();
            }
            G.fHoldMs = 0;
        }
    }
    if(key == APP_KEY_PTT){
        if(G.pttHoldMs < HOLD_MS + TICK_MS) G.pttHoldMs += TICK_MS;
        if(G.pttHoldMs == HOLD_MS){ G.flags ^= F_AUTO; drawAndBlit(); }
    } else {
        if(G.pttHoldMs > 0 && G.pttHoldMs < HOLD_MS){ handleTap(APP_KEY_PTT); drawAndBlit(); }
        G.pttHoldMs = 0;
    }
}

static void loadConfig(void){
    uint8_t c[8]; A->cfg_load(c, 8);
    if(c[0] == CFG_MAGIC){
        uint16_t u = c[1] | (c[2] << 8);
        if(u >= UNIT_MIN && u <= UNIT_MAX) G.unitMs = u;
        if(c[3]) G.flags |= F_RX;
        if(c[4]) G.flags |= F_CARR;
        if(c[5]) G.flags |= F_AUTO;
        uint16_t t = c[6] | (c[7] << 8);
        if(t >= 500 && t <= 1200) G.toneHz = t;
    }
}

static void saveConfig(void){
    uint8_t c[8] = {CFG_MAGIC, G.unitMs & 0xFF, G.unitMs >> 8,
        (G.flags & F_RX) ? 1 : 0, (G.flags & F_CARR) ? 1 : 0, (G.flags & F_AUTO) ? 1 : 0,
        G.toneHz & 0xFF, G.toneHz >> 8};
    A->cfg_save(c, 8);
}

__attribute__((section(".text.entry"), used))
void app_main(const app_api_t *api){
    uint8_t rxAcc = 0;
    A = api;
    
    memset(&G, 0, sizeof(G));
    G.lastKey = APP_KEY_INVALID;
    G.prevKey = APP_KEY_INVALID;
    G.unitMs = UNIT_DEF;
    G.toneHz = 1000;
    G.flags = F_RUN;
    rxReset();
    
    loadConfig();
    if(G.flags & F_RX) setMode(true);
    A->backlight_on();
    drawAndBlit();

    while(G.flags & F_RUN){
        uint8_t key = A->get_key();

        if(key == APP_KEY_EXIT && key != G.prevKey){
            if(G.txPaused){
                G.txPaused = false; G.txPauseAt = 0;
                G.prevKey = key; drawAndBlit(); continue;
            }
            G.flags &= ~F_RUN; G.prevKey = key; continue;
        }

        if((G.flags & F_FDOWN) && key != G.prevKey && key != APP_KEY_INVALID){
            bool handled = true;
            if(key == APP_KEY_0){ if(G.toneHz > 500) G.toneHz -= 100; }
            else if(key == APP_KEY_STAR){ if(G.toneHz < 1200) G.toneHz += 100; }
            else if(key >= APP_KEY_1 && key <= APP_KEY_9){
                if(!(G.flags & F_RX)){
                    const char *s = getFlatStr(PROSIGN_FLAT, key - APP_KEY_1);
                    while(*s && G.msgLen < MSG_MAX) G.msg[G.msgLen++] = *s++;
                    G.msg[G.msgLen] = 0; G.lastKey = APP_KEY_INVALID; G.tapMs = 0;
                }
            } else handled = false;
            if(handled){ G.flags &= ~F_FDOWN; G.fTapMs = 0; G.prevKey = key; drawAndBlit(); }
        } else if(key != APP_KEY_INVALID && key != G.prevKey && key != APP_KEY_F && key != APP_KEY_PTT){
            G.prevKey = key; A->backlight_on();
            if(key == APP_KEY_MENU) setMode(!(G.flags & F_RX));
            else {
                int8_t dir = A->nav_dir(key);
                if(dir > 0){ if(G.unitMs > UNIT_MIN) G.unitMs -= UNIT_STEP; }
                else if(dir < 0){ if(G.unitMs < UNIT_MAX) G.unitMs += UNIT_STEP; }
                else handleTap(key);
            }
            if(G.flags & F_RUN) drawAndBlit();
        } else if(key != APP_KEY_F && key != APP_KEY_PTT){
            G.prevKey = key;
        }
        holdTick();

        if(G.tapMs > TICK_MS) G.tapMs -= TICK_MS; else if(G.tapMs){ G.tapMs = 0; G.lastKey = APP_KEY_INVALID; }
        if(G.fTapMs > TICK_MS) G.fTapMs -= TICK_MS; else if(G.fTapMs) G.fTapMs = 0;

        if(G.flags & F_RX){
            for(int i = 0; i < TICK_MS / RX_TICK; i++){ rxSample(); A->delay_ms(RX_TICK); }
            if(++rxAcc >= 8){ rxAcc = 0; drawAndBlit(); }
        } else A->delay_ms(TICK_MS);
        
        if(!(G.flags & F_MARK) && ++G.batTimer >= 40){ G.batTimer = 0; A->battery_sample(); }
        A->backlight_update();
    }
    
    saveConfig();
    A->audio_path(false);
    A->led(false);
    A->tx_end();
}
