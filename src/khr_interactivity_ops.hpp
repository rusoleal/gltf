#pragma once

#include <string>
#include <gltf/gltf.hpp>

/**
 * Checks every node of a structurally parsed KHR_interactivity graph against the
 * operations of the specification: known operation names, the input value sockets
 * each operation (or overload) requires and their types, the output value sockets
 * other nodes may read, and operation-specific configuration (variable, event and
 * pointer template rules). Extension-declared operations use their declared sockets.
 *
 * The first failure marks the graph invalid (graph.valid / graph.error); others are ignored.
 */
void checkInteractivityOperations(systems::leal::gltf::InteractivityGraph &graph, const std::string &context);
