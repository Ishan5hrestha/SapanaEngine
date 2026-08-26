#include "sapana/physics/PhysicsSystem.hpp"

#include "sapana/ecs/Components.hpp"
#include "sapana/physics/PhysicsComponents.hpp"
#include "JoltLayerFilters.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <cstdarg>
#include <cstdio>
#include <iostream>

JPH_SUPPRESS_WARNINGS

namespace sapana
{
namespace physics
{

namespace
{

using Diligent::float3;
using Diligent::PI_F;

void TraceImpl(const char* fmt, ...)
{
    char    buffer[1024];
    va_list list;
    va_start(list, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, list);
    va_end(list);
    std::cerr << "Jolt: " << buffer << '\n';
}

#ifdef JPH_ENABLE_ASSERTS
bool AssertFailedImpl(const char* expression, const char* message, const char* file, JPH::uint line)
{
    std::cerr << "Jolt assert " << file << ":" << line << " (" << expression << ") "
              << (message != nullptr ? message : "") << '\n';
    return true;
}
#endif

JPH::Vec3 ToJolt(const float3& v)
{
    return JPH::Vec3(v.x, v.y, v.z);
}

float3 FromJolt(JPH::RVec3Arg v)
{
    return float3{static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ())};
}

/// Matches ecs::Transform::ToMatrix rotation order: Rx * Ry * Rz (degrees).
JPH::Quat EulerDegreesToQuat(const float3& degrees)
{
    const float rx = degrees.x * (PI_F / 180.f);
    const float ry = degrees.y * (PI_F / 180.f);
    const float rz = degrees.z * (PI_F / 180.f);
    return JPH::Quat::sRotation(JPH::Vec3::sAxisX(), rx) *
           JPH::Quat::sRotation(JPH::Vec3::sAxisY(), ry) *
           JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), rz);
}

float3 QuatToEulerDegrees(JPH::QuatArg q)
{
    // Jolt GetEulerAngles returns XYZ radians in the same convention we need for display sync.
    const JPH::Vec3 rad = q.GetEulerAngles();
    return float3{
        rad.GetX() * (180.f / PI_F),
        rad.GetY() * (180.f / PI_F),
        rad.GetZ() * (180.f / PI_F)};
}

float3 ResolveHalfExtents(const Collider& collider, const ecs::Transform& transform)
{
    if (collider.HalfExtents.x > 0.f || collider.HalfExtents.y > 0.f || collider.HalfExtents.z > 0.f)
        return collider.HalfExtents;

    switch (collider.Shape)
    {
        case ShapeType::Plane:
            // Visual plane is unit XZ [-1,1]; scale is half-extent on X/Z. Thin Y for a finite ground.
            return float3{transform.Scale.x, 0.05f, transform.Scale.z};
        case ShapeType::Box:
        case ShapeType::Sphere:
        default:
            // Builtin cube spans [-1,1]; Scale is already half-extent in each axis.
            return transform.Scale;
    }
}

JPH::RVec3 BodyPositionForShape(ShapeType shape, const float3& transformPos, const float3& halfExtents)
{
    // Plane visual lies on Transform.Position; sink the thin box so its top matches that plane.
    if (shape == ShapeType::Plane)
        return JPH::RVec3(transformPos.x, transformPos.y - halfExtents.y, transformPos.z);
    return JPH::RVec3(transformPos.x, transformPos.y, transformPos.z);
}

} // namespace

struct PhysicsSystem::Impl
{
    PhysicsConfig Config;

    std::unique_ptr<JPH::TempAllocatorImpl>        TempAllocator;
    std::unique_ptr<JPH::JobSystemSingleThreaded>  JobSystem;
    std::unique_ptr<JPH::PhysicsSystem>            World;

    jolt_layers::BPLayerInterfaceImpl             BroadPhaseLayers;
    jolt_layers::ObjectVsBroadPhaseLayerFilterImpl ObjectVsBroadPhase;
    jolt_layers::ObjectLayerPairFilterImpl        ObjectVsObject;

