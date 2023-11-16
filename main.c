#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <windows.h>

#include "bmp.h"
#include "vector.h"

int32_t ClientWidth;
int32_t ClientHeight;

int32_t BitmapWidth;
int32_t BitmapHeight;

int8_t running = 1;

#define GRID_SIZE 40

// ARGB
#define PATH 0xFFFFFFFF // White
#define WALL 0xFFFF0000 // Red
#define COIN 0xFFFFFF00 // Yellow
#define FOOD 0xFFFF7F00 // Orange

#define PACMAN 0xFF0000FF // Blue

#define GHOST1 0xFF00FFFF // Cyan
#define GHOST2 0xFFFFC0CB // Pink
#define GHOST3 0xFFFFA500 // Orange
#define GHOST4 0xFF880808 // Blood Red

#define COLOR_BLACK 0xFF000000
#define COLOR_MAGENTA 0xFFFF00FF

typedef uint32_t color_t;
typedef int8_t type_t;

typedef struct ghost_t {
    point_t point;
    type_t type;
} ghost_t;

Image map1 = {0, 0, NULL};
Image map2 = {0, 0, NULL};
Image map3 = {0, 0, NULL};
Image *current_map = NULL;

Image background_texture = {0, 0, NULL};
Image wall_texture = {0, 0, NULL};
Image coin_texture = {0, 0, NULL};
Image food_texture = {0, 0, NULL};

Image pacman_tilemap_texture = {0, 0, NULL};

Image ghost1_texture = {0, 0, NULL};
Image ghost2_texture = {0, 0, NULL};
Image ghost3_texture = {0, 0, NULL};
Image ghost4_texture = {0, 0, NULL};

vector_t walls = EMPTY_VECTOR;
vector_t coins = EMPTY_VECTOR;
vector_t foods = EMPTY_VECTOR;

point_t pacman = ZERO_POINT;

ghost_t ghosts[4] = {0};

LRESULT CALLBACK WndProcedure(HWND, UINT, WPARAM, LPARAM);
int update(void *memory, HDC hdc, BITMAPINFO *bitmap_info);

void gc_putpixel(void *memory, int32_t x, int32_t y, color_t color);
void gc_fill_screen(void *memory, color_t color);
void gc_fill_rectangle(void *memory, int32_t x, int32_t y, int32_t width, int32_t height, color_t color);
void gc_draw_image(void *memory, int32_t x, int32_t y, Image *image);
void gc_draw_map(void *memory, Image *image);


int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR args, int ncmdshow)
{
    map1 = openImage("assets/maps/map#1.bmp");
    map2 = openImage("assets/maps/map#2.bmp");
    map3 = openImage("assets/maps/map#3.bmp");

    background_texture = openImage("assets/textures/background.bmp");
    wall_texture = openImage("assets/textures/wall.bmp");
    coin_texture = openImage("assets/textures/coin.bmp");
    food_texture = openImage("assets/textures/food.bmp");

    pacman_tilemap_texture = openImage("assets/textures/pacman_tilemap.bmp");

    ghost1_texture = openImage("assets/textures/ghost_cyan.bmp");
    ghost2_texture = openImage("assets/textures/ghost_pink.bmp");
    ghost3_texture = openImage("assets/textures/ghost_orange.bmp");
    ghost4_texture = openImage("assets/textures/ghost_red.bmp");

    current_map = &map3;

    if(!current_map->memory) return -1;

    WNDCLASSW wc = {0};
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"Root";
    wc.lpfnWndProc = WndProcedure;

    if(!RegisterClassW(&wc)) return -1;
    
    int32_t Width = current_map->width * GRID_SIZE;
    int32_t Height = current_map->height * GRID_SIZE;

    RECT window_rect = {0};
    window_rect.right = Width;
    window_rect.bottom = Height;
    window_rect.left = 0;
    window_rect.top = 0;

    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0);
    HWND window = CreateWindowW(
        wc.lpszClassName,
        L"Pacman",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        NULL,
        NULL,
        NULL,
        NULL
    );

    GetWindowRect(window, &window_rect);
    ClientWidth = window_rect.right - window_rect.left;
    ClientHeight = window_rect.bottom - window_rect.top;

    BitmapWidth = Width;
    BitmapHeight = Height;

    uint32_t BytesPerPixel = 4;

    void *memory = VirtualAlloc(
        0,
        BitmapWidth*BitmapHeight*BytesPerPixel,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE
    );

    BITMAPINFO bitmap_info;
    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = BitmapWidth;
    bitmap_info.bmiHeader.biHeight = -BitmapHeight;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(window);

    // Draw once
    gc_draw_image(memory, 0, 0, &background_texture);
    gc_draw_map(memory, current_map);

    while(running) {
        MSG msg;
        while(PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if(msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Draw Loop

        update(memory, hdc, &bitmap_info);
    }

    return 0;
}

LRESULT CALLBACK WndProcedure(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg) {
        case WM_DESTROY: {
            PostQuitMessage(0);
            break;
        }
        default: {
            return DefWindowProcW(hWnd, msg, wp, lp);
        }
    }
}

