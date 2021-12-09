#include <float.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define WINDOW_TITLE "Raytracer"
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 400

union pixel
{
    uint32_t color;
    struct
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };
};

struct vec3
{
    float x;
    float y;
    float z;
};

float vec3_length(struct vec3 v)
{
    return sqrtf(powf(v.x, 2) + powf(v.y, 2) + powf(v.z, 2));
}

struct vec3 vec3_normalize(struct vec3 v)
{
    float length = vec3_length(v);
    struct vec3 normalized;
    normalized.x = v.x / length;
    normalized.y = v.y / length;
    normalized.z = v.z / length;
    return normalized;
}

float vec3_dot(struct vec3 a, struct vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

struct vec3 vec3_inverse(struct vec3 v)
{
    struct vec3 inverted;
    inverted.x = -v.x;
    inverted.y = -v.y;
    inverted.z = -v.z;
    return inverted;
}

struct vec3 vec3_reflect(struct vec3 I, struct vec3 N)
{
    struct vec3 reflected;
    float dot = vec3_dot(I, N);
    reflected.x = I.x - N.x * 2 * dot;
    reflected.y = I.y - N.y * 2 * dot;
    reflected.z = I.z - N.z * 2 * dot;
    return reflected;
}

struct vec3 vec3_refract(struct vec3 I, struct vec3 N, float eta_t, float eta_i)
{
    float cosi = -max(-1, min(1, vec3_dot(I, N)));
    if (cosi < 0)
    {
        return vec3_refract(I, vec3_inverse(N), eta_i, eta_t);
    }
    float eta = eta_i / eta_t;
    float k = 1 - powf(eta, 2) * (1 - powf(cosi, 2));
    struct vec3 refracted;
    refracted.x = k < 0 ? 1 : I.x * eta + N.x * (eta * cosi - sqrtf(k));
    refracted.y = k < 0 ? 0 : I.y * eta + N.y * (eta * cosi - sqrtf(k));
    refracted.z = k < 0 ? 0 : I.z * eta + N.z * (eta * cosi - sqrtf(k));
    return refracted;
}

struct vec4
{
    float x;
    float y;
    float z;
    float w;
};

struct material
{
    struct vec4 albedo;
    struct vec3 diffuse;
    float specular_exponent;
    float refractive_index;
};

struct sphere
{
    struct vec3 center;
    float radius;
    struct material *material;
};

bool sphere_ray_intersect(struct sphere *sphere, struct vec3 origin, struct vec3 direction, float *t0)
{
    struct vec3 L;
    L.x = sphere->center.x - origin.x;
    L.y = sphere->center.y - origin.y;
    L.z = sphere->center.z - origin.z;

    float tca = vec3_dot(L, direction);
    float d2 = vec3_dot(L, L) - powf(tca, 2);

    if (d2 > powf(sphere->radius, 2))
    {
        return false;
    }

    float thc = sqrtf(powf(sphere->radius, 2) - d2);

    *t0 = tca - thc;
    float t1 = tca + thc;
    if (*t0 < 0)
    {
        *t0 = t1;
    }
    if (*t0 < 0)
    {
        return false;
    }

    return true;
}

struct light
{
    struct vec3 position;
    float intensity;
};

bool scene_intersect(struct vec3 origin, struct vec3 direction, struct sphere **spheres, unsigned int num_spheres, struct vec3 *hit, struct vec3 *N, struct material **material)
{
    float spheres_dist = FLT_MAX;
    for (unsigned int i = 0; i < num_spheres; i++)
    {
        float dist_i;
        if (sphere_ray_intersect(spheres[i], origin, direction, &dist_i) && dist_i < spheres_dist)
        {
            spheres_dist = dist_i;
            hit->x = origin.x + (direction.x * dist_i);
            hit->y = origin.y + (direction.y * dist_i);
            hit->z = origin.z + (direction.z * dist_i);
            N->x = hit->x - spheres[i]->center.x;
            N->y = hit->y - spheres[i]->center.y;
            N->z = hit->z - spheres[i]->center.z;
            *N = vec3_normalize(*N);
            *material = spheres[i]->material;
        }
    }
    return spheres_dist < 1000;
}

struct vec3 cast_ray(struct vec3 origin, struct vec3 direction, struct sphere **spheres, unsigned int num_spheres, struct light **lights, unsigned int num_lights, unsigned int depth)
{
    struct vec3 color;

    struct vec3 point;
    struct vec3 N;
    struct material *material;

    if (depth <= 4 && scene_intersect(origin, direction, spheres, num_spheres, &point, &N, &material))
    {
        struct vec3 reflect_direction = vec3_normalize(vec3_reflect(direction, N));
        struct vec3 reflect_origin;
        reflect_origin.x = vec3_dot(reflect_direction, N) < 0
                               ? point.x - N.x * 1e-3f
                               : point.x + N.x * 1e-3f;
        reflect_origin.y = vec3_dot(reflect_direction, N) < 0
                               ? point.y - N.y * 1e-3f
                               : point.y + N.y * 1e-3f;
        reflect_origin.z = vec3_dot(reflect_direction, N) < 0
                               ? point.z - N.z * 1e-3f
                               : point.z + N.z * 1e-3f;
        struct vec3 reflect_color = cast_ray(reflect_origin, reflect_direction, spheres, num_spheres, lights, num_lights, depth + 1);

        struct vec3 refract_direction = vec3_normalize(vec3_refract(direction, N, material->refractive_index, 1));
        struct vec3 refract_origin;
        refract_origin.x = vec3_dot(refract_direction, N) < 0
                               ? point.x - N.x * 1e-3f
                               : point.x + N.x * 1e-3f;
        refract_origin.y = vec3_dot(refract_direction, N) < 0
                               ? point.y - N.y * 1e-3f
                               : point.y + N.y * 1e-3f;
        refract_origin.z = vec3_dot(refract_direction, N) < 0
                               ? point.z - N.z * 1e-3f
                               : point.z + N.z * 1e-3f;
        struct vec3 refract_color = cast_ray(refract_origin, refract_direction, spheres, num_spheres, lights, num_lights, depth + 1);

        float diffuse_light_intensity = 0;
        float specular_light_intensity = 0;
        for (unsigned int i = 0; i < num_lights; i++)
        {
            struct vec3 light_direction;
            light_direction.x = lights[i]->position.x - point.x;
            light_direction.y = lights[i]->position.y - point.y;
            light_direction.z = lights[i]->position.z - point.z;
            float light_distance = vec3_length(light_direction);
            light_direction = vec3_normalize(light_direction);

            struct vec3 shadow_origin;
            shadow_origin.x = vec3_dot(light_direction, N) < 0
                                  ? point.x - N.x * 1e-3f
                                  : point.x + N.x * 1e-3f;
            shadow_origin.y = vec3_dot(light_direction, N) < 0
                                  ? point.y - N.y * 1e-3f
                                  : point.y + N.y * 1e-3f;
            shadow_origin.z = vec3_dot(light_direction, N) < 0
                                  ? point.z - N.z * 1e-3f
                                  : point.z + N.z * 1e-3f;
            struct vec3 shadow_point;
            struct vec3 shadow_N;
            struct material *shadow_material;
            if (scene_intersect(shadow_origin, light_direction, spheres, num_spheres, &shadow_point, &shadow_N, &shadow_material))
            {
                struct vec3 shadow_diff;
                shadow_diff.x = shadow_point.x - shadow_origin.x;
                shadow_diff.y = shadow_point.y - shadow_origin.y;
                shadow_diff.z = shadow_point.z - shadow_origin.z;
                if (vec3_length(shadow_diff) < light_distance)
                {
                    continue;
                }
            }

            diffuse_light_intensity += max(
                                           0,
                                           vec3_dot(
                                               light_direction,
                                               N)) *
                                       lights[i]->intensity;
            specular_light_intensity += powf(
                                            max(
                                                0,
                                                vec3_dot(
                                                    vec3_reflect(
                                                        light_direction,
                                                        N),
                                                    direction)),
                                            material->specular_exponent) *
                                        lights[i]->intensity;
        }

        color.x = material->diffuse.x * diffuse_light_intensity * material->albedo.x +
                  specular_light_intensity * material->albedo.y +
                  reflect_color.x * material->albedo.z +
                  refract_color.x * material->albedo.w;
        color.y = material->diffuse.y * diffuse_light_intensity * material->albedo.x +
                  specular_light_intensity * material->albedo.y +
                  reflect_color.y * material->albedo.z +
                  refract_color.y * material->albedo.w;
        color.z = material->diffuse.z * diffuse_light_intensity * material->albedo.x +
                  specular_light_intensity * material->albedo.y +
                  reflect_color.z * material->albedo.z +
                  refract_color.z * material->albedo.w;
    }
    else
    {
        color.x = 0.2f;
        color.y = 0.7f;
        color.z = 0.8f;
    }

    return color;
}

int main(int argc, char *argv[])
{
    // init SDL
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    // create window
    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        0);

    // create renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_SOFTWARE);

    // create screen texture
    SDL_Texture *screen = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        WINDOW_WIDTH,
        WINDOW_HEIGHT);

    // render buffers
    union pixel *pixel_buffer = malloc(WINDOW_WIDTH * WINDOW_HEIGHT * sizeof(union pixel));

    const float pi = 3.14159f;
    const float fov = pi / 3;

    struct material ivory;
    ivory.albedo.x = 0.6f;
    ivory.albedo.y = 0.3f;
    ivory.albedo.z = 0.1f;
    ivory.albedo.w = 0;
    ivory.diffuse.x = 0.4f;
    ivory.diffuse.y = 0.4f;
    ivory.diffuse.z = 0.3f;
    ivory.specular_exponent = 50;
    ivory.refractive_index = 1;

    struct material glass;
    glass.albedo.x = 0;
    glass.albedo.y = 0.5f;
    glass.albedo.z = 0.1f;
    glass.albedo.w = 0.8f;
    glass.diffuse.x = 0.6f;
    glass.diffuse.y = 0.7f;
    glass.diffuse.z = 0.8f;
    glass.specular_exponent = 125;
    glass.refractive_index = 1.5f;

    struct material rubber;
    rubber.albedo.x = 0.9f;
    rubber.albedo.y = 0.1f;
    rubber.albedo.z = 0;
    rubber.albedo.w = 0;
    rubber.diffuse.x = 0.3f;
    rubber.diffuse.y = 0.1f;
    rubber.diffuse.z = 0.1f;
    rubber.specular_exponent = 10;
    rubber.refractive_index = 1;

    struct material mirror;
    mirror.albedo.x = 0;
    mirror.albedo.y = 10;
    mirror.albedo.z = 0.8f;
    mirror.albedo.w = 0;
    mirror.diffuse.x = 1;
    mirror.diffuse.y = 1;
    mirror.diffuse.z = 1;
    mirror.specular_exponent = 1425;
    mirror.refractive_index = 1;

    struct sphere sphere1;
    sphere1.center.x = -3;
    sphere1.center.y = 0;
    sphere1.center.z = -16;
    sphere1.radius = 2;
    sphere1.material = &ivory;

    struct sphere sphere2;
    sphere2.center.x = -1;
    sphere2.center.y = -1.5f;
    sphere2.center.z = -12;
    sphere2.radius = 2;
    sphere2.material = &mirror;

    struct sphere sphere3;
    sphere3.center.x = 1.5f;
    sphere3.center.y = -0.5f;
    sphere3.center.z = -18;
    sphere3.radius = 3;
    sphere3.material = &rubber;

    struct sphere sphere4;
    sphere4.center.x = 7.0;
    sphere4.center.y = 5;
    sphere4.center.z = -18;
    sphere4.radius = 4;
    sphere4.material = &mirror;

    struct sphere *spheres[] = {
        &sphere1,
        &sphere2,
        &sphere3,
        &sphere4};

    struct light light1;
    light1.position.x = -20;
    light1.position.y = 20;
    light1.position.z = 20;
    light1.intensity = 1.5f;

    struct light light2;
    light2.position.x = 30;
    light2.position.y = 50;
    light2.position.z = -25;
    light2.intensity = 1.8f;

    struct light light3;
    light3.position.x = 30;
    light3.position.y = 20;
    light3.position.z = 30;
    light3.intensity = 1.7f;

    struct light *lights[] = {
        &light1,
        &light2,
        &light3};

    struct vec3 origin;
    origin.x = 0;
    origin.y = 0;
    origin.z = 0;

    for (int y = 0; y < WINDOW_HEIGHT; y++)
    {
        for (int x = 0; x < WINDOW_WIDTH; x++)
        {
            struct vec3 direction;
            direction.x = ((float)x + 0.5f) - (float)WINDOW_WIDTH / 2;
            direction.y = -((float)y + 0.5f) + (float)WINDOW_HEIGHT / 2;
            direction.z = -(float)WINDOW_HEIGHT / (2 * tanf(fov / 2));
            direction = vec3_normalize(direction);

            struct vec3 color = cast_ray(origin, direction, spheres, sizeof(spheres) / sizeof(spheres[0]), lights, sizeof(lights) / sizeof(lights[0]), 0);

            float color_max = max(color.x, max(color.y, color.z));
            if (color_max > 1)
            {
                color.x /= color_max;
                color.y /= color_max;
                color.z /= color_max;
            }

            pixel_buffer[x + y * WINDOW_WIDTH].r = (uint8_t)(0xff * color.x);
            pixel_buffer[x + y * WINDOW_WIDTH].g = (uint8_t)(0xff * color.y);
            pixel_buffer[x + y * WINDOW_WIDTH].b = (uint8_t)(0xff * color.z);
        }
    }

    // main loop
    bool quit = false;
    while (!quit)
    {
        // handle events
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
            {
                quit = true;
            }
            break;
            }
        }

        sphere1.center.y = sinf(SDL_GetTicks() / 1000.0f);

        // clear the renderer
        SDL_RenderClear(renderer);

        // display pixel buffer
        SDL_UpdateTexture(
            screen,
            NULL,
            pixel_buffer,
            WINDOW_WIDTH * sizeof(unsigned int));
        SDL_RenderCopy(renderer, screen, NULL, NULL);

        // display the renderer
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(screen);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    IMG_Quit();
    SDL_Quit();

    return 0;
}
