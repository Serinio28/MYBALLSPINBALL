#pragma once

#include "Globals.h"
#include "Module.h"

#include "p2Point.h"

#include "raylib.h"
#include <vector>
#include <string>
using namespace std;
class PhysBody;

struct Bumper
{
	PhysBody* body;
	int x, y, radius;
	int flash;
};

struct Slingshot
{
	PhysBody* body;
	Vector2 p[3];      // triangle. The kick face goes from p[0] to p[2]
	Vector2 dir;       // unit vector of the kick face
	Vector2 normal;    // unit normal of the kick face, pointing to the playfield
	float length;
	int flash;
};

struct Rollover
{
	PhysBody* body;
	int x, y, radius;
	bool tocado;
};

enum GameState
{
	STATE_PLAYING,
	STATE_GAME_OVER
};

class ModuleGame : public Module
{
public:
	ModuleGame(Application* app, bool start_enabled = true);
	~ModuleGame();

	bool Start();
	update_status Update();
	bool CleanUp();

	void OnCollision(PhysBody* bodyA, PhysBody* bodyB) override;

private:

	// Table layout: u = 0..1 across the table width, v = 0..1 across the screen height
	int TX(float u) const { return (int)(X0 + u * W); }
	int TY(float v) const { return (int)(v * H); }

	void BuildTable();
	void AddWall(const std::vector<int>& points);
	void AddBumper(float u, float v);
	void AddSlingshot(float ua, float va, float ub, float vb, float uc, float vc);
	void AddRollover(float u, float v);

	void NewGame();
	void SpawnBall();
	void DestroyBall();
	void UpdateLauncher(float dt);
	void AddScore(int points);
	void ShowMessage(const char* text);
	void Draw();

public:

	uint32 bonus_fx;

private:

	// Table geometry (pixels)
	float W, H, X0;
	int ball_radius;
	int lane_left_x, lane_center_x, lane_bottom_y;
	int flipper_length, flipper_base, flipper_tip;

	// Physics objects
	PhysBody* ball;
	PhysBody* drain;
	PhysBody* flipper_left;
	PhysBody* flipper_right;
	vector<PhysBody*> walls;
	vector<vector<Vector2>> wall_lines; //Es el vector de arriba pero sirve para dibujar.
	vector<Bumper> bumpers;
	vector<Slingshot> slingshots;
	vector<Rollover> rollovers;

	// Game state
	GameState state;
	int score;
	int balls_left;
	int next_extra_ball_score;
	bool ball_drained;
	float charge;          // plunger 0..1
	float launch_top_speed; // speed needed to reach the top of the table
	string message;
	float message_timer;
};
