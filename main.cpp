#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

int WIN_W = 700;
int WIN_H = 750;

#define PI 3.1415926f

/* Game states */
typedef enum {
    STATE_MENU,
    STATE_HELP,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAMEOVER
} GameState;
GameState state = STATE_MENU;

/* Timing */
const int FPS      = 60;
const int TIMER_MS = 1000 / 60;

/* Time */
int time_remaining    = 90;  /* seconds */
const int TIME_LIMIT  = 90;

/* Score & high score */
int score      = 0;
int high_score = 0;

typedef enum {
    ITEM_NORMAL = 0,   /* white  +1  */
    ITEM_BLUE,         /* blue   +5  */
    ITEM_GOLDEN,       /* gold   +10 */
    ITEM_POOP,         /* brown  -10 */
    ITEM_PERK_WIDE,    /* green block: bigger basket */
    ITEM_PERK_SLOW,    /* cyan block:  slow eggs     */
    ITEM_PERK_TIME     /* yellow block: +15 seconds  */
} ItemType;

#define MAX_ITEMS 20

typedef struct {
    float x, y;
    float vx, vy;      /* velocity */
    int   active;
    int   caught;
    int   flash_ms;
    ItemType type;
} Item;

Item items[MAX_ITEMS];

/* Item spawn timer */
int item_accum_ms    = 0;
int item_interval_ms = 1600;


int perk_wide_ms = 0;   /* remaining ms for wide basket perk */
int perk_slow_ms = 0;   /* remaining ms for slow egg perk    */

const float BASKET_NORMAL_HW = 45.0f;
const float BASKET_WIDE_HW   = 80.0f;
const float PERK_DURATION_MS = 8000;

float basket_x        = 350.0f;
float basket_half_width = 45.0f;
const float BASKET_Y    = 40.0f;
const float BASKET_HEIGHT = 20.0f;

#define NUM_CHICKENS 2

typedef struct {
    float x, y;        /* position of chicken body centre */
    float speed;       /* horizontal speed on its stick   */
    float stick_y;     /* y of the bamboo stick           */
} Chicken;

Chicken chickens[NUM_CHICKENS];

int   game_elapsed_ms       = 0;
int   speed_stage           = 0;
const float SPEED_INCREASE  = 1.22f;
const float EGG_GRAVITY     = 280.0f;   /* pixels/s^2 */
float egg_gravity_scale     = 1.0f;     /* slowed by perk */

void drawText(float x, float y, const char *s);
void drawTextLarge(float x, float y, const char *s);
void reset_game(void);
void set_ortho(void);
void draw_filled_ellipse(float cx, float cy, float rx, float ry, int seg);
void init_items(void);
void init_chickens(void);

void draw_filled_ellipse(float cx, float cy, float rx, float ry, int seg) {
    glBegin(GL_POLYGON);
    for (int i = 0; i < seg; i++) {
        float t = (2.0f * PI * i) / seg;
        glVertex2f(cx + cosf(t)*rx, cy + sinf(t)*ry);
    }
    glEnd();
}

void draw_rect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x,   y);
    glVertex2f(x+w, y);
    glVertex2f(x+w, y+h);
    glVertex2f(x,   y+h);
    glEnd();
}

/* ---- Background ---- */
void draw_cloud(float x, float y, float sc) {
    glColor3f(0.93f, 0.93f, 0.95f);
    draw_filled_ellipse(x,         y,        28*sc, 15*sc, 32);
    draw_filled_ellipse(x+25*sc,   y+5*sc,   24*sc, 18*sc, 32);
    draw_filled_ellipse(x-25*sc,   y+4*sc,   22*sc, 16*sc, 32);
    draw_filled_ellipse(x+5*sc,    y+16*sc,  22*sc, 18*sc, 32);
}

void draw_background(void) {
    /* sky gradient simulation - two quads */
    glBegin(GL_QUADS);
    glColor3f(0.45f, 0.70f, 0.95f);
    glVertex2f(0,     WIN_H);
    glVertex2f(WIN_W, WIN_H);
    glColor3f(0.72f, 0.88f, 1.0f);
    glVertex2f(WIN_W, WIN_H*0.4f);
    glVertex2f(0,     WIN_H*0.4f);
    glEnd();
    glBegin(GL_QUADS);
    glColor3f(0.72f, 0.88f, 1.0f);
    glVertex2f(0,     WIN_H*0.4f);
    glVertex2f(WIN_W, WIN_H*0.4f);
    glColor3f(0.80f, 0.92f, 1.0f);
    glVertex2f(WIN_W, 0);
    glVertex2f(0,     0);
    glEnd();

    /* clouds */
    draw_cloud(80,  680, 1.0f);
    draw_cloud(260, 640, 0.85f);
    draw_cloud(460, 700, 1.15f);
    draw_cloud(600, 620, 0.75f);
    draw_cloud(150, 590, 0.65f);

    /* grass */
    glColor3f(0.40f, 0.72f, 0.30f);
    draw_rect(0, 0, WIN_W, 80);
    /* darker grass strip */
    glColor3f(0.32f, 0.58f, 0.22f);
    draw_rect(0, 72, WIN_W, 8);
}

/* ---- Bamboo stick ---- */
void draw_bamboo(float stick_y) {
    glColor3f(0.52f, 0.32f, 0.10f);
    glLineWidth(10.0f);
    glBegin(GL_LINES);
    glVertex2f(20,        stick_y);
    glVertex2f(WIN_W-20, stick_y);
    glEnd();

    glColor3f(0.72f, 0.52f, 0.22f);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    glVertex2f(20,        stick_y+3);
    glVertex2f(WIN_W-20, stick_y+3);
    glEnd();

    glColor3f(0.32f, 0.20f, 0.06f);
    glLineWidth(3.0f);
    for (int x = 70; x < WIN_W-20; x += 90) {
        glBegin(GL_LINES);
        glVertex2f(x, stick_y-6);
        glVertex2f(x, stick_y+6);
        glEnd();
    }
}

