# Party Follow

`UPartyFollowComponent` records the controlled character's recent ground
trajectory and gives follower AI a `Move`, `Hold`, or `Avoid` command. The
component does not move characters directly.

## Formation activation

Party construction and controlled-character switching both configure a fresh
formation. Nearby followers hold their current location until the leader has
actually moved; this prevents spawned party members and reassigned followers
from immediately correcting toward an artificial formation point.

- `FormationActivationMoveDistance`: leader displacement required to activate
  the formation. Default: `30 cm`.
- `FormationActivationHoldMaxDistance`: followers within this leader distance
  hold before activation. Default: `340 cm`.
- Followers farther than the hold distance immediately catch up.
- Avoidance has priority over activation hold, so overlapping characters can
  still move apart safely.

`URPGAISubsystem::ConfigurePartyFollow` is the common entry point:

- `OnPartyConstructed` calls it with activation waiting enabled.
- A real controlled-character handoff calls it with activation waiting enabled.
- An initial possession event before party construction does not falsely count
  as a handoff.

After activation, followers reacquire their trajectory targets and request a
semantic left or right claim from the leader. The existing waiting, avoidance,
and settled-state behavior then proceeds normally.
