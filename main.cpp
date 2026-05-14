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



/* Chicken (on bamboo) */
float chicken_x = 300.0f;
float chicken_y = 620.0f;
/* base chicken speed */
float chicken_speed = 80.0f; /* pixels/sec moving back and forth */


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


/* Utility */
void drawText(float x, float y, const char *s);

void set_ortho();



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
        //char buf[64];
        //sprintf(buf, "High Score: %d", highscore);
        //drawText(WIN_W / 2 - 120, WIN_H / 2 - 90, buf);
    }
    else if (state == STATE_PLAYING || state == STATE_PAUSED) {
        /* sky background */
        glColor3f(0.6f, 0.9f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(WIN_W, 0); glVertex2f(WIN_W, WIN_H); glVertex2f(0, WIN_H);
        glEnd();

        /* draw bamboo and chicken (moving automatically) */
        draw_bamboo();
        draw_chicken(chicken_x, chicken_y);


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
    //if (dt_ms > 1000) dt_ms = TIMER_MS; /* avoid huge jumps */
    last_time = now;

    if (state == STATE_PLAYING) {
        /* accumulate elapsed time for speed progression */
        //game_elapsed_ms += dt_ms;



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





    }

    /* redisplay and rearm timer */
    glutPostRedisplay();
    glutTimerFunc(TIMER_MS, update_game, 0);
}

/* Keyboard controls */
void keyboard(unsigned char key, int x, int y) {
    if (state == STATE_MENU) {
        if (key == 's' || key == 'S') {
            //reset_game();
            state = STATE_PLAYING;
        } else if (key == 'q' || key == 'Q' || key == 27) {

            exit(0);
        }
    }

    else if (state == STATE_PAUSED) {
        if (key == 'p' || key == 'P') {
            state = STATE_PLAYING;
        } else if (key == 'q' || key == 'Q' || key == 27) {

            exit(0);
        } else if (key == 'm' || key == 'M') {
            state = STATE_MENU;
        }

    }
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

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("Egg Catcher - GLUT Game (speed-up at 15/30/45s)");
    init_gl();
    glutDisplayFunc(display);

    glutKeyboardFunc(keyboard);


    glutTimerFunc(TIMER_MS, update_game, 0);


    glutMainLoop();
    return 0;
}

