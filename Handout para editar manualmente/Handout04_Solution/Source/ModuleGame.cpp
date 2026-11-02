#include "Globals.h"
#include "Application.h"
#include "ModuleRender.h"
#include "ModuleGame.h"
#include "ModuleAudio.h"
#include "ModulePhysics.h"

#include <algorithm>

int pelotas = 3;
bool start = true;
bool startball = false;
int score = 0;
int fps = 60;




// Base class for everything in the scene that has a physics body
class PhysicEntity
{
protected:

	PhysicEntity(PhysBody* _body)
		: body(_body)
	{

	}

public:
	virtual ~PhysicEntity() = default;

	// Draw the entity at the position of its body
	virtual void Update() = 0;

public:
	PhysBody* body;
};

class Circle : public PhysicEntity
{
public:
	Circle(ModulePhysics* physics, int _x, int _y, Texture2D _texture)
		: PhysicEntity(physics->CreateCircle(_x, _y, 25))
		, texture(_texture)
	{

	}

	void Update() override
	{
		int x, y;
		body->GetPosition(x, y);

		// Red when the mouse is inside the body
		Color tint = body->Contains(GetMouseX(), GetMouseY()) ? RED : WHITE;

		// Box2D positions are the center of the body: draw the texture around its center
		Rectangle source = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
		Rectangle dest = { (float)x, (float)y, (float)texture.width, (float)texture.height };
		Vector2 origin = { (float)texture.width / 2.0f, (float)texture.height / 2.0f };
		DrawTexturePro(texture, source, dest, origin, body->GetRotation() * RAD2DEG, tint);
		if (IsKeyDown(KEY_E)) body->body->ApplyAngularImpulse(10, true);
		if (IsKeyDown(KEY_Q)) body->body->ApplyAngularImpulse(-10, true);
		if (y < 300) start = false;
		if (IsKeyPressed(KEY_ONE) && body->body->GetGravityScale() > 0) body->body->SetGravityScale(body->body->GetGravityScale() - 1); //default = 10
		if (IsKeyPressed(KEY_TWO) && body->body->GetGravityScale() < 20) body->body->SetGravityScale(body->body->GetGravityScale() + 1);
		int g = body->body->GetGravityScale();
		DrawText(TextFormat("Gravetat: %d", g), 675, 200, 30, DARKGREEN);
		if (IsKeyPressed(KEY_THREE) && fps > 1) fps--, SetTargetFPS(fps);
		if (IsKeyPressed(KEY_FOUR) && fps < 60) fps++, SetTargetFPS(fps);
		DrawText(TextFormat("Target FPS: %d", fps), 675, 250, 30, DARKGREEN);
	}

private:
	Texture2D texture;
};

class Box : public PhysicEntity
{
public:

	void Update() override
	{
		int x, y;
		body->GetPosition(x, y);

		Color tint = body->Contains(GetMouseX(), GetMouseY()) ? RED : WHITE;

		Rectangle source = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
		Rectangle dest = { (float)x, (float)y, (float)texture.width, (float)texture.height };
		Vector2 origin = { (float)texture.width / 2.0f, (float)texture.height / 2.0f };
		DrawTexturePro(texture, source, dest, origin, body->GetRotation() * RAD2DEG, tint);
	}

private:
	Texture2D texture;
};

class Bumper : public PhysicEntity
{
public:
	int t;
	Bumper(ModulePhysics* physics, int _x, int _y, Texture2D _texture)
		: PhysicEntity(physics->CreateBumper(_x, _y, 40))
		, texture(_texture)
	{

	}

	void Update() override
	{
		int x, y;
		body->GetPosition(x, y);
		Color color1 = RED;
		Color color2 = BLACK;
		if (!body->body->IsFixedRotation()) {
			DrawCircle(x, y, 40, color1);
			DrawCircle(x, y, 30, color2);
			t = 8;
		}
		else {
			DrawCircle(x, y, 50, color1);
			DrawCircle(x, y, 40, color2);
			DrawCircle(x, y, 30, color1);
			t--;
		}
		if (t == 0) body->body->SetFixedRotation(false);
	}

private:
	Texture2D texture;
};
class Fliper : public PhysicEntity
{
public:
	Fliper(ModulePhysics* physics, int _x, int _y, bool left)
		: PhysicEntity(physics->CreateFlipper(_x, _y, 80, 10, left))
	{

	}

	void Update() override
	{
		int x, y;
		body->GetPosition(x, y);
		float r = body->GetRotation() * RAD2DEG;
		if (x > 300)
		{
			if (IsKeyDown(KEY_RIGHT)) {
			
				body->body->ApplyAngularImpulse(50000, true);
			}	
		}

		if (x < 300)
		{
			if (IsKeyDown(KEY_LEFT)) {

				body->body->ApplyAngularImpulse(-50000, true);
			}
		}


		DrawRectanglePro({ (float)x, (float)y, 80*2, 10*2 }, {80, 10}, r, RED);
	}

private:
};
class Kicker : public PhysicEntity
{
public:
	float force = 0;
	bool impact = false;
	Kicker(ModulePhysics* physics, int _x, int _y)
		: PhysicEntity(physics->CreateKicker(_x, _y, 30, 10))
	{

	}

