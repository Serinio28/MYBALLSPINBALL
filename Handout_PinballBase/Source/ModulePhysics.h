#pragma once

#include "Module.h"
#include "Globals.h"

#include "box2d/box2d.h"

// Gravity in m/s^2. Screen Y grows DOWNWARDS and we convert meters <-> pixels without flipping Y,
// so a POSITIVE Y gravity makes bodies fall down on screen. That is why the world receives -GRAVITY_Y.
extern float GRAVITY_X;
extern float GRAVITY_Y;

// Units: Box2D works in meters (float), raylib draws in pixels (int)
#define PIXELS_PER_METER 50.0f // if touched change METER_PER_PIXEL too
#define METER_PER_PIXEL 0.02f  // this is 1 / PIXELS_PER_METER !

#define METERS_TO_PIXELS(m) ((int) floor(PIXELS_PER_METER * (m)))
#define PIXEL_TO_METERS(p)  ((float) METER_PER_PIXEL * (p))

// Small class to return to other modules to track position and rotation of physics bodies
class PhysBody
{
public:
	PhysBody() : body(NULL), listener(NULL), joint(NULL), flip_speed(0.0f)
	{}

	void GetPosition(int& x, int& y) const;
	float GetRotation() const;
	bool Contains(int x, int y) const;
	int RayCast(int x1, int y1, int x2, int y2, float& normal_x, float& normal_y) const;

	// Velocities in m/s (Y grows downwards)
	void SetVelocity(float vx, float vy);
	void AddVelocity(float vx, float vy);

	// Only for flippers: true = swing up, false = go back to rest
	void Flip(bool active);

public:
	b2Body* body;
	// Module that listens to the collisions of this body
	Module* listener;

	// Flipper data (NULL / 0 for the rest of bodies)
	b2RevoluteJoint* joint;
	float flip_speed;
};

// Module --------------------------------------
class ModulePhysics : public Module, public b2ContactListener
{
public:
	ModulePhysics(Application* app, bool start_enabled = true);
	~ModulePhysics();

	bool Start();
	update_status PreUpdate();
	update_status PostUpdate();
	bool CleanUp();

	PhysBody* CreateCircle(int x, int y, int radius, b2BodyType type = b2_dynamicBody, float restitution = 0.0f);
	PhysBody* CreateRectangle(int x, int y, int half_width, int half_height);
	PhysBody* CreateRectangleSensor(int x, int y, int half_width, int half_height);
	PhysBody* CreateCircleSensor(int x, int y, int radius);

	// Closed static loop (original function)
	PhysBody* CreateChain(int x, int y, const int* points, int size);
	// Open static chain of segments: walls and guides
	PhysBody* CreateOpenChain(int x, int y, const int* points, int size, float restitution = 0.0f);
	// Convex polygon (max 8 vertices) with points relative to x,y. size = number of ints
	PhysBody* CreatePolygon(int x, int y, const int* points, int size, b2BodyType type = b2_staticBody, float restitution = 0.0f);

	// Flipper with a pivot at x,y. direction: +1 points to the right, -1 to the left
	// Angles in degrees (screen angles: positive = clockwise on screen)
	PhysBody* CreateFlipper(int x, int y, int length, int base_half, int tip_half, int direction, float rest_deg, float active_deg);

	void DestroyBody(PhysBody* pbody);

	void BeginContact(b2Contact* contact) override;

private:

	bool debug;
	b2World* world;

	// Static body with no fixtures, needed to create joints like the mouse joint
	b2Body* ground;
	b2MouseJoint* mouse_joint;
};
