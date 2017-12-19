
///CS 5610 HW 3 - Shading
///Wesley Oates

//The following comment is necessary to access the static glew lib, and to specify its name is glew32s.lib.  I couldn't get other library to successfully link.
#pragma comment (lib, "glew32s.lib")
#define GLEW_STATIC

//The following definition shuts of some warnings that arose out of cytrimesh.h
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <stdio.h>
#include "cytrimesh.h"
#include "cyMatrix.h"
#include "GraphicsWindow.h"
#include "Passes.h"
#include "wo.h"
#include "WaterSimulator.h"


GraphicsWindow* main_window;

WaterSimulator* simulator;


GraphicsCamera* reflection_camera;
GraphicsObjectMesh* waterSurface;
GraphicsMaterialWaterSurface* waterMaterial;


GLuint *tex_cube;

bool is_paused = true;
int mouse_pressed;
cy::Point2<int> mouse_point;
float yaw = 0;
float pitch = (float)(-PI / 2);
char keysPressed = 0;
auto lastRun = std::chrono::steady_clock::now();
int raining = 0;

/*
=====================================
=			main.cpp				=
=		INPUT HANDLERS				=
=====================================
*/



void AdjustView() {

	cy::Point3f newLookDir = waterSurface->GetPosition() - main_window->camera.GetPosition();
	cy::Point3f newUpDir = main_window->camera.GetUpDirection();
	newLookDir = cy::Point3f(newLookDir.x, newLookDir.y, -newLookDir.z);
	newUpDir = cy::Point3f(newUpDir.x, newUpDir.y, -newUpDir.z);
	reflection_camera->SetLookAt(waterSurface->GetPosition(), waterSurface->GetPosition() + newLookDir, newUpDir);

	glutPostRedisplay();
}
void SetPosition(cy::Point3f position) {
	main_window->camera.SetPosition(position);
	reflection_camera->SetLookAt(cy::Point3f(0, 0, 0), position, main_window->camera.GetUpDirection());
	AdjustView();
}



