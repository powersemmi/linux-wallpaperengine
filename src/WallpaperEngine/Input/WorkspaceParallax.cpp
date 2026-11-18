#include "WorkspaceParallax.h"

#include "WallpaperEngine/Logging/Log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace WallpaperEngine::Input;

bool ParallaxSpring::step (
    const float target, const float dt, const float stiffness, const float dampingRatio, const float epsilon
) {
    const double omega = std::sqrt (static_cast<double> (stiffness));
    const double zeta = dampingRatio;
    const double x0 = this->position - target;
    const double v0 = this->velocity;
    const double t = dt;
    double x, v;

    if (std::abs (zeta - 1.0) < 1e-4) {
	// critically damped: x(t) = (x0 + (v0 + w x0) t) e^(-w t)
	const double c2 = v0 + omega * x0;
	const double e = std::exp (-omega * t);
	x = (x0 + c2 * t) * e;
	v = (v0 - omega * c2 * t) * e;
    } else if (zeta < 1.0) {
	// underdamped: oscillates around the target with a decaying envelope
	const double wd = omega * std::sqrt (1.0 - zeta * zeta);
	const double decay = zeta * omega;
	const double e = std::exp (-decay * t);
	const double c1 = x0;
	const double c2 = (v0 + decay * x0) / wd;
	const double cs = std::cos (wd * t);
	const double sn = std::sin (wd * t);
	x = e * (c1 * cs + c2 * sn);
	v = e * ((c2 * wd - decay * c1) * cs - (c1 * wd + decay * c2) * sn);
    } else {
	// overdamped: sum of two decaying exponentials
	const double root = omega * std::sqrt (zeta * zeta - 1.0);
	const double r1 = -zeta * omega + root;
	const double r2 = -zeta * omega - root;
	const double a = (v0 - r2 * x0) / (r1 - r2);
	const double b = x0 - a;
	const double e1 = std::exp (r1 * t);
	const double e2 = std::exp (r2 * t);
	x = a * e1 + b * e2;
	v = a * r1 * e1 + b * r2 * e2;
    }

    if (std::abs (x) < epsilon && std::abs (v) < epsilon * omega) {
	this->snap (target);
	return false;
    }

    this->position = static_cast<float> (target + x);
    this->velocity = static_cast<float> (v);
    return true;
}

void ParallaxSpring::snap (const float target) {
    this->position = target;
    this->velocity = 0.0f;
}

WorkspacePositionSource::WorkspacePositionSource (const Kind kind) : m_kind (kind) {
    this->m_wakeFd = eventfd (0, EFD_CLOEXEC | EFD_NONBLOCK);

    if (this->m_wakeFd < 0) {
	sLog.error ("Workspace parallax: cannot create an eventfd: ", strerror (errno));
	return;
    }

    if (kind == Kind::Stdin) {
	this->m_fd = STDIN_FILENO;
    } else if (!this->openNiri ()) {
	return;
    }

    this->m_thread = std::thread ([this] { this->run (); });
}

WorkspacePositionSource::~WorkspacePositionSource () {
    if (this->m_wakeFd >= 0) {
	constexpr uint64_t one = 1;
	[[maybe_unused]] const auto written = write (this->m_wakeFd, &one, sizeof (one));
    }

    if (this->m_thread.joinable ()) {
	this->m_thread.join ();
    }

    if (this->m_kind == Kind::Niri && this->m_fd >= 0) {
	close (this->m_fd);
    }

    if (this->m_wakeFd >= 0) {
	close (this->m_wakeFd);
    }
}

