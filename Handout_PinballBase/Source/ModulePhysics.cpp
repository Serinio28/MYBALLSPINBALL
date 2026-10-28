#include "Globals.h"
#include "Application.h"
#include "ModuleRender.h"
#include "ModulePhysics.h"

#include "p2Point.h"

#include <math.h>

static const float FLIPPER_SPEED = 8.0f;        // rad/s
static const float FLIPPER_TORQUE = 8000000.0f;
static const float DEG_TO_RAD_F = 3.14159265f / 180.0f;
float GRAVITY_X = 0.0f;
float GRAVITY_Y = -10.0f;

ModulePhysics::ModulePhysics(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	world = NULL;
	ground = NULL;
	mouse_joint = NULL;
	debug = false; // F1 to see the physics shapes
}

// Destructor
ModulePhysics::~ModulePhysics()
{
}

bool ModulePhysics::Start()
{
	LOG("Creating Physics 2D environment");

	world = new b2World(b2Vec2(GRAVITY_X, -GRAVITY_Y));

	world->SetContactListener(this);

	// Needed to create joints (mouse joint, flippers)
	b2BodyDef bd;
	ground = world->CreateBody(&bd);

	return true;
}

update_status ModulePhysics::PreUpdate()
{
	world->Step(1.0f / 60.0f, 8, 3);

	return UPDATE_CONTINUE;
}

