# Sapana Physics Module

Opt-in rigid-body simulation backed by [Jolt Physics](https://github.com/jrouwe/JoltPhysics) (`v5.3.0` via CMake FetchContent).

## Rules

- **Do not** `#include` Jolt headers from game code. Use only `sapana/physics/*`.
- Physics is **opt-in**: entities without a `physics` block in scene JSON are visual-only.
- **Y-up**, units ≈ meters. Scene rotations are authored in **degrees**; conversion to Jolt quaternions happens inside `PhysicsSystem`.

## Components

| Component | Authoring | Purpose |
|-----------|-----------|---------|
| `RigidBody` | Scene JSON | `Static` / `Dynamic` / `Kinematic` (Kinematic reserved → treated as Static in v1) |
| `Collider` | Scene JSON | `Box` / `Plane` (finite thin box) / `Sphere` (reserved) |
| `PhysicsBody` | Runtime | Opaque `BodyId` created by `PhysicsSystem::CreateBodies` |
| `ForceMotor` | Optional | Translation-only tank/simple props (not used by sandbox drone) |
| `FlightMotor` | Scene JSON `motor` | Full 6-DOF quad. DJI: game-owned level attitude. FPV: Jolt + 4 motor forces. |

## Scene JSON

```json
"physics": {
  "body": "static" | "dynamic",
  "shape": "box" | "plane",
  "mass": 1.0,
  "friction": 0.5,
  "restitution": 0.0,
  "half_extents": [1, 1, 1]
},
"motor": {
  "enabled": true
}
```

Legacy motor force fields in scene JSON are ignored; flight tunables live in `config/flight.json`.

If `half_extents` is omitted or zero, extents are derived from `Transform.scale`.

## Config

- `config/physics.json` — gravity, fixed_dt
- `config/flight.json` — DJI/FPV rates, hover, altitude hold, drag, max tilt

## Frame loop (drone)

1. `Initialize(config)` once  
2. `CreateBodies(registry)` after scene load (`FlightMotor` → full DOFs)  
3. Each frame (Chase / FpvNose):
   - `FlightController::BuildCommand` → `QuadDynamics::Apply` **before** `PhysicsSystem::Update`
     - **DJI:** snap level attitude, world-up hover, altitude hold
     - **FPV:** four motor forces at arm corners along body up (idle = 0, gravity wins); rate PID torques; no `SetRotation`
   - `PhysicsSystem::Update` writes position + quaternion into `FlightMotor::Attitude`
4. Cameras parent to `FlightMotor::Attitude`

API helpers: `AddForce`, `AddTorque`, `Get/SetLinearVelocity`, `Get/SetAngularVelocity`, `SetRotationDegrees(..., resetAngularVelocity)`.

## Extension roadmap (not in v1)

- Contact listener → gameplay events  
- `HeightFieldShape` for real terrain  
- `CharacterVirtual`  
- Extra collision layers (triggers, projectiles)  
- Constraints / ragdolls  
- Kinematic platforms  
- Per-propeller visuals  
