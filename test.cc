#include <cmath>

#include "engine/buffer.hh"
#include "engine/object_manager.hh"
#include "engine/shader.hh"
#include "engine/vao.hh"
#include "engine/window.hh"

namespace {
constexpr size_t kWidth = 1280;
constexpr size_t kHeight = 720;

static std::string vertex_shader_text = R"(
#version 330
layout (location = 0) in vec2 world_position;

void main()
{
    gl_Position = vec4(world_position.x, world_position.y, 0.0, 1.0);
}
)";

static std::string fragment_shader_text = R"(
#version 330
out vec4 fragment;

void main()
{
    fragment = vec4(1.0, 1.0, 1.0, 1.0);
}
)";

static std::string geometry_shader_text = R"(
#version 330
layout(triangles) in;
layout(triangle_strip, max_vertices = 10) out;

uniform mat3 screen_from_world;

vec4 to_screen(vec2 world)
{
    vec3 screen = screen_from_world * vec3(world.x, world.y, 1.0);
    return vec4(screen.x, screen.y, 0.0, 1.0);
}

void main()
{
    float thickness = 2;
    vec2 start = gl_in[0].gl_Position.xy;
    vec2 end = gl_in[1].gl_Position.xy;

    vec2 normal = thickness * normalize(vec2(-(end.y - start.y), end.x - start.x));

    // Draw the main section
    gl_Position = to_screen(start - normal);
    EmitVertex();
    gl_Position = to_screen(end - normal);
    EmitVertex();
    gl_Position = to_screen(start + normal);
    EmitVertex();
    gl_Position = to_screen(end + normal);
    EmitVertex();
    EndPrimitive();

    // Then the end cap (which connects to the next line)
    vec2 next = gl_in[2].gl_Position.xy;
    vec2 next_normal = thickness * normalize(vec2(-(next.y - end.y), next.x - end.x));

    gl_Position = to_screen(end - normal);
    EmitVertex();
    gl_Position = to_screen(end - next_normal);
    EmitVertex();
    gl_Position = to_screen(end);
    EmitVertex();
    EndPrimitive();

    gl_Position = to_screen(end + normal);
    EmitVertex();
    gl_Position = to_screen(end + next_normal);
    EmitVertex();
    gl_Position = to_screen(end);
    EmitVertex();
    EndPrimitive();
}
)";

static std::string point_geometry_shader_text = R"(
#version 330
layout(points) in;
layout(triangle_strip, max_vertices = 10) out;

uniform mat3 screen_from_world;

vec4 to_screen(vec2 world)
{
    vec3 screen = screen_from_world * vec3(world.x, world.y, 1.0);
    return vec4(screen.x, screen.y, 0.0, 1.0);
}

void main()
{
    float size = 5;

    gl_Position = to_screen(gl_in[0].gl_Position.xy + vec2(-size, size));
    EmitVertex();
    gl_Position = to_screen(gl_in[0].gl_Position.xy + vec2(size, size));
    EmitVertex();
    gl_Position = to_screen(gl_in[0].gl_Position.xy + vec2(-size, -size));
    EmitVertex();
    gl_Position = to_screen(gl_in[0].gl_Position.xy + vec2(size, -size));
    EmitVertex();
    EndPrimitive();
}
)";

template <typename Data>
size_t size_in_bytes(const std::vector<Data>& vec) {
    return vec.size() * sizeof(Data);
}
}  // namespace

class CatenarySolver {
public:
    CatenarySolver(Eigen::Vector2f start, Eigen::Vector2f end, float length) {
        start_ = start;
        diff_.x() = end.x() - start.x();
        diff_.y() = end.y() - start.y();
        length_ = length;
    }

    double f(double x) const { return alpha_ * std::cosh((x - x_offset_) / alpha_) + y_offset_; }

    constexpr static double sq(double in) { return in * in; }

    bool solve(double b = 10, double tol = 1E-3, size_t max_iter = 100) {
        size_t iter = 0;

        // Function we're optimizing which relates the free parameter (b) to the size of the opening we need.
        // [1] https://foggyhazel.wordpress.com/2018/02/12/catenary-passing-through-2-points/
        auto y = [this](double b) {
            return 1.0 / std::sqrt(2 * sq(b) * std::sinh(1 / (2 * sq(b))) - 1) -
                   1.0 / std::sqrt(std::sqrt(sq(length_) - sq(diff_.y())) / diff_.x() - 1);
        };
        // Derivative according to sympy
        auto dy = [](double b) {
            return (-2 * b * sinh(1 / (2 * sq(b))) + cosh(1.0 / (2.0 * sq(b))) / b) /
                   std::pow(2.0 * sq(b) * std::sinh(1.0 / (2.0 * sq(b))) - 1.0, 3.0 / 2.0);
        };

        // Newton iteration
        for (; iter < max_iter; ++iter) {
            float y_val = y(b);
            if (abs(y_val) < tol) break;
            b -= y_val / dy(b);
        }

        // Since b = sqrt(h / a), we can solve easily for a
        alpha_ = diff_.x() * sq(b);
        x_offset_ = 0.5 * (diff_.x() + alpha_ * std::log((length_ - diff_.y()) / (length_ + diff_.y())));
        y_offset_ = -f(0);
        return iter < max_iter;
    }