/* ---- Chicken ---- */
void draw_chicken(float cx, float cy) {
    /* body */
    glColor3f(1.0f, 0.85f, 0.20f);
    draw_filled_ellipse(cx, cy, 34.0f, 24.0f, 40);
    /* belly */
    glColor3f(1.0f, 0.93f, 0.45f);
    draw_filled_ellipse(cx-4, cy-4, 20.0f, 14.0f, 32);
    /* rear puff */
    glColor3f(1.0f, 0.85f, 0.20f);
    draw_filled_ellipse(cx+28, cy+18, 18.0f, 17.0f, 32);
    /* head */
    glColor3f(0.95f, 0.68f, 0.10f);
    draw_filled_ellipse(cx-8, cy+2, 16.0f, 11.0f, 32);
    /* wing feathers */
    glColor3f(0.85f, 0.45f, 0.05f);
    glBegin(GL_TRIANGLES);
    glVertex2f(cx-34, cy+8);  glVertex2f(cx-55, cy+24); glVertex2f(cx-42, cy-2);
    glVertex2f(cx-36, cy);    glVertex2f(cx-58, cy+4);  glVertex2f(cx-43, cy-14);
    glEnd();
    /* comb */
    glColor3f(0.85f, 0.05f, 0.05f);
    draw_filled_ellipse(cx+22, cy+36, 6, 8, 20);
    draw_filled_ellipse(cx+30, cy+39, 6, 9, 20);
    draw_filled_ellipse(cx+38, cy+35, 6, 8, 20);
    /* beak lower */
    glColor3f(1.0f, 0.55f, 0.05f);
    glBegin(GL_POLYGON);
    glVertex2f(cx+43, cy+20);
    glVertex2f(cx+53, cy+25);
    glVertex2f(cx+61, cy+21);
    glVertex2f(cx+53, cy+18);
    glEnd();
    /* beak upper */
    glColor3f(0.95f, 0.35f, 0.0f);
    glBegin(GL_POLYGON);
    glVertex2f(cx+43, cy+18);
    glVertex2f(cx+53, cy+18);
    glVertex2f(cx+60, cy+14);
    glVertex2f(cx+52, cy+13);
    glEnd();
    /* beak line */
    glColor3f(0.45f, 0.18f, 0.0f);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
    glVertex2f(cx+45, cy+18);
    glVertex2f(cx+59, cy+17);
    glEnd();
    /* eye */
    glColor3f(0, 0, 0);
    draw_filled_ellipse(cx+34, cy+24, 3.0f, 3.0f, 16);
    /* legs */
    glColor3f(0.9f, 0.45f, 0.0f);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    glVertex2f(cx-8,  cy-22); glVertex2f(cx-8,  cy-36);
    glVertex2f(cx+12, cy-22); glVertex2f(cx+12, cy-36);
    glVertex2f(cx-8,  cy-36); glVertex2f(cx-18, cy-40);
    glVertex2f(cx-8,  cy-36); glVertex2f(cx+1,  cy-40);
    glVertex2f(cx+12, cy-36); glVertex2f(cx+2,  cy-40);
    glVertex2f(cx+12, cy-36); glVertex2f(cx+22, cy-40);
    glEnd();
}

