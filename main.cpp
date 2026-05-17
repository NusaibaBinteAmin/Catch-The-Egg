#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

/* Screen */
int WIN_W = 600;
int WIN_H = 700;

/* Game states */
typedef enum { STATE_MENU, STATE_PLAYING, STATE_PAUSED, STATE_GAMEOVER } GameState;
GameState state = STATE_MENU;

/* Timing */
const int FPS = 60;
const int TIMER_MS = 1000 / FPS;
const int TIME_LIMIT_SECONDS = 60; /* total play time */
int time_remaining = TIME_LIMIT_SECONDS;

/* Basket */
float basket_x = 280.0f;
const float BASKET_Y = 40.0f;
float basket_half_width = 40.0f; /* default half width */
const float BASKET_HEIGHT = 20.0f;

/* Score/highscore */
int score_points = 0;
int highscore = 0;
const char *HIGHSCORE_FILE = "highscore.dat";

/* Chicken (on bamboo) */
float chicken_x = 300.0f;
float chicken_y = 620.0f;
/* base chicken speed */
float chicken_speed = 80.0f; /* pixels/sec moving back and forth */

/* Speed progression tracking */
int game_elapsed_ms = 0; /* accumulated ms while playing */
int speed_stage = 0;     /* 0 = no increases yet, 1 = after 15s, 2 = after 30s, 3 = after 45s */

/* Factor applied at each milestone to both chicken and egg speeds */
const float SPEED_INCREASE_FACTOR = 1.25f;

/* Falling object types */
typedef enum {
    OBJ_EGG_NORMAL,
    OBJ_EGG_BLUE,
    OBJ_EGG_GOLD,
    OBJ_POOP
} ObjType;

typedef struct {
    int active;
    float x, y;
    float vy;      /* pixels/sec (base individual speed) */
    ObjType type;
} FallingObj;

#define MAX_OBJS 30
FallingObj objs[MAX_OBJS];

/* Difficulty / spawn control */
int spawn_accumulator_ms = 0;
int spawn_interval_ms = 900; /* base spawn interval */
/* global multiplier applied to falling velocities (increases with time) */
float global_speed_multiplier = 1.0f;

/* Utility */
void drawText(float x, float y, const char *s);
void reset_game();
void load_highscore();
void save_highscore();
void spawn_from_chicken();
void set_ortho();

/* Random utility */
static int rnd(int a, int b) { return a + rand() % (b - a + 1); }

/* Draw simple chicken */
void draw_chicken(float cx, float cy) {
    /* simple yellow ellipse + beak */
    glColor3f(1.0f, 0.9f, 0.0f);
    glBegin(GL_POLYGON);
    for (int i = 0; i < 32; ++i) {
        float theta = (2.0f * 3.1415926f * (float)i) / 32.0f;
        float rx = cosf(theta) * 22.0f;
        float ry = sinf(theta) * 16.0f;
        glVertex2f(cx + rx, cy + ry);
    }
    glEnd();
    /* eye */
    glColor3f(0, 0, 0);
    glPointSize(4.0f);
    glBegin(GL_POINTS);
    glVertex2f(cx + 6, cy + 6);
    glEnd();
    /* beak */
    glColor3f(1.0f, 0.6f, 0.0f);
    glBegin(GL_TRIANGLES);
    glVertex2f(cx + 22, cy);
    glVertex2f(cx + 32, cy + 5);
    glVertex2f(cx + 32, cy - 5);
    glEnd();
}

/* Draw bamboo stick horizontally under chicken */
void draw_bamboo() {
    glColor3f(0.6f, 0.4f, 0.2f);
    glLineWidth(8.0f);
    glBegin(GL_LINES);
    glVertex2f(20, chicken_y - 12);
    glVertex2f(WIN_W - 20, chicken_y - 12);
    glEnd();
}