    float Accumulator = 0.f;
    bool  TypesRegistered = false;
};

PhysicsSystem::PhysicsSystem()
    : m_Impl(std::make_unique<Impl>())
{
}

PhysicsSystem::~PhysicsSystem()
{
    Shutdown();
}

bool PhysicsSystem::IsInitialized() const
{
    return m_Impl != nullptr && m_Impl->World != nullptr;
}

bool PhysicsSystem::Initialize(const PhysicsConfig& config)
{
    if (m_Impl == nullptr)
        m_Impl = std::make_unique<Impl>();

    Shutdown();

    m_Impl->Config = config;
    if (m_Impl->Config.FixedDt <= 0.f)
        m_Impl->Config.FixedDt = 1.f / 60.f;
    if (m_Impl->Config.MaxSubsteps < 1)
        m_Impl->Config.MaxSubsteps = 1;

    JPH::RegisterDefaultAllocator();
    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

    if (JPH::Factory::sInstance == nullptr)
        JPH::Factory::sInstance = new JPH::Factory();

    JPH::RegisterTypes();
    m_Impl->TypesRegistered = true;

    m_Impl->TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
    // Single-threaded job system — fine for Intel HD 620 / sandbox scale.
    m_Impl->JobSystem     = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);

    constexpr JPH::uint cMaxBodies             = 4096;
    constexpr JPH::uint cNumBodyMutexes        = 0;
    constexpr JPH::uint cMaxBodyPairs          = 4096;
    constexpr JPH::uint cMaxContactConstraints = 4096;

    m_Impl->World = std::make_unique<JPH::PhysicsSystem>();
    m_Impl->World->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
                        m_Impl->BroadPhaseLayers, m_Impl->ObjectVsBroadPhase, m_Impl->ObjectVsObject);

    m_Impl->World->SetGravity(ToJolt(m_Impl->Config.Gravity));
    m_Impl->Accumulator = 0.f;

    std::cerr << "Sapana PhysicsSystem: initialized (Jolt, fixed_dt=" << m_Impl->Config.FixedDt << ")\n";
    return true;
}

