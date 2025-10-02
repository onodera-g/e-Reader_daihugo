// src/game.c
#include "game.h"
#include "rng.h"
#include "cards.h"
#include "render.h"
#include "def.h"
#include "ai.h"
#include "deck.h"

/* ── 初期化 ── */
void game_init(GameState* g, const Hand hands[PLAYERS], int start_player_for_deal){
  // 可視枚数と目標枚数を設定
  for (int p=0;p<PLAYERS;++p){
    g->visible[p] = 0;
    g->target[p]  = hands[p].count;
  }
  // 配り状態
  g->deal_turn   = start_player_for_deal & 3;
  g->deal_delay  = 0;
  g->deal_done   = 0; 
  // 場
  g->field_visible   = 0;
  g->last_played     = -1;
  g->pass_count      = 0;
  // 縛り/8切り
  g->yagiri_waiting  = 0;
  g->yagiri_timer    = 0;
  g->sfx_yagiri      = 0;
  // 複数出し用
  g->field_suit_mask = 0;
  g->field_count     = 0;
  g->field_eff_rank  = 0;
  for (int i=0;i<4;++i) g->field_names[i] = NULL;

  g->sibari_waiting  = 0;
  g->sibari_timer    = 0;
  g->sfx_sibari      = 0;
  // ターン管理
  g->turn_player = 0;
  g->turn_delay  = 0;
}


/* 手札配列から index の1枚を削除（左詰め） */
static void remove_card_at(Hand* h, int index){
  for (int i=index+1;i<h->count;++i) h->cards[i-1] = h->cards[i];
  h->count--;
}

/* 指定カード名（ポインタ一致）を1枚だけ取り除く。成功で 1 */
static int remove_card_name_once(Hand* h, const char* name){
  for (int i=0;i<h->count;++i){
    if (h->cards[i] == name){ remove_card_at(h, i); return 1; }
  }
  return 0;
}

/* 全員の配り目標に達したか？ */
static int deal_finished_all(const GameState* g){
  for (int p=0; p<PLAYERS; ++p){
    if (g->visible[p] < g->target[p]) return 0;
  }
  return 1;
}

/* target を手札枚数から再計算（描画側が呼ぶ前提の補助） */
void game_set_targets(GameState* g, const Hand hands[PLAYERS]){
  for (int p=0;p<PLAYERS;++p){
    g->target[p] = hands[p].count;
  }
}

/* ── 配り演出(1枚) ── 
   - 全プレイヤーを走査して、目標枚数に達していないプレイヤーを見つける
   - 対象プレイヤーに配り枚数+1して終了
   - プレイヤー番号が1ならSEを鳴らすフラグをONにする
   - 帰り値はp：SEを鳴らすフラグ */
int game_step_deal(GameState* g){
  if (g->deal_done) return 0;
  if (g->deal_delay > 0){g->deal_delay--;return 0;}
  for (int tries=0; tries<PLAYERS; ++tries){
    int p = g->deal_turn;
    g->deal_turn = (g->deal_turn + 1) & 3;// 次回用に順番を回す（0..3を循環）
    if (g->visible[p] < g->target[p]){
      g->visible[p]++;
      g->deal_delay = DEAL_DELAY_FRAMES;
      if (deal_finished_all(g)) g->deal_done = 1; 
      return (p == 1) ? 1 : 0;
    }
  }
  return 0;
}

/* ── ターン進行：AIのみで回す最小版 ──
   - 場のカード状況、役の効果状況を加味して出すカードを決定
   - 場に出るカードの情報を更新
   - 場に出たカードの分、内部の手札管理と、描写用の手札管理を減らす　*/
int game_step_turn(GameState* g,
                   Hand hands[PLAYERS])
{
  // ターン進行させない
  if (!g->deal_done) return 0; // 配り中なら何もしない
  if (g->turn_delay > 0){ g->turn_delay--; return 0; } // 待ち時間消費中なら何もしない

  // 手番のプレイヤー
  const int p = g->turn_player; 

  // ai.cに“今の場の情報”を渡すために、既存のバラバラな変数たちをひとまとめにして FieldState fsとした
  FieldState fs;
  fs.field_visible        = (u8)g->field_visible;
  fs.revolution           = 0; 
  fs.jback_active         = 0; 
  fs.right_neighbor_count = 0;
  fs.field_count          = g->field_count;
  fs.field_eff_rank       = g->field_eff_rank;
  fs.field_suit_mask      = g->field_suit_mask;
  for (int i = 0; i < 4; i++) {fs.field_names[i] = g->field_names[i];}

  //  AI に打ち手を選ばせる（1〜4枚）
  const char* chosen[4];
  chosen[0] = NULL;
  chosen[1] = NULL;
  chosen[2] = NULL;
  chosen[3] = NULL;
  u8 n = 0; // 場に出すカードの枚数
  int played = ai_choose_move_group(&hands[p], &fs, chosen, &n); // 場に出るカードの詳細（１～４枚）

  // 場に出るカードがある場合の処理
  if (played && n > 0){ 
    // 手札から場に出すカードを取り除く
    for (u8 i=0;i<n;++i){ remove_card_name_once(&hands[p], chosen[i]); }

    // 場の情報を更新（中央スプライト表示用）
    g->field_visible  = 1;
    g->field_count    = (u8)n;
    u8 raw_rank = eval_rank_from_name(chosen[0]); // 選んだカードを強さに変換(1枚なのは複数出しても1枚目のみで十分だから)
    g->field_eff_rank = raw_rank;
    for (int i=0;i<4;++i) g->field_names[i] = (i < (int)n) ? chosen[i] : NULL; // 複数枚の表示用に一覧も更新
    u8 mask = 0;
    for (u8 i = 0; i < n; ++i) {
        u8 suit_id = (u8)suit_of_name(chosen[i]); // スート情報を取り出す
        mask |= (1 << suit_id);
    }
    g->field_suit_mask = mask;

    // 手札の枚数と見えてる枚数に反映
    g->visible[p] = hands[p].count;

    // ゲームの状況の更新
    g->last_played = p;
    g->pass_count  = 0;
    g->turn_player = (p + 1) & 3;
    g->turn_delay  = TURN_DELAY_FRAMES;
    return 1; // played=1

  }
  // 場に出るカードがない場合の処理
  else{
    // ゲームの状況の更新
    g->pass_count++;
    g->turn_player = (p + 1) & 3;
    g->turn_delay  = TURN_DELAY_FRAMES;

    // 3人連続パス → 場流し（当面は縛り解除など無し）
    if (g->pass_count >= 3){
      g->field_visible  = 0;
      g->field_count    = 0;
      g->field_eff_rank = 0;
      for (int i=0;i<4;++i) g->field_names[i] = NULL;
      g->pass_count = 0;
      g->field_suit_mask = 0;
    }
    return 0; // “出していない”
  }
}
