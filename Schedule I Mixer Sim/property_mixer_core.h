#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <algorithm>
#include <cmath>
#include <random>

// Forward declarations
class Property;
class MixerMapEffect;
class MixerMap;
class StationRecipe;

// Global properties map
extern std::map<std::string, Property*> properties;

// Enums
enum class DrugType {
    Marijuana = 0,
    Methamphetamine = 1,
    Cocaine = 2,
    MDMA = 3,
    Shrooms = 4,
    Heroin = 5
};


// Define a Drug Product class
class DrugProduct {
public:
    DrugProduct(const std::string& name, DrugType type, const std::string& rank,
        const std::vector<Property*>& startingProperties = std::vector<Property*>())
        : name(name), type(type), rank(rank), properties(startingProperties) {}

    std::string name;
    DrugType type;
    std::string rank;
    std::vector<Property*> properties;
};

// Global products map
std::map<std::string, DrugProduct*> products;

// Initialize the products
void initializeProducts() {
    // Create marijuana products
    products["OG Kush"] = new DrugProduct(
        "OG Kush",
        DrugType::Marijuana,
        "Street Rat I",
        { properties["calming"] }
    );

    products["Sour Diesel"] = new DrugProduct(
        "Sour Diesel",
        DrugType::Marijuana,
        "Street Rat V",
        { properties["refreshing"] }
    );

    products["Green Crack"] = new DrugProduct(
        "Green Crack",
        DrugType::Marijuana,
        "Hoodlum III",
        { properties["energizing"] }
    );

    products["Granddaddy Purple"] = new DrugProduct(
        "Granddaddy Purple",
        DrugType::Marijuana,
        "Hoodlum V",
        { properties["sedating"] }
    );

    // Create other drug products
    products["Methamphetamine"] = new DrugProduct(
        "Methamphetamine",
        DrugType::Methamphetamine,
        "Hoodlum I"
        // No starting properties
    );

    products["Cocaine"] = new DrugProduct(
        "Cocaine",
        DrugType::Cocaine,
        "Enforcer I"
        // No starting properties
    );
}

// Vector2 implementation (similar to Unity's Vector2)
struct Vector2 {
    float x;
    float y;

    Vector2() : x(0.0f), y(0.0f) {}
    Vector2(float x, float y) : x(x), y(y) {}

    // Vector addition operator
    Vector2 operator+(const Vector2& other) const {
        return Vector2(x + other.x, y + other.y);
    }

    // Scalar multiplication
    Vector2 operator*(float scalar) const {
        return Vector2(x * scalar, y * scalar);
    }

    // Magnitude calculation
    float Magnitude() const {
        return std::sqrt(x * x + y * y);
    }

    // Static distance method
    static float Distance(const Vector2& a, const Vector2& b) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

// Base Property class
class Property {
public:
    Property() :
        name(""),
        id(""),
        tier(1),
        addictiveness(0.0f),
        valueChange(0),
        valueMultiplier(1.0f),
        addBaseValueMultiple(0.0f),
        mixDirection(Vector2()),
        mixMagnitude(0.0f) {}

    Property(const std::string& name, const std::string& id, int tier, float addictiveness,
        int valueChange, float valueMultiplier, float addBaseValueMultiple,
        Vector2 mixDirection, float mixMagnitude) :
        name(name),
        id(id),
        tier(tier),
        addictiveness(addictiveness),
        valueChange(valueChange),
        valueMultiplier(valueMultiplier),
        addBaseValueMultiple(addBaseValueMultiple),
        mixDirection(mixDirection),
        mixMagnitude(mixMagnitude) {}

    virtual ~Property() = default;

    std::string name;
    std::string id;
    int tier;
    float addictiveness;
    int valueChange;
    float valueMultiplier;
    float addBaseValueMultiple;
    Vector2 mixDirection;
    float mixMagnitude;

