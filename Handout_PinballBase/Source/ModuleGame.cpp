#include "Globals.h"
#include "Application.h"
#include "ModuleRender.h"
#include "ModuleGame.h"
#include "ModuleAudio.h"
#include "ModulePhysics.h"

#include <algorithm>
#include <math.h>
using namespace std;

Texture2D steelball = LoadTexture("Steel_Ball.png");

namespace
{
	const int   START_BALLS = 3;
	const int   EXTRA_BALL_EVERY = 10000;   // score needed for each extra ball
	const float BUMPER_SPEED = 9.0f;        // m/s the ball leaves a bumper with
	const float SLING_KICK = 6.0f;          // m/s added by a slingshot
	const float CHARGE_TIME = 1.0f;         // seconds to fully charge the plunger
	const float PWU = 0.92f;                // right edge of the playfield (the rest is the launch lane)
	const float PI_F = 3.14159265f;
	const int   ARC_SEGMENTS = 32;

	// Mirror a horizontal position of the playfield
	float Mirror(float u) { return PWU - u; }
}

ModuleGame::ModuleGame(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	W = H = X0 = 0.0f;
	ball_radius = 0;
	lane_left_x = lane_center_x = lane_bottom_y = 0;
	flipper_length = flipper_base = flipper_tip = 0;

	ball = NULL;
	drain = NULL;
	flipper_left = NULL;
	flipper_right = NULL;

	state = STATE_PLAYING;
	score = 0;
	balls_left = 0;
	next_extra_ball_score = EXTRA_BALL_EVERY;
	ball_drained = false;
	charge = 0.0f;
	launch_top_speed = 0.0f;
	message_timer = 0.0f;
	bonus_fx = 0;
}

ModuleGame::~ModuleGame()
{}

// Load assets
bool ModuleGame::Start()
{
	LOG("Loading Pinball");

	App->renderer->camera.x = App->renderer->camera.y = 0;

	bonus_fx = App->audio->LoadFx("Assets/bonus.wav");

	BuildTable();
	NewGame();

	return true;
}

// Unload assets
bool ModuleGame::CleanUp()
{
	LOG("Unloading Pinball");

	DestroyBall();

	App->physics->DestroyBody(drain);
	App->physics->DestroyBody(flipper_left);
	App->physics->DestroyBody(flipper_right);
	drain = flipper_left = flipper_right = NULL;

	for (PhysBody* w : walls)
		App->physics->DestroyBody(w);
	for (Bumper& b : bumpers)
		App->physics->DestroyBody(b.body);
	for (Slingshot& s : slingshots)
		App->physics->DestroyBody(s.body);
	for (Rollover& r : rollovers)
		App->physics->DestroyBody(r.body);

	walls.clear();
	wall_lines.clear();
	bumpers.clear();
	slingshots.clear();
	rollovers.clear();

	return true;
}

// Table construction ------------------------------------------------------------------