update_status ModulePhysics::PostUpdate()
{
	if (IsKeyPressed(KEY_F1))
	{
		debug = !debug;
	}

	if (!debug)
	{
		// The mouse joint only lives in debug mode
		if (mouse_joint != NULL)
		{
			world->DestroyJoint(mouse_joint);
			mouse_joint = NULL;
		}
		return UPDATE_CONTINUE;
	}

	b2Vec2 mouse_position(PIXEL_TO_METERS(GetMouseX()), PIXEL_TO_METERS(GetMouseY()));
	b2Body* body_clicked = NULL;

	for (b2Body* b = world->GetBodyList(); b; b = b->GetNext())
	{
		for(b2Fixture* f = b->GetFixtureList(); f; f = f->GetNext())
		{
			switch(f->GetType())
			{
				// Draw circles ------------------------------------------------
				case b2Shape::e_circle:
				{
					b2CircleShape* shape = (b2CircleShape*)f->GetShape();
					b2Vec2 pos = f->GetBody()->GetPosition();
					
					DrawCircle(METERS_TO_PIXELS(pos.x), METERS_TO_PIXELS(pos.y), (float)METERS_TO_PIXELS(shape->m_radius), Color{0, 0, 0, 128});

					// Radius line to see the rotation
					b2Vec2 edge = b->GetWorldPoint(b2Vec2(shape->m_radius, 0.0f));
					DrawLine(METERS_TO_PIXELS(pos.x), METERS_TO_PIXELS(pos.y), METERS_TO_PIXELS(edge.x), METERS_TO_PIXELS(edge.y), WHITE);
				}
				break;

				// Draw polygons ------------------------------------------------
				case b2Shape::e_polygon:
				{
					b2PolygonShape* polygonShape = (b2PolygonShape*)f->GetShape();
					int32 count = polygonShape->m_count;
					b2Vec2 prev, v;

					for(int32 i = 0; i < count; ++i)
					{
						v = b->GetWorldPoint(polygonShape->m_vertices[i]);
						if(i > 0)
							DrawLine(METERS_TO_PIXELS(prev.x), METERS_TO_PIXELS(prev.y), METERS_TO_PIXELS(v.x), METERS_TO_PIXELS(v.y), RED);

						prev = v;
					}

					v = b->GetWorldPoint(polygonShape->m_vertices[0]);
					DrawLine(METERS_TO_PIXELS(prev.x), METERS_TO_PIXELS(prev.y), METERS_TO_PIXELS(v.x), METERS_TO_PIXELS(v.y), RED);
				}
				break;

				// Draw chains contour -------------------------------------------
				// (loops already repeat the first vertex at the end, open chains must NOT be closed)
				case b2Shape::e_chain:
				{
					b2ChainShape* shape = (b2ChainShape*)f->GetShape();
					b2Vec2 prev, v;

					for(int32 i = 0; i < shape->m_count; ++i)
					{
						v = b->GetWorldPoint(shape->m_vertices[i]);
						if(i > 0)
							DrawLine(METERS_TO_PIXELS(prev.x), METERS_TO_PIXELS(prev.y), METERS_TO_PIXELS(v.x), METERS_TO_PIXELS(v.y), GREEN);
						prev = v;
					}
				}
				break;

				// Draw a single segment(edge) ----------------------------------
				case b2Shape::e_edge:
				{
					b2EdgeShape* shape = (b2EdgeShape*)f->GetShape();
					b2Vec2 v1, v2;

					v1 = b->GetWorldPoint(shape->m_vertex1);
					v2 = b->GetWorldPoint(shape->m_vertex2);
					DrawLine(METERS_TO_PIXELS(v1.x), METERS_TO_PIXELS(v1.y), METERS_TO_PIXELS(v2.x), METERS_TO_PIXELS(v2.y), BLUE);
				}
				break;
			}

			// Only dynamic bodies can be dragged
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse_joint == NULL && body_clicked == NULL
				&& b->GetType() == b2_dynamicBody && f->TestPoint(mouse_position))
			{
				body_clicked = b;
			}
		}
	}

	if (body_clicked != NULL)
	{
		b2MouseJointDef def;
		def.bodyA = ground;
		def.bodyB = body_clicked;
		def.target = mouse_position;
		def.maxForce = 100.0f * body_clicked->GetMass();
		b2LinearStiffness(def.stiffness, def.damping, 5.0f, 0.7f, def.bodyA, def.bodyB); // 5 Hz, damping ratio 0.7

		mouse_joint = (b2MouseJoint*)world->CreateJoint(&def);
		body_clicked->SetAwake(true);
	}

	if (mouse_joint != NULL && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
	{
		mouse_joint->SetTarget(mouse_position);

		b2Vec2 anchor_a = mouse_joint->GetAnchorA(); // the target
		b2Vec2 anchor_b = mouse_joint->GetAnchorB(); // the point of the body we clicked
		DrawLine(METERS_TO_PIXELS(anchor_a.x), METERS_TO_PIXELS(anchor_a.y), METERS_TO_PIXELS(anchor_b.x), METERS_TO_PIXELS(anchor_b.y), RED);
	}

	if (mouse_joint != NULL && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
	{
		world->DestroyJoint(mouse_joint);
		mouse_joint = NULL;
	}

	return UPDATE_CONTINUE;
}


// Called before quitting
bool ModulePhysics::CleanUp()
{
	LOG("Destroying physics world");

	// Delete the whole physics world! (this also frees every body, fixture and joint it owns)
	delete world;
	world = NULL;
	ground = NULL;
	mouse_joint = NULL;

	return true;
}

// PhysBody pointer as UserData: set it in the b2BodyDef BEFORE calling CreateBody()
PhysBody* ModulePhysics::CreateCircle(int x, int y, int radius, b2BodyType type, float restitution)
{
	PhysBody* pbody = new PhysBody();

	b2BodyDef body;
	body.type = type;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody);

	b2Body* b = world->CreateBody(&body);

	b2CircleShape shape;
	shape.m_radius = PIXEL_TO_METERS(radius);

	b2FixtureDef fixture;
	fixture.shape = &shape;
	fixture.density = 1.0f;
	fixture.restitution = restitution;
	b->CreateFixture(&fixture);

	pbody->body = b;

	return pbody;
}

PhysBody* ModulePhysics::CreateRectangle(int x, int y, int half_width, int half_height)
{
	PhysBody* pbody = new PhysBody();

	b2BodyDef body;
	body.type = b2_dynamicBody;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody);

	b2Body* b = world->CreateBody(&body);

	b2PolygonShape box;
	box.SetAsBox(PIXEL_TO_METERS(half_width), PIXEL_TO_METERS(half_height));

	b2FixtureDef fixture;
	fixture.shape = &box;
	fixture.density = 1.0f;
	b->CreateFixture(&fixture);

	pbody->body = b;

	return pbody;
}

// Static box that detects overlaps but does not collide (a trigger)
PhysBody* ModulePhysics::CreateRectangleSensor(int x, int y, int half_width, int half_height)
{
	PhysBody* pbody = new PhysBody();

	b2BodyDef body;
	body.type = b2_staticBody;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody);

	b2Body* b = world->CreateBody(&body);

	b2PolygonShape box;
	box.SetAsBox(PIXEL_TO_METERS(half_width), PIXEL_TO_METERS(half_height));

	b2FixtureDef fixture;
	fixture.shape = &box;
	fixture.isSensor = true;
	b->CreateFixture(&fixture);

	pbody->body = b;

	return pbody;
}

