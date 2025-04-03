#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <algorithm>
#include <queue>
#include "property_mixer_core.h"

// Structure to track property transitions when mixing
struct PropertyTransition {
    Vector2 startPosition;
    Vector2 endPosition;
    Property* sourceProperty;
    Property* resultProperty;
    float animationTime;
    float totalAnimationTime;
};

class EnhancedVisualizer {
public:
    EnhancedVisualizer() : isInitialized(false), window(nullptr), showTransitions(true),
        animationSpeed(1.0f), showMixingLines(true), showTooltips(true) {}

    ~EnhancedVisualizer() {
        if (window) {
            window->close();
            delete window;
        }
    }

    // Initialize the visualizer window
    void initialize() {
        if (!isInitialized) {
            window = new sf::RenderWindow(sf::VideoMode(1024, 768), "Enhanced Property Mixer Visualizer");
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

            // Initialize clock for animations
            clock.restart();
            isInitialized = true;
        }
    }

    // Update the visualizer with the current mixer map and properties
    void updateVisualization(const MixerMap* mixerMap, const std::vector<Property*>& currentProperties) {
        if (!isInitialized || !window) {
            initialize();
        }

        // Update the time delta
        float deltaTime = clock.restart().asSeconds();

        // Update any active property transitions
        updateTransitions(deltaTime);

        // Handle window events
        handleEvents();

        // Clear the window
        window->clear(sf::Color(25, 25, 35));

        // Draw the mixer map
        drawMixerMap(mixerMap);

        // Draw transition animations
        if (showTransitions) {
            drawTransitions();
        }

        // Draw mixing lines showing connections between properties
        if (showMixingLines && mixerMap) {
            drawMixingLines(mixerMap, currentProperties);
        }

        // Draw the current properties list and stats
        drawCurrentProperties(currentProperties);

        // Draw help text and controls
        drawHelpText();

        // Display the window
        window->display();
    }

    // Trigger a new property transition animation
    void addPropertyTransition(Property* sourceProp, Property* resultProp, const MixerMap* mixerMap) {
        if (!mixerMap || !sourceProp || !resultProp) return;

        // Find the effect positions for the source and result properties
        Vector2 startPos, endPos;
        bool foundStart = false, foundEnd = false;

        for (auto* effect : mixerMap->effects) {
            if (effect->property->id == sourceProp->id) {
                startPos = effect->position;
                foundStart = true;
            }
            if (effect->property->id == resultProp->id) {
                endPos = effect->position;
                foundEnd = true;
            }
        }

        if (foundStart && foundEnd) {
            PropertyTransition transition;
            transition.startPosition = startPos;
            transition.endPosition = endPos;
            transition.sourceProperty = sourceProp;
            transition.resultProperty = resultProp;
            transition.animationTime = 0.0f;
            transition.totalAnimationTime = 1.0f; // 1 second animation

            activeTransitions.push_back(transition);
        }
    }

private:
    bool isInitialized;
    sf::RenderWindow* window;
    sf::Font font;
    sf::Clock clock;

    // Visualization options
    bool showTransitions;
    float animationSpeed;
    bool showMixingLines;
    bool showTooltips;

    // Active property transitions for animation
    std::vector<PropertyTransition> activeTransitions;

    // Store last known positions of properties
    std::map<std::string, Vector2> propertyPositions;

    // Property that mouse is hovering over (for tooltips)
    Property* hoveredProperty = nullptr;

    // Color mapping for different tiers
    std::map<int, sf::Color> tierColors = {
        {1, sf::Color(60, 179, 113)},   // MediumSeaGreen
        {2, sf::Color(30, 144, 255)},   // DodgerBlue
        {3, sf::Color(255, 165, 0)},    // Orange
        {4, sf::Color(255, 69, 0)},     // OrangeRed
        {5, sf::Color(178, 34, 34)}     // Firebrick
    };