void ModuleGame::BuildTable()
{
	// The table is a vertical rectangle centered on the screen. Everything is placed
	// with fractions of its size, so it works with any SCREEN_WIDTH / SCREEN_HEIGHT
	H = (float)SCREEN_HEIGHT;
	W = std::min((float)SCREEN_WIDTH, H * 0.7f);
	X0 = ((float)SCREEN_WIDTH - W) * 0.5f;

	ball_radius = std::max(5, (int)(H * 0.012f));

	const float R = W * 0.5f;           // radius of the top arch
	const float cx = X0 + R;
	const float cy = H * 0.01f + R;

	lane_left_x = TX(PWU);
	lane_center_x = (lane_left_x + TX(1.0f)) / 2;
	lane_bottom_y = TY(0.97f);

	flipper_length = (int)(W * 0.16f);
	flipper_base = std::max(3, (int)(W * 0.016f));
	flipper_tip = std::max(2, (int)(W * 0.009f));

	const int pivot_y = TY(0.90f);
	const int pivot_left_x = TX(0.30f);
	const int pivot_right_x = TX(Mirror(0.30f));

	// Speed needed to go from the lane to the top of the table
	launch_top_speed = sqrtf(2.0f * fabsf(GRAVITY_Y) * PIXEL_TO_METERS(lane_bottom_y));

	// --- Outer wall: left funnel -> left wall -> top arch -> right wall -> launch lane -> divider
	{
		std::vector<int> pts;
		auto add = [&pts](int x, int y) { pts.push_back(x); pts.push_back(y); };

		add(pivot_left_x, pivot_y - flipper_base);   // end of the left funnel, at the flipper
		add(TX(0.0f), TY(0.78f));
		add(TX(0.0f), (int)cy);

		for (int i = 1; i <= ARC_SEGMENTS; ++i)
		{
			float a = PI_F + PI_F * (float)i / (float)ARC_SEGMENTS;
			add((int)(cx + R * cosf(a)), (int)(cy + R * sinf(a)));
		}

		add(TX(1.0f), lane_bottom_y);
		add(lane_left_x, lane_bottom_y);
		add(lane_left_x, TY(0.30f));                 // top of the divider between lane and playfield

		AddWall(pts);
	}

	// --- Right funnel: from the divider to the right flipper
	{
		std::vector<int> pts;
		pts.push_back(lane_left_x);      pts.push_back(TY(0.78f));
		pts.push_back(pivot_right_x);    pts.push_back(pivot_y - flipper_base);
		AddWall(pts);
	}

	// --- Flippers
	flipper_left = App->physics->CreateFlipper(pivot_left_x, pivot_y, flipper_length, flipper_base, flipper_tip, +1, 30.0f, -30.0f);
	flipper_right = App->physics->CreateFlipper(pivot_right_x, pivot_y, flipper_length, flipper_base, flipper_tip, -1, -30.0f, 30.0f);

	// --- Bumpers
	AddBumper(0.30f, 0.28f);
	AddBumper(Mirror(0.30f), 0.28f);
	AddBumper(0.46f, 0.40f);

	// --- Slingshots (triangles above the flippers that kick the ball)
	AddSlingshot(0.08f, 0.63f, 0.08f, 0.735f, 0.235f, 0.79f);
	AddSlingshot(Mirror(0.08f), 0.63f, Mirror(0.08f), 0.735f, Mirror(0.235f), 0.79f);

	// --- Rollovers at the top: light all three to get an extra ball
	AddRollover(0.30f, 0.11f);
	AddRollover(0.46f, 0.11f);
	AddRollover(Mirror(0.30f), 0.11f);

	// --- Drain sensor below the screen
	drain = App->physics->CreateRectangleSensor(SCREEN_WIDTH / 2, SCREEN_HEIGHT + 40, SCREEN_WIDTH, 30);
}

void ModuleGame::AddWall(const std::vector<int>& points)
{
	PhysBody* wall = App->physics->CreateOpenChain(0, 0, points.data(), (int)points.size(), 0.2f);
	walls.push_back(wall);

	std::vector<Vector2> line;
	for (size_t i = 0; i + 1 < points.size(); i += 2)
		line.push_back(Vector2{ (float)points[i], (float)points[i + 1] });
	wall_lines.push_back(line);
}

void ModuleGame::AddBumper(float u, float v)
{
	Bumper b;
	b.x = TX(u);
	b.y = TY(v);
	b.radius = (int)(W * 0.05f);
	b.flash = 0;
	b.body = App->physics->CreateCircle(b.x, b.y, b.radius, b2_staticBody, 0.5f);
	bumpers.push_back(b);
}

