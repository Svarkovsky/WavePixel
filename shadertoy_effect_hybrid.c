/*

    gcc -std=c17 -o shadertoy_effect_hybrid shadertoy_effect_hybrid.c -lGLEW -lGL -lGLU -lSDL2 -lm \
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
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define WINDOW_WIDTH  960
#define WINDOW_HEIGHT 540
#ifndef M_PI
    #define M_PI acos(-1.0)
#endif

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
        if (index == 0) return (HSV){0.0f, 0.0f, 0.0f};
        uint32_t seed = cs->seed + index * 12345;
        float h = (float)(xorshift32(&seed) % 360);
        float s = 0.7f + (float)(xorshift32(&seed) % 30) / 100.0f;
        float v = 0.7f + (float)(xorshift32(&seed) % 30) / 100.0f;
        return (HSV){h, s, v};
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
            case 0: c.r = v; c.g = t; c.b = p; break;
            case 1: c.r = q; c.g = v; c.b = p; break;
            case 2: c.r = p; c.g = v; c.b = t; break;
            case 3: c.r = p; c.g = q; c.b = v; break;
            case 4: c.r = t; c.g = p; c.b = v; break;
            case 5: c.r = v; c.g = p; c.b = q; break;
        }
        return c;
    }

    void select_next_color() {
        int next, attempts = 0;
        do {
            next = xorshift32(&cs->seed) % cs->palette_size;
            attempts++;
            if (attempts > 10) break;
        } while (next == cs->current_palette || next == cs->history[0] || next == cs->history[1]);
        cs->history[1] = cs->history[0];
        cs->history[0] = cs->current_palette;
        cs->next_palette = next;
    }

    HSV lerp_hsv(HSV a, HSV b, float t) {
        float smooth_t = (1.0f - cosf(t * M_PI)) * 0.5f;
        float h_diff = b.h - a.h;
        if (h_diff > 180.0f) h_diff -= 360.0f;
        if (h_diff < -180.0f) h_diff += 360.0f;
        float h = a.h + h_diff * smooth_t;
        if (h < 0.0f) h += 360.0f;
        if (h >= 360.0f) h -= 360.0f;
        float s = a.s + (b.s - a.s) * smooth_t;
        float v = a.v + (b.v - a.v) * smooth_t;
        return (HSV){h, s, v};
    }

    // Динамический blend_speed
    float calculate_blend_speed(HSV current, HSV next) {
        float h_diff = fabsf(next.h - current.h);
        if (h_diff > 180.0f) h_diff = 360.0f - h_diff; // Кратчайший путь по кругу
        float s_diff = fabsf(next.s - current.s);
        float v_diff = fabsf(next.v - current.v);
        float total_diff = h_diff / 360.0f + s_diff + v_diff; // Нормализуем Hue
        // Чем больше разница, тем медленнее переход (от 0.01 до 0.05)
        return 0.05f - (total_diff * 0.04f); // total_diff от 0 до ~1.5, speed от 0.05 до 0.01
 //       return 0.05f - (total_diff * 0.045f);
 //       return 0.0125f - (total_diff * 0.01f); // Диапазон 0.0125–0.0025
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
    "uniform int clouds_enabled;\n" // Новая опция для облаков
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
    "float value_noise(vec2 p) {\n" // Простой value noise
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
    "    vec2 cloud_uv = uv + vec2(time * 0.1, 0.0);\n" // Движение без изменений
    "    float noise = value_noise(cloud_uv * 100.0);\n" // Увеличиваем масштаб шума (меньше облака)
    "    return smoothstep(0.985, 0.999, noise);\n" // Увеличиваем порог (реже облака)
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
    "        float parallax_scale = 0.15;\n"  // 0.05
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
    if (!vertex_shader) return 0;
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
    printf("Initializing OpenGL...\n");
    gl->shader_program = create_shader_program(vertex_shader_src, fragment_shader_src);
    if (!gl->shader_program) {
        fprintf(stderr, "Failed to create shader program\n");
        return 0;
    }
    float vertices[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    glGenVertexArrays(1, &gl->vao);
    glGenBuffers(1, &gl->vbo);
    glBindVertexArray(gl->vao);
    glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    GLint pos_attrib = glGetAttribLocation(gl->shader_program, "position");
    if (pos_attrib == -1) {
        fprintf(stderr, "Failed to get attribute location for 'position'\n");
        return 0;
    }
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    printf("OpenGL initialized successfully\n");
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
    glUniform1i(glGetUniformLocation(gl->shader_program, "clouds_enabled"), clouds_enabled); // Новая uniform
    glBindVertexArray(gl->vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    int has_arb_sync = glewIsSupported("GL_ARB_sync");
    if (use_arb_sync == -1) use_arb_sync = has_arb_sync;
    if (use_arb_sync && has_arb_sync) {
        Uint32 sync_start = SDL_GetTicks();
        GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        GLenum result = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 60000000);
        glDeleteSync(sync);
        Uint32 sync_time = SDL_GetTicks() - sync_start;
        if (first_call) { printf("Using GL_ARB_sync (wait: %u ms)\n", sync_time); first_call = 0; }
        if (result == GL_TIMEOUT_EXPIRED) { 
            fprintf(stderr, "GL_ARB_sync timeout (%u ms), switching to glFinish()\n", sync_time); 
            use_arb_sync = 0; 
        }
    } else {
        glFinish();
        if (first_call) { printf("Using glFinish() (%s)\n", has_arb_sync ? "timeout" : "no GL_ARB_sync"); first_call = 0; }
    }
    *render_time = SDL_GetTicks() - start;
}

void stabilize_frame_rate(Uint32 frame_start, Uint32 render_time, float* avg_frame_time, int fullscreen) {
    const float alpha = 0.05f;
    *avg_frame_time = (1.0f - alpha) * (*avg_frame_time) + alpha * render_time;
    float target_fps = fullscreen ? 40.0f : 60.0f;
    if (*avg_frame_time > 33.3f) target_fps = fullscreen ? 30.0f : 45.0f;
    Uint32 frame_time = SDL_GetTicks() - frame_start;
    if (frame_time < 1000.0f / target_fps) SDL_Delay((Uint32)(1000.0f / target_fps - frame_time));
}

void display_frame_info(Uint32 frame_start, Uint32* last_time, int* frame_count, float* fps, Uint32 render_time) {
    *frame_count += 1;
    if (frame_start - *last_time >= 1000) {
        *fps = *frame_count * 1000.0f / (frame_start - *last_time);
        float gpu_load = render_time / (1000.0f / *fps) * 100.0f;
        printf("FPS: %.1f | GPU: %.1f%% (%u ms)\n", *fps, gpu_load > 100.0f ? 100.0f : gpu_load, render_time);
        *frame_count = 0;
        *last_time = frame_start;
    }
}

int main(int argc, char* argv[]) {
    printf("Starting program...\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL init error: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL initialized\n");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("Smooth Color Transitions", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "Window creation error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    printf("Window created\n");

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        fprintf(stderr, "GL context error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("GL context created\n");

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "GLEW init error\n");
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("GLEW initialized\n");

    GLData gl_data = {0};
    if (!init_gl(&gl_data)) {
        fprintf(stderr, "OpenGL initialization failed\n");
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

// Определяет количество уникальных цветов в "палитре" (от 0 до 23). 
// Ограничивает выбор current_palette и next_palette, влияя на разнообразие переходов. Больше значение — больше вариантов цветов, меньше — меньше.
 
ColorState color_state = {
        .seed = 42,
        .palette_size = 24, // 24
        .current_palette = 0,
        .next_palette = 0,
        .blend_factor = 0.0f,
        .blend_speed = 0.0f, // Не используется напрямую, теперь динамический
        .blend_enabled = 0,
        .history = {0, 0, 0}
    };
    color_state.next_palette = xorshift32(&color_state.seed) % color_state.palette_size;
   
    int clouds_enabled = 0; // По умолчанию облака выключены
    int parallax_enabled = 0; // По умолчанию параллакс выключен
    int running = 1, fullscreen = 0;
    float time = 0.0f, avg_frame_time = 16.0f, fps = 0.0f;
    Uint32 last_time = SDL_GetTicks(), frame_count = 0, render_time = 0;

   while (running) {
        Uint32 frame_start = SDL_GetTicks();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_KEYDOWN) {
                int ctrl_pressed = (SDL_GetModState() & KMOD_CTRL) != 0;
                switch (e.key.keysym.scancode) {
                    case SDL_SCANCODE_F: 
                        fullscreen = !fullscreen; 
                        SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0); 
                        break;
                    case SDL_SCANCODE_ESCAPE: case SDL_SCANCODE_Q: running = 0; break;
                    case SDL_SCANCODE_P: 
                        if (ctrl_pressed) {
                            parallax_enabled = !parallax_enabled;
                            printf("Parallax %s\n", parallax_enabled ? "enabled" : "disabled");
                        } else {
                            color_state.current_palette = (color_state.current_palette + 1) % color_state.palette_size;
                            color_state.next_palette = xorshift32(&color_state.seed) % color_state.palette_size;
                            color_state.blend_factor = 0.0f;
                        }
                        break;
                    case SDL_SCANCODE_O: // Включение/выключение облаков
                        clouds_enabled = !clouds_enabled;
                        printf("Clouds %s\n", clouds_enabled ? "enabled" : "disabled");
                        break;
                    case SDL_SCANCODE_C: color_state.current_palette = 0; break;
        
	            case SDL_SCANCODE_S: sun_enabled = !sun_enabled; break;
			
                    case SDL_SCANCODE_X:
                        color_state.blend_enabled = !color_state.blend_enabled;
                        if (color_state.blend_enabled) {
                            color_state.seed = SDL_GetTicks();
                            color_state.next_palette = xorshift32(&color_state.seed) % color_state.palette_size;
                            color_state.blend_factor = 0.0f;
                        }
			printf("Blends %s\n", color_state.blend_enabled ? "enabled" : "disabled");
                        break;
                    default: break;
                }
            } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                glViewport(0, 0, e.window.data1, e.window.data2);
            }
        }

        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        time += 0.016f;

        Color final_color = manage_color_state(&color_state, 0.016f);

        glUseProgram(gl_data.shader_program);
        GLint loc = glGetUniformLocation(gl_data.shader_program, "base_color");
        if (loc == -1) {
            fprintf(stderr, "Failed to get uniform location for 'base_color'\n");
            running = 0;
        } else {
            glUniform3f(loc, final_color.r, final_color.g, final_color.b);
        }

        render_scene(&gl_data, width, height, time, &render_time, parallax_enabled, clouds_enabled); // Передаём clouds_enabled

        SDL_GL_SwapWindow(window);
        stabilize_frame_rate(frame_start, render_time, &avg_frame_time, fullscreen);
        display_frame_info(frame_start, &last_time, &frame_count, &fps, render_time);
    }

    glDeleteVertexArrays(1, &gl_data.vao);
    glDeleteBuffers(1, &gl_data.vbo);
    glDeleteProgram(gl_data.shader_program);
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("Program terminated\n");
    return 0;
}

