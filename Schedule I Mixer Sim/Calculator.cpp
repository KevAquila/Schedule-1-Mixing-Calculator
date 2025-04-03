#include "property_mixer_core.h"
#include "Visualizer.h"
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <numeric>
// Structure to track property origins
struct PropertyWithOrigin {
    Property* property;
    std::vector<std::string> ingredients;
};

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

// Reverse map for finding ingredient names from property IDs
std::map<std::string, std::string> propertyToIngredientMap;

// Function to display a stats bar
void displayStatsBar(float value, float maxValue, int width, const std::string& label) {
    int filledWidth = static_cast<int>(value / maxValue * width);

    std::cout << label << " [";
    for (int i = 0; i < width; i++) {
        if (i < filledWidth) {
            std::cout << "#";
        }
        else {
            std::cout << " ";
        }
    }
    std::cout << "] " << std::fixed << std::setprecision(2) << value << std::endl;
}
#include <algorithm> // for next_permutation
std::vector<std::string> findBestMix(int maxIngredients) {
    std::vector<std::string> bestCombo;
    float bestBaseValueBonus = -1.0f;

    std::vector<std::string> ingredientNames;
    for (const auto& pair : ingredientPropertyMapping) {
        ingredientNames.push_back(pair.first);
    }

    int total = ingredientNames.size();

    // Explore all subset sizes up to maxIngredients
    for (int subsetSize = 1; subsetSize <= maxIngredients; ++subsetSize) {
        std::vector<bool> mask(total, false);
        std::fill(mask.begin(), mask.begin() + subsetSize, true);

        do {
            // Collect subset
            std::vector<std::string> subset;
            for (int i = 0; i < total; ++i) {
                if (mask[i]) subset.push_back(ingredientNames[i]);
            }

            // Now try all permutations of this subset
            std::sort(subset.begin(), subset.end());
            do {
                // Simulate mix
                std::vector<Property*> currentProps;
                for (const std::string& ing : subset) {
                    Property* newProp = getPropertyByNameOrId(ingredientPropertyMapping[ing]);
                    currentProps = PropertyMixCalculator::mixProperties(currentProps, newProp, DrugType::Marijuana);
                }

                // Evaluate result
                float totalBaseValue = 0.0f;
                for (auto* prop : currentProps) {
                    totalBaseValue += prop->addBaseValueMultiple;
                }

                if (totalBaseValue > bestBaseValueBonus) {
                    bestBaseValueBonus = totalBaseValue;
                    bestCombo = subset;
                }

            } while (std::next_permutation(subset.begin(), subset.end()));

        } while (std::prev_permutation(mask.begin(), mask.end()));
    }

    return bestCombo;
}
#include <thread>
#include <mutex>
#include <future>
#include <numeric>
#include <algorithm>
#include <atomic>
#include <chrono>
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
    std::vector<std::string> ingredients;
};

MixResult findBestMixWorker(const std::vector<std::vector<std::string>>& subsets) {
    MixResult best{ -1.0f, {} };

    for (const auto& subset : subsets) {
        std::vector<std::string> perm = subset;
        std::sort(perm.begin(), perm.end());

        do {
            std::vector<Property*> props;
            for (const auto& ing : perm) {
                Property* newProp = getPropertyByNameOrId(ingredientPropertyMapping[ing]);
                props = PropertyMixCalculator::mixProperties(props, newProp, DrugType::Marijuana);
            }

            float sum = 0.0f;
            for (auto* p : props) {
                sum += p->addBaseValueMultiple;
            }

            if (sum > best.baseValueBonus) {
                best.baseValueBonus = sum;
                best.ingredients = perm;
            }
            permutationsDone++;
        } while (std::next_permutation(perm.begin(), perm.end()));
    }

    return best;
}

std::vector<std::string> findBestMixMultithreaded(int ingredientCount, int numThreads) {
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

        futures.push_back(std::async(std::launch::async, [chunk]() {
            return findBestMixWorker(chunk);
            }));
    }

    // Collect best from all threads
    MixResult globalBest{ -1.0f, {} };
    for (auto& f : futures) {
        MixResult res = f.get();
        if (res.baseValueBonus > globalBest.baseValueBonus) {
            globalBest = res;
        }
    }
    progressThread.join();

    return globalBest.ingredients;
}

