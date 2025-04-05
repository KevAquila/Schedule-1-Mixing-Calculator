#include "../Schedule I Mixer Sim/property_mixer_core.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <thread>
#include <mutex>
#include <future>
#include <atomic>
#include <chrono>
#include <cmath>
#include <bitset>
#include <fstream>
#include <unordered_map>
#include <stack>
#include <functional>

// Define ingredient mapping
std::map<std::string, std::string> ingredientPropertyMapping = {
    {"Cuke", "energizing"},
    {"Donut", "caloriedense"},
    {"Flu Medicine", "sedating"},
    {"Gasoline", "toxic"},
    {"Energy Drink", "athletic"},
    {"Mouth Wash", "balding"},
    {"Banana", "gingeritis"},
    {"Chili", "spicy"},
    {"Motor Oil", "slippery"},
    {"Iodine", "jennerising"},
    {"Paracetamol", "sneaky"},
    {"Viagra", "tropicthunder"},
    {"Horse Semen", "giraffying"},
    {"Mega Bean", "foggy"},
    {"Addy", "thoughtprovoking"},
    {"Battery", "brighteyed"}
};

// Memory-optimized data structures
using PropertySet = uint64_t;    // Bit representation of properties (64 bits max)

// Compact path entry structure with sequence preservation
struct CompactPathEntry {
    std::vector<uint8_t> ingredientSequence;  // Sequence of ingredient indices (0-15)
    float baseValueBonus;
    float addictiveness;
    float valueMultiplier;

    // Constructor for convenience
    CompactPathEntry() : baseValueBonus(0), addictiveness(0), valueMultiplier(1.0f) {}
};

// Main lookup table: property bitset -> paths
using PropertyPathTable = std::unordered_map<PropertySet, std::vector<CompactPathEntry>>;

// Mapping tables for bit conversion
std::unordered_map<std::string, uint64_t> propertyBitMapping;
std::vector<std::string> ingredientByBitPosition;
std::vector<std::string> propertyByBitPosition;

// Progress tracking
std::atomic<size_t> permutationsDone = 0;
std::mutex resultMutex;

// =================== BIT MAPPING FUNCTIONS ===================

// Initialize bit mappings
void initializeBitMappings() {
    // Initialize ingredient bit mapping
    ingredientByBitPosition.clear();
    for (const auto& [name, _] : ingredientPropertyMapping) {
        ingredientByBitPosition.push_back(name);
    }

    // Initialize property bit mapping
    propertyBitMapping.clear();
    propertyByBitPosition.clear();
    uint64_t propBit = 0;
    for (const auto& [id, prop] : properties) {
        propertyBitMapping[id] = 1ULL << propBit;
        propertyByBitPosition.push_back(id);
        propBit++;
    }
}

// Convert property to bit
uint64_t propertyToBit(Property* prop) {
    auto it = propertyBitMapping.find(prop->id);
    if (it != propertyBitMapping.end()) {
        return it->second;
    }
    return 0;
}

// Convert properties to bitset
PropertySet propertiesToBitset(const std::vector<Property*>& props) {
    PropertySet bits = 0;
    for (auto* prop : props) {
        bits |= propertyToBit(prop);
    }
    return bits;
}

// Convert bitset to properties
std::vector<Property*> bitsetToProperties(PropertySet bits) {
    std::vector<Property*> props;

    for (int i = 0; i < 64; i++) {
        if (bits & (1ULL << i)) {
            if (i < propertyByBitPosition.size()) {
                Property* prop = getPropertyByNameOrId(propertyByBitPosition[i]);
                if (prop) {
                    props.push_back(prop);
                }
            }
        }
    }

    return props;
}

// =================== PROGRESS DISPLAY FUNCTIONS ===================

