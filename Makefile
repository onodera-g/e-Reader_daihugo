# ===== e-Reader build (quiet by default, verbose with v=1 or V=1) =====

# --- verbose: V=1 も v=1 にエイリアス
ifeq ($(origin V), command line)
  v := $(V)
endif
ifeq ($(v),1)
  Q :=
else
  Q := @
endif

# 簡易ターゲット: make r2 / r1
.PHONY: r2 r1
r2: ; $(MAKE) bmp REGION=2 v=$(v)
r1: ; $(MAKE) bmp REGION=1 v=$(v)

# --- 基本設定 ---
OUT       := daihugo
OUTDIR  := build
$(shell mkdir -p $(OUTDIR))
NAME_KEY  ?= daihugo

DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
export PATH := $(DEVKITPRO)/tools/bin:$(PATH)

CC      := $(DEVKITARM)/bin/arm-none-eabi-gcc
AS      := $(DEVKITARM)/bin/arm-none-eabi-as
OBJCOPY := $(DEVKITARM)/bin/arm-none-eabi-objcopy

CFLAGS  := -mthumb -mcpu=arm7tdmi -Os -ffunction-sections -fdata-sections \
           -fno-builtin -fomit-frame-pointer -Wall -Wextra -Iinclude
LDFLAGS := -T ereader.ld -nostdlib -Wl,--gc-sections
LIBS    := -lgcc

# --- ソース（PSGドライバは使わないので除外） ---
SRCS := $(filter-out src/sound.c,$(wildcard src/*.c))
ASMS := crt0.s
OBJS := $(SRCS:.c=.o) $(ASMS:.s=.o)

# --- nedclib tools ---
TOOLS   ?= $(CURDIR)/tools/bin
NEVPK    = $(TOOLS)/nevpk
NEDCMAKE = $(TOOLS)/nedcmake
RAW2BMP  = $(TOOLS)/raw2bmp

RAW_GLOB := $(OUTDIR)/card-*.raw

# --- 地域設定 ---
REGION   ?= 2
ENCODING := ASCII//TRANSLIT
ifeq ($(REGION),2)
  ENCODING := SHIFT_JIS
endif

# --- UIラベル用（UTF-8 → #define 群） ---
GENHDR := include/messages_autogen.h
MSGTXT := text/messages_jpn.txt

# --- ログ ---
LOGDIR   := build
RAW_LOG  := $(LOGDIR)/raw.log
BMP_LOG  := $(LOGDIR)/bmp.log

.PHONY: all clean gba vpk raw bmp check_cards info
all: $(GENHDR) bmp

info:
	@echo "[info] OUT='$(OUT)' REGION=$(REGION)"

# --- ヘッダ自動生成（messages_jpn.txt → messages_autogen.h） ---
$(GENHDR): $(MSGTXT) scripts/msg2h.py
	$(Q)mkdir -p include
	$(Q)python3 scripts/msg2h.py $(MSGTXT) $@

# --- ビルド ---
src/%.o: src/%.c $(GENHDR)
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.s
	$(Q)$(AS) -o $@ $<

$(OUTDIR)/$(OUT).elf: $(OBJS) ereader.ld
	$(Q)$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)

$(OUTDIR)/$(OUT).gba: $(OUTDIR)/$(OUT).elf
	$(Q)$(OBJCOPY) -O binary $< $@

$(OUTDIR)/$(OUT).vpk: $(OUTDIR)/$(OUT).gba
	$(Q)"$(NEVPK)" -i "$<" -o "$@" -c

# --- VPK → RAW（カード名は messages_jpn.txt の NAME_KEY を使用） ---
raw: $(OUTDIR)/$(OUT).vpk
	$(Q)mkdir -p $(LOGDIR)
	$(Q)rm -f $(RAW_GLOB) $(OUTDIR)/card*.bin $(RAW_LOG)
	$(Q)NAME_OPT=; \
	name_utf8=$$(awk 'NR==1{sub(/^\xef\xbb\xbf/,"")} {gsub(/\r/,"")} !/^[[:space:]]*(#|$$)/ { if (match($$0,/^'$(NAME_KEY)'[[:space:]]*=/)) {sub(/^[^=]*=/,""); print; exit} }' "$(MSGTXT)"); \
	if [ -n "$$name_utf8" ]; then \
	  if [ "$(REGION)" = "2" ]; then \
	    name_enc=$$(printf '%s' "$$name_utf8" | iconv -f UTF-8 -t SHIFT_JIS 2>/dev/null | tr -d '\n'); \
	  else \
	    name_enc=$$(printf '%s' "$$name_utf8" | iconv -f UTF-8 -t ASCII//TRANSLIT 2>/dev/null | tr -d '\n'); \
	  fi; \
	  NAME_OPT="-name \"$$name_enc\""; \
	fi; \
	{ "$(NEDCMAKE)" -i "$(OUTDIR)/$(OUT).vpk" -type 2 -region $(REGION) $$NAME_OPT -bin -raw -dcsize 0 -fill 1 -save 0 -o "$(OUTDIR)/card"; } \
	  >"$(RAW_LOG)" 2>&1; \
	rc=$$?; \
	if [ $$rc -ne 0 ]; then \
	  echo "[RAW] FAILED (nedcmake rc=$$rc)"; \
	  cat "$(RAW_LOG)"; exit $$rc; \
	fi; \
	cnt=$$(ls -1 $(RAW_GLOB) 2>/dev/null | wc -l | tr -d ' '); \
	if [ "$$cnt" -eq 0 ]; then \
	  echo "[RAW] FAILED (no card-*.raw produced)"; \
	  cat "$(RAW_LOG)"; exit 1; \
	fi; \
	for f in $$(ls -1 $(RAW_GLOB)); do echo "[RAW] OK: $$f"; done; \
	$(MAKE) check_cards v=$(v)

# --- RAW → BMP ---
$(OUTDIR)/$(OUT).bmp: raw
	$(Q)mkdir -p $(LOGDIR)
	$(Q)rm -f $(BMP_LOG) $(OUTDIR)/$(OUT).bmp
	$(Q){ "$(RAW2BMP)" -i "$(OUTDIR)/card-01.raw" -o "$(OUTDIR)/$(OUT)" -dpi 300; } \
	  >"$(BMP_LOG)" 2>&1; \
	rc=$$?; \
	if [ $$rc -ne 0 ]; then \
	  echo "[BMP] FAILED (raw2bmp rc=$$rc)"; \
	  cat "$(BMP_LOG)"; exit $$rc; \
	fi; \
	echo "[BMP] OK: $(OUTDIR)/$(OUT).bmp"

bmp: $(OUTDIR)/$(OUT).bmp
vpk: $(OUTDIR)/$(OUT).vpk
gba: $(OUTDIR)/$(OUT).gba

check_cards:
	$(Q)set -e; \
	cnt=$$(ls -1 $(RAW_GLOB) 2>/dev/null | wc -l | tr -d ' '); \
	echo "==> Cards: $$cnt file(s)"; \
	if [ "$$cnt" -gt 4 ]; then echo "ERROR: $$cnt cards (>4)."; exit 2; fi

clean:
	$(Q)rm -f $(OBJS) $(GENHDR)
	$(Q)rm -rf $(OUTDIR) $(LOGDIR)
