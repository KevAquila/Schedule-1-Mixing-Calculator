#pragma once

#include <vector>
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <algorithm>
#include <queue>
#include <sstream>
#include <iomanip>
#include "../Schedule I Mixer Sim/property_mixer_core.h"

// PropertyTransition struct for animations
struct PropertyTransition {
    Vector2 startPosition;
    Vector2 endPosition;
    Property* sourceProperty;
    Property* resultProperty;
    float animationTime;
    float totalAnimationTime;
};


// Structure for button
struct Button {
    sf::RectangleShape shape;
    sf::Text text;
    std::string id;
    bool isHovered;
    bool isActive;
    sf::Color defaultColor;
    sf::Color hoverColor;
    sf::Color activeColor;

    Button() :
        isHovered(false),
        isActive(false),
        defaultColor(sf::Color(60, 60, 80)),
        hoverColor(sf::Color(80, 80, 120)),
        activeColor(sf::Color(100, 180, 100)) {}

    bool contains(const sf::Vector2f& point) const {
        return shape.getGlobalBounds().contains(point);
    }

    void updateColor() {
        if (isActive) {
            shape.setFillColor(activeColor);
        }
        else if (isHovered) {
            shape.setFillColor(hoverColor);
        }
        else {
            shape.setFillColor(defaultColor);
        }
    }
};

// Structure for a property on the product
struct ProductProperty {
    Property* property;
    std::vector<std::string> ingredients;
};

class VisualPropertyMixer {
public:
    VisualPropertyMixer()
        : isInitialized(false)
        , window(nullptr)
        , showTransitions(true)
        , animationSpeed(0.6f)  // Reduced animation speed
        , showMixingLines(true)
        , showTooltips(true)
        , mode(Mode::Normal)
        , previewNewProperty(nullptr)
        , selectedIngredientIndex(-1)
        , windowWidth(1600)     // Increased window size
        , windowHeight(900)     // Increased window size
    {}

    ~VisualPropertyMixer() {
        if (window) {
            window->close();
            delete window;
        }
    }
    std::vector<Button> productButtons;
    std::string selectedProduct;
    int hoveredIngredientIndex = -1;

    enum class Mode {
        Normal,
        PreviewMix,
        Help
    };


    // Add new method to draw mix vectors
    void drawMixVectors() {
        if (hoveredIngredientIndex < 0 || hoveredIngredientIndex >= ingredientButtons.size()) {
            return; // No ingredient hovered
        }

        // Get the property for the hovered ingredient
        std::string ingredientId = ingredientButtons[hoveredIngredientIndex].id;
        std::string propertyId = ingredientPropertyMapping[ingredientId];
        Property* previewProperty = getPropertyByNameOrId(propertyId);

        if (!previewProperty || !mixerMap) {
            return;
        }

        const float centerX = windowWidth / 2.0f;
        const float centerY = windowHeight / 2.0f;
        const float scaleFactor = 80.0f; // Same scale as used elsewhere

        // Draw the mix direction as a vector for each active property
        for (const auto& propWithOrigin : currentProperties) {
            Property* currentProp = propWithOrigin.property;
            MixerMapEffect* effect = mixerMap->getEffect(currentProp);

            if (effect) {
                // Current property position
                float startX = centerX + effect->position.x * scaleFactor;
                float startY = centerY - effect->position.y * scaleFactor; // Y is inverted in SFML

                // Calculate where it would move to
                Vector2 endVector = effect->position +
                    (previewProperty->mixDirection * previewProperty->mixMagnitude);

                float endX = centerX + endVector.x * scaleFactor;
                float endY = centerY - endVector.y * scaleFactor; // Y is inverted in SFML

                // Draw the vector line
                sf::Vertex line[] = {
                    sf::Vertex(sf::Vector2f(startX, startY), sf::Color(255, 165, 0, 200)), // Orange start
                    sf::Vertex(sf::Vector2f(endX, endY), sf::Color(255, 0, 0, 200))        // Red end
                };
                window->draw(line, 2, sf::Lines);

                // Draw an arrow head
                float angle = std::atan2(startY - endY, endX - startX); // Note: y coords are inverted
                float arrowSize = 10.0f;

                sf::Vertex arrowHead[] = {
                    sf::Vertex(sf::Vector2f(endX, endY), sf::Color(255, 0, 0, 200)),
                    sf::Vertex(sf::Vector2f(
                        endX - arrowSize * std::cos(angle - 0.5f),
                        endY + arrowSize * std::sin(angle - 0.5f)),  // Y inverted
                        sf::Color(255, 0, 0, 200)),
                    sf::Vertex(sf::Vector2f(endX, endY), sf::Color(255, 0, 0, 200)),
                    sf::Vertex(sf::Vector2f(
                        endX - arrowSize * std::cos(angle + 0.5f),
                        endY + arrowSize * std::sin(angle + 0.5f)),  // Y inverted
                        sf::Color(255, 0, 0, 200))
                };
                window->draw(arrowHead, 4, sf::Lines);

                // Find what property, if any, would be at the end position
                MixerMapEffect* resultEffect = mixerMap->getEffectAtPoint(endVector);
                if (resultEffect) {
                    // Draw a small circle to indicate the target property
                    sf::CircleShape targetCircle(5.0f);
                    targetCircle.setFillColor(sf::Color(255, 0, 0, 200));
                    targetCircle.setPosition(endX - 5.0f, endY - 5.0f); // Center the circle
                    window->draw(targetCircle);

                    // Draw a small label with the target property name
                    sf::Text targetLabel;
                    targetLabel.setFont(font);
                    targetLabel.setString(resultEffect->property->name);
                    targetLabel.setCharacterSize(12);
                    targetLabel.setFillColor(sf::Color::White);
                    targetLabel.setOutlineColor(sf::Color::Black);
                    targetLabel.setOutlineThickness(1.0f);
                    targetLabel.setPosition(endX + 8.0f, endY - 6.0f);
                    window->draw(targetLabel);
                }
            }
        }

        // Draw a note about the preview
        sf::Text previewNote;
        previewNote.setFont(font);
        previewNote.setString("Preview: " + ingredientId + " effect on properties");
        previewNote.setCharacterSize(14);
        previewNote.setFillColor(sf::Color::Yellow);
        previewNote.setOutlineColor(sf::Color::Black);
        previewNote.setOutlineThickness(1.0f);
        previewNote.setPosition(10, windowHeight - 40);
        window->draw(previewNote);
    }

    void createProductButtons() {
        float buttonHeight = 20.0f;
        float buttonWidth = 160.0f;
        float padding = 5.0f;
        float startX = 20.0f;
        float startY = 650.0f; // Position below ingredient buttons

        int index = 0;
        for (const auto& pair : products) {
            DrugProduct* product = pair.second;

            Button button;
            button.shape.setSize(sf::Vector2f(buttonWidth, buttonHeight));
            button.shape.setPosition(startX, startY + index * (buttonHeight + padding));

            // Color by drug type
            if (product->type == DrugType::Marijuana) {
                button.defaultColor = sf::Color(40, 120, 40); // Green for marijuana
                button.hoverColor = sf::Color(60, 180, 60);
            }
            else if (product->type == DrugType::Methamphetamine) {
                button.defaultColor = sf::Color(40, 120, 40); // Purple for meth
                button.hoverColor = sf::Color(60, 180, 60);
            }
            else if (product->type == DrugType::Cocaine) {
                button.defaultColor = sf::Color(40, 120, 40); // White/Gray for cocaine
                button.hoverColor = sf::Color(60, 180, 60);
            }

            button.shape.setFillColor(button.defaultColor);
            button.shape.setOutlineColor(sf::Color(100, 100, 150));
            button.shape.setOutlineThickness(1.0f);

            button.text.setFont(font);
            button.text.setString(product->name);
            button.text.setCharacterSize(16);
            button.text.setFillColor(sf::Color::White);
            button.text.setPosition(
                button.shape.getPosition().x + padding,
                button.shape.getPosition().y + (buttonHeight - 16) / 2.0f
            );

            button.id = product->name;
            button.isHovered = false;
            button.isActive = false;

            productButtons.push_back(button);
            index++;
        }
    }
    // Method to handle product button click
    void handleProductButtonClick(const std::string& productName) {
        // Clear current properties
        currentProperties.clear();
        ingredientHistory.clear();
        activeTransitions.clear();

        selectedProduct = productName;

        // Set the starting properties for this product
        auto productIt = products.find(productName);
        if (productIt != products.end()) {
            DrugProduct* product = productIt->second;

            // Add each property to currentProperties
            for (Property* prop : product->properties) {
                ProductProperty propWithOrigin;
                propWithOrigin.property = prop;
                propWithOrigin.ingredients.push_back(productName); // The product is the source
                currentProperties.push_back(propWithOrigin);
            }
        }

        // Highlight the selected product button
        for (auto& button : productButtons) {
            button.isActive = (button.id == productName);
            button.updateColor();
        }
    }

