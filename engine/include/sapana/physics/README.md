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

## Scene JSON

```json
"physics": {
  "body": "static" | "dynamic",
  "shape": "box" | "plane",
  "mass": 1.0,
  "friction": 0.5,
  "restitution": 0.0,
  "half_extents": [1, 1, 1]
}
```

If `half_extents` is omitted or zero, extents are derived from `Transform.scale` (builtin cube spans `[-1,1]`, so scale is half-extent; plane uses scale XZ and a thin Y).

## Config (`config/physics.json`)

```json
{ "gravity": [0, -9.81, 0], "fixed_dt": 0.0166667, "max_substeps": 5 }
```

Missing file → engine defaults (same values).

## Frame loop

1. `Initialize(config)` once  
2. `CreateBodies(registry)` after scene load  
3. Each frame: optional `SetLinearVelocity` / `SetRotationDegrees` for driven bodies → `Update(registry, dt)` — fixed step, then **dynamic** poses overwrite `ecs::Transform`  
4. Render reads `Transform` as usual  

`SetGravityFactor(id, 0)` is used for the sandbox drone so it flies like freelook while still colliding.

## Extension roadmap (not in v1)

- Contact listener → gameplay events  
- `HeightFieldShape` for real terrain  
- `CharacterVirtual`  
- Extra collision layers (triggers, projectiles)  
- Constraints / ragdolls  
- Kinematic platforms  