void PhysicsSystem::Shutdown()
{
    if (m_Impl == nullptr)
        return;

    if (m_Impl->World != nullptr)
    {
        JPH::BodyInterface& bodies = m_Impl->World->GetBodyInterface();
        JPH::BodyIDVector   ids;
        m_Impl->World->GetBodies(ids);
        for (JPH::BodyID id : ids)
        {
            bodies.RemoveBody(id);
            bodies.DestroyBody(id);
        }
        m_Impl->World.reset();
    }

    m_Impl->JobSystem.reset();
    m_Impl->TempAllocator.reset();
    m_Impl->Accumulator = 0.f;

    if (m_Impl->TypesRegistered)
    {
        JPH::UnregisterTypes();
        m_Impl->TypesRegistered = false;
    }

    if (JPH::Factory::sInstance != nullptr)
    {
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

void PhysicsSystem::CreateBodies(entt::registry& registry)
{
    if (!IsInitialized())
    {
        std::cerr << "Sapana PhysicsSystem: CreateBodies called before Initialize\n";
        return;
    }

    JPH::BodyInterface& bodyInterface = m_Impl->World->GetBodyInterface();

    auto view = registry.view<ecs::Transform, RigidBody, Collider>(entt::exclude<PhysicsBody>);
    for (auto entity : view)
    {
        const auto& transform = view.get<ecs::Transform>(entity);
        const auto& rigid     = view.get<RigidBody>(entity);
        const auto& collider  = view.get<Collider>(entity);

        BodyType bodyType = rigid.Type;
        if (bodyType == BodyType::Kinematic)
        {
            std::cerr << "Sapana PhysicsSystem: Kinematic reserved; treating as Static\n";
            bodyType = BodyType::Static;
        }

        if (collider.Shape == ShapeType::Sphere)
        {
            std::cerr << "Sapana PhysicsSystem: Sphere shape reserved; skipping entity\n";
            continue;
        }

        const float3 halfExtents = ResolveHalfExtents(collider, transform);
        if (halfExtents.x <= 0.f || halfExtents.y <= 0.f || halfExtents.z <= 0.f)
        {
            std::cerr << "Sapana PhysicsSystem: invalid half extents; skipping entity\n";
            continue;
        }

        JPH::BoxShapeSettings shapeSettings(ToJolt(halfExtents));
        shapeSettings.SetEmbedded();
        JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
        if (shapeResult.HasError())
        {
            std::cerr << "Sapana PhysicsSystem: shape create failed: " << shapeResult.GetError() << '\n';
            continue;
        }
        JPH::ShapeRefC shape = shapeResult.Get();

        const JPH::EMotionType motion =
            (bodyType == BodyType::Dynamic) ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static;
        const JPH::ObjectLayer layer =
            (bodyType == BodyType::Dynamic) ? jolt_layers::Layers::MOVING : jolt_layers::Layers::NON_MOVING;

        JPH::BodyCreationSettings settings(
            shape,
            BodyPositionForShape(collider.Shape, transform.Position, halfExtents),
            EulerDegreesToQuat(transform.RotationDegrees),
            motion,
            layer);
        settings.mFriction    = rigid.Friction;
        settings.mRestitution = rigid.Restitution;
        if (bodyType == BodyType::Dynamic && rigid.Mass > 0.f)
        {
            settings.mOverrideMassProperties       = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = rigid.Mass;
        }

        // ForceMotor: translation only (game snaps yaw). FlightMotor: full 6-DOF quad.
        if (bodyType == BodyType::Dynamic && registry.all_of<ForceMotor>(entity) &&
            !registry.all_of<FlightMotor>(entity))
        {
            settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY |
                                    JPH::EAllowedDOFs::TranslationZ;
        }
        else if (bodyType == BodyType::Dynamic && registry.all_of<FlightMotor>(entity))
        {
            settings.mAllowedDOFs = JPH::EAllowedDOFs::All;
        }

        const JPH::EActivation activation =
            (bodyType == BodyType::Dynamic) ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;

        const JPH::BodyID joltId = bodyInterface.CreateAndAddBody(settings, activation);
        if (joltId.IsInvalid())
        {
            std::cerr << "Sapana PhysicsSystem: failed to create body\n";
            continue;
        }

        PhysicsBody handle;
        handle.Id = joltId.GetIndexAndSequenceNumber();
        registry.emplace<PhysicsBody>(entity, handle);
    }

    m_Impl->World->OptimizeBroadPhase();
}

void PhysicsSystem::Update(entt::registry& registry, float elapsedSeconds)
{
    if (!IsInitialized() || elapsedSeconds <= 0.f)
        return;

    m_Impl->Accumulator += elapsedSeconds;
    const float fixedDt    = m_Impl->Config.FixedDt;
    int         steps      = 0;

    while (m_Impl->Accumulator >= fixedDt && steps < m_Impl->Config.MaxSubsteps)
    {
        m_Impl->World->Update(fixedDt, 1, m_Impl->TempAllocator.get(), m_Impl->JobSystem.get());
        m_Impl->Accumulator -= fixedDt;
        ++steps;
    }

    // Avoid spiral of death if frame hitch was huge.
    if (m_Impl->Accumulator > fixedDt * static_cast<float>(m_Impl->Config.MaxSubsteps))
        m_Impl->Accumulator = 0.f;

    JPH::BodyInterface& bodyInterface = m_Impl->World->GetBodyInterface();

    auto view = registry.view<ecs::Transform, RigidBody, PhysicsBody>();
    for (auto entity : view)
    {
        const auto& rigid = view.get<RigidBody>(entity);
        if (rigid.Type != BodyType::Dynamic)
            continue;

        const auto& handle = view.get<PhysicsBody>(entity);
        if (handle.Id == kInvalidBodyId)
            continue;

        const JPH::BodyID joltId(handle.Id);
        if (!bodyInterface.IsAdded(joltId))
            continue;

        auto& transform    = view.get<ecs::Transform>(entity);
        transform.Position = FromJolt(bodyInterface.GetPosition(joltId));

        // Flight craft: keep quaternion from Jolt (FPV integrates here; DJI snapped it this frame).
        if (registry.all_of<FlightMotor>(entity))
        {
            auto& motor = registry.get<FlightMotor>(entity);
            JPH::Quat jq = bodyInterface.GetRotation(joltId);
            if (!jq.IsNormalized())
                jq = jq.Normalized();
            const Diligent::QuaternionF att{jq.GetX(), jq.GetY(), jq.GetZ(), jq.GetW()};
            motor.Attitude            = att;
            motor.AttitudeInitialized = true;
            transform.RotationQuat    = att;
            transform.UseRotationQuat = true;
            continue;
        }

        transform.RotationDegrees = QuatToEulerDegrees(bodyInterface.GetRotation(joltId));
    }
}

void PhysicsSystem::SetLinearVelocity(BodyId id, const Diligent::float3& velocity)
{
    if (!IsInitialized() || id == kInvalidBodyId)
        return;

    const JPH::BodyID joltId(id);
    JPH::BodyInterface& bodies = m_Impl->World->GetBodyInterface();
    if (!bodies.IsAdded(joltId))
        return;

    bodies.SetLinearVelocity(joltId, ToJolt(velocity));
    bodies.ActivateBody(joltId);
}

void PhysicsSystem::SetRotationDegrees(BodyId id, const Diligent::float3& eulerDegrees, bool resetAngularVelocity)
{
    if (!IsInitialized() || id == kInvalidBodyId)
        return;

    const JPH::BodyID joltId(id);
    JPH::BodyInterface& bodies = m_Impl->World->GetBodyInterface();
    if (!bodies.IsAdded(joltId))
        return;

    bodies.SetRotation(joltId, EulerDegreesToQuat(eulerDegrees), JPH::EActivation::Activate);
    if (resetAngularVelocity)
        bodies.SetAngularVelocity(joltId, JPH::Vec3::sZero());
}

void PhysicsSystem::SetRotation(BodyId id, const Diligent::QuaternionF& rotation, bool resetAngularVelocity)
{
    if (!IsInitialized() || id == kInvalidBodyId)
        return;

    const JPH::BodyID joltId(id);
    JPH::BodyInterface& bodies = m_Impl->World->GetBodyInterface();
    if (!bodies.IsAdded(joltId))
        return;

    const JPH::Quat q(rotation.q.x, rotation.q.y, rotation.q.z, rotation.q.w);
    bodies.SetRotation(joltId, q.IsNormalized() ? q : q.Normalized(), JPH::EActivation::Activate);
    if (resetAngularVelocity)
        bodies.SetAngularVelocity(joltId, JPH::Vec3::sZero());
}

void PhysicsSystem::SetGravityFactor(BodyId id, float factor)
{
    if (!IsInitialized() || id == kInvalidBodyId)
        return;

    const JPH::BodyID joltId(id);
    JPH::BodyInterface& bodies = m_Impl->World->GetBodyInterface();
    if (!bodies.IsAdded(joltId))
        return;

    bodies.SetGravityFactor(joltId, factor);
}

void PhysicsSystem::AddForce(BodyId id, const Diligent::float3& force)
{
    if (!IsInitialized() || id == kInvalidBodyId)
        return;

    const JPH::BodyID joltId(id);
    JPH::BodyInterface& bodies = m_Impl->World->GetBodyInterface();
    if (!bodies.IsAdded(joltId))
        return;

    bodies.AddForce(joltId, ToJolt(force));
    bodies.ActivateBody(joltId);
}

void PhysicsSystem::AddForceAtWorldPoint(BodyId id, const Diligent::float3& force, const Diligent::float3& worldPoint)
{
    if (!IsInitialized() || id == kInvalidBodyId)
        return;

    const JPH::BodyID joltId(id);
    JPH::BodyInterface& bodies = m_Impl->World->GetBodyInterface();
    if (!bodies.IsAdded(joltId))
        return;

    bodies.AddForce(joltId, ToJolt(force), JPH::RVec3(worldPoint.x, worldPoint.y, worldPoint.z));
    bodies.ActivateBody(joltId);
}

void PhysicsSystem::AddTorque(BodyId id, const Diligent::float3& torque)
{
    if (!IsInitialized() || id == kInvalidBodyId)
        return;

    const JPH::BodyID joltId(id);
    JPH::BodyInterface& bodies = m_Impl->World->GetBodyInterface();
    if (!bodies.IsAdded(joltId))
        return;

    bodies.AddTorque(joltId, ToJolt(torque));
    bodies.ActivateBody(joltId);
}

Diligent::float3 PhysicsSystem::GetLinearVelocity(BodyId id)
{
    if (!IsInitialized() || id == kInvalidBodyId)
        return Diligent::float3{0.f, 0.f, 0.f};

    const JPH::BodyID joltId(id);
    JPH::BodyInterface& bodies = m_Impl->World->GetBodyInterface();
    if (!bodies.IsAdded(joltId))
        return Diligent::float3{0.f, 0.f, 0.f};

    const JPH::Vec3 v = bodies.GetLinearVelocity(joltId);
    return Diligent::float3{v.GetX(), v.GetY(), v.GetZ()};
}

Diligent::float3 PhysicsSystem::GetAngularVelocity(BodyId id)
{
    if (!IsInitialized() || id == kInvalidBodyId)
        return Diligent::float3{0.f, 0.f, 0.f};

    const JPH::BodyID joltId(id);
    JPH::BodyInterface& bodies = m_Impl->World->GetBodyInterface();
    if (!bodies.IsAdded(joltId))
        return Diligent::float3{0.f, 0.f, 0.f};

    const JPH::Vec3 w = bodies.GetAngularVelocity(joltId);
    return Diligent::float3{w.GetX(), w.GetY(), w.GetZ()};
}

Diligent::float3 PhysicsSystem::GetPosition(BodyId id)
{
    if (!IsInitialized() || id == kInvalidBodyId)
        return Diligent::float3{0.f, 0.f, 0.f};

    const JPH::BodyID joltId(id);
    JPH::BodyInterface& bodies = m_Impl->World->GetBodyInterface();
    if (!bodies.IsAdded(joltId))
        return Diligent::float3{0.f, 0.f, 0.f};

    return FromJolt(bodies.GetPosition(joltId));
}

Diligent::QuaternionF PhysicsSystem::GetRotation(BodyId id)
{
    if (!IsInitialized() || id == kInvalidBodyId)
        return Diligent::QuaternionF{0.f, 0.f, 0.f, 1.f};

    const JPH::BodyID joltId(id);
    JPH::BodyInterface& bodies = m_Impl->World->GetBodyInterface();
    if (!bodies.IsAdded(joltId))
        return Diligent::QuaternionF{0.f, 0.f, 0.f, 1.f};

    JPH::Quat q = bodies.GetRotation(joltId);
    if (!q.IsNormalized())
        q = q.Normalized();
    return Diligent::QuaternionF{q.GetX(), q.GetY(), q.GetZ(), q.GetW()};
}

void PhysicsSystem::SetAngularVelocity(BodyId id, const Diligent::float3& angularVelocity)
{
    if (!IsInitialized() || id == kInvalidBodyId)
        return;

    const JPH::BodyID joltId(id);
    JPH::BodyInterface& bodies = m_Impl->World->GetBodyInterface();
    if (!bodies.IsAdded(joltId))
        return;

    bodies.SetAngularVelocity(joltId, ToJolt(angularVelocity));
    bodies.ActivateBody(joltId);
}

} // namespace physics
} // namespace sapana
