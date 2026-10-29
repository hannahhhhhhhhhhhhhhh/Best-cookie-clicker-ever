#include <SPI.h>
#include <EEPROM.h>
#include <string.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_CS   1
#define TFT_DC   0
#define TFT_RST  29
#define TFT_SCK  2
#define TFT_MOSI 3
#define TFT_TAB  INITR_BLACKTAB

#define PIN_CLICK   26
#define PIN_CONFIRM 27
#define PIN_MINUS   28
#define PIN_GOLD    6
#define PIN_GAMBLE  7
#define PIN_PLUS    4

#define SCREEN_W 128
#define SCREEN_H 160

#define UNLOCK_AT     5000
#define GOLD_VALUE    2
#define BET_STEP      100
#define START_BET     100
#define DEBOUNCE_MS   15
#define REPEAT_DELAY  450
#define REPEAT_RATE   120
#define ROLL_MS       1400
#define RESULT_MS     4000
#define SAVE_INTERVAL 15000
#define RESET_HOLD_MS 3000
#define ART_CHANCE    30
#define ART_TOP       84
#define ART_BOTTOM    142
#define ART_STAR      6
#define SAVE_MAGIC    0xC00C1E55UL

#define RGB(r,g,b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define C_BG      RGB(10, 8, 20)
#define C_WHITE   RGB(255, 255, 255)
#define C_GREY    RGB(130, 130, 150)
#define C_GOLD    RGB(255, 200, 40)
#define C_RED     RGB(255, 70, 70)
#define C_GREEN   RGB(80, 255, 120)
#define C_CYAN    RGB(80, 230, 255)
#define C_PINK    RGB(255, 120, 200)
#define C_ORANGE  RGB(255, 140, 40)
#define C_BLUE_D  RGB(30, 60, 140)
#define C_RED_D   RGB(140, 30, 40)

struct Btn {
  uint8_t  pin;
  bool     raw;
  bool     stable;
  uint32_t edgeAt;
  uint32_t repeatAt;
};

struct Art {
  uint8_t     n;
  const char* r[7];
};

struct SaveData {
  uint32_t magic;
  uint32_t clicks;
  uint32_t wins;
  uint32_t losses;
  uint32_t unlocked;
  uint32_t check;
};

enum Mode   : uint8_t { M_CLICKER, M_GAMBLE };
enum GState : uint8_t { G_IDLE, G_ROLLING, G_WIN, G_LOSE };

const Art POOL[] = {
  {5, {"  .-~~~-.", " /  o  o \\", "|  o   o  |", " \\ o  o  /", "  '-___-'"}},
  {5, {" .-----. ", "| ^   ^ |", "|   o   |", "| \\___/ |", " '-----' "}},
  {3, {" /\\_/\\", "( o.o )", " > ^ <"}},
  {4, {" _  _ ", "( \\/ )", " \\  / ", "  \\/  "}},
  {6, {"   /\\", "  /  \\", " | () |", " |    |", " /_||_\\", "   ^^"}},
  {5, {" .-. ", "(o o)", "| O |", "|   |", "'~~~'"}},
  {5, {"    *    ", "   ***   ", " ******* ", "   ***   ", "  *   *  "}},
  {5, {"  ( ( (", " .-----.", " |     |]", " |     |", " '-----'"}},
};
const uint8_t POOL_N = sizeof(POOL) / sizeof(POOL[0]);

const Art ART_DICE = {5, {" .-----. ", " |o   o| ", " |  o  | ", " |o   o| ", " '-----' "}};
const Art ART_WIN  = {3, {"  \\o/  ", "   |   ", "  / \\  "}};
const Art ART_LOSE = {5, {"  _____  ", " /     \\", "| x   x |", "|  ___  |", " \\_____/"}};

const uint16_t PALETTE[] = {C_GOLD, C_CYAN, C_PINK, C_GREEN, C_ORANGE, C_WHITE};
const uint8_t  PALETTE_N = sizeof(PALETTE) / sizeof(PALETTE[0]);

Adafruit_ST7735 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(SCREEN_W, SCREEN_H);

Btn bClick   = {PIN_CLICK,   false, false, 0, 0};
Btn bGold    = {PIN_GOLD,    false, false, 0, 0};
Btn bGamble  = {PIN_GAMBLE,  false, false, 0, 0};
Btn bConfirm = {PIN_CONFIRM, false, false, 0, 0};
Btn bPlus    = {PIN_PLUS,    false, false, 0, 0};
Btn bMinus   = {PIN_MINUS,   false, false, 0, 0};

uint32_t clicks        = 0;
uint32_t wins          = 0;
uint32_t losses        = 0;
bool     bonusUnlocked = false;
uint32_t bet           = START_BET;
uint32_t lastBet       = START_BET;
bool     rollWin       = false;

