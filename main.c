#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>

#include <string.h>

#include <X11/Xlib.h>

#define abs(v) (v<0?v*-1:v)

#define CELL_ROWS 3
#define CELL_COLS 3

#define CELL_SIZE 300

#define HEIGHT CELL_ROWS*CELL_SIZE
#define WIDTH CELL_COLS*CELL_SIZE

int8_t board[CELL_ROWS][CELL_COLS] = {
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0}
};

int8_t turn = 1;
int8_t won = 0;

uint32_t decodeRGB(uint8_t, uint8_t, uint8_t);
int8_t in_bounds(int32_t, int32_t, int64_t, int64_t);
void gc_put_pixel(void *, int32_t, int32_t, uint32_t);
void gc_fill_rectangle(void *, int32_t, int32_t, int32_t, int32_t, uint32_t);
void gc_draw_line(void *, int32_t, int32_t, int32_t, int32_t, uint32_t);
void gc_draw_circle(void *, int32_t, int32_t, int32_t, uint32_t);
void update(Display *, GC *, Window *, XImage *);

int32_t pad0 = 20;
int32_t pad1 = 40;

void draw_bars(void *memory)
{
    for(int i = CELL_SIZE; i < CELL_SIZE*CELL_COLS; i+=CELL_SIZE) {
        gc_draw_line(
            memory,
            i, pad0,
            i, CELL_SIZE*CELL_ROWS - pad0,
            0x00FFFFFF
        );
    }

    for(int j = CELL_SIZE; j < CELL_SIZE*CELL_ROWS; j+=CELL_SIZE) {
        gc_draw_line(
            memory,
            pad0, j,
            CELL_SIZE*CELL_COLS - pad0, j,
            0x00FFFFFF
        );
    }
}

void draw_board(void *memory)
{
    for(int j = 0; j < CELL_ROWS; ++j) {
        for(int i = 0; i < CELL_COLS; ++i) {
            switch(board[j][i]) {
                case 'O': {
                    gc_draw_circle(
                        memory,
                        i*CELL_SIZE + CELL_SIZE/2,
                        j*CELL_SIZE + CELL_SIZE/2,
                        CELL_SIZE/2 - pad1,
                        0x0000FF00
                    );
                    break;
                }
                case 'X': {
                    gc_draw_line(
                        memory,
                        i*CELL_SIZE + pad1, j*CELL_SIZE + pad1,
                        (i+1)*CELL_SIZE - pad1, (j+1)*CELL_SIZE - pad1,
                        0x00FF0000
                    );
                    gc_draw_line(
                        memory,
                        i*CELL_SIZE + pad1, (j+1)*CELL_SIZE - pad1,
                        (i+1)*CELL_SIZE - pad1, j*CELL_SIZE + pad1,
                        0x00FF0000
                    );
                    break;
                }
            }
        }
    }
}

int8_t detect_win()
{
    for(int j = 0; j < CELL_ROWS; ++j) {
        int sum = board[j][0] + board[j][1] + board[j][2];
        if(sum == 3*79) return 'O';
        if(sum == 3*88) return 'X';
    }

    for(int i = 0; i < CELL_COLS; ++i) {
        int sum = board[0][i] + board[1][i] + board[2][i];
        if(sum == 3*79) return 'O';
        if(sum == 3*88) return 'X';
    }
    int diagonal_sum0 = board[0][0] + board[1][1] + board[2][2];
    int diagonal_sum1 = board[0][2] + board[1][1] + board[2][0];
    if(diagonal_sum0 == 3*79 || diagonal_sum1 == 3*79) return 'O';
    if(diagonal_sum0 == 3*88 || diagonal_sum1 == 3*88) return 'X';

    int total_sum = 0;
    for(int j = 0; j < CELL_ROWS; ++j) {
        for(int i = 0; i < CELL_COLS; ++i) {
            total_sum += board[j][i]==0?0:1;
        }
    }
    return total_sum==9?'D':0;
}

void put_sign(int x, int y, Display *display, Window *window)
{
    if(board[y][x] == 0) {
        if(turn) board[y][x] = 'O';
        else board[y][x] = 'X';
        switch(detect_win()) {
            case 'X': {
                XStoreName(display, *window, "Tic Tac Toe - X won the game! Press F5 to restart!");
                won = !won;
                break;
            }
            case 'O': {
                XStoreName(display, *window, "Tic Tac Toe - O won the game! Press F5 to restart!");
                won = !won;
                break;
            }
            case 'D': {
                XStoreName(display, *window, "Tic Tac Toe - Draw! Press F5 to restart!");
                won = !won;
                break;
            }
        }
        if(!won) {
            turn = !turn;
            XStoreName(display, *window, (turn?"Tic Tac Toe - O turn":"Tic Tac Toe - X turn"));
        }
    }
}

int8_t exitloop = 0;
int8_t auto_update = 0;