/* ---- Basket ---- */
void draw_basket(void) {
    float w = basket_half_width;
    float x = basket_x;
    float y = BASKET_Y;

    /* glow if perk active */
    if (perk_wide_ms > 0) {
        glColor4f(0.2f, 1.0f, 0.4f, 0.25f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        draw_filled_ellipse(x, y, w+14, 12, 30);
        glDisable(GL_BLEND);
    }

    /* handle */
    glColor3f(0.45f, 0.22f, 0.08f);
    glLineWidth(4.0f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 24; i++) {
        float t = PI * i / 24.0f;
        glVertex2f(x + cosf(t)*(w+8), y + 5 + sinf(t)*38);
    }
    glEnd();
    /* rim */
    glColor3f(0.55f, 0.28f, 0.10f);
    glBegin(GL_QUADS);
    glVertex2f(x-w-10, y+10); glVertex2f(x+w+10, y+10);
    glVertex2f(x+w+6,  y);    glVertex2f(x-w-6,  y);
    glEnd();
    /* body */
    glColor3f(0.78f, 0.42f, 0.16f);
    glBegin(GL_QUADS);
    glVertex2f(x-w-6, y);    glVertex2f(x+w+6, y);
    glVertex2f(x+w-8, y-BASKET_HEIGHT-12);
    glVertex2f(x-w+8, y-BASKET_HEIGHT-12);
    glEnd();
    /* bottom */
    glColor3f(0.45f, 0.22f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2f(x-w+8,  y-BASKET_HEIGHT-12);
    glVertex2f(x+w-8,  y-BASKET_HEIGHT-12);
    glVertex2f(x+w-14, y-BASKET_HEIGHT-18);
    glVertex2f(x-w+14, y-BASKET_HEIGHT-18);
    glEnd();
    /* vertical weave */
    glColor3f(0.35f, 0.18f, 0.07f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (int i = -3; i <= 3; i++) {
        float lx = x + i*(w/4.0f);
        glVertex2f(lx, y);
        glVertex2f(lx, y-BASKET_HEIGHT-12);
    }
    glEnd();
    /* horizontal weave */
    glColor3f(0.90f, 0.58f, 0.25f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(x-w-2, y-6);   glVertex2f(x+w+2, y-6);
    glVertex2f(x-w+2, y-14);  glVertex2f(x+w-2, y-14);
    glVertex2f(x-w+8, y-22);  glVertex2f(x+w-8, y-22);
    glEnd();
    /* shadows */
    glColor3f(0.38f, 0.18f, 0.06f);
    glBegin(GL_TRIANGLES);
    glVertex2f(x-w-6,  y);
    glVertex2f(x-w+8,  y-BASKET_HEIGHT-12);
    glVertex2f(x-w+16, y-BASKET_HEIGHT-12);
    glVertex2f(x+w+6,  y);
    glVertex2f(x+w-8,  y-BASKET_HEIGHT-12);
    glVertex2f(x+w-16, y-BASKET_HEIGHT-12);
    glEnd();
}

/* ---- Draw one item ---- */
void draw_item(Item *it) {
    float cx = it->x;
    float cy = it->y;

    if (it->caught) {
        /* flash white */
        glColor3f(1, 1, 1);
        draw_filled_ellipse(cx, cy, 14, 18, 24);
        return;
    }

    switch (it->type) {
        case ITEM_NORMAL:
            /* cream egg */
            glColor3f(0.97f, 0.93f, 0.83f);
            draw_filled_ellipse(cx, cy, 9, 12, 32);
            glColor3f(1, 1, 1);
            draw_filled_ellipse(cx-3, cy+4, 3, 4, 16);
            glColor3f(0.75f, 0.65f, 0.50f);
            glLineWidth(1.2f);
            glBegin(GL_LINE_LOOP);
            for (int i=0;i<32;i++){float t=2*PI*i/32;glVertex2f(cx+cosf(t)*9,cy+sinf(t)*12);}
            glEnd();
            break;

        case ITEM_BLUE:
            /* blue egg */
            glColor3f(0.25f, 0.55f, 0.95f);
            draw_filled_ellipse(cx, cy, 10, 13, 32);
            glColor3f(0.7f, 0.85f, 1.0f);
            draw_filled_ellipse(cx-3, cy+4, 3, 4, 16);
            glColor3f(0.10f, 0.30f, 0.70f);
            glLineWidth(1.2f);
            glBegin(GL_LINE_LOOP);
            for (int i=0;i<32;i++){float t=2*PI*i/32;glVertex2f(cx+cosf(t)*10,cy+sinf(t)*13);}
            glEnd();
            break;

        case ITEM_GOLDEN:
            /* golden egg with star shimmer */
            glColor3f(1.0f, 0.82f, 0.0f);
            draw_filled_ellipse(cx, cy, 11, 14, 32);
            glColor3f(1.0f, 1.0f, 0.6f);
            draw_filled_ellipse(cx-3, cy+5, 4, 5, 16);
            glColor3f(0.75f, 0.55f, 0.0f);
            glLineWidth(1.5f);
            glBegin(GL_LINE_LOOP);
            for (int i=0;i<32;i++){float t=2*PI*i/32;glVertex2f(cx+cosf(t)*11,cy+sinf(t)*14);}
            glEnd();
            /* sparkle */
            glColor3f(1.0f, 1.0f, 0.8f);
            glLineWidth(1.5f);
            glBegin(GL_LINES);
            glVertex2f(cx+11, cy+14); glVertex2f(cx+16, cy+20);
            glVertex2f(cx-11, cy+14); glVertex2f(cx-16, cy+20);
            glVertex2f(cx+11, cy-14); glVertex2f(cx+16, cy-20);
            glEnd();
            break;

        case ITEM_POOP:
            /* brown poop pile */
            glColor3f(0.42f, 0.22f, 0.05f);
            draw_filled_ellipse(cx,   cy,    10, 7, 24);
            draw_filled_ellipse(cx-2, cy+7,   7, 5, 24);
            draw_filled_ellipse(cx,   cy+12,  5, 4, 24);
            draw_filled_ellipse(cx+1, cy+15,  3, 3, 20);
            /* stink lines */
            glColor3f(0.60f, 0.45f, 0.15f);
            glLineWidth(1.5f);
            glBegin(GL_LINES);
            glVertex2f(cx-8, cy+18); glVertex2f(cx-10, cy+26);
            glVertex2f(cx,   cy+19); glVertex2f(cx,    cy+27);
            glVertex2f(cx+8, cy+18); glVertex2f(cx+10, cy+26);
            glEnd();
            break;

        case ITEM_PERK_WIDE:
            /* green block */
            glColor3f(0.20f, 0.75f, 0.30f);
            draw_rect(cx-14, cy-10, 28, 20);
            glColor3f(0.60f, 1.0f, 0.60f);
            glLineWidth(2.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(cx-14, cy-10); glVertex2f(cx+14, cy-10);
            glVertex2f(cx+14, cy+10); glVertex2f(cx-14, cy+10);
            glEnd();
            /* "W" label */
            glColor3f(1,1,1);
            drawText(cx-6, cy-6, "W");
            break;

        case ITEM_PERK_SLOW:
            /* cyan block */
            glColor3f(0.10f, 0.80f, 0.85f);
            draw_rect(cx-14, cy-10, 28, 20);
            glColor3f(0.60f, 1.0f, 1.0f);
            glLineWidth(2.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(cx-14, cy-10); glVertex2f(cx+14, cy-10);
            glVertex2f(cx+14, cy+10); glVertex2f(cx-14, cy+10);
            glEnd();
            glColor3f(1,1,1);
            drawText(cx-6, cy-6, "S");
            break;

        case ITEM_PERK_TIME:
            /* yellow block */
            glColor3f(0.95f, 0.80f, 0.05f);
            draw_rect(cx-14, cy-10, 28, 20);
            glColor3f(1.0f, 1.0f, 0.60f);
            glLineWidth(2.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(cx-14, cy-10); glVertex2f(cx+14, cy-10);
            glVertex2f(cx+14, cy+10); glVertex2f(cx-14, cy+10);
            glEnd();
            glColor3f(0.2f, 0.1f, 0.0f);
            drawText(cx-6, cy-6, "T");
            break;
    }
}

/* ---- Draw all items ---- */
void draw_items(void) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active) draw_item(&items[i]);
    }
}

/* ---- Perk indicators ---- */
void draw_perk_bar(void) {
    float x = 10;
    float y = WIN_H - 55;

    if (perk_wide_ms > 0) {
        glColor3f(0.20f, 0.75f, 0.30f);
        draw_rect(x, y, 100*(perk_wide_ms/(float)PERK_DURATION_MS), 12);
        glColor3f(1,1,1);
        drawText(x+2, y, "WIDE");
        y -= 18;
    }
    if (perk_slow_ms > 0) {
        glColor3f(0.10f, 0.80f, 0.85f);
        draw_rect(x, y, 100*(perk_slow_ms/(float)PERK_DURATION_MS), 12);
        glColor3f(1,1,1);
        drawText(x+2, y, "SLOW");
    }
}

/* ---- HUD ---- */
void draw_hud(void) {
    char buf[128];

    /* panel bg */
    glColor3f(0.10f, 0.10f, 0.15f);
    draw_rect(0, WIN_H-36, WIN_W, 36);

    /* Score */
    glColor3f(1.0f, 0.85f, 0.20f);
    sprintf(buf, "Score: %d", score);
    drawText(10, WIN_H-24, buf);

    /* High score */
    glColor3f(0.70f, 0.70f, 0.70f);
    sprintf(buf, "Best: %d", high_score);
    drawText(WIN_W/2 - 40, WIN_H-24, buf);

    /* Time */
    if (time_remaining <= 10)
        glColor3f(1.0f, 0.25f, 0.25f);
    else
        glColor3f(0.30f, 1.0f, 0.55f);
    sprintf(buf, "Time: %d", time_remaining);
    drawText(WIN_W-110, WIN_H-24, buf);

    /* legend */
    glColor3f(1.0f, 0.82f, 0.0f);   drawText(10, WIN_H-50, "Gold+10");
    glColor3f(0.25f,0.55f,0.95f);   drawText(90, WIN_H-50, "Blue+5");
    glColor3f(0.85f,0.85f,0.75f);   drawText(160, WIN_H-50, "Egg+1");
    glColor3f(0.55f,0.30f,0.05f);   drawText(230, WIN_H-50, "Poop-10");

    draw_perk_bar();
}

void init_items(void) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        items[i].active   = 0;
        items[i].caught   = 0;
        items[i].flash_ms = 0;
    }
}

/* Pick a random item type weighted by probability */
ItemType random_item_type(void) {
    int r = rand() % 100;
    if (r < 15) return ITEM_GOLDEN;     /* 15% */
    if (r < 40) return ITEM_BLUE;       /* 25% */
    if (r < 65) return ITEM_NORMAL;     /* 25% */
    if (r < 80) return ITEM_POOP;       /* 15% */
    if (r < 87) return ITEM_PERK_WIDE;  /*  7% */
    if (r < 94) return ITEM_PERK_SLOW;  /*  7% */
    return ITEM_PERK_TIME;              /*  6% */
}

/* Spawn from a random chicken */
void spawn_item(void) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) {
            /* pick a random chicken to drop from */
            int ci = rand() % NUM_CHICKENS;
            items[i].x        = chickens[ci].x + 20.0f;
            items[i].y        = chickens[ci].y - 20.0f;
            items[i].vx       = 0.0f;
            items[i].vy       = -15.0f;
            items[i].active   = 1;
            items[i].caught   = 0;
            items[i].flash_ms = 0;
            items[i].type     = random_item_type();
            break;
        }
    }
}