void displayProgressBar(size_t totalPermutations) {
    const int barWidth = 50;
    auto startTime = std::chrono::steady_clock::now();

    while (permutationsDone < totalPermutations) {
        float progress = static_cast<float>(permutationsDone.load()) / totalPermutations;
        int pos = static_cast<int>(barWidth * progress);

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

        int remaining = progress > 0 ? static_cast<int>((elapsed / progress) - elapsed) : 0;

        int hrs = remaining / 3600;
        int mins = (remaining % 3600) / 60;
        int secs = remaining % 60;

        std::cout << "\r[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }

        std::cout << "] ";
        std::cout << std::fixed << std::setprecision(2) << (progress * 100.0) << "%";
        std::cout << "  ETA: " << std::setw(2) << std::setfill('0') << hrs << ":"
            << std::setw(2) << std::setfill('0') << mins << ":"
            << std::setw(2) << std::setfill('0') << secs;
        std::cout << "  (" << permutationsDone.load() << "/" << totalPermutations << ")";
        std::cout << std::flush;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Final bar
    std::cout << "\r[";
    for (int i = 0; i < barWidth; ++i) std::cout << "=";
    std::cout << "] 100.00%  ETA: 00:00:00";
    std::cout << "  (" << permutationsDone.load() << "/" << totalPermutations << ")" << std::endl;
}

// =================== FILE I/O FUNCTIONS ===================

// Save path table in binary format
void saveBinaryPathTable(const PropertyPathTable& table, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);

    if (!file) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }

    // Write table size
    uint32_t tableSize = table.size();
    file.write(reinterpret_cast<const char*>(&tableSize), sizeof(tableSize));

    // Write each entry
    for (const auto& [propBits, entries] : table) {
        // Write property bitset
        file.write(reinterpret_cast<const char*>(&propBits), sizeof(propBits));

        // Write number of entries
        uint8_t entryCount = entries.size();
        file.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));

        // Write each path entry
        for (const auto& entry : entries) {
            // Write sequence length
            uint8_t seqLength = entry.ingredientSequence.size();
            file.write(reinterpret_cast<const char*>(&seqLength), sizeof(seqLength));

            // Write ingredient sequence
            for (uint8_t idx : entry.ingredientSequence) {
                file.write(reinterpret_cast<const char*>(&idx), sizeof(idx));
            }

            // Write stats
            file.write(reinterpret_cast<const char*>(&entry.baseValueBonus), sizeof(entry.baseValueBonus));
            file.write(reinterpret_cast<const char*>(&entry.addictiveness), sizeof(entry.addictiveness));
            file.write(reinterpret_cast<const char*>(&entry.valueMultiplier), sizeof(entry.valueMultiplier));
        }
    }

    file.close();
    std::cout << "Saved " << tableSize << " property combinations to " << filename << std::endl;
}

// Load path table from binary format
PropertyPathTable loadBinaryPathTable(const std::string& filename) {
    PropertyPathTable table;
    std::ifstream file(filename, std::ios::binary);

    if (!file) {
        std::cerr << "Error opening file for reading: " << filename << std::endl;
        return table;
    }

    // Read table size
    uint32_t tableSize;
    file.read(reinterpret_cast<char*>(&tableSize), sizeof(tableSize));

    // Read each entry
    for (uint32_t i = 0; i < tableSize; i++) {
        // Read property bitset
        PropertySet propBits;
        file.read(reinterpret_cast<char*>(&propBits), sizeof(propBits));

        // Read number of entries
        uint8_t entryCount;
        file.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));

        // Read each path entry
        std::vector<CompactPathEntry> entries;
        for (uint8_t j = 0; j < entryCount; j++) {
            CompactPathEntry entry;

            // Read sequence length
            uint8_t seqLength;
            file.read(reinterpret_cast<char*>(&seqLength), sizeof(seqLength));

            // Read ingredient sequence
            entry.ingredientSequence.resize(seqLength);
            for (uint8_t k = 0; k < seqLength; k++) {
                file.read(reinterpret_cast<char*>(&entry.ingredientSequence[k]), sizeof(uint8_t));
            }

            // Read stats
            file.read(reinterpret_cast<char*>(&entry.baseValueBonus), sizeof(entry.baseValueBonus));
            file.read(reinterpret_cast<char*>(&entry.addictiveness), sizeof(entry.addictiveness));
            file.read(reinterpret_cast<char*>(&entry.valueMultiplier), sizeof(entry.valueMultiplier));

            entries.push_back(entry);
        }

        table[propBits] = entries;
    }

    file.close();
    std::cout << "Loaded " << table.size() << " property combinations from " << filename << std::endl;
    return table;
}

// =================== PROCESSING FUNCTIONS ===================

