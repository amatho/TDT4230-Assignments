#include <glad/glad.h>
#include <program.hpp>
#include "glutils.h"
#include <vector>

template <class T>
unsigned int generateAttribute(int id, int elementsPerEntry, std::vector<T> data, bool normalize) {
    unsigned int bufferID;
    glGenBuffers(1, &bufferID);
    glBindBuffer(GL_ARRAY_BUFFER, bufferID);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(T), data.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(id, elementsPerEntry, GL_FLOAT, normalize ? GL_TRUE : GL_FALSE, sizeof(T), 0);
    glEnableVertexAttribArray(id);
    return bufferID;
}

unsigned int generateBuffer(Mesh &mesh) {
    unsigned int vaoID;
    glGenVertexArrays(1, &vaoID);
    glBindVertexArray(vaoID);

    generateAttribute(0, 3, mesh.vertices, false);
    if (mesh.normals.size() > 0) {
        generateAttribute(1, 3, mesh.normals, true);
    }
    if (mesh.textureCoordinates.size() > 0) {
        generateAttribute(2, 2, mesh.textureCoordinates, false);
    }

    if (mesh.normals.size() > 0 && mesh.textureCoordinates.size() > 0) {
        std::vector<glm::vec3> tangents;
        std::vector<glm::vec3> bitangents;

        for (size_t i = 0; i < mesh.vertices.size(); i += 3) {
            glm::vec3 v0 = mesh.vertices[i];
            glm::vec3 v1 = mesh.vertices[i + 1];
            glm::vec3 v2 = mesh.vertices[i + 2];

            glm::vec2 uv0 = mesh.textureCoordinates[i];
            glm::vec2 uv1 = mesh.textureCoordinates[i + 1];
            glm::vec2 uv2 = mesh.textureCoordinates[i + 2];

            glm::vec3 deltaPos1 = v1 - v0;
            glm::vec3 deltaPos2 = v2 - v0;

            glm::vec2 deltaUv1 = uv1 - uv0;
            glm::vec2 deltaUv2 = uv2 - uv0;

            float r = 1.0f / (deltaUv1.x * deltaUv2.y - deltaUv1.y * deltaUv2.x);
            glm::vec3 tangent = (deltaPos1 * deltaUv2.y - deltaPos2 * deltaUv1.y) * r;
            glm::vec3 bitangent = (deltaPos2 * deltaUv1.x - deltaPos1 * deltaUv2.x) * r;

            tangents.push_back(tangent);
            tangents.push_back(tangent);
            tangents.push_back(tangent);
            bitangents.push_back(bitangent);
            bitangents.push_back(bitangent);
            bitangents.push_back(bitangent);
        }

        generateAttribute(3, 3, tangents, false);
        generateAttribute(4, 3, bitangents, false);
    }

    unsigned int indexBufferID;
    glGenBuffers(1, &indexBufferID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int), mesh.indices.data(), GL_STATIC_DRAW);

    return vaoID;
}