void apply_perk(ItemType t) {
    switch (t) {
        case ITEM_PERK_WIDE:
            perk_wide_ms = PERK_DURATION_MS;
            basket_half_width = BASKET_WIDE_HW;
            break;
        case ITEM_PERK_SLOW:
            perk_slow_ms = PERK_DURATION_MS;
            egg_gravity_scale = 0.40f;
            break;
        case ITEM_PERK_TIME:
            time_remaining += 15;
            break;
        default: break;
    }
}

void update_items(int dt_ms) {
    float dt  = dt_ms / 1000.0f;
    float grav = EGG_GRAVITY * egg_gravity_scale;

    /* basket catch zone */
    float bx1   = basket_x - basket_half_width - 8;
    float bx2   = basket_x + basket_half_width + 8;
    float by_top = BASKET_Y + 12;
    float by_bot = BASKET_Y - BASKET_HEIGHT - 20;

    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) continue;

        if (items[i].caught) {
            items[i].flash_ms -= dt_ms;
            if (items[i].flash_ms <= 0) items[i].active = 0;
            continue;
        }

        /* physics */
        items[i].vy -= grav * dt;
        items[i].x  += items[i].vx * dt;
        items[i].y  += items[i].vy * dt;

        /* horizontal boundary bounce */
        if (items[i].x < 10)       { items[i].x = 10;       items[i].vx =  fabsf(items[i].vx); }
        if (items[i].x > WIN_W-10) { items[i].x = WIN_W-10; items[i].vx = -fabsf(items[i].vx); }

        /* catch check */
        if (items[i].x >= bx1 && items[i].x <= bx2 &&
            items[i].y <= by_top && items[i].y >= by_bot) {

            items[i].caught   = 1;
            items[i].flash_ms = 280;

            switch (items[i].type) {
                case ITEM_NORMAL: score += 1;  break;
                case ITEM_BLUE:   score += 5;  break;
                case ITEM_GOLDEN: score += 10; break;
                case ITEM_POOP:   score -= 10; break;
                default:
                    apply_perk(items[i].type);
                    break;
            }
            if (score < 0) score = 0;
            continue;
        }

        /* fell off bottom */
        if (items[i].y < -30) {
            items[i].active = 0;
        }
    }
}
void init_chickens(void) {
    /* Chicken 0: upper stick */
    chickens[0].stick_y = 620.0f;
    chickens[0].y       = chickens[0].stick_y + 36.0f;
    chickens[0].x       = WIN_W * 0.3f;
    chickens[0].speed   = 75.0f;

    /* Chicken 1: lower second stick (in between) */
    chickens[1].stick_y = 440.0f;
    chickens[1].y       = chickens[1].stick_y + 36.0f;
    chickens[1].x       = WIN_W * 0.65f;
    chickens[1].speed   = -60.0f;
}

