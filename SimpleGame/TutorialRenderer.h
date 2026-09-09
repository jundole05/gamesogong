#pragma once
#include "Dependencies/glew.h"
#include <vector>
#include <string>
#include <map>

namespace tutorial {
struct Vec2 {
    float x, y;
    Vec2(float px = 0, float py = 0) : x(px), y(py) {}
    Vec2 operator+(Vec2 b) const { return Vec2(x + b.x, y + b.y); }
    Vec2 operator-(Vec2 b) const { return Vec2(x - b.x, y - b.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
};
struct Color {
    float r, g, b, a;
    Color(unsigned rgb = 0xffffff, float alpha = 1) :
        r(((rgb >> 16) & 255) / 255.f), g(((rgb >> 8) & 255) / 255.f),
        b((rgb & 255) / 255.f), a(alpha) {}
};

// Screen coordinates are top-left based in a letterboxed 1280 x 720 canvas.
// Geometry and Korean text use the same shader and alpha-blended triangle stream.
class TutorialRenderer {
public:
    bool Initialize();
    void Shutdown();
    void Resize(int width, int height);
    void Begin(float time = 0);
    void Composite(); // Finish world post-processing before drawing sharp UI.
    void End();
    void Triangle(Vec2 a, Vec2 b, Vec2 c, Color color);
    void Quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color);
    void Rect(float x, float y, float w, float h, Color color);
    void Ellipse(Vec2 p, float rx, float ry, Color color, int segments = 24);
    void Line(Vec2 a, Vec2 b, float width, Color color);
    void SoftEllipse(Vec2 p, float rx, float ry, Color color);
    void MaterialQuad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color, int material, Vec2 origin, Vec2 span);
    void Character(Vec2 feet, unsigned color, int direction, int frame, float alpha = 1);
    void Text(float x, float y, const std::wstring& text, int size = 18,
              Color color = Color(0xece5d2));
    void TextBlock(float x, float y, int maxWidth, const std::wstring& text,
                   int size = 18, Color color = Color(0xece5d2));
private:
    struct Vertex { float x, y, u, v, r, g, b, a, material; };
    struct Label { GLuint texture = 0; int width = 0, height = 0; };
    std::vector<Vertex> vertices;
    std::map<std::pair<int, std::wstring>, Label> labels;
    std::map<std::pair<int, std::wstring>, std::vector<std::wstring>> wrapped;
    GLuint program = 0, vao = 0, vbo = 0, white = 0, active = 0;
    GLuint post = 0, sceneFbo = 0, sceneTexture = 0;
    GLuint bloomFbo[2] = {}, bloomTexture[2] = {};
    std::map<unsigned, GLuint> sprites;
    bool composited = false;
    int viewX = 0, viewY = 0, viewW = 1280, viewH = 720;
    void Flush();
    void Texture(GLuint texture);
    void Push(Vec2 p, Vec2 uv, Color color, float material = 0);
    bool CreatePost();
    Label MakeLabel(const std::wstring& text, int size);
};
}