/*Called when a key is pressed down.*/
void OnKeyDown(unsigned char key, int x, int y) {
	static int obstacles = 0;
	if (key == 27) glutLeaveMainLoop();
	
	else if (key == 'a') { SetPosition(main_window->camera.GetPosition() + main_window->camera.GetRightDirection()*10); }
	else if (key == 's') { SetPosition(main_window->camera.GetPosition() - main_window->camera.GetUpDirection()*10);  }
	else if (key == 'd') { SetPosition(main_window->camera.GetPosition() - main_window->camera.GetRightDirection()*10);  }
	else if (key == 'w') { SetPosition(main_window->camera.GetPosition() + main_window->camera.GetUpDirection() * 10);  }

	else if (key == 'Q') { main_window->camera.SetLookAt(main_window->camera.GetPosition(), main_window->camera.GetPosition() - main_window->camera.GetLookDirection(), main_window->camera.GetUpDirection()); AdjustView(); }
	else if (key == 'q') { main_window->camera.SetUpDirection(cy::Point3f(0, 1, 0)); AdjustView(); }
	else if (key == 'E') { main_window->camera.SetLookAt(cy::Point3f(0, 0, 0), cy::Point3f(-1, 0, 0), cy::Point3f(0, 1, 0)); AdjustView(); }
	else if (key == 'e') { main_window->camera.SetLookAt(cy::Point3f(0, 0, 0), cy::Point3f(0, 0, 1), cy::Point3f(0, 1, 0)); AdjustView(); }
	


	else if (key == ' ') {

		static auto sim_started = std::chrono::steady_clock::now();

		if (is_paused) {
			simulator->runCount = 0;
			sim_started = std::chrono::steady_clock::now();
			std::cout << "Simulation started." << std::endl;
		}
		else {
			auto sim_ended = std::chrono::steady_clock::now();
			int milliSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(sim_ended - sim_started).count();
			float seconds = milliSeconds / 1000.0f;
			std::cout << simulator->runCount << " runs in " << milliSeconds << " milliseconds = " << (float)milliSeconds / (float)simulator->runCount << " ms/frame, or " << (float)simulator->runCount / seconds << " FPS." << std::endl;
		}
		is_paused = !is_paused;
		if (is_paused) lastRun = std::chrono::steady_clock::now();
	}
	else if (key == 'R') { if (raining < 10) raining++;		std::cout << "Rain set to " << raining << std::endl; }
	else if (key == 'r') { if (raining > 0) raining--;		std::cout << "Rain set to " << raining << std::endl; }
	else if (key == 'c') { simulator->Clear(); }
	else if (key == 'F') { simulator->depth *= 1.2f;		simulator->depth = fminf(simulator->depth, 100.0f);		waterMaterial->depth = simulator->depth / 10.0f;		std::cout << "Depth set to " << simulator->depth << std::endl; }
	else if (key == 'f') { simulator->depth *= 0.8f;		simulator->depth = fmaxf(simulator->depth, 0.01f);		waterMaterial->depth = simulator->depth / 10.0f;		std::cout << "Depth set to " << simulator->depth << std::endl; }
	else if (key == 'O') {
		if (obstacles < 7) {
			obstacles++;
			bool hasBorder = (obstacles & 1) > 0;
			bool hasBox = (obstacles & 2) > 0;
			bool hasBar = (obstacles & 4) > 0;
			simulator->SetObstacles(hasBorder, hasBox, hasBar);
			std::cout << "Obstacle configuration set to " << obstacles << ", border:" << hasBorder << ", box:" << hasBox << ", bar:" << hasBar << std::endl;
		}
	}
	else if (key == 'o') {
		if (obstacles > 0) {
			obstacles--;
			bool hasBorder = (obstacles & 1) > 0;
			bool hasBox = (obstacles & 2) > 0;
			bool hasBar = (obstacles & 4) > 0;
			simulator->SetObstacles(hasBorder, hasBox, hasBar);
			std::cout << "Obstacle configuration set to " << obstacles << ", border:" << hasBorder << ", box:" << hasBox << ", bar:" << hasBar << std::endl;
		}
	}
	else if (key == 'P') {
		cy::Point2f pt = cy::Point2f(150, 230);
		for (int i = 0; i < simulator->levels; i++)
			simulator->Perturb(pt, i, pt, 0.01f, 10.0f, simulator->currentTime);
	}
	else if (key == 'p') {
		cy::Point2f pt = cy::Point2f(150, 230);
		for (int i = 0; i < simulator->levels; i++) {
			simulator->Perturb(pt, i, pt, 0.001f, 1.0f, simulator->currentTime);
		}
	}
	else if (key == 'T') {
		if (simulator->amplitude_time_ebb < 1.0f) { simulator->amplitude_time_ebb += 0.05f;	std::cout << "Time ebbing ratio set to " << simulator->amplitude_time_ebb << std::endl; }
	}
	else if (key == 't') {
		if (simulator->amplitude_time_ebb > 0.0f) { simulator->amplitude_time_ebb -= 0.05f; std::cout << "Time ebbing ratio set to " << simulator->amplitude_time_ebb << std::endl; }
	}
	else if (key == 'Y') {
		if (simulator->amplitude_distance_ebb < 1.0f) { simulator->amplitude_distance_ebb += 0.05f; std::cout << "Distance ebbing ratio set to " << simulator->amplitude_distance_ebb << std::endl; }
	}
	else if (key == 'y') {
		if (simulator->amplitude_distance_ebb > 0.0f) { simulator->amplitude_distance_ebb -= 0.05f; std::cout << "Distance ebbing ratio set to " << simulator->amplitude_distance_ebb << std::endl; }
	}
	else if (key == 'U') { if (simulator->soliton_speed < 1.0f) { simulator->soliton_speed += 0.05f; std::cout << "Soliton speed ratio set to " << simulator->soliton_speed << std::endl; } }
	else if (key == 'u') { if (simulator->soliton_speed > 0.0f) { simulator->soliton_speed -= 0.05f; std::cout << "Soliton speed ratio set to " << simulator->soliton_speed << std::endl; } }

	else {
		std::cout << "'" << key << "'" << std::endl;
	}
	glutPostRedisplay();
	
	
}
/*Called when a key is released.*/
void OnKeyUp(unsigned char key, int x, int y) {}
/*Called when a mouse button is pressed.*/
void OnMouseInput(int button, int state, int x, int y) {	

	if (button == 3) {
		//Scroll up.
		SetPosition(main_window->camera.GetPosition() + main_window->camera.GetLookDirection());
		AdjustView();
	}
	else if (button == 4) {
		//Scroll down.
		SetPosition(main_window->camera.GetPosition() - main_window->camera.GetLookDirection());
		AdjustView();
	}
	else if (state == GLUT_UP)
		mouse_pressed = -1;
	else {
		mouse_pressed = button;
		mouse_point = cy::Point2<int>(x, y);
	}
}
/*Called when the mouse is moved with a button pressed.*/
void OnMouseMoveInput(int x, int y) {
	float deltaX = (float)x - mouse_point.x;
	float deltaY = (float)y - mouse_point.y;
	mouse_point = cy::Point2<int>(x, y);

	//No Alt pressed
	if (mouse_pressed == GLUT_RIGHT_BUTTON) {
		if (glutGetModifiers() & GLUT_ACTIVE_CTRL) {

		}
		else {
			//camera_position.z += deltaY / 10.0f;			
			if (main_window != nullptr) {
				//Front-back and left-right movement
				cy::Point3f advance = -deltaY * main_window->camera.GetLookDirection() /  10.0f;
				cy::Point3f right = deltaX * main_window->camera.GetRightDirection() / 10.0f;
				SetPosition(main_window->camera.GetPosition() + advance  + right);
				
				//if (mirrorPass!=nullptr) mirrorPass->SetReflection(&main_window->camera);
			}
		}
	}

	//No alt pressed.
	else if (mouse_pressed == GLUT_LEFT_BUTTON) {
		if (glutGetModifiers() & GLUT_ACTIVE_CTRL) {
			yaw += deltaX / 180.0f;
			pitch += deltaY / 180.0f;
			//if (teapot != nullptr) teapot->SetRotation(pitch, yaw, 0);

		}
		else {
			main_window->camera.Pitch(deltaY / 360.0f);
			main_window->camera.Yaw(deltaX / 360.0f);
			//if (mirrorPass != nullptr) mirrorPass->SetReflection(&main_window->camera);
		}
	}
	
	else {
		return;
	}

	AdjustView();
}