void ModuleGame::AddSlingshot(float ua, float va, float ub, float vb, float uc, float vc)
{
	Slingshot s;
	s.p[0] = Vector2{ (float)TX(ua), (float)TY(va) };
	s.p[1] = Vector2{ (float)TX(ub), (float)TY(vb) };
	s.p[2] = Vector2{ (float)TX(uc), (float)TY(vc) };

	int pts[6] = {
		(int)s.p[0].x, (int)s.p[0].y,
		(int)s.p[1].x, (int)s.p[1].y,
		(int)s.p[2].x, (int)s.p[2].y
	};
	s.body = App->physics->CreatePolygon(0, 0, pts, 6, b2_staticBody, 0.3f);

	// The kick face is p0 -> p2. The normal points away from p1 (outside of the triangle)
	float dx = s.p[2].x - s.p[0].x;
	float dy = s.p[2].y - s.p[0].y;
	s.length = sqrtf(dx * dx + dy * dy);
	s.dir = Vector2{ dx / s.length, dy / s.length };
	s.normal = Vector2{ s.dir.y, -s.dir.x };

	float inside = (s.p[1].x - s.p[0].x) * s.normal.x + (s.p[1].y - s.p[0].y) * s.normal.y;
	if (inside > 0.0f)
	{
		s.normal.x = -s.normal.x;
		s.normal.y = -s.normal.y;
	}

	s.flash = 0;
	slingshots.push_back(s);
}

void ModuleGame::AddRollover(float u, float v)
{
	Rollover r;
	r.x = TX(u);
	r.y = TY(v);
	r.radius = (int)(W * 0.03f);
	r.lit = false;
	r.body = App->physics->CreateCircleSensor(r.x, r.y, r.radius);
	rollovers.push_back(r);
}

// Game flow ---------------------------------------------------------------------------

void ModuleGame::NewGame()
{
	DestroyBall();

	score = 0;
	balls_left = START_BALLS;
	next_extra_ball_score = EXTRA_BALL_EVERY;
	state = STATE_PLAYING;
	ball_drained = false;
	message_timer = 0.0f;

	for (Rollover& r : rollovers)
		r.lit = false;

	SpawnBall();
}

void ModuleGame::SpawnBall()
{
	ball = App->physics->CreateCircle(lane_center_x, lane_bottom_y - ball_radius - 1, ball_radius, b2_dynamicBody, 0.35f);
	ball->listener = this;
	ball->body->SetBullet(true);            // avoids going through walls at high speed
	ball->body->SetSleepingAllowed(false);
	charge = 0.0f;
}

void ModuleGame::DestroyBall()
{
	if (ball != NULL)
	{
		App->physics->DestroyBody(ball);
		ball = NULL;
	}
}

void ModuleGame::AddScore(int points)
{
	score += points;

	// A new ball every EXTRA_BALL_EVERY points
	while (score >= next_extra_ball_score)
	{
		next_extra_ball_score += EXTRA_BALL_EVERY;
		++balls_left;
		ShowMessage("EXTRA BALL!");
		App->audio->PlayFx(bonus_fx);
	}
}

void ModuleGame::ShowMessage(const char* text)
{
	message = text;
	message_timer = 2.0f;
}

// Hold DOWN / SPACE to charge the plunger, release to launch the ball
void ModuleGame::UpdateLauncher(float dt)
{
	if (ball == NULL)
		return;

	int bx, by;
	ball->GetPosition(bx, by);

	bool in_lane = bx > lane_left_x && by > TY(0.5f);
	if (!in_lane)
	{
		charge = 0.0f;
		return;
	}

	if (IsKeyDown(KEY_DOWN))
	{
		charge = min(1.0f, charge + dt / CHARGE_TIME);
	}
	else if (charge > 0.0f)
	{
		float speed = launch_top_speed * (0.65f + 0.55f * charge);
		ball->SetVelocity(0.0f, -speed); // Y grows downwards: negative is up
		charge = 0.0f;
	}
}

