#include "gamelogic.h"
#include "sceneGraph.hpp"
#include <GLFW/glfw3.h>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <chrono>
#include <fmt/format.h>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#include <iostream>
#include <utilities/glutils.h>
#include <utilities/mesh.h>
#include <utilities/shader.hpp>
#include <utilities/shapes.h>
#include <utilities/timeutils.h>
#define GLM_ENABLE_EXPERIMENTAL
#include "textures.hpp"
#include "utilities/glfont.h"
#include "utilities/imageLoader.hpp"
#include <glm/gtx/transform.hpp>

enum KeyFrameAction { BOTTOM, TOP };

#include <timestamps.h>

double padPositionX = 0;
double padPositionZ = 0;

unsigned int currentKeyFrame = 0;
unsigned int previousKeyFrame = 0;

SceneNode *rootNode;
SceneNode *boxNode;
SceneNode *ballNode;
SceneNode *padNode;
// Node for the dynamic (moving) light
SceneNode *dynLightNode;
// Nodes for the static lights
SceneNode *staticLightNode;
SceneNode *staticLightNode2;
// Node for text
SceneNode *textNode;

double ballRadius = 3.0f;

// These are heap allocated, because they should not be initialised at the start
// of the program
sf::SoundBuffer *buffer;
Gloom::Shader *shader;
sf::Sound *sound;

const glm::vec3 boxDimensions(180, 90, 90);
const glm::vec3 padDimensions(30, 3, 40);

glm::vec3 ballPosition(0, ballRadius + padDimensions.y, boxDimensions.z / 2);
glm::vec3 ballDirection(1, 1, 0.2f);

CommandLineOptions options;

bool hasStarted = false;
bool hasLost = false;
bool jumpedToNextFrame = false;
bool isPaused = false;

bool mouseLeftPressed = false;
bool mouseLeftReleased = false;
bool mouseRightPressed = false;
bool mouseRightReleased = false;

// Modify if you want the music to start further on in the track. Measured in
// seconds.
const float debug_startTime = 0;
double totalElapsedTime = debug_startTime;
double gameElapsedTime = debug_startTime;

double mouseSensitivity = 1.0;
double lastMouseX = windowWidth / 2.0;
double lastMouseY = windowHeight / 2.0;
void mouseCallback(GLFWwindow *window, double x, double y) {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);

    double deltaX = x - lastMouseX;
    double deltaY = y - lastMouseY;

    padPositionX -= mouseSensitivity * deltaX / windowWidth;
    padPositionZ -= mouseSensitivity * deltaY / windowHeight;

    if (padPositionX > 1)
        padPositionX = 1;
    if (padPositionX < 0)
        padPositionX = 0;
    if (padPositionZ > 1)
        padPositionZ = 1;
    if (padPositionZ < 0)
        padPositionZ = 0;

    glfwSetCursorPos(window, windowWidth / 2.0, windowHeight / 2.0);
}

//// A few lines to help you if you've never used c++ structs
// struct LightSource {
//     bool a_placeholder_value;
// };
// LightSource lightSources[/*Put number of light sources you want here*/];