// Process a batch of sequences for a specific ingredient count
void processSequenceBatch(
    PropertyPathTable& pathTable,
    const std::vector<Property*>& initialProperties,
    size_t startIngredient,
    size_t endIngredient,
    int ingredientCount,
    std::atomic<size_t>& sequencesProcessed,
    int threadId
) {
    // For each starting ingredient
    for (size_t firstIngredient = startIngredient; firstIngredient < endIngredient; firstIngredient++) {
        // Get property for the first ingredient
        std::string firstIngName = ingredientByBitPosition[firstIngredient];
        Property* firstProp = getPropertyByNameOrId(ingredientPropertyMapping[firstIngName]);

        if (!firstProp) continue;

        // Apply first ingredient
        std::vector<Property*> firstProps = PropertyMixCalculator::mixProperties(
            initialProperties, firstProp, DrugType::Marijuana);

        // Current sequence starts with this ingredient
        std::vector<uint8_t> currentSeq = { static_cast<uint8_t>(firstIngredient) };

        // Stack-based DFS to avoid recursion and stack overflow
        struct StackState {
            std::vector<uint8_t> sequence;
            std::vector<Property*> properties;
            size_t depth;
            size_t nextIngredient;

            StackState(const std::vector<uint8_t>& seq, const std::vector<Property*>& props,
                size_t d, size_t next)
                : sequence(seq), properties(props), depth(d), nextIngredient(next) {}
        };

        std::stack<StackState> dfsStack;
        dfsStack.push(StackState(currentSeq, firstProps, 1, 0));

        // For infrequent logging
        auto lastLogTime = std::chrono::steady_clock::now();
        const auto logInterval = std::chrono::seconds(5);

        while (!dfsStack.empty()) {
            StackState current = dfsStack.top();
            dfsStack.pop();

            // Log progress much less frequently - only every 5 seconds
            auto now = std::chrono::steady_clock::now();
            if (now - lastLogTime > logInterval) {
                // Use a mutex to prevent interleaved console output
                static std::mutex logMutex;
                {
                    std::lock_guard<std::mutex> lock(logMutex);
                    std::cout << "\rThread " << threadId << ": Ingredient "
                        << firstIngredient << "/" << (endIngredient - 1)
                        << ", sequences: " << sequencesProcessed.load() << std::flush;
                }
                lastLogTime = now;
            }

            // If we've reached target depth, add to results
            if (current.depth == ingredientCount) {
                // Calculate statistics
                float baseValueSum = 0.0f;
                float addictiveness = 0.0f;
                float valueMultiplier = 1.0f;

                for (auto* p : current.properties) {
                    baseValueSum += p->addBaseValueMultiple;
                    addictiveness += p->addictiveness;
                    valueMultiplier *= p->valueMultiplier;
                }

                // Create entry
                PropertySet propBits = propertiesToBitset(current.properties);
                CompactPathEntry entry;
                entry.ingredientSequence = current.sequence;
                entry.baseValueBonus = baseValueSum;
                entry.addictiveness = addictiveness;
                entry.valueMultiplier = valueMultiplier;

                // Add to results - need lock here
                {
                    std::lock_guard<std::mutex> lock(resultMutex);
                    pathTable[propBits].push_back(entry);
                }

                sequencesProcessed++;
                continue;
            }

            // Otherwise, try each ingredient for the next position
            for (size_t i = 0; i < ingredientByBitPosition.size(); i++) {
                std::string ingName = ingredientByBitPosition[i];
                Property* prop = getPropertyByNameOrId(ingredientPropertyMapping[ingName]);

                if (prop) {
                    // Apply this ingredient
                    std::vector<Property*> newProps = PropertyMixCalculator::mixProperties(
                        current.properties, prop, DrugType::Marijuana);

                    // Add to sequence
                    std::vector<uint8_t> newSeq = current.sequence;
                    newSeq.push_back(i);

                    // Push to stack
                    dfsStack.push(StackState(newSeq, newProps, current.depth + 1, 0));
                }
            }
        }
    }
}

// Filter and sort path table entries - can be called after merging
void filterAndSortPathTable(PropertyPathTable& table) {
    for (auto& [propBits, entries] : table) {
        if (entries.empty()) continue;

        // Sort by sequence length (fewer = better)
        std::sort(entries.begin(), entries.end(),
            [](const CompactPathEntry& a, const CompactPathEntry& b) {
                if (a.ingredientSequence.size() == b.ingredientSequence.size()) {
                    return a.baseValueBonus > b.baseValueBonus;
                }
        return a.ingredientSequence.size() < b.ingredientSequence.size();
            });

        // Keep only shortest paths
        size_t shortestLength = entries[0].ingredientSequence.size();
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [shortestLength](const CompactPathEntry& entry) {
                    return entry.ingredientSequence.size() > shortestLength;
                }),
            entries.end()
                    );

        // Limit to top 5
        if (entries.size() > 5) {
            entries.resize(5);
        }
    }
}

