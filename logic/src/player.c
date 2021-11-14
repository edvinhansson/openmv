#include <math.h>
#include <stdio.h>
#include <string.h>

#include "coresys.h"
#include "keymap.h"
#include "logic_store.h"
#include "platform.h"
#include "player.h"
#include "res.h"
#include "sprites.h"

struct player_constants {
	float move_speed;
	float jump_force;
	float gravity;
	float accel;
	float friction;
};

const struct player_constants player_constants = {
	.move_speed = 300,
	.jump_force = -550,
	.gravity = 1000,
	.accel = 1000,
	.friction = 1300
};

entity new_player_entity(struct world* world) {
	struct texture* tex = load_texture("res/bmp/char.bmp");

	entity e = new_entity(world);
	add_componentv(world, e, struct transform, .dimentions = { 64, 64 });
	add_componentv(world, e, struct player, .position = { 128, 128 }, .collider = { 4*4, 1*4, 9*4, 15*4 });
	add_component(world, e, struct animated_sprite, get_animated_sprite(animsprid_player_run_right));
	
	return e;
}

void player_system(struct world* world, struct renderer* renderer, struct room** room, double ts) {
	for (view(world, view,
			type_info(struct transform),
			type_info(struct player),
			type_info(struct animated_sprite))) {
		struct transform* transform = view_get(&view, struct transform);
		struct player* player = view_get(&view, struct player);
		struct animated_sprite* sprite = view_get(&view, struct animated_sprite);

		player->velocity.y += player_constants.gravity * ts;

		if (key_pressed(main_window, mapped_key("right"))) {
			if (player->velocity.x < player_constants.move_speed) {
				player->velocity.x += player_constants.accel * ts;
			}
			
			player->face = player_face_right;
		} else if (key_pressed(main_window, mapped_key("left"))) {
			if (player->velocity.x > -player_constants.move_speed) {
				player->velocity.x -= player_constants.accel * ts;
			}

			player->face = player_face_left;
		} else {
			if (player->velocity.x > 0.0f) {
				player->velocity.x -= player_constants.friction * ts;
			} else {
				player->velocity.x += player_constants.friction * ts;
			}
		}

		player->position = v2f_add(player->position, v2f_mul(player->velocity, make_v2f(ts, ts)));

		handle_body_collisions(room, player->collider, &player->position, &player->velocity);
		handle_body_transitions(room, player->collider, &player->position);

		{
			struct rect ground_test_rect = {
				player->position.x + player->collider.x,
				player->position.y + player->collider.y + player->collider.h,
				player->collider.w,
				3
			};

			v2i normal;
			if (rect_room_overlap(*room, ground_test_rect, &normal)) {
				player->on_ground = normal.y == 1;
			} else {
				player->on_ground = false;
			}
		}

		if (key_just_pressed(main_window, mapped_key("jump")) && player->on_ground) {
			player->velocity.y = player_constants.jump_force;
		}

		if (player->on_ground) {
			if (player->velocity.x > 0.5f || player->velocity.x < -0.5f) {
				if (player->face == player_face_left) {
					if (sprite->id != animsprid_player_run_left) {
						*sprite = get_animated_sprite(animsprid_player_run_left);
					}
				} else {
					if (sprite->id != animsprid_player_run_right) {
						*sprite = get_animated_sprite(animsprid_player_run_right);
					}
				}
			} else {
				if (player->face == player_face_left) {
					*sprite = get_animated_sprite(animsprid_player_idle_left);
				} else {
					*sprite = get_animated_sprite(animsprid_player_idle_right);
				}
			}
		} else {
			if (player->velocity.y < 0.0f) {
				if (player->face == player_face_left) {
					*sprite = get_animated_sprite(animsprid_player_jump_left);
				} else {
					*sprite = get_animated_sprite(animsprid_player_jump_right);
				}
			} else {
				if (player->face == player_face_left) {
					*sprite = get_animated_sprite(animsprid_player_fall_left);
				} else {
					*sprite = get_animated_sprite(animsprid_player_fall_right);
				}
			}
		}

		transform->position = make_v2i((i32)player->position.x, (i32)player->position.y);

		float distance_to_player = sqrtf(powf(logic_store->camera_position.x - player->position.x, 2)
			+ powf(logic_store->camera_position.y - player->position.y, 2));

		v2f camera_dir = v2f_normalised(v2f_sub(player->position, logic_store->camera_position));

		logic_store->camera_position.x += camera_dir.x * distance_to_player * ts * 10.0f;
		logic_store->camera_position.y += camera_dir.y * distance_to_player * ts * 10.0f;

		renderer->camera_pos = make_v2i((i32)logic_store->camera_position.x, (i32)logic_store->camera_position.y);
	}
}
