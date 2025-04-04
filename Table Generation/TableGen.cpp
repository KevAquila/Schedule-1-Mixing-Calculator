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

// Count bits set in a number (population count)
int countBits(uint64_t bits) {
    int count = 0;
    while (bits != 0) {
        count += bits & 1;
        bits >>= 1;
    }
    return count;
}

// Generate combinations of k items from n
std::vector<std::vector<uint8_t>> generateCombinations(int k, int n) {
    std::vector<std::vector<uint8_t>> combinations;

    std::vector<bool> bitmask(n, false);
    std::fill(bitmask.begin(), bitmask.begin() + k, true);

    do {
        std::vector<uint8_t> combo;
        for (int i = 0; i < n; i++) {
            if (bitmask[i]) {
                combo.push_back(i);
            }
        }
        combinations.push_back(combo);
    } while (std::prev_permutation(bitmask.begin(), bitmask.end()));

    return combinations;
}

// Progress tracking
std::atomic<size_t> permutationsDone = 0;
std::mutex resultMutex;

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

// Process a chunk of combinations
PropertyPathTable processWorkerChunk(const std::vector<std::vector<uint8_t>>& combinations,
    const std::vector<Property*>& initialProperties) {
    PropertyPathTable pathTable;

    // For each combination, generate all permutations
    for (const auto& combo : combinations) {
        std::vector<uint8_t> perm = combo;

        // Process all permutations of this combination
        do {
            // Start with initial properties
            std::vector<Property*> props = initialProperties;

            // Apply each ingredient in sequence
            for (uint8_t idx : perm) {
                std::string ingName = ingredientByBitPosition[idx];
                Property* newProp = getPropertyByNameOrId(ingredientPropertyMapping[ingName]);
                if (newProp) {
                    props = PropertyMixCalculator::mixProperties(props, newProp, DrugType::Marijuana);
                }
            }

            // Calculate statistics
            float baseValueSum = 0.0f;
            float addictiveness = 0.0f;
            float valueMultiplier = 1.0f;

            for (auto* p : props) {
                baseValueSum += p->addBaseValueMultiple;
                addictiveness += p->addictiveness;
                valueMultiplier *= p->valueMultiplier;
            }

            // Create entry
            PropertySet propBits = propertiesToBitset(props);
            CompactPathEntry entry;
            entry.ingredientSequence = perm;  // Store actual sequence
            entry.baseValueBonus = baseValueSum;
            entry.addictiveness = addictiveness;
            entry.valueMultiplier = valueMultiplier;

            // Add to result
            pathTable[propBits].push_back(entry);
            permutationsDone++;

        } while (std::next_permutation(perm.begin(), perm.end()));
    }

    // Filter and sort entries
    for (auto& [propBits, entries] : pathTable) {
        // Sort by sequence length (fewer = better)
        std::sort(entries.begin(), entries.end(),
            [](const CompactPathEntry& a, const CompactPathEntry& b) {
                if (a.ingredientSequence.size() == b.ingredientSequence.size()) {
                    return a.baseValueBonus > b.baseValueBonus;
                }
        return a.ingredientSequence.size() < b.ingredientSequence.size();
            });

        // Keep only the shortest paths
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

    return pathTable;
}

// Merge two path tables
void mergePathTables(PropertyPathTable& target, const PropertyPathTable& source) {
    for (const auto& [propBits, entries] : source) {
        // If property set not in target, add all entries
        if (target.find(propBits) == target.end()) {
            target[propBits] = entries;
        }
        else {
            // Otherwise, merge entries
            auto& targetEntries = target[propBits];

            // Add source entries
            targetEntries.insert(targetEntries.end(), entries.begin(), entries.end());

            // Re-sort combined entries
            std::sort(targetEntries.begin(), targetEntries.end(),
                [](const CompactPathEntry& a, const CompactPathEntry& b) {
                    if (a.ingredientSequence.size() == b.ingredientSequence.size()) {
                        return a.baseValueBonus > b.baseValueBonus;
                    }
            return a.ingredientSequence.size() < b.ingredientSequence.size();
                });

            // Keep only shortest paths
            size_t shortestLength = targetEntries[0].ingredientSequence.size();
            targetEntries.erase(
                std::remove_if(targetEntries.begin(), targetEntries.end(),
                    [shortestLength](const CompactPathEntry& entry) {
                        return entry.ingredientSequence.size() > shortestLength;
                    }),
                targetEntries.end()
                        );

            // Limit to top 5
            if (targetEntries.size() > 5) {
                targetEntries.resize(5);
            }
        }
    }
}

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
void mergeResultsInParallel(PropertyPathTable& globalTable, std::vector<PropertyPathTable>& threadResults, int numThreads);
// Hybrid approach for path generation with parallel merging
PropertyPathTable findAllPathsHybrid(int maxIngredientCount, int numThreads,
    const std::string& productName = "") {

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
        std::cout << "\nProcessing " << ingredientCount << " ingredient combinations..." << std::endl;

        // Generate all combinations with exactly 'ingredientCount' ingredients
        std::vector<std::vector<uint8_t>> combinations = generateCombinations(ingredientCount, ingredientByBitPosition.size());

        std::cout << "Generated " << combinations.size() << " combinations with "
            << ingredientCount << " ingredients." << std::endl;

        // Calculate total permutations for progress tracking
        size_t totalPermutations = 0;
        for (size_t i = 0; i < combinations.size(); i++) {
            // Number of permutations for this combination is k!
            size_t permCount = 1;
            for (int j = 2; j <= ingredientCount; j++) {
                permCount *= j;
            }
            totalPermutations += permCount;
        }

        permutationsDone = 0;

        std::cout << "Processing " << totalPermutations << " permutations..." << std::endl;

        // Split work among threads
        size_t batchSize = (combinations.size() + numThreads - 1) / numThreads;
        std::vector<std::future<PropertyPathTable>> futures;

        // Start progress thread
        std::thread progressThread(displayProgressBar, totalPermutations);

        // Launch worker threads
        for (int t = 0; t < numThreads; t++) {
            size_t start = t * batchSize;
            if (start >= combinations.size()) break;

            size_t end = std::min(start + batchSize, combinations.size());
            std::vector<std::vector<uint8_t>> chunk(combinations.begin() + start, combinations.begin() + end);

            futures.push_back(std::async(std::launch::async,
                [chunk, initialProperties]() {
                    return processWorkerChunk(chunk, initialProperties);
                }
            ));
        }

        // Collect results into a vector
        std::vector<PropertyPathTable> threadResults;
        for (auto& f : futures) {
            threadResults.push_back(f.get());
        }

        // Wait for progress thread
        progressThread.join();

        // Merge results in parallel
        mergeResultsInParallel(globalPathTable, threadResults, numThreads);

        // Clear combinations to free memory
        std::vector<std::vector<uint8_t>> emptyCombos;
        combinations.swap(emptyCombos);

        // Save intermediate results
        std::string intermediateFile = "paths_" +
            (productName.empty() ? "none" : productName) +
            "_" + std::to_string(ingredientCount) + ".dat";

        saveBinaryPathTable(globalPathTable, intermediateFile);

        std::cout << "Completed " << ingredientCount << " ingredient combinations." << std::endl;
        std::cout << "Current unique property combinations: " << globalPathTable.size() << std::endl;
    }

    return globalPathTable;
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

    // Sort by ingredient count (fewer = better)
    std::sort(matchingPaths.begin(), matchingPaths.end(),
        [](const auto& a, const auto& b) {
            if (a.second.ingredientSequence.size() == b.second.ingredientSequence.size()) {
                return a.second.baseValueBonus > b.second.baseValueBonus;
            }
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



// Improved merging function that works in parallel
void mergeResultsInParallel(PropertyPathTable& globalTable, std::vector<PropertyPathTable>& threadResults, int numThreads) {
    std::cout << "Merging results from " << threadResults.size() << " threads..." << std::endl;

    // First, gather all unique property bitsets
    std::vector<PropertySet> allPropertySets;
    std::mutex setsMutex;

#pragma omp parallel for num_threads(numThreads)
    for (size_t i = 0; i < threadResults.size(); i++) {
        std::vector<PropertySet> threadSets;
        for (const auto& [propBits, _] : threadResults[i]) {
            threadSets.push_back(propBits);
        }

        // Add to global list
        {
            std::lock_guard<std::mutex> lock(setsMutex);
            allPropertySets.insert(allPropertySets.end(), threadSets.begin(), threadSets.end());
        }
    }

    // Remove duplicates
    std::sort(allPropertySets.begin(), allPropertySets.end());
    allPropertySets.erase(std::unique(allPropertySets.begin(), allPropertySets.end()), allPropertySets.end());

    std::cout << "Found " << allPropertySets.size() << " unique property combinations." << std::endl;

    // Process each property set in parallel
    std::atomic<size_t> processedSets(0);
    size_t totalSets = allPropertySets.size();
    std::mutex globalTableMutex;

    // Start progress thread for merging
    std::thread progressThread([&processedSets, totalSets]() {
        const int barWidth = 50;
    while (processedSets < totalSets) {
        float progress = static_cast<float>(processedSets) / totalSets;
        int pos = static_cast<int>(barWidth * progress);

        std::cout << "\r[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << std::fixed << std::setprecision(1) << (progress * 100.0) << "%";
        std::cout << " (" << processedSets << "/" << totalSets << ")";
        std::cout << std::flush;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Final bar
    std::cout << "\r[";
    for (int i = 0; i < barWidth; ++i) std::cout << "=";
    std::cout << "] 100.0% (" << totalSets << "/" << totalSets << ")" << std::endl;
        });

    // Process property sets in chunks
    const size_t chunkSize = (allPropertySets.size() + numThreads - 1) / numThreads;
    std::vector<std::future<void>> mergeFutures;

    for (size_t t = 0; t < numThreads; t++) {
        size_t start = t * chunkSize;
        if (start >= allPropertySets.size()) break;

        size_t end = std::min(start + chunkSize, allPropertySets.size());

        mergeFutures.push_back(std::async(std::launch::async, [&, start, end]() {
            // Local table for this thread
            PropertyPathTable localTable;

        // Process assigned property sets
        for (size_t i = start; i < end; i++) {
            PropertySet propBits = allPropertySets[i];
            std::vector<CompactPathEntry> allEntries;

            // Gather entries from all thread results
            for (auto& threadTable : threadResults) {
                auto it = threadTable.find(propBits);
                if (it != threadTable.end()) {
                    allEntries.insert(allEntries.end(), it->second.begin(), it->second.end());
                }
            }

            // Skip if no entries found (shouldn't happen, but just in case)
            if (allEntries.empty()) {
                processedSets++;
                continue;
            }

            // Sort and filter entries
            std::sort(allEntries.begin(), allEntries.end(),
                [](const CompactPathEntry& a, const CompactPathEntry& b) {
                    if (a.ingredientSequence.size() == b.ingredientSequence.size()) {
                        return a.baseValueBonus > b.baseValueBonus;
                    }
            return a.ingredientSequence.size() < b.ingredientSequence.size();
                });

            // Keep only shortest paths
            size_t shortestLength = allEntries[0].ingredientSequence.size();
            allEntries.erase(
                std::remove_if(allEntries.begin(), allEntries.end(),
                    [shortestLength](const CompactPathEntry& entry) {
                        return entry.ingredientSequence.size() > shortestLength;
                    }),
                allEntries.end()
                        );

            // Limit to top 5
            if (allEntries.size() > 5) {
                allEntries.resize(5);
            }

            // Add to local table
            localTable[propBits] = allEntries;

            // Update progress
            processedSets++;
        }

        // Merge local table with global table
        {
            std::lock_guard<std::mutex> lock(globalTableMutex);
            for (const auto& [propBits, entries] : localTable) {
                globalTable[propBits] = entries;
            }
        }
            }));
    }

    // Wait for all merge operations to complete
    for (auto& f : mergeFutures) {
        f.wait();
    }

    // Wait for progress thread
    progressThread.join();

    // Clear thread results to free memory
    for (auto& threadTable : threadResults) {
        PropertyPathTable empty;
        threadTable.swap(empty);
    }

    std::cout << "Merge complete. Final table contains " << globalTable.size() << " entries." << std::endl;
}


// Free allocated memory
void cleanup() {
    for (auto& pair : products) {
        delete pair.second;
    }
    products.clear();
}

int main() {
    // Initialize the property system
    initializeGameSystem();
    initializeProducts();
    initializeBitMappings();

    std::cout << "===== Schedule I Property Path Generator (Hybrid) =====" << std::endl;

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
            int maxIngredientCount = 8;  // Maximum number of ingredients to use
            int threads = 24;            // Number of threads to use

            pathTable = findAllPathsHybrid(maxIngredientCount, threads, productName);
            saveBinaryPathTable(pathTable, filename);
        }
    }
    else {
        // Generate new table
        int maxIngredientCount = 8;  // Maximum number of ingredients to use
        int threads = 24;            // Number of threads to use

        pathTable = findAllPathsHybrid(maxIngredientCount, threads, productName);
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