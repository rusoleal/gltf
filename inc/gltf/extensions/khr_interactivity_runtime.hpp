#pragma once

#include <gltf/extensions/khr_interactivity.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace systems::leal::gltf
{
    class GLTF;

    /**
     * A reference value (the `ref` type): null, a glTF object ("/nodes/3",
     * "/materials/0"), an event or a delay. Object references come from
     * literals in the graph (a JSON Pointer to the object) and are substituted
     * by their index into pointer templates ("/materials/{m}/..." with
     * m = /materials/4 gives "/materials/4/...").
     */
    struct InteractivityReference
    {
        enum class Kind
        {
            Null,
            Object, ///< `collection`[`index`]
            Event,  ///< an event occurrence (`id`)
            Delay   ///< a scheduled flow/setDelay activation (`id`)
        };
        Kind kind = Kind::Null;
        std::string collection; ///< Object: the top-level glTF array ("nodes", "materials", ...)
        int64_t index = -1;     ///< Object: the index in that array
        uint64_t id = 0;        ///< Event, Delay

        bool isNull() const { return kind == Kind::Null; }
        bool operator==(const InteractivityReference &o) const
        {
            return kind == o.kind && collection == o.collection && index == o.index && id == o.id;
        }
    };

    /**
     * A runtime value of one of the specification's value types. Floating-point
     * types (float, floatN, floatNxN) keep their components in `f` (matrices in
     * column-major order, as glTF); `i` holds an int, `b` a bool, `ref` a ref.
     */
    struct InteractivityRuntimeValue
    {
        InteractivityTypeSignature type = InteractivityTypeSignature::Float;
        std::array<double, 16> f{};
        int32_t i = 0;
        bool b = false;
        InteractivityReference ref;

        /// The number of float components of `type` (0 for bool, int, ref).
        static int componentCount(InteractivityTypeSignature type);
        /// The type default (spec "Custom Variable Types"): false, 0, NaN components, null.
        static InteractivityRuntimeValue defaultOf(InteractivityTypeSignature type);
        static InteractivityRuntimeValue ofBool(bool v);
        static InteractivityRuntimeValue ofInt(int32_t v);
        static InteractivityRuntimeValue ofFloat(double v);
        static InteractivityRuntimeValue ofFloats(InteractivityTypeSignature type, const double *values);
        static InteractivityRuntimeValue ofRef(const InteractivityReference &r);
    };

    /**
     * What the runtime needs from the application: the live state behind the
     * glTF Asset Object Model, and its outputs. Pointers are effective JSON
     * Pointers ("/nodes/3/translation", "/materials/4/pbrMetallicRoughness/
     * baseColorTexture/extensions/KHR_texture_transform/offset"), never templates.
     */
    class InteractivityHost
    {
    public:
        virtual ~InteractivityHost() = default;
        /// pointer/get: the property's current value. False when the pointer
        /// can't be resolved or the property's type isn't `type`.
        virtual bool getPointer(const std::string &pointer, InteractivityTypeSignature type,
                                InteractivityRuntimeValue &value) = 0;
        /// pointer/set: false when the pointer can't be resolved, the property
        /// is read-only or its type isn't value.type.
        virtual bool setPointer(const std::string &pointer, const InteractivityRuntimeValue &value) = 0;
        /// debug/log.
        virtual void log(const std::string &message) { (void)message; }
        /// event/send: a custom event the graph sends to the application.
        virtual void eventSent(int64_t eventIndex, const std::map<std::string, InteractivityRuntimeValue> &values)
        {
            (void)eventIndex;
            (void)values;
        }
        /// math/random and flow/multiGate's random order: a value in [0, 1).
        virtual double random();
        /// animation/start: show glTF animation `animation` at `time` seconds
        /// (within its keyframes, [0, maxTime]): sample its channels and write
        /// them to the scene. Called on every tick while the graph plays it.
        /// With a behavior graph, animations don't play by themselves: the
        /// application plays none.
        virtual void applyAnimation(int64_t animation, double time)
        {
            (void)animation;
            (void)time;
        }
        /// The graph stopped playing `animation` (animation/stop, or its end
        /// or stop time was reached). The properties it animated keep their
        /// current values.
        virtual void animationStopped(int64_t animation) { (void)animation; }
    };

    /**
     * Executes a KHR_interactivity behavior graph (the asset's default graph).
     *
     * The application calls start() once the asset is ready, tick() once per
     * rendered frame, and select() when the user selects a node (with
     * KHR_node_selectability: the node under the pointer, see the
     * extension's propagation rule, which select() applies). Everything runs
     * on the caller's thread, synchronously.
     *
     * Operations the runtime doesn't implement behave as the specification's
     * "no-op" (their flows do nothing, their outputs are type defaults); they
     * are listed in unsupportedOperations(). Each event's execution is bounded
     * (a step budget against endless loops); exceeding it stops that event
     * and adds an entry to errors().
     */
    class InteractivityRuntime
    {
    public:
        /// `asset` must outlive the runtime. Does nothing when the asset has
        /// no valid KHR_interactivity graph (valid() is false).
        InteractivityRuntime(const GLTF &asset, InteractivityHost &host);
        ~InteractivityRuntime();
        InteractivityRuntime(const InteractivityRuntime &) = delete;
        InteractivityRuntime &operator=(const InteractivityRuntime &) = delete;

        /// The asset has a valid graph to run.
        bool valid() const;

        /// event/onStart (once).
        void start();
        /// One frame: animations the graph plays advance (applyAnimation()),
        /// then variable/pointer interpolations, then delays that are due, then
        /// event/onTick. `seconds`: time since the previous tick (or since
        /// start() for the first).
        void tick(double seconds);
        /// event/onSelect for `node` and its ancestors (lowest first, until a
        /// handler stops propagation). Returns whether any handler ran.
        bool select(int64_t node, int32_t controllerIndex = 0, const double *selectionPoint = nullptr,
                    const double *selectionRayOrigin = nullptr);
        /// Whether `node` or one of its ancestors has an event/onSelect
        /// handler (to show it can be clicked).
        bool selectable(int64_t node) const;
        /// The node under the pointer of `controllerIndex` is now `node` (-1:
        /// none). The application picks it as for select(), honouring
        /// KHR_node_hoverability (the first hoverable node along the ray), and
        /// calls this when it may have changed (pointer or camera moved, the
        /// scene changed); calls with the same node do nothing. Fires
        /// event/onHoverOut for the subtrees the pointer left, then
        /// event/onHoverIn for those it entered (each from the hovered node up,
        /// stopping below the lowest common ancestor). Returns whether any
        /// handler ran.
        bool hover(int64_t node, int32_t controllerIndex = 0);
        /// Whether the graph plays an animation or interpolates a variable or
        /// property now (the application keeps drawing frames while it does).
        bool animating() const;
        /// Whether the graph has event/onHoverIn or event/onHoverOut handlers
        /// (the application need not pick on pointer moves otherwise).
        bool usesHover() const;
        /// event/receive: a custom event from the application.
        void receive(int64_t eventIndex, const std::map<std::string, InteractivityRuntimeValue> &values);

        /// Operation names the runtime runs as no-ops.
        const std::vector<std::string> &unsupportedOperations() const;
        /// Runtime errors so far (budget exceeded, ...), oldest first.
        const std::vector<std::string> &errors() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
