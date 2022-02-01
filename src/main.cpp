#include <chrono>
#include <glm/glm.hpp>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <vector>

const auto window_title = "Raytracer";
const auto window_width = 640;
const auto window_height = 400;

const auto samples_per_pixel = 100;
const auto max_depth = 50;

const float infinity = std::numeric_limits<float>::infinity();
const float pi = 3.14159f;

inline float deg_to_rad(float degrees)
{
    return degrees * pi / 180;
}

inline float random_float()
{
    static std::uniform_real_distribution<float> distribution(0.0, 1.0);
    static std::mt19937 generator;
    return distribution(generator);
}

inline float random_float(float min, float max)
{
    return min + (max - min) * random_float();
}

inline glm::vec3 random_vec3()
{
    return glm::vec3(random_float(), random_float(), random_float());
}

inline glm::vec3 random_vec3(float min, float max)
{
    return glm::vec3(random_float(min, max), random_float(min, max), random_float(min, max));
}

inline glm::vec3 random_in_unit_sphere()
{
    while (true)
    {
        auto p = random_vec3(-1, 1);
        if (glm::dot(p, p) >= 1)
        {
            continue;
        }
        return p;
    }
}

inline glm::vec3 random_unit_vector()
{
    return glm::normalize(random_in_unit_sphere());
}

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

class ray
{
public:
    glm::vec3 origin;
    glm::vec3 direction;

    ray() {}
    ray(const glm::vec3 &origin, const glm::vec3 &direction) : origin(origin), direction(direction) {}

    glm::vec3 at(float t) const
    {
        return origin + t * direction;
    }
};

struct hit_record
{
    float distance;
    glm::vec3 point;
    glm::vec3 normal;
    bool front_face;

    inline void set_face_normal(const ray &ray, const glm::vec3 &outward_normal)
    {
        front_face = glm::dot(ray.direction, outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

class hittable
{
public:
    virtual bool hit(const ray &ray, float t_min, float t_max, hit_record &record) const = 0;
};

class world : public hittable
{
public:
    std::vector<std::shared_ptr<hittable>> objects;

    world() {}
    world(std::shared_ptr<hittable> object)
    {
        add(object);
    }

    void clear()
    {
        objects.clear();
    }

    void add(std::shared_ptr<hittable> object)
    {
        objects.push_back(object);
    }

    virtual bool hit(const ray &ray, float t_min, float t_max, hit_record &record) const override
    {
        hit_record temp_record;
        auto hit_anything = false;
        auto closest_so_far = t_max;

        for (const auto &object : objects)
        {
            if (object->hit(ray, t_min, closest_so_far, temp_record))
            {
                hit_anything = true;
                closest_so_far = temp_record.distance;
                record = temp_record;
            }
        }

        return hit_anything;
    }
};

class sphere : public hittable
{
public:
    glm::vec3 center;
    float radius;

    sphere() {}
    sphere(const glm::vec3 &center, float radius) : center(center), radius(radius) {}

    virtual bool hit(const ray &ray, float t_min, float t_max, hit_record &record) const override
    {
        auto oc = ray.origin - center;
        auto a = glm::dot(ray.direction, ray.direction);
        auto half_b = glm::dot(oc, ray.direction);
        auto c = glm::dot(oc, oc) - powf(radius, 2);

        auto discriminant = powf(half_b, 2) - a * c;
        if (discriminant < 0)
            return false;
        auto sqrtd = sqrtf(discriminant);

        auto root = (-half_b - sqrtd) / a;
        if (root < t_min || t_max < root)
        {
            root = (-half_b + sqrtd) / a;
            if (root < t_min || t_max < root)
                return false;
        }

        record.distance = root;
        record.point = ray.at(record.distance);
        record.set_face_normal(ray, (record.point - center) / radius);

        return true;
    };
};

class camera
{
public:
    camera()
    {
        auto aspect_ratio = (float)window_width / (float)window_height;

        auto viewport_height = 2.0f;
        auto viewport_width = aspect_ratio * viewport_height;
        auto focal_length = 1.0f;

        origin = glm::vec3(0, 0, 0);
        horizontal = glm::vec3(viewport_width, 0, 0);
        vertical = glm::vec3(0, viewport_height, 0);
        lower_left = origin - horizontal * 0.5f - vertical * 0.5f - glm::vec3(0, 0, focal_length);
    }

    ray get_ray(float u, float v) const
    {
        return ray(origin, lower_left + u * horizontal + v * vertical - origin);
    }

private:
    glm::vec3 origin;
    glm::vec3 horizontal;
    glm::vec3 vertical;
    glm::vec3 lower_left;
};

glm::vec3 raytrace(const ray &r, const hittable &world, int depth)
{
    if (depth <= 0)
    {
        return glm::vec3(0, 0, 0);
    }

    hit_record record;
    if (world.hit(r, 0.001f, infinity, record))
    {
        auto target = record.point + record.normal + random_unit_vector();
        return 0.5f * raytrace(ray(record.point, target - record.point), world, depth - 1);
    }

    auto unit_direction = glm::normalize(r.direction);
    auto t = 0.5f * (unit_direction.y + 1);
    return (1.0f - t) * glm::vec3(1, 1, 1) + t * glm::vec3(0.5f, 0.7f, 1.0f);
}

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    auto window = SDL_CreateWindow(
        window_title,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        window_width,
        window_height,
        0);

    auto renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_SOFTWARE);

    auto screen = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        window_width,
        window_height);

    auto pixels = new pixel[window_width * window_height];

    world world;
    world.add(std::make_shared<sphere>(glm::vec3(0, 0, -1), 0.5f));
    world.add(std::make_shared<sphere>(glm::vec3(0.0f, -100.5f, -1), 100.0f));

    camera camera;

    auto start = std::chrono::high_resolution_clock::now();
    for (auto y = 0; y < window_height; y++)
    {
        std::cout << "Scanlines remaining: " << window_height - y << std::endl;
        for (auto x = 0; x < window_width; x++)
        {
            auto color = glm::vec3(0, 0, 0);
            for (int s = 0; s < samples_per_pixel; s++)
            {
                auto u = (float)(x + random_float()) / (window_width - 1);
                auto v = (float)(window_height - (y + random_float())) / (window_height - 1);
                auto ray = camera.get_ray(u, v);
                color += raytrace(ray, world, max_depth);
            }

            auto scale = 1.0f / samples_per_pixel;
            color.r = sqrtf(color.r * scale);
            color.g = sqrtf(color.g * scale);
            color.b = sqrtf(color.b * scale);

            pixels[x + y * window_width].r = (uint8_t)(0xff * glm::clamp(color.x, 0.0f, 0.999f));
            pixels[x + y * window_width].g = (uint8_t)(0xff * glm::clamp(color.y, 0.0f, 0.999f));
            pixels[x + y * window_width].b = (uint8_t)(0xff * glm::clamp(color.z, 0.0f, 0.999f));
        }
    }
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    std::cout << "Done in " << duration.count() << "ms" << std::endl;

    auto quit = false;
    while (!quit)
    {
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

        SDL_RenderClear(renderer);
        SDL_UpdateTexture(
            screen,
            NULL,
            pixels,
            window_width * sizeof(pixel));
        SDL_RenderCopy(renderer, screen, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(screen);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    IMG_Quit();
    SDL_Quit();

    return 0;
}
