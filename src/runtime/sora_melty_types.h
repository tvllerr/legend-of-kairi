/* sora_melty_types.h (s209 step 2, ours, MIT): the plain C types and numbers of the Melty lab calls that the game's glue
 * uses with or without the connector (runtime/melty/sora_melty.h includes this; runtime/sora_net_sock.h too, for its
 * no-op stand-ins). Numbers as Melty's protocol.md Notes and melty_addon.h say them. */
#ifndef SORA_MELTY_TYPES_H
#define SORA_MELTY_TYPES_H

#define SORA_MELTY_ACT_START 0
#define SORA_MELTY_ACT_END 1
#define SORA_MELTY_PLAYED 0
#define SORA_MELTY_PLAYED_REFUSED 1

/* autopilot's command kinds (melty_addon.h PilotKind's order) */
#define SORA_MELTY_PILOT_GOTO 0
#define SORA_MELTY_PILOT_FACE 1
#define SORA_MELTY_PILOT_LEVEL 2
#define SORA_MELTY_PILOT_WANDER 3
#define SORA_MELTY_PILOT_STAY 4
#define SORA_MELTY_PILOT_DIE 5
#define SORA_MELTY_PILOT_ACT 6
#define SORA_MELTY_PILOT_EVENT 7
#define SORA_MELTY_PILOT_ATTACK 8
#define SORA_MELTY_PILOT_STOP 9
/* the states a PILOT note says */
#define SORA_MELTY_PILOT_IDLE 0
#define SORA_MELTY_PILOT_MOVING 1
#define SORA_MELTY_PILOT_ARRIVED 2
#define SORA_MELTY_PILOT_STUCK 3
#define SORA_MELTY_PILOT_FAILED 4

typedef struct sora_melty_pilot {
    int number, kind;  /* its number (PILOT notes say it), its kind (SORA_MELTY_PILOT_*) */
    float x, y, z;     /* goto: where (BotW metres) */
    char gait[8];      /* goto: "", "walk", "run", "sprint" */
    float yaw;         /* face: the kit's heading (radians: 0 faces +z, turning towards +x) */
    char action[40];   /* act: the action's id */
    float seconds;     /* act: for how long (has_seconds) */
    int has_seconds;
    float radius;      /* wander */
    int peer;          /* attack: the player */
} sora_melty_pilot;

/* the kit's goto steering, one update (melty_addon.h PilotSteer, y up) */
typedef struct sora_melty_steer {
    float heading, distance;  /* the kit's heading to go (radians), the ground distance left (m) */
    int arrived, stuck, want_jump, state;
} sora_melty_steer;

#endif
