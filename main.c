#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <windows.h>
#include <time.h>

#include "bmp.h"
#include "vector.h"

int32_t ClientWidth;
int32_t ClientHeight;

int32_t BitmapWidth;
int32_t BitmapHeight;

double delta_time = 0.0f;
clock_t clock_start = 0;
int32_t fps = 0;

double fps_delta_sum = 0.0f;

int8_t running = 1;

#define size(array) (sizeof(array)/sizeof(array[0]))

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
typedef int8_t dir_t;

#define NON_DIR -1
#define EAST 0
#define NORTH_EAST 1
#define NORTH 2
#define NORTH_WEST 3
#define WEST 4
#define SOUTH_WEST 5
#define SOUTH 6
#define SOUTH_EAST 7

#define NON_TYPE -1
#define GHOST_TYPE_0 0
#define GHOST_TYPE_1 1
#define GHOST_TYPE_2 2
#define GHOST_TYPE_3 3
#define DEFAULT_GHOST_TYPE GHOST_TYPE_1

typedef struct player_t {
    point_t location;
    dir_t direction;
    int32_t framecount;
    int32_t max_framecount;
} player_t;

typedef struct ghost_t {
    point_t location;
    type_t type;
} ghost_t;

#define DEFAULT_PLAYER {{0, 0}, EAST, 0, 0}
#define DEFAULT_GHOST {{0, 0}, DEFAULT_GHOST_TYPE}

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

vector_t paths = EMPTY_VECTOR;
vector_t walls = EMPTY_VECTOR;
vector_t coins = EMPTY_VECTOR;
vector_t foods = EMPTY_VECTOR;

player_t pacman = DEFAULT_PLAYER;
double pacman_delta_sum = 0.0f;

ghost_t ghosts[4] = {DEFAULT_GHOST, DEFAULT_GHOST, DEFAULT_GHOST, DEFAULT_GHOST};

LRESULT CALLBACK WndProcedure(HWND, UINT, WPARAM, LPARAM);
int update(void *memory, HDC hdc, BITMAPINFO *bitmap_info);

void load_map(Image *map_image);

void gc_putpixel(void *memory, int32_t x, int32_t y, color_t color);
void gc_fill_screen(void *memory, color_t color);
void gc_fill_rectangle(void *memory, int32_t x, int32_t y, int32_t width, int32_t height, color_t color);
void gc_draw_image(void *memory, int32_t x, int32_t y, Image *image);
void gc_draw_image_region(void *memory, int32_t x, int32_t y, Image *image, point_t region_start, point_t region_end);
void gc_render(void *memory);


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
    pacman.max_framecount = pacman_tilemap_texture.width / GRID_SIZE;
    pacman.framecount = pacman.max_framecount;

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
    load_map(current_map);
    gc_render(memory);

    while(running) {
        clock_start = clock();
        MSG msg;
        while(PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if(msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Draw Loop
        {
            fps_delta_sum += delta_time;
            pacman_delta_sum += delta_time;
            
            if(fps_delta_sum >= 1.0f) {
                printf("FPS: %i\n", fps);
                fps_delta_sum = 0.0f;
                fps = 0;
            }
            if(pacman_delta_sum >= 0.05f) {
                int8_t state =  (--pacman.framecount < 0);
                pacman.framecount = pacman.max_framecount * state + pacman.framecount * !state;
                pacman_delta_sum = 0.0f;
            }

            gc_render(memory);
            update(memory, hdc, &bitmap_info);
        }
        delta_time = ((double)(clock() - clock_start))/CLOCKS_PER_SEC;
        fps++;
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

void load_map(Image *map_image)
{
    for(int32_t j = 0; j < map_image->height; ++j) {
        for(int32_t i = 0; i <  map_image->width; ++i) {
            color_t color = *((color_t *)  map_image->memory + (i + j *  map_image->width));
            int32_t x_pos = i * GRID_SIZE;
            int32_t y_pos = j * GRID_SIZE;
            switch(color) {
                case PATH: {
                    vector_add(&paths, (point_t){x_pos, y_pos});
                    break;
                }
                case WALL: {
                    vector_add(&walls, (point_t){x_pos, y_pos});
                    break;
                }
                case COIN: {
                    vector_add(&coins, (point_t){x_pos, y_pos});
                    break;
                }
                case FOOD: {
                    vector_add(&foods, (point_t){x_pos, y_pos});
                    break;
                }
                case GHOST1: {
                    ghosts[0] = (ghost_t){(point_t){x_pos, y_pos}, 0};
                    break;
                }
                case GHOST2: {
                    ghosts[1] = (ghost_t){(point_t){x_pos, y_pos}, 1};
                    break;
                }
                case GHOST3: {
                    ghosts[2] = (ghost_t){(point_t){x_pos, y_pos}, 2};
                    break;
                }
                case GHOST4: {
                    ghosts[3] = (ghost_t){(point_t){x_pos, y_pos}, 3};
                    break;
                }
                case PACMAN: {
                    pacman.location = (point_t) {x_pos, y_pos};
                    break;
                }
                default: {
                    vector_add(&paths, (point_t){x_pos, y_pos});
                }
            }
        }
    }
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

void gc_draw_image_region(void *memory, int32_t x, int32_t y, Image *image, point_t region_start, point_t region_end)
{
    point_t sample = {0, 0};
    color_t color = 0xFF000000;
    for(int32_t j = region_start.y; j < region_end.y; ++j) {
        for(int32_t i = region_start.x; i < region_end.x; ++i) {
            color = *((color_t *) image->memory + (i + j * image->width));
            sample.x = x + (i - region_start.x);
            sample.y = y + (j - region_start.y);
            if(((color >> 24) & 0xFF) == 0xFF) {
                gc_putpixel(memory, sample.x, sample.y, color);
            } else {
                color = *((color_t *) background_texture.memory + (sample.x + sample.y * background_texture.width));
                gc_putpixel(memory, sample.x, sample.y, color);
            }
            
        }
    }
}

void gc_render(void *memory)
{
    point_t loc = {0, 0};
    for(int32_t i = 0; i < walls.size; ++i) {
        loc = vector_get(&walls, i);
        gc_draw_image(memory, loc.x, loc.y, &wall_texture);
        
    }
    for(int32_t i = 0; i < coins.size; ++i) {
        loc = vector_get(&coins, i);
        gc_draw_image(memory, loc.x, loc.y, &coin_texture);
    }
    for(int32_t i = 0; i < foods.size; ++i) {
        loc = vector_get(&foods, i);
        gc_draw_image(memory, loc.x, loc.y, &food_texture);
    }
    gc_draw_image_region(
        memory, pacman.location.x, pacman.location.y, &pacman_tilemap_texture,
        (point_t){pacman.framecount * GRID_SIZE, pacman.direction * GRID_SIZE},
        (point_t){(pacman.framecount + 1) * GRID_SIZE, (pacman.direction + 1) * GRID_SIZE}
    );
    for(int i = 0; i < size(ghosts); ++i) {
        Image *end_ghost_texture = NULL;
        switch(ghosts[i].type) {
            case GHOST_TYPE_0: {
                end_ghost_texture = &ghost1_texture;
                break;
            }
            case GHOST_TYPE_1: {
                end_ghost_texture = &ghost2_texture;
                break;
            }
            case GHOST_TYPE_2: {
                end_ghost_texture = &ghost3_texture;
                break;
            }
            case GHOST_TYPE_3: {
                end_ghost_texture = &ghost4_texture;
                break;
            }
        }
        gc_draw_image(memory, ghosts[i].location.x, ghosts[i].location.y, end_ghost_texture);
    }
}