void initGame(GLFWwindow *window, CommandLineOptions gameOptions) {
    buffer = new sf::SoundBuffer();
    if (!buffer->loadFromFile("../res/Hall of the Mountain King.ogg")) {
        return;
    }

    options = gameOptions;

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetCursorPosCallback(window, mouseCallback);

    shader = new Gloom::Shader();
    shader->makeBasicShader("../res/shaders/simple.vert", "../res/shaders/simple.frag");
    shader->activate();

    // Create meshes
    Mesh pad = cube(padDimensions, glm::vec2(30, 40), true);
    Mesh box = cube(boxDimensions, glm::vec2(90), true, true);
    Mesh sphere = generateSphere(1.0, 40, 40);

    // Fill buffers
    unsigned int ballVAO = generateBuffer(sphere);
    unsigned int boxVAO = generateBuffer(box);
    unsigned int padVAO = generateBuffer(pad);

    // Load charmap image
    PNGImage charmap = loadPNGFile("../res/textures/charmap.png");
    // Generate charmap texture
    GLuint charmapTex = generateTexture(charmap);
    // Generate charmap mesh
    Mesh charmapMesh = generateTextGeometryBuffer("Test", 39.0 / 29.0, 3712.0);
    // Generate charmap VAO
    GLuint charmapVao = generateBuffer(charmapMesh);

    // Construct scene
    rootNode = createSceneNode();
    boxNode = createSceneNode();
    padNode = createSceneNode();
    ballNode = createSceneNode();
    // Create light nodes
    dynLightNode = createSceneNode();
    staticLightNode = createSceneNode();
    staticLightNode2 = createSceneNode();
    // Create text nodes
    textNode = createSceneNode();

    // Set the correct node type for the lights
    dynLightNode->nodeType = SceneNodeType::POINT_LIGHT;
    staticLightNode->nodeType = SceneNodeType::POINT_LIGHT;
    staticLightNode2->nodeType = SceneNodeType::POINT_LIGHT;

    // Set the correct node type for the text nodes
    textNode->nodeType = SceneNodeType::GEOMETRY_2D;

    rootNode->children.push_back(boxNode);
    rootNode->children.push_back(padNode);
    rootNode->children.push_back(ballNode);
    // Add text node to the root node
    rootNode->children.push_back(textNode);
    // Add lights to the scene graph. The dynamic light is attached to the pad
    padNode->children.push_back(dynLightNode);
    rootNode->children.push_back(staticLightNode);
    rootNode->children.push_back(staticLightNode2);

    // Set the relative positions of the lights
    dynLightNode->position = glm::vec3(0.0, 10.0, 0.0);
    staticLightNode->position = glm::vec3(5.0, -50.0, -100.0);
    staticLightNode2->position = glm::vec3(-5.0, -50.0, -100.0);

    boxNode->vertexArrayObjectID = boxVAO;
    boxNode->VAOIndexCount = box.indices.size();

    padNode->vertexArrayObjectID = padVAO;
    padNode->VAOIndexCount = pad.indices.size();

    ballNode->vertexArrayObjectID = ballVAO;
    ballNode->VAOIndexCount = sphere.indices.size();

    getTimeDeltaSeconds();

    std::cout << fmt::format("Initialized scene with {} SceneNodes.", totalChildren(rootNode)) << std::endl;

    std::cout << "Ready. Click to start!" << std::endl;
}