// Process single ingredient combinations
void processSingleIngredientCombinations(
    PropertyPathTable& pathTable,
    const std::vector<Property*>& initialProperties
) {
    size_t totalIngredients = ingredientByBitPosition.size();

    // Process all single-ingredient combinations directly
    for (size_t i = 0; i < totalIngredients; i++) {
        std::string ingName = ingredientByBitPosition[i];
        Property* prop = getPropertyByNameOrId(ingredientPropertyMapping[ingName]);

        if (prop) {
            std::vector<Property*> mixedProps = PropertyMixCalculator::mixProperties(
                initialProperties, prop, DrugType::Marijuana);

            // Calculate statistics
            float baseValueSum = 0.0f;
            float addictiveness = 0.0f;
            float valueMultiplier = 1.0f;

            for (auto* p : mixedProps) {
                baseValueSum += p->addBaseValueMultiple;
                addictiveness += p->addictiveness;
                valueMultiplier *= p->valueMultiplier;
            }

            // Create entry
            PropertySet propBits = propertiesToBitset(mixedProps);
            CompactPathEntry entry;
            entry.ingredientSequence.push_back(i);
            entry.baseValueBonus = baseValueSum;
            entry.addictiveness = addictiveness;
            entry.valueMultiplier = valueMultiplier;

            // Add to table
            pathTable[propBits].push_back(entry);
        }
    }
}

// More efficient path table merging
void mergePathTables(PropertyPathTable& target, PropertyPathTable& source) {
    // Reserve capacity in target to avoid excessive reallocation
    if (target.size() < target.size() + source.size()) {
        target.reserve(target.size() + source.size());
    }

    // Process source entries in batches to avoid long-running operations
    const size_t BATCH_SIZE = 1000;
    size_t entriesProcessed = 0;
    size_t totalEntries = source.size();

    // Create a vector of keys to process to avoid iterator invalidation
    std::vector<PropertySet> sourceKeys;
    sourceKeys.reserve(source.size());
    for (const auto& [propBits, _] : source) {
        sourceKeys.push_back(propBits);
    }

    // Display merge progress
    auto startTime = std::chrono::steady_clock::now();

    // Process keys in batches
    for (size_t i = 0; i < sourceKeys.size(); i++) {
        PropertySet propBits = sourceKeys[i];
        auto& entries = source[propBits];

        // Move entries to target
        auto targetIt = target.find(propBits);
        if (targetIt == target.end()) {
            // Property set not in target, insert directly
            target.emplace(propBits, std::move(entries));
        }
        else {
            // Already exists in target, append entries
            auto& targetEntries = targetIt->second;
            targetEntries.insert(
                targetEntries.end(),
                std::make_move_iterator(entries.begin()),
                std::make_move_iterator(entries.end())
            );
        }

        // Empty the source entries to free memory immediately
        std::vector<CompactPathEntry>().swap(entries);

        // Update progress every batch
        entriesProcessed++;
        if (entriesProcessed % BATCH_SIZE == 0 || entriesProcessed == totalEntries) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsed == 0) elapsed = 1;

            float progress = static_cast<float>(entriesProcessed) / totalEntries;
            float entriesPerSecond = static_cast<float>(entriesProcessed) / elapsed;

            int eta = 0;
            if (progress > 0.01f) {
                eta = static_cast<int>((1.0f - progress) * elapsed / progress);
            }

            std::cout << "\rMerge: " << std::fixed << std::setprecision(1)
                << (progress * 100.0f) << "% ("
                << entriesProcessed << "/" << totalEntries
                << "), " << static_cast<int>(entriesPerSecond) << " entries/s"
                << ", ETA: " << eta << "s" << std::flush;
        }
    }

    // Clear the source table completely to free memory
    PropertyPathTable empty;
    source.swap(empty);

    std::cout << std::endl;
}

// =================== MAIN PROCESSING FUNCTION ===================

