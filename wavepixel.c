/*
    MIT License

    Copyright (c) Ivan Svarkovsky - 2025

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

/*

    What is it?

    WavePixel (shadertoy effect hybrid midi) is a hybrid application merging real-time OpenGL graphics with MIDI audio playback. It displays a dynamic Shadertoy-inspired visual scene (color-blended landscape with sun, clouds, and parallax effects) while playing MIDI files with customizable audio effects (reverb, chorus, vibrato, etc.), all processed in real-time.

    Requirements

    Files: Place .mid (MIDI) and optionally .sf2 (SoundFont) files in the program directory.
    OS: Windows/Linux with SDL2, SDL2_mixer, GLEW, and OpenGL installed.

    Controls

    Graphics:
        F: Toggle fullscreen.
        P: Next color palette.
        Ctrl+P: Toggle parallax effect.
        O: Toggle clouds.
        S: Toggle sun.
        X: Toggle color blending (off by default).
        C: Reset to black palette.
        Esc/Q: Quit.
    MIDI Playback:
        Right Arrow: Next track.
        Left Arrow: Previous track.
        Left + Right Arrows (hold 2s): Pause/Resume.

    Features

    Graphics: Smooth color transitions, grid floor, sun with bloom, optional clouds/parallax.
    Audio: Plays MIDI files with effects (reverb, chorus, vibrato, tremolo, echo, stereo) enabled by default. Works without .sf2, but audio is disabled with a warning.
    Behavior: MIDI loops automatically; graphics run continuously.

*/

/*
    gcc -std=c17 -o wavepixel wavepixel.c -lSDL2 -lSDL2_mixer -lGLEW -lGL -lGLU -lm \
    -Ofast -flto=$(nproc) \
    -march=native -mtune=native \
    -mfpmath=sse \
    -falign-functions=16 -falign-loops=16 -fomit-frame-pointer -fno-ident -fno-asynchronous-unwind-tables -fvisibility=hidden -fno-plt \
    -ftree-vectorize -fopt-info-vec \
    -fipa-pta -fipa-icf -fipa-cp-clone -funroll-loops -floop-interchange -fgraphite-identity -floop-nest-optimize -fmerge-all-constants \
    -fvariable-expansion-in-unroller \
    -fno-stack-protector \
    -Wl,-z,norelro \
    -s -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,--strip-all \
    -pipe -DNDEBUG

*/

/*
    В коде реализована система плавного перехода цветов с использованием HSV-пространства:

    static float blend_factor = 0.0f

    Описание: Это коэффициент смешивания (от 0.0 до 1.0), который определяет, насколько текущий цвет "перетекает" в следующий в процессе интерполяции.
    Влияние:
        Меньше значение (ближе к 0.0): Цвет ближе к текущему (current_palette). Например, при blend_factor = 0.0 вы видите только текущий цвет, без влияния следующего.
        Больше значение (ближе к 1.0): Цвет приближается к следующему (next_palette). При blend_factor = 1.0 текущий цвет полностью заменяется следующим.
    Динамика: Значение увеличивается со временем (в цикле main) на величину blend_speed * 0.016f за кадр (при 60 FPS это примерно 1/60 секунды). Когда достигает 1.0, текущий цвет становится следующим, и процесс начинается заново.

    static float blend_speed = 0.5f

    Описание: Это скорость изменения blend_factor, которая регулирует, как быстро происходит переход между цветами.
    Влияние:
        Меньше значение (например, 0.1): Переход медленнее. При blend_speed = 0.1 полный переход (от 0.0 до 1.0) занимает около 10 секунд (0.1 * 0.016 * 625 кадров = 1.0).
        Больше значение (например, 1.0): Переход быстрее. При blend_speed = 1.0 переход занимает около 1 секунды (1.0 * 0.016 * 62.5 кадров = 1.0).
    Эффект: Чем выше blend_speed, тем более резкими и частыми будут смены цветов. Низкое значение создаёт плавный, почти незаметный переход.

    HSV-пространство в generate_color

    Цвета генерируются в HSV, а затем преобразуются в RGB. Каждый компонент влияет на итоговый цвет:
    H (Hue): Случайное значение от 0 до 360 градусов

    Описание: Оттенок, определяющий "цвет" (красный, синий, зелёный и т.д.) в цветовом круге.
    Влияние:
        Меньше значение (0–60°): Тёплые тона (красный, оранжевый, жёлтый).
        Больше значение (180–360°): Холодные тона (голубой, синий, фиолетовый).
        Случайность: Используется xorshift32(&seed) % 360, что даёт полный спектр оттенков, равномерно распределённых по кругу.
    Эффект: Разнообразие оттенков, от ярко-красного до глубокого синего.

    S (Saturation): От 0.7 до 1.0

    Описание: Насыщенность, определяющая "чистоту" цвета.
    Влияние:
        Меньше значение (0.7): Цвет менее насыщенный, ближе к серому, но всё ещё яркий благодаря высокому V.
        Больше значение (1.0): Максимально чистый, насыщенный цвет (например, ярко-красный вместо бледно-розового).
        Диапазон: 0.7 + (xorshift32(&seed) % 30) / 100.0f ограничивает разброс до 0.7–1.0.
    Эффект: Высокая насыщенность (минимум 0.7) гарантирует, что цвета остаются выразительными, а не бледными.

    V (Value): От 0.7 до 1.0

    Описание: Яркость, определяющая интенсивность цвета.
    Влияние:
        Меньше значение (0.7): Цвет чуть темнее, но всё ещё заметно яркий.
        Больше значение (1.0): Максимальная яркость, цвет выглядит "светящимся".
        Диапазон: 0.7 + (xorshift32(&seed) % 30) / 100.0f даёт разброс 0.7–1.0.
    Эффект: Высокая яркость (минимум 0.7) делает цвета живыми и заметными, избегая тусклости.

    Преобразование HSV в RGB

    Алгоритм: Стандартная формула делит круг Hue на 6 секторов (0–5), вычисляя промежуточные значения p, q, t для плавных переходов между основными цветами (красный, жёлтый, зелёный, голубой, синий, фиолетовый).
    Эффект: Обеспечивает плавное и естественное разнообразие цветов с сохранением яркости и насыщенности.

    Индекс 0: {0.0f, 0.0f, 0.0f}

    Описание: Фиксированный черный цвет для дефолтного состояния.
    Влияние: При current_palette = 0 или next_palette = 0 эффект начинается или заканчивается на чистом чёрном, что создаёт контраст с яркими цветами.

    Итоговое воздействие

    blend_factor: Контролирует моментальный баланс между двумя цветами. Чем ближе к 0 или 1, тем сильнее доминирует один из цветов.
    blend_speed: Определяет темп смены цветов. Меньше — плавнее и дольше, больше — быстрее и резче.
    HSV:
        H: Даёт разнообразие оттенков (от красного до фиолетового).
        S: Поддерживает высокую насыщенность (0.7–1.0), избегая бледности.
        V: Обеспечивает яркость (0.7–1.0), делая цвета заметными.

    Эти параметры вместе создают динамичный, яркий и насыщенный визуальный эффект с плавными переходами!

*/