void updateFrame(GLFWwindow *window) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    double timeDelta = getTimeDeltaSeconds();

    const float ballBottomY = boxNode->position.y - (boxDimensions.y / 2) + ballRadius + padDimensions.y;
    const float ballTopY = boxNode->position.y + (boxDimensions.y / 2) - ballRadius;
    const float BallVerticalTravelDistance = ballTopY - ballBottomY;

    const float cameraWallOffset = 30; // Arbitrary addition to prevent ball
                                       // from going too much into camera

    const float ballMinX = boxNode->position.x - (boxDimensions.x / 2) + ballRadius;
    const float ballMaxX = boxNode->position.x + (boxDimensions.x / 2) - ballRadius;
    const float ballMinZ = boxNode->position.z - (boxDimensions.z / 2) + ballRadius;
    const float ballMaxZ = boxNode->position.z + (boxDimensions.z / 2) - ballRadius - cameraWallOffset;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1)) {
        mouseLeftPressed = true;
        mouseLeftReleased = false;
    } else {
        mouseLeftReleased = mouseLeftPressed;
        mouseLeftPressed = false;
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2)) {
        mouseRightPressed = true;
        mouseRightReleased = false;
    } else {
        mouseRightReleased = mouseRightPressed;
        mouseRightPressed = false;
    }

    if (!hasStarted) {
        if (mouseLeftPressed) {
            if (options.enableMusic) {
                sound = new sf::Sound();
                sound->setBuffer(*buffer);
                sf::Time startTime = sf::seconds(debug_startTime);
                sound->setPlayingOffset(startTime);
                sound->play();
            }
            totalElapsedTime = debug_startTime;
            gameElapsedTime = debug_startTime;
            hasStarted = true;
        }

        ballPosition.x = ballMinX + (1 - padPositionX) * (ballMaxX - ballMinX);
        ballPosition.y = ballBottomY;
        ballPosition.z = ballMinZ + (1 - padPositionZ) * ((ballMaxZ + cameraWallOffset) - ballMinZ);
    } else {
        totalElapsedTime += timeDelta;
        if (hasLost) {
            if (mouseLeftReleased) {
                hasLost = false;
                hasStarted = false;
                currentKeyFrame = 0;
                previousKeyFrame = 0;
            }
        } else if (isPaused) {
            if (mouseRightReleased) {
                isPaused = false;
                if (options.enableMusic) {
                    sound->play();
                }
            }
        } else {
            gameElapsedTime += timeDelta;
            if (mouseRightReleased) {
                isPaused = true;
                if (options.enableMusic) {
                    sound->pause();
                }
            }
            // Get the timing for the beat of the song
            for (unsigned int i = currentKeyFrame; i < keyFrameTimeStamps.size(); i++) {
                if (gameElapsedTime < keyFrameTimeStamps.at(i)) {
                    continue;
                }
                currentKeyFrame = i;
            }

            jumpedToNextFrame = currentKeyFrame != previousKeyFrame;
            previousKeyFrame = currentKeyFrame;

            double frameStart = keyFrameTimeStamps.at(currentKeyFrame);
            double frameEnd = keyFrameTimeStamps.at(currentKeyFrame + 1); // Assumes last keyframe at infinity

            double elapsedTimeInFrame = gameElapsedTime - frameStart;
            double frameDuration = frameEnd - frameStart;
            double fractionFrameComplete = elapsedTimeInFrame / frameDuration;

            double ballYCoord = 0.0;

            KeyFrameAction currentOrigin = keyFrameDirections.at(currentKeyFrame);
            KeyFrameAction currentDestination = keyFrameDirections.at(currentKeyFrame + 1);

            // Synchronize ball with music
            if (currentOrigin == BOTTOM && currentDestination == BOTTOM) {
                ballYCoord = ballBottomY;
            } else if (currentOrigin == TOP && currentDestination == TOP) {
                ballYCoord = ballBottomY + BallVerticalTravelDistance;
            } else if (currentDestination == BOTTOM) {
                ballYCoord = ballBottomY + BallVerticalTravelDistance * (1 - fractionFrameComplete);
            } else if (currentDestination == TOP) {
                ballYCoord = ballBottomY + BallVerticalTravelDistance * fractionFrameComplete;
            }

            // Make ball move
            const float ballSpeed = 60.0f;
            ballPosition.x += timeDelta * ballSpeed * ballDirection.x;
            ballPosition.y = ballYCoord;
            ballPosition.z += timeDelta * ballSpeed * ballDirection.z;

            // Make ball bounce
            if (ballPosition.x < ballMinX) {
                ballPosition.x = ballMinX;
                ballDirection.x *= -1;
            } else if (ballPosition.x > ballMaxX) {
                ballPosition.x = ballMaxX;
                ballDirection.x *= -1;
            }
            if (ballPosition.z < ballMinZ) {
                ballPosition.z = ballMinZ;
                ballDirection.z *= -1;
            } else if (ballPosition.z > ballMaxZ) {
                ballPosition.z = ballMaxZ;
                ballDirection.z *= -1;
            }

            if (options.enableAutoplay) {
                padPositionX = 1 - (ballPosition.x - ballMinX) / (ballMaxX - ballMinX);
                padPositionZ = 1 - (ballPosition.z - ballMinZ) / ((ballMaxZ + cameraWallOffset) - ballMinZ);
            }

            // Check if the ball is hitting the pad when the ball is at the
            // bottom. If not, you just lost the game! (hehe)
            if (jumpedToNextFrame && currentOrigin == BOTTOM && currentDestination == TOP) {
                double padLeftX = boxNode->position.x - (boxDimensions.x / 2) +
                                  (1 - padPositionX) * (boxDimensions.x - padDimensions.x);
                double padRightX = padLeftX + padDimensions.x;
                double padFrontZ = boxNode->position.z - (boxDimensions.z / 2) +
                                   (1 - padPositionZ) * (boxDimensions.z - padDimensions.z);
                double padBackZ = padFrontZ + padDimensions.z;

                if (ballPosition.x < padLeftX || ballPosition.x > padRightX || ballPosition.z < padFrontZ ||
                    ballPosition.z > padBackZ) {
                    hasLost = true;
                    if (options.enableMusic) {
                        sound->stop();
                        delete sound;
                    }
                }
            }
        }
    }

    glm::mat4 projection = glm::perspective(glm::radians(80.0f), float(windowWidth) / float(windowHeight), 0.1f, 350.f);

    glm::vec3 cameraPosition = glm::vec3(0, 2, -20);
    // Send the camera position as a uniform
    glUniform3fv(6, 1, glm::value_ptr(cameraPosition));

    // Some math to make the camera move in a nice way
    float lookRotation = -0.6 / (1 + exp(-5 * (padPositionX - 0.5))) + 0.3;
    glm::mat4 cameraTransform = glm::rotate(0.3f + 0.2f * float(-padPositionZ * padPositionZ), glm::vec3(1, 0, 0)) *
                                glm::rotate(lookRotation, glm::vec3(0, 1, 0)) * glm::translate(-cameraPosition);

    glm::mat4 VP = projection * cameraTransform;

    // Move and rotate various SceneNodes
    boxNode->position = {0, -10, -80};

    ballNode->position = ballPosition;
    ballNode->scale = glm::vec3(ballRadius);
    ballNode->rotation = {0, totalElapsedTime * 2, 0};

    padNode->position = {boxNode->position.x - (boxDimensions.x / 2) + (padDimensions.x / 2) +
                             (1 - padPositionX) * (boxDimensions.x - padDimensions.x),
                         boxNode->position.y - (boxDimensions.y / 2) + (padDimensions.y / 2),
                         boxNode->position.z - (boxDimensions.z / 2) + (padDimensions.z / 2) +
                             (1 - padPositionZ) * (boxDimensions.z - padDimensions.z)};

    updateNodeTransformations(rootNode, glm::identity<glm::mat4>(), VP);

    // Send the updated ball position as a uniform
    glUniform3fv(7, 1, glm::value_ptr(ballNode->position));

    // Populate the lights uniform array with the dynamic and static lights
    auto dynLightPos = glm::vec3(dynLightNode->currentModelMatrix * glm::vec4(0, 0, 0, 1));
    glUniform3fv(shader->getUniformFromName("lights[0].position"), 1, glm::value_ptr(dynLightPos));
    glUniform3fv(shader->getUniformFromName("lights[0].color"), 1, glm::value_ptr(glm::vec3(1.0, 0.0, 0.0)));

    auto staticLightPos = glm::vec3(staticLightNode->currentModelMatrix * glm::vec4(0, 0, 0, 1));
    glUniform3fv(shader->getUniformFromName("lights[1].position"), 1, glm::value_ptr(staticLightPos));
    glUniform3fv(shader->getUniformFromName("lights[1].color"), 1, glm::value_ptr(glm::vec3(0.0, 1.0, 0.0)));

    auto staticLightPos2 = glm::vec3(staticLightNode2->currentModelMatrix * glm::vec4(0, 0, 0, 1));
    glUniform3fv(shader->getUniformFromName("lights[2].position"), 1, glm::value_ptr(staticLightPos2));
    glUniform3fv(shader->getUniformFromName("lights[2].color"), 1, glm::value_ptr(glm::vec3(0.0, 0.0, 1.0)));
}