// Process ingredients recursively, one first-ingredient at a time
PropertyPathTable processIngredientBatch(
    int firstIngredient,
    int targetDepth,
    const std::vector<Property*>& initialProperties,
    int numThreads
) {
    PropertyPathTable batchResult;
    std::string firstIngName = ingredientByBitPosition[firstIngredient];
    Property* firstProp = getPropertyByNameOrId(ingredientPropertyMapping[firstIngName]);

    if (!firstProp) {
        return batchResult; // Empty result if ingredient not found
    }

    // Apply first ingredient
    std::vector<Property*> firstProps = PropertyMixCalculator::mixProperties(
        initialProperties, firstProp, DrugType::Marijuana);

    // Create stack state with first ingredient
    std::vector<uint8_t> startSeq = { static_cast<uint8_t>(firstIngredient) };

    // If target depth is 1, we're done
    if (targetDepth == 1) {
        float baseValueSum = 0.0f;
        float addictiveness = 0.0f;
        float valueMultiplier = 1.0f;

        for (auto* p : firstProps) {
            baseValueSum += p->addBaseValueMultiple;
            addictiveness += p->addictiveness;
            valueMultiplier *= p->valueMultiplier;
        }

        CompactPathEntry entry;
        entry.ingredientSequence = startSeq;
        entry.baseValueBonus = baseValueSum;
        entry.addictiveness = addictiveness;
        entry.valueMultiplier = valueMultiplier;

        PropertySet propBits = propertiesToBitset(firstProps);
        batchResult[propBits].push_back(entry);
        return batchResult;
    }

    // For depth > 1, process in parallel
    std::vector<std::thread> workers;
    std::vector<PropertyPathTable> threadResults(numThreads);
    std::atomic<size_t> sequencesProcessed(0);
    std::atomic<int> completedThreads(0);

    // Calculate how many ingredients each thread should handle
    size_t totalIngredients = ingredientByBitPosition.size();
    size_t ingredientsPerThread = (totalIngredients + numThreads - 1) / numThreads;

    // Launch threads
    for (int t = 0; t < numThreads; t++) {
        size_t startIdx = t * ingredientsPerThread;
        if (startIdx >= totalIngredients) continue;

        size_t endIdx = std::min(startIdx + ingredientsPerThread, totalIngredients);

        workers.push_back(std::thread([&, startIdx, endIdx, t]() {
            // Process each 2nd-level ingredient in this thread's range
            for (size_t secondIdx = startIdx; secondIdx < endIdx; secondIdx++) {
                std::string secondIngName = ingredientByBitPosition[secondIdx];
                Property* secondProp = getPropertyByNameOrId(ingredientPropertyMapping[secondIngName]);

                if (!secondProp) continue;

                // Apply second ingredient
                std::vector<Property*> secondProps = PropertyMixCalculator::mixProperties(
                    firstProps, secondProp, DrugType::Marijuana);

                // Start sequence with first and second ingredients
                std::vector<uint8_t> currentSeq = startSeq;
                currentSeq.push_back(secondIdx);

                // Stack-based processing for depth > 2
                if (targetDepth > 2) {
                    struct StackState {
                        std::vector<uint8_t> sequence;
                        std::vector<Property*> properties;
                        size_t depth;

                        StackState(const std::vector<uint8_t>& seq, const std::vector<Property*>& props, size_t d)
                            : sequence(seq), properties(props), depth(d) {}
                    };

                    std::stack<StackState> dfsStack;
                    dfsStack.push(StackState(currentSeq, secondProps, 2));

                    while (!dfsStack.empty()) {
                        StackState current = dfsStack.top();
                        dfsStack.pop();

                        // If reached target depth, add to results
                        if (current.depth == targetDepth) {
                            float baseValueSum = 0.0f;
                            float addictiveness = 0.0f;
                            float valueMultiplier = 1.0f;

                            for (auto* p : current.properties) {
                                baseValueSum += p->addBaseValueMultiple;
                                addictiveness += p->addictiveness;
                                valueMultiplier *= p->valueMultiplier;
                            }

                            CompactPathEntry entry;
                            entry.ingredientSequence = current.sequence;
                            entry.baseValueBonus = baseValueSum;
                            entry.addictiveness = addictiveness;
                            entry.valueMultiplier = valueMultiplier;

                            PropertySet propBits = propertiesToBitset(current.properties);
                            threadResults[t][propBits].push_back(entry);
                            sequencesProcessed++;
                            continue;
                        }

                        // Try each next ingredient
                        for (size_t nextIdx = 0; nextIdx < totalIngredients; nextIdx++) {
                            std::string nextIngName = ingredientByBitPosition[nextIdx];
                            Property* nextProp = getPropertyByNameOrId(ingredientPropertyMapping[nextIngName]);

                            if (nextProp) {
                                std::vector<Property*> nextProps = PropertyMixCalculator::mixProperties(
                                    current.properties, nextProp, DrugType::Marijuana);

                                std::vector<uint8_t> nextSeq = current.sequence;
                                nextSeq.push_back(nextIdx);

                                dfsStack.push(StackState(nextSeq, nextProps, current.depth + 1));
                            }
                        }
                    }
                }
                else {
                    // For depth == 2, add directly to results
                    float baseValueSum = 0.0f;
                    float addictiveness = 0.0f;
                    float valueMultiplier = 1.0f;

                    for (auto* p : secondProps) {
                        baseValueSum += p->addBaseValueMultiple;
                        addictiveness += p->addictiveness;
                        valueMultiplier *= p->valueMultiplier;
                    }

                    CompactPathEntry entry;
                    entry.ingredientSequence = currentSeq;
                    entry.baseValueBonus = baseValueSum;
                    entry.addictiveness = addictiveness;
                    entry.valueMultiplier = valueMultiplier;

                    PropertySet propBits = propertiesToBitset(secondProps);
                    threadResults[t][propBits].push_back(entry);
                    sequencesProcessed++;
                }
            }
        completedThreads++;
            }));
    }

    // Progress monitoring with ETA
    std::thread progressThread([&]() {
        const int barWidth = 50;
    auto startTime = std::chrono::steady_clock::now();
    size_t lastCount = 0;

    // Update every 2 seconds
    const auto updateInterval = std::chrono::seconds(2);
    auto lastUpdateTime = startTime;

    while (completedThreads < workers.size()) {
        auto now = std::chrono::steady_clock::now();

        // Only update display at defined intervals
        if (now - lastUpdateTime < updateInterval) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        auto updateElapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdateTime).count();
        if (updateElapsed == 0) updateElapsed = 1; // Avoid division by zero

        // Calculate sequences per second
        size_t currentCount = sequencesProcessed.load();
        size_t seqPerSecond = (currentCount - lastCount) / updateElapsed;
        lastCount = currentCount;
        lastUpdateTime = now;

        // Progress and ETA calculation
        float progress = static_cast<float>(completedThreads) / workers.size();
        int pos = static_cast<int>(barWidth * progress);

        // Calculate ETA
        int eta = 0;
        if (progress > 0.01f && seqPerSecond > 0) {
            eta = static_cast<int>((1.0f - progress) * elapsed / progress);
        }

        int etaHrs = eta / 3600;
        int etaMins = (eta % 3600) / 60;
        int etaSecs = eta % 60;

        std::cout << "\r[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }

        std::cout << "] ";
        std::cout << std::fixed << std::setprecision(1) << (progress * 100.0) << "%";
        std::cout << " Threads: " << completedThreads << "/" << workers.size();
        std::cout << " Speed: " << seqPerSecond << "/s";
        std::cout << " ETA: " << std::setw(2) << std::setfill('0') << etaHrs << ":"
            << std::setw(2) << std::setfill('0') << etaMins << ":"
            << std::setw(2) << std::setfill('0') << etaSecs;
        std::cout << std::flush;
    }

    // Final bar - simplified
    std::cout << "\r[";
    for (int i = 0; i < barWidth; ++i) std::cout << "=";
    std::cout << "] 100% - Ingredient " << firstIngredient << " complete! ";
    std::cout << sequencesProcessed << " sequences processed";
    std::cout << std::endl;
        });

    // Wait for all worker threads
    for (auto& thread : workers) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    // Wait for progress thread
    if (progressThread.joinable()) {
        progressThread.join();
    }

    // Merge thread results into batch result
    for (int t = 0; t < numThreads; t++) {
        mergePathTables(batchResult, threadResults[t]);
    }

    // Filter and sort the batch result
    filterAndSortPathTable(batchResult);

    return batchResult;
}

