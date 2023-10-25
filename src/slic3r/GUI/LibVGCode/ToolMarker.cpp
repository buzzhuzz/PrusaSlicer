//################################################################################################################################
// PrusaSlicer development only -> !!!TO BE REMOVED!!!
#include "libslic3r/Technologies.hpp"
//################################################################################################################################

///|/ Copyright (c) Prusa Research 2023 Enrico Turri @enricoturri1966
///|/
///|/ libvgcode is released under the terms of the AGPLv3 or higher
///|/
#include "ToolMarker.hpp"
#include "OpenGLUtils.hpp"
#include "Utils.hpp"

#include <algorithm>

//################################################################################################################################
// PrusaSlicer development only -> !!!TO BE REMOVED!!!
#if ENABLE_NEW_GCODE_VIEWER
//################################################################################################################################

namespace libvgcode {

ToolMarker::~ToolMarker()
{
    if (m_ibo_id != 0)
        glsafe(glDeleteBuffers(1, &m_ibo_id));
    if (m_vbo_id != 0)
        glsafe(glDeleteBuffers(1, &m_vbo_id));
    if (m_vao_id != 0)
        glsafe(glDeleteVertexArrays(1, &m_vao_id));
}

// Geometry:
// Arrow with cylindrical stem and conical tip, with the given dimensions and resolution
// The origin of the arrow is at the tip of the conical section
// The axis of symmetry is along the Z axis
// The arrow is pointing downward
void ToolMarker::init(uint16_t resolution, float tip_radius, float tip_height, float stem_radius, float stem_height)
{
    if (m_vao_id != 0)
        return;

    // ensure vertices count does not exceed 65536
    resolution = std::clamp<uint16_t>(resolution, 4, 10922);

    std::vector<float> vertices;
    const uint16_t vertices_count = 6 * resolution + 2;
    vertices.reserve(6 * vertices_count);

    m_indices_count = 6 * resolution * 3;
    std::vector<uint16_t> indices;
    indices.reserve(m_indices_count);

    const float angle_step = 2.0f * PI / float(resolution);
    std::vector<float> cosines(resolution);
    std::vector<float> sines(resolution);

    for (uint16_t i = 0; i < resolution; ++i) {
        const float angle = angle_step * float(i);
        cosines[i] = ::cos(angle);
        sines[i] = -::sin(angle);
    }

    const float total_height = tip_height + stem_height;

    // tip vertices
    add_vertex(toVec3f(0.0f), toVec3f(0.0f, 0.0f, -1.0f), vertices);
    for (uint16_t i = 0; i < resolution; ++i) {
        add_vertex(toVec3f(tip_radius * sines[i], tip_radius * cosines[i], tip_height), toVec3f(sines[i], cosines[i], 0.0f), vertices);
    }

    // tip triangles
    for (uint16_t i = 0; i < resolution; ++i) {
        const uint16_t v3 = (i < resolution - 1) ? i + 2 : 1;
        add_triangle(0, i + 1, v3, indices);
    }

    // tip cap outer perimeter vertices
    for (uint16_t i = 0; i < resolution; ++i) {
        add_vertex(toVec3f(tip_radius * sines[i], tip_radius * cosines[i], tip_height), toVec3f(0.0f, 0.0f, 1.0f), vertices);
    }

    // tip cap inner perimeter vertices
    for (uint16_t i = 0; i < resolution; ++i) {
        add_vertex(toVec3f(stem_radius * sines[i], stem_radius * cosines[i], tip_height), toVec3f(0.0f, 0.0f, 1.0f), vertices);
    }

    // tip cap triangles
    for (uint16_t i = 0; i < resolution; ++i) {
        const uint16_t v2 = (i < resolution - 1) ? i + resolution + 2 : resolution + 1;
        const uint16_t v3 = (i < resolution - 1) ? i + 2 * resolution + 2 : 2 * resolution + 1;
        add_triangle(i + resolution + 1, v3, v2, indices);
        add_triangle(i + resolution + 1, i + 2 * resolution + 1, v3, indices);
    }

    // stem bottom vertices
    for (uint16_t i = 0; i < resolution; ++i) {
        add_vertex(toVec3f(stem_radius * sines[i], stem_radius * cosines[i], tip_height), toVec3f(sines[i], cosines[i], 0.0f), vertices);
    }

    // stem top vertices
    for (uint16_t i = 0; i < resolution; ++i) {
        add_vertex(toVec3f(stem_radius * sines[i], stem_radius * cosines[i], total_height), toVec3f(sines[i], cosines[i], 0.0f), vertices);
    }

    // stem triangles
    for (uint16_t i = 0; i < resolution; ++i) {
        const uint16_t v2 = (i < resolution - 1) ? i + 3 * resolution + 2 : 3 * resolution + 1;
        const uint16_t v3 = (i < resolution - 1) ? i + 4 * resolution + 2 : 4 * resolution + 1;
        add_triangle(i + 3 * resolution + 1, v3, v2, indices);
        add_triangle(i + 3 * resolution + 1, i + 4 * resolution + 1, v3, indices);
    }

    // stem cap vertices
    add_vertex(toVec3f(0.0f, 0.0f, total_height), toVec3f(0.0f, 0.0f, 1.0f), vertices);
    for (uint16_t i = 0; i < resolution; ++i) {
        add_vertex(toVec3f(stem_radius * sines[i], stem_radius * cosines[i], total_height), toVec3f(0.0f, 0.0f, 1.0f), vertices);
    }

    // stem cap triangles
    for (uint16_t i = 0; i < resolution; ++i) {
        const uint16_t v3 = (i < resolution - 1) ? i + 5 * resolution + 3 : 5 * resolution + 2;
        add_triangle(5 * resolution + 1, v3, i + 5 * resolution + 2, indices);
    }

    const size_t vertex_stride = 6 * sizeof(float);
    const size_t position_offset = 0;
    const size_t normal_offset = 3 * sizeof(float);

    int curr_vertex_array;
    glsafe(glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &curr_vertex_array));
    int curr_array_buffer;
    glsafe(glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &curr_array_buffer));

    glsafe(glGenVertexArrays(1, &m_vao_id));
    glsafe(glBindVertexArray(m_vao_id));
    glsafe(glGenBuffers(1, &m_vbo_id));
    glsafe(glBindBuffer(GL_ARRAY_BUFFER, m_vbo_id));
    glsafe(glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW));
    glsafe(glEnableVertexAttribArray(0));
    glsafe(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_stride, (const void*)position_offset));
    glsafe(glEnableVertexAttribArray(1));
    glsafe(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_stride, (const void*)normal_offset));

    glsafe(glGenBuffers(1, &m_ibo_id));
    glsafe(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo_id));
    glsafe(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint16_t), indices.data(), GL_STATIC_DRAW));

    glsafe(glBindBuffer(GL_ARRAY_BUFFER, curr_array_buffer));
    glsafe(glBindVertexArray(curr_vertex_array));
}

void ToolMarker::render()
{
    if (m_vao_id == 0 || m_vbo_id == 0 || m_ibo_id == 0)
        return;

    int curr_vertex_array;
    glsafe(glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &curr_vertex_array));
    glcheck();

    glsafe(glBindVertexArray(m_vao_id));
    glsafe(glDrawElements(GL_TRIANGLES, m_indices_count, GL_UNSIGNED_SHORT, (const void*)0));
    glsafe(glBindVertexArray(curr_vertex_array));
}

const Vec3f& ToolMarker::get_position() const
{
    return m_position;
}

void ToolMarker::set_position(const Vec3f& position)
{
    m_position = position;
}

const Color& ToolMarker::get_color() const
{
    return m_color;
}

void ToolMarker::set_color(const Color& color)
{
    m_color = color;
}

float ToolMarker::get_alpha() const
{
    return m_alpha;
}

void ToolMarker::set_alpha(float alpha)
{
    m_alpha = std::clamp(alpha, 0.25f, 0.75f);
}

} // namespace libvgcode

//################################################################################################################################
// PrusaSlicer development only -> !!!TO BE REMOVED!!!
#endif // ENABLE_NEW_GCODE_VIEWER
//################################################################################################################################