/* Draw basket */
void draw_basket() {
    float w = basket_half_width;
    float x = basket_x;
    float y = BASKET_Y;
    /* rim */
    glColor3f(0.8f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(x - w, y + 8);
    glVertex2f(x + w, y + 8);
    glVertex2f(x + w + 6, y);
    glVertex2f(x - w - 6, y);
    glEnd();
    /* body */
    glColor3f(0.9f, 0.5f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(x - w - 6, y);
    glVertex2f(x + w + 6, y);
    glVertex2f(x + w, y - BASKET_HEIGHT);
    glVertex2f(x - w, y - BASKET_HEIGHT);
    glEnd();
}

/* Draw falling object */
void draw_obj(const FallingObj *o) {
    if (!o->active) return;
    switch (o->type) {
        case OBJ_EGG_NORMAL:
            glColor3f(1.0f, 1.0f, 0.9f);
            break;
        case OBJ_EGG_BLUE:
            glColor3f(0.3f, 0.6f, 1.0f);
            break;
        case OBJ_EGG_GOLD:
            glColor3f(1.0f, 0.85f, 0.15f);
            break;
        case OBJ_POOP:
            glColor3f(0.25f, 0.15f, 0.05f);
            break;
    }
    /* draw as egg/poop ellipse */
    float rx = 8.0f;
    float ry = 12.0f;
    glBegin(GL_POLYGON);
    for (int i = 0; i < 24; ++i) {
        float th = (2.0f * 3.1415926f * i) / 24.0f;
        glVertex2f(o->x + cosf(th) * rx, o->y + sinf(th) * ry);
    }
    glEnd();
}

/* Reset all objects inactive */
void clear_objs() {
    for (int i = 0; i < MAX_OBJS; ++i) objs[i].active = 0;
}

/* Reset game to starting state */
void reset_game() {
    score_points = 0;
    time_remaining = TIME_LIMIT_SECONDS;
    basket_half_width = 40.0f;
    global_speed_multiplier = 1.0f;
    chicken_x = WIN_W * 0.5f; /* center chicken */
    chicken_y = 620.0f;
    chicken_speed = 80.0f; /* base automatic movement speed */
    clear_objs();
    spawn_accumulator_ms = 0;
    /* place basket centered */
    basket_x = WIN_W * 0.5f;

    /* reset speed progression */
    game_elapsed_ms = 0;
    speed_stage = 0;
}

/* Load highscore from file */
void load_highscore() {
    FILE *f = fopen(HIGHSCORE_FILE, "rb");
    if (!f) { highscore = 0; return; }
    if (fread(&highscore, sizeof(int), 1, f) != 1) highscore = 0;
    fclose(f);
}

/* Save highscore to file */
void save_highscore() {
    if (score_points > highscore) {
        highscore = score_points;
        FILE *f = fopen(HIGHSCORE_FILE, "wb");
        if (!f) return;
        fwrite(&highscore, sizeof(int), 1, f);
        fclose(f);
    }
}

/* Spawn a new falling object at chicken position (x +/- small jitter)
   Only eggs and poop spawn. */
void spawn_from_chicken() {
    /* find slot */
    for (int i = 0; i < MAX_OBJS; ++i) {
        if (!objs[i].active) {
            objs[i].active = 1;
            objs[i].x = chicken_x + rnd(-18, 18);
            objs[i].y = chicken_y - 18;
            /* decide type probabilistically (no perks) */
            int r = rnd(1, 100);
            if (r <= 6) {
                objs[i].type = OBJ_EGG_GOLD; /* rare */
                objs[i].vy = 110.0f;
            } else if (r <= 20) {
                objs[i].type = OBJ_EGG_BLUE;
                objs[i].vy = 100.0f;
            } else if (r <= 30) {
                objs[i].type = OBJ_POOP;
                objs[i].vy = 130.0f;
            } else {
                objs[i].type = OBJ_EGG_NORMAL;
                objs[i].vy = 90.0f;
            }
            break;
        }
    }
}

/* Collision check and scoring */
void update_collision_and_score(FallingObj *o) {
    if (!o->active) return;
    /* If object hit basket region */
    float top_of_basket = BASKET_Y + 8;
    float bottom_of_basket = BASKET_Y - BASKET_HEIGHT;
    if (o->y <= top_of_basket && o->y >= bottom_of_basket) {
        /* x overlap check */
        float leftb = basket_x - basket_half_width - 6;
        float rightb = basket_x + basket_half_width + 6;
        if (o->x >= leftb && o->x <= rightb) {
            /* Caught */
            switch (o->type) {
                case OBJ_EGG_NORMAL: score_points += 1; break;
                case OBJ_EGG_BLUE: score_points += 5; break;
                case OBJ_EGG_GOLD: score_points += 10; break;
                case OBJ_POOP: score_points -= 10; break;
            }
            o->active = 0;
        }
    }
}

/* Draw HUD (score, time, highscore) */
void draw_hud() {
    char buf[128];
    sprintf(buf, "Score: %d", score_points);
    drawText(10, WIN_H - 24, buf);
    sprintf(buf, "Time: %d", time_remaining);
    drawText(WIN_W - 120, WIN_H - 24, buf);
    sprintf(buf, "High: %d", highscore);
    drawText(10, WIN_H - 44, buf);
}

/* Display callback */
void display(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    set_ortho();

    if (state == STATE_MENU) {
        /* background */
        glColor3f(0.65f, 0.85f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(WIN_W, 0); glVertex2f(WIN_W, WIN_H); glVertex2f(0, WIN_H);
        glEnd();

        glColor3f(0, 0, 0);
        drawText(WIN_W / 2 - 80, WIN_H / 2 + 80, "EGG CATCHER");
        drawText(WIN_W / 2 - 120, WIN_H / 2 + 40, "S : Start");
        drawText(WIN_W / 2 - 120, WIN_H / 2 + 20, "Q or ESC : Exit");
        drawText(WIN_W / 2 - 120, WIN_H / 2 - 0, "Use mouse to move basket");
        drawText(WIN_W / 2 - 120, WIN_H / 2 - 20, "A/D or Left/Right keys to move basket");
        drawText(WIN_W / 2 - 120, WIN_H / 2 - 40, "P : Pause/Resume");
        drawText(WIN_W / 2 - 120, WIN_H / 2 - 60, "Catch eggs, avoid poop!");
        char buf[64];
        sprintf(buf, "High Score: %d", highscore);
        drawText(WIN_W / 2 - 120, WIN_H / 2 - 90, buf);
    } else if (state == STATE_PLAYING || state == STATE_PAUSED) {
        /* sky background */
        glColor3f(0.6f, 0.9f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(WIN_W, 0); glVertex2f(WIN_W, WIN_H); glVertex2f(0, WIN_H);
        glEnd();

        /* draw bamboo and chicken (moving automatically) */
        draw_bamboo();
        draw_chicken(chicken_x, chicken_y);

        /* draw falling objects */
        for (int i = 0; i < MAX_OBJS; ++i) {
            if (objs[i].active) draw_obj(&objs[i]);
        }

        /* draw basket */
        draw_basket();

        /* HUD */
        draw_hud();

        if (state == STATE_PAUSED) {
            glColor3f(0, 0, 0);
            drawText(WIN_W / 2 - 40, WIN_H / 2, "PAUSED");
            drawText(WIN_W / 2 - 100, WIN_H / 2 - 20, "Press P to resume");
        }
    } else if (state == STATE_GAMEOVER) {
        glColor3f(0.2f, 0.2f, 0.3f);
        glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(WIN_W, 0); glVertex2f(WIN_W, WIN_H); glVertex2f(0, WIN_H);
        glEnd();

        char buf[128];
        sprintf(buf, "Game Over");
        drawText(WIN_W / 2 - 50, WIN_H / 2 + 40, buf);
        sprintf(buf, "Score: %d", score_points);
        drawText(WIN_W / 2 - 50, WIN_H / 2 + 10, buf);
        sprintf(buf, "High: %d", highscore);
        drawText(WIN_W / 2 - 50, WIN_H / 2 - 20, buf);
        drawText(WIN_W / 2 - 120, WIN_H / 2 - 60, "S : Restart    M : Menu    Q/ESC : Quit");
    }

    glutSwapBuffers();
}

/* Draw string helper (bitmap) */
void drawText(float x, float y, const char *s) {
    glColor3f(0, 0, 0);
    glRasterPos2f(x, y);
    while (*s) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *s++);
    }
}

/* Update function called by timer */
void update_game(int value) {
    static int last_time = 0;
    static int initialized = 0;
    int now = glutGet(GLUT_ELAPSED_TIME);
    if (!initialized) { last_time = now; initialized = 1; }
    int dt_ms = now - last_time;
    if (dt_ms > 1000) dt_ms = TIMER_MS; /* avoid huge jumps */
    last_time = now;

    if (state == STATE_PLAYING) {
        /* accumulate elapsed time for speed progression */
        game_elapsed_ms += dt_ms;

        /* check speed milestones: 15s, 30s, 45s
           when hitting a milestone, increase both chicken speed and falling speed multiplier */
        if (speed_stage == 0 && game_elapsed_ms >= 15000) {
            chicken_speed *= SPEED_INCREASE_FACTOR;
            global_speed_multiplier *= SPEED_INCREASE_FACTOR;
            speed_stage = 1;
        } else if (speed_stage == 1 && game_elapsed_ms >= 30000) {
            chicken_speed *= SPEED_INCREASE_FACTOR;
            global_speed_multiplier *= SPEED_INCREASE_FACTOR;
            speed_stage = 2;
        } else if (speed_stage == 2 && game_elapsed_ms >= 45000) {
            chicken_speed *= SPEED_INCREASE_FACTOR;
            global_speed_multiplier *= SPEED_INCREASE_FACTOR;
            speed_stage = 3;
        }

        /* update chicken automatic movement */
        float dt = dt_ms / 1000.0f;
        chicken_x += chicken_speed * dt;
        /* bounce near edges to stay visible and not cross bamboo */
        if (chicken_x < 40) {
            chicken_x = 40;
            chicken_speed = fabs(chicken_speed);
        }
        if (chicken_x > WIN_W - 60) {
            chicken_x = WIN_W - 60;
            chicken_speed = -fabs(chicken_speed);
        }

        /* spawn accumulator */
        spawn_accumulator_ms += dt_ms;
        /* make spawn_interval slightly shorter with higher score */
        int effective_spawn = spawn_interval_ms;
        if (score_points >= 30) effective_spawn = 550;
        else if (score_points >= 15) effective_spawn = 730;
        if (spawn_accumulator_ms >= effective_spawn) {
            spawn_accumulator_ms = 0;
            spawn_from_chicken();
        }

        /* update falling objects */
        for (int i = 0; i < MAX_OBJS; ++i) {
            if (!objs[i].active) continue;
            /* apply global speed multiplier to the object's vy */
            objs[i].y -= objs[i].vy * dt * global_speed_multiplier;
            /* check collision with basket */
            update_collision_and_score(&objs[i]);
            /* if below screen, deactivate */
            if (objs[i].y < -20) objs[i].active = 0;
        }

        /* time remaining */
        static int time_accum = 0;
        time_accum += dt_ms;
        if (time_accum >= 1000) {
            time_accum -= 1000;
            time_remaining -= 1;
            if (time_remaining <= 0) {
                time_remaining = 0;
                /* game over */
                save_highscore();
                state = STATE_GAMEOVER;
            }
        }
    }

    /* redisplay and rearm timer */
    glutPostRedisplay();
    glutTimerFunc(TIMER_MS, update_game, 0);
}

/* Keyboard controls */
void keyboard(unsigned char key, int x, int y) {
    if (state == STATE_MENU) {
        if (key == 's' || key == 'S') {
            reset_game();
            state = STATE_PLAYING;
        } else if (key == 'q' || key == 'Q' || key == 27) {
            save_highscore();
            exit(0);
        }
    } else if (state == STATE_PLAYING) {
        if (key == 'p' || key == 'P') {
            state = STATE_PAUSED;
        } else if (key == 'q' || key == 'Q' || key == 27) {
            save_highscore();
            exit(0);
        } else if (key == 'a' || key == 'A') {
            /* Move basket left */
            basket_x -= 20; if (basket_x < 20) basket_x = 20;
        } else if (key == 'd' || key == 'D') {
            /* Move basket right */
            basket_x += 20; if (basket_x > WIN_W - 20) basket_x = WIN_W - 20;
        }
    } else if (state == STATE_PAUSED) {
        if (key == 'p' || key == 'P') {
            state = STATE_PLAYING;
        } else if (key == 'q' || key == 'Q' || key == 27) {
            save_highscore();
            exit(0);
        } else if (key == 'm' || key == 'M') {
            state = STATE_MENU;
        }
    } else if (state == STATE_GAMEOVER) {
        if (key == 's' || key == 'S') {
            reset_game();
            state = STATE_PLAYING;
        } else if (key == 'm' || key == 'M') {
            state = STATE_MENU;
        } else if (key == 'q' || key == 'Q' || key == 27) {
            save_highscore();
            exit(0);
        }
    }
}

/* Special keys for basket movement (Left/Right arrows) */
void special_keys(int key, int x, int y) {
    if (state == STATE_PLAYING) {
        if (key == GLUT_KEY_LEFT) { basket_x -= 20; if (basket_x < 20) basket_x = 20; }
        if (key == GLUT_KEY_RIGHT) { basket_x += 20; if (basket_x > WIN_W - 20) basket_x = WIN_W - 20; }
    }
}

/* Mouse passive motion to control basket */
void passive_mouse(int x, int y) {
    /* convert window coords (origin top-left) to our ortho (origin bottom-left) */
    int ny = WIN_H - y;
    (void)ny; /* ny not used but kept for clarity */
    basket_x = (float)x;
    /* clamp */
    if (basket_x < 20) basket_x = 20;
    if (basket_x > WIN_W - 20) basket_x = WIN_W - 20;
    glutPostRedisplay();
}

/* Window reshape */
void reshape(int w, int h) {
    WIN_W = w; WIN_H = h;
    glViewport(0, 0, w, h);
    set_ortho();
}

/* Update ortho projection */
void set_ortho() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WIN_W, 0, WIN_H);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

/* Init OpenGL */
void init_gl() {
    glClearColor(0.6f, 0.9f, 1.0f, 1.0f);
    glShadeModel(GL_SMOOTH);
}

/* Entry point */
int main(int argc, char **argv) {
    srand((unsigned int)time(NULL));
    load_highscore();
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("Egg Catcher - GLUT Game (speed-up at 15/30/45s)");
    init_gl();
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