// Memory-efficient approach for generating combinations with incremental processing
PropertyPathTable findAllPaths(int maxIngredientCount, int numThreads, const std::string& productName = "") {
    PropertyPathTable globalPathTable;
    std::vector<Property*> initialProperties;

    // Get initial properties from product if provided
    if (!productName.empty()) {
        auto it = products.find(productName);
        if (it != products.end()) {
            initialProperties = it->second->properties;
            std::cout << "Starting with " << productName << " properties:" << std::endl;
            for (auto* prop : initialProperties) {
                std::cout << " - " << prop->name << std::endl;
            }
        }
    }

    // Process one ingredient count at a time
    for (int ingredientCount = 1; ingredientCount <= maxIngredientCount; ingredientCount++) {
        std::cout << "\n========== Processing " << ingredientCount << " ingredient combinations ==========" << std::endl;

        // For 1-ingredient paths, use simple processing
        if (ingredientCount == 1) {
            processSingleIngredientCombinations(globalPathTable, initialProperties);

            // Save progress
            std::string finalFile = "paths_" +
                (productName.empty() ? "none" : productName) +
                "_" + std::to_string(ingredientCount) + ".dat";

            saveBinaryPathTable(globalPathTable, finalFile);
            std::cout << "Completed 1-ingredient combinations." << std::endl;
            std::cout << "Current unique property combinations: " << globalPathTable.size() << std::endl;
            continue;
        }

        // For multi-ingredient paths, process one first-ingredient at a time
        size_t totalIngredients = ingredientByBitPosition.size();
        PropertyPathTable incrementalResults;

        auto startTime = std::chrono::steady_clock::now();

        for (size_t firstIdx = 0; firstIdx < totalIngredients; firstIdx++) {
            std::cout << "\nProcessing first ingredient " << firstIdx + 1 << "/" << totalIngredients
                << " (" << ingredientByBitPosition[firstIdx] << ")" << std::endl;

            // Process this batch
            PropertyPathTable batchResult = processIngredientBatch(
                firstIdx, ingredientCount, initialProperties, numThreads);

            // Get interim batch size
            size_t batchCombinations = batchResult.size();

            // Merge this batch into incremental results
            std::cout << "Merging batch into incremental results..." << std::endl;
            mergePathTables(incrementalResults, batchResult);

            // Save interim results after each first ingredient
            std::string interimFile = "paths_" +
                (productName.empty() ? "none" : productName) +
                "_" + std::to_string(ingredientCount) +
                "_interim_" + std::to_string(firstIdx) + ".dat";

            saveBinaryPathTable(incrementalResults, interimFile);

            // Calculate overall progress
            float overallProgress = (firstIdx + 1.0f) / totalIngredients;

            // Calculate ETA
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            int eta = 0;
            if (overallProgress > 0.01f) {
                eta = static_cast<int>((1.0f - overallProgress) * elapsed / overallProgress);
            }

            int etaHrs = eta / 3600;
            int etaMins = (eta % 3600) / 60;
            int etaSecs = eta % 60;

            std::cout << "Overall progress: " << std::fixed << std::setprecision(1)
                << (overallProgress * 100.0) << "%" << std::endl;
            std::cout << "ETA: " << etaHrs << ":"
                << std::setw(2) << std::setfill('0') << etaMins << ":"
                << std::setw(2) << std::setfill('0') << etaSecs << std::endl;
            std::cout << "Current unique property combinations: " << incrementalResults.size()
                << " (+" << batchCombinations << " from this batch)" << std::endl;
        }

        // Final filtering and sorting
        std::cout << "\nFinalizing results for " << ingredientCount << " ingredient combinations..." << std::endl;
        filterAndSortPathTable(incrementalResults);

        // Merge with global results
        mergePathTables(globalPathTable, incrementalResults);

        // Save final results for this ingredient count
        std::string finalFile = "paths_" +
            (productName.empty() ? "none" : productName) +
            "_" + std::to_string(ingredientCount) + ".dat";

        saveBinaryPathTable(globalPathTable, finalFile);
        std::cout << "Completed " << ingredientCount << " ingredient combinations." << std::endl;
        std::cout << "Total unique property combinations: " << globalPathTable.size() << std::endl;
    }

    return globalPathTable;
}