void OnIdle() {
	const int  milliseconds_per_tick = 30;

	static auto runStart = std::chrono::steady_clock::now();

	//Do nothing if not enough time has passed.
	if (is_paused) return;

	

	auto currentTime = std::chrono::steady_clock::now();
	int elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastRun).count();
	if (elapsed_time < milliseconds_per_tick) return;
	elapsed_time = milliseconds_per_tick;
	int total_time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - runStart).count();

	//Add a raindrop?
	if (raining > 0) {
		int rain_yes = rand() % 20;
		if (rain_yes < raining) {
			int r_x = rand() % simulator->width;
			int r_y = rand() % simulator->height;
			float phase_offset = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / PI));
			//int r_level_mask = rand();
			for (int i = 0; i < simulator->levels; i++) {
				int mask = 1 << i;
				//if ((r_level_mask & mask) == 0) continue;
				//						location		level		origin			waveNumber	  amplitude	   timeStamp		phase_offset = 0.0f
				simulator->Perturb(cy::Point2f(r_x, r_y), i, cy::Point2f(r_x, r_y), 1.0f / 5*(i + 1), 1.0f, simulator->currentTime, phase_offset);
			}
		}
	}

	simulator->Execute(elapsed_time);
	glutPostRedisplay();

	lastRun = std::chrono::steady_clock::now();
}
GLuint frameBufferName = 0;
GLuint renderTextureName = 0;
GLuint depthBufferName = 0;