    // For debugging
    void print() const {
        std::cout << "Property: " << name << " (ID: " << id << ")" << std::endl;
        std::cout << "  Tier: " << tier << ", Addictiveness: " << addictiveness << std::endl;
        std::cout << "  MixDirection: (" << mixDirection.x << ", " << mixDirection.y << ")" << std::endl;
        std::cout << "  MixMagnitude: " << mixMagnitude << std::endl;
    }
};
// MixerMapEffect with proper isPointInEffect method
class MixerMapEffect {
public:
    MixerMapEffect(const Vector2& position, float radius, Property* property) :
        position(position), radius(radius), property(property) {}

    Vector2 position;
    float radius;
    Property* property;

    // Check if a point is within this effect's radius
    bool isPointInEffect(const Vector2& point) const {
        return Vector2::Distance(position, point) <= radius;
    }
};
// Updated MixerMap class with proper GetEffect method
class MixerMap {
public:
    MixerMap() : mapRadius(4.0f) {}
    ~MixerMap() {
        for (auto* effect : effects) {
            delete effect;
        }
    }

    float mapRadius;
    std::vector<MixerMapEffect*> effects;

    void addEffect(const Vector2& position, float radius, Property* property) {
        effects.push_back(new MixerMapEffect(position, radius, property));
    }

    // Find the effect that contains a specific property (matches C# GetEffect)
    MixerMapEffect* getEffect(Property* property) const {
        for (auto* effect : effects) {
            if (effect->property->id == property->id) {
                return effect;
            }
        }
        return nullptr;
    }

    // Find the effect at a given point
    MixerMapEffect* getEffectAtPoint(const Vector2& point) const {
        // First check if the point is within the map radius
        if (point.Magnitude() > mapRadius) {
            return nullptr;
        }

        // Iterate through all effects to find the first one containing the point
        for (auto* effect : effects) {
            if (effect->isPointInEffect(point)) {
                return effect;
            }
        }

        return nullptr;
    }
};



// Recipe system components
class StationRecipe {
public:
    StationRecipe(const std::vector<Property*>& ingredients, Property* result)
        : ingredients(ingredients), result(result) {}

    std::vector<Property*> ingredients;
    Property* result;
};

// ProductManager singleton to manage recipes and mixer maps
class ProductManager {
public:
    static ProductManager& getInstance() {
        static ProductManager instance;
        return instance;
    }

    // Get a recipe (returning a property) for a combination
    StationRecipe* getRecipe(const std::vector<Property*>& existingProperties, Property* newProperty) {
        // Look for a recipe that matches all existing properties and the new property
        for (auto* recipe : mixRecipes) {
            // Quick check if recipe has the right number of ingredients
            if (recipe->ingredients.size() != existingProperties.size() + 1) {
                continue;
            }

            // Check if all existing properties are in the recipe
            bool allPropertiesMatch = true;
            for (auto* existingProp : existingProperties) {
                bool foundMatch = false;
                for (auto* ingredient : recipe->ingredients) {
                    if (ingredient->id == existingProp->id) {
                        foundMatch = true;
                        break;
                    }
                }
                if (!foundMatch) {
                    allPropertiesMatch = false;
                    break;
                }
            }

            // Check if the new property is in the recipe
            bool newPropertyMatch = false;
            for (auto* ingredient : recipe->ingredients) {
                if (ingredient->id == newProperty->id) {
                    newPropertyMatch = true;
                    break;
                }
            }

            if (allPropertiesMatch && newPropertyMatch) {
                return recipe;
            }
        }

        return nullptr;
    }

    // Get the mixer map for a specific drug type
    MixerMap* getMixerMap(DrugType drugType) {
        switch (drugType) {
        case DrugType::Marijuana:
            return weedMixMap;
        case DrugType::Methamphetamine:
            return methMixMap;
        case DrugType::Cocaine:
            return cokeMixMap;
        default:
            std::cerr << "Error: No mixer map for drug type " << static_cast<int>(drugType) << std::endl;
            return nullptr;
        }
    }

    // Add a recipe
    void addRecipe(StationRecipe* recipe) {
        mixRecipes.push_back(recipe);
    }

