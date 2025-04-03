#include "property_mixer_core.h"
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
        std::cout << std::flush;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Final bar
    std::cout << "\r[";
    for (int i = 0; i < barWidth; ++i) std::cout << "=";
    std::cout << "] 100.00%  ETA: 00:00:00\n";
}

struct MixResult {
    float baseValueBonus;
    float addictiveness;
    float valueMultiplier;
    std::vector<std::string> ingredients;
    std::vector<Property*> properties;
};

// Worker function to find best mix
MixResult findBestMixWorker(const std::vector<std::vector<std::string>>& subsets,
    const std::vector<Property*>& initialProperties = std::vector<Property*>()) {
    MixResult best{ -1.0f, 0.0f, 1.0f, {}, {} };

    for (const auto& subset : subsets) {
        std::vector<std::string> perm = subset;
        std::sort(perm.begin(), perm.end());

        do {
            // Start with initial properties if provided
            std::vector<Property*> props = initialProperties;

            for (const auto& ing : perm) {
                Property* newProp = getPropertyByNameOrId(ingredientPropertyMapping[ing]);
                props = PropertyMixCalculator::mixProperties(props, newProp, DrugType::Marijuana);
            }

            float baseValueSum = 0.0f;
            float addictiveness = 0.0f;
            float valueMultiplier = 1.0f;

            for (auto* p : props) {
                baseValueSum += p->addBaseValueMultiple;
                addictiveness += p->addictiveness;
                valueMultiplier *= p->valueMultiplier;
            }

            if (baseValueSum > best.baseValueBonus) {
                best.baseValueBonus = baseValueSum;
                best.addictiveness = addictiveness;
                best.valueMultiplier = valueMultiplier;
                best.ingredients = perm;
                best.properties = props;
            }
            permutationsDone++;
        } while (std::next_permutation(perm.begin(), perm.end()));
    }

    return best;
}