#define SDL_MAIN_HANDLED
#define _POSIX_C_SOURCE 200809L // Для strdup на POSIX
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #define STRDUP _strdup
#else
    #define STRDUP strdup
#endif



#define WINDOW_WIDTH  960
#define WINDOW_HEIGHT 540

#ifndef M_PI
    #define M_PI acos(-1.0)
#endif

#define SAMPLE_RATE 44100
#define ECHO_DELAY (SAMPLE_RATE / 4) // 0.25 сек
#define REVERB_DELAY_1 (SAMPLE_RATE / 20)  // 50 мс
#define REVERB_DELAY_2 (SAMPLE_RATE / 10)  // 100 мс
#define REVERB_DELAY_3 6610  // 150 мс
#define REVERB_DELAY_4 (SAMPLE_RATE / 25)  // 40 мс
#define REVERB_DELAY_5 (SAMPLE_RATE / 12)  // 80 мс
#define CHORUS_DELAY_1 (SAMPLE_RATE / 100)  // 10 мс
#define CHORUS_DELAY_2 661   // 15 мс
#define CHORUS_DELAY_3 (SAMPLE_RATE / 50)   // 20 мс
#define STEREO_DELAY (SAMPLE_RATE / 200)  // 5 мс

static Sint16 echo_buffer[ECHO_DELAY] = {0};
static int echo_pos = 0;
static Sint16 reverb_buffer1[REVERB_DELAY_1] = {0};
static Sint16 reverb_buffer2[REVERB_DELAY_2] = {0};
static Sint16 reverb_buffer3[REVERB_DELAY_3] = {0};
static Sint16 reverb_buffer4[REVERB_DELAY_4] = {0};
static Sint16 reverb_buffer5[REVERB_DELAY_5] = {0};
static int reverb_pos1 = 0, reverb_pos2 = 0, reverb_pos3 = 0, reverb_pos4 = 0, reverb_pos5 = 0;
static Sint16 chorus_buffer1[CHORUS_DELAY_1] = {0};
static Sint16 chorus_buffer2[CHORUS_DELAY_2] = {0};
static Sint16 chorus_buffer3[CHORUS_DELAY_3] = {0};
static int chorus_pos1 = 0, chorus_pos2 = 0, chorus_pos3 = 0;
static Sint16 stereo_buffer[STEREO_DELAY] = {0};
static int stereo_pos = 0;

static float vibrato_phase = 0.0f;
static float tremolo_phase = 0.0f;
static float chorus_phase1 = 0.5f;
static float chorus_phase2 = 0.5f;
static float chorus_phase3 = 0.0f;
static float reverb_level = 0.5f;
static float reverb_feedback = 0.5f;
static float reverb_damping = 0.6f;
static float chorus_level = 0.5f;
static float chorus_depth = 0.7f;
static float chorus_speed = 3.0f;
static float stereo_width = 0.55f;
static float global_volume = 0.65f;
static float limiter_threshold = 0.98f;
static int limiter_enabled = 1;
static int reverb_enabled = 1;
static int chorus_enabled = 1;
static int stereo_enabled = 1;
static int vibrato_enabled = 1;
static int tremolo_enabled = 1;
static int echo_enabled = 1;

static int use_arb_sync = -1;
static int sun_enabled = 1;

typedef struct { float r, g, b; } Color;

typedef struct {
    uint32_t seed;
    int palette_size;
    int current_palette;
    int next_palette;
    float blend_factor;
    float blend_speed;
    int blend_enabled;
    int history[3];
} ColorState;