Mode     mode = M_CLICKER;
GState   gs   = G_IDLE;
uint32_t gsAt = 0;

int      artIdx     = -1;
int      artX       = 0;
int      artY       = ART_TOP;
uint16_t artColor   = C_GOLD;
uint32_t artUntil   = 0;
uint32_t nextArtAt  = 0;

char     msg[24]    = "";
uint16_t msgColor   = C_WHITE;
uint32_t msgUntil   = 0;

bool     dirty      = false;
bool     needDraw   = true;
uint32_t lastDraw   = 0;
uint32_t lastSave   = 0;
uint32_t lastAnim   = 0;

bool     comboOn    = false;
bool     comboDone  = false;
uint32_t comboAt    = 0;

uint32_t checksum(const SaveData& s) {
  return s.magic ^ (s.clicks * 31u) ^ (s.wins * 17u) ^ (s.losses * 13u) ^ (s.unlocked * 7u) ^ 0xA5A5A5A5u;
}

void saveNow() {
  SaveData s = {(uint32_t)SAVE_MAGIC, clicks, wins, losses, bonusUnlocked ? 1u : 0u, 0};
  s.check = checksum(s);
  EEPROM.put(0, s);
  EEPROM.commit();
  dirty = false;
  lastSave = millis();
}

bool loadSave() {
  SaveData s;
  EEPROM.get(0, s);
  if (s.magic != (uint32_t)SAVE_MAGIC || s.check != checksum(s)) return false;
  clicks = s.clicks;
  wins = s.wins;
  losses = s.losses;
  bonusUnlocked = (s.unlocked == 1);
  return true;
}

void setMsg(const char* t, uint16_t c, uint32_t ms) {
  strncpy(msg, t, sizeof(msg) - 1);
  msg[sizeof(msg) - 1] = 0;
  msgColor = c;
  msgUntil = millis() + ms;
  needDraw = true;
}

void wipeSave() {
  clicks = 0;
  wins = 0;
  losses = 0;
  bonusUnlocked = false;
  bet = START_BET;
  gs = G_IDLE;
  mode = M_CLICKER;
  artIdx = -1;
  saveNow();
  setMsg("SAVE WIPED", C_RED, 2500);
}

bool pressed(Btn& b, uint32_t now) {
  bool r = (digitalRead(b.pin) == LOW);
  if (r != b.raw) {
    b.raw = r;
    b.edgeAt = now;
  }
  if ((now - b.edgeAt) >= DEBOUNCE_MS && r != b.stable) {
    b.stable = r;
    if (r) {
      b.repeatAt = now + REPEAT_DELAY;
      return true;
    }
  }
  return false;
}

bool repeating(Btn& b, uint32_t now) {
  if (b.stable && now >= b.repeatAt) {
    b.repeatAt = now + REPEAT_RATE;
    return true;
  }
  return false;
}

void fmt(uint32_t v, char* out) {
  char t[16];
  snprintf(t, sizeof(t), "%lu", (unsigned long)v);
  int n = strlen(t);
  int o = 0;
  for (int i = 0; i < n; i++) {
    if (i > 0 && (n - i) % 3 == 0) out[o++] = ',';
    out[o++] = t[i];
  }
  out[o] = 0;
}

void textCentered(const char* s, int y, uint8_t size, uint16_t col) {
  canvas.setTextSize(size);
  canvas.setTextColor(col);
  int w = strlen(s) * 6 * size;
  canvas.setCursor((SCREEN_W - w) / 2, y);
  canvas.print(s);
}

uint8_t artWidth(const Art& a) {
  uint8_t w = 0;
  for (uint8_t i = 0; i < a.n; i++) {
    uint8_t l = strlen(a.r[i]);
    if (l > w) w = l;
  }
  return w;
}

void drawArt(const Art& a, int x, int y, uint16_t col) {
  canvas.setTextSize(1);
  canvas.setTextColor(col);
  for (uint8_t i = 0; i < a.n; i++) {
    canvas.setCursor(x, y + i * 8);
    canvas.print(a.r[i]);
  }
}

void drawArtCentered(const Art& a, int y, uint16_t col) {
  drawArt(a, (SCREEN_W - artWidth(a) * 6) / 2, y, col);
}

void spawnArt(uint8_t idx, uint32_t ms) {
  const Art& a = POOL[idx];
  int maxX = SCREEN_W - artWidth(a) * 6 - 2;
  int h = a.n * 8;
  int maxY = (ART_BOTTOM - ART_TOP) - h;
  artIdx = idx;
  artX = 1 + (maxX > 0 ? random(0, maxX + 1) : 0);
  artY = ART_TOP + (maxY > 0 ? random(0, maxY + 1) : 0);
  artColor = PALETTE[random(PALETTE_N)];
  artUntil = millis() + ms;
  needDraw = true;
}