void updateNodeTransformations(SceneNode *node, glm::mat4 modelThusFar, glm::mat4 mvpThusFar) {
    glm::mat4 transformationMatrix = glm::translate(node->position) * glm::translate(node->referencePoint) *
                                     glm::rotate(node->rotation.y, glm::vec3(0, 1, 0)) *
                                     glm::rotate(node->rotation.x, glm::vec3(1, 0, 0)) *
                                     glm::rotate(node->rotation.z, glm::vec3(0, 0, 1)) * glm::scale(node->scale) *
                                     glm::translate(-node->referencePoint);

    node->currentModelMatrix = modelThusFar * transformationMatrix;
    node->currentMVPMatrix = mvpThusFar * transformationMatrix;

    switch (node->nodeType) {
    case SceneNodeType::GEOMETRY:
        break;
    case SceneNodeType::POINT_LIGHT:
        break;
    case SceneNodeType::SPOT_LIGHT:
        break;
    }

    for (SceneNode *child : node->children) {
        updateNodeTransformations(child, node->currentModelMatrix, node->currentMVPMatrix);
    }
}

void renderNode(SceneNode *node) {
    glUniformMatrix4fv(3, 1, GL_FALSE, glm::value_ptr(node->currentMVPMatrix));
    glUniformMatrix4fv(4, 1, GL_FALSE, glm::value_ptr(node->currentModelMatrix));
    glm::mat3 normalTransform = glm::mat3(glm::transpose(glm::inverse(node->currentModelMatrix)));
    glUniformMatrix3fv(5, 1, GL_FALSE, glm::value_ptr(normalTransform));

    switch (node->nodeType) {
    case SceneNodeType::GEOMETRY:
        if (node->vertexArrayObjectID != -1) {
            glBindVertexArray(node->vertexArrayObjectID);
            glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
        }
        break;
    case SceneNodeType::POINT_LIGHT:
        break;
    case SceneNodeType::SPOT_LIGHT:
        break;
    }

    for (SceneNode *child : node->children) {
        renderNode(child);
    }
}

void renderFrame(GLFWwindow *window) {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);

    renderNode(rootNode);
}
