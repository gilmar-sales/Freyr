#include <Freyr/Freyr.hpp>
#include <benchmark/benchmark.h>

struct Position : fr::Component
{
    float x;
    float y;
    float z;
};

struct Velocity : fr::Component
{
    float vx;
    float vy;
    float vz;
};

struct Acceleration : fr::Component
{
    float ax;
    float ay;
    float az;
};

class MovementSystem : public fr::System
{
  public:
    explicit MovementSystem(const Ref<fr::Scene>& scene) : System(scene) {}

    void Update(float dt) override
    {
        mScene->ForEachAsync<Acceleration>([dt](auto entity, Acceleration& acceleration) {
            acceleration.ax += dt;
            acceleration.ay += dt;
            acceleration.az += dt;
        });

        mScene->ForEachAsync<Acceleration>([dt](auto entity, Acceleration& acceleration) {
            acceleration.ax -= dt;
            acceleration.ay -= dt;
            acceleration.az -= dt;
        });

        mScene->ForEachAsync<Acceleration>([dt](auto entity, Acceleration& acceleration) {
            acceleration.ax += dt;
            acceleration.ay += dt;
            acceleration.az += dt;
        });
        mScene->ForEachAsync<Acceleration>([dt](auto entity, Acceleration& acceleration) {
            acceleration.ax += dt;
            acceleration.ay += dt;
            acceleration.az += dt;
        });

        mScene->ForEachAsync<Acceleration>([dt](auto entity, Acceleration& acceleration) {
            acceleration.ax -= dt;
            acceleration.ay -= dt;
            acceleration.az -= dt;
        });

        mScene->ForEachAsync<Acceleration>([dt](auto entity, Acceleration& acceleration) {
            acceleration.ax += dt;
            acceleration.ay += dt;
            acceleration.az += dt;
        });

        mScene->ForEachAsync<Velocity, Acceleration>(
            [dt](auto entity, Velocity& velocity, const Acceleration& acceleration) {
                velocity.vx += acceleration.ax * dt;
                velocity.vy += acceleration.ay * dt;
                velocity.vz += acceleration.az * dt;
            });

        mScene->ForEachAsync<Position, Velocity>([dt](auto entity, Position& position, const Velocity& velocity) {
            position.x += velocity.vx * dt;
            position.y += velocity.vy * dt;
            position.z += velocity.vz * dt;
        });
    }
};

class App : public skr::IApplication
{
  public:
    explicit App(const Ref<skr::ServiceProvider>& rootServiceProvider) : IApplication(rootServiceProvider) {}

    void Run() override {}
};

static void DefaultStrategy(benchmark::State& state)
{
    auto app =
        skr::ApplicationBuilder()
            .AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                freyr
                    .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                        builder.WithArchetypeChunkCapacity(512).WithThreadCount(std::thread::hardware_concurrency());
                    })
                    .AddComponent<Position>()
                    .AddComponent<Velocity>()
                    .AddComponent<Acceleration>()
                    .AddSystem<MovementSystem>();
            })
            .Build<App>();

    auto scene = app->GetRootServiceProvider().GetService<fr::Scene>();

    const auto entityCount = state.range(0);
    for (int i = 0; i < entityCount; ++i)
    {
        scene->CreateEntity(
            Position { .x = static_cast<float>(i), .y = static_cast<float>(i), .z = static_cast<float>(i) },
            Velocity { .vx = 1.0f, .vy = 1.0f, .vz = 1.0f },
            Acceleration { .ax = 1.0f, .ay = 1.0f, .az = 1.0f });
    }

    const std::int64_t bytesProcessed =
        entityCount * static_cast<std::int64_t>(sizeof(Position) + sizeof(Velocity) + sizeof(Acceleration));
    for (auto _ : state)
    {
        scene->Update(0.16f);
        state.SetBytesProcessed(bytesProcessed);
    }
}

static void ChunkAffinity(benchmark::State& state)
{
    auto app = skr::ApplicationBuilder()
                   .AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr
                           .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                               builder.WithArchetypeChunkCapacity(512)
                                   .WithThreadCount(std::thread::hardware_concurrency())
                                   .WithExecutionStrategy(fr::FreyrExecutionStategy::ChunkAffinity);
                           })
                           .AddComponent<Position>()
                           .AddComponent<Velocity>()
                           .AddComponent<Acceleration>()
                           .AddSystem<MovementSystem>();
                   })
                   .Build<App>();

    auto scene = app->GetRootServiceProvider().GetService<fr::Scene>();

    const auto entityCount = state.range(0);
    for (int i = 0; i < entityCount; ++i)
    {
        scene->CreateEntity(
            Position { .x = static_cast<float>(i), .y = static_cast<float>(i), .z = static_cast<float>(i) },
            Velocity { .vx = 1.0f, .vy = 1.0f, .vz = 1.0f },
            Acceleration { .ax = 1.0f, .ay = 1.0f, .az = 1.0f });
    }

    const std::int64_t bytesProcessed =
        entityCount * static_cast<std::int64_t>(sizeof(Position) + sizeof(Velocity) + sizeof(Acceleration));
    for (auto _ : state)
    {
        scene->Update(0.16f);
        state.SetBytesProcessed(bytesProcessed);
    }
}

BENCHMARK(ChunkAffinity)->RangeMultiplier(2)->Range(100'000, 1'000'000)->Unit(benchmark::kMillisecond);
BENCHMARK(DefaultStrategy)->RangeMultiplier(2)->Range(100'000, 1'000'000)->Unit(benchmark::kMillisecond);
BENCHMARK_MAIN();