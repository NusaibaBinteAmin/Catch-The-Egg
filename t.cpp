
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>

int x1, y1, x2, y2;
int maxWidth;

int winW = 500, winH = 500;

int *px, *py;
int totalPoints = 0;
int currentPoints = 0;

void buildLine(int xStart, int yStart, int xEnd, int yEnd)
{
    int dx = abs(xEnd - xStart);
    int dy = abs(yEnd - yStart);

    int sx = (xStart < xEnd) ? 1 : -1;
    int sy = (yStart < yEnd) ? 1 : -1;

    int err = dx - dy;

    int x = xStart;
    int y = yStart;

    px = (int*)malloc(sizeof(int) * (dx + dy + 10));
    py = (int*)malloc(sizeof(int) * (dx + dy + 10));

    while (1)
    {
        px[totalPoints] = x;
        py[totalPoints] = y;
        totalPoints++;

        if (x == xEnd && y == yEnd)
            break;

        int e2 = 2 * err;

        if (e2 > -dy)
        {
            err -= dy;
            x += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y += sy;
        }
    }
}

void init()
{
    glClearColor(1, 1, 1, 1);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(0.2, 0.4, 0.9);

    glBegin(GL_POINTS);

    for (int i = 0; i < currentPoints && i < totalPoints; i++)
    {
        int r = maxWidth / 2;

        for (int dx = -r; dx <= r; dx++)
        {
            for (int dy = -r; dy <= r; dy++)
            {
                glVertex2i(px[i] + dx, py[i] + dy);
            }
        }
    }

    glEnd();
    glutSwapBuffers();
}

void timer(int value)
{
    if (currentPoints < totalPoints)
        currentPoints++;

    glutPostRedisplay();
    glutTimerFunc(5, timer, 0);
}

int main(int argc, char** argv)
{
    printf("Enter x1 y1 x2 y2: ");
    scanf("%d %d %d %d", &x1, &y1, &x2, &y2);

    printf("Enter max width: ");
    scanf("%d", &maxWidth);

    if (maxWidth < 1) maxWidth = 1;

    buildLine(x1, y1, x2, y2);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(winW, winH);
    glutInitWindowPosition(50, 50);
    glutCreateWindow("Animated Bresenham Line");

    init();
    glutDisplayFunc(display);
    glutTimerFunc(5, timer, 0);

    glutMainLoop();
    return 0;
}