    // Initialize the mixer maps
    void initializeMixerMaps(MixerMap* weed, MixerMap* meth, MixerMap* coke) {
        weedMixMap = weed;
        methMixMap = meth;
        cokeMixMap = coke;
    }

private:
    ProductManager() : weedMixMap(nullptr), methMixMap(nullptr), cokeMixMap(nullptr) {}
    ~ProductManager() {
        delete weedMixMap;
        delete methMixMap;
        delete cokeMixMap;

        for (auto* recipe : mixRecipes) {
            delete recipe;
        }
    }

    MixerMap* weedMixMap;
    MixerMap* methMixMap;
    MixerMap* cokeMixMap;
    std::vector<StationRecipe*> mixRecipes;
};

// Reaction class for property mixing
class Reaction {
public:
    Reaction(Property* existing, Property* output) : existing(existing), output(output) {}

    Property* existing;
    Property* output;
};

// The main PropertyMixCalculator class
class PropertyMixCalculator {
public:
    static const int MAX_PROPERTIES = 8;

    // Main function to mix properties - matches C# MixProperties exactly
    static std::vector<Property*> mixProperties(const std::vector<Property*>& existingProperties,
        Property* newProperty,
        DrugType drugType) {
        // First, check if there's a recipe for this combination
        ProductManager& productManager = ProductManager::getInstance();
        StationRecipe* recipe = productManager.getRecipe(existingProperties, newProperty);

        if (recipe != nullptr) {
            // If there's a recipe, return its result
            std::cout << "Found recipe with result: " << recipe->result->name << std::endl;
            return { recipe->result };
        }

        // If no recipe, proceed with the mixing logic
        if (newProperty == nullptr) {
            std::cerr << "Error: newProperty is null" << std::endl;
            return existingProperties;
        }

        // Calculate scaled mix direction - matches C# vector = mixDirection * mixMagnitude
        Vector2 vector = newProperty->mixDirection * newProperty->mixMagnitude;

        // Get the mixer map for this drug type
        MixerMap* mixerMap = productManager.getMixerMap(drugType);
        if (mixerMap == nullptr) {
            std::cerr << "Error: mixer map is null for drug type " << static_cast<int>(drugType) << std::endl;
            return existingProperties;
        }

        // Create a list to track reactions - matches C# list creation
        std::vector<Reaction> reactions;

        // Process each existing property - matches C# iteration
        for (size_t i = 0; i < existingProperties.size(); i++) {
            // Get the effect for this property - using pointer comparison
            MixerMapEffect* effect = mixerMap->getEffect(existingProperties[i]);

            // Calculate the new position - matches C# vector2 = position + vector
            Vector2 vector2 = effect->position + vector;

            // Find the effect at the new position
            MixerMapEffect* effectAtPoint = mixerMap->getEffectAtPoint(vector2);

            // Get the property from the effect
            Property* property = (effectAtPoint != nullptr) ? effectAtPoint->property : nullptr;

            // Add reaction if property exists
            if (property != nullptr) {
                Reaction reaction;
                reaction.existing = existingProperties[i];
                reaction.output = property;
                reactions.push_back(reaction);
            }
        }

        // Create a new list starting with existing properties - matches C# list copy
        std::vector<Property*> result(existingProperties);

        // Apply reactions - matches C# foreach loop exactly
        for (const auto& reaction : reactions) {
            // Only update if output isn't already in the list
            if (std::find(result.begin(), result.end(), reaction.output) == result.end()) {
                // Find index of existing property
                auto it = std::find(result.begin(), result.end(), reaction.existing);
                if (it != result.end()) {
                    // Replace at that index
                    *it = reaction.output;
                }
            }
        }

        // Add the new property if not already in list and under max - matches C# check
        if (std::find(result.begin(), result.end(), newProperty) == result.end() &&
            result.size() < MAX_PROPERTIES) {
            result.push_back(newProperty);
        }

        // Make distinct list - matches C# Distinct() function
        std::vector<Property*> distinctResult;
        for (auto* prop : result) {
            if (std::find(distinctResult.begin(), distinctResult.end(), prop) == distinctResult.end()) {
                distinctResult.push_back(prop);
            }
        }

        return distinctResult;
    }