// Collisions: only the ball has a listener, so bodyA is always the ball -------------------
void ModuleGame::OnCollision(PhysBody* bodyA, PhysBody* bodyB)
{
	if (bodyA != ball || bodyB == NULL)
		return;

	// Never destroy or create bodies here (the world is locked): just set a flag
	if (bodyB == drain)
	{
		ball_drained = true;
		return;
	}

	int bx, by;
	ball->GetPosition(bx, by);

	for (Bumper& b : bumpers)
	{
		if (b.body == bodyB)
		{
			// Push the ball away from the center of the bumper
			float dx = (float)(bx - b.x);
			float dy = (float)(by - b.y);
			float len = sqrtf(dx * dx + dy * dy);
			if (len > 0.001f)
				ball->SetVelocity(dx / len * BUMPER_SPEED, dy / len * BUMPER_SPEED);

			b.flash = 8;
			AddScore(100);
			App->audio->PlayFx(bonus_fx);
			return;
		}
	}

	for (Slingshot& s : slingshots)
	{
		if (s.body == bodyB)
		{
			// Only kick if the ball touches the kick face (not the other sides of the triangle)
			float rx = (float)bx - s.p[0].x;
			float ry = (float)by - s.p[0].y;
			float side = rx * s.normal.x + ry * s.normal.y;
			float along = rx * s.dir.x + ry * s.dir.y;

			if (side > 0.0f && along >= 0.0f && along <= s.length)
			{
				ball->AddVelocity(s.normal.x * SLING_KICK, s.normal.y * SLING_KICK);
				s.flash = 6;
				AddScore(10);
			}
			return;
		}
	}

	for (Rollover& r : rollovers)
	{
		if (r.body == bodyB)
		{
			if (!r.lit)
			{
				r.lit = true;
				AddScore(50);

				bool all_lit = true;
				for (Rollover& other : rollovers)
					all_lit = all_lit && other.lit;

				// All three lit: extra ball
				if (all_lit)
				{
					for (Rollover& other : rollovers)
						other.lit = false;

					++balls_left;
					AddScore(500);
					ShowMessage("EXTRA BALL!");
					App->audio->PlayFx(bonus_fx);
				}
			}
			return;
		}
	}
}

// Update ------------------------------------------------------------------------------

update_status ModuleGame::Update()
{
	float dt = GetFrameTime();

	// Flippers work even in game over, it is more fun
	flipper_left->Flip(IsKeyDown(KEY_LEFT));
	flipper_right->Flip(IsKeyDown(KEY_RIGHT));

	if (IsKeyDown(KEY_E)) ball->body->ApplyAngularImpulse(10, true);
	if (IsKeyDown(KEY_Q)) ball->body->ApplyAngularImpulse(-10, true);
	if (state == STATE_GAME_OVER)
	{
		if (IsKeyPressed(KEY_ENTER))
			NewGame();
	}
	else
	{
		UpdateLauncher(dt);

		// Safety net: a ball that somehow went far below the table is a drained ball
		if (ball != NULL)
		{
			int bx, by;
			ball->GetPosition(bx, by);
			if (by > SCREEN_HEIGHT + 100)
				ball_drained = true;
		}

		if (ball_drained)
		{
			ball_drained = false;
			DestroyBall();
			--balls_left;

			if (balls_left > 0)
				SpawnBall();
			else
				state = STATE_GAME_OVER;
		}
	}

	if (message_timer > 0.0f)
		message_timer -= dt;

	Draw();

	return UPDATE_CONTINUE;
}

// Drawing: everything is made with raylib primitives, no textures needed ------------------