int main(void)
{
    Display *display = XOpenDisplay(NULL);

    int screen = DefaultScreen(display);

    Window window = XCreateSimpleWindow(
        display, RootWindow(display, screen),
        0, 0,
        WIDTH, HEIGHT,
        0, 0,
        0
    );

    XStoreName(display, window, (turn?"Tic Tac Toe - O turn":"Tic Tac Toe - X turn"));

    char *memory = (char *) malloc(sizeof(uint32_t)*HEIGHT*WIDTH);

    XWindowAttributes winattr = {0};
    XGetWindowAttributes(display, window, &winattr);

    XImage *image = XCreateImage(
        display, winattr.visual, winattr.depth,
        ZPixmap, 0, memory,
        WIDTH, HEIGHT,
        32, WIDTH*4
    );

    GC graphics = XCreateGC(display, window, 0, NULL);

    Atom delete_window = XInternAtom(display, "WM_DELETE_WINDOW", 0);
    XSetWMProtocols(display, window, &delete_window, 1);

    Mask mouse = ButtonPressMask | ButtonReleaseMask;
    Mask key = KeyPressMask | KeyReleaseMask;
    XSelectInput(display, window, ExposureMask | mouse | key);

    XMapWindow(display, window);
    XSync(display, False);

    XEvent event;

    draw_bars(memory);
    draw_board(memory);
    update(display, &graphics, &window, image);

    while(!exitloop) {
        while(XPending(display) > 0) {
            XNextEvent(display, &event);
            switch(event.type) {
                case Expose: {
                    update(display, &graphics, &window, image);
                    break;
                }
                case ClientMessage: {
                    if((Atom) event.xclient.data.l[0] == delete_window) {
                        exitloop = 1;   
                    }
                    break;
                }
                case ButtonPress: {
                    if(event.xbutton.button == Button1 && !won) {
                        put_sign(event.xkey.x/CELL_SIZE, event.xkey.y/CELL_SIZE, display, &window);
                        draw_board(memory);
                        update(display, &graphics, &window, image);
                    }
                    break;
                }
                case KeyPress: {
                    if(event.xkey.keycode == 0x47 && won) {
                        for(int j = 0; j < CELL_ROWS; ++j) {
                            for(int i = 0; i < CELL_COLS; ++i) {
                                board[j][i] = 0;
                            }
                        }
                        turn = 1;
                        won = 0;
                        gc_fill_rectangle(memory, 0, 0, WIDTH, HEIGHT, 0x00000000);
                        draw_bars(memory);
                        draw_board(memory);
                        XStoreName(display, window, (turn?"Tic Tac Toe - O turn":"Tic Tac Toe - X turn"));
                        update(display, &graphics, &window, image);
                    }
                    break;
                }
            }
        }
    }


    XCloseDisplay(display);

    free(memory);

    return 0;
}

uint32_t decodeRGB(uint8_t r, uint8_t g, uint8_t b)
{
    return (r << 16) + (g << 8) + b;
}


int8_t in_bounds(int32_t x, int32_t y, int64_t w, int64_t h)
{
    return (x >= 0 && x < w && y >= 0 && y < h);
}


void gc_put_pixel(void *memory, int32_t x, int32_t y, uint32_t color)
{
    if(in_bounds(x, y, WIDTH, HEIGHT))
        *((uint32_t *) memory + y * WIDTH + x) = color;
}

void gc_fill_rectangle(void *memory, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    for(int j = y0; j <= y1; ++j) {
        for(int i = x0; i <= x1; ++i) {
            gc_put_pixel(memory, i, j, color);
        }
    }
}

void gc_draw_line(void *memory, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    int dx = x1 - x0;
    int dy = y1 - y0;

    int steps = abs(dy)>abs(dx)?abs(dy):abs(dx);

    float X = x0;
    float Y = y0;

    float Xinc = dx / (float) steps;
    float Yinc = dy / (float) steps;

    for(int i = 0; i < steps; ++i) {
        gc_put_pixel(memory, (int) X, (int) Y, color);
        X += Xinc;
        Y += Yinc;
    }
}

void gc_draw_circle(void *memory, int32_t cx, int32_t cy, int32_t r, uint32_t color)
{
    for(int j = cy-r; j < cy+r; ++j) {
        for(int i = cx-r; i < cx+r; ++i) {
            int dx = cx - i;
            int dy = cy - j;
            int sqrlen = dx*dx + dy*dy;
            if(sqrlen >= (r-1)*(r-1) && sqrlen <= r*r) {
                gc_put_pixel(memory, i, j, color);
            }
        }
    }
}

void update(Display *display, GC *graphics, Window *window, XImage *image)
{
    XPutImage(
        display,
        *window,
        *graphics,
        image,
        0, 0,
        0, 0,
        WIDTH, HEIGHT
    );

    XSync(display, False);
}