    // Initialize the visualizer window
    void initialize() {
        if (!isInitialized) {
            window = new sf::RenderWindow(sf::VideoMode(windowWidth, windowHeight), "Visual Property Mixer", sf::Style::Default);
            window->setFramerateLimit(60);

            // Try to load font
            if (!font.loadFromFile("arial.ttf")) {
                // Fallback to another common font if arial isn't available
                if (!font.loadFromFile("C:/Windows/Fonts/arial.ttf")) {
                    // Try system fonts on different platforms
#ifdef __APPLE__
                    if (!font.loadFromFile("/System/Library/Fonts/Helvetica.ttc")) {
                        std::cerr << "Warning: Could not load any fonts. Text will not display correctly." << std::endl;
                    }
#elif __linux__
                    if (!font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
                        std::cerr << "Warning: Could not load any fonts. Text will not display correctly." << std::endl;
                    }
#endif
                }
            }

            // Initialize product manager and get mixer map
            ProductManager& productManager = ProductManager::getInstance();
            mixerMap = productManager.getMixerMap(DrugType::Marijuana);

            // Initialize ingredient mapping
            initializeIngredientMapping();

            // Create ingredient buttons
            createIngredientButtons();

            // Create action buttons
            createActionButtons();
            // Create product buttons
            createProductButtons();
            // Initialize clock for animations
            clock.restart();
            isInitialized = true;
        }
    }

    // Main loop for the visual application
    void run() {
        if (!isInitialized) {
            initialize();
        }

        while (window->isOpen()) {
            // Update time
            float deltaTime = clock.restart().asSeconds();

            // Handle events
            handleEvents();

            // Update transitions
            updateTransitions(deltaTime);

            // Clear the window with a darker background for better contrast
            window->clear(sf::Color(20, 20, 30));

            // Draw the interface
            drawInterface();

            // Display the window
            window->display();
        }
    }

private:
    bool isInitialized;
    sf::RenderWindow* window;
    sf::Font font;
    sf::Clock clock;
    MixerMap* mixerMap;

    // Window dimensions
    int windowWidth;
    int windowHeight;

    // Application mode
    Mode mode;

    // Selected ingredient for preview
    Property* previewNewProperty;
    int selectedIngredientIndex;

    // Current product properties
    std::vector<ProductProperty> currentProperties;

    // Ingredient history
    std::vector<std::string> ingredientHistory;

    // Visualization options
    bool showTransitions;
    float animationSpeed;
    bool showMixingLines;
    bool showTooltips;

    // Active property transitions for animation
    std::vector<PropertyTransition> activeTransitions;

    // Store last known positions of properties on the map
    std::map<std::string, Vector2> propertyPositions;

    // Property that mouse is hovering over (for tooltips)
    Property* hoveredProperty = nullptr;

    // Buttons
    std::vector<Button> ingredientButtons;
    std::vector<Button> actionButtons;

    // Ingredient mapping
    std::map<std::string, std::string> ingredientPropertyMapping;
    std::map<std::string, std::string> propertyToIngredientMap;

    // Color mapping for different tiers
    std::map<int, sf::Color> tierColors = {
        {1, sf::Color(60, 179, 113)},   // MediumSeaGreen
        {2, sf::Color(30, 144, 255)},   // DodgerBlue
        {3, sf::Color(255, 165, 0)},    // Orange
        {4, sf::Color(255, 69, 0)},     // OrangeRed
        {5, sf::Color(178, 34, 34)}     // Firebrick
    };

