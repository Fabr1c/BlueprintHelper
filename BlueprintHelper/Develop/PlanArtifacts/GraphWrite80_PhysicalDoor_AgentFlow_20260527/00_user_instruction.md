User instruction being simulated:

Create a new Actor Blueprint for an interactable physical door. The door must be internal-only, with no player, line trace, widget, UI, or character dependency. Build Root, Hinge, and DoorMesh components. DoorMesh starts static. Add two opening functions: LightPush and ForceOpen. Both open toward about 177 degrees, enable physics on DoorMesh, and apply different impulse strengths. Add a collision-close function that checks a closed threshold and disables physics when the door is closed.

Execution rule:

Setup creates the new asset, components, and state variables. After setup, each GraphWrite change is executed as a normal Agent TaskSpec preview/execute step, not through a custom E2E runner script.