void clampBet() {
  uint32_t maxBet = (clicks / BET_STEP) * BET_STEP;
  if (bet > maxBet) bet = maxBet;
  if (bet < BET_STEP) bet = BET_STEP;
}

void addClicks(uint32_t n) {
  clicks += n;
  dirty = true;
  needDraw = true;
  if (mode == M_CLICKER && artIdx < 0 && random(ART_CHANCE) == 0) {
    spawnArt((uint8_t)random(POOL_N), random(2200, 4200));
  }
}

void startGamble(uint32_t now) {
  uint32_t maxBet = (clicks / BET_STEP) * BET_STEP;
  if (maxBet < BET_STEP) {
    setMsg("NEED 100 CLICKS", C_RED, 1500);
    return;
  }
  clampBet();
  lastBet = bet;
  clicks -= bet;
  rollWin = (rp2040.hwrand32() & 1) != 0;
  gs = G_ROLLING;
  gsAt = now;
  saveNow();
  needDraw = true;
}

void resolveGamble(uint32_t now) {
  if (rollWin) {
    clicks += lastBet * 2;
    wins++;
    gs = G_WIN;
  } else {
    losses++;
    gs = G_LOSE;
  }
  gsAt = now;
  saveNow();
  needDraw = true;
}

void renderGamble(uint32_t now) {
  char a[24];
  char b[40];

  if (gs == G_ROLLING) {
    drawArtCentered(ART_DICE, 86, ((now / 120) & 1) ? C_GOLD : C_WHITE);
    textCentered("ROLLING...", 130, 1, C_WHITE);
    return;
  }

  if (gs == G_WIN) {
    drawArtCentered(ART_WIN, 86, C_GREEN);
    fmt(lastBet, a);
    snprintf(b, sizeof(b), "YOU WIN +%s", a);
    textCentered(b, 116, 1, C_GREEN);
    textCentered("DOUBLED!", 128, 1, C_GOLD);
    return;
  }

  if (gs == G_LOSE) {
    drawArtCentered(ART_LOSE, 84, C_RED);
    fmt(lastBet, a);
    snprintf(b, sizeof(b), "YOU LOSE -%s", a);
    textCentered(b, 128, 1, C_RED);
    return;
  }

  uint32_t maxBet = (clicks / BET_STEP) * BET_STEP;
  if (maxBet < BET_STEP) {
    textCentered("NEED 100 CLICKS", 100, 1, C_RED);
    textCentered("TO GAMBLE", 112, 1, C_RED);
    return;
  }

  textCentered("DOUBLE OR NOTHING", 86, 1, C_WHITE);
  textCentered("BET", 98, 1, C_GREY);
  fmt(bet, a);
  uint8_t sz = (strlen(a) * 12 <= 124) ? 2 : 1;
  textCentered(a, sz == 2 ? 108 : 112, sz, C_GOLD);
  snprintf(b, sizeof(b), "WIN +%s", a);
  textCentered(b, 127, 1, C_GREEN);
  snprintf(b, sizeof(b), "LOSE -%s", a);
  textCentered(b, 137, 1, C_RED);
}

void render(uint32_t now) {
  char buf[32];
  bool gm = (mode == M_GAMBLE);

  canvas.fillScreen(C_BG);
  canvas.fillRect(0, 0, SCREEN_W, 12, gm ? C_RED_D : C_BLUE_D);
  textCentered(gm ? "GAMBLING DEN" : "COOKIE CLICKER", 2, 1, C_WHITE);

  fmt(clicks, buf);
  uint8_t len = strlen(buf);
  uint8_t sz = (len * 18 <= 124) ? 3 : ((len * 12 <= 124) ? 2 : 1);
  textCentered(buf, 20 + (3 - sz) * 4, sz, C_GOLD);
  textCentered("clicks", 48, 1, C_GREY);

  if (bonusUnlocked) {
    snprintf(buf, sizeof(buf), "GOLD KEY +%d", GOLD_VALUE);
    textCentered(buf, 62, 1, C_GOLD);
  } else {
    uint32_t c = clicks > UNLOCK_AT ? UNLOCK_AT : clicks;
    textCentered("GOLD KEY LOCKED", 58, 1, C_GREY);
    canvas.drawRect(7, 68, 114, 9, C_GREY);
    canvas.fillRect(8, 69, (c * 112) / UNLOCK_AT, 7, C_GOLD);
  }

  if (gm) {
    renderGamble(now);
  } else if (artIdx >= 0) {
    drawArt(POOL[artIdx], artX, artY, artColor);
  }

  if (msg[0]) {
    textCentered(msg, 148, 1, msgColor);
  } else if (gm) {
    snprintf(buf, sizeof(buf), "W:%lu L:%lu", (unsigned long)wins, (unsigned long)losses);
    textCentered(buf, 148, 1, C_GREY);
  }
}

