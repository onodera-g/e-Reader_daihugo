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
# NAME_KEY は不要になったので削除

DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
export PATH := $(DEVKITPRO)/tools/bin:$(PATH)

CC      := $(DEVKITARM)/bin/arm-none-eabi-gcc
AS      := $(DEVKITARM)/bin/arm-none-eabi-as
OBJCOPY := $(DEVKITARM)/bin/arm-none-eabi-objcopy

 CFLAGS  := -mthumb -mcpu=arm7tdmi -Os -ffunction-sections -fdata-sections \
            -fno-builtin -fomit-frame-pointer -Wall -Wextra -Iinclude \
            -I$(DEVKITPRO)/libgba/include
LDFLAGS := -T ereader.ld -nostdlib -Wl,--gc-sections 
LIBS    := -lgcc

# --- ソース（PSGドライバは使わないので除外） ---
SRCS := $(wildcard src/*.c)
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

# --- UIラベル用：message.txt は廃止 ---
# GENHDR / MSGTXT は使わないので定義ごと削除

# --- ログ ---
LOGDIR   := build
RAW_LOG  := $(LOGDIR)/raw.log
BMP_LOG  := $(LOGDIR)/bmp.log

.PHONY: all clean gba vpk raw bmp check_cards info
all: bmp

info:
	@echo "[info] OUT='$(OUT)' REGION=$(REGION)"

# --- ビルド ---
# (GENHDR 依存を削除)
src/%.o: src/%.c
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.s
	$(Q)$(AS) -o $@ $<

$(OUTDIR)/$(OUT).elf: $(OBJS) ereader.ld
	$(Q)$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)

$(OUTDIR)/$(OUT).gba: $(OUTDIR)/$(OUT).elf
	$(Q)$(OBJCOPY) -O binary $< $@

$(OUTDIR)/$(OUT).vpk: $(OUTDIR)/$(OUT).gba
	$(Q)"$(NEVPK)" -i "$<" -o "$@" -c

# --- VPK → RAW（タイトル名は常に "daihugo" 固定） ---
raw: $(OUTDIR)/$(OUT).vpk
	$(Q)mkdir -p $(LOGDIR)
	$(Q)rm -f $(RAW_GLOB) $(OUTDIR)/card*.bin $(RAW_LOG)
	$(Q){ "$(NEDCMAKE)" -i "$(OUTDIR)/$(OUT).vpk" -type 2 -region $(REGION) \
	       -name "daihugo" -bin -raw -dcsize 0 -fill 1 -save 0 -o "$(OUTDIR)/card"; } \
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
	$(Q)rm -f $(OBJS)
	$(Q)rm -rf $(OUTDIR) $(LOGDIR)