int main() {
    // Initialize the property system
    initializeGameSystem();

    // Create reverse mapping from property ID to ingredient name
    for (const auto& pair : ingredientPropertyMapping) {
        propertyToIngredientMap[pair.second] = pair.first;
    }

    // Start with empty set of properties with origins
    std::vector<PropertyWithOrigin> currentPropertiesWithOrigin;

    // Track the history of added ingredients
    std::vector<std::string> ingredientHistory;

    // Welcome message
    std::cout << "===== Schedule I Property Mixer =====" << std::endl;
    std::cout << "This program simulates the property mixing system from the game." << std::endl;
    std::cout << "Add ingredients to see how properties combine according to the game's logic." << std::endl;


    /*for (int m = 0; m <= 7; m++)*/ {

        int ingredientCount = 10;
        int threads = 24; // Tailored to your 13900KF

        auto bestIngredients = findBestMixMultithreaded(ingredientCount, threads);

        std::cout << "\n=== BEST MIX FOUND ===\n";
        for (const auto& ing : bestIngredients) {
            std::cout << " - " << ing << std::endl;
        }

        // Recompute the final properties
        std::vector<Property*> finalProps;
        for (const auto& ing : bestIngredients) {
            Property* newProp = getPropertyByNameOrId(ingredientPropertyMapping[ing]);
            finalProps = PropertyMixCalculator::mixProperties(finalProps, newProp, DrugType::Marijuana);
        }

        float totalBase = 0.0f;
        std::cout << "\nProperties:" << std::endl;
        for (auto* p : finalProps) {
            std::cout << " - " << p->name << " (Tier " << p->tier << ", Base Value Bonus: " << p->addBaseValueMultiple << ")\n";
            totalBase += p->addBaseValueMultiple;
        }

        std::cout << "\nTotal Base Value Bonus: " << totalBase << std::endl;

    }

    // Main interactive loop
    std::string command;
    while (true) {
        // Calculate cumulative stats
        float totalAddictiveness = 0.0;
        float totalBaseValueMultiple = 0.0;
        float totalValueMultiplier = 1.0;
        int totalValueChange = 0;

        for (const auto& propWithOrigin : currentPropertiesWithOrigin) {
            totalAddictiveness += propWithOrigin.property->addictiveness;
            totalBaseValueMultiple += propWithOrigin.property->addBaseValueMultiple;
            totalValueMultiplier *= propWithOrigin.property->valueMultiplier;
            totalValueChange += propWithOrigin.property->valueChange;
        }

        // Print current properties
        std::cout << "\nCurrent properties:" << std::endl;
        if (currentPropertiesWithOrigin.empty()) {
            std::cout << "  (none)" << std::endl;
        }
        else {
            // Display properties with their stats
            for (size_t i = 0; i < currentPropertiesWithOrigin.size(); i++) {
                Property* prop = currentPropertiesWithOrigin[i].property;

                std::cout << "  " << (i + 1) << ". " << prop->name << " (Tier " << prop->tier << ")" << std::endl;

                // Display ingredient origins
                std::cout << "     Origin: ";
                if (currentPropertiesWithOrigin[i].ingredients.empty()) {
                    std::cout << "Direct ingredient";
                }
                else {
                    for (size_t j = 0; j < currentPropertiesWithOrigin[i].ingredients.size(); j++) {
                        std::cout << currentPropertiesWithOrigin[i].ingredients[j];
                        if (j < currentPropertiesWithOrigin[i].ingredients.size() - 1) {
                            std::cout << " + ";
                        }
                    }
                }
                std::cout << std::endl;

                // Display individual property stats
                std::cout << "     Addictiveness: " << prop->addictiveness << std::endl;
                std::cout << "     Base Value Multiple: " << prop->addBaseValueMultiple << std::endl;
                std::cout << "     Value Multiplier: " << prop->valueMultiplier << std::endl;
                std::cout << "     Value Change: " << prop->valueChange << std::endl;
            }

            // Display cumulative stats
            std::cout << "\nCumulative Stats:" << std::endl;
            displayStatsBar(totalAddictiveness, 1.0, 20, "Addictiveness");
            displayStatsBar(totalBaseValueMultiple, 4.0, 20, "Total Base Value Bonus");
            std::cout << "Value Multiplier: " << totalValueMultiplier << std::endl;
            std::cout << "Value Change: " << totalValueChange << std::endl;

            // Display the final value calculation formula
            std::cout << "\nFinal Value = Base Value * (1 + " << totalBaseValueMultiple << ") * "
                << totalValueMultiplier << " + " << totalValueChange << std::endl;
        }

        // Show ingredient history
        if (!ingredientHistory.empty()) {
            std::cout << "\nIngredient History:" << std::endl;
            for (size_t i = 0; i < ingredientHistory.size(); i++) {
                std::cout << "  " << (i + 1) << ". " << ingredientHistory[i] << std::endl;
            }
        }

        // Menu options
        std::cout << "\nOptions:" << std::endl;
        std::cout << "  1. Add ingredient" << std::endl;
        std::cout << "  2. Reset properties" << std::endl;
        std::cout << "  3. View detailed property info" << std::endl;
        std::cout << "  4. View mixer map visualization" << std::endl;
        std::cout << "  5. Save recipe to file" << std::endl;
        std::cout << "  6. Exit" << std::endl;
        std::cout << "Enter choice: ";
        std::getline(std::cin, command);

        if (command == "1") {
            // Show available ingredients
            std::cout << "\nAvailable ingredients:" << std::endl;
            int index = 1;
            std::vector<std::string> ingredientNames;
            for (const auto& pair : ingredientPropertyMapping) {
                std::cout << "  " << index << ". " << pair.first << std::endl;
                ingredientNames.push_back(pair.first);
                index++;
            }

            // Get user selection
            std::cout << "Enter ingredient number: ";
            std::getline(std::cin, command);
            try {
                int selection = std::stoi(command);
                if (selection >= 1 && selection <= ingredientNames.size()) {
                    std::string ingredientName = ingredientNames[selection - 1];
                    std::string propertyId = ingredientPropertyMapping[ingredientName];
                    Property* newProperty = getPropertyByNameOrId(propertyId);

                    std::cout << "\nAdding " << ingredientName << " with property: " << newProperty->name << std::endl;

                    // Add to ingredient history
                    ingredientHistory.push_back(ingredientName);

                    // Extract just the properties without origin info for mixing
                    std::vector<Property*> currentProperties;
                    for (const auto& propWithOrigin : currentPropertiesWithOrigin) {
                        currentProperties.push_back(propWithOrigin.property);
                    }

                    // Mix the properties
                    std::vector<Property*> result = PropertyMixCalculator::mixProperties(
                        currentProperties, newProperty, DrugType::Marijuana);

                    // Create new properties with origin information
                    std::vector<PropertyWithOrigin> newPropertiesWithOrigin;

                    // For each property in the result, determine its origin
                    for (Property* resultProp : result) {
                        PropertyWithOrigin newPropWithOrigin;
                        newPropWithOrigin.property = resultProp;

                        // Check if this property was in the previous set
                        bool wasInPrevious = false;
                        for (const auto& prevPropWithOrigin : currentPropertiesWithOrigin) {
                            if (prevPropWithOrigin.property->id == resultProp->id) {
                                // This property was in the previous set, copy its ingredient list
                                newPropWithOrigin.ingredients = prevPropWithOrigin.ingredients;
                                wasInPrevious = true;
                                break;
                            }
                        }

                        // If it's the new property we just added
                        if (!wasInPrevious && resultProp->id == newProperty->id) {
                            // This is just the ingredient we added
                            newPropWithOrigin.ingredients.push_back(ingredientName);
                        }
                        // If it's a new property created by mixing
                        else if (!wasInPrevious) {
                            // This is a result of mixing, so we need to list all ingredients that led to it
                            // For simplicity, let's say it resulted from all previous ingredients plus the new one
                            for (const auto& prevIngredient : ingredientHistory) {
                                if (std::find(newPropWithOrigin.ingredients.begin(),
                                    newPropWithOrigin.ingredients.end(),
                                    prevIngredient) == newPropWithOrigin.ingredients.end()) {
                                    newPropWithOrigin.ingredients.push_back(prevIngredient);
                                }
                            }
                        }

                        newPropertiesWithOrigin.push_back(newPropWithOrigin);
                    }

                    // Update current properties
                    currentPropertiesWithOrigin = newPropertiesWithOrigin;

                    std::cout << "\nResult:" << std::endl;
                    for (size_t i = 0; i < currentPropertiesWithOrigin.size(); i++) {
                        std::cout << "  " << (i + 1) << ". " << currentPropertiesWithOrigin[i].property->name
                            << " (Tier " << currentPropertiesWithOrigin[i].property->tier << ")" << std::endl;
                    }
                }
                else {
                    std::cout << "Invalid selection" << std::endl;
                }
            }
            catch (const std::exception& e) {
                std::cout << "Invalid input" << std::endl;
            }
        }
        else if (command == "2") {
            // Reset properties
            currentPropertiesWithOrigin.clear();
            ingredientHistory.clear();
            std::cout << "Properties reset" << std::endl;
        }
        else if (command == "3") {
            // View detailed property info
            if (currentPropertiesWithOrigin.empty()) {
                std::cout << "No properties to display" << std::endl;
            }
            else {
                std::cout << "\nSelect property number for detailed info: ";
                std::getline(std::cin, command);
                try {
                    int selection = std::stoi(command);
                    if (selection >= 1 && selection <= currentPropertiesWithOrigin.size()) {
                        Property* prop = currentPropertiesWithOrigin[selection - 1].property;
                        std::cout << "\nDetailed info for " << prop->name << ":" << std::endl;
                        std::cout << "  ID: " << prop->id << std::endl;
                        std::cout << "  Tier: " << prop->tier << std::endl;
                        std::cout << "  Addictiveness: " << prop->addictiveness << std::endl;
                        std::cout << "  Value Change: " << prop->valueChange << std::endl;
                        std::cout << "  Value Multiplier: " << prop->valueMultiplier << std::endl;
                        std::cout << "  Add Base Value Multiple: " << prop->addBaseValueMultiple << std::endl;
                        std::cout << "  Mix Direction: (" << prop->mixDirection.x << ", " << prop->mixDirection.y << ")" << std::endl;
                        std::cout << "  Mix Magnitude: " << prop->mixMagnitude << std::endl;

                        // Show ingredient origins
                        std::cout << "  Origin Ingredients: ";
                        const auto& ingredients = currentPropertiesWithOrigin[selection - 1].ingredients;
                        if (ingredients.empty()) {
                            std::cout << "Direct ingredient" << std::endl;
                        }
                        else {
                            for (size_t i = 0; i < ingredients.size(); i++) {
                                std::cout << ingredients[i];
                                if (i < ingredients.size() - 1) {
                                    std::cout << " + ";
                                }
                            }
                            std::cout << std::endl;
                        }
                    }
                    else {
                        std::cout << "Invalid selection" << std::endl;
                    }
                }
                catch (const std::exception& e) {
                    std::cout << "Invalid input" << std::endl;
                }
            }
        }
        else if (command == "4") {

        }
        else if (command == "5") {
            // Save recipe to file
            if (ingredientHistory.empty()) {
                std::cout << "No ingredients to save" << std::endl;
                continue;
            }

            std::cout << "Enter filename to save recipe (e.g., recipe.txt): ";
            std::getline(std::cin, command);

            try {
                std::ofstream file(command);
                if (file.is_open()) {
                    // Write recipe header
                    file << "=== Schedule I Recipe ===" << std::endl;
                    file << "Date: " << __DATE__ << " " << __TIME__ << std::endl << std::endl;

                    // Write ingredients
                    file << "Ingredients (in order):" << std::endl;
                    for (size_t i = 0; i < ingredientHistory.size(); i++) {
                        file << "  " << (i + 1) << ". " << ingredientHistory[i] << std::endl;
                    }

                    // Write resulting properties
                    file << std::endl << "Resulting Properties:" << std::endl;
                    for (const auto& propWithOrigin : currentPropertiesWithOrigin) {
                        file << "  - " << propWithOrigin.property->name << " (Tier "
                            << propWithOrigin.property->tier << ")" << std::endl;

                        // Write origin ingredients
                        file << "    Origin: ";
                        if (propWithOrigin.ingredients.empty()) {
                            file << "Direct ingredient";
                        }
                        else {
                            for (size_t i = 0; i < propWithOrigin.ingredients.size(); i++) {
                                file << propWithOrigin.ingredients[i];
                                if (i < propWithOrigin.ingredients.size() - 1) {
                                    file << " + ";
                                }
                            }
                        }
                        file << std::endl;
                    }

                    // Write cumulative stats
                    float totalAddictiveness = 0.0;
                    float totalBaseValueMultiple = 0.0;
                    float totalValueMultiplier = 1.0;
                    int totalValueChange = 0;

                    for (const auto& propWithOrigin : currentPropertiesWithOrigin) {
                        totalAddictiveness += propWithOrigin.property->addictiveness;
                        totalBaseValueMultiple += propWithOrigin.property->addBaseValueMultiple;
                        totalValueMultiplier *= propWithOrigin.property->valueMultiplier;
                        totalValueChange += propWithOrigin.property->valueChange;
                    }

                    file << std::endl << "Cumulative Stats:" << std::endl;
                    file << "  Addictiveness: " << totalAddictiveness << std::endl;
                    file << "  Base Value Multiple: " << totalBaseValueMultiple << std::endl;
                    file << "  Value Multiplier: " << totalValueMultiplier << std::endl;
                    file << "  Value Change: " << totalValueChange << std::endl;

                    file << std::endl << "Final Value = Base Value × (1 + " << totalBaseValueMultiple << ") × "
                        << totalValueMultiplier << " + " << totalValueChange << std::endl;

                    file.close();
                    std::cout << "Recipe saved to " << command << std::endl;
                }
                else {
                    std::cout << "Unable to open file for writing" << std::endl;
                }
            }
            catch (const std::exception& e) {
                std::cout << "Error saving file: " << e.what() << std::endl;
            }
        }
        else if (command == "6") {
            // Exit
            break;
        }
        else {
            std::cout << "Invalid command" << std::endl;
        }
    }

    // Cleanup (in a real program, we would properly delete all allocated objects)

    std::cout << "Exiting property mixer" << std::endl;
    return 0;
}