bool WorkspacePositionSource::openNiri () {
    const char* path = getenv ("NIRI_SOCKET");

    if (path == nullptr || *path == '\0') {
	sLog.error ("Workspace parallax: NIRI_SOCKET is not set, workspace switches will not move the wallpaper");
	return false;
    }

    sockaddr_un address {};
    address.sun_family = AF_UNIX;

    if (strlen (path) >= sizeof (address.sun_path)) {
	sLog.error ("Workspace parallax: NIRI_SOCKET path is too long");
	return false;
    }

    strcpy (address.sun_path, path);

    this->m_fd = socket (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

    if (this->m_fd < 0 || connect (this->m_fd, reinterpret_cast<sockaddr*> (&address), sizeof (address)) != 0) {
	sLog.error ("Workspace parallax: cannot connect to niri at ", path, ": ", strerror (errno));
	if (this->m_fd >= 0) {
	    close (this->m_fd);
	    this->m_fd = -1;
	}
	return false;
    }

    // ask for the event stream; niri answers with {"Ok":"Handled"} and then one event per line
    constexpr char request[] = "\"EventStream\"\n";

    if (write (this->m_fd, request, sizeof (request) - 1) != sizeof (request) - 1) {
	sLog.error ("Workspace parallax: cannot request niri's event stream: ", strerror (errno));
	close (this->m_fd);
	this->m_fd = -1;
	return false;
    }

    shutdown (this->m_fd, SHUT_WR);
    return true;
}

void WorkspacePositionSource::run () {
    std::string buffer;
    char chunk[4096];

    while (true) {
	pollfd fds[2] = {
	    { .fd = this->m_fd, .events = POLLIN, .revents = 0 },
	    { .fd = this->m_wakeFd, .events = POLLIN, .revents = 0 },
	};

	if (poll (fds, 2, -1) < 0) {
	    if (errno == EINTR) {
		continue;
	    }

	    sLog.error ("Workspace parallax: poll failed: ", strerror (errno));
	    return;
	}

	if (fds[1].revents != 0) {
	    return;
	}

	const ssize_t count = read (this->m_fd, chunk, sizeof (chunk));

	if (count < 0 && errno == EINTR) {
	    continue;
	}

	if (count <= 0) {
	    // niri went away or stdin closed: keep the last positions, the wallpaper stays where it is
	    sLog.out ("Workspace parallax: the position source closed");
	    return;
	}

	buffer.append (chunk, static_cast<size_t> (count));

	size_t start = 0;
	for (size_t end = buffer.find ('\n'); end != std::string::npos; end = buffer.find ('\n', start)) {
	    const std::string line = buffer.substr (start, end - start);
	    start = end + 1;

	    if (this->m_kind == Kind::Niri) {
		this->handleNiriLine (line);
	    } else {
		this->handleStdinLine (line);
	    }
	}

	buffer.erase (0, start);
    }
}

void WorkspacePositionSource::handleNiriLine (const std::string& line) {
    const auto json = nlohmann::json::parse (line, nullptr, false);

    if (json.is_discarded () || !json.is_object ()) {
	return;
    }

    if (const auto changed = json.find ("WorkspacesChanged"); changed != json.end ()) {
	this->m_workspaces.clear ();

	for (const auto& workspace : changed->value ("workspaces", nlohmann::json::array ())) {
	    if (!workspace.contains ("output") || !workspace["output"].is_string ()) {
		continue;
	    }

	    this->m_workspaces.push_back (
		{
		    .id = workspace.value ("id", uint64_t { 0 }),
		    .idx = workspace.value ("idx", uint64_t { 0 }),
		    .output = workspace["output"].get<std::string> (),
		    .active = workspace.value ("is_active", false),
		}
	    );
	}

	this->publishNiri ();
    } else if (const auto activated = json.find ("WorkspaceActivated"); activated != json.end ()) {
	const auto id = activated->value ("id", uint64_t { 0 });
	const auto found = std::ranges::find_if (this->m_workspaces, [id] (const auto& w) { return w.id == id; });

	if (found == this->m_workspaces.end ()) {
	    return;
	}

	const std::string output = found->output;

	for (auto& workspace : this->m_workspaces) {
	    if (workspace.output == output) {
		workspace.active = workspace.id == id;
	    }
	}

	this->publishNiri ();
    }
}

void WorkspacePositionSource::publishNiri () {
    // niri stacks workspaces vertically: the active one's place in its output's column, spread over [0, 1]
    std::map<std::string, std::pair<uint64_t, uint64_t>> outputs; // output -> (active idx, highest idx)

    for (const auto& workspace : this->m_workspaces) {
	auto& [active, highest] = outputs[workspace.output];
	highest = std::max (highest, workspace.idx);

	if (workspace.active) {
	    active = workspace.idx;
	}
    }

    for (const auto& [output, indices] : outputs) {
	const auto [active, highest] = indices;

	if (active == 0) {
	    continue;
	}

	const float y = highest > 1 ? static_cast<float> (active - 1) / static_cast<float> (highest - 1) : 0.5f;
	this->publish (output, { 0.5f, y });
    }
}

void WorkspacePositionSource::handleStdinLine (const std::string& line) {
    // "OUTPUT X Y", both in [0, 1]
    std::istringstream stream (line);
    std::string output;
    float x, y;

    if (!(stream >> output >> x >> y)) {
	sLog.error ("Workspace parallax: expected \"OUTPUT X Y\" on stdin, got \"", line, "\"");
	return;
    }

    this->publish (output, { std::clamp (x, 0.0f, 1.0f), std::clamp (y, 0.0f, 1.0f) });
}

void WorkspacePositionSource::publish (const std::string& output, const glm::vec2 position) {
    {
	std::scoped_lock lock (this->m_mutex);
	const auto existing = this->m_positions.find (output);

	if (existing != this->m_positions.end () && existing->second == position) {
	    return;
	}

	this->m_positions.insert_or_assign (output, position);
    }

    this->m_generation.fetch_add (1, std::memory_order_release);
}

std::optional<glm::vec2> WorkspacePositionSource::position (const std::string& output) const {
    std::scoped_lock lock (this->m_mutex);

    if (const auto found = this->m_positions.find (output); found != this->m_positions.end ()) {
	return found->second;
    }

    return std::nullopt;
}