    // Handle window events
    void handleEvents() {
        sf::Event event;
        while (window->pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window->close();
                delete window;
                window = nullptr;
                isInitialized = false;
                initialize(); // Reopen the window if closed
            }
            else if (event.type == sf::Event::KeyPressed) {
                // Toggle showing transitions with 'T' key
                if (event.key.code == sf::Keyboard::T) {
                    showTransitions = !showTransitions;
                }
                // Toggle showing mixing lines with 'L' key
                else if (event.key.code == sf::Keyboard::L) {
                    showMixingLines = !showMixingLines;
                }
                // Toggle tooltips with 'I' key
                else if (event.key.code == sf::Keyboard::I) {
                    showTooltips = !showTooltips;
                }
                // Increase animation speed with '+'
                else if (event.key.code == sf::Keyboard::Add) {
                    animationSpeed = std::min(animationSpeed + 0.2f, 3.0f);
                }
                // Decrease animation speed with '-'
                else if (event.key.code == sf::Keyboard::Subtract) {
                    animationSpeed = std::max(animationSpeed - 0.2f, 0.2f);
                }
            }
        }
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

    // Draw the mixer map with all effects
    void drawMixerMap(const MixerMap* mixerMap) {
        if (!mixerMap) return;

        const float centerX = window->getSize().x / 2.0f;
        const float centerY = window->getSize().y / 2.0f;
        const float scaleFactor = 50.0f; // Scale the map coordinates to screen coordinates

        // Draw the map boundary circle
        sf::CircleShape mapBoundary(mixerMap->mapRadius * scaleFactor);
        mapBoundary.setFillColor(sf::Color(40, 40, 60, 100));
        mapBoundary.setOutlineColor(sf::Color(100, 100, 150));
        mapBoundary.setOutlineThickness(2.0f);
        mapBoundary.setPosition(centerX - mapBoundary.getRadius(), centerY - mapBoundary.getRadius());
        window->draw(mapBoundary);

        // Draw grid lines
        for (int i = 1; i <= (int)mixerMap->mapRadius; i++) {
            sf::CircleShape gridCircle(i * scaleFactor);
            gridCircle.setFillColor(sf::Color::Transparent);
            gridCircle.setOutlineColor(sf::Color(70, 70, 100, 100));
            gridCircle.setOutlineThickness(1.0f);
            gridCircle.setPosition(centerX - gridCircle.getRadius(), centerY - gridCircle.getRadius());
            window->draw(gridCircle);
        }

        // Draw coordinate axes
        sf::Vertex xAxis[] = {
            sf::Vertex(sf::Vector2f(centerX - mapBoundary.getRadius(), centerY), sf::Color(100, 100, 150, 150)),
            sf::Vertex(sf::Vector2f(centerX + mapBoundary.getRadius(), centerY), sf::Color(100, 100, 150, 150))
        };
        sf::Vertex yAxis[] = {
            sf::Vertex(sf::Vector2f(centerX, centerY - mapBoundary.getRadius()), sf::Color(100, 100, 150, 150)),
            sf::Vertex(sf::Vector2f(centerX, centerY + mapBoundary.getRadius()), sf::Color(100, 100, 150, 150))
        };
        window->draw(xAxis, 2, sf::Lines);
        window->draw(yAxis, 2, sf::Lines);

        // Check for hovering over effects
        sf::Vector2i mousePos = sf::Mouse::getPosition(*window);
        hoveredProperty = nullptr;

        // Draw the effects (circles)
        for (auto* effect : mixerMap->effects) {
            // Calculate screen position
            float screenX = centerX + effect->position.x * scaleFactor;
            float screenY = centerY - effect->position.y * scaleFactor; // Y is inverted in SFML

            // Store property position for other visualizations
            propertyPositions[effect->property->id] = Vector2(screenX, screenY);

            // Check if mouse is hovering over this effect
            float distance = std::sqrt(
                std::pow(mousePos.x - screenX, 2) +
                std::pow(mousePos.y - screenY, 2)
            );

            if (distance <= effect->radius * scaleFactor) {
                hoveredProperty = effect->property;
            }

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

            effectCircle.setFillColor(sf::Color(effectColor.r, effectColor.g, effectColor.b, 100)); // Semi-transparent
            effectCircle.setOutlineColor(effectColor);
            effectCircle.setOutlineThickness(2.0f);
            effectCircle.setPosition(screenX - effectCircle.getRadius(), screenY - effectCircle.getRadius());
            window->draw(effectCircle);

            // Draw the property name
            sf::Text propertyText;
            propertyText.setFont(font);
            propertyText.setString(effect->property->name);
            propertyText.setCharacterSize(10);
            propertyText.setFillColor(sf::Color::White);
            propertyText.setPosition(screenX - propertyText.getLocalBounds().width / 2.0f,
                screenY - propertyText.getLocalBounds().height / 2.0f);
            window->draw(propertyText);

            // Draw the tier number with small circle
            sf::CircleShape tierCircle(8.0f);
            tierCircle.setFillColor(effectColor);
            tierCircle.setPosition(screenX - 8.0f, screenY - effectCircle.getRadius() - 20.0f);
            window->draw(tierCircle);

            sf::Text tierText;
            tierText.setFont(font);
            tierText.setString(std::to_string(effect->property->tier));
            tierText.setCharacterSize(10);
            tierText.setFillColor(sf::Color::White);
            tierText.setPosition(screenX - 4.0f, screenY - effectCircle.getRadius() - 20.0f);
            window->draw(tierText);
        }

        // Draw tooltip for hovered property
        if (hoveredProperty && showTooltips) {
            drawPropertyTooltip(hoveredProperty, sf::Vector2f(mousePos.x, mousePos.y));
        }
    }

