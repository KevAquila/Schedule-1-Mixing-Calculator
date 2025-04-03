#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <tlhelp32.h>
#include <fstream>
#include <iomanip>

// Structures to match the game's memory layout
struct Vector2 {
    float x;
    float y;
};

struct Color {
    float r, g, b, a;
};

struct Property {
    DWORD_PTR m_CachedPtr;
    std::string name;
    std::string description;
    std::string id;
    int tier;
    float addictiveness;
    Color productColor;
    Color labelColor;
    bool implementedPriorMixingRework;
    int valueChange;
    float valueMultiplier;
    float addBaseValueMultiple;
    Vector2 mixDirection;
    float mixMagnitude;
};

struct MixerMapEffect {
    Vector2 position;
    float radius;
    DWORD_PTR property_ptr;
    Property property;
};

struct MixerMap {
    float mapRadius;
    std::vector<MixerMapEffect> effects;
};

struct MixIngredient {
    DWORD_PTR ptr;
    std::string name;
    Property property;
};

// Helper functions
DWORD GetProcessIdByName(const std::string& processName) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(snapshot, &processEntry)) {
            do {
                // Convert wide character process name to standard string for comparison
                std::wstring wProcessName(processEntry.szExeFile);
                std::string exeName(wProcessName.begin(), wProcessName.end());
                if (exeName == processName) {
                    pid = processEntry.th32ProcessID;
                    break;
                }
            } while (Process32Next(snapshot, &processEntry));
        }
        CloseHandle(snapshot);
    }
    return pid;
}

template<typename T>
T ReadMemory(HANDLE hProcess, DWORD_PTR address) {
    T value;
    ReadProcessMemory(hProcess, (LPCVOID)address, &value, sizeof(T), nullptr);
    return value;
}

std::string ReadString(HANDLE hProcess, DWORD_PTR address) {
    // First read the string metadata
    DWORD_PTR stringData = ReadMemory<DWORD_PTR>(hProcess, address);
    if (stringData == 0) return "";

    // Read the length (assuming it's at offset 0x10 in the string object)
    int length = ReadMemory<int>(hProcess, stringData + 0x10);
    if (length <= 0 || length > 1000) return ""; // Sanity check

    // Read the string data - Unity strings are UTF-16 (2 bytes per character)
    std::vector<wchar_t> wbuffer(length + 1, 0);
    ReadProcessMemory(hProcess, (LPCVOID)(stringData + 0x14), wbuffer.data(), length * 2, nullptr);

    // Convert UTF-16 to UTF-8/ASCII
    std::string result;
    for (int i = 0; i < length; i++) {
        // For simplicity, just take the lower byte for ASCII chars
        result += static_cast<char>(wbuffer[i] & 0xFF);
    }

    return result;
}

Property ReadProperty(HANDLE hProcess, DWORD_PTR address) {
    Property prop;
    prop.m_CachedPtr = address;

    // Read strings based on screenshots
    prop.name = ReadString(hProcess, address + 0x18);
    prop.description = ReadString(hProcess, address + 0x20);
    prop.id = ReadString(hProcess, address + 0x28);

    // Read values based on screenshots
    prop.tier = ReadMemory<int>(hProcess, address + 0x30);
    prop.addictiveness = ReadMemory<float>(hProcess, address + 0x34);

    // Read colors
    prop.productColor.r = ReadMemory<float>(hProcess, address + 0x38);
    prop.productColor.g = ReadMemory<float>(hProcess, address + 0x3C);
    prop.productColor.b = ReadMemory<float>(hProcess, address + 0x40);
    prop.productColor.a = ReadMemory<float>(hProcess, address + 0x44);

    prop.labelColor.r = ReadMemory<float>(hProcess, address + 0x48);
    prop.labelColor.g = ReadMemory<float>(hProcess, address + 0x4C);
    prop.labelColor.b = ReadMemory<float>(hProcess, address + 0x50);
    prop.labelColor.a = ReadMemory<float>(hProcess, address + 0x54);

    prop.implementedPriorMixingRework = ReadMemory<bool>(hProcess, address + 0x58);
    prop.valueChange = ReadMemory<int>(hProcess, address + 0x5C);
    prop.valueMultiplier = ReadMemory<float>(hProcess, address + 0x60);
    prop.addBaseValueMultiple = ReadMemory<float>(hProcess, address + 0x64);

    // Read MixDirection (Vector2)
    prop.mixDirection.x = ReadMemory<float>(hProcess, address + 0x68);
    prop.mixDirection.y = ReadMemory<float>(hProcess, address + 0x6C);

    // Read MixMagnitude
    prop.mixMagnitude = ReadMemory<float>(hProcess, address + 0x70);

    return prop;
}

MixerMapEffect ReadMixerMapEffect(HANDLE hProcess, DWORD_PTR address) {
    MixerMapEffect effect;

    // Read Position (Vector2) directly inline from memory
    effect.position.x = ReadMemory<float>(hProcess, address + 0x10);
    effect.position.y = ReadMemory<float>(hProcess, address + 0x14);

    // Read Radius
    effect.radius = ReadMemory<float>(hProcess, address + 0x18);

    // Read Property pointer
    effect.property_ptr = ReadMemory<DWORD_PTR>(hProcess, address + 0x20);

    // Read property if pointer is valid
    if (effect.property_ptr != 0) {
        effect.property = ReadProperty(hProcess, effect.property_ptr);
    }

    return effect;
}

