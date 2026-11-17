#pragma once
// Medieval Village: rolling terrain + carved river channel + water surface.
// Replace previous terrain.h with this file (rename to terrain.h).
#include "glad/glad.h"
#include "glm/glm.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace village_terrain {
namespace detail {
inline GLuint terrain_vao = 0, terrain_vbo = 0;
inline GLuint water_vao = 0, water_vbo = 0;
inline GLsizei terrain_count = 0, water_count = 0;
constexpr float half_size = 16.0f;
constexpr float water_y = -0.16f;

inline float river_center(float z) { return 5.4f + 0.75f * std::sin(z * 0.30f); }
inline float river_distance(float x, float z) { return std::abs(x - river_center(z)); }
inline float river_half_width(float z) { return 0.95f + 0.12f * std::sin(z * 0.43f + 0.3f); }

inline float height(float x, float z) {
    const float radius = std::sqrt(x*x + z*z);
    const float t = std::clamp((radius - 4.5f) / 8.0f, 0.0f, 1.0f);
    const float blend = t*t*(3.0f - 2.0f*t);
    const float waves = 0.38f + 0.22f * std::sin(0.43f*x + 0.26f*z)
                               + 0.14f * std::cos(0.67f*z - 0.18f*x);
    const float base = blend * std::max(0.0f, waves);
    // Water is below surrounding terrain. Inner channel ends under water.
    const float d = river_distance(x,z);
    const float bank = river_half_width(z) + 0.55f;
    const float r = std::clamp((bank - d) / (bank - river_half_width(z) + 0.001f), 0.0f, 1.0f);
    const float inner = std::clamp((river_half_width(z) - d) / 0.34f, 0.0f, 1.0f);
    const float cut = r*r*(3.0f-2.0f*r);
    const float depth = 0.35f + base;
    return base - depth * cut - 0.15f * inner;
}
inline glm::vec3 normal(float x,float z) {
    constexpr float e=0.035f;
    const float dx=(height(x+e,z)-height(x-e,z))/(2*e);
    const float dz=(height(x,z+e)-height(x,z-e))/(2*e);
    return glm::normalize(glm::vec3(-dx,1.0f,-dz));
}
inline void vertex(std::vector<float>& v, float x,float z) {
    auto n=normal(x,z);
    v.insert(v.end(),{x,height(x,z),z,n.x,n.y,n.z,(x+16.0f)*0.25f,(z+16.0f)*0.25f});
}
inline void water_vertex(std::vector<float>& v, float x,float z) {
    v.insert(v.end(),{x,water_y,z,0.0f,1.0f,0.0f,x*0.45f,z*0.45f});
}
inline void upload(GLuint& vao,GLuint& vbo,GLsizei& count,const std::vector<float>& vertices) {
    count=static_cast<GLsizei>(vertices.size()/8);
    glGenVertexArrays(1,&vao);
    glGenBuffers(1,&vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}
inline void bind_material(GLuint program, GLuint tex, const glm::vec3& tint) {
    glm::mat4 model(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(program,"u_model"),1,GL_FALSE,&model[0][0]);
    glUniform3fv(glGetUniformLocation(program,"u_tint"),1,&tint[0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,tex);
}
}

// Exposed for positioning the future bridge and paths.
inline float river_x(float z) { return detail::river_center(z); }
inline float ground_height(float x,float z) { return detail::height(x,z); }

inline void initialize() {
    if (detail::terrain_vao != 0) return;
    constexpr int cells=128;
    constexpr float step=32.0f/cells;
    std::vector<float> land;
    land.reserve(cells*cells*6*8);
    for(int z=0;z<cells;z++) for(int x=0;x<cells;x++) {
        const float x0=-16.0f+x*step, x1=x0+step;
        const float z0=-16.0f+z*step, z1=z0+step;
        detail::vertex(land,x0,z0); detail::vertex(land,x0,z1); detail::vertex(land,x1,z0);
        detail::vertex(land,x1,z0); detail::vertex(land,x0,z1); detail::vertex(land,x1,z1);
    }
    detail::upload(detail::terrain_vao,detail::terrain_vbo,detail::terrain_count,land);

    // Water ribbon follows the same curving centerline as the carved riverbed.
    std::vector<float> water;
    constexpr int segments=160;
    for(int i=0;i<segments;i++) {
        const float z0=-16.0f + 32.0f*i/segments;
        const float z1=-16.0f + 32.0f*(i+1)/segments;
        const float a0=detail::river_half_width(z0)-0.10f;
        const float a1=detail::river_half_width(z1)-0.10f;
        const float l0=detail::river_center(z0)-a0, r0=detail::river_center(z0)+a0;
        const float l1=detail::river_center(z1)-a1, r1=detail::river_center(z1)+a1;
        detail::water_vertex(water,l0,z0); detail::water_vertex(water,l1,z1); detail::water_vertex(water,r0,z0);
        detail::water_vertex(water,r0,z0); detail::water_vertex(water,l1,z1); detail::water_vertex(water,r1,z1);
    }
    detail::upload(detail::water_vao,detail::water_vbo,detail::water_count,water);
}

// Original interface retained; separate water texture supplied optionally.
inline void draw(GLuint shader_program, GLuint grass_texture, GLuint water_texture=0) {
    if(!detail::terrain_vao || !shader_program) return;
    detail::bind_material(shader_program,grass_texture,glm::vec3(1.0f));
    glBindVertexArray(detail::terrain_vao);
    glDrawArrays(GL_TRIANGLES,0,detail::terrain_count);
    if(water_texture != 0) {
        // The fragment shader scrolls two layers of water texture over time.
        // Only water is translucent; buildings and ground remain opaque.
        const GLint water_flag=glGetUniformLocation(shader_program,"u_is_water");
        if(water_flag >= 0) glUniform1i(water_flag,1);
        detail::bind_material(shader_program,water_texture,glm::vec3(1.0f));
        glDepthMask(GL_FALSE);
        glBindVertexArray(detail::water_vao);
        glDrawArrays(GL_TRIANGLES,0,detail::water_count);
        glDepthMask(GL_TRUE);
        if(water_flag >= 0) glUniform1i(water_flag,0);
    }
    glBindVertexArray(0);
}
inline void cleanup() {
    if(detail::terrain_vbo) glDeleteBuffers(1,&detail::terrain_vbo);
    if(detail::terrain_vao) glDeleteVertexArrays(1,&detail::terrain_vao);
    if(detail::water_vbo) glDeleteBuffers(1,&detail::water_vbo);
    if(detail::water_vao) glDeleteVertexArrays(1,&detail::water_vao);
    detail::terrain_vbo=detail::terrain_vao=detail::water_vbo=detail::water_vao=0;
    detail::terrain_count=detail::water_count=0;
}
} // namespace village_terrain