void update_chickens(int dt_ms) {
    float dt = dt_ms / 1000.0f;
    for (int i = 0; i < NUM_CHICKENS; i++) {
        chickens[i].x += chickens[i].speed * dt;
        if (chickens[i].x < 40)         { chickens[i].x = 40;         chickens[i].speed =  fabsf(chickens[i].speed); }
        if (chickens[i].x > WIN_W - 70) { chickens[i].x = WIN_W - 70; chickens[i].speed = -fabsf(chickens[i].speed); }
    }
}

void reset_game(void) {
    time_remaining     = TIME_LIMIT;
    score              = 0;
    game_elapsed_ms    = 0;
    speed_stage        = 0;
    item_accum_ms      = 0;
    item_interval_ms   = 1600;
    perk_wide_ms       = 0;
    perk_slow_ms       = 0;
    egg_gravity_scale  = 1.0f;
    basket_x           = WIN_W * 0.5f;
    basket_half_width  = BASKET_NORMAL_HW;
    init_items();
    init_chickens();
}

void drawText(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *s++);
}

void drawTextLarge(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *s++);
}

void drawTextSmall(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *s++);
}

/* Semi-transparent overlay */
void draw_overlay(float alpha) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.08f, alpha);
    draw_rect(0, 0, WIN_W, WIN_H);
    glDisable(GL_BLEND);
}

/* Centered panel box */
void draw_panel(float cx, float cy, float w, float h) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.05f, 0.08f, 0.18f, 0.88f);
    draw_rect(cx-w/2, cy-h/2, w, h);
    glDisable(GL_BLEND);
    glColor3f(0.35f, 0.60f, 1.0f);
    glLineWidth(2.5f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx-w/2, cy-h/2); glVertex2f(cx+w/2, cy-h/2);
    glVertex2f(cx+w/2, cy+h/2); glVertex2f(cx-w/2, cy+h/2);
    glEnd();
}


void draw_menu(void) {
    draw_background();
    draw_panel(WIN_W/2, WIN_H/2, 460, 380);

    /* Title */
    glColor3f(1.0f, 0.82f, 0.0f);
    drawTextLarge(WIN_W/2 - 115, WIN_H/2 + 165, "*** CATCH THE EGGS ***");

    char hsbuf[64];
    sprintf(hsbuf, "High Score: %d", high_score);
    glColor3f(0.55f, 0.90f, 0.55f);
    drawText(WIN_W/2 - 65, WIN_H/2 + 130, hsbuf);

    glColor3f(0.85f, 0.85f, 0.95f);
    drawText(WIN_W/2 - 80,  WIN_H/2 + 90,  "S  -  Start Game");
    drawText(WIN_W/2 - 80,  WIN_H/2 + 60,  "H  -  Help / Controls");
    drawText(WIN_W/2 - 80,  WIN_H/2 + 30,  "Q / ESC  -  Quit");

    /* egg legend preview */
    glColor3f(0.70f, 0.70f, 0.80f);
    drawText(WIN_W/2 - 80,  WIN_H/2 - 10,  "Egg Types:");
    glColor3f(1.0f, 0.82f, 0.0f);   drawText(WIN_W/2-80, WIN_H/2-40, "Golden  +10 pts");
    glColor3f(0.25f,0.55f,0.95f);   drawText(WIN_W/2-80, WIN_H/2-65, "Blue    +5  pts");
    glColor3f(0.97f,0.93f,0.83f);   drawText(WIN_W/2-80, WIN_H/2-90, "Normal  +1  pt");
    glColor3f(0.55f,0.30f,0.05f);   drawText(WIN_W/2-80, WIN_H/2-115,"Poop    -10 pts");

    glColor3f(0.55f, 0.70f, 0.55f);
    drawText(WIN_W/2 - 80,  WIN_H/2 - 145, "Blocks: W=Wide  S=Slow  T=+Time");
}