void ExtractProductManager(HANDLE hProcess, DWORD_PTR address) {
    // Raw data dump file
    std::ofstream rawDataFile("property_data_raw.json");
    rawDataFile << "{\n";
    rawDataFile << "  \"productManagerAddress\": \"0x" << std::hex << address << "\",\n";

    // Read pointers to MixerMaps
    DWORD_PTR weedMixMap_ptr = ReadMemory<DWORD_PTR>(hProcess, address + 0x160);
    DWORD_PTR methMixMap_ptr = ReadMemory<DWORD_PTR>(hProcess, address + 0x168);
    DWORD_PTR cokeMixMap_ptr = ReadMemory<DWORD_PTR>(hProcess, address + 0x170);

    std::cout << "ProductManager at 0x" << std::hex << address << std::dec << std::endl;
    std::cout << "WeedMixMap pointer: 0x" << std::hex << weedMixMap_ptr << std::dec << std::endl;
    std::cout << "MethMixMap pointer: 0x" << std::hex << methMixMap_ptr << std::dec << std::endl;
    std::cout << "CokeMixMap pointer: 0x" << std::hex << cokeMixMap_ptr << std::dec << std::endl;

    rawDataFile << "  \"mixMaps\": {\n";

    // Extract mix maps
    if (weedMixMap_ptr != 0) {
        std::cout << "Extracting WeedMixMap..." << std::endl;

        float mapRadius = ReadMemory<float>(hProcess, weedMixMap_ptr + 0x18);
        DWORD_PTR effects_ptr = ReadMemory<DWORD_PTR>(hProcess, weedMixMap_ptr + 0x20);
        int effects_count = 0;

        rawDataFile << "    \"weed\": {\n";
        rawDataFile << "      \"mapRadius\": " << mapRadius << ",\n";
        rawDataFile << "      \"effects\": [\n";

        if (effects_ptr != 0) {
            DWORD_PTR items_ptr = ReadMemory<DWORD_PTR>(hProcess, effects_ptr + 0x10);
            effects_count = ReadMemory<int>(hProcess, effects_ptr + 0x18);

            std::cout << "  Effects count: " << effects_count << std::endl;

            // Extract all effects
            for (int i = 0; i < effects_count; i++) {
                DWORD_PTR effectPtr = ReadMemory<DWORD_PTR>(hProcess, items_ptr + 0x20 + (i * 0x8));

                if (effectPtr != 0) {
                    MixerMapEffect effect = ReadMixerMapEffect(hProcess, effectPtr);

                    // Write effect data to JSON
                    rawDataFile << "        {\n";
                    rawDataFile << "          \"position\": {\"x\": " << effect.position.x << ", \"y\": " << effect.position.y << "},\n";
                    rawDataFile << "          \"radius\": " << effect.radius << ",\n";
                    rawDataFile << "          \"property\": {\n";
                    rawDataFile << "            \"id\": \"" << effect.property.id << "\",\n";
                    rawDataFile << "            \"name\": \"" << effect.property.name << "\",\n";
                    rawDataFile << "            \"tier\": " << effect.property.tier << ",\n";
                    rawDataFile << "            \"addictiveness\": " << effect.property.addictiveness << ",\n";
                    rawDataFile << "            \"valueChange\": " << effect.property.valueChange << ",\n";
                    rawDataFile << "            \"valueMultiplier\": " << effect.property.valueMultiplier << ",\n";
                    rawDataFile << "            \"addBaseValueMultiple\": " << effect.property.addBaseValueMultiple << ",\n";
                    rawDataFile << "            \"mixDirection\": {\"x\": " << effect.property.mixDirection.x
                        << ", \"y\": " << effect.property.mixDirection.y << "},\n";
                    rawDataFile << "            \"mixMagnitude\": " << effect.property.mixMagnitude << "\n";
                    rawDataFile << "          }\n";
                    rawDataFile << "        }" << (i < effects_count - 1 ? "," : "") << "\n";

                    std::cout << "  Effect[" << i << "] Position: (" << effect.position.x << ", "
                        << effect.position.y << "), Radius: " << effect.radius << std::endl;
                    std::cout << "    Property: " << effect.property.name << " (ID: " << effect.property.id << ")" << std::endl;
                }
            }
        }

        rawDataFile << "      ]\n";
        rawDataFile << "    },\n";
    }

    // Similar code for methMixMap and cokeMixMap would go here
    // For brevity, I've only included the WeedMixMap extraction

    rawDataFile << "    \"meth\": {},\n";
    rawDataFile << "    \"coke\": {}\n";
    rawDataFile << "  },\n";

    // Read ValidMixIngredients
    DWORD_PTR validMixIngredients_ptr = ReadMemory<DWORD_PTR>(hProcess, address + 0x138);
    std::vector<MixIngredient> mixIngredients;

    rawDataFile << "  \"validMixIngredients\": [\n";

    if (validMixIngredients_ptr != 0) {
        DWORD_PTR items_ptr = ReadMemory<DWORD_PTR>(hProcess, validMixIngredients_ptr + 0x10);
        int count = ReadMemory<int>(hProcess, validMixIngredients_ptr + 0x18);

        std::cout << "ValidMixIngredients count: " << count << std::endl;

        // Extract all valid mix ingredients
        for (int i = 0; i < count; i++) {
            DWORD_PTR ingredientPtr = ReadMemory<DWORD_PTR>(hProcess, items_ptr + 0x20 + (i * 0x8));

            if (ingredientPtr != 0) {
                MixIngredient ingredient;
                ingredient.ptr = ingredientPtr;

                // Read the ingredient's name - from Image 1, it's at offset 0x18
                ingredient.name = ReadString(hProcess, ingredientPtr + 0x18);

                // Read property information
                DWORD_PTR propertiesPtr = ReadMemory<DWORD_PTR>(hProcess, ingredientPtr + 0xB0);

                if (propertiesPtr != 0) {
                    DWORD_PTR props_items_ptr = ReadMemory<DWORD_PTR>(hProcess, propertiesPtr + 0x10);
                    int props_count = ReadMemory<int>(hProcess, propertiesPtr + 0x18);
                    printf("propCount: %i\n", props_count);
                    if (props_count > 0 && props_items_ptr != 0) {
                        DWORD_PTR propPtr = ReadMemory<DWORD_PTR>(hProcess, props_items_ptr + 0x20);

                        if (propPtr != 0) {
                            ingredient.property = ReadProperty(hProcess, propPtr);
                            mixIngredients.push_back(ingredient);

                            // Write ingredient data to JSON
                            rawDataFile << "    {\n";
                            rawDataFile << "      \"ingredient_name\": \"" << ingredient.name << "\",\n";
                            rawDataFile << "      \"property\": {\n";
                            rawDataFile << "        \"id\": \"" << ingredient.property.id << "\",\n";
                            rawDataFile << "        \"name\": \"" << ingredient.property.name << "\",\n";
                            rawDataFile << "        \"tier\": " << ingredient.property.tier << ",\n";
                            rawDataFile << "        \"addictiveness\": " << ingredient.property.addictiveness << ",\n";
                            rawDataFile << "        \"valueChange\": " << ingredient.property.valueChange << ",\n";
                            rawDataFile << "        \"valueMultiplier\": " << ingredient.property.valueMultiplier << ",\n";
                            rawDataFile << "        \"addBaseValueMultiple\": " << ingredient.property.addBaseValueMultiple << ",\n";
                            rawDataFile << "        \"mixDirection\": {\"x\": " << ingredient.property.mixDirection.x
                                << ", \"y\": " << ingredient.property.mixDirection.y << "},\n";
                            rawDataFile << "        \"mixMagnitude\": " << ingredient.property.mixMagnitude << "\n";
                            rawDataFile << "      }\n";
                            rawDataFile << "    }" << (i < count - 1 ? "," : "") << "\n";

                            std::cout << "Ingredient[" << i << "]: " << ingredient.name << std::endl;
                            std::cout << "  Property: " << ingredient.property.name
                                << " (ID: " << ingredient.property.id << ")" << std::endl;
                            std::cout << "  MixDirection: (" << ingredient.property.mixDirection.x << ", "
                                << ingredient.property.mixDirection.y << "), MixMagnitude: "
                                << ingredient.property.mixMagnitude << std::endl;
                        }
                    }
                }
            }
        }
    }

    rawDataFile << "  ]\n";
    rawDataFile << "}" << std::endl;
    rawDataFile.close();

    std::cout << "Raw data extracted to property_data_raw.json" << std::endl;
}