PhysBody* ModulePhysics::CreateCircleSensor(int x, int y, int radius)
{
	PhysBody* pbody = new PhysBody();

	b2BodyDef body;
	body.type = b2_staticBody;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody);

	b2Body* b = world->CreateBody(&body);

	b2CircleShape shape;
	shape.m_radius = PIXEL_TO_METERS(radius);

	b2FixtureDef fixture;
	fixture.shape = &shape;
	fixture.isSensor = true;
	b->CreateFixture(&fixture);

	pbody->body = b;

	return pbody;
}

// size is the number of ints, not vertices
PhysBody* ModulePhysics::CreateChain(int x, int y, const int* points, int size)
{
	PhysBody* pbody = new PhysBody();

	b2BodyDef body;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody);

	b2Body* b = world->CreateBody(&body);

	int count = size / 2;
	b2Vec2* vertices = new b2Vec2[count];
	for (int i = 0; i < count; ++i)
	{
		vertices[i].x = PIXEL_TO_METERS(points[i * 2 + 0]);
		vertices[i].y = PIXEL_TO_METERS(points[i * 2 + 1]);
	}

	b2ChainShape shape;
	shape.CreateLoop(vertices, count);
	delete[] vertices; // CreateLoop copies the vertices

	b2FixtureDef fixture;
	fixture.shape = &shape;
	b->CreateFixture(&fixture);

	pbody->body = b;

	return pbody;
}

// Static open chain: used for the walls of the table
PhysBody* ModulePhysics::CreateOpenChain(int x, int y, const int* points, int size, float restitution)
{
	PhysBody* pbody = new PhysBody();

	b2BodyDef body;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody);

	b2Body* b = world->CreateBody(&body);

	int count = size / 2;
	b2Vec2* vertices = new b2Vec2[count];
	for (int i = 0; i < count; ++i)
	{
		vertices[i].x = PIXEL_TO_METERS(points[i * 2 + 0]);
		vertices[i].y = PIXEL_TO_METERS(points[i * 2 + 1]);
	}

	// b2ChainShape is ONE-SIDED (the ball goes through it from the back side), so the
	// walls are made of independent two-sided edges: they collide from both sides
	for (int i = 0; i + 1 < count; ++i)
	{
		b2EdgeShape edge;
		edge.SetTwoSided(vertices[i], vertices[i + 1]);

		b2FixtureDef fixture;
		fixture.shape = &edge;
		fixture.restitution = restitution;
		b->CreateFixture(&fixture);
	}
	delete[] vertices;

	pbody->body = b;

	return pbody;
}

PhysBody* ModulePhysics::CreatePolygon(int x, int y, const int* points, int size, b2BodyType type, float restitution)
{
	PhysBody* pbody = new PhysBody();

	b2BodyDef body;
	body.type = type;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody);

	b2Body* b = world->CreateBody(&body);

	int count = size / 2;
	if (count > b2_maxPolygonVertices)
		count = b2_maxPolygonVertices;

	b2Vec2 vertices[b2_maxPolygonVertices];
	for (int i = 0; i < count; ++i)
	{
		vertices[i].x = PIXEL_TO_METERS(points[i * 2 + 0]);
		vertices[i].y = PIXEL_TO_METERS(points[i * 2 + 1]);
	}

	b2PolygonShape shape;
	shape.Set(vertices, count);

	b2FixtureDef fixture;
	fixture.shape = &shape;
	fixture.density = 1.0f;
	fixture.restitution = restitution;
	b->CreateFixture(&fixture);

	pbody->body = b;

	return pbody;
}

