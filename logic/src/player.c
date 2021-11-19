#include <math.h>
#include <stdio.h>
#include <string.h>

#include "consts.h"
#include "coresys.h"
#include "fx.h"
#include "keymap.h"
#include "logic_store.h"
#include "physics.h"
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
	float dash_force;
	i32 ground_hit_range;
	double max_jump;
	double max_dash;
	double dash_fx_interval;
	double dash_cooldown;

	i32 max_air_dash;

	struct rect left_collider;
	struct rect right_collider;
};

const struct player_constants player_constants = {
	.move_speed = 300,
	.jump_force = -350,
	.gravity = 1000,
	.accel = 1000,
	.friction = 1300,
	.dash_force = 1000,
	.ground_hit_range = 12,
	.max_jump = 0.23,
	.max_dash = 0.15,
	.dash_fx_interval = 0.045,
	.dash_cooldown = 0.3,

	.max_air_dash = 3,

	.right_collider = { 4 * sprite_scale, 1 * sprite_scale, 9 * sprite_scale, 15 * sprite_scale },
	.left_collider =  { 3 * sprite_scale, 1 * sprite_scale, 9 * sprite_scale, 15 * sprite_scale }
};

entity new_player_entity(struct world* world) {
	struct texture* tex = load_texture("res/bmp/char.bmp");

	entity e = new_entity(world);
	add_componentv(world, e, struct transform, .dimentions = { 64, 64 });
	add_componentv(world, e, struct player, .position = { 128, 128 }, .collider = player_constants.left_collider);
	add_component(world, e, struct animated_sprite, get_animated_sprite(animsprid_player_run_right));
	
	return e;
}

void player_system(struct world* world, struct renderer* renderer, struct room** room, double ts) {
	struct entity_buffer* to_destroy = new_entity_buffer();

	for (view(world, view,
			type_info(struct transform),
			type_info(struct player),
			type_info(struct animated_sprite))) {
		struct transform* transform = view_get(&view, struct transform);
		struct player* player = view_get(&view, struct player);
		struct animated_sprite* sprite = view_get(&view, struct animated_sprite);

		if (!player->dashing) {
			player->velocity.y += player_constants.gravity * ts;
		}

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

		player->dash_cooldown_timer += ts;
		if (player->items & upgrade_jetpack &&
			!player->on_ground &&
			player->dash_cooldown_timer > player_constants.dash_cooldown &&
			!player->dashing &&
			player->dash_count < player_constants.max_air_dash &&
			key_just_pressed(main_window, mapped_key("dash"))) {
			player->dashing = true;
			player->dash_time = 0.0;
			player->dash_count++;
			player->dash_cooldown_timer = 0.0;

			if (key_pressed(main_window, mapped_key("up"))) {
				player->velocity.y = -player_constants.dash_force;
				player->velocity.x = 0;
				player->dash_dir = make_v2i(0, -1);
			} else if (key_pressed(main_window, mapped_key("down"))) {
				player->velocity.y = player_constants.dash_force;
				player->velocity.x = 0;
				player->dash_dir = make_v2i(0, 1);
			} else if (player->face == player_face_left) {
				player->velocity.x = -player_constants.dash_force;
				player->velocity.y = 0;
				player->dash_dir = make_v2i(-1, 0);
			} else if (player->face == player_face_right) {
				player->velocity.x = player_constants.dash_force;
				player->velocity.y = 0;
				player->dash_dir = make_v2i(1, 0);
			}
		}

		if (player->face == player_face_left) {
			player->collider = player_constants.left_collider;
		} else {
			player->collider = player_constants.right_collider;
		}

		player->dash_time += ts;
		if (player->dashing) {
			player->dash_fx_time += ts;
			if (player->dash_fx_time >= player_constants.dash_fx_interval) {
				player->dash_fx_time = 0.0;
				new_jetpack_particle(world, make_v2i(
					transform->position.x + transform->dimentions.x / 2,
					transform->position.y + transform->dimentions.y / 2));
			}

			if (player->dash_time >= player_constants.max_dash) {
				player->dashing = false;
				if (player->dash_dir.x > 0) {
					player->velocity.x = 0;
				} else if (player->dash_dir.x < 0) {
					player->velocity.x = 0;
				} else if (player->dash_dir.y > 0) {
					player->velocity.y = 0;
				} else if (player->dash_dir.y < 0) {
					player->velocity.y = 0;
				}
			}
		}

		player->position = v2f_add(player->position, v2f_mul(player->velocity, make_v2f(ts, ts)));

		handle_body_collisions(room, player->collider, &player->position, &player->velocity);
		handle_body_transitions(room, player->collider, &player->position);

		struct rect player_rect = {
			(i32)player->position.x + player->collider.x,
			(i32)player->position.y + player->collider.y,
			player->collider.w, player->collider.h
		};
		for (single_view(world, up_view, struct upgrade)) {
			struct upgrade* upgrade = single_view_get(&up_view);

			struct rect up_rect = {
				upgrade->collider.x * sprite_scale,
				upgrade->collider.y * sprite_scale,
				upgrade->collider.w * sprite_scale,
				upgrade->collider.h * sprite_scale,
			};

			if (rect_overlap(player_rect, up_rect, null)) {
				player->items |= upgrade->id;
				entity_buffer_push(to_destroy, up_view.e);
			}
		}

		/* Update pointers because the pools might have been reallocated. */
		transform = view_get(&view, struct transform);
		player = view_get(&view, struct player);
		sprite = view_get(&view, struct animated_sprite);

		{
			struct rect ground_test_rect = {
				player->position.x + player->collider.x + 1,
				player->position.y + player->collider.y + player->collider.h,
				player->collider.w - 2,
				player_constants.ground_hit_range
			};

			v2i normal;
			player->on_ground = rect_room_overlap(*room, ground_test_rect, &normal);
		}

		if (key_just_pressed(main_window, mapped_key("jump")) && player->on_ground) {
			player->velocity.y = player_constants.jump_force;
			player->jump_time = 0.0;
		}

		player->jump_time += ts;
		if (!player->on_ground && key_pressed(main_window, mapped_key("jump")) && player->jump_time < player_constants.max_jump) {
			player->velocity.y += player_constants.jump_force * 5 * ts;
		}

		if (player->on_ground) {
			player->dash_count = 0;

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

	entity_buffer_clear(to_destroy, world);
}