	void Update() override
	{
		int x, y;
		body->GetPosition(x, y);

		if (IsKeyDown(KEY_DOWN)) {
			if (y<900)body->body->ApplyForceToCenter({ 0, 300 }, true), force += 2;
			else body->body->SetLinearVelocity({ 0, 0 });
		}
		else body->body->ApplyLinearImpulseToCenter({ 0, -10 * force }, true);
		if (y < 700) body->body->SetLinearVelocity({ 0, 10 }), impact = true, force = 0;
		if (y >= 800 && impact) body->body->SetLinearVelocity({ 0, 0 }), impact = false;
		if (start) DrawRectanglePro({ (float)x, (float)y, 30 * 2, 10 * 2 }, { 30, 10 }, 0, BLUE);
		body->body->SetEnabled(start);

	}

private:
};


class Muro : public PhysicEntity
{
public:
	// Pivot 0, 0
	static constexpr int muro[42] = {
		10, 320,
		10, 727,
		220, 946,
		220, 1100,
		450, 1100,
		450, 946,
		660, 727,
		660, 320,
		649, 250,
		623, 184,
		584, 126,
		533, 77,
		472, 40,
		405, 18,
		335, 10,
		265, 18,
		198, 40,
		137, 77,
		86, 126,
		47, 184,
		21, 250
	};

	Muro(ModulePhysics* physics, int _x, int _y, Texture2D _texture)
		: PhysicEntity(physics->CreateChain(_x, _y, muro, 42))
		, texture(_texture)
	{

	}

	void Update() override
	{
		int x, y;
		body->GetPosition(x, y);

		// Chains have no volume: Contains() is never true for them
		Color tint = body->Contains(GetMouseX(), GetMouseY()) ? RED : WHITE;

		DrawTextureEx(texture, Vector2{ (float)x, (float)y }, body->GetRotation() * RAD2DEG, 1.0f, tint);
		
	}

private:
	Texture2D texture;
};
class Muro2 : public PhysicEntity
{
public:
	// Pivot 0, 0
	static constexpr int st[8] = {
		580, 300,
		600, 300,
		600, 820,
		580, 820
	};

	Muro2(ModulePhysics* physics, int _x, int _y, Texture2D _texture)
		: PhysicEntity(physics->CreateChain(_x, _y, st, 8))
		, texture(_texture)
	{

	}

	void Update() override
	{
		int x, y;
		body->GetPosition(x, y);

		// Chains have no volume: Contains() is never true for them
		Color tint = body->Contains(GetMouseX(), GetMouseY()) ? RED : WHITE;

		DrawTextureEx(texture, Vector2{ (float)x, (float)y }, body->GetRotation() * RAD2DEG, 1.0f, tint);
		if (start) DrawRectangle(580, 300, 20, 520, BLACK); 		
		body->body->SetEnabled(start);

	}

private:
	Texture2D texture;
};

ModuleGame::ModuleGame(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	ray_on = false;
	ray.x = ray.y = 0;
	sensor = NULL;
}

ModuleGame::~ModuleGame()
{}

// Load assets
bool ModuleGame::Start()
{
	LOG("Loading Intro assets");
	bool ret = true;

	App->renderer->camera.x = App->renderer->camera.y = 0;

	BackG = LoadTexture("Assets/steel_ballground.jpeg");
	circle = LoadTexture("Assets/wheel.png");
	box = LoadTexture("Assets/crate.png");
	rick = LoadTexture("Assets/rick_head.png");
	Music smusic = LoadMusicStream("STEEL_BALL_RUN_THEME.mp3");
	Sound music = LoadSound("STEEL_BALL_RUN_THEME.mp3");
	
	bonus_fx = App->audio->LoadFx("Assets/bonus.wav");

	// Sensor at the bottom of the screen
	sensor = App->physics->CreateRectangleSensor(SCREEN_WIDTH , SCREEN_HEIGHT*1.5, SCREEN_WIDTH , 50);
	sensor->listener = this;
	//PhysBody* leftFlipper = App->physics->CreateFlipper(225, 915, 93, 10, true);
	//PhysBody* rightFlipper = App->physics->CreateFlipper(455, 915, 93, 10, false);



	entities.push_back(new Muro(App->physics, 0, 0, rick));
	entities.push_back(new Muro2(App->physics, 0, 0, rick));
	entities.push_back(new Fliper(App->physics, 240, 915, true));
	entities.push_back(new Fliper(App->physics, 440, 915, false));
	entities.push_back(new Kicker(App->physics, 630, 800));
	entities.push_back(new Bumper(App->physics, 150, 250, BackG));
	entities.push_back(new Bumper(App->physics, 450, 600, BackG));
	entities.push_back(new Bumper(App->physics, 550, 250, BackG));
	entities.push_back(new Bumper(App->physics, 190, 650, BackG));

	PlaySound(music);

	return ret;
}

