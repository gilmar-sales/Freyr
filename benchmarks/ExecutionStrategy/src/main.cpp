#include <Freyr/Freyr.hpp>
#include <benchmark/benchmark.h>
#include <thread>

struct Position : fr::Component
{
    float x, y, z;
};

struct Velocity : fr::Component
{
    float vx, vy, vz;
};

struct Acceleration : fr::Component
{
    float ax, ay, az;
};

struct Mass : fr::Component
{
    float value;
};

struct Drag : fr::Component
{
    float coefficient;
};

struct Lifetime : fr::Component
{
    float remaining;
};

struct Collidable : fr::Component
{
    float restitution;
};

struct Temperature : fr::Component
{
    float value;
    float dissipationRate;
};

class GravitySystem : public fr::System
{
  public:
    explicit GravitySystem(const Ref<fr::Scene>& scene) : System(scene) {}

    void Update(float dt) override
    {
        mScene->ForEachAsync<Acceleration, Mass>([](auto, Acceleration& acc, const Mass& mass) {
            acc.ax = 0.0f;
            acc.ay = -9.8f * mass.value;
            acc.az = 0.0f;
        });
    }
};

class DragSystem : public fr::System
{
  public:
    explicit DragSystem(const Ref<fr::Scene>& scene) : System(scene) {}

    void Update(float dt) override
    {
        mScene->ForEachAsync<Acceleration, Velocity, Drag>(
            [](auto, Acceleration& acc, const Velocity& vel, const Drag& drag) {
                acc.ax -= drag.coefficient * vel.vx;
                acc.ay -= drag.coefficient * vel.vy;
                acc.az -= drag.coefficient * vel.vz;
            });
    }
};

class IntegrationSystem : public fr::System
{
  public:
    explicit IntegrationSystem(const Ref<fr::Scene>& scene) : System(scene) {}

    void Update(float dt) override
    {
        mScene->ForEachAsync<Velocity, Acceleration, Mass>(
            [dt](auto, Velocity& vel, const Acceleration& acc, const Mass& mass) {
                const float invMass = mass.value > 0.0f ? 1.0f / mass.value : 0.0f;
                vel.vx += acc.ax * invMass * dt;
                vel.vy += acc.ay * invMass * dt;
                vel.vz += acc.az * invMass * dt;
            });

        mScene->ForEachAsync<Position, Velocity>([dt](auto, Position& pos, const Velocity& vel) {
            pos.x += vel.vx * dt;
            pos.y += vel.vy * dt;
            pos.z += vel.vz * dt;
        });
    }
};

class CollisionSystem : public fr::System
{
  public:
    explicit CollisionSystem(const Ref<fr::Scene>& scene) : System(scene) {}

    void Update(float dt) override
    {
        mScene->ForEachAsync<Position, Velocity, Collidable>(
            [](auto, Position& pos, Velocity& vel, const Collidable& col) {
                if (pos.y < 0.0f)
                {
                    pos.y  = 0.0f;
                    vel.vy = -vel.vy * col.restitution;
                    vel.vx *= 0.98f;
                    vel.vz *= 0.98f;
                }
            });
    }
};

class LifetimeSystem : public fr::System
{
  public:
    explicit LifetimeSystem(const Ref<fr::Scene>& scene) : System(scene) {}

    void Update(float dt) override
    {
        mScene->ForEachAsync<Lifetime>([dt](auto, Lifetime& lt) { lt.remaining -= dt; });
    }
};

class ThermalSystem : public fr::System
{
  public:
    explicit ThermalSystem(const Ref<fr::Scene>& scene) : System(scene) {}

    void Update(float dt) override
    {
        mScene->ForEachAsync<Temperature>([dt](auto, Temperature& temp) {
            temp.value -= temp.dissipationRate * temp.value * dt;
            if (temp.value < 0.0f)
                temp.value = 0.0f;
        });
    }
};

class App : public skr::IApplication
{
  public:
    explicit App(const Ref<skr::ServiceProvider>& sp) : IApplication(sp) {}
    void Run() override {}
};