PhysBody* ModulePhysics::CreateFlipper(int x, int y, int length, int base_half, int tip_half, int direction, float rest_deg, float active_deg)
{
	PhysBody* pbody = new PhysBody();

	b2BodyDef body;
	body.type = b2_dynamicBody;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.angle = rest_deg * DEG_TO_RAD_F; // starts in the rest position
	body.bullet = true;
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody);

	b2Body* b = world->CreateBody(&body);

	// Local coordinates: the pivot is the origin of the body
	float l = PIXEL_TO_METERS(length) * (float)direction;
	float bh = PIXEL_TO_METERS(base_half);
	float th = PIXEL_TO_METERS(tip_half);

	b2Vec2 vertices[4];
	vertices[0].Set(0.0f, -bh);
	vertices[1].Set(l, -th);
	vertices[2].Set(l, th);
	vertices[3].Set(0.0f, bh);

	b2PolygonShape shape;
	shape.Set(vertices, 4);

	b2FixtureDef fixture;
	fixture.shape = &shape;
	fixture.density = 10000.0f;
	fixture.friction = 0.4f;
	fixture.restitution = 0.2f;
	b->CreateFixture(&fixture);

	// Revolute joint with limits and a motor. The joint angle is 0 in the rest position
	float relative_active = (active_deg - rest_deg) * DEG_TO_RAD_F;

	b2RevoluteJointDef jd;
	jd.Initialize(ground, b, b->GetPosition());
	jd.enableLimit = true;
	jd.lowerAngle = relative_active < 0.0f ? relative_active : 0.0f;
	jd.upperAngle = relative_active > 0.0f ? relative_active : 0.0f;
	jd.enableMotor = true;
	jd.maxMotorTorque = FLIPPER_TORQUE;

	pbody->flip_speed = relative_active > 0.0f ? FLIPPER_SPEED : -FLIPPER_SPEED;
	jd.motorSpeed = -pbody->flip_speed; // pushes towards the rest position

	pbody->joint = (b2RevoluteJoint*)world->CreateJoint(&jd);
	pbody->body = b;

	return pbody;
}

void ModulePhysics::DestroyBody(PhysBody* pbody)
{
	if (pbody == NULL)
		return;

	// Box2D also destroys the joints of this body: forget the mouse joint
	if (mouse_joint != NULL && mouse_joint->GetBodyB() == pbody->body)
		mouse_joint = NULL;

	world->DestroyBody(pbody->body); // Box2D frees the body, its fixtures and its joints
	delete pbody;                    // we free our own wrapper
}

// Call the listeners that are not NULL
void ModulePhysics::BeginContact(b2Contact* contact)
{
	PhysBody* a = (PhysBody*)contact->GetFixtureA()->GetBody()->GetUserData().pointer;
	PhysBody* b = (PhysBody*)contact->GetFixtureB()->GetBody()->GetUserData().pointer;

	if (a != NULL && a->listener != NULL)
		a->listener->OnCollision(a, b);

	if (b != NULL && b->listener != NULL)
		b->listener->OnCollision(b, a);
}

// PhysBody ------------------------------------
void PhysBody::GetPosition(int& x, int& y) const
{
	b2Vec2 pos = body->GetPosition();
	x = METERS_TO_PIXELS(pos.x);
	y = METERS_TO_PIXELS(pos.y);
}

float PhysBody::GetRotation() const
{
	return body->GetAngle();
}

void PhysBody::SetVelocity(float vx, float vy)
{
	body->SetLinearVelocity(b2Vec2(vx, vy));
}

void PhysBody::AddVelocity(float vx, float vy)
{
	body->SetLinearVelocity(body->GetLinearVelocity() + b2Vec2(vx, vy));
}

void PhysBody::Flip(bool active)
{
	if (joint != NULL)
		joint->SetMotorSpeed(active ? flip_speed : -flip_speed);
}

bool PhysBody::Contains(int x, int y) const
{
	b2Vec2 p(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));

	// True if the point is inside ANY of the shapes of this body
	for (b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
	{
		if (fixture->GetShape()->TestPoint(body->GetTransform(), p))
			return true;
	}

	return false;
}

int PhysBody::RayCast(int x1, int y1, int x2, int y2, float& normal_x, float& normal_y) const
{
	// No hit: return -1
	// Hit: fill normal_x and normal_y and return the distance between x1,y1 and the hit point
	int ret = -1;

	b2RayCastInput input;
	input.p1.Set(PIXEL_TO_METERS(x1), PIXEL_TO_METERS(y1));
	input.p2.Set(PIXEL_TO_METERS(x2), PIXEL_TO_METERS(y2));
	input.maxFraction = 1.0f;

	float closest = 1.0f;

	for (b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
	{
		// A chain has one child per edge
		for (int32 i = 0; i < fixture->GetShape()->GetChildCount(); ++i)
		{
			b2RayCastOutput output;
			if (fixture->GetShape()->RayCast(&output, input, body->GetTransform(), i) && output.fraction <= closest)
			{
				closest = output.fraction;

				float dx = (float)(x2 - x1);
				float dy = (float)(y2 - y1);
				ret = (int)(output.fraction * sqrtf(dx * dx + dy * dy));

				normal_x = output.normal.x;
				normal_y = output.normal.y;
			}
		}
	}

	return ret;
}