    // Draw property transitions (animations)
    void drawTransitions() {
        const float centerX = window->getSize().x / 2.0f;
        const float centerY = window->getSize().y / 2.0f;
        const float scaleFactor = 50.0f;

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
                    sf::Color(200, 200, 200, 100)),
                sf::Vertex(sf::Vector2f(screenX, screenY),
                    sf::Color(200, 200, 200, 200))
            };
            window->draw(path, 2, sf::Lines);

            // Draw the moving property circle
            sf::CircleShape transitionCircle(10.0f);
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
            window->draw(transitionCircle);

            // Draw property name above the circle
            sf::Text propText;
            propText.setFont(font);
            propText.setString(transition.sourceProperty->name + " → " + transition.resultProperty->name);
            propText.setCharacterSize(10);
            propText.setFillColor(sf::Color::White);
            propText.setPosition(screenX - propText.getLocalBounds().width / 2.0f,
                screenY - transitionCircle.getRadius() - 20.0f);
            window->draw(propText);
        }
    }

    // Draw lines showing mixing connections between properties
    void drawMixingLines(const MixerMap* mixerMap, const std::vector<Property*>& currentProperties) {
        if (currentProperties.size() <= 1) return;

        // For each pair of properties, draw a connection line
        for (size_t i = 0; i < currentProperties.size(); i++) {
            for (size_t j = i + 1; j < currentProperties.size(); j++) {
                // Get positions from our stored map
                auto posIt1 = propertyPositions.find(currentProperties[i]->id);
                auto posIt2 = propertyPositions.find(currentProperties[j]->id);

                if (posIt1 != propertyPositions.end() && posIt2 != propertyPositions.end()) {
                    sf::Vertex line[] = {
                        sf::Vertex(sf::Vector2f(posIt1->second.x, posIt1->second.y),
                                 sf::Color(255, 255, 255, 80)),
                        sf::Vertex(sf::Vector2f(posIt2->second.x, posIt2->second.y),
                                 sf::Color(255, 255, 255, 80))
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
        sf::RectangleShape tooltip(sf::Vector2f(250, 120));
        tooltip.setFillColor(sf::Color(20, 20, 30, 230));
        tooltip.setOutlineColor(tierColors[property->tier]);
        tooltip.setOutlineThickness(2);

        // Position tooltip near the mouse, but ensure it stays within window bounds
        float tooltipX = mousePos.x + 10;
        float tooltipY = mousePos.y + 10;

        // Adjust if tooltip would go off screen
        if (tooltipX + tooltip.getSize().x > window->getSize().x) {
            tooltipX = mousePos.x - tooltip.getSize().x - 10;
        }
        if (tooltipY + tooltip.getSize().y > window->getSize().y) {
            tooltipY = mousePos.y - tooltip.getSize().y - 10;
        }

        tooltip.setPosition(tooltipX, tooltipY);
        window->draw(tooltip);

        // Add property details to tooltip
        sf::Text nameText;
        nameText.setFont(font);
        nameText.setString(property->name + " (Tier " + std::to_string(property->tier) + ")");
        nameText.setCharacterSize(14);
        nameText.setStyle(sf::Text::Bold);
        nameText.setFillColor(tierColors[property->tier]);
        nameText.setPosition(tooltipX + 10, tooltipY + 10);
        window->draw(nameText);

        // Property stats
        sf::Text statsText;
        statsText.setFont(font);
        std::string statsStr =
            "ID: " + property->id + "\n" +
            "Addictiveness: " + std::to_string(property->addictiveness) + "\n" +
            "Base Value Add: " + std::to_string(property->addBaseValueMultiple) + "\n" +
            "Value Multiplier: " + std::to_string(property->valueMultiplier) + "\n" +
            "Mix Direction: (" + std::to_string(property->mixDirection.x) + ", "
            + std::to_string(property->mixDirection.y) + ")";

        statsText.setString(statsStr);
        statsText.setCharacterSize(12);
        statsText.setFillColor(sf::Color::White);
        statsText.setPosition(tooltipX + 10, tooltipY + 35);
        window->draw(statsText);
    }

    // Draw the current properties information
    void drawCurrentProperties(const std::vector<Property*>& properties) {
        const float startX = 10.0f;
        const float startY = 10.0f;
        const float lineHeight = 20.0f;
        const float panelWidth = 240.0f;

        // Draw background panel
        sf::RectangleShape panel(sf::Vector2f(panelWidth, 400));
        panel.setFillColor(sf::Color(20, 20, 30, 200));
        panel.setOutlineColor(sf::Color(100, 100, 150));
        panel.setOutlineThickness(1.0f);
        panel.setPosition(startX, startY);
        window->draw(panel);

        // Title
        sf::Text titleText;
        titleText.setFont(font);
        titleText.setString("Current Properties:");
        titleText.setCharacterSize(16);
        titleText.setFillColor(sf::Color::White);
        titleText.setStyle(sf::Text::Bold);
        titleText.setPosition(startX + 10, startY + 10);
        window->draw(titleText);

        // Properties list
        float y = startY + 40;
        if (properties.empty()) {
            sf::Text noneText;
            noneText.setFont(font);
            noneText.setString("(none)");
            noneText.setCharacterSize(14);
            noneText.setFillColor(sf::Color(180, 180, 180));
            noneText.setPosition(startX + 15, y);
            window->draw(noneText);
        }
        else {
            for (size_t i = 0; i < properties.size(); i++) {
                Property* prop = properties[i];

                sf::Text propText;
                propText.setFont(font);
                propText.setString(std::to_string(i + 1) + ". " + prop->name);
                propText.setCharacterSize(14);
                propText.setFillColor(tierColors[prop->tier]);
                propText.setPosition(startX + 15, y);
                window->draw(propText);

                y += lineHeight;
            }
        }

        // Draw property stats if any properties exist
        if (!properties.empty()) {
            // Calculate cumulative stats
            float totalAddictiveness = 0.0f;
            float totalBaseValueMultiple = 0.0f;
            float totalValueMultiplier = 1.0f;
            int totalValueChange = 0;

            for (auto* prop : properties) {
                totalAddictiveness += prop->addictiveness;
                totalBaseValueMultiple += prop->addBaseValueMultiple;
                totalValueMultiplier *= prop->valueMultiplier;
                totalValueChange += prop->valueChange;
            }

            y += lineHeight;

            // Stats title
            sf::Text statsTitle;
            statsTitle.setFont(font);
            statsTitle.setString("Cumulative Stats:");
            statsTitle.setCharacterSize(14);
            statsTitle.setFillColor(sf::Color::White);
            statsTitle.setStyle(sf::Text::Bold);
            statsTitle.setPosition(startX + 10, y);
            window->draw(statsTitle);

            y += lineHeight * 1.5f;

            // Draw stats bars
            drawStatsBar("Addictiveness", totalAddictiveness, 1.0f, startX + 15, y, panelWidth - 30, 15.0f);
            y += lineHeight * 1.5f;

            drawStatsBar("Base Value Bonus", totalBaseValueMultiple, 4.0f, startX + 15, y, panelWidth - 30, 15.0f);
            y += lineHeight * 1.5f;

            // Value multiplier text
            sf::Text multText;
            multText.setFont(font);
            multText.setString("Value Multiplier: " + std::to_string(totalValueMultiplier));
            multText.setCharacterSize(14);
            multText.setFillColor(sf::Color::White);
            multText.setPosition(startX + 15, y);
            window->draw(multText);
            y += lineHeight;

            // Value change text
            sf::Text changeText;
            changeText.setFont(font);
            changeText.setString("Value Change: " + std::to_string(totalValueChange));
            changeText.setCharacterSize(14);
            changeText.setFillColor(sf::Color::White);
            changeText.setPosition(startX + 15, y);
            window->draw(changeText);
            y += lineHeight * 1.5f;

            // Final value formula
            sf::Text formulaText;
            formulaText.setFont(font);
            formulaText.setString("Final Value = Base * (1 + " +
                std::to_string(totalBaseValueMultiple) + ") * " +
                std::to_string(totalValueMultiplier) + " + " +
                std::to_string(totalValueChange));
            formulaText.setCharacterSize(12);
            formulaText.setFillColor(sf::Color::Yellow);
            formulaText.setPosition(startX + 15, y);
            window->draw(formulaText);
        }
    }

    // Draw help text for keyboard controls
    void drawHelpText() {
        const float startX = window->getSize().x - 240.0f;
        const float startY = 10.0f;
        const float lineHeight = 20.0f;
        const float panelWidth = 230.0f;

        // Draw background panel
        sf::RectangleShape panel(sf::Vector2f(panelWidth, 150));
        panel.setFillColor(sf::Color(20, 20, 30, 200));
        panel.setOutlineColor(sf::Color(100, 100, 150));
        panel.setOutlineThickness(1.0f);
        panel.setPosition(startX, startY);
        window->draw(panel);

        // Title
        sf::Text titleText;
        titleText.setFont(font);
        titleText.setString("Keyboard Controls:");
        titleText.setCharacterSize(14);
        titleText.setFillColor(sf::Color::White);
        titleText.setStyle(sf::Text::Bold);
        titleText.setPosition(startX + 10, startY + 10);
        window->draw(titleText);

        // Control instructions
        float y = startY + 35;

        sf::Text text1;
        text1.setFont(font);
        text1.setString("[T] Toggle transitions: " + std::string(showTransitions ? "ON" : "OFF"));
        text1.setCharacterSize(12);
        text1.setFillColor(sf::Color(180, 180, 180));
        text1.setPosition(startX + 15, y);
        window->draw(text1);
        y += lineHeight;

        sf::Text text2;
        text2.setFont(font);
        text2.setString("[L] Toggle mixing lines: " + std::string(showMixingLines ? "ON" : "OFF"));
        text2.setCharacterSize(12);
        text2.setFillColor(sf::Color(180, 180, 180));
        text2.setPosition(startX + 15, y);
        window->draw(text2);
        y += lineHeight;

        sf::Text text3;
        text3.setFont(font);
        text3.setString("[I] Toggle tooltips: " + std::string(showTooltips ? "ON" : "OFF"));
        text3.setCharacterSize(12);
        text3.setFillColor(sf::Color(180, 180, 180));
        text3.setPosition(startX + 15, y);
        window->draw(text3);
        y += lineHeight;

        sf::Text text4;
        text4.setFont(font);
        text4.setString("[+/-] Animation speed: " + std::to_string(animationSpeed) + "x");
        text4.setCharacterSize(12);
        text4.setFillColor(sf::Color(180, 180, 180));
        text4.setPosition(startX + 15, y);
        window->draw(text4);
    }

    // Draw a stats bar with a label
    void drawStatsBar(const std::string& label, float value, float maxValue,
        float x, float y, float width, float height) {
        // Calculate filled width based on the value
        float filledWidth = (value / maxValue) * width;
        if (filledWidth > width) filledWidth = width;

        // Draw bar background
        sf::RectangleShape barBg(sf::Vector2f(width, height));
        barBg.setFillColor(sf::Color(60, 60, 60));
        barBg.setPosition(x, y);
        window->draw(barBg);

        // Draw filled portion
        sf::RectangleShape barFill(sf::Vector2f(filledWidth, height));
        barFill.setFillColor(sf::Color(0, 191, 255));
        barFill.setPosition(x, y);
        window->draw(barFill);

        // Draw label
        sf::Text labelText;
        labelText.setFont(font);
        labelText.setString(label + ": " + std::to_string(value));
        labelText.setCharacterSize(12);
        labelText.setFillColor(sf::Color::White);
        labelText.setPosition(x, y - labelText.getLocalBounds().height - 2);
        window->draw(labelText);
    }

    // Easing function for smoother animations
    float easeInOutCubic(float t) {
        return t < 0.5 ? 4 * t * t * t : 1 - pow(-2 * t + 2, 3) / 2;
    }
};