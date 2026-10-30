#include "Globals.h"
#include "Application.h"
#include "ModuleRender.h"
#include "ModuleGame.h"
#include "ModuleAudio.h"
#include "ModulePhysics.h"

#include <algorithm>

int pelotas = 3;




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
	}

private:
	Texture2D texture;
};

class Box : public PhysicEntity
{
public:
	Box(ModulePhysics* physics, int _x, int _y, Texture2D _texture)
		: PhysicEntity(physics->CreateRectangle(_x, _y, 50, 25))
		, texture(_texture)
	{

	}

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

class Rick : public PhysicEntity
{
public:
	// Pivot 0, 0
	static constexpr int rick_head[44] = {
		213, 461,
		262, 461,
		280, 447,
		295, 428,
		301, 412,
		304, 400,
		304, 66,
		297, 44,
		290, 28,
		280, 19,
		261, 8,
		47, 8,
		30, 19,
		15, 37,
		5, 59,
		5, 399,
		9, 418,
		16, 433,
		23, 446,
		38, 457,
		50, 461,
		89, 461
	};

	Rick(ModulePhysics* physics, int _x, int _y, Texture2D _texture)
		: PhysicEntity(physics->CreateChain(_x, _y, rick_head, 44))
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

	circle = LoadTexture("Assets/wheel.png");
	box = LoadTexture("Assets/crate.png");
	rick = LoadTexture("Assets/rick_head.png");

	bonus_fx = App->audio->LoadFx("Assets/bonus.wav");

	// Sensor at the bottom of the screen
	sensor = App->physics->CreateRectangleSensor(SCREEN_WIDTH / 2, SCREEN_HEIGHT, SCREEN_WIDTH / 2, 25);
	sensor->listener = this;

	return ret;
}

// Unload assets
bool ModuleGame::CleanUp()
{
	LOG("Unloading Intro scene");

	DestroyAllEntities();

	App->physics->DestroyBody(sensor);
	sensor = NULL;

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

	if (IsKeyPressed(KEY_ONE))
	{
		Circle* c = new Circle(App->physics, GetMouseX(), GetMouseY(), circle);
		c->body->listener = this;
		entities.push_back(c);
	}

	if (IsKeyPressed(KEY_TWO))
	{
		entities.push_back(new Box(App->physics, GetMouseX(), GetMouseY(), box));
	}

	if (IsKeyPressed(KEY_FOUR))
	{
		entities.push_back(new Rick(App->physics, GetMouseX(), GetMouseY(), rick));
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

	DrawText(TextFormat("Bolas restantes: %d", pelotas), 60, 100, 40, RED);

	if (pelotas == 0)
	{
		DrawText("GAME OVER", 60, 100, 60, RED);


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
			return;

		}
		
	}

	if (bodyB == sensor)
		return;

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