static uint32_t xorshift32(uint32_t* state) {
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

static Color manage_color_state(ColorState* cs, float delta_time) {
    typedef struct { float h, s, v; } HSV;

    HSV generate_hsv(int index) {
        if (index == 0) return (HSV) {0.0f, 0.0f, 0.0f};

        uint32_t seed = cs->seed + index * 12345;

        float h = (float)(xorshift32(&seed) % 360);

        float s = 0.7f + (float)(xorshift32(&seed) % 30) / 100.0f;

        float v = 0.7f + (float)(xorshift32(&seed) % 30) / 100.0f;

        return (HSV) {h, s, v};
    }

    Color hsv_to_rgb(HSV hsv) {
        float h = hsv.h / 60.0f;
        float s = hsv.s;
        float v = hsv.v;
        int i = (int)h;
        float f = h - i;
        float p = v * (1.0f - s);
        float q = v * (1.0f - s * f);
        float t = v * (1.0f - s * (1.0f - f));
        Color c;

        switch (i % 6) {
            case 0:
                c.r = v;
                c.g = t;
                c.b = p;
                break;

            case 1:
                c.r = q;
                c.g = v;
                c.b = p;
                break;

            case 2:
                c.r = p;
                c.g = v;
                c.b = t;
                break;

            case 3:
                c.r = p;
                c.g = q;
                c.b = v;
                break;

            case 4:
                c.r = t;
                c.g = p;
                c.b = v;
                break;

            case 5:
                c.r = v;
                c.g = p;
                c.b = q;
                break;
        }

        return c;
    }

    void select_next_color() {
        int next, attempts = 0;

        do {
            next = xorshift32(&cs->seed) % cs->palette_size;
            attempts++;

            if (attempts > 10) { break; }
        }
        while (next == cs->current_palette || next == cs->history[0] || next == cs->history[1]);

        cs->history[1] = cs->history[0];
        cs->history[0] = cs->current_palette;
        cs->next_palette = next;
    }

    HSV lerp_hsv(HSV a, HSV b, float t) {
        float smooth_t = (1.0f - cosf(t * M_PI)) * 0.5f;
        float h_diff = b.h - a.h;

        if (h_diff > 180.0f) { h_diff -= 360.0f; }

        if (h_diff < -180.0f) { h_diff += 360.0f; }

        float h = a.h + h_diff * smooth_t;

        if (h < 0.0f) { h += 360.0f; }

        if (h >= 360.0f) { h -= 360.0f; }

        float s = a.s + (b.s - a.s) * smooth_t;
        float v = a.v + (b.v - a.v) * smooth_t;
        return (HSV) {h, s, v};
    }

    float calculate_blend_speed(HSV current, HSV next) {
        float h_diff = fabsf(next.h - current.h);

        if (h_diff > 180.0f) { h_diff = 360.0f - h_diff; }

        float s_diff = fabsf(next.s - current.s);
        float v_diff = fabsf(next.v - current.v);
        float total_diff = h_diff / 360.0f + s_diff + v_diff; // Нормализуем Hue
        // Чем больше разница, тем медленнее переход (от 0.01 до 0.05)
//       return 0.05f - (total_diff * 0.04f); // total_diff от 0 до ~1.5, speed от 0.05 до 0.01
//        return 0.05f - (total_diff * 0.045f);
        return 0.0125f - (total_diff * 0.01f); // Диапазон 0.0125–0.0025
    }
    /*
        Формула return 0.05f - (total_diff * 0.04f) в calculate_blend_speed определяет динамическую скорость перехода (blend_speed) на основе разницы между цветами (total_diff). Вот как она работает:

        total_diff: Сумма нормализованных различий между текущим и следующим цветом в HSV:
            Hue (0–360) делится на 360, давая 0–1.
            Saturation и Value (0.8–1.0) добавляют 0–0.2 каждое.
            Итог: total_diff обычно от 0 (цвета идентичны) до ~1.5 (максимальная разница).
        0.04f: Коэффициент масштабирования, который преобразует total_diff в изменение скорости.
            При total_diff = 1.5: 1.5 * 0.04 = 0.06 (чуть больше максимального вычитания).
        0.05f: Базовая скорость (максимальная), когда total_diff = 0.
            Вычитание total_diff * 0.04f уменьшает её.
        Результат:
            Если total_diff = 0 (цвета одинаковы): 0.05 - 0 = 0.05 (быстрый переход).
            Если total_diff = 1 (средняя разница): 0.05 - 0.04 = 0.01 (медленный).
            Если total_diff = 1.5 (максимальная): 0.05 - 0.06 = -0.01 (ограничивается минимально, но логика не доходит до этого).

        Итог: Скорость линейно уменьшается от 0.05 до 0.01 с ростом различий, обеспечивая плавность для далёких цветов и быстроту для близких.
    */

    if (cs->blend_enabled) {
        HSV current = generate_hsv(cs->current_palette);
        HSV next = generate_hsv(cs->next_palette);
        float dynamic_speed = calculate_blend_speed(current, next);
        cs->blend_factor += dynamic_speed * delta_time;

        if (cs->blend_factor >= 1.0f) {
            cs->current_palette = cs->next_palette;
            select_next_color();
            cs->blend_factor -= 1.0f;
        }

        HSV blended = lerp_hsv(current, next, cs->blend_factor);
        return hsv_to_rgb(blended);
    }

    return hsv_to_rgb(generate_hsv(cs->current_palette));
}

const char* vertex_shader_src =
    "#version 120\n"
    "attribute vec2 position;\n"
    "varying vec2 uv;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "    uv = position * 0.5 + 0.5;\n"
    "}\n";

const char* fragment_shader_src =
    "#version 120\n"
    "varying vec2 uv;\n"
    "uniform float time;\n"
    "uniform float battery;\n"
    "uniform vec2 resolution;\n"
    "uniform int sun_enabled;\n"
    "uniform int parallax_enabled;\n"
    "uniform int clouds_enabled;\n"
    "uniform vec3 base_color;\n"
    "float clamp(float x, float minVal, float maxVal) { return min(max(x, minVal), maxVal); }\n"
    "float mix(float x, float y, float a) { return x * (1.0 - a) + y * a; }\n"
    "float smoothstep(float edge0, float edge1, float x) { float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0); return t * t * (3.0 - 2.0 * t); }\n"
    "float sun_effect(float u, float v) {\n"
    "    float len = sqrt(u * u + v * v);\n"
    "    float val = smoothstep(0.3, 0.29, len);\n"
    "    float bloom = smoothstep(0.7, 0.0, len);\n"
    "    float cut = clamp(3.0 * sin((v + time * 0.2 * (battery + 0.02)) * 100.0) + clamp(v * 14.0 + 1.0, -6.0, 6.0), 0.0, 1.0);\n"
    "    return clamp(val * cut, 0.0, 1.0) + bloom * 0.6;\n"
    "}\n"
    "float grid_effect(float u, float v) {\n"
    "    float size_y = v * v * 0.2 * 0.01, size_x = v * 0.01;\n"
    "    u += time * 4.0 * (battery + 0.05);\n"
    "    u = abs(mod(u, 1.0) - 0.5); v = abs(mod(v, 1.0) - 0.5);\n"
    "    return clamp(smoothstep(size_x, 0.0, u) + smoothstep(size_y, 0.0, v) + smoothstep(size_x * 5.0, 0.0, u) * 0.4 * battery + smoothstep(size_y * 5.0, 0.0, v) * 0.4 * battery, 0.0, 3.0);\n"
    "}\n"
    "float heightmap(vec2 uv) {\n"
    "    return sin(uv.x * 10.0 + time) * cos(uv.y * 10.0 + time) * 0.5 + 0.5;\n"
    "}\n"
    "float value_noise(vec2 p) {\n"
    "    vec2 i = floor(p);\n"
    "    vec2 f = fract(p);\n"
    "    vec2 u = f * f * (3.0 - 2.0 * f);\n"
    "    float a = fract(sin(dot(i, vec2(127.1, 311.7))) * 43758.5453);\n"
    "    float b = fract(sin(dot(i + vec2(1.0, 0.0), vec2(127.1, 311.7))) * 43758.5453);\n"
    "    float c = fract(sin(dot(i + vec2(0.0, 1.0), vec2(127.1, 311.7))) * 43758.5453);\n"
    "    float d = fract(sin(dot(i + vec2(1.0, 1.0), vec2(127.1, 311.7))) * 43758.5453);\n"
    "    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);\n"
    "}\n"
    "float clouds(vec2 uv) {\n"
    "    vec2 cloud_uv = uv + vec2(time * 0.1, 0.0);\n"
    "    float noise = value_noise(cloud_uv * 100.0);\n"
    "    return smoothstep(0.985, 0.999, noise);\n"
    "}\n"
    "void main() {\n"
    "    vec2 uv = vec2(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0);\n"
    "    uv.x *= resolution.x / resolution.y;\n"
    "    float fog = smoothstep(0.1, -0.02, abs(uv.y + 0.2));\n"
    "    float r = base_color.r, g = base_color.g, b = base_color.b;\n"
    "    vec2 final_uv = uv;\n"
    "    if (parallax_enabled == 1) {\n"
    "        vec2 view_dir = vec2(0.1, 0.2);\n"
    "        float height = heightmap(uv);\n"
    "        float parallax_scale = 0.15;\n"
    "        vec2 offset = view_dir * (height - 0.5) * parallax_scale;\n"
    "        final_uv = uv + offset;\n"
    "    }\n"
    "    if (final_uv.y < -0.2) {\n"
    "        final_uv.y = 3.0 / (abs(final_uv.y + 0.2) + 0.05); final_uv.x *= final_uv.y;\n"
    "        float gridVal = grid_effect(final_uv.x, final_uv.y);\n"
    "        r = mix(r, 1.0, gridVal); g = mix(g, 0.5, gridVal); b = mix(b, 1.0, gridVal);\n"
    "    } else if (sun_enabled == 1) {\n"
    "        final_uv.y -= battery * 1.1 - 0.51;\n"
    "        float sunVal = sun_effect(final_uv.x + 0.95, final_uv.y + 0.02);\n"
    "        r = mix(r, 1.0, sunVal); g = mix(g, 0.4, sunVal); b = mix(b, 0.1, sunVal);\n"
    "    }\n"
    "    if (clouds_enabled == 1 && final_uv.y > -0.2) {\n"
    "        float cloud_val = clouds(final_uv);\n"
    "        r = mix(r, 0.9, cloud_val); g = mix(g, 0.9, cloud_val); b = mix(b, 0.95, cloud_val);\n"
    "    }\n"
    "    r = mix(r, 0.5, fog * fog * fog); g = mix(g, 0.5, fog * fog * fog); b = mix(b, 0.5, fog * fog * fog);\n"
    "    gl_FragColor = vec4(clamp(r, 0.0, 1.0), clamp(g, 0.0, 1.0), clamp(b, 0.0, 1.0), 1.0);\n"
    "}\n";

typedef struct { GLuint shader_program, vao, vbo; } GLData;

GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        fprintf(stderr, "Shader compilation failed (%s): %s\n", type == GL_VERTEX_SHADER ? "vertex" : "fragment", info_log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint create_shader_program(const char* vertex_src, const char* fragment_src) {
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_src);

    if (!vertex_shader) { return 0; }

    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_src);

    if (!fragment_shader) {
        glDeleteShader(vertex_shader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, 512, NULL, info_log);
        fprintf(stderr, "Shader program linking failed: %s\n", info_log);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(program);
        return 0;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program;
}

int init_gl(GLData* gl) {
    gl->shader_program = create_shader_program(vertex_shader_src, fragment_shader_src);

    if (!gl->shader_program) { return 0; }

    float vertices[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    glGenVertexArrays(1, &gl->vao);
    glGenBuffers(1, &gl->vbo);
    glBindVertexArray(gl->vao);
    glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    GLint pos_attrib = glGetAttribLocation(gl->shader_program, "position");

    if (pos_attrib == -1) { return 0; }

    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return 1;
}

static int first_call = 1;

void render_scene(GLData* gl, int width, int height, float time, Uint32* render_time, int parallax_enabled, int clouds_enabled) {
    Uint32 start = SDL_GetTicks();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(gl->shader_program);
    glUniform1f(glGetUniformLocation(gl->shader_program, "time"), time);
    glUniform1f(glGetUniformLocation(gl->shader_program, "battery"), 1.0f);
    glUniform2f(glGetUniformLocation(gl->shader_program, "resolution"), (float)width, (float)height);
    glUniform1i(glGetUniformLocation(gl->shader_program, "sun_enabled"), sun_enabled);
    glUniform1i(glGetUniformLocation(gl->shader_program, "parallax_enabled"), parallax_enabled);
    glUniform1i(glGetUniformLocation(gl->shader_program, "clouds_enabled"), clouds_enabled);
    glBindVertexArray(gl->vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    int has_arb_sync = glewIsSupported("GL_ARB_sync");

    if (use_arb_sync == -1) { use_arb_sync = has_arb_sync; }

    if (use_arb_sync && has_arb_sync) {
        GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 60000000);
        glDeleteSync(sync);
    }

    else {
        glFinish();
    }

    *render_time = SDL_GetTicks() - start;
}

void stabilize_frame_rate(Uint32 frame_start, Uint32 render_time, float* avg_frame_time, int fullscreen) {
    const float alpha = 0.05f;
    *avg_frame_time = (1.0f - alpha) * (*avg_frame_time) + alpha * render_time;
    float target_fps = fullscreen ? 40.0f : 60.0f;

    if (*avg_frame_time > 33.3f) { target_fps = fullscreen ? 30.0f : 45.0f; }

    Uint32 frame_time = SDL_GetTicks() - frame_start;

    if (frame_time < 1000.0f / target_fps) { SDL_Delay((Uint32)(1000.0f / target_fps - frame_time)); }
}

void audio_effect(void* udata, Uint8* stream, int len) {
    Sint16* buffer = (Sint16*)stream;
    int samples = len / sizeof(Sint16);
    Sint32 max_amplitude = 0;

    for (int i = 0; i < samples; i += 2) {
        Sint16 left_sample = buffer[i];
        Sint16 right_sample = buffer[i + 1];
        left_sample = (Sint16)(left_sample * global_volume);
        right_sample = (Sint16)(right_sample * global_volume);
        Sint32 mixed_left = (Sint32)left_sample;
        Sint32 mixed_right = (Sint32)right_sample;

        if (echo_enabled) {
            mixed_left += echo_buffer[echo_pos] * 0.3f;
            mixed_right += echo_buffer[echo_pos] * 0.3f;
            echo_buffer[echo_pos] = (left_sample + right_sample) / 2;
            echo_pos = (echo_pos + 1) % ECHO_DELAY;
        }

        if (reverb_enabled) {
            Sint16 reverb1 = reverb_buffer1[reverb_pos1] * 0.5f;
            Sint16 reverb2 = reverb_buffer2[reverb_pos2] * 0.4f;
            Sint16 reverb3 = reverb_buffer3[reverb_pos3] * 0.3f;
            Sint16 reverb4 = reverb_buffer4[reverb_pos4] * 0.3f * (1.0f - reverb_damping);
            Sint16 reverb5 = reverb_buffer5[reverb_pos5] * 0.15f * (1.0f - reverb_damping);
            Sint32 reverb_sum = (Sint32)(reverb1 + reverb2 + reverb3 + reverb4 + reverb5);
            mixed_left += reverb_sum * 0.2f;
            mixed_right += reverb_sum * 0.2f;
            Sint16 reverb_input = (left_sample + right_sample) / 2 + reverb_sum * reverb_feedback;
            reverb_buffer1[reverb_pos1] = reverb_input;
            reverb_pos1 = (reverb_pos1 + 1) % REVERB_DELAY_1;
            reverb_buffer2[reverb_pos2] = reverb_input;
            reverb_pos2 = (reverb_pos2 + 1) % REVERB_DELAY_2;
            reverb_buffer3[reverb_pos3] = reverb_input;
            reverb_pos3 = (reverb_pos3 + 1) % REVERB_DELAY_3;
            reverb_buffer4[reverb_pos4] = reverb_input;
            reverb_pos4 = (reverb_pos4 + 1) % REVERB_DELAY_4;
            reverb_buffer5[reverb_pos5] = reverb_input;
            reverb_pos5 = (reverb_pos5 + 1) % REVERB_DELAY_5;
        }

        if (chorus_enabled) {
            int chorus_idx1 = (chorus_pos1 - CHORUS_DELAY_1 + CHORUS_DELAY_1) % CHORUS_DELAY_1;
            int chorus_idx2 = (chorus_pos2 - CHORUS_DELAY_2 + CHORUS_DELAY_2) % CHORUS_DELAY_2;
            int chorus_idx3 = (chorus_pos3 - CHORUS_DELAY_3 + CHORUS_DELAY_3) % CHORUS_DELAY_3;
            float mod1 = 0.5f + chorus_depth * sinf(chorus_phase1);
            float mod2 = 0.5f + chorus_depth * sinf(chorus_phase2);
            float mod3 = 0.5f + chorus_depth * sinf(chorus_phase3);
            Sint16 chorus1 = (Sint16)(chorus_buffer1[chorus_idx1] * mod1 * 0.4f);
            Sint16 chorus2 = (Sint16)(chorus_buffer2[chorus_idx2] * mod2 * 0.4f);
            Sint16 chorus3 = (Sint16)(chorus_buffer3[chorus_idx3] * mod3 * 0.3f);
            mixed_left += (Sint32)(chorus1 + chorus2 + chorus3) * 0.15f;
            mixed_right += (Sint32)(chorus1 + chorus2 + chorus3) * 0.15f;
            chorus_buffer1[chorus_pos1] = (left_sample + right_sample) / 2;
            chorus_pos1 = (chorus_pos1 + 1) % CHORUS_DELAY_1;
            chorus_buffer2[chorus_pos2] = (left_sample + right_sample) / 2;
            chorus_pos2 = (chorus_pos2 + 1) % CHORUS_DELAY_2;
            chorus_buffer3[chorus_pos3] = (left_sample + right_sample) / 2;
            chorus_pos3 = (chorus_pos3 + 1) % CHORUS_DELAY_3;
            chorus_phase1 += 2 * M_PI * chorus_speed / SAMPLE_RATE;

            if (chorus_phase1 > 2 * M_PI) { chorus_phase1 -= 2 * M_PI; }

            chorus_phase2 += 2 * M_PI * chorus_speed / SAMPLE_RATE;

            if (chorus_phase2 > 2 * M_PI) { chorus_phase2 -= 2 * M_PI; }

            chorus_phase3 += 2 * M_PI * chorus_speed / SAMPLE_RATE;

            if (chorus_phase3 > 2 * M_PI) { chorus_phase3 -= 2 * M_PI; }
        }

        if (vibrato_enabled) {
            float vibrato = sinf(vibrato_phase) * 0.03f;
            mixed_left = (Sint32)(mixed_left * (1.0f + vibrato));
            mixed_right = (Sint32)(mixed_right * (1.0f + vibrato));
            vibrato_phase += 2 * M_PI * 3.0f / SAMPLE_RATE;

            if (vibrato_phase > 2 * M_PI) { vibrato_phase -= 2 * M_PI; }
        }

        if (tremolo_enabled) {
            float tremolo = 0.85f + 0.075f * sinf(tremolo_phase);
            mixed_left = (Sint32)(mixed_left * tremolo);
            mixed_right = (Sint32)(mixed_right * tremolo);
            tremolo_phase += 2 * M_PI * 3.0f / SAMPLE_RATE;

            if (tremolo_phase > 2 * M_PI) { tremolo_phase -= 2 * M_PI; }
        }

        if (stereo_enabled) {
            Sint16 stereo_delayed = stereo_buffer[stereo_pos];
            mixed_left += (Sint32)(stereo_delayed * 0.5f);
            mixed_right += (Sint32)(stereo_delayed * -0.5f);
            stereo_buffer[stereo_pos] = (left_sample + right_sample) / 2;
            stereo_pos = (stereo_pos + 1) % STEREO_DELAY;
        }

        max_amplitude = abs(mixed_left) > max_amplitude ? abs(mixed_left) : max_amplitude;
        max_amplitude = abs(mixed_right) > max_amplitude ? abs(mixed_right) : max_amplitude;
        buffer[i] = (Sint16)mixed_left;
        buffer[i + 1] = (Sint16)mixed_right;
    }

    if (max_amplitude > 32767) {
        float scale = 32767.0f / max_amplitude;

        for (int i = 0; i < samples; i += 2) {
            buffer[i] = (Sint16)(buffer[i] * scale);
            buffer[i + 1] = (Sint16)(buffer[i + 1] * scale);
        }
    }
}

typedef struct {
    char** files;
    int count;
    int capacity;
} MidiList;

MidiList* midi_list_init() {
    MidiList* list = malloc(sizeof(MidiList));
    list->count = 0;
    list->capacity = 10;
    list->files = malloc(list->capacity * sizeof(char*));
    return list;
}

void midi_list_add(MidiList* list, const char* filename) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->files[i], filename) == 0) { return; }
    }

    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->files = realloc(list->files, list->capacity * sizeof(char*));
    }

    list->files[list->count++] = strdup(filename);
}