void initButtons() {
  pinMode(PIN_CLICK,   INPUT_PULLUP);
  pinMode(PIN_GOLD,    INPUT_PULLUP);
  pinMode(PIN_GAMBLE,  INPUT_PULLUP);
  pinMode(PIN_CONFIRM, INPUT_PULLUP);
  pinMode(PIN_PLUS,    INPUT_PULLUP);
  pinMode(PIN_MINUS,   INPUT_PULLUP);
}

void setup() {
  initButtons();

  SPI.setRX(NOPIN);
  SPI.setSCK(TFT_SCK);
  SPI.setTX(TFT_MOSI);
  tft.initR(TFT_TAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  canvas.setTextWrap(false);

  randomSeed(rp2040.hwrand32());

  EEPROM.begin(sizeof(SaveData));
  if (loadSave()) setMsg("WELCOME BACK", C_CYAN, 2000);
  else            setMsg("NEW GAME", C_CYAN, 2000);

  nextArtAt = millis() + random(5000, 12000);
  lastSave = millis();
}

void loop() {
  uint32_t now = millis();

  bool evClick   = pressed(bClick, now);
  bool evGold    = pressed(bGold, now);
  bool evGamble  = pressed(bGamble, now);
  bool evConfirm = pressed(bConfirm, now);
  bool evPlus    = pressed(bPlus, now);
  bool evMinus   = pressed(bMinus, now);
  bool repPlus   = repeating(bPlus, now);
  bool repMinus  = repeating(bMinus, now);

  if (bPlus.stable && bMinus.stable) {
    if (!comboOn) {
      comboOn = true;
      comboAt = now;
    } else if (!comboDone && (now - comboAt) >= RESET_HOLD_MS) {
      comboDone = true;
      wipeSave();
    }
  } else {
    comboOn = false;
    comboDone = false;
  }

  if (evClick) addClicks(1);

  if (evGold) {
    if (bonusUnlocked) addClicks(GOLD_VALUE);
    else setMsg("LOCKED: 5,000 CLICKS", C_RED, 1500);
  }

  if (gs == G_ROLLING) {
    if ((now - gsAt) >= ROLL_MS) {
      resolveGamble(now);
    } else if ((now - lastAnim) >= 120) {
      lastAnim = now;
      needDraw = true;
    }
  } else {
    if (evGamble) {
      if (mode == M_CLICKER) {
        mode = M_GAMBLE;
        bet = START_BET;
        gs = G_IDLE;
        artIdx = -1;
      } else {
        mode = M_CLICKER;
        gs = G_IDLE;
        nextArtAt = now + random(3000, 8000);
      }
      needDraw = true;
    }

    if (mode == M_GAMBLE) {
      if (evConfirm) {
        startGamble(now);
      } else if (evPlus || repPlus) {
        if (gs != G_IDLE) {
          gs = G_IDLE;
          clampBet();
        }
        uint32_t maxBet = (clicks / BET_STEP) * BET_STEP;
        if (bet + BET_STEP <= maxBet) bet += BET_STEP;
        else setMsg("MAX BET", C_ORANGE, 800);
        needDraw = true;
      } else if (evMinus || repMinus) {
        if (gs != G_IDLE) {
          gs = G_IDLE;
          clampBet();
        }
        if (bet > BET_STEP) bet -= BET_STEP;
        else setMsg("MIN BET 100", C_ORANGE, 800);
        needDraw = true;
      }

      if ((gs == G_WIN || gs == G_LOSE) && (now - gsAt) >= RESULT_MS) {
        gs = G_IDLE;
        clampBet();
        needDraw = true;
      }
    }
  }

  if (!bonusUnlocked && clicks >= UNLOCK_AT) {
    bonusUnlocked = true;
    saveNow();
    setMsg("GOLD KEY UNLOCKED!", C_GOLD, 4000);
    if (mode == M_CLICKER) spawnArt(ART_STAR, 4000);
  }

  if (artIdx >= 0 && now >= artUntil) {
    artIdx = -1;
    needDraw = true;
  }

  if (mode == M_CLICKER && artIdx < 0 && now >= nextArtAt) {
    spawnArt((uint8_t)random(POOL_N), random(2500, 4500));
    nextArtAt = now + random(6000, 16000);
  }

  if (msg[0] && now >= msgUntil) {
    msg[0] = 0;
    needDraw = true;
  }

  if (dirty && gs != G_ROLLING && (now - lastSave) >= SAVE_INTERVAL) {
    saveNow();
  }

  if (needDraw && (now - lastDraw) >= 20) {
    needDraw = false;
    lastDraw = now;
    render(now);
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), SCREEN_W, SCREEN_H);
  }
}