// Multi-threaded optimization function
MixResult findBestMixMultithreaded(int ingredientCount, int numThreads,
    const std::string& productName = "") {
    std::vector<Property*> initialProperties;

    // If a product name is provided, use its initial properties
    if (!productName.empty()) {
        auto it = products.find(productName);
        if (it != products.end()) {
            initialProperties = it->second->properties;
            std::cout << "Starting with " << productName << " properties:" << std::endl;
            for (auto* prop : initialProperties) {
                std::cout << " - " << prop->name << std::endl;
            }
        }
        else {
            std::cout << "Product not found: " << productName << std::endl;
        }
    }

    std::vector<std::string> allIngredients;
    for (const auto& pair : ingredientPropertyMapping) {
        allIngredients.push_back(pair.first);
    }

    std::vector<std::vector<std::string>> allSubsets;
    int n = allIngredients.size();

    // Generate all combinations of specified size
    std::vector<bool> mask(n, false);
    std::fill(mask.begin(), mask.begin() + ingredientCount, true);
    do {
        std::vector<std::string> subset;
        for (int i = 0; i < n; ++i) {
            if (mask[i]) subset.push_back(allIngredients[i]);
        }
        allSubsets.push_back(subset);
    } while (std::prev_permutation(mask.begin(), mask.end()));

    // Split into thread chunks
    size_t total = allSubsets.size();
    size_t chunkSize = (total + numThreads - 1) / numThreads;

    std::vector<std::future<MixResult>> futures;

    size_t totalPermutations = allSubsets.size() * std::tgamma(ingredientCount + 1); // n! = tgamma(n+1)
    permutationsDone = 0;

    std::thread progressThread(displayProgressBar, totalPermutations);

    for (int t = 0; t < numThreads; ++t) {
        size_t start = t * chunkSize;
        if (start >= total) break;  // Nothing left to process
        size_t end = std::min(start + chunkSize, total);

        std::vector<std::vector<std::string>> chunk(allSubsets.begin() + start, allSubsets.begin() + end);

        futures.push_back(std::async(std::launch::async,
            [chunk, initialProperties]() {
                return findBestMixWorker(chunk, initialProperties);
            }
        ));
    }

    // Collect best from all threads
    MixResult globalBest{ -1.0f, 0.0f, 1.0f, {}, {} };
    for (auto& f : futures) {
        MixResult res = f.get();
        if (res.baseValueBonus > globalBest.baseValueBonus) {
            globalBest = res;
        }
    }
    progressThread.join();

    return globalBest;
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

    // Initialize products
    initializeProducts();

    std::cout << "===== Schedule I Property Mixer Optimizer =====" << std::endl;
    std::cout << "Finding the optimal ingredient combinations for different products." << std::endl;

    // List all available products
    std::cout << "\nAvailable products:" << std::endl;
    for (const auto& pair : products) {
        std::cout << " - " << pair.first << " (" << pair.second->rank << ")" << std::endl;
    }

    // Set optimization parameters here
    int ingredientCount = 8;  // Max number of ingredients to use
    int threads = 24;         // Number of threads to use (adjust based on your CPU)

    // Run optimization for each product
    for (const auto& pair : products) {
        if (pair.second->type == DrugType::Marijuana) {  // Only run for marijuana products
            std::string productName = pair.first;

            std::cout << "\n\n========================================" << std::endl;
            std::cout << "OPTIMIZATION FOR: " << productName << std::endl;
            std::cout << "========================================" << std::endl;

            auto result = findBestMixMultithreaded(ingredientCount, threads, productName);

            std::cout << "\n=== BEST MIX FOUND FOR " << productName << " ===\n";
            std::cout << "Ingredients:" << std::endl;
            for (const auto& ing : result.ingredients) {
                std::cout << " - " << ing << std::endl;
            }

            std::cout << "\nFinal Properties:" << std::endl;
            for (auto* p : result.properties) {
                std::cout << " - " << p->name << " (Tier " << p->tier
                    << ", Base Value: " << p->addBaseValueMultiple
                    << ", Addictiveness: " << p->addictiveness << ")\n";
            }

            std::cout << "\nSummary Stats:" << std::endl;
            std::cout << "Total Base Value Bonus: " << result.baseValueBonus << std::endl;
            std::cout << "Total Addictiveness: " << result.addictiveness << std::endl;
            std::cout << "Total Value Multiplier: " << result.valueMultiplier << std::endl;

            // Calculate final value ratio (base × multiplier)
            float finalValueFactor = (1.0f + result.baseValueBonus) * result.valueMultiplier;
            std::cout << "Final Value Factor: " << finalValueFactor << "× (base value)" << std::endl;
        }
    }

    // Also run for no starting product as a baseline
    std::cout << "\n\n========================================" << std::endl;
    std::cout << "OPTIMIZATION WITH NO STARTING PRODUCT" << std::endl;
    std::cout << "========================================" << std::endl;

    auto result = findBestMixMultithreaded(ingredientCount, threads);

    std::cout << "\n=== BEST MIX (NO STARTING PRODUCT) ===\n";
    std::cout << "Ingredients:" << std::endl;
    for (const auto& ing : result.ingredients) {
        std::cout << " - " << ing << std::endl;
    }

    std::cout << "\nFinal Properties:" << std::endl;
    for (auto* p : result.properties) {
        std::cout << " - " << p->name << " (Tier " << p->tier
            << ", Base Value: " << p->addBaseValueMultiple
            << ", Addictiveness: " << p->addictiveness << ")\n";
    }

    std::cout << "\nSummary Stats:" << std::endl;
    std::cout << "Total Base Value Bonus: " << result.baseValueBonus << std::endl;
    std::cout << "Total Addictiveness: " << result.addictiveness << std::endl;
    std::cout << "Total Value Multiplier: " << result.valueMultiplier << std::endl;

    // Calculate final value ratio (base × multiplier)
    float finalValueFactor = (1.0f + result.baseValueBonus) * result.valueMultiplier;
    std::cout << "Final Value Factor: " << finalValueFactor << "* (base value)" << std::endl;

    std::cin.get();
    // Clean up
    cleanup();

    return 0;
}