int update(void *memory, HDC hdc, BITMAPINFO *bitmap_info)
{
    return StretchDIBits(
        hdc, 0, 0,
        BitmapWidth, BitmapHeight,
        0, 0,
        BitmapWidth, BitmapHeight,
        memory, bitmap_info,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}

void gc_putpixel(void *memory, int32_t x, int32_t y, color_t color)
{
    if((x >= 0 && x < BitmapWidth) && (y >= 0 && y < BitmapHeight))
        *((color_t *) memory + (x + y * BitmapWidth)) = color; 
}

void gc_fill_screen(void *memory, color_t color)
{
    for(int32_t j = 0; j < BitmapHeight; ++j) {
        for(int32_t i = 0; i < BitmapWidth; ++i) {
            gc_putpixel(memory, i, j, color);
        }
    }
}

void gc_fill_rectangle(void *memory, int32_t x, int32_t y, int32_t width, int32_t height, color_t color)
{
    int8_t ws = width < 0;
    int8_t hs = height < 0;
    int32_t x0 = (x + width) * ws + x * !ws,
            y0 = (y + height) * hs + y * !hs,
            x1 = x * ws + (x + width) * !ws,
            y1 = y * hs + (y + height) * !hs;

    for(int32_t j = y0; j < y1; ++j) {
        for(int32_t i = x0; i < x1; ++i) {
            gc_putpixel(memory, i, j, color);
        }
    }
}

void gc_draw_image(void *memory, int32_t x, int32_t y, Image *image)
{
    for(int32_t j = 0; j < image->height; ++j) {
        for(int32_t i = 0; i < image->width; ++i) {
            color_t color = *((color_t *) image->memory + (i + j * image->width));
            if(((color >> 24) & 0xFF) == 0xFF)
                gc_putpixel(memory, x + i, y + j, color);
        }
    }
}

void gc_draw_map(void *memory, Image *image)
{
    for(int32_t j = 0; j < image->height; ++j) {
        for(int32_t i = 0; i < image->width; ++i) {
            color_t color = *((color_t *) image->memory + (i + j * image->width));
            int32_t x_pos = i * GRID_SIZE;
            int32_t y_pos = j * GRID_SIZE;
            switch(color) {
                case PATH: break;
                case WALL: {
                    gc_draw_image(memory, x_pos, y_pos, &wall_texture);
                    vector_add(&walls, (point_t){x_pos, y_pos});
                    break;
                }
                case COIN: {
                    gc_draw_image(memory, x_pos, y_pos, &coin_texture);
                    vector_add(&coins, (point_t){x_pos, y_pos});
                    break;
                }
                case FOOD: {
                    gc_draw_image(memory, x_pos, y_pos, &food_texture);
                    vector_add(&foods, (point_t){x_pos, y_pos});
                    break;
                }
                case GHOST1: {
                    gc_draw_image(memory, x_pos, y_pos, &ghost1_texture);
                    ghosts[0] = (ghost_t){(point_t){x_pos, y_pos}, 0};
                    break;
                }
                case GHOST2: {
                    gc_draw_image(memory, x_pos, y_pos, &ghost2_texture);
                    ghosts[1] = (ghost_t){(point_t){x_pos, y_pos}, 1};
                    break;
                }
                case GHOST3: {
                    gc_draw_image(memory, x_pos, y_pos, &ghost3_texture);
                    ghosts[2] = (ghost_t){(point_t){x_pos, y_pos}, 2};
                    break;
                }
                case GHOST4: {
                    gc_draw_image(memory, x_pos, y_pos, &ghost4_texture);
                    ghosts[3] = (ghost_t){(point_t){x_pos, y_pos}, 3};
                    break;
                }
                default: {
                    gc_fill_rectangle(memory, x_pos, y_pos, GRID_SIZE, GRID_SIZE, color);
                }
            }
        }
    }
}