void draw_help(void) {
    draw_background();
    draw_panel(WIN_W/2, WIN_H/2, 530, 440);

    glColor3f(1.0f, 0.82f, 0.0f);
    drawTextLarge(WIN_W/2 - 60, WIN_H/2 + 200, "HELP & CONTROLS");

    glColor3f(0.85f, 0.95f, 1.0f);
    float lx = WIN_W/2 - 230;
    float ly = WIN_H/2 + 160;
    float dy = 26;
    drawText(lx, ly,      "MOVEMENT:");
    drawText(lx, ly-dy,   "  Mouse    - Move basket");
    drawText(lx, ly-2*dy, "  A / D    - Move basket left / right");
    drawText(lx, ly-3*dy, "  Left / Right Arrow keys");

    drawText(lx, ly-4.5f*dy, "GAME CONTROLS:");
    drawText(lx, ly-5.5f*dy, "  S        - Start / Restart");
    drawText(lx, ly-6.5f*dy, "  P        - Pause / Resume");
    drawText(lx, ly-7.5f*dy, "  M        - Return to Menu");
    drawText(lx, ly-8.5f*dy, "  Q / ESC  - Quit");

    drawText(lx, ly-10.0f*dy, "POWER-UP BLOCKS:");
    glColor3f(0.20f, 0.80f, 0.35f);
    drawText(lx, ly-11.0f*dy, "  W  - Wider basket for 8s");
    glColor3f(0.10f, 0.80f, 0.85f);
    drawText(lx, ly-12.0f*dy, "  S  - Slows egg fall for 8s");
    glColor3f(0.95f, 0.80f, 0.05f);
    drawText(lx, ly-13.0f*dy, "  T  - +15 seconds bonus time");

    glColor3f(0.65f, 0.65f, 0.75f);
    drawText(WIN_W/2 - 110, WIN_H/2 - 190, "Press M to go back to Menu");
}

void draw_paused(void) {
    draw_overlay(0.55f);
    draw_panel(WIN_W/2, WIN_H/2, 300, 160);
    glColor3f(1.0f, 0.82f, 0.0f);
    drawTextLarge(WIN_W/2 - 48, WIN_H/2 + 50, "PAUSED");
    glColor3f(0.85f, 0.85f, 0.95f);
    drawText(WIN_W/2 - 90, WIN_H/2 + 15,  "P  - Resume");
    drawText(WIN_W/2 - 90, WIN_H/2 - 15,  "M  - Menu");
    drawText(WIN_W/2 - 90, WIN_H/2 - 45,  "Q  - Quit");
}

void draw_gameover(void) {
    draw_overlay(0.60f);
    draw_panel(WIN_W/2, WIN_H/2, 400, 260);

    glColor3f(1.0f, 0.30f, 0.30f);
    drawTextLarge(WIN_W/2 - 75, WIN_H/2 + 105, "GAME  OVER");

    char buf[128];
    glColor3f(1.0f, 0.82f, 0.0f);
    sprintf(buf, "Final Score: %d", score);
    drawText(WIN_W/2 - 75, WIN_H/2 + 65, buf);

    if (score >= high_score && score > 0) {
        glColor3f(0.30f, 1.0f, 0.50f);
        drawText(WIN_W/2 - 80, WIN_H/2 + 35, "*** NEW HIGH SCORE! ***");
    } else {
        glColor3f(0.55f, 0.90f, 0.55f);
        sprintf(buf, "Best: %d", high_score);
        drawText(WIN_W/2 - 40, WIN_H/2 + 35, buf);
    }

    glColor3f(0.85f, 0.85f, 0.95f);
    drawText(WIN_W/2 - 120, WIN_H/2 - 10, "S  - Play Again");
    drawText(WIN_W/2 - 120, WIN_H/2 - 40, "M  - Main Menu");
    drawText(WIN_W/2 - 120, WIN_H/2 - 70, "Q / ESC  - Quit");
}

void reset_game(void) {
    time_remaining     = TIME_LIMIT;
    score              = 0;
    game_elapsed_ms    = 0;
    speed_stage        = 0;
    item_accum_ms      = 0;
    item_interval_ms   = 1600;
    perk_wide_ms       = 0;
    perk_slow_ms       = 0;
    egg_gravity_scale  = 1.0f;
    basket_x           = WIN_W * 0.5f;
    basket_half_width  = BASKET_NORMAL_HW;
    init_items();
    init_chickens();
}

void drawText(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *s++);
}

void drawTextLarge(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *s++);
}

void drawTextSmall(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *s++);
}

/* Semi-transparent overlay */
void draw_overlay(float alpha) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.08f, alpha);
    draw_rect(0, 0, WIN_W, WIN_H);
    glDisable(GL_BLEND);
}

/* Centered panel box */
void draw_panel(float cx, float cy, float w, float h) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.05f, 0.08f, 0.18f, 0.88f);
    draw_rect(cx-w/2, cy-h/2, w, h);
    glDisable(GL_BLEND);
    glColor3f(0.35f, 0.60f, 1.0f);
    glLineWidth(2.5f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx-w/2, cy-h/2); glVertex2f(cx+w/2, cy-h/2);
    glVertex2f(cx+w/2, cy+h/2); glVertex2f(cx-w/2, cy+h/2);
    glEnd();
}