static void PopulateScene(const Ref<fr::Scene>& scene, int entityCount)
{
    const int n = entityCount;

    const int fullPhysics = n * 40 / 100;
    for (int i = 0; i < fullPhysics; ++i)
    {
        scene->CreateEntity(
            Position { .x = static_cast<float>(i), .y = 10.0f, .z = 0.0f },
            Velocity { .vx = 0.1f, .vy = 0.0f, .vz = 0.0f },
            Acceleration { .ax = 0.0f, .ay = 0.0f, .az = 0.0f },
            Mass { .value = 1.0f },
            Drag { .coefficient = 0.05f },
            Collidable { .restitution = 0.6f },
            Lifetime { .remaining = 5.0f });
    }

    const int sparks = n * 30 / 100;
    for (int i = 0; i < sparks; ++i)
    {
        scene->CreateEntity(
            Position { .x = static_cast<float>(i), .y = 5.0f, .z = 0.0f },
            Velocity { .vx = 0.2f, .vy = 2.0f, .vz = 0.0f },
            Acceleration { .ax = 0.0f, .ay = 0.0f, .az = 0.0f },
            Mass { .value = 0.01f },
            Lifetime { .remaining = 2.0f },
            Temperature { .value = 1200.0f, .dissipationRate = 0.8f });
    }

    const int smoke = n * 20 / 100;
    for (int i = 0; i < smoke; ++i)
    {
        scene->CreateEntity(Position { .x = static_cast<float>(i), .y = 2.0f, .z = 0.0f },
                            Velocity { .vx = 0.05f, .vy = 0.3f, .vz = 0.0f },
                            Lifetime { .remaining = 8.0f },
                            Temperature { .value = 300.0f, .dissipationRate = 0.2f });
    }

    const int debris = n - fullPhysics - sparks - smoke;
    for (int i = 0; i < debris; ++i)
    {
        scene->CreateEntity(Position { .x = static_cast<float>(i), .y = 20.0f, .z = 0.0f },
                            Velocity { .vx = 1.0f, .vy = 5.0f, .vz = 0.5f },
                            Acceleration { .ax = 0.0f, .ay = 0.0f, .az = 0.0f },
                            Mass { .value = 5.0f },
                            Collidable { .restitution = 0.3f });
    }
}

static void RegisterComponents(fr::FreyrExtension& freyr)
{
    freyr.AddComponent<Position>()
        .AddComponent<Velocity>()
        .AddComponent<Acceleration>()
        .AddComponent<Mass>()
        .AddComponent<Drag>()
        .AddComponent<Collidable>()
        .AddComponent<Lifetime>()
        .AddComponent<Temperature>();
}

static void RegisterSystems(fr::FreyrExtension& freyr)
{
    freyr.AddSystem<GravitySystem>()
        .AddSystem<DragSystem>()
        .AddSystem<IntegrationSystem>()
        .AddSystem<CollisionSystem>()
        .AddSystem<LifetimeSystem>()
        .AddSystem<ThermalSystem>();
}

static void RunBenchmark(benchmark::State& state, fr::FreyrExecutionStategy strategy)
{
    constexpr auto chunkCap    = 1 * 1024;
    const auto     entityCount = state.range(0);
    const auto     maxEntities = ((entityCount + chunkCap - 1) / chunkCap + 2) * chunkCap;

    auto app = skr::ApplicationBuilder()
                   .AddExtension<fr::FreyrExtension>([&](fr::FreyrExtension& freyr) {
                       freyr.WithOptions([&](fr::FreyrOptionsBuilder& builder) {
                           builder.WithMaxEntities(maxEntities)
                               .WithArchetypeChunkCapacity(chunkCap)
                               .WithThreadCount(std::thread::hardware_concurrency() - 2)
                               .WithExecutionStrategy(strategy);
                       });
                       RegisterComponents(freyr);
                       RegisterSystems(freyr);
                   })
                   .Build<App>();

    auto scene = app->GetRootServiceProvider().GetService<fr::Scene>();
    PopulateScene(scene, entityCount);

    constexpr auto bytesPerEntity =
        static_cast<std::int64_t>(sizeof(Position) + sizeof(Velocity) + sizeof(Acceleration) + sizeof(Mass) +
                                  sizeof(Drag) + sizeof(Collidable) + sizeof(Lifetime) + sizeof(Temperature));
    const std::int64_t bytesPerFrame = entityCount * bytesPerEntity;

    std::int64_t totalBytes = 0;
    for (auto _ : state)
    {
        scene->Update(0.016f);
        totalBytes += bytesPerFrame;
        state.SetBytesProcessed(totalBytes);
    }
}

static void DefaultStrategy(benchmark::State& state)
{
    RunBenchmark(state, fr::FreyrExecutionStategy::Default);
}

static void ChunkAffinity(benchmark::State& state)
{
    RunBenchmark(state, fr::FreyrExecutionStategy::ChunkAffinity);
}

BENCHMARK(DefaultStrategy)
    ->RangeMultiplier(2)
    ->Range(100'000, 1'000'000)
    ->MinWarmUpTime(10.0)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(ChunkAffinity)
    ->RangeMultiplier(2)
    ->Range(100'000, 1'000'000)
    ->MinWarmUpTime(10.0)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();