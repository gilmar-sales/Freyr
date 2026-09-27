#include <benchmark/benchmark.h>

#include "Scenes.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace
{
    using Entity               = std::uint32_t;
    constexpr Entity kNoEntity = ~Entity { 0 };

    class MapVecStore
    {
      public:
        explicit MapVecStore(std::size_t capacity) : mParent(capacity, kNoEntity), mIndex(capacity, 0)
        {
        }

        void Attach(Entity child, Entity parent)
        {
            auto& kids    = mChildren[parent];
            mIndex[child] = static_cast<std::uint32_t>(kids.size());
            kids.push_back(child);
            mParent[child] = parent;
        }

        void Detach(Entity child)
        {
            const Entity parent = mParent[child];
            if (parent == kNoEntity)
                return;
            const auto it    = mChildren.find(parent);
            auto&      kids  = it->second;
            const auto index = mIndex[child];
            kids.erase(kids.begin() + index);
            for (std::size_t i = index; i < kids.size(); ++i)
                mIndex[kids[i]] = static_cast<std::uint32_t>(i);
            if (kids.empty())
                mChildren.erase(it);
            mParent[child] = kNoEntity;
        }

        template <typename TFunc>
        void ForEachChild(Entity parent, TFunc&& func) const
        {
            const auto it = mChildren.find(parent);
            if (it == mChildren.end())
                return;
            for (const Entity child : it->second)
                func(child);
        }

      private:
        std::vector<Entity>                             mParent;
        std::vector<std::uint32_t>                      mIndex;
        std::unordered_map<Entity, std::vector<Entity>> mChildren;
    };

    class MapVecKeepStore
    {
      public:
        explicit MapVecKeepStore(std::size_t capacity) :
            mParent(capacity, kNoEntity), mIndex(capacity, 0)
        {
        }

        void Attach(Entity child, Entity parent)
        {
            auto& kids    = mChildren[parent];
            mIndex[child] = static_cast<std::uint32_t>(kids.size());
            kids.push_back(child);
            mParent[child] = parent;
        }

        void Detach(Entity child)
        {
            const Entity parent = mParent[child];
            if (parent == kNoEntity)
                return;
            auto&      kids  = mChildren.find(parent)->second;
            const auto index = mIndex[child];
            kids.erase(kids.begin() + index);
            for (std::size_t i = index; i < kids.size(); ++i)
                mIndex[kids[i]] = static_cast<std::uint32_t>(i);
            mParent[child] = kNoEntity;
        }

        template <typename TFunc>
        void ForEachChild(Entity parent, TFunc&& func) const
        {
            const auto it = mChildren.find(parent);
            if (it == mChildren.end())
                return;
            for (const Entity child : it->second)
                func(child);
        }

      private:
        std::vector<Entity>                             mParent;
        std::vector<std::uint32_t>                      mIndex;
        std::unordered_map<Entity, std::vector<Entity>> mChildren;
    };

    class FlatVecStore
    {
      public:
        explicit FlatVecStore(std::size_t capacity) :
            mParent(capacity, kNoEntity), mIndex(capacity, 0), mChildren(capacity)
        {
        }

        void Attach(Entity child, Entity parent)
        {
            auto& kids    = mChildren[parent];
            mIndex[child] = static_cast<std::uint32_t>(kids.size());
            kids.push_back(child);
            mParent[child] = parent;
        }

        void Detach(Entity child)
        {
            const Entity parent = mParent[child];
            if (parent == kNoEntity)
                return;
            auto&      kids  = mChildren[parent];
            const auto index = mIndex[child];
            kids.erase(kids.begin() + index);
            for (std::size_t i = index; i < kids.size(); ++i)
                mIndex[kids[i]] = static_cast<std::uint32_t>(i);
            mParent[child] = kNoEntity;
        }

        template <typename TFunc>
        void ForEachChild(Entity parent, TFunc&& func) const
        {
            for (const Entity child : mChildren[parent])
                func(child);
        }

      private:
        std::vector<Entity>              mParent;
        std::vector<std::uint32_t>       mIndex;
        std::vector<std::vector<Entity>> mChildren;
    };

    class FlatVecSwapRemoveStore
    {
      public:
        explicit FlatVecSwapRemoveStore(std::size_t capacity) :
            mParent(capacity, kNoEntity), mIndex(capacity, 0), mChildren(capacity)
        {
        }

        void Attach(Entity child, Entity parent)
        {
            auto& kids    = mChildren[parent];
            mIndex[child] = static_cast<std::uint32_t>(kids.size());
            kids.push_back(child);
            mParent[child] = parent;
        }

        void Detach(Entity child)
        {
            const Entity parent = mParent[child];
            if (parent == kNoEntity)
                return;
            auto&        kids  = mChildren[parent];
            const auto   index = mIndex[child];
            const Entity last  = kids.back();
            kids[index]        = last;
            mIndex[last]       = index;
            kids.pop_back();
            mParent[child] = kNoEntity;
        }

        template <typename TFunc>
        void ForEachChild(Entity parent, TFunc&& func) const
        {
            for (const Entity child : mChildren[parent])
                func(child);
        }

      private:
        std::vector<Entity>              mParent;
        std::vector<std::uint32_t>       mIndex;
        std::vector<std::vector<Entity>> mChildren;
    };

    class IntrusiveListStore
    {
      public:
        explicit IntrusiveListStore(std::size_t capacity) : mLinks(capacity)
        {
        }

        void Attach(Entity child, Entity parent)
        {
            auto& parentLinks = mLinks[parent];
            auto& childLinks  = mLinks[child];
            childLinks.parent = parent;
            childLinks.prev   = parentLinks.last;
            childLinks.next   = kNoEntity;
            if (parentLinks.last != kNoEntity)
                mLinks[parentLinks.last].next = child;
            else
                parentLinks.first = child;
            parentLinks.last = child;
        }

        void Detach(Entity child)
        {
            auto& childLinks = mLinks[child];
            if (childLinks.parent == kNoEntity)
                return;
            auto& parentLinks = mLinks[childLinks.parent];
            if (childLinks.prev != kNoEntity)
                mLinks[childLinks.prev].next = childLinks.next;
            else
                parentLinks.first = childLinks.next;
            if (childLinks.next != kNoEntity)
                mLinks[childLinks.next].prev = childLinks.prev;
            else
                parentLinks.last = childLinks.prev;
            childLinks.parent = kNoEntity;
            childLinks.prev   = kNoEntity;
            childLinks.next   = kNoEntity;
        }

        template <typename TFunc>
        void ForEachChild(Entity parent, TFunc&& func) const
        {
            for (Entity child = mLinks[parent].first; child != kNoEntity; child = mLinks[child].next)
                func(child);
        }

      private:
        struct Links
        {
            Entity parent = kNoEntity;
            Entity first  = kNoEntity;
            Entity last   = kNoEntity;
            Entity prev   = kNoEntity;
            Entity next   = kNoEntity;
        };

        std::vector<Links> mLinks;
    };

    class IntrusiveSparseStore
    {
      public:
        explicit IntrusiveSparseStore(std::size_t)
        {
        }

        void Attach(Entity child, Entity parent)
        {
            const auto parentIndex = Ensure(parent);
            const auto childIndex  = Ensure(child);
            auto&      childLinks  = mLinks[childIndex];
            const auto last        = mLinks[parentIndex].last;
            childLinks.parent      = parent;
            childLinks.prev        = last;
            childLinks.next        = kNoEntity;
            if (last != kNoEntity)
                mLinks[mNodes.index(last)].next = child;
            else
                mLinks[parentIndex].first = child;
            mLinks[parentIndex].last = child;
        }

        void Detach(Entity child)
        {
            const auto childIndex = mNodes.find(child);
            if (childIndex == Nodes::npos)
                return;
            auto& childLinks = mLinks[childIndex];
            if (childLinks.parent == kNoEntity)
                return;
            auto& parentLinks = mLinks[mNodes.index(childLinks.parent)];
            if (childLinks.prev != kNoEntity)
                mLinks[mNodes.index(childLinks.prev)].next = childLinks.next;
            else
                parentLinks.first = childLinks.next;
            if (childLinks.next != kNoEntity)
                mLinks[mNodes.index(childLinks.next)].prev = childLinks.prev;
            else
                parentLinks.last = childLinks.prev;
            childLinks.parent = kNoEntity;
            childLinks.prev   = kNoEntity;
            childLinks.next   = kNoEntity;
        }

        template <typename TFunc>
        void ForEachChild(Entity parent, TFunc&& func) const
        {
            const auto parentIndex = mNodes.find(parent);
            if (parentIndex == Nodes::npos)
                return;
            for (Entity child = mLinks[parentIndex].first; child != kNoEntity;
                 child        = mLinks[mNodes.index(child)].next)
                func(child);
        }

      private:
        using Nodes = fr::LocalSparseSet<Entity>;

        struct Links
        {
            Entity parent = kNoEntity;
            Entity first  = kNoEntity;
            Entity last   = kNoEntity;
            Entity prev   = kNoEntity;
            Entity next   = kNoEntity;
        };

        std::size_t Ensure(Entity entity)
        {
            const auto index = mNodes.find(entity);
            if (index != Nodes::npos)
                return index;
            mNodes.insert(entity);
            mLinks.emplace_back();
            return mLinks.size() - 1;
        }

        Nodes              mNodes;
        std::vector<Links> mLinks;
    };

    template <typename TStore>
    void Load(TStore& store, const scenes::SceneDesc& scene)
    {
        for (std::size_t i = 0; i < scene.nodes.size(); ++i)
        {
            if (scene.nodes[i].parent >= 0)
                store.Attach(static_cast<Entity>(i), static_cast<Entity>(scene.nodes[i].parent));
        }
    }

    std::vector<Entity> Roots(const scenes::SceneDesc& scene)
    {
        std::vector<Entity> roots;
        for (std::size_t i = 0; i < scene.nodes.size(); ++i)
        {
            if (scene.nodes[i].parent < 0)
                roots.push_back(static_cast<Entity>(i));
        }
        return roots;
    }

    template <typename TStore>
    void BM_Swap(benchmark::State& state)
    {
        const auto scene = scenes::BuildCrowd(static_cast<std::uint32_t>(state.range(0)));
        TStore     store(scene.nodes.size());
        Load(store, scene);

        bool toLeft = true;
        for (auto _ : state)
        {
            for (const auto& character : scene.characters)
            {
                const auto weapon = static_cast<Entity>(character.weapon);
                store.Detach(weapon);
                store.Attach(weapon, static_cast<Entity>(toLeft ? character.leftSocket
                                                                : character.rightSocket));
            }
            toLeft = !toLeft;
        }
        state.SetItemsProcessed(state.iterations() *
                                static_cast<std::int64_t>(scene.characters.size()));
    }

    template <typename TStore>
    void BM_Traverse(benchmark::State& state)
    {
        const auto scene = scenes::BuildCrowd(static_cast<std::uint32_t>(state.range(0)));
        TStore     store(scene.nodes.size());
        Load(store, scene);
        const auto roots = Roots(scene);

        std::vector<Entity> stack;
        stack.reserve(scene.nodes.size());
        for (auto _ : state)
        {
            std::uint64_t sum = 0;
            stack.assign(roots.begin(), roots.end());
            while (!stack.empty())
            {
                const Entity parent = stack.back();
                stack.pop_back();
                store.ForEachChild(parent, [&](Entity child) {
                    sum += child;
                    stack.push_back(child);
                });
            }
            benchmark::DoNotOptimize(sum);
        }
        state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(scene.nodes.size()));
    }

    template <typename TStore>
    void BM_Build(benchmark::State& state)
    {
        const auto scene = scenes::BuildCrowd(static_cast<std::uint32_t>(state.range(0)));
        for (auto _ : state)
        {
            TStore store(scene.nodes.size());
            Load(store, scene);
            benchmark::DoNotOptimize(store);
        }
        state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(scene.nodes.size()));
    }

    template <typename TStore>
    void BM_WideFrontDetach(benchmark::State& state)
    {
        const auto width = static_cast<Entity>(state.range(0));
        TStore     store(width + 1);
        for (Entity child = 1; child <= width; ++child)
            store.Attach(child, 0);

        Entity next = 1;
        for (auto _ : state)
        {
            store.Detach(next);
            store.Attach(next, 0);
            next = next == width ? 1 : next + 1;
        }
    }

    double Micros(std::chrono::steady_clock::duration duration)
    {
        return std::chrono::duration<double, std::micro>(duration).count();
    }

    void BM_WeaponSwapBreakdown_Freyr(benchmark::State& state)
    {
        using Clock = std::chrono::steady_clock;

        const auto scene    = scenes::BuildCrowd(static_cast<std::uint32_t>(state.range(0)));
        auto       app      = scenes::CreateFreyrApp(scene.nodes.size() * 2 + 4'096);
        auto&      registry = *app->registry;
        const auto entities = scenes::LoadFreyr(registry, scene);

        std::vector<std::array<float, 16>> worlds(scene.characters.size());

        double captureUs = 0.0, setParentUs = 0.0, syncUs = 0.0, relocalUs = 0.0, updateUs = 0.0;

        bool toLeft = true;
        for (auto _ : state)
        {
            const auto t0 = Clock::now();
            for (std::size_t i = 0; i < scene.characters.size(); ++i)
            {
                registry.TryGetComponents<fr::WorldTransform3D>(
                    entities[scene.characters[i].weapon], [&](fr::WorldTransform3D& current) {
                        std::copy(std::begin(current.matrix), std::end(current.matrix),
                                  worlds[i].begin());
                    });
            }

            const auto t1 = Clock::now();
            for (const auto& character : scene.characters)
            {
                registry.SetParent(entities[character.weapon],
                                   entities[toLeft ? character.leftSocket : character.rightSocket]);
            }

            const auto t2 = Clock::now();
            registry.FlushHierarchyComponents();

            const auto t3 = Clock::now();
            for (std::size_t i = 0; i < scene.characters.size(); ++i)
            {
                const auto& character = scene.characters[i];
                const auto  weapon    = entities[character.weapon];
                const auto  socket = entities[toLeft ? character.leftSocket : character.rightSocket];
                registry.TryGetComponents<fr::WorldTransform3D>(
                    socket, [&](fr::WorldTransform3D& parentWorld) {
                        const auto next = fr::LocalFromWorld(parentWorld.matrix, worlds[i].data());
                        registry.TryGetComponents<fr::Transform3D>(
                            weapon, [&](fr::Transform3D& local) { local = next; });
                    });
                registry.MarkHierarchyDirty<fr::Transform3D>(weapon);
            }

            const auto t4 = Clock::now();
            registry.Update(0.016f);
            const auto t5 = Clock::now();

            captureUs += Micros(t1 - t0);
            setParentUs += Micros(t2 - t1);
            syncUs += Micros(t3 - t2);
            relocalUs += Micros(t4 - t3);
            updateUs += Micros(t5 - t4);
            toLeft = !toLeft;
        }

        using benchmark::Counter;
        state.counters["capture_us"]   = Counter(captureUs, Counter::kAvgIterations);
        state.counters["setparent_us"] = Counter(setParentUs, Counter::kAvgIterations);
        state.counters["sync_us"]      = Counter(syncUs, Counter::kAvgIterations);
        state.counters["relocal_us"]   = Counter(relocalUs, Counter::kAvgIterations);
        state.counters["update_us"]    = Counter(updateUs, Counter::kAvgIterations);
    }

    void BM_Swap_FreyrHierarchyManager(benchmark::State& state)
    {
        const auto scene    = scenes::BuildCrowd(static_cast<std::uint32_t>(state.range(0)));
        auto       app      = scenes::CreateFreyrApp(scene.nodes.size() * 2 + 4'096);
        auto&      registry = *app->registry;
        const auto entities = scenes::LoadFreyr(registry, scene);
        auto       manager  = registry.GetHierarchyManager();

        bool toLeft = true;
        for (auto _ : state)
        {
            for (const auto& character : scene.characters)
            {
                manager->SetParent(entities[character.weapon],
                                   entities[toLeft ? character.leftSocket : character.rightSocket]);
            }
            toLeft = !toLeft;
        }
        state.SetItemsProcessed(state.iterations() *
                                static_cast<std::int64_t>(scene.characters.size()));
    }
} // namespace

BENCHMARK(BM_WeaponSwapBreakdown_Freyr)->Arg(1'000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Swap_FreyrHierarchyManager)->Arg(1'000)->Unit(benchmark::kMicrosecond);

BENCHMARK_TEMPLATE(BM_Swap, MapVecStore)->Arg(1'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Swap, MapVecKeepStore)->Arg(1'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Swap, FlatVecStore)->Arg(1'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Swap, FlatVecSwapRemoveStore)->Arg(1'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Swap, IntrusiveListStore)->Arg(1'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Swap, IntrusiveSparseStore)->Arg(1'000)->Unit(benchmark::kMicrosecond);

BENCHMARK_TEMPLATE(BM_Traverse, MapVecStore)->Arg(5'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Traverse, MapVecKeepStore)->Arg(5'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Traverse, FlatVecStore)->Arg(5'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Traverse, FlatVecSwapRemoveStore)->Arg(5'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Traverse, IntrusiveListStore)->Arg(5'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Traverse, IntrusiveSparseStore)->Arg(5'000)->Unit(benchmark::kMicrosecond);

BENCHMARK_TEMPLATE(BM_Build, MapVecStore)->Arg(1'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Build, MapVecKeepStore)->Arg(1'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Build, FlatVecStore)->Arg(1'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Build, FlatVecSwapRemoveStore)->Arg(1'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Build, IntrusiveListStore)->Arg(1'000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Build, IntrusiveSparseStore)->Arg(1'000)->Unit(benchmark::kMicrosecond);

BENCHMARK_TEMPLATE(BM_WideFrontDetach, MapVecStore)->Arg(16)->Arg(256)->Arg(4'096);
BENCHMARK_TEMPLATE(BM_WideFrontDetach, FlatVecStore)->Arg(16)->Arg(256)->Arg(4'096);
BENCHMARK_TEMPLATE(BM_WideFrontDetach, FlatVecSwapRemoveStore)->Arg(16)->Arg(256)->Arg(4'096);
BENCHMARK_TEMPLATE(BM_WideFrontDetach, IntrusiveListStore)->Arg(16)->Arg(256)->Arg(4'096);
BENCHMARK_TEMPLATE(BM_WideFrontDetach, IntrusiveSparseStore)->Arg(16)->Arg(256)->Arg(4'096);

BENCHMARK_MAIN();