/*
=====================================
=			main.cpp				=
=		INSERTION POINT				=
=====================================
*/
int main(int argc, char **argv) {

	//STEP #1, file operations to get the object at the filename given at the command line.
	if (argc != 2) throw std::exception("Program must be executed with exactly one command line parameter specifying the object file to load.");
	char* filename = argv[1];
	

	//Step #2, build and fire up the window and INPUT CALLBACKS
	main_window = new GraphicsWindow(1024, 768, "I'm a little teapot.");	
	glutKeyboardFunc(OnKeyDown);
	glutMouseFunc(OnMouseInput);
	glutMotionFunc(OnMouseMoveInput);
	glutKeyboardUpFunc(OnKeyUp);
	glutIdleFunc(OnIdle);

	//Step #3a - set up the window BACKGROUND and CAMERA
	main_window->GetStandardPass()->background = cy::Point3f(0.2f, 0.2f, 0.3f);
	main_window->camera.SetPerspective();
	main_window->camera.SetPosition(0, 0, 0);
	
	//Step #3b - set up the LIGHT OBJECT
	//GraphicsObjectMesh* lightObject = GraphicsObjectMesh::CreateCube(2.5, 2.5, 2.5);
	GraphicsObjectMesh* lightObject = GraphicsObjectMesh::CreateGlobe(2, 5, 5);
	lightObject->BlendNormals();
	lightObject->name = "lightObject";	
	lightObject->material = new GraphicsMaterialEmissive(cy::Point3f(0, 1, 0), 15.0f);
	main_window->Add(lightObject);
	main_window->lighting = new GraphicsLightPoint(cy::Point3f(300,0,0), cy::Point3f(1, 1, 1), cy::Point3f(0.3f, 0.3f, 0.3f), lightObject);	

	//Step #3c - set up the lighting example GLOBE
	//GraphicsObjectMesh* globe = GraphicsObjectMesh::CreateGlobe(50.0f, 20, 20);
	GraphicsObjectMesh* globe = GraphicsObjectMesh::CreateCube(25,25,25);
	//globe->BlendNormals();
	globe->name = "globe";
	globe->material = new GraphicsMaterialBlinn(cy::Point3f(0.1f, 0.1f, 0.1f), cy::Point3f(1.0f, 0.0f, 1.0f), cy::Point3f(1, 1, 1), 150.0f);
	globe->SetPosition(150, 0, 0);
	main_window->Add(globe);

	//Step #4a, create the ENVIRONMENT MAP pass
	wo::TextureCubeMap* cubeMap = wo::TextureCubeMap::FromFiles("Resources/cubeMap_posx.png", "Resources/cubeMap_posy.png", "Resources/cubeMap_posz.png", "Resources/cubeMap_negx.png", "Resources/cubeMap_negy.png", "Resources/cubeMap_negz.png");
	GraphicsObjectEnvironment* env = new GraphicsObjectEnvironment(cubeMap, nullptr);
	env->name = "environment";
	GraphicsPassEnvironment* envPass = new GraphicsPassEnvironment(env, &main_window->camera);
	envPass->clear_end = false;


	//Step #4b, create the CUBE MAPPING pass
	reflection_camera = new GraphicsCamera(nullptr);
	reflection_camera->SetLookAt(cy::Point3f(0, 0, 0), main_window->camera.GetPosition(), main_window->camera.GetUpDirection());
	GraphicsPassCubeMapping* cube_mapping_pass = new GraphicsPassCubeMapping(1024, 15,  reflection_camera);
	//GraphicsPassCubeMapping* cube_mapping_pass = new GraphicsPassCubeMapping(1024, 15, &main_window->camera);
	cube_mapping_pass->SetReflection(true);
	//GraphicsMaterialCubeMap* reflector_material = new GraphicsMaterialCubeMap(cube_mapping_pass->GetCubeMap());
	CHECK_GL_ERROR("here");


	//Step #5a - create the WATER SIMULATOR
	simulator = new WaterSimulator(256, 256, 4, 1.0f);
	simulator->depth = 10.0f;
	simulator->Execute(0);

	//Step #5b, frequency DEV textures
	{	
		GraphicsMaterialTexture<GL_TEXTURE_2D>* devMaterialTextures[6];
		devMaterialTextures[0] = new GraphicsMaterialTexture<GL_TEXTURE_2D>(0, simulator->devTextures[0], 0, 150.0f, false);
		devMaterialTextures[1] = new GraphicsMaterialTexture<GL_TEXTURE_2D>(0, simulator->devTextures[1], 0, 150.0f, false);
		devMaterialTextures[2] = new GraphicsMaterialTexture<GL_TEXTURE_2D>(0, simulator->devTextures[2], 0, 150.0f, false);
		devMaterialTextures[3] = new GraphicsMaterialTexture<GL_TEXTURE_2D>(0, simulator->devTextures[3], 0, 150.0f, false);
		devMaterialTextures[4] = new GraphicsMaterialTexture<GL_TEXTURE_2D>(0, simulator->GetReflectionMapID(), 0, 150.0f, false);
		devMaterialTextures[5] = new GraphicsMaterialTexture<GL_TEXTURE_2D>(0, simulator->GetNormalMapID(), 0, 150.0f, false);
		GraphicsObjectMesh* devSurfaces[6];
		for (int i = 0; i < 6; i++) {
			devSurfaces[i] = GraphicsObjectMesh::CreateRectangle(100, 100);
			devSurfaces[i]->name = "devSurface";
			devSurfaces[i]->material = devMaterialTextures[i];
			devSurfaces[i]->SetRotation(0, PI / 2, 0);
			main_window->Add(devSurfaces[i]);
		}
		devSurfaces[0]->SetPosition(-350, 101, -51);
		devSurfaces[1]->SetPosition(-350, 101, 51);
		devSurfaces[2]->SetPosition(-350, 0, -51);
		devSurfaces[3]->SetPosition(-350, 0, 51);
		devSurfaces[4]->SetPosition(-350, -101, -51);
		devSurfaces[5]->SetPosition(-350, -101, 51);

		

		CHECK_GL_ERROR("here");
	}
	
	
	//Step #5b, prepare the water surface viewer.
	cy::GLTexture2D* waterBed = GetTexture("RESOURCES/brick.png");
	waterBed->SetWrappingMode(GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER);
	waterMaterial = new  GraphicsMaterialWaterSurface(simulator->GetNormalMapID(), waterBed->GetID(), cube_mapping_pass->GetCubeMap()->GetID(), 200.0f);
	waterMaterial->water_color = cy::Point3f(0.05, 0.2, 0.8);
	waterMaterial->depth = simulator->depth/ 10.0f;

	waterSurface = GraphicsObjectMesh::CreateRectangle(100, 100);
	waterSurface->name = "waterSurface";
	waterSurface->material = waterMaterial;	
	main_window->Add(waterSurface);
	waterSurface->SetPosition(0, 0, 150);
	//waterSurface->SetRotation(-PI / 2, 0, 0);
	waterSurface->SetRotation(0, -PI, 0);
	
	CHECK_GL_ERROR("here");







	//Step #6, add the appropriate PASSES
	cube_mapping_pass->AddPrePass(envPass);	
	cube_mapping_pass->AddPrePass(main_window->GetStandardPass());
	main_window->AddPass(cube_mapping_pass);
	main_window->SetEnvironmentPass(envPass);
	envPass->clear_start = false;
	main_window->GetStandardPass()->clear_start = false;
	main_window->use_standard_pass = true;
	
	
	
	//Step #6, check for errors.
	CHECK_GL_ERROR("main complete.");
	

	//Step #7, run the loop!
	glutMainLoop();
}


