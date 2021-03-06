#pragma once
#include <OpenGL/gl3.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <Eigen/Dense>
#include <iostream>
#include <mutex>

#include "engine/gl.hh"
#include "engine/object_global.hh"

namespace engine {

class Window {
public:
    Window(size_t width, size_t height, GlobalObjectManager object_manager);
    ~Window();

public:
    void init();
    bool render_loop();
    void reset();

    GlobalObjectManager& manager();
    std::mutex& mutex();

public:
    void update_mouse_position_incremental(Eigen::Vector2f increment);

    void handle_mouse_event(const MouseEvent& event);
    void handle_keyboard_event(const KeyboardEvent& event);

private:
    double scale() const;
    Eigen::Matrix3f get_screen_from_world() const;

private:
    MouseEventManager mouse_;
    KeyboardEventManager keyboard_;

    GLFWwindow* window_;

    const Eigen::Vector2f kWindowDim;
    const Eigen::Vector2f kInitialHalfDim_;
    const Eigen::Vector2f kMinHalfDim_;
    const Eigen::Vector2f kMaxHalfDim_;

    Eigen::Vector2f center_;
    Eigen::Vector2f half_dim_;

    Eigen::Matrix3f world_from_screen_;
    std::optional<Eigen::Vector3f> previous_screen_mouse_;

    GlobalObjectManager object_manager_;
    std::mutex mutex_;
};

}  // namespace engine