void draw_menu(void) {
    draw_background();
    draw_panel(WIN_W/2, WIN_H/2, 460, 380);

    /* Title */
    glColor3f(1.0f, 0.82f, 0.0f);
    drawTextLarge(WIN_W/2 - 115, WIN_H/2 + 165, "*** CATCH THE EGGS ***");

    char hsbuf[64];
    sprintf(hsbuf, "High Score: %d", high_score);
    glColor3f(0.55f, 0.90f, 0.55f);
    drawText(WIN_W/2 - 65, WIN_H/2 + 130, hsbuf);

    glColor3f(0.85f, 0.85f, 0.95f);
    drawText(WIN_W/2 - 80,  WIN_H/2 + 90,  "S  -  Start Game");
    drawText(WIN_W/2 - 80,  WIN_H/2 + 60,  "H  -  Help / Controls");
    drawText(WIN_W/2 - 80,  WIN_H/2 + 30,  "Q / ESC  -  Quit");

    /* egg legend preview */
    glColor3f(0.70f, 0.70f, 0.80f);
    drawText(WIN_W/2 - 80,  WIN_H/2 - 10,  "Egg Types:");
    glColor3f(1.0f, 0.82f, 0.0f);   drawText(WIN_W/2-80, WIN_H/2-40, "Golden  +10 pts");
    glColor3f(0.25f,0.55f,0.95f);   drawText(WIN_W/2-80, WIN_H/2-65, "Blue    +5  pts");
    glColor3f(0.97f,0.93f,0.83f);   drawText(WIN_W/2-80, WIN_H/2-90, "Normal  +1  pt");
    glColor3f(0.55f,0.30f,0.05f);   drawText(WIN_W/2-80, WIN_H/2-115,"Poop    -10 pts");

    glColor3f(0.55f, 0.70f, 0.55f);
    drawText(WIN_W/2 - 80,  WIN_H/2 - 145, "Blocks: W=Wide  S=Slow  T=+Time");
}

void draw_help(void) {
    draw_background();
    draw_panel(WIN_W/2, WIN_H/2, 530, 440);

    glColor3f(1.0f, 0.82f, 0.0f);
    drawTextLarge(WIN_W/2 - 60, WIN_H/2 + 200, "HELP & CONTROLS");

    glColor3f(0.85f, 0.95f, 1.0f);
    float lx = WIN_W/2 - 230;
    float ly = WIN_H/2 + 160;
    float dy = 26;
    drawText(lx, ly,      "MOVEMENT:");
    drawText(lx, ly-dy,   "  Mouse    - Move basket");
    drawText(lx, ly-2*dy, "  A / D    - Move basket left / right");
    drawText(lx, ly-3*dy, "  Left / Right Arrow keys");

    drawText(lx, ly-4.5f*dy, "GAME CONTROLS:");
    drawText(lx, ly-5.5f*dy, "  S        - Start / Restart");
    drawText(lx, ly-6.5f*dy, "  P        - Pause / Resume");
    drawText(lx, ly-7.5f*dy, "  M        - Return to Menu");
    drawText(lx, ly-8.5f*dy, "  Q / ESC  - Quit");

    drawText(lx, ly-10.0f*dy, "POWER-UP BLOCKS:");
    glColor3f(0.20f, 0.80f, 0.35f);
    drawText(lx, ly-11.0f*dy, "  W  - Wider basket for 8s");
    glColor3f(0.10f, 0.80f, 0.85f);
    drawText(lx, ly-12.0f*dy, "  S  - Slows egg fall for 8s");
    glColor3f(0.95f, 0.80f, 0.05f);
    drawText(lx, ly-13.0f*dy, "  T  - +15 seconds bonus time");

    glColor3f(0.65f, 0.65f, 0.75f);
    drawText(WIN_W/2 - 110, WIN_H/2 - 190, "Press M to go back to Menu");
}

void draw_paused(void) {
    draw_overlay(0.55f);
    draw_panel(WIN_W/2, WIN_H/2, 300, 160);
    glColor3f(1.0f, 0.82f, 0.0f);
    drawTextLarge(WIN_W/2 - 48, WIN_H/2 + 50, "PAUSED");
    glColor3f(0.85f, 0.85f, 0.95f);
    drawText(WIN_W/2 - 90, WIN_H/2 + 15,  "P  - Resume");
    drawText(WIN_W/2 - 90, WIN_H/2 - 15,  "M  - Menu");
    drawText(WIN_W/2 - 90, WIN_H/2 - 45,  "Q  - Quit");
}

void draw_gameover(void) {
    draw_overlay(0.60f);
    draw_panel(WIN_W/2, WIN_H/2, 400, 260);

    glColor3f(1.0f, 0.30f, 0.30f);
    drawTextLarge(WIN_W/2 - 75, WIN_H/2 + 105, "GAME  OVER");

    char buf[128];
    glColor3f(1.0f, 0.82f, 0.0f);
    sprintf(buf, "Final Score: %d", score);
    drawText(WIN_W/2 - 75, WIN_H/2 + 65, buf);

    if (score >= high_score && score > 0) {
        glColor3f(0.30f, 1.0f, 0.50f);
        drawText(WIN_W/2 - 80, WIN_H/2 + 35, "*** NEW HIGH SCORE! ***");
    } else {
        glColor3f(0.55f, 0.90f, 0.55f);
        sprintf(buf, "Best: %d", high_score);
        drawText(WIN_W/2 - 40, WIN_H/2 + 35, buf);
    }

    glColor3f(0.85f, 0.85f, 0.95f);
    drawText(WIN_W/2 - 120, WIN_H/2 - 10, "S  - Play Again");
    drawText(WIN_W/2 - 120, WIN_H/2 - 40, "M  - Main Menu");
    drawText(WIN_W/2 - 120, WIN_H/2 - 70, "Q / ESC  - Quit");
}

void display(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    set_ortho();

    if (state == STATE_MENU) {
        draw_menu();
    }
    else if (state == STATE_HELP) {
        draw_help();
    }
    else if (state == STATE_PLAYING || state == STATE_PAUSED) {
        draw_background();

        /* Two sticks and two chickens */
        for (int i = 0; i < NUM_CHICKENS; i++) {
            draw_bamboo(chickens[i].stick_y);
            draw_chicken(chickens[i].x, chickens[i].y);
        }

        draw_items();
        draw_basket();
        draw_hud();

        if (state == STATE_PAUSED) {
            draw_paused();
        }
    }
    else if (state == STATE_GAMEOVER) {
        /* Show game world behind overlay */
        draw_background();
        for (int i = 0; i < NUM_CHICKENS; i++) {
            draw_bamboo(chickens[i].stick_y);
            draw_chicken(chickens[i].x, chickens[i].y);
        }
        draw_items();
        draw_basket();
        draw_gameover();
    }

    glutSwapBuffers();
}

