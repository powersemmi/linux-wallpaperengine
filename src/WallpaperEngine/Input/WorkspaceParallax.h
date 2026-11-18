#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <glm/vec2.hpp>

namespace WallpaperEngine::Input {
/**
 * Damped spring that follows a target the way niri animates a workspace switch.
 *
 * Mass is 1, so the spring is fully described by its stiffness and damping ratio, the same two numbers
 * niri's `workspace-switch { spring ... }` takes. Each step uses the closed-form solution, so the motion
 * does not depend on the frame rate and stays stable at any frame time.
 */
struct ParallaxSpring {
    float position = 0.5f;
    float velocity = 0.0f;

    /**
     * Advances the spring by dt seconds towards target.
     *
     * @return true while the spring is still moving, false once it came to rest on the target
     */
    bool step (float target, float dt, float stiffness, float dampingRatio, float epsilon);

    void snap (float target);
};

/**
 * Where the active workspace of each output sits, as a position in [0, 1] per axis.
 *
 * A background thread reads the positions from the compositor (niri's event stream) or from standard input
 * and publishes them under a mutex. Readers poll generation () once per frame, an atomic load, and take the
 * lock only when it changed.
 */
class WorkspacePositionSource {
public:
    enum class Kind { Niri, Stdin };

    explicit WorkspacePositionSource (Kind kind);
    ~WorkspacePositionSource ();

    WorkspacePositionSource (const WorkspacePositionSource&) = delete;
    WorkspacePositionSource& operator= (const WorkspacePositionSource&) = delete;

    /** @return A counter that changes whenever any output's position changes */
    [[nodiscard]] uint32_t generation () const { return this->m_generation.load (std::memory_order_acquire); }

    /** @return The position of the active workspace of the given output, if the source knows it */
    [[nodiscard]] std::optional<glm::vec2> position (const std::string& output) const;

private:
    struct NiriWorkspace {
	uint64_t id;
	uint64_t idx;
	std::string output;
	bool active;
    };

    void run ();
    bool openNiri ();
    void handleNiriLine (const std::string& line);
    void handleStdinLine (const std::string& line);
    void publishNiri ();
    void publish (const std::string& output, glm::vec2 position);

    Kind m_kind;
    int m_fd = -1;
    int m_wakeFd = -1;
    std::thread m_thread;
    mutable std::mutex m_mutex;
    std::map<std::string, glm::vec2> m_positions;
    std::atomic<uint32_t> m_generation = 0;
    /** niri's workspaces, only touched by the reader thread */
    std::vector<NiriWorkspace> m_workspaces;
};
} // namespace WallpaperEngine::Input