// =================== PATH SEARCH FUNCTION ===================

// Find paths for desired properties
void findPathsForDesiredProperties(const PropertyPathTable& table, const std::vector<std::string>& desiredPropertyIds) {
    std::cout << "Finding paths for properties: ";
    for (const auto& id : desiredPropertyIds) {
        std::cout << id << " ";
    }
    std::cout << std::endl;

    // Convert desired property IDs to a bitset
    PropertySet desiredBits = 0;
    std::vector<Property*> desiredProps;

    for (const auto& id : desiredPropertyIds) {
        Property* prop = getPropertyByNameOrId(id);
        if (prop) {
            desiredBits |= propertyToBit(prop);
            desiredProps.push_back(prop);
            std::cout << " - " << prop->name << " (Tier " << prop->tier << ")" << std::endl;
        }
        else {
            std::cout << "Warning: Unknown property '" << id << "'" << std::endl;
        }
    }

    if (desiredProps.empty()) {
        std::cout << "No valid properties specified." << std::endl;
        return;
    }

    // Find matching paths
    std::vector<std::pair<PropertySet, CompactPathEntry>> matchingPaths;

    for (const auto& [propBits, entries] : table) {
        // Check if all desired bits are present
        if ((propBits & desiredBits) == desiredBits) {
            // Add all entries for this property combination
            for (const auto& entry : entries) {
                matchingPaths.emplace_back(propBits, entry);
            }
        }
    }

    // Sort by ingredient count only (fewer = better)
    std::sort(matchingPaths.begin(), matchingPaths.end(),
        [](const auto& a, const auto& b) {
            return a.second.ingredientSequence.size() < b.second.ingredientSequence.size();
        });

    // Display results
    if (matchingPaths.empty()) {
        std::cout << "No paths found that contain all specified properties." << std::endl;
        return;
    }

    std::cout << "\nFound " << matchingPaths.size() << " paths. Showing top 5:" << std::endl;

    int shown = 0;
    for (const auto& [propBits, entry] : matchingPaths) {
        if (shown >= 5) break;

        // Convert ingredient indices to names
        std::vector<std::string> ingredientNames;
        for (uint8_t idx : entry.ingredientSequence) {
            if (idx < ingredientByBitPosition.size()) {
                ingredientNames.push_back(ingredientByBitPosition[idx]);
            }
        }

        // Get all properties
        std::vector<Property*> allProps = bitsetToProperties(propBits);

        std::cout << "\nPath " << (shown + 1) << " (" << ingredientNames.size() << " ingredients):" << std::endl;
        std::cout << "Ingredients (in order): ";
        for (const auto& name : ingredientNames) {
            std::cout << name << " ";
        }
        std::cout << std::endl;

        std::cout << "Properties (" << allProps.size() << "): ";
        for (auto* prop : allProps) {
            // Highlight desired properties
            bool isDesired = std::find(desiredProps.begin(), desiredProps.end(), prop) != desiredProps.end();
            if (isDesired) {
                std::cout << "[" << prop->name << "] ";
            }
            else {
                std::cout << prop->name << " ";
            }
        }
        std::cout << std::endl;

        std::cout << "Base Value Bonus: " << entry.baseValueBonus << std::endl;
        std::cout << "Addictiveness: " << entry.addictiveness << std::endl;
        std::cout << "Value Multiplier: " << entry.valueMultiplier << std::endl;

        shown++;
    }
}

