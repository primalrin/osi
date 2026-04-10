#include "allocator.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

struct Operation {
    enum class Type {
        Allocate,
        Free
    };

    Type type = Type::Allocate;
    size_t id = 0;
    size_t size = 0;
};

struct Scenario {
    std::string name;
    std::string description;
    std::vector<Operation> operations;
};

struct ActiveAllocation {
    void* pointer = nullptr;
    size_t requestedSize = 0;
    size_t reservedSize = 0;
};

struct ScenarioResult {
    std::string allocatorName;
    std::string scenarioName;
    size_t totalAllocationRequests = 0;
    size_t successfulAllocations = 0;
    size_t failedAllocations = 0;
    double averageAllocationNs = 0.0;
    double averageFreeNs = 0.0;
    double averageUtilization = 0.0;
    double peakUtilization = 0.0;
    size_t peakRequestedBytes = 0;
    size_t peakReservedBytes = 0;
};

std::vector<Operation> generateRandomOperations(
    size_t operationCount,
    size_t maxLiveBlocks,
    size_t minSize,
    size_t maxSize,
    double freeProbability,
    uint32_t seed
) {
    std::mt19937 generator(seed);
    std::uniform_int_distribution<size_t> sizeDistribution(minSize, maxSize);
    std::uniform_real_distribution<double> actionDistribution(0.0, 1.0);

    std::vector<size_t> activeIds;
    std::vector<Operation> operations;
    operations.reserve(operationCount + maxLiveBlocks);

    size_t nextId = 0;
    for (size_t i = 0; i < operationCount; ++i) {
        const bool mustAllocate = activeIds.empty();
        const bool canAllocateMore = activeIds.size() < maxLiveBlocks;
        const bool chooseAllocate = mustAllocate || (canAllocateMore && actionDistribution(generator) > freeProbability);

        if (chooseAllocate) {
            const size_t id = nextId++;
            operations.push_back({Operation::Type::Allocate, id, sizeDistribution(generator)});
            activeIds.push_back(id);
        } else {
            std::uniform_int_distribution<size_t> indexDistribution(0, activeIds.size() - 1);
            const size_t index = indexDistribution(generator);
            operations.push_back({Operation::Type::Free, activeIds[index], 0});
            activeIds.erase(activeIds.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    for (size_t id : activeIds) {
        operations.push_back({Operation::Type::Free, id, 0});
    }

    return operations;
}

Scenario buildSmallBlocksScenario() {
    return {
        "Мелкие блоки",
        "Случайные блоки размером от 16 до 128 байт, умеренное число одновременно живых объектов.",
        generateRandomOperations(50000, 512, 16, 128, 0.45, 42)
    };
}

Scenario buildMixedScenario() {
    return {
        "Смешанная нагрузка",
        "Блоки размером от 32 до 4096 байт, нагрузка похожа на реальную программу общего назначения.",
        generateRandomOperations(50000, 256, 32, 4096, 0.48, 1337)
    };
}

Scenario buildFragmentationScenario() {
    std::vector<Operation> operations;
    operations.reserve(25000);

    size_t nextId = 0;
    std::vector<size_t> smallIds;
    std::vector<size_t> largeIds;

    for (size_t i = 0; i < 2000; ++i) {
        const size_t smallId = nextId++;
        const size_t largeId = nextId++;
        operations.push_back({Operation::Type::Allocate, smallId, 64});
        operations.push_back({Operation::Type::Allocate, largeId, 2048});
        smallIds.push_back(smallId);
        largeIds.push_back(largeId);
    }

    for (size_t i = 0; i < smallIds.size(); i += 2) {
        operations.push_back({Operation::Type::Free, smallIds[i], 0});
    }
    for (size_t i = 0; i < largeIds.size(); i += 2) {
        operations.push_back({Operation::Type::Free, largeIds[i], 0});
    }

    auto randomPart = generateRandomOperations(15000, 300, 24, 3000, 0.5, 2024);
    for (Operation& operation : randomPart) {
        operation.id += nextId;
    }
    operations.insert(operations.end(), randomPart.begin(), randomPart.end());

    for (size_t i = 1; i < smallIds.size(); i += 2) {
        operations.push_back({Operation::Type::Free, smallIds[i], 0});
    }
    for (size_t i = 1; i < largeIds.size(); i += 2) {
        operations.push_back({Operation::Type::Free, largeIds[i], 0});
    }

    return {
        "Фрагментация",
        "Чередование маленьких и больших блоков с частичным освобождением для создания разрывов в памяти.",
        std::move(operations)
    };
}

ScenarioResult runScenario(const Scenario& scenario, AllocatorKind kind, size_t bufferSize) {
    std::vector<std::byte> memory(bufferSize + 64);

    Allocator* allocator = nullptr;
    if (kind == AllocatorKind::FirstFit) {
        allocator = createFirstFitAllocator(memory.data(), memory.size());
    } else {
        allocator = createPowerOfTwoAllocator(memory.data(), memory.size());
    }

    if (allocator == nullptr) {
        throw std::runtime_error("Не удалось создать аллокатор.");
    }

    ScenarioResult result;
    result.allocatorName = getAllocatorName(allocator);
    result.scenarioName = scenario.name;

    std::unordered_map<size_t, ActiveAllocation> active;
    active.reserve(2048);

    size_t currentRequestedBytes = 0;
    size_t currentReservedBytes = 0;
    double utilizationSum = 0.0;
    size_t utilizationSamples = 0;

    long long allocationTimeNs = 0;
    long long freeTimeNs = 0;
    size_t freeOperations = 0;

    for (const Operation& operation : scenario.operations) {
        if (operation.type == Operation::Type::Allocate) {
            ++result.totalAllocationRequests;

            const auto start = std::chrono::steady_clock::now();
            void* pointer = alloc(allocator, operation.size);
            const auto finish = std::chrono::steady_clock::now();

            allocationTimeNs += std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();

            if (pointer != nullptr) {
                const size_t reservedSize = getAllocatedBlockFootprint(allocator, pointer);
                active[operation.id] = {pointer, operation.size, reservedSize};
                currentRequestedBytes += operation.size;
                currentReservedBytes += reservedSize;
                ++result.successfulAllocations;
            } else {
                ++result.failedAllocations;
            }
        } else {
            auto it = active.find(operation.id);
            if (it == active.end()) {
                continue;
            }

            const auto start = std::chrono::steady_clock::now();
            freeBlock(allocator, it->second.pointer);
            const auto finish = std::chrono::steady_clock::now();

            freeTimeNs += std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
            currentRequestedBytes -= it->second.requestedSize;
            currentReservedBytes -= it->second.reservedSize;
            active.erase(it);
            ++freeOperations;
        }

        const AllocatorStats stats = getAllocatorStats(allocator);
        if (currentRequestedBytes > 0 && currentReservedBytes > 0) {
            const double utilization = static_cast<double>(currentRequestedBytes) /
                static_cast<double>(currentReservedBytes);

            utilizationSum += utilization;
            ++utilizationSamples;
            result.peakUtilization = std::max(result.peakUtilization, utilization);
        }

        result.peakRequestedBytes = std::max(result.peakRequestedBytes, currentRequestedBytes);
        result.peakReservedBytes = std::max(result.peakReservedBytes, currentReservedBytes);
    }

    result.averageAllocationNs = result.totalAllocationRequests == 0
        ? 0.0
        : static_cast<double>(allocationTimeNs) / static_cast<double>(result.totalAllocationRequests);

    result.averageFreeNs = freeOperations == 0
        ? 0.0
        : static_cast<double>(freeTimeNs) / static_cast<double>(freeOperations);

    result.averageUtilization = utilizationSamples == 0
        ? 0.0
        : utilizationSum / static_cast<double>(utilizationSamples);

    destroyAllocator(allocator);
    return result;
}

ScenarioResult runScenarioRepeated(const Scenario& scenario, AllocatorKind kind, size_t bufferSize, size_t repetitions) {
    ScenarioResult accumulated;

    for (size_t i = 0; i < repetitions; ++i) {
        ScenarioResult current = runScenario(scenario, kind, bufferSize);

        if (i == 0) {
            accumulated.allocatorName = current.allocatorName;
            accumulated.scenarioName = current.scenarioName;
            accumulated.totalAllocationRequests = current.totalAllocationRequests;
            accumulated.successfulAllocations = current.successfulAllocations;
            accumulated.failedAllocations = current.failedAllocations;
            accumulated.peakRequestedBytes = current.peakRequestedBytes;
            accumulated.peakReservedBytes = current.peakReservedBytes;
        }

        accumulated.averageAllocationNs += current.averageAllocationNs;
        accumulated.averageFreeNs += current.averageFreeNs;
        accumulated.averageUtilization += current.averageUtilization;
        accumulated.peakUtilization += current.peakUtilization;
    }

    accumulated.averageAllocationNs /= static_cast<double>(repetitions);
    accumulated.averageFreeNs /= static_cast<double>(repetitions);
    accumulated.averageUtilization /= static_cast<double>(repetitions);
    accumulated.peakUtilization /= static_cast<double>(repetitions);

    return accumulated;
}

void printScenarioHeader(const Scenario& scenario) {
    std::cout << "\n=== " << scenario.name << " ===\n";
    std::cout << scenario.description << "\n";
}

void printResult(const ScenarioResult& result) {
    std::cout << result.allocatorName
              << " | успешных выделений: " << std::setw(6) << result.successfulAllocations
              << " | отказов: " << std::setw(6) << result.failedAllocations
              << " | alloc, нс: " << std::setw(10) << std::fixed << std::setprecision(2) << result.averageAllocationNs
              << " | free, нс: " << std::setw(10) << result.averageFreeNs
              << " | средн. использование: " << std::setw(6) << result.averageUtilization
              << " | пик: " << result.peakUtilization << "\n";
}

int main() {
    try {
        constexpr size_t bufferSize = 1 << 20;
        constexpr size_t repetitions = 5;

        std::vector<Scenario> scenarios;
        scenarios.push_back(buildSmallBlocksScenario());
        scenarios.push_back(buildMixedScenario());
        scenarios.push_back(buildFragmentationScenario());

        std::cout << "Сравнение аллокаторов памяти\n";
        std::cout << "Размер тестового буфера: " << bufferSize << " байт\n";
        std::cout << "Количество прогонов каждого сценария: " << repetitions << "\n";

        for (const Scenario& scenario : scenarios) {
            printScenarioHeader(scenario);
            const ScenarioResult firstFit = runScenarioRepeated(scenario, AllocatorKind::FirstFit, bufferSize, repetitions);
            const ScenarioResult powerOfTwo = runScenarioRepeated(scenario, AllocatorKind::PowerOfTwo, bufferSize, repetitions);

            printResult(firstFit);
            printResult(powerOfTwo);
        }
    } catch (const std::exception& exception) {
        std::cerr << "Ошибка: " << exception.what() << "\n";
        return 1;
    }

    return 0;
}