void midi_list_free(MidiList* list) {
    if (list) {
        for (int i = 0; i < list->count; i++) { free(list->files[i]); }

        free(list->files);
        free(list);
    }
}

#ifdef _WIN32
    #include <windows.h>
    #define STRDUP _strdup
    #define DIR_SCAN_TYPE WIN32_FIND_DATA
    #define DIR_OPEN(dir) FindFirstFile("*.mid", &dir)
    #define DIR_NEXT(hFind, fd) FindNextFile(hFind, &fd)
    #define DIR_CLOSE(hFind) FindClose(hFind)
    #define DIR_VALID(hFind) (hFind != INVALID_HANDLE_VALUE)
    #define DIR_NAME(fd) fd.cFileName
    #define IS_DIR(fd) (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
#else
    #include <dirent.h>
    #define STRDUP strdup
    #define DIR_SCAN_TYPE struct dirent
    #define DIR_OPEN(dir) opendir(".")
    #define DIR_NEXT(dir, entry) (entry = readdir(dir))
    #define DIR_CLOSE(dir) closedir(dir)
    #define DIR_VALID(dir) (dir != NULL)
    #define DIR_NAME(entry) entry->d_name
    #define IS_DIR(entry) (entry->d_type == DT_DIR)
#endif

void update_midi_list(MidiList* list) {
    MidiList* new_list = midi_list_init();
#ifdef _WIN32
    WIN32_FIND_DATA fd;
    HANDLE hFind = DIR_OPEN(fd);

    if (DIR_VALID(hFind)) {
        do {
            if (!IS_DIR(fd)) { midi_list_add(new_list, DIR_NAME(fd)); }
        }
        while (DIR_NEXT(hFind, fd));

        DIR_CLOSE(hFind);
    }

#else
    DIR* dir = DIR_OPEN(dir);

    if (DIR_VALID(dir)) {
        struct dirent* entry;

        while (DIR_NEXT(dir, entry)) {
            if (strstr(DIR_NAME(entry), ".mid")) { midi_list_add(new_list, DIR_NAME(entry)); }
        }

        DIR_CLOSE(dir);
    }

#endif

    for (int i = 0; i < list->count; i++) { free(list->files[i]); }

    free(list->files);
    list->files = new_list->files;
    list->count = new_list->count;
    list->capacity = new_list->capacity;
    free(new_list);
}