void update_game(int value) {
    static int last_time   = 0;
    static int initialized = 0;
    static int time_accum  = 0;

    int now = glutGet(GLUT_ELAPSED_TIME);
    if (!initialized) { last_time = now; initialized = 1; }

    int dt_ms = now - last_time;
    if (dt_ms > 200) dt_ms = TIMER_MS;
    last_time = now;

    if (state == STATE_PLAYING) {
        game_elapsed_ms += dt_ms;

        /* Speed stages: chickens faster, items spawn faster */
        if      (speed_stage == 0 && game_elapsed_ms >= 20000) { for(int i=0;i<NUM_CHICKENS;i++) chickens[i].speed*=SPEED_INCREASE; speed_stage=1; item_interval_ms=1300; }
        else if (speed_stage == 1 && game_elapsed_ms >= 40000) { for(int i=0;i<NUM_CHICKENS;i++) chickens[i].speed*=SPEED_INCREASE; speed_stage=2; item_interval_ms=1000; }
        else if (speed_stage == 2 && game_elapsed_ms >= 65000) { for(int i=0;i<NUM_CHICKENS;i++) chickens[i].speed*=SPEED_INCREASE; speed_stage=3; item_interval_ms=750;  }

        update_chickens(dt_ms);
        /* Spawn items */
        item_accum_ms += dt_ms;
        if (item_accum_ms >= item_interval_ms) {
            item_accum_ms -= item_interval_ms;
            spawn_item();
        }

        update_items(dt_ms);

        /* Perk timers */
        if (perk_wide_ms > 0) {
            perk_wide_ms -= dt_ms;
            if (perk_wide_ms <= 0) {
                perk_wide_ms = 0;
                basket_half_width = BASKET_NORMAL_HW;
            }
        }
        if (perk_slow_ms > 0) {
            perk_slow_ms -= dt_ms;
            if (perk_slow_ms <= 0) {
                perk_slow_ms = 0;
                egg_gravity_scale = 1.0f;
            }
        }

        /* Countdown */
        time_accum += dt_ms;
        if (time_accum >= 1000) {
            time_accum -= 1000;
            time_remaining--;
            if (time_remaining <= 0) {
                time_remaining = 0;
                if (score > high_score) high_score = score;
                state = STATE_GAMEOVER;
            }
        }
    }

    glutPostRedisplay();
    glutTimerFunc(TIMER_MS, update_game, 0);
}

void keyboard(unsigned char key, int x, int y) {
    if (state == STATE_MENU) {
        if      (key=='s'||key=='S')             { reset_game(); state = STATE_PLAYING; }
        else if (key=='h'||key=='H')             { state = STATE_HELP; }
        else if (key=='q'||key=='Q'||key==27)    { exit(0); }
    }
    else if (state == STATE_HELP) {
        if (key=='m'||key=='M'||key==27)         { state = STATE_MENU; }
    }
    else if (state == STATE_PLAYING) {
        if      (key=='p'||key=='P')             { state = STATE_PAUSED; }
        else if (key=='q'||key=='Q'||key==27)    { exit(0); }
        else if (key=='a'||key=='A')             { basket_x -= 22; if(basket_x<25) basket_x=25; }
        else if (key=='d'||key=='D')             { basket_x += 22; if(basket_x>WIN_W-25) basket_x=WIN_W-25; }
    }
    else if (state == STATE_PAUSED) {
        if      (key=='p'||key=='P')             { state = STATE_PLAYING; }
        else if (key=='m'||key=='M')             { state = STATE_MENU; }
        else if (key=='q'||key=='Q'||key==27)    { exit(0); }
    }
    else if (state == STATE_GAMEOVER) {
        if      (key=='s'||key=='S')             { reset_game(); state = STATE_PLAYING; }
        else if (key=='m'||key=='M')             { state = STATE_MENU; }
        else if (key=='q'||key=='Q'||key==27)    { exit(0); }
    }
}

void special_keys(int key, int x, int y) {
    if (state == STATE_PLAYING) {
        if (key==GLUT_KEY_LEFT)  { basket_x -= 22; if(basket_x<25) basket_x=25; }
        if (key==GLUT_KEY_RIGHT) { basket_x += 22; if(basket_x>WIN_W-25) basket_x=WIN_W-25; }
    }
}

void passive_mouse(int x, int y) {
    /* GLUT gives y from top; OpenGL from bottom */
    basket_x = (float)x;
    if (basket_x < 25)       basket_x = 25;
    if (basket_x > WIN_W-25) basket_x = WIN_W-25;
    glutPostRedisplay();
}

void reshape(int w, int h) {
    WIN_W = w; WIN_H = h;
    glViewport(0, 0, w, h);
    set_ortho();
}

void set_ortho(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WIN_W, 0, WIN_H);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void init_gl(void) {
    glClearColor(0.5f, 0.8f, 1.0f, 1.0f);
    glShadeModel(GL_SMOOTH);
}

int main(int argc, char **argv) {
    srand((unsigned int)time(NULL));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("Catch The Eggs Bangladesh");

    init_gl();
    init_items();
    init_chickens();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_keys);
    glutPassiveMotionFunc(passive_mouse);
    glutTimerFunc(TIMER_MS, update_game, 0);

    reset_game();
    glutMainLoop();
    return 0;
}