    // Shuffle function implementation (for randomization if needed)
    template<typename T>
    static void shuffle(std::vector<T>& list, int seed) {
        std::mt19937 g(seed);
        std::shuffle(list.begin(), list.end(), g);
    }

    // Inner Reaction class - matches C# inner class
    class Reaction {
    public:
        Property* existing = nullptr;
        Property* output = nullptr;
    };
};
// Global properties map
std::map<std::string, Property*> properties;

// Helper function to get a property by name or ID
Property* getPropertyByNameOrId(const std::string& nameOrId) {
    // Try to find by exact ID match
    auto it = properties.find(nameOrId);
    if (it != properties.end()) {
        return it->second;
    }

    // Try to find by case-insensitive name or ID match
    std::string lowerNameOrId = nameOrId;
    std::transform(lowerNameOrId.begin(), lowerNameOrId.end(), lowerNameOrId.begin(), ::tolower);

    for (const auto& pair : properties) {
        std::string lowerName = pair.second->name;
        std::string lowerId = pair.second->id;

        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);

        if (lowerName == lowerNameOrId || lowerId == lowerNameOrId) {
            return pair.second;
        }
    }

    return nullptr;
}

// Create properties from the extracted data
void createPropertiesFromData() {
    // Create all properties from the extracted data
    properties["calming"] = new Property(
        "Calming", "calming", 1, 0,
        0, 1, 0.1,
        Vector2(0.999811, 0.0194138), 1.03019
    );

    properties["Euphoric"] = new Property(
        "Euphoric", "Euphoric", 1, 0.235,
        0, 1, 0.18,
        Vector2(0, 1), 1.07
    );

    properties["Focused"] = new Property(
        "Focused", "Focused", 1, 0.104,
        0, 1, 0.16,
        Vector2(-0.998846, 0.0480215), 1.0412
    );

    properties["munchies"] = new Property(
        "Munchies", "munchies", 1, 0.096,
        0, 1, 0.12,
        Vector2(0.0291139, -0.999576), 1.03044
    );

    properties["paranoia"] = new Property(
        "Paranoia", "paranoia", 1, 0,
        0, 1, 0,
        Vector2(-0.73821, -0.674571), 1.57137
    );

    properties["refreshing"] = new Property(
        "Refreshing", "refreshing", 1, 0.104,
        0, 1, 0.14,
        Vector2(-0.703985, 0.710215), 1.60515
    );

    properties["smelly"] = new Property(
        "Smelly", "smelly", 1, 0,
        0, 1, 0,
        Vector2(0.75001, -0.661426), 1.69331
    );

    properties["caloriedense"] = new Property(
        "Calorie-Dense", "caloriedense", 2, 0.1,
        0, 1, 0.28,
        Vector2(0.694483, 0.719509), 1.59831
    );

    properties["disorienting"] = new Property(
        "Disorienting", "disorienting", 2, 0,
        0, 1, 0,
        Vector2(-0.275337, 0.961348), 2.14283
    );

    properties["energizing"] = new Property(
        "Energizing", "energizing", 2, 0.34,
        0, 1, 0.22,
        Vector2(-0.96631, 0.257382), 2.21461
    );

    properties["gingeritis"] = new Property(
        "Gingeritis", "gingeritis", 2, 0,
        0, 1, 0.2,
        Vector2(-0.283827, -0.958875), 2.08578
    );

    properties["sedating"] = new Property(
        "Sedating", "sedating", 2, 0,
        0, 1, 0.26,
        Vector2(0.982339, -0.187112), 2.13776
    );

    properties["sneaky"] = new Property(
        "Sneaky", "sneaky", 2, 0.327,
        0, 1, 0.24,
        Vector2(0.364043, -0.931382), 2.11514
    );

    properties["toxic"] = new Property(
        "Toxic", "toxic", 2, 0,
        0, 1, 0,
        Vector2(0.954557, 0.298029), 2.31521
    );

    properties["athletic"] = new Property(
        "Athletic", "athletic", 3, 0.607,
        0, 1, 0.32,
        Vector2(-0.967801, -0.251715), 2.30419
    );

    properties["balding"] = new Property(
        "Balding", "balding", 3, 0,
        0, 1, 0.3,
        Vector2(-0.0467715, -0.998906), 2.99328
    );

    properties["foggy"] = new Property(
        "Foggy", "foggy", 3, 0.1,
        0, 1, 0.36,
        Vector2(0.223898, 0.974613), 2.27783
    );

    properties["laxative"] = new Property(
        "Laxative", "laxative", 3, 0.1,
        0, 1, 0,
        Vector2(-0.804176, 0.594391), 2.57406
    );

    properties["seizure"] = new Property(
        "Seizure-Inducing", "seizure", 3, 0,
        0, 1, 0,
        Vector2(-0.624239, -0.781233), 2.67526
    );

    properties["slippery"] = new Property(
        "Slippery", "slippery", 3, 0.309,
        0, 1, 0.34,
        Vector2(0.775649, -0.631165), 2.63006
    );

    properties["spicy"] = new Property(
        "Spicy", "spicy", 3, 0.665,
        0, 1, 0.38,
        Vector2(0.750938, 0.660373), 2.65002
    );

    properties["brighteyed"] = new Property(
        "Bright-Eyed", "brighteyed", 4, 0.2,
        0, 1, 0.4,
        Vector2(0.999913, -0.0132002), 3.03026
    );

    properties["glowie"] = new Property(
        "Glowing", "glowie", 4, 0.472,
        0, 1, 0.48,
        Vector2(0.475517, 0.879707), 2.94416
    );

    properties["jennerising"] = new Property(
        "Jennerising", "jennerising", 4, 0.343,
        0, 1, 0.42,
        Vector2(-0.429359, -0.903134), 3.37713
    );

    properties["lethal"] = new Property(
        "Lethal", "lethal", 4, 0,
        0, 1, 0,
        Vector2(-0.999824, 0.0187467), 3.20056
    );

    properties["schizophrenic"] = new Property(
        "Schizophrenic", "schizophrenic", 4, 0,
        0, 1, 0,
        Vector2(0.64213, -0.766596), 3.53511
    );

    properties["thoughtprovoking"] = new Property(
        "Thought-Provoking", "thoughtprovoking", 4, 0.37,
        0, 1, 0.44,
        Vector2(-0.862103, -0.506733), 3.03908
    );

    properties["tropicthunder"] = new Property(
        "Tropic Thunder", "tropicthunder", 4, 0.803,
        0, 1, 0.46,
        Vector2(0.935815, -0.35249), 3.20576
    );

    properties["antigravity"] = new Property(
        "Anti-gravity", "antigravity", 5, 0.611,
        0, 1, 0.54,
        Vector2(0.308505, -0.951223), 3.11178
    );

    properties["cyclopean"] = new Property(
        "Cyclopean", "cyclopean", 5, 0.1,
        0, 1, 0.56,
        Vector2(-0.52159, 0.853196), 2.895
    );

    properties["electrifying"] = new Property(
        "Electrifying", "electrifying", 5, 0.235,
        0, 1, 0.5,
        Vector2(-0.918833, 0.394646), 3.31943
    );

    properties["explosive"] = new Property(
        "Explosive", "explosive", 5, 0,
        0, 1, 0,
        Vector2(0.675211, 0.737625), 3.52483
    );

    properties["giraffying"] = new Property(
        "Long faced", "giraffying", 5, 0.607,
        0, 1, 0.52,
        Vector2(-0.0681009, 0.997678), 2.93682
    );

    properties["shrinking"] = new Property(
        "Shrinking", "shrinking", 5, 0.336,
        0, 1, 0.6,
        Vector2(-0.964696, -0.263368), 3.3793
    );

    properties["zombifying"] = new Property(
        "Zombifying", "zombifying", 5, 0.598,
        0, 1, 0.58,
        Vector2(0.929986, 0.367596), 3.18284
    );
}