// Free allocated memory
void cleanup() {
    for (auto& pair : products) {
        delete pair.second;
    }
    products.clear();
}

// =================== MAIN FUNCTION ===================

int main() {
    // Initialize the property system
    initializeGameSystem();
    initializeProducts();
    initializeBitMappings();

    std::cout << "===== Schedule I Property Path Generator (Optimized) =====" << std::endl;

    // Ask user which product to start with
    std::cout << "\nAvailable products:" << std::endl;
    for (const auto& pair : products) {
        std::cout << " - " << pair.first << " (" << pair.second->rank << ")" << std::endl;
    }

    std::string productName = "";  // Empty for no starting product
    std::cout << "\nEnter starting product (or press Enter for none): ";
    std::getline(std::cin, productName);

    // Check if path table already exists for this product
    std::string filename = "paths_" + (productName.empty() ? "none" : productName) + ".dat";
    PropertyPathTable pathTable;

    std::ifstream testFile(filename);
    if (testFile.good()) {
        testFile.close();

        char response;
        std::cout << "Found existing path data for this product. Load it? (y/n): ";
        std::cin >> response;
        std::cin.ignore();  // Clear newline

        if (response == 'y' || response == 'Y') {
            pathTable = loadBinaryPathTable(filename);
        }
        else {
            // Generate new table
            int maxIngredientCount;
            int threads;

            std::cout << "Enter maximum number of ingredients (recommended 3-4 for first run): ";
            std::cin >> maxIngredientCount;

            std::cout << "Enter number of threads to use: ";
            std::cin >> threads;

            std::cin.ignore(); // Clear newline

            pathTable = findAllPaths(maxIngredientCount, threads, productName);
            saveBinaryPathTable(pathTable, filename);
        }
    }
    else {
        // Generate new table
        int maxIngredientCount;
        int threads;

        std::cout << "Enter maximum number of ingredients (recommended 3-4 for first run): ";
        std::cin >> maxIngredientCount;

        std::cout << "Enter number of threads to use: ";
        std::cin >> threads;

        std::cin.ignore(); // Clear newline

        pathTable = findAllPaths(maxIngredientCount, threads, productName);
        saveBinaryPathTable(pathTable, filename);
    }

    // Property search interface
    while (true) {
        std::cout << "\n=== Property Path Finder ===" << std::endl;
        std::cout << "Enter property IDs to search for (comma-separated), or 'quit' to exit:" << std::endl;
        std::cout << "Example: energizing,foggy,spicy" << std::endl;

        std::string input;
        std::getline(std::cin, input);

        if (input == "quit" || input == "exit") {
            break;
        }

        // Split input by commas
        std::vector<std::string> propertyIds;
        std::stringstream ss(input);
        std::string id;

        while (std::getline(ss, id, ',')) {
            // Trim whitespace
            id.erase(0, id.find_first_not_of(" \t"));
            id.erase(id.find_last_not_of(" \t") + 1);

            if (!id.empty()) {
                propertyIds.push_back(id);
            }
        }

        if (propertyIds.empty()) {
            std::cout << "No properties specified." << std::endl;
            continue;
        }

        findPathsForDesiredProperties(pathTable, propertyIds);
    }

    // Clean up
    cleanup();

    return 0;
}