char* find_soundfont() {
#ifdef _WIN32
    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile("*.sf2", &fd);

    if (hFind != INVALID_HANDLE_VALUE) {
        char* sf2 = STRDUP(fd.cFileName);
        FindClose(hFind);
        return sf2;
    }

#else
    DIR* dir = opendir(".");

    if (dir) {
        struct dirent* entry;

        while ((entry = readdir(dir))) {
            char* name = entry->d_name;
            size_t len = strlen(name);

            if (len >= 4 && strcasecmp(name + len - 4, ".sf2") == 0) {
                char* sf2 = STRDUP(name);
                closedir(dir);
                return sf2;
            }
        }

        closedir(dir);
    }

#endif
    return NULL;
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL init error: %s\n", SDL_GetError());
        return 1;
    }

//
    printf("WavePixel v0.1\n\n");
    printf("Author: Ivan Svarkovsky  <https://github.com/Svarkovsky> License: MIT\n");
    printf("A hybrid application merging real-time OpenGL graphics with MIDI audio playback.\n");
//

    int mixer_initialized = 0;

    if (Mix_Init(MIX_INIT_MID) >= 0 && Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) >= 0) {
        mixer_initialized = 1;
        Mix_SetPostMix(audio_effect, NULL);
    }

    else {
        printf("Warning: Mixer init failed (%s), audio disabled\n", Mix_GetError());
    }

    char* soundfont = find_soundfont();

    if (mixer_initialized && soundfont) {
        Mix_SetSoundFonts(soundfont);
        printf("Using SoundFont: %s\n", soundfont);
    }

    else if (mixer_initialized) {
        printf("Warning: No SoundFont (.sf2) found, audio disabled\n");
        Mix_CloseAudio();
        Mix_Quit();
        mixer_initialized = 0;
    }

    if (soundfont) { free(soundfont); }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("WavePixel", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window) {
        fprintf(stderr, "Window creation error: %s\n", SDL_GetError());

        if (mixer_initialized) { Mix_CloseAudio(); Mix_Quit(); }

        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    if (!gl_context) {
        fprintf(stderr, "GL context error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);

        if (mixer_initialized) { Mix_CloseAudio(); Mix_Quit(); }

        SDL_Quit();
        return 1;
    }

    glewExperimental = GL_TRUE;

    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "GLEW init error\n");
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);

        if (mixer_initialized) { Mix_CloseAudio(); Mix_Quit(); }

        SDL_Quit();
        return 1;
    }

    GLData gl_data = {0};

    if (!init_gl(&gl_data)) {
        fprintf(stderr, "OpenGL init failed\n");
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);

        if (mixer_initialized) { Mix_CloseAudio(); Mix_Quit(); }

        SDL_Quit();
        return 1;
    }

    ColorState color_state = {
        .seed = 42, .palette_size = 24, .current_palette = 0, .next_palette = 1,
        .blend_factor = 0.0f, .blend_speed = 0.0f, .blend_enabled = 0, .history = {0, 0, 0}
    };

    MidiList* midi_list = midi_list_init();

    if (mixer_initialized) {
        update_midi_list(midi_list);

        if (midi_list->count == 0) {
            printf("No MIDI files found\n");
        }

        else {
            printf("Found %d MIDI files\n", midi_list->count);
        }
    }

    Mix_Music* music = NULL;
    int current_track = 0;
    int running = 1, fullscreen = 0, parallax_enabled = 0, clouds_enabled = 0;
    float time = 0.0f, avg_frame_time = 16.0f;

    while (running) {
        Uint32 frame_start = SDL_GetTicks();
        SDL_Event e;
        const Uint8* keystate = SDL_GetKeyboardState(NULL);

// Автодобавление .midi (задержка 5 секунд)
        static Uint32 last_update = 0;
        Uint32 current_time = SDL_GetTicks();

        if (mixer_initialized && current_time - last_update >= 5000) {
            int old_count = midi_list->count;
            update_midi_list(midi_list);

            if (midi_list->count > old_count && current_track >= old_count) {
                current_track = old_count; // Перейти к первому новому треку
            }

            last_update = current_time;
        }

//

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = 0; }

            else if (e.type == SDL_KEYDOWN) {
                int ctrl_pressed = (SDL_GetModState() & KMOD_CTRL) != 0;

                switch (e.key.keysym.scancode) {
                    case SDL_SCANCODE_F:
                        fullscreen = !fullscreen;
                        SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                        break;

                    case SDL_SCANCODE_ESCAPE:
                    case SDL_SCANCODE_Q:
                        running = 0;
                        break;

                    case SDL_SCANCODE_P:
                        if (ctrl_pressed) {
                            parallax_enabled = !parallax_enabled;
                            printf("Parallax %s\n", parallax_enabled ? "enabled" : "disabled");
                        }

                        else {
                            color_state.current_palette = (color_state.current_palette + 1) % color_state.palette_size;
                            color_state.next_palette = xorshift32(&color_state.seed) % color_state.palette_size;
                            color_state.blend_factor = 0.0f;
                        }

                        break;

                    case SDL_SCANCODE_O:
                        clouds_enabled = !clouds_enabled;
                        printf("Clouds %s\n", clouds_enabled ? "enabled" : "disabled");
                        break;

                    case SDL_SCANCODE_C:
                        color_state.current_palette = 0;
                        break;

                    case SDL_SCANCODE_S:
                        sun_enabled = !sun_enabled;
                        break;

                    case SDL_SCANCODE_X:
                        color_state.blend_enabled = !color_state.blend_enabled;
                        printf("Blends %s\n", color_state.blend_enabled ? "enabled" : "disabled");
                        break;

                    case SDL_SCANCODE_RIGHT:
                        if (mixer_initialized && midi_list->count > 0) {
                            if (music) { Mix_HaltMusic(); }

                            current_track = (current_track + 1) % midi_list->count;
                            music = Mix_LoadMUS(midi_list->files[current_track]);

                            if (music) {
                                Mix_PlayMusic(music, 1);
                                printf("Playing: %s\n", midi_list->files[current_track]);
                            }

                            else {
                                printf("Failed to load: %s\n", midi_list->files[current_track]);
                            }
                        }

                        break;

                    case SDL_SCANCODE_LEFT:
                        if (mixer_initialized && midi_list->count > 0) {
                            if (music) { Mix_HaltMusic(); }

                            current_track = (current_track - 1 + midi_list->count) % midi_list->count;
                            music = Mix_LoadMUS(midi_list->files[current_track]);

                            if (music) {
                                Mix_PlayMusic(music, 1);
                                printf("Playing: %s\n", midi_list->files[current_track]);
                            }

                            else {
                                printf("Failed to load: %s\n", midi_list->files[current_track]);
                            }
                        }

                        break;

                    default:
                        break;
                }
            }

            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                glViewport(0, 0, e.window.data1, e.window.data2);
            }
        }

        if (mixer_initialized && keystate[SDL_SCANCODE_LEFT] && keystate[SDL_SCANCODE_RIGHT]) {
            if (Mix_PlayingMusic()) {
                if (Mix_PausedMusic()) {
                    Mix_ResumeMusic();
                    printf(" Resumed\n");
                }

                else {
                    Mix_PauseMusic();
                    printf(" Paused\n");
                }
            }

            SDL_Delay(2000); // Задержка 2 секунды
        }

        if (mixer_initialized && midi_list->count > 0 && !Mix_PlayingMusic() && music) {
            Mix_FreeMusic(music);
            music = Mix_LoadMUS(midi_list->files[current_track]);

            if (music) {
                Mix_PlayMusic(music, 1);
                printf("Playing: %s\n", midi_list->files[current_track]);
            }

            current_track = (current_track + 1) % midi_list->count;
        }

        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        time += 0.016f;

        Color final_color = manage_color_state(&color_state, 0.016f);
        glUseProgram(gl_data.shader_program);
        glUniform3f(glGetUniformLocation(gl_data.shader_program, "base_color"), final_color.r, final_color.g, final_color.b);

        Uint32 render_time;
        render_scene(&gl_data, width, height, time, &render_time, parallax_enabled, clouds_enabled);
        SDL_GL_SwapWindow(window);
        stabilize_frame_rate(frame_start, render_time, &avg_frame_time, fullscreen);
    }

    if (music) { Mix_FreeMusic(music); }

    midi_list_free(midi_list);
    glDeleteVertexArrays(1, &gl_data.vao);
    glDeleteBuffers(1, &gl_data.vbo);
    glDeleteProgram(gl_data.shader_program);
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);

    if (mixer_initialized) { Mix_CloseAudio(); Mix_Quit(); }

    SDL_Quit();
    return 0;
}