    std::vector<Eigen::Vector2f> trace(size_t steps) {
        std::vector<Eigen::Vector2f> result;
        result.reserve(steps);

        double step_size = diff_.x() / (steps - 1);
        double x = 0;
        for (size_t step = 0; step < steps; ++step, x += step_size) {
            result.push_back({x + start_.x(), f(x) + start_.y()});
        }
        return result;
    }

    float get_alpha() const { return alpha_; }

private:
    Eigen::Vector2f start_;
    Eigen::Vector2d diff_;
    double length_;
    float alpha_;
    float y_offset_ = 0;
    float x_offset_ = 0;
};

class TestObjectManager final : public engine::AbstractObjectManager {
public:
    TestObjectManager()
        : shader_(vertex_shader_text, fragment_shader_text, geometry_shader_text),
          point_shader_(vertex_shader_text, fragment_shader_text, point_geometry_shader_text) {}
    virtual ~TestObjectManager() = default;

    static constexpr size_t kNumSteps = 64;

    void init() override {
        shader_.init();
        point_shader_.init();

        sfw_ = glGetUniformLocation(shader_.get_program_id(), "screen_from_world");
        engine::throw_on_gl_error("glGetUniformLocation");

        reset();

        vao_.init();
        vbo_.init(GL_ARRAY_BUFFER, 0, 2, vao_);
        ebo_.init(GL_ELEMENT_ARRAY_BUFFER, vao_);

        auto elements = ebo_.batched_updater();
        for (size_t i = 0; i < kNumSteps - 1; ++i) {
            elements.push_back(i);
            elements.push_back(i + 1);
            elements.push_back(i + 2);
        }
    }

    void render(const Eigen::Matrix3f& screen_from_world) override {
        shader_.activate();
        gl_check(glUniformMatrix3fv, sfw_, 1, GL_FALSE, screen_from_world.data());
        gl_check_with_vao(vao_, glDrawElements, GL_TRIANGLES, ebo_.size(), GL_UNSIGNED_INT, (void*)0);

        point_shader_.activate();
        gl_check(glUniformMatrix3fv, sfw_, 1, GL_FALSE, screen_from_world.data());
        gl_check_with_vao(vao_, glDrawArrays, GL_POINTS, 0, 1);
        gl_check_with_vao(vao_, glDrawArrays, GL_POINTS, kNumSteps, 1);
    }

    void update(float) override {
        if (!needs_update_) return;
        needs_update_ = false;

        length_ = std::max(min_length(), length_);
        CatenarySolver solver{start_, end_, length_};
        if (!solver.solve(alpha_)) {
            throw std::runtime_error("CatenarySolver unable to converge!");
        }
        alpha_ = solver.get_alpha();
        auto points = solver.trace(kNumSteps);

        auto vertices = vbo_.batched_updater();
        vertices.resize(2 * (points.size() + 1));

        constexpr size_t stride = 2;
        for (size_t i = 0; i < points.size(); ++i) {
            const Eigen::Vector2f& point = points[i];

            size_t el = 0;
            vertices[stride * i + el++] = point.x();
            vertices[stride * i + el++] = point.y();
        }

        size_t el = 0;
        vertices[stride * points.size() + el++] = end_.x();
        vertices[stride * points.size() + el++] = end_.y();
    }

    void handle_mouse_event(const engine::MouseEvent& event) override {
        if (point_ && event.held()) {
            *point_ = event.mouse_position;
            needs_update_ = true;
        } else if (event.pressed()) {
            constexpr float kClickRadius = 10;
            if ((event.mouse_position - start_).squaredNorm() < kClickRadius) {
                point_ = &start_;
            } else if ((event.mouse_position - end_).squaredNorm() < kClickRadius) {
                point_ = &end_;
            }
        } else {
            point_ = nullptr;
        }
    }

    float min_length() const { return 1.01 * (end_ - start_).norm(); }

    void reset() {
        start_ = {100, 200};
        end_ = {200, 300};
        length_ = min_length();
    }

    void handle_keyboard_event(const engine::KeyboardEvent& event) override {
        if (event.space == true) {
            reset();
        }
    }

    Eigen::Vector2f* point_;
    float length_ = 0.0;
    Eigen::Vector2f start_;
    Eigen::Vector2f end_;
    float alpha_ = 10.0;

    bool needs_update_ = true;

    engine::Shader shader_;
    engine::Shader point_shader_;
    int sfw_;
    engine::VertexArrayObject vao_;
    engine::Buffer<float> vbo_;
    engine::Buffer<unsigned int> ebo_;
};

int main() {
    engine::GlobalObjectManager object_manager;
    object_manager.add_manager(std::make_shared<TestObjectManager>());
    engine::Window window{kWidth, kHeight, std::move(object_manager)};

    window.init();

    while (window.render_loop()) {
    }

    exit(EXIT_SUCCESS);
}