// Unload assets
bool ModuleGame::CleanUp()
{
	LOG("Unloading Intro scene");

	DestroyAllEntities();

	App->physics->DestroyBody(sensor);
	sensor = NULL;

	UnloadTexture(BackG);
	UnloadTexture(circle);
	UnloadTexture(box);
	UnloadTexture(rick);
	

	return true;
}

// Update: draw background
update_status ModuleGame::Update()
{
	// Destroy the bodies that touched the sensor
	for (PhysBody* body : bodies_to_destroy)
	{
		for (PhysicEntity* entity : entities)
		{
			if (entity->body == body)
			{
				DestroyEntity(entity);
				break;
			}
		}
	}
	bodies_to_destroy.clear();

	if (IsKeyPressed(KEY_SPACE))
	{
		ray_on = !ray_on;
		ray.x = GetMouseX();
		ray.y = GetMouseY();
	}

	if (start && pelotas>0 && !startball)
	{
		Circle* c = new Circle(App->physics, 620, 700, circle);
		c->body->listener = this;
		entities.push_back(c);
		startball = true;
	}

	if (IsKeyPressed(KEY_BACKSPACE))
	{
		DestroyAllEntities();
	}

	// Prepare for raycast ------------------------------------------------------

	vec2i mouse;
	mouse.x = GetMouseX();
	mouse.y = GetMouseY();
	int ray_hit = ray.DistanceTo(mouse);

	vec2f normal(0.0f, 0.0f);






	// All draw functions ------------------------------------------------------
	ClearBackground(BLACK);
	DrawRectangle(0, 0, 1000, 1000, BLACK);
	DrawRectangle(0, 10, 990, 980, DARKGRAY);
	DrawTexture(rick, 0, 0, WHITE);
	

	for (PhysicEntity* entity : entities)
	{
		entity->Update();

		// Keep the closest hit
		if (ray_on)
		{
			vec2f hit_normal(0.0f, 0.0f);
			int hit = entity->body->RayCast(ray.x, ray.y, mouse.x, mouse.y, hit_normal.x, hit_normal.y);
			if (hit >= 0 && hit < ray_hit)
			{
				ray_hit = hit;
				normal = hit_normal;
			}
		}
	}
	
	DrawText(TextFormat("Bolas restantes: %d", pelotas), 675, 100, 30, RED);
	DrawText(TextFormat("Puntuacion: %d", score), 675, 150, 30, DARKGREEN);

	if (pelotas <= 0)
	{
		DrawRectangle(0, 0, 1000, 1000, BLACK);
		DrawText("GAME OVER", 250, 400, 80, RED);
		DrawText("Presiona enter para volver a empezar", 60, 500, 45, WHITE);

		if (IsKeyPressed(KEY_ENTER))
		{
			pelotas = 3;
			score = 0;
		}
	}
	

	// ray -----------------
	if (ray_on == true)
	{
		vec2f destination((float)(mouse.x - ray.x), (float)(mouse.y - ray.y));
		destination.Normalize();
		destination *= (float)ray_hit;

		DrawLine(ray.x, ray.y, (int)(ray.x + destination.x), (int)(ray.y + destination.y), RED);

		if (normal.x != 0.0f || normal.y != 0.0f)
		{
			DrawLine((int)(ray.x + destination.x), (int)(ray.y + destination.y), (int)(ray.x + destination.x + normal.x * 25.0f), (int)(ray.y + destination.y + normal.y * 25.0f), Color{ 100, 255, 100, 255 });
		}
	}

	

	return UPDATE_CONTINUE;
}





// Circles play bonus_fx when they collide
void ModuleGame::OnCollision(PhysBody* bodyA, PhysBody* bodyB)
{
	// Bodies that touch the sensor are destroyed
	if (bodyA == sensor)
	{
		// Never destroy bodies inside a collision callback: the world is locked
		// Keep them in a list and destroy them in Update()
		if (bodyB != NULL && std::find(bodies_to_destroy.begin(), bodies_to_destroy.end(), bodyB) == bodies_to_destroy.end()) 
		{

			bodies_to_destroy.push_back(bodyB);
			--pelotas;
			start = true;
			startball = false;
			return;

		}
		
	}
	b2Vec2 boom = bodyA->body->GetPosition() - bodyB->body->GetPosition();
	boom.Normalize();
	if (bodyB->body->GetGravityScale() == 69)
	{
		bodyA->body->SetLinearVelocity(40 * boom);
		//bodyA->body->SetLinearVelocity({ 0, 0 });
		//bodyA->body->ApplyLinearImpulseToCenter(30*boom, true);
		score += 100;
		bodyB->body->SetFixedRotation(true);
	}

	if (bodyB == sensor) return;

	App->audio->PlayFx(bonus_fx);
}






void ModuleGame::DestroyEntity(PhysicEntity* entity)
{
	entities.erase(std::find(entities.begin(), entities.end(), entity));
	App->physics->DestroyBody(entity->body);
	delete entity;
}

void ModuleGame::DestroyAllEntities()
{
	for (PhysicEntity* entity : entities)
	{
		App->physics->DestroyBody(entity->body);
		delete entity;
	}
	entities.clear();
	bodies_to_destroy.clear();
}