void ModuleGame::Draw()
{
	const Color wall_color = Color{ 120, 200, 255, 255 };

	// Esto es lo mejor que se me ha ocurrido para pintar el césped.
	DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,GREEN);
	DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT / 8, DARKGREEN);
	DrawRectangle(0, 150, SCREEN_WIDTH, SCREEN_HEIGHT / 8, DARKGREEN);
	DrawRectangle(0, 300, SCREEN_WIDTH, SCREEN_HEIGHT / 8, DARKGREEN);
	DrawRectangle(0, 450, SCREEN_WIDTH, SCREEN_HEIGHT / 8, DARKGREEN);
	DrawRectangle(0, 600, SCREEN_WIDTH, SCREEN_HEIGHT / 8, DARKGREEN);

	// Walls
	for (const std::vector<Vector2>& line : wall_lines)
	{
		for (size_t i = 0; i + 1 < line.size(); ++i)
			DrawLineEx(line[i], line[i + 1], 4.0f, wall_color);
	}

	// Slingshots
	for (Slingshot& s : slingshots)
	{
		Color c = s.flash > 0 ? YELLOW : RED;
		DrawTriangle(s.p[0], s.p[1], s.p[2], c);
		DrawTriangle(s.p[0], s.p[2], s.p[1], c); // the other winding order, raylib culls one of them
		if (s.flash > 0) --s.flash;
	}

	// Bumpers
	for (Bumper& b : bumpers)
	{
		DrawCircle(b.x, b.y, (float)b.radius, b.flash > 0 ? YELLOW : GREEN);
		DrawCircle(b.x, b.y, b.radius * 0.6f, b.flash > 0 ? WHITE : BLUE);
		DrawCircleLines(b.x, b.y, (float)b.radius, WHITE);
		if (b.flash > 0) --b.flash;
	}

	// Rollovers
	for (const Rollover& r : rollovers)
	{
		DrawCircle(r.x, r.y, (float)r.radius, r.lit ? Color{ 80, 255, 120, 255 } : Color{ 50, 60, 90, 255 });
		DrawCircleLines(r.x, r.y, (float)r.radius, WHITE);
	}



	// Flippers
	PhysBody* flippers[2] = { flipper_left, flipper_right };
	float directions[2] = { 1.0f, -1.0f };
	for (int i = 0; i < 2; ++i)
	{
		int px, py;
		flippers[i]->GetPosition(px, py);
		float a = flippers[i]->GetRotation();
		Vector2 pivot = { (float)px, (float)py };
		Vector2 tip = { pivot.x + directions[i] * flipper_length * cosf(a), pivot.y + directions[i] * flipper_length * sinf(a) };

		DrawLineEx(pivot, tip, (float)(flipper_base + flipper_tip), BLACK);
		DrawCircle((int)pivot.x, (int)pivot.y, (float)flipper_base, BLACK);
		DrawCircle((int)tip.x, (int)tip.y, (float)flipper_tip, BLACK);
	}

	//Función para que lancemos la pelota para arriba la primera vez que le damos.
	int lane_w = TX(1.0f) - lane_left_x;
	int travel = (int)H - lane_bottom_y - 8;
	int knob_y = lane_bottom_y + (int)(charge * travel);
	DrawRectangle(lane_left_x + 4, knob_y, lane_w - 8, 8, RED);

	// Ball
	if (ball != NULL)
	{
		int ballx, bally;
		float br;
		br = ball->GetRotation();
		ball->GetPosition(ballx, bally);
		DrawCircle(ballx, bally, (float)ball_radius - 0.2, GREEN);
		DrawCircleLines(ballx, bally, (float)ball_radius, DARKGREEN);
		DrawCircleSectorLines({ (float)ballx, (float)bally }, (float)ball_radius, br, br+90, 2, DARKGREEN);
		DrawCircleSectorLines({ (float)ballx, (float)bally }, (float)ball_radius, br-90, br-180, 2, DARKGREEN);
	}

	DrawText(TextFormat("SCORE  %d", score), 1100, 20, 28, YELLOW);

	// Messages
	if (message_timer > 0.0f)
	{
		int w = MeasureText(message.c_str(), 36);
		DrawText(message.c_str(), (int)(X0 + W / 2) - w / 2, TY(0.5f), 36, GREEN);
	}

	if (state == STATE_GAME_OVER)
	{

		DrawText(TextFormat("GAME OVER"), SCREEN_WIDTH/3.5, SCREEN_HEIGHT/2.5, 100, RED);
		DrawText(TextFormat("Enter para volver a empezar"), SCREEN_WIDTH / 4.5, SCREEN_HEIGHT /1.75, 50, WHITE);

	}
}