int main(int argc, char* argv[]) {
    // Process name
    std::string processName = "Schedule I.exe";

    // Default ProductManager address from screenshot
    DWORD_PTR productManagerAddress = 0x1CE74BE06C0;

    // Allow overriding the address from command line
    if (argc > 1) {
        try {
            productManagerAddress = std::stoull(argv[1], nullptr, 16);
        }
        catch (const std::exception& e) {
            std::cout << "Invalid address format. Please use hexadecimal (e.g. 0x1CE74BE06C0)" << std::endl;
            return 1;
        }
    }

    std::cout << "Looking for process: " << processName << std::endl;
    std::cout << "Using ProductManager address: 0x" << std::hex << productManagerAddress << std::dec << std::endl;

    // Get process ID
    DWORD pid = GetProcessIdByName(processName);
    if (pid == 0) {
        std::cout << "Process not found. Is the game running?" << std::endl;
        return 1;
    }

    std::cout << "Found process ID: " << pid << std::endl;

    // Open process with read access
    HANDLE hProcess = OpenProcess(PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) {
        std::cout << "Failed to open process. Error code: " << GetLastError() << std::endl;
        return 1;
    }

    // Extract data
    try {
        ExtractProductManager(hProcess, productManagerAddress);
    }
    catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    // Close handle
    CloseHandle(hProcess);

    std::cout << "Done!" << std::endl;
    return 0;
}