// Initialize mixer maps from the extracted data
MixerMap* createWeedMixMap() {
    MixerMap* weedMap = new MixerMap();
    weedMap->mapRadius = 4.0f;

    // Add all effects to the weed map
    weedMap->addEffect(Vector2(1.03, 0.02), 0.4f, properties["calming"]);
    weedMap->addEffect(Vector2(0, 1.07), 0.4f, properties["Euphoric"]);
    weedMap->addEffect(Vector2(-1.04, 0.05), 0.4f, properties["Focused"]);
    weedMap->addEffect(Vector2(0.03, -1.03), 0.4f, properties["munchies"]);
    weedMap->addEffect(Vector2(-1.16, -1.06), 0.4f, properties["paranoia"]);
    weedMap->addEffect(Vector2(-1.13, 1.14), 0.4f, properties["refreshing"]);
    weedMap->addEffect(Vector2(1.27, -1.12), 0.4f, properties["smelly"]);
    weedMap->addEffect(Vector2(1.11, 1.15), 0.4f, properties["caloriedense"]);
    weedMap->addEffect(Vector2(-0.59, 2.06), 0.4f, properties["disorienting"]);
    weedMap->addEffect(Vector2(-2.14, 0.57), 0.4f, properties["energizing"]);
    weedMap->addEffect(Vector2(-0.592, -2), 0.4f, properties["gingeritis"]);
    weedMap->addEffect(Vector2(2.1, -0.4), 0.4f, properties["sedating"]);
    weedMap->addEffect(Vector2(0.77, -1.97), 0.4f, properties["sneaky"]);
    weedMap->addEffect(Vector2(2.21, 0.69), 0.4f, properties["toxic"]);
    weedMap->addEffect(Vector2(-2.23, -0.58), 0.4f, properties["athletic"]);
    weedMap->addEffect(Vector2(-0.14, -2.99), 0.4f, properties["balding"]);
    weedMap->addEffect(Vector2(0.51, 2.22), 0.4f, properties["foggy"]);
    weedMap->addEffect(Vector2(-2.07, 1.53), 0.4f, properties["laxative"]);
    weedMap->addEffect(Vector2(-1.67, -2.09), 0.4f, properties["seizure"]);
    weedMap->addEffect(Vector2(2.04, -1.66), 0.4f, properties["slippery"]);
    weedMap->addEffect(Vector2(1.99, 1.75), 0.4f, properties["spicy"]);
    weedMap->addEffect(Vector2(3.03, -0.04), 0.4f, properties["brighteyed"]);
    weedMap->addEffect(Vector2(1.4, 2.59), 0.4f, properties["glowie"]);
    weedMap->addEffect(Vector2(-1.45, -3.05), 0.4f, properties["jennerising"]);
    weedMap->addEffect(Vector2(-3.2, 0.06), 0.4f, properties["lethal"]);
    weedMap->addEffect(Vector2(2.27, -2.71), 0.4f, properties["schizophrenic"]);
    weedMap->addEffect(Vector2(-2.62, -1.54), 0.4f, properties["thoughtprovoking"]);
    weedMap->addEffect(Vector2(3, -1.13), 0.4f, properties["tropicthunder"]);
    weedMap->addEffect(Vector2(0.96, -2.96), 0.4f, properties["antigravity"]);
    weedMap->addEffect(Vector2(-1.51, 2.47), 0.4f, properties["cyclopean"]);
    weedMap->addEffect(Vector2(-3.05, 1.31), 0.4f, properties["electrifying"]);
    weedMap->addEffect(Vector2(2.38, 2.6), 0.4f, properties["explosive"]);
    weedMap->addEffect(Vector2(-0.2, 2.93), 0.4f, properties["giraffying"]);
    weedMap->addEffect(Vector2(-3.26, -0.89), 0.4f, properties["shrinking"]);
    weedMap->addEffect(Vector2(2.96, 1.17), 0.4f, properties["zombifying"]);

    return weedMap;
}

// Initialize the game system
void initializeGameSystem() {
    // Create properties
    createPropertiesFromData();

    // Create mixer maps
    MixerMap* weedMap = createWeedMixMap();

    // Initialize ProductManager
    ProductManager& manager = ProductManager::getInstance();
    manager.initializeMixerMaps(weedMap, weedMap, weedMap); // Using weedMap for all types for now

    // Initialize products with their starting properties
    initializeProducts();
}