    // Initialize ingredient mapping
    void initializeIngredientMapping() {
        ingredientPropertyMapping = {
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

        // Create reverse mapping
        for (const auto& pair : ingredientPropertyMapping) {
            propertyToIngredientMap[pair.second] = pair.first;
        }
    }

    // Create buttons for ingredients
    void createIngredientButtons() {
        float startY = 60.0f;
        float buttonHeight = 20.0f;  // Increased button height
        float buttonWidth = 120.f;  // Increased button width
        float padding = 10.0f;
        float startX = windowWidth - buttonWidth - padding - 20.f;

        int index = 0;
        for (const auto& pair : ingredientPropertyMapping) {
            Button button;
            button.shape.setSize(sf::Vector2f(buttonWidth, buttonHeight));
            button.shape.setPosition(startX, startY + index * (buttonHeight + padding));
            button.defaultColor = sf::Color(40, 40, 60);
            button.hoverColor = sf::Color(60, 60, 100);
            button.activeColor = sf::Color(100, 180, 100);
            button.shape.setFillColor(button.defaultColor);
            button.shape.setOutlineColor(sf::Color(100, 100, 150));
            button.shape.setOutlineThickness(1.0f);

            button.text.setFont(font);
            button.text.setString(pair.first);
            button.text.setCharacterSize(16);  // Increased font size
            button.text.setFillColor(sf::Color::White);
            button.text.setPosition(
                button.shape.getPosition().x + padding,
                button.shape.getPosition().y + (buttonHeight - 16) / 2.0f
            );

            button.id = pair.first;
            button.isHovered = false;
            button.isActive = false;

            ingredientButtons.push_back(button);
            index++;
        }
    }

    // Create action buttons
    void createActionButtons() {
        float buttonHeight = 40.0f;  // Increased button height
        float buttonWidth = 150.0f;  // Increased button width
        float padding = 10.0f;
        float startY = windowHeight - buttonHeight - padding;

        // Create Reset button
        Button resetButton;
        resetButton.shape.setSize(sf::Vector2f(buttonWidth, buttonHeight));
        resetButton.shape.setPosition(padding, startY);
        resetButton.defaultColor = sf::Color(180, 60, 60);
        resetButton.hoverColor = sf::Color(220, 80, 80);
        resetButton.activeColor = sf::Color(180, 60, 60);
        resetButton.shape.setFillColor(resetButton.defaultColor);
        resetButton.shape.setOutlineColor(sf::Color(100, 100, 150));
        resetButton.shape.setOutlineThickness(1.0f);

        resetButton.text.setFont(font);
        resetButton.text.setString("Reset");
        resetButton.text.setCharacterSize(16);  // Increased font size
        resetButton.text.setFillColor(sf::Color::White);
        resetButton.text.setPosition(
            resetButton.shape.getPosition().x + (buttonWidth - resetButton.text.getLocalBounds().width) / 2.0f,
            resetButton.shape.getPosition().y + (buttonHeight - 16) / 2.0f
        );

        resetButton.id = "reset";
        resetButton.isHovered = false;

        actionButtons.push_back(resetButton);

        // Create Help button
        Button helpButton;
        helpButton.shape.setSize(sf::Vector2f(buttonWidth, buttonHeight));
        helpButton.shape.setPosition(padding * 2 + buttonWidth, startY);
        helpButton.defaultColor = sf::Color(60, 60, 180);
        helpButton.hoverColor = sf::Color(80, 80, 220);
        helpButton.activeColor = sf::Color(60, 60, 180);
        helpButton.shape.setFillColor(helpButton.defaultColor);
        helpButton.shape.setOutlineColor(sf::Color(100, 100, 150));
        helpButton.shape.setOutlineThickness(1.0f);

        helpButton.text.setFont(font);
        helpButton.text.setString("Help");
        helpButton.text.setCharacterSize(16);  // Increased font size
        helpButton.text.setFillColor(sf::Color::White);
        helpButton.text.setPosition(
            helpButton.shape.getPosition().x + (buttonWidth - helpButton.text.getLocalBounds().width) / 2.0f,
            helpButton.shape.getPosition().y + (buttonHeight - 16) / 2.0f
        );

        helpButton.id = "help";
        helpButton.isHovered = false;

        actionButtons.push_back(helpButton);

        // Create Confirm Mix button (initially hidden)
        Button confirmButton;
        confirmButton.shape.setSize(sf::Vector2f(buttonWidth * 1.5f, buttonHeight));
        confirmButton.shape.setPosition(windowWidth - buttonWidth * 1.5f - padding, startY);
        confirmButton.defaultColor = sf::Color(60, 180, 60);
        confirmButton.hoverColor = sf::Color(80, 220, 80);
        confirmButton.activeColor = sf::Color(60, 180, 60);
        confirmButton.shape.setFillColor(confirmButton.defaultColor);
        confirmButton.shape.setOutlineColor(sf::Color(100, 100, 150));
        confirmButton.shape.setOutlineThickness(1.0f);

        confirmButton.text.setFont(font);
        confirmButton.text.setString("Confirm Mix");
        confirmButton.text.setCharacterSize(16);  // Increased font size
        confirmButton.text.setFillColor(sf::Color::White);
        confirmButton.text.setPosition(
            confirmButton.shape.getPosition().x + (buttonWidth * 1.5f - confirmButton.text.getLocalBounds().width) / 2.0f,
            confirmButton.shape.getPosition().y + (buttonHeight - 16) / 2.0f
        );

        confirmButton.id = "confirm";
        confirmButton.isHovered = false;

        actionButtons.push_back(confirmButton);

        // Create Cancel Mix button (initially hidden)
        Button cancelButton;
        cancelButton.shape.setSize(sf::Vector2f(buttonWidth, buttonHeight));
        cancelButton.shape.setPosition(windowWidth - buttonWidth * 2.5f - padding * 2, startY);
        cancelButton.defaultColor = sf::Color(180, 60, 60);
        cancelButton.hoverColor = sf::Color(220, 80, 80);
        cancelButton.activeColor = sf::Color(180, 60, 60);
        cancelButton.shape.setFillColor(cancelButton.defaultColor);
        cancelButton.shape.setOutlineColor(sf::Color(100, 100, 150));
        cancelButton.shape.setOutlineThickness(1.0f);

        cancelButton.text.setFont(font);
        cancelButton.text.setString("Cancel");
        cancelButton.text.setCharacterSize(16);  // Increased font size
        cancelButton.text.setFillColor(sf::Color::White);
        cancelButton.text.setPosition(
            cancelButton.shape.getPosition().x + (buttonWidth - cancelButton.text.getLocalBounds().width) / 2.0f,
            cancelButton.shape.getPosition().y + (buttonHeight - 16) / 2.0f
        );

        cancelButton.id = "cancel";
        cancelButton.isHovered = false;

        actionButtons.push_back(cancelButton);
    }

    // Handle window events
    void handleEvents() {
        sf::Event event;
        while (window->pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window->close();
            }
            else if (event.type == sf::Event::KeyPressed) {
                handleKeyPress(event.key.code);
            }
            else if (event.type == sf::Event::MouseMoved) {
                handleMouseMove(event.mouseMove.x, event.mouseMove.y);
            }
            else if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    handleMouseClick(event.mouseButton.x, event.mouseButton.y);
                }
            }
        }
    }

    // Handle keyboard input
    void handleKeyPress(sf::Keyboard::Key key) {
        // Toggle visualization options
        if (key == sf::Keyboard::T) {
            showTransitions = !showTransitions;
        }
        else if (key == sf::Keyboard::L) {
            showMixingLines = !showMixingLines;
        }
        else if (key == sf::Keyboard::I) {
            showTooltips = !showTooltips;
        }
        // Adjust animation speed
        else if (key == sf::Keyboard::Add || key == sf::Keyboard::Equal) {
            animationSpeed = std::min(animationSpeed + 0.1f, 2.0f);
        }
        else if (key == sf::Keyboard::Subtract || key == sf::Keyboard::Dash) {
            animationSpeed = std::max(animationSpeed - 0.1f, 0.1f);
        }
        // Exit preview mode with Escape
        else if (key == sf::Keyboard::Escape) {
            if (mode == Mode::PreviewMix) {
                cancelPreview();
            }
            else if (mode == Mode::Help) {
                mode = Mode::Normal;
            }
        }
    }

    // Handle mouse movement for hover effects
    void handleMouseMove(int x, int y) {
        sf::Vector2f mousePos(static_cast<float>(x), static_cast<float>(y));



        // Check ingredient buttons
        hoveredIngredientIndex = -1;
        for (size_t i = 0; i < ingredientButtons.size(); i++) {
            ingredientButtons[i].isHovered = ingredientButtons[i].contains(mousePos);
            ingredientButtons[i].updateColor();

            if (ingredientButtons[i].isHovered) {
                hoveredIngredientIndex = i;
            }
        }

        // Check action buttons
        for (auto& button : actionButtons) {
            button.isHovered = button.contains(mousePos);
            button.updateColor();
        }

        // Check product buttons
        for (auto& button : productButtons) {
            button.isHovered = button.contains(mousePos);
            button.updateColor();
        }

        // Check property hover on the map
        checkPropertyHover(mousePos);
    }

    // Check if mouse is hovering over a property on the map
    void checkPropertyHover(const sf::Vector2f& mousePos) {
        if (!mixerMap) return;

        hoveredProperty = nullptr;

        const float centerX = windowWidth / 2.0f;
        const float centerY = windowHeight / 2.0f;
        const float scaleFactor = 80.0f; // Increased scale factor for larger map

        for (auto* effect : mixerMap->effects) {
            float screenX = centerX + effect->position.x * scaleFactor;
            float screenY = centerY - effect->position.y * scaleFactor; // Y is inverted in SFML

            float distance = std::sqrt(
                std::pow(mousePos.x - screenX, 2) +
                std::pow(mousePos.y - screenY, 2)
            );

            if (distance <= effect->radius * scaleFactor) {
                hoveredProperty = effect->property;
                break;
            }
        }
    }

    // Handle mouse clicks
    void handleMouseClick(int x, int y) {
        sf::Vector2f mousePos(static_cast<float>(x), static_cast<float>(y));

        // Handle clicks in normal mode
        if (mode == Mode::Normal) {

            for (const auto& button : productButtons) {
                if (button.contains(mousePos)) {
                    handleProductButtonClick(button.id);
                    return;
                }
            }


            // Check ingredient buttons
            for (size_t i = 0; i < ingredientButtons.size(); i++) {
                if (ingredientButtons[i].contains(mousePos)) {
                    handleIngredientClick(i);
                    return;
                }
            }

            // Check action buttons
            for (const auto& button : actionButtons) {
                if (button.contains(mousePos)) {
                    handleActionButtonClick(button.id);
                    return;
                }
            }
        }
        // Handle clicks in preview mode
        else if (mode == Mode::PreviewMix) {
            // Check confirm/cancel buttons
            for (const auto& button : actionButtons) {
                if (button.contains(mousePos) && (button.id == "confirm" || button.id == "cancel")) {
                    handleActionButtonClick(button.id);
                    return;
                }
            }

            // Also allow clicking on a different ingredient to switch preview
            for (size_t i = 0; i < ingredientButtons.size(); i++) {
                if (ingredientButtons[i].contains(mousePos)) {
                    // Cancel current preview and start a new one
                    cancelPreview();
                    handleIngredientClick(i);
                    return;
                }
            }
        }
        // Handle clicks in help mode
        else if (mode == Mode::Help) {
            // Any click returns to normal mode
            mode = Mode::Normal;
        }
    }

    // Handle ingredient button click
    void handleIngredientClick(int index) {
        // Get the property for this ingredient
        std::string ingredientName = ingredientButtons[index].id;
        std::string propertyId = ingredientPropertyMapping[ingredientName];
        Property* newProperty = getPropertyByNameOrId(propertyId);

        if (newProperty) {
            // Set up preview mode
            previewNewProperty = newProperty;
            selectedIngredientIndex = index;
            mode = Mode::PreviewMix;

            // Highlight the selected button
            for (auto& button : ingredientButtons) {
                button.isActive = (button.id == ingredientName);
                button.updateColor();
            }
        }
    }

    // Handle action button click
    void handleActionButtonClick(const std::string& buttonId) {
        if (buttonId == "reset") {
            // Reset all properties
            currentProperties.clear();
            ingredientHistory.clear();
            // Clear any active transitions
            activeTransitions.clear();
        }
        else if (buttonId == "help") {
            // Show help screen
            mode = Mode::Help;
        }
        else if (buttonId == "confirm" && mode == Mode::PreviewMix) {
            // Confirm mixing the previewed ingredient
            confirmMix();
        }
        else if (buttonId == "cancel" && mode == Mode::PreviewMix) {
            // Cancel the preview
            cancelPreview();
        }
    }

    // Cancel preview mode
    void cancelPreview() {
        previewNewProperty = nullptr;
        mode = Mode::Normal;

        // Reset button highlights
        for (auto& button : ingredientButtons) {
            button.isActive = false;
            button.updateColor();
        }
    }

    // Confirm mixing the previewed ingredient
    // Update the confirmMix method in VisualPropertyMixer
    void confirmMix() {
        if (!previewNewProperty) {
            cancelPreview();
            return;
        }

        // Get the ingredient name
        std::string ingredientName = ingredientButtons[selectedIngredientIndex].id;

        // Add to ingredient history
        ingredientHistory.push_back(ingredientName);

        // Extract just the properties without origin info for mixing
        std::vector<Property*> currentProps;
        for (const auto& propWithOrigin : currentProperties) {
            currentProps.push_back(propWithOrigin.property);
        }

        // Store pre-mixing properties to track transitions
        std::vector<Property*> beforeProperties = currentProps;

        // Mix the properties with the updated calculator
        std::vector<Property*> result = PropertyMixCalculator::mixProperties(
            currentProps, previewNewProperty, DrugType::Marijuana);

        // Create new properties with origin information
        std::vector<ProductProperty> newProperties;

        // For each property in the result, determine its origin
        for (Property* resultProp : result) {
            ProductProperty newProp;
            newProp.property = resultProp;

            // Check if this property was in the previous set
            bool wasInPrevious = false;
            for (const auto& prevProp : currentProperties) {
                if (prevProp.property == resultProp) { // Direct pointer comparison
                    // This property was in the previous set, copy its ingredient list
                    newProp.ingredients = prevProp.ingredients;
                    wasInPrevious = true;
                    break;
                }
            }

            // If it's the new property we just added
            if (!wasInPrevious && resultProp == previewNewProperty) { // Direct pointer comparison
                // This is just the ingredient we added
                newProp.ingredients.push_back(ingredientName);
            }
            // If it's a new property created by mixing
            else if (!wasInPrevious) {
                // This is a result of mixing, so list all ingredients
                for (const auto& prevIngredient : ingredientHistory) {
                    if (std::find(newProp.ingredients.begin(),
                        newProp.ingredients.end(),
                        prevIngredient) == newProp.ingredients.end()) {
                        newProp.ingredients.push_back(prevIngredient);
                    }
                }
            }

            newProperties.push_back(newProp);
        }

        // Find property transitions for animation
        // For each property that was there before
        for (auto* beforeProp : beforeProperties) {
            // Check if it still exists in the result
            bool stillExists = false;
            for (auto* afterProp : result) {
                if (beforeProp == afterProp) { // Direct pointer comparison
                    stillExists = true;
                    break;
                }
            }

            // If not found in result, it was transformed
            if (!stillExists) {
                // Find the effect for this property
                MixerMapEffect* beforeEffect = mixerMap->getEffect(beforeProp);
                if (beforeEffect) {
                    // Calculate where it moved to
                    Vector2 movePos = beforeEffect->position +
                        (previewNewProperty->mixDirection * previewNewProperty->mixMagnitude);

                    // Find the effect at that position
                    MixerMapEffect* afterEffect = mixerMap->getEffectAtPoint(movePos);
                    if (afterEffect) {
                        // Add a transition animation
                        PropertyTransition transition;
                        transition.startPosition = beforeEffect->position;
                        transition.endPosition = afterEffect->position;
                        transition.sourceProperty = beforeProp;
                        transition.resultProperty = afterEffect->property;
                        transition.animationTime = 0.0f;
                        transition.totalAnimationTime = 1.5f;

                        activeTransitions.push_back(transition);
                    }
                }
            }
        }

        // Update current properties
        currentProperties = newProperties;

        // Exit preview mode
        cancelPreview();
    }
    // Update any active property transitions
    void updateTransitions(float deltaTime) {
        // Update animation time for all transitions
        for (auto& transition : activeTransitions) {
            transition.animationTime += deltaTime * animationSpeed;
        }

        // Remove completed transitions
        activeTransitions.erase(
            std::remove_if(activeTransitions.begin(), activeTransitions.end(),
                [](const PropertyTransition& t) {
                    return t.animationTime >= t.totalAnimationTime;
                }
            ),
            activeTransitions.end()
                    );
    }

    // Draw the interface
    void drawInterface() {
        // Draw the mixer map
        drawMixerMap();

        // Draw transition animations
        if (showTransitions) {
            drawTransitions();
        }

        // Draw mixing lines showing connections between properties
        if (showMixingLines) {
            drawMixingLines();
        }

        // Draw vector preview when hovering over ingredients
        if (hoveredIngredientIndex >= 0 && mode == Mode::Normal) {
            drawMixVectors();
        }

        // Draw the stats panel
        drawStatsPanel();

        // Draw ingredients panel
        drawIngredientsPanel();
        // Draw products panel
        drawProductsPanel();

        // Draw action buttons
        drawActionButtons();

        // Draw preview if in preview mode
        if (mode == Mode::PreviewMix && previewNewProperty) {
            drawPreview();
        }

        // Draw help screen if in help mode
        if (mode == Mode::Help) {
            drawHelpScreen();
        }

        // Draw tooltip for hovered property
        if (hoveredProperty && showTooltips && mode != Mode::Help) {
            sf::Vector2i mousePos = sf::Mouse::getPosition(*window);
            drawPropertyTooltip(hoveredProperty, sf::Vector2f(mousePos.x, mousePos.y));
        }
    }

    // Draw the mixer map with all effects
    void drawMixerMap() {
        if (!mixerMap) return;

        const float centerX = windowWidth / 2.0f;
        const float centerY = windowHeight / 2.0f;
        const float scaleFactor = 80.0f; // Increased scale factor for larger map

        // Draw the map boundary circle
        sf::CircleShape mapBoundary(mixerMap->mapRadius * scaleFactor);
        mapBoundary.setFillColor(sf::Color(30, 30, 50, 100));
        mapBoundary.setOutlineColor(sf::Color(100, 100, 200));
        mapBoundary.setOutlineThickness(2.0f);
        mapBoundary.setPosition(centerX - mapBoundary.getRadius(), centerY - mapBoundary.getRadius());
        window->draw(mapBoundary);

        // Draw grid lines
        for (int i = 1; i <= (int)mixerMap->mapRadius; i++) {
            sf::CircleShape gridCircle(i * scaleFactor);
            gridCircle.setFillColor(sf::Color::Transparent);
            gridCircle.setOutlineColor(sf::Color(70, 70, 120, 100));
            gridCircle.setOutlineThickness(1.0f);
            gridCircle.setPosition(centerX - gridCircle.getRadius(), centerY - gridCircle.getRadius());
            window->draw(gridCircle);
        }

        // Draw coordinate axes
        sf::Vertex xAxis[] = {
            sf::Vertex(sf::Vector2f(centerX - mapBoundary.getRadius(), centerY), sf::Color(120, 120, 200, 150)),
            sf::Vertex(sf::Vector2f(centerX + mapBoundary.getRadius(), centerY), sf::Color(120, 120, 200, 150))
        };
        sf::Vertex yAxis[] = {
            sf::Vertex(sf::Vector2f(centerX, centerY - mapBoundary.getRadius()), sf::Color(120, 120, 200, 150)),
            sf::Vertex(sf::Vector2f(centerX, centerY + mapBoundary.getRadius()), sf::Color(120, 120, 200, 150))
        };
        window->draw(xAxis, 2, sf::Lines);
        window->draw(yAxis, 2, sf::Lines);
                // Draw the effects (circles)
                for (auto* effect : mixerMap->effects) {
                    // Calculate screen position
                    float screenX = centerX + effect->position.x * scaleFactor;
                    float screenY = centerY - effect->position.y * scaleFactor; // Y is inverted in SFML

                    // Store property position for other visualizations
                    propertyPositions[effect->property->id] = Vector2(screenX, screenY);

                    // Draw the effect circle
                    sf::CircleShape effectCircle(effect->radius * scaleFactor);

                    // Get color based on property tier
                    sf::Color effectColor = tierColors[effect->property->tier];

                    // Make hovering effect brighter
                    if (effect->property == hoveredProperty) {
                        effectColor = sf::Color(
                            std::min(effectColor.r + 50, 255),
                            std::min(effectColor.g + 50, 255),
                            std::min(effectColor.b + 50, 255)
                        );
                    }

                    // Check if this property is in our current properties
                    bool isActive = false;
                    for (const auto& prop : currentProperties) {
                        if (prop.property->id == effect->property->id) {
                            isActive = true;
                            break;
                        }
                    }

                    // Make active properties more visible
                    if (isActive) {
                        effectCircle.setFillColor(sf::Color(effectColor.r, effectColor.g, effectColor.b, 200)); // Less transparent
                        effectCircle.setOutlineColor(sf::Color::White);
                        effectCircle.setOutlineThickness(3.0f);
                    }
                    else {
                        effectCircle.setFillColor(sf::Color(effectColor.r, effectColor.g, effectColor.b, 100)); // More transparent
                        effectCircle.setOutlineColor(effectColor);
                        effectCircle.setOutlineThickness(2.0f);
                    }

                    effectCircle.setPosition(screenX - effectCircle.getRadius(), screenY - effectCircle.getRadius());
                    window->draw(effectCircle);

                    // Draw the property name with better contrast
                    sf::Text propertyText;
                    propertyText.setFont(font);
                    propertyText.setString(effect->property->name + "\n    Tier" + std::to_string(effect->property->tier));
                    propertyText.setCharacterSize(14);  // Increased font size
                    if (isActive) {
                        propertyText.setFillColor(sf::Color::White);
                        propertyText.setStyle(sf::Text::Bold);
                        
                    }
                    else {
                        propertyText.setFillColor(sf::Color(220, 220, 220));
                    }
                    // Add black outline for better readability
                    propertyText.setOutlineColor(sf::Color::Black);
                    propertyText.setOutlineThickness(1.0f);
                    propertyText.setPosition(screenX - propertyText.getLocalBounds().width / 2.0f,
                        screenY - propertyText.getLocalBounds().height / 2.0f);
                    window->draw(propertyText);

                    //// Draw the tier number with small circle
                    //sf::CircleShape tierCircle(12.0f);  // Slightly larger
                    //tierCircle.setFillColor(effectColor);
                    //tierCircle.setPosition(screenX - 12.0f, screenY - effectCircle.getRadius() - 30.0f);
                    //window->draw(tierCircle);

                    //sf::Text tierText;
                    //tierText.setFont(font);
                    //tierText.setString(std::to_string(effect->property->tier));
                    //tierText.setCharacterSize(14);  // Increased font size
                    //tierText.setFillColor(sf::Color::White);
                    //tierText.setPosition(screenX - 5.0f, screenY - effectCircle.getRadius() - 30.0f);
                    //window->draw(tierText);
                }
        }

        // Draw property transitions (animations)
        void drawTransitions() {
            const float centerX = windowWidth / 2.0f;
            const float centerY = windowHeight / 2.0f;
            const float scaleFactor = 80.0f;

            for (const auto& transition : activeTransitions) {
                // Calculate interpolated position
                float t = std::min(transition.animationTime / transition.totalAnimationTime, 1.0f);
                float easeT = easeInOutCubic(t); // Apply easing function

                float x = transition.startPosition.x + easeT * (transition.endPosition.x - transition.startPosition.x);
                float y = transition.startPosition.y + easeT * (transition.endPosition.y - transition.startPosition.y);

                // Convert to screen coordinates
                float screenX = centerX + x * scaleFactor;
                float screenY = centerY - y * scaleFactor; // Y is inverted in SFML

                // Draw path line
                sf::Vertex path[] = {
                    sf::Vertex(sf::Vector2f(
                        centerX + transition.startPosition.x * scaleFactor,
                        centerY - transition.startPosition.y * scaleFactor),
                        sf::Color(255, 255, 255, 100)),  // Brighter line
                    sf::Vertex(sf::Vector2f(screenX, screenY),
                        sf::Color(255, 255, 255, 200))   // Brighter line
                };
                window->draw(path, 2, sf::Lines);

                // Draw the moving property circle
                sf::CircleShape transitionCircle(20.0f);  // Larger circle
                sf::Color startColor = tierColors[transition.sourceProperty->tier];
                sf::Color endColor = tierColors[transition.resultProperty->tier];
                sf::Color currentColor(
                    startColor.r + easeT * (endColor.r - startColor.r),
                    startColor.g + easeT * (endColor.g - startColor.g),
                    startColor.b + easeT * (endColor.b - startColor.b)
                );

                transitionCircle.setFillColor(currentColor);
                transitionCircle.setPosition(screenX - transitionCircle.getRadius(),
                    screenY - transitionCircle.getRadius());
                transitionCircle.setOutlineColor(sf::Color::White);
                transitionCircle.setOutlineThickness(2.0f);
                window->draw(transitionCircle);

                // Draw property name above the circle with better contrast
                sf::Text propText;
                propText.setFont(font);
                propText.setString(transition.sourceProperty->name + " → " + transition.resultProperty->name);
                propText.setCharacterSize(14);  // Increased font size
                propText.setFillColor(sf::Color::White);
                propText.setOutlineColor(sf::Color::Black);
                propText.setOutlineThickness(1.0f);
                propText.setPosition(screenX - propText.getLocalBounds().width / 2.0f,
                    screenY - transitionCircle.getRadius() - 25.0f);
                window->draw(propText);
            }
        }

        // Draw lines showing mixing connections between properties
        void drawMixingLines() {
            if (currentProperties.size() <= 1) return;

            // For each pair of properties, draw a connection line
            for (size_t i = 0; i < currentProperties.size(); i++) {
                for (size_t j = i + 1; j < currentProperties.size(); j++) {
                    // Get positions from our stored map
                    auto posIt1 = propertyPositions.find(currentProperties[i].property->id);
                    auto posIt2 = propertyPositions.find(currentProperties[j].property->id);

                    if (posIt1 != propertyPositions.end() && posIt2 != propertyPositions.end()) {
                        // Draw white line with dashed effect for better visibility
                        sf::Color lineColor(150, 250, 150, 100);  // Light green with transparency

                        sf::Vertex line[] = {
                            sf::Vertex(sf::Vector2f(posIt1->second.x, posIt1->second.y), lineColor),
                            sf::Vertex(sf::Vector2f(posIt2->second.x, posIt2->second.y), lineColor)
                        };
                        window->draw(line, 2, sf::Lines);
                    }
                }
            }
        }

        // Draw the property tooltip
        void drawPropertyTooltip(Property* property, const sf::Vector2f& mousePos) {
            if (!property) return;

            // Create tooltip background
            sf::RectangleShape tooltip(sf::Vector2f(300, 180));  // Larger tooltip
            tooltip.setFillColor(sf::Color(20, 20, 30, 230));
            tooltip.setOutlineColor(tierColors[property->tier]);
            tooltip.setOutlineThickness(2);

            // Position tooltip near the mouse, but ensure it stays within window bounds
            float tooltipX = mousePos.x + 15;
            float tooltipY = mousePos.y + 15;

            // Adjust if tooltip would go off screen
            if (tooltipX + tooltip.getSize().x > windowWidth) {
                tooltipX = mousePos.x - tooltip.getSize().x - 15;
            }
            if (tooltipY + tooltip.getSize().y > windowHeight) {
                tooltipY = mousePos.y - tooltip.getSize().y - 15;
            }

            tooltip.setPosition(tooltipX, tooltipY);
            window->draw(tooltip);

            // Add property details to tooltip
            sf::Text nameText;
            nameText.setFont(font);
            nameText.setString(property->name + " (Tier " + std::to_string(property->tier) + ")");
            nameText.setCharacterSize(16);  // Increased font size
            nameText.setStyle(sf::Text::Bold);
            nameText.setFillColor(tierColors[property->tier]);
            nameText.setPosition(tooltipX + 15, tooltipY + 15);
            window->draw(nameText);

            // Property stats
            sf::Text statsText;
            statsText.setFont(font);
            std::stringstream ss;
            ss << std::fixed << std::setprecision(2);
            ss << "ID: " << property->id << "\n"
                << "Addictiveness: " << property->addictiveness << "\n"
                << "Base Value Add: " << property->addBaseValueMultiple << "\n"
                << "Value Multiplier: " << property->valueMultiplier << "\n"
                << "Mix Direction: (" << property->mixDirection.x << ", "
                << property->mixDirection.y << ")\n"
                << "Mix Magnitude: " << property->mixMagnitude;

            statsText.setString(ss.str());
            statsText.setCharacterSize(14);  // Increased font size
            statsText.setFillColor(sf::Color::White);
            statsText.setPosition(tooltipX + 15, tooltipY + 45);
            window->draw(statsText);

            // Check if this is an ingredient
            auto it = propertyToIngredientMap.find(property->id);
            if (it != propertyToIngredientMap.end()) {
                sf::Text ingredientText;
                ingredientText.setFont(font);
                ingredientText.setString("Ingredient: " + it->second);
                ingredientText.setCharacterSize(14);  // Increased font size
                ingredientText.setStyle(sf::Text::Bold);
                ingredientText.setFillColor(sf::Color(200, 200, 100));
                ingredientText.setPosition(tooltipX + 15, tooltipY + 145);
                window->draw(ingredientText);
            }
        }

        // Add a new method to draw the products panel
        void drawProductsPanel() {
            float panelWidth = 180.f;
            float panelHeight = 210.0f;
            float startX = 10.0f;
            float startY = 600.0f; // Position below ingredient panel

            // Draw panel background
            sf::RectangleShape panel(sf::Vector2f(panelWidth, panelHeight));
            panel.setFillColor(sf::Color(20, 20, 30, 200));
            panel.setOutlineColor(sf::Color(100, 100, 150));
            panel.setOutlineThickness(1.0f);
            panel.setPosition(startX, startY);
            window->draw(panel);

            // Title
            sf::Text titleText;
            titleText.setFont(font);
            titleText.setString("Select Product");
            titleText.setCharacterSize(20);
            titleText.setFillColor(sf::Color::White);
            titleText.setStyle(sf::Text::Bold);
            titleText.setPosition(startX + 15.0f, startY + 15.0f);
            window->draw(titleText);

            // Draw product buttons
            for (auto& button : productButtons) {
                window->draw(button.shape);
                window->draw(button.text);
            }

            // Display selected product info if one is selected
            if (!selectedProduct.empty()) {
                float infoY = startY + 60.0f + productButtons.size() * 40.0f;

                sf::Text selectedText;
                selectedText.setFont(font);
                selectedText.setString("Selected: " + selectedProduct);
                selectedText.setCharacterSize(16);
                selectedText.setFillColor(sf::Color(100, 200, 100));
                selectedText.setStyle(sf::Text::Bold);
                selectedText.setPosition(startX + 15.0f, infoY);
                window->draw(selectedText);
            }
        }


        // Draw the stats panel
        void drawStatsPanel() {
            const float panelWidth = 350.0f;  // Larger panel
            const float panelHeight = 450.0f + 20.f * currentProperties.size();  // Larger panel
            const float startX = 10.0f;
            const float startY = 10.0f;
            const float lineHeight = 30.0f;  // Increased line height
            const float padding = 15.0f;

            // Draw panel background
            sf::RectangleShape panel(sf::Vector2f(panelWidth, panelHeight));
            panel.setFillColor(sf::Color(20, 20, 30, 200));
            panel.setOutlineColor(sf::Color(100, 100, 150));
            panel.setOutlineThickness(1.0f);
            panel.setPosition(startX, startY);
            window->draw(panel);

            // Title
            sf::Text titleText;
            titleText.setFont(font);
            titleText.setString("Property Stats");
            titleText.setCharacterSize(20);  // Increased font size
            titleText.setFillColor(sf::Color::White);
            titleText.setStyle(sf::Text::Bold);
            titleText.setPosition(startX + padding, startY + padding);
            window->draw(titleText);

            // If no properties, show message
            if (currentProperties.empty()) {
                sf::Text noPropsText;
                noPropsText.setFont(font);

                if (selectedProduct.empty()) {
                    noPropsText.setString("No properties yet.\nSelect a product to begin.");
                }
                else {
                    noPropsText.setString("Selected product: " + selectedProduct +
                        "\nNo properties added yet.\nSelect an ingredient to add.");
                }

                noPropsText.setCharacterSize(16);
                noPropsText.setFillColor(sf::Color(180, 180, 180));
                noPropsText.setPosition(startX + padding, startY + padding + lineHeight * 2);
                window->draw(noPropsText);
                return;
            }

            // Calculate cumulative stats
            float totalAddictiveness = 0.0f;
            float totalBaseValueMultiple = 0.0f;
            float totalValueMultiplier = 1.0f;
            int totalValueChange = 0;

            for (const auto& prop : currentProperties) {
                totalAddictiveness += prop.property->addictiveness;
                totalBaseValueMultiple += prop.property->addBaseValueMultiple;
                totalValueMultiplier *= prop.property->valueMultiplier;
                totalValueChange += prop.property->valueChange;
            }

            // Draw current properties heading
            sf::Text propsTitle;
            propsTitle.setFont(font);
            propsTitle.setString("Current Properties:");
            propsTitle.setCharacterSize(16);  // Increased font size
            propsTitle.setFillColor(sf::Color::White);
            propsTitle.setStyle(sf::Text::Bold);
            propsTitle.setPosition(startX + padding, startY + padding + lineHeight * 1.5f);
            window->draw(propsTitle);

            // Draw property list (limited to what fits)
            float y = startY + padding + lineHeight * 2.5f;
            int maxToShow = 10; // Limit properties shown to fit in panel
            int shown = 0;

            for (size_t i = 0; i < currentProperties.size() && shown < maxToShow; i++) {
                Property* prop = currentProperties[i].property;

                sf::Text propText;
                propText.setFont(font);
                propText.setString(std::to_string(i + 1) + ". " + prop->name + " (Tier " + std::to_string(prop->tier) + ")");
                propText.setCharacterSize(16);  // Increased font size
                propText.setFillColor(tierColors[prop->tier]);
                propText.setPosition(startX + padding * 2, y);
                window->draw(propText);

                y += lineHeight;
                shown++;
            }

            // If there are more properties than we can show
            if (currentProperties.size() > maxToShow) {
                sf::Text moreText;
                moreText.setFont(font);
                moreText.setString("... and " + std::to_string(currentProperties.size() - maxToShow) + " more");
                moreText.setCharacterSize(14);
                moreText.setFillColor(sf::Color(150, 150, 150));
                moreText.setPosition(startX + padding * 2, y);
                window->draw(moreText);
                y += lineHeight;
            }

            y += lineHeight / 2;

            // Draw stats title
            sf::Text statsTitle;
            statsTitle.setFont(font);
            statsTitle.setString("Cumulative Stats:");
            statsTitle.setCharacterSize(16);  // Increased font size
            statsTitle.setFillColor(sf::Color::White);
            statsTitle.setStyle(sf::Text::Bold);
            statsTitle.setPosition(startX + padding, y);
            window->draw(statsTitle);

            y += lineHeight * 1.5f;

            // Draw stats bars
            drawStatsBar("Addictiveness", totalAddictiveness, 1.0f, startX + padding, y, panelWidth - padding * 2, 20.0f);
            y += lineHeight * 1.5f;

            drawStatsBar("Base Value Bonus", totalBaseValueMultiple, 4.0f, startX + padding, y, panelWidth - padding * 2, 20.0f);
            y += lineHeight * 1.5f;

            // Value multiplier text
            sf::Text multText;
            multText.setFont(font);
            multText.setString("Value Multiplier: " + std::to_string(totalValueMultiplier));
            multText.setCharacterSize(16);  // Increased font size
            multText.setFillColor(sf::Color::White);
            multText.setPosition(startX + padding, y);
            window->draw(multText);
            y += lineHeight;

            // Value change text
            sf::Text changeText;
            changeText.setFont(font);
            changeText.setString("Value Change: " + std::to_string(totalValueChange));
            changeText.setCharacterSize(16);  // Increased font size
            changeText.setFillColor(sf::Color::White);
            changeText.setPosition(startX + padding, y);
            window->draw(changeText);
            y += lineHeight * 1.5f;

            // Final value formula
            sf::RectangleShape formulaBox(sf::Vector2f(panelWidth - padding * 2, lineHeight * 2));
            formulaBox.setFillColor(sf::Color(40, 40, 60));
            formulaBox.setOutlineColor(sf::Color(100, 100, 150));
            formulaBox.setOutlineThickness(1.0f);
            formulaBox.setPosition(startX + padding, y);
            window->draw(formulaBox);

            // Create formatted string with fixed precision
            std::stringstream formula;
            formula << std::fixed << std::setprecision(2);
            formula << "Final Value = Base Value * (1 + " << totalBaseValueMultiple << ") * "
                << totalValueMultiplier << " + " << totalValueChange;

            sf::Text formulaText;
            formulaText.setFont(font);
            formulaText.setString(formula.str());
            formulaText.setCharacterSize(14);  // Increased font size
            formulaText.setFillColor(sf::Color::Yellow);
            formulaText.setPosition(startX + padding * 2, y + padding);
            window->draw(formulaText);
        }

        // Draw the ingredients panel
        void drawIngredientsPanel() {
            float panelWidth = 160.f;  // Wider panel
            float panelHeight = 530.f;  // Taller panel
            float startX = windowWidth - panelWidth - 10.0f;
            float startY = 10.0f;

            // Draw panel background
            sf::RectangleShape panel(sf::Vector2f(panelWidth, panelHeight));
            panel.setFillColor(sf::Color(20, 20, 30, 200));
            panel.setOutlineColor(sf::Color(100, 100, 150));
            panel.setOutlineThickness(1.0f);
            panel.setPosition(startX, startY);
            window->draw(panel);

            // Title
            sf::Text titleText;
            titleText.setFont(font);
            titleText.setString("Ingredients");
            titleText.setCharacterSize(20);  // Increased font size
            titleText.setFillColor(sf::Color::White);
            titleText.setStyle(sf::Text::Bold);
            titleText.setPosition(startX + 15.0f, startY + 15.0f);
            window->draw(titleText);

            // Draw ingredient buttons
            for (auto& button : ingredientButtons) {
                // Draw the button
                window->draw(button.shape);
                window->draw(button.text);
            }

            // Draw ingredient history BELOW the ingredient buttons, not overlapping
            if (!ingredientHistory.empty()) {
                // Calculate position below the last ingredient button
                float historyStartY = ingredientButtons.back().shape.getPosition().y +
                    ingredientButtons.back().shape.getSize().y + 30.0f;

                //// Draw separator line
                //sf::RectangleShape separator(sf::Vector2f(panelWidth - 30.0f, 1.0f));
                //separator.setFillColor(sf::Color(100, 100, 150));
                //separator.setPosition(startX + 15.0f, historyStartY - 15.0f);
                //window->draw(separator);

                // History title
                sf::Text historyTitle;
                historyTitle.setFont(font);
                historyTitle.setString("Ingredient History:");
                historyTitle.setCharacterSize(16);  // Increased font size
                historyTitle.setFillColor(sf::Color::White);
                historyTitle.setStyle(sf::Text::Bold);
                historyTitle.setPosition(startX + 15.0f, historyStartY);
                window->draw(historyTitle);

                // Draw history items (most recent first, limit to 5 items)
                float y = historyStartY + 30.0f;
                int maxToShow = 20;

                for (int i = ingredientHistory.size() - 1;
                    i >= 0 && i >= (int)ingredientHistory.size() - maxToShow;
                    i--) {
                    sf::Text historyText;
                    historyText.setFont(font);
                    historyText.setString(std::to_string(i + 1) + ". " + ingredientHistory[i]);
                    historyText.setCharacterSize(14);
                    historyText.setFillColor(sf::Color(180, 180, 180));
                    historyText.setPosition(startX + 25.0f, y);
                    window->draw(historyText);

                    y += 20.0f;
                }
            }
        }

        // Draw action buttons
        void drawActionButtons() {
            for (auto& button : actionButtons) {
                // Only show confirm/cancel buttons in preview mode
                if ((button.id == "confirm" || button.id == "cancel") && mode != Mode::PreviewMix) {
                    continue;
                }

                // Draw the button
                window->draw(button.shape);
                window->draw(button.text);
            }
        }

        // Draw preview information when mixing
        void drawPreview() {
            if (!previewNewProperty) return;

            const float startX = windowWidth / 2.0f + 350.0f;
            const float startY = 10.0f;
            const float panelWidth = 250;  // Larger panel
            const float panelHeight = 350.0f;  // Larger panel

            // Draw preview panel
            sf::RectangleShape panel(sf::Vector2f(panelWidth, panelHeight));
            panel.setFillColor(sf::Color(20, 20, 30, 230));
            panel.setOutlineColor(sf::Color(100, 160, 100));
            panel.setOutlineThickness(2.0f);
            panel.setPosition(startX, startY);
            //window->draw(panel);

            // Title
            sf::Text titleText;
            titleText.setFont(font);
            titleText.setString("Preview Mix: " + ingredientButtons[selectedIngredientIndex].id);
            titleText.setCharacterSize(18);  // Increased font size
            titleText.setFillColor(sf::Color::White);
            titleText.setStyle(sf::Text::Bold);
            titleText.setPosition(startX + 15.0f, startY + 15.0f);
            window->draw(titleText);

            // Extract current properties for mixing calculation
            std::vector<Property*> currentProps;
            for (const auto& prop : currentProperties) {
                currentProps.push_back(prop.property);
            }

            // Calculate result properties
            std::vector<Property*> resultProps = PropertyMixCalculator::mixProperties(
                currentProps, previewNewProperty, DrugType::Marijuana);

            // Draw preview result
            sf::Text resultTitle;
            resultTitle.setFont(font);
            resultTitle.setString("Result Properties:");
            resultTitle.setCharacterSize(16);  // Increased font size
            resultTitle.setFillColor(sf::Color::White);
            resultTitle.setStyle(sf::Text::Bold);
            resultTitle.setPosition(startX + 15.0f, startY + 50.0f);
            window->draw(resultTitle);

            // Draw result properties
            float y = startY + 80.0f;
            for (size_t i = 0; i < resultProps.size(); i++) {
                Property* prop = resultProps[i];

                // Check if this is a new property (not in current properties)
                bool isNew = true;
                for (const auto& currentProp : currentProps) {
                    if (currentProp->id == prop->id) {
                        isNew = false;
                        break;
                    }
                }

                // Draw with appropriate highlighting
                sf::Text propText;
                propText.setFont(font);
                propText.setString(std::to_string(i + 1) + ". " + prop->name + " (Tier " + std::to_string(prop->tier) + ")");
                propText.setCharacterSize(16);  // Increased font size

                if (isNew) {
                    // New property - highlight green
                    propText.setFillColor(sf::Color(100, 255, 100));
                    propText.setStyle(sf::Text::Bold);
                }
                else {
                    // Existing property
                    propText.setFillColor(tierColors[prop->tier]);
                }

                propText.setPosition(startX + 25.0f, y);
                window->draw(propText);

                y += 30.0f;  // Increased line height
            }

            // Calculate and show stats changes
            float currentAddictiveness = 0.0f;
            float currentBaseValue = 0.0f;
            float currentMultiplier = 1.0f;
            int currentValueChange = 0;

            float newAddictiveness = 0.0f;
            float newBaseValue = 0.0f;
            float newMultiplier = 1.0f;
            int newValueChange = 0;

            // Current stats
            for (auto* prop : currentProps) {
                currentAddictiveness += prop->addictiveness;
                currentBaseValue += prop->addBaseValueMultiple;
                currentMultiplier *= prop->valueMultiplier;
                currentValueChange += prop->valueChange;
            }

            // New stats
            for (auto* prop : resultProps) {
                newAddictiveness += prop->addictiveness;
                newBaseValue += prop->addBaseValueMultiple;
                newMultiplier *= prop->valueMultiplier;
                newValueChange += prop->valueChange;
            }

            y += 10.0f;

            // Draw stats comparison
            sf::Text statsTitle;
            statsTitle.setFont(font);
            statsTitle.setString("Stats Changes:");
            statsTitle.setCharacterSize(16);  // Increased font size
            statsTitle.setFillColor(sf::Color::White);
            statsTitle.setStyle(sf::Text::Bold);
            statsTitle.setPosition(startX + 15.0f, y);
            window->draw(statsTitle);

            y += 30.0f;

            // Addictiveness change
            drawStatChange("Addictiveness", currentAddictiveness, newAddictiveness, startX + 25.0f, y);
            y += 30.0f;  // Increased line height

            // Base value change
            drawStatChange("Base Value Bonus", currentBaseValue, newBaseValue, startX + 25.0f, y);
            y += 30.0f;  // Increased line height

            // Multiplier change
            drawStatChange("Value Multiplier", currentMultiplier, newMultiplier, startX + 25.0f, y);
            y += 30.0f;  // Increased line height

            // Value change
            drawStatChange("Value Change", (float)currentValueChange, (float)newValueChange, startX + 25.0f, y);

            // Instructions
            sf::Text instructionsText;
            instructionsText.setFont(font);
            instructionsText.setString("'Confirm Mix' to apply \n'Cancel' to go back\nYou can also click on another ingredient.");
            instructionsText.setCharacterSize(12);
            instructionsText.setFillColor(sf::Color(180, 180, 180));
            instructionsText.setPosition(startX + 15.0f, y + 30.f);
            window->draw(instructionsText);

            
        }

        // Draw a stat change with arrow indicator
        void drawStatChange(const std::string& label, float currentValue, float newValue, float x, float y) {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(2);
            ss << label << ": " << currentValue << " -> " << newValue;

            sf::Text statText;
            statText.setFont(font);
            statText.setString(ss.str());
            statText.setCharacterSize(16);  // Increased font size

            // Color based on change
            if (newValue > currentValue) {
                statText.setFillColor(sf::Color(100, 255, 100)); // Green for increase
            }
            else if (newValue < currentValue) {
                statText.setFillColor(sf::Color(255, 100, 100)); // Red for decrease
            }
            else {
                statText.setFillColor(sf::Color(180, 180, 180)); // Gray for no change
            }

            statText.setPosition(x, y);
            window->draw(statText);
        }

        // Draw help screen
        void drawHelpScreen() {
            // Semi-transparent background
            sf::RectangleShape bg(sf::Vector2f(windowWidth, windowHeight));
            bg.setFillColor(sf::Color(0, 0, 0, 220));
            window->draw(bg);

            // Help panel
            const float panelWidth = 700.0f;  // Larger panel
            const float panelHeight = 600.0f;  // Larger panel
            const float startX = (windowWidth - panelWidth) / 2.0f;
            const float startY = (windowHeight - panelHeight) / 2.0f;

            sf::RectangleShape panel(sf::Vector2f(panelWidth, panelHeight));
            panel.setFillColor(sf::Color(30, 30, 40, 250));
            panel.setOutlineColor(sf::Color(100, 100, 150));
            panel.setOutlineThickness(2.0f);
            panel.setPosition(startX, startY);
            window->draw(panel);

            // Title
            sf::Text titleText;
            titleText.setFont(font);
            titleText.setString("Property Mixer - Help");
            titleText.setCharacterSize(28);  // Increased font size
            titleText.setFillColor(sf::Color::White);
            titleText.setStyle(sf::Text::Bold);
            titleText.setPosition(startX + 25.0f, startY + 25.0f);
            window->draw(titleText);

            // Help content
            std::vector<std::string> helpLines = {
                "Controls:",
                "* Click on an ingredient in the right panel to preview mixing it",
                "* Click 'Confirm Mix' to apply the mix or 'Cancel' to go back",
                "* While in preview mode, you can click on another ingredient to switch",
                "* Click 'Reset' to clear all properties and start over",
                "* Press 'T' to toggle transition animations",
                "* Press 'L' to toggle connection lines between properties",
                "* Press 'I' to toggle tooltips when hovering over properties",
                "* Press '+' or '-' to adjust animation speed",
                "* Press 'ESC' to exit preview or help mode",
                "",
                "How it works:",
                "* The large circular map shows all possible properties",
                "* Each property has a tier (1-5) indicated by color and number",
                "* Properties have stats that affect the final product value:",
                "  - Addictiveness: Player addiction rate",
                "  - Base Value Bonus: Increases the base product value",
                "  - Value Multiplier: Multiplies the total value",
                "* When mixing, properties may transform based on their position",
                "  and the mix direction of the added ingredient",
                "* Hover over any property to see detailed stats",
                "",
                "User Interface:",
                "* Left Panel: Shows current properties and cumulative stats",
                "* Center Map: Visual representation of all properties and their relationships",
                "* Right Panel: Ingredients available for mixing",
                "* Preview Panel: Shows what will happen when you mix an ingredient",
                "",
                "Click anywhere to return to the mixer"
            };

            float y = startY + 80.0f;
            for (const auto& line : helpLines) {
                sf::Text helpText;
                helpText.setFont(font);
                helpText.setString(line);
                helpText.setCharacterSize(16);  // Increased font size

                if (line.empty() || line.find(":") != std::string::npos) {
                    helpText.setFillColor(sf::Color(200, 200, 100));
                    helpText.setStyle(sf::Text::Bold);
                }
                else {
                    helpText.setFillColor(sf::Color::White);
                }

                helpText.setPosition(startX + 35.0f, y);
                window->draw(helpText);

                y += 22.0f;  // Slightly increased line height
            }
        }

        // Draw a stats bar with a label
        void drawStatsBar(const std::string& label, float value, float maxValue,
            float x, float y, float width, float height) {
            // Calculate filled width based on the value
            float filledWidth = (value / maxValue) * width;
            if (filledWidth > width) filledWidth = width;

            // Draw bar background
            sf::RectangleShape barBg(sf::Vector2f(width, height));
            barBg.setFillColor(sf::Color(60, 60, 80));  // Slightly lighter background
            barBg.setPosition(x, y);
            window->draw(barBg);

            // Draw filled portion
            sf::RectangleShape barFill(sf::Vector2f(filledWidth, height));
            barFill.setFillColor(sf::Color(0, 191, 255));  // Bright blue
            barFill.setPosition(x, y);
            window->draw(barFill);

            // Draw label
            sf::Text labelText;
            labelText.setFont(font);

            std::stringstream ss;
            ss << std::fixed << std::setprecision(2);
            ss << label << ": " << value;

            labelText.setString(ss.str());
            labelText.setCharacterSize(14);  // Increased font size
            labelText.setFillColor(sf::Color::White);
            labelText.setPosition(x, y - labelText.getLocalBounds().height - 5);
            window->draw(labelText);

            // Draw the value as text inside the bar for better visibility
            sf::Text valueText;
            valueText.setFont(font);
            valueText.setString(ss.str().substr(label.length() + 2));  // Just the value
            valueText.setCharacterSize(14);
            valueText.setFillColor(sf::Color::White);
            valueText.setPosition(x + 5, y + (height - 14) / 2);
            window->draw(valueText);
        }

        // Easing function for smoother animations
        float easeInOutCubic(float t) {
            return t < 0.5 ? 4 * t * t * t : 1 - pow(-2 * t + 2, 3) / 2;
        }
    };

    // Main function for the application
    int main() {
        // Initialize the property system
        initializeGameSystem();

        // Create and run the visual property mixer
        VisualPropertyMixer mixer;
        mixer.initialize();
        mixer.run();

        return 0;
    }