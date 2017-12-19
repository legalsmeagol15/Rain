
///CS 5610 HW 4 - Textures
///Wesley Oates

#ifndef _GRAPHICS_WINDOW	//Not all compilers allow "#pragma once"
#define _GRAPHICS_WINDOW

//The following comment is necessary to access the static glew lib, and to specify its name is glew32s.lib.  I couldn't get other library to successfully link.
#pragma comment (lib, "glew32s.lib")
#define GLEW_STATIC

//The following definition shuts of some warnings that arose out of cytrimesh.h
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <GL/glew.h>
#include <GL/freeglut.h>

#include "cytrimesh.h"
#include "cyGL.h"
#include "cyMatrix.h"
#include "GraphicsObject.h"
#include "GraphicsCamera.h"
#include "GraphicsPass.h"
#include "GraphicsLighting.h"
#include "lodepng.h"
#include "helpers.h"
#include <unordered_map>
#include <vector>
#include <exception>
#include <chrono>
#include "WaterSimulator.h"

# define PI				3.14159265358979323846  /* pi, probably don't need all of math.h */


# define NAME_ENVIRONMENT_MAP			"environmentMap"


/*Used for marshalling glut callbacks.*/
GraphicsWindow* _current_window;

cy::GLTextureCubeMap* environment_map;

GraphicsCamera* lightEye;
cy::Matrix4f lightTransform;


class GraphicsPass;

/*A class used for setting up a relationship with OpenGL.  Eventually, the plan is to use this class to manage an OpenGL relationship with one window per 
class, though from research it appears this will mean switching "contexts" - whatever that means.  That functionality will develop later.  For now, though, 
everything will be static.*/
class GraphicsWindow {

public:

	GraphicsLighting* lighting = nullptr;

	/*Sets the lighting for a scene within this window.*/
	void SetLighting(GraphicsPass* pass, cy::GLSLProgram* program) {
		if (lighting == nullptr) return;
		lighting->SetLighting(pass->camera, program);
	}
	
	/*Sets up the world's camera and lighting.*/
	static bool StandardPassStart(GraphicsWindow* window, GraphicsPass* pass, cy::GLSLProgram* prog) {
		
		prog->SetUniformMatrix4(NAME_CAMERA_TRANSFORM, pass->camera->GetTransform().data);
		prog->SetUniform(NAME_CAMERA_POSITION, pass->camera->GetPosition());
		prog->SetUniformMatrix4(NAME_WORLD_TRANSFORM, pass->camera->GetVariableWorld().data);
		//prog->SetUniformMatrix4(NAME_WORLD_TRANSFORM_INVERSE, pass->camera->GetWorldInverse().data);
		window->SetLighting(pass, prog);
		return true;
	}

	/*Doesn't really do anything.*/
	static void StandardPassEnd(GraphicsWindow* window, GraphicsPass* pass, cy::GLSLProgram* program) {	}

	/*Sets up the transformation for the object.  NOTE:  appearance for the object is handled elsewhere.*/
	static bool StandardObjectSetup(GraphicsWindow* window, GraphicsPass* pass, GraphicsObject* object, cy::GLSLProgram* prog) {		
		prog->SetUniformMatrix4(NAME_OBJECT_TRANSFORM, object->GetRelativeTransform().data);
		return true;
	}
	


protected:
	/*The rendering method.*/
	void OnDisplay() {

		//std::cout << "OnDisplay" << std::endl;
		glClearColor(0.0f, 0.0f, 0.5f,1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST); 
		
		
		for (auto pass : _passes) {			
			if (pass == nullptr) continue;		
			CHECK_GL_ERROR("GraphicsWindow::OnDisplay before execution", pass->name);
			pass->Execute(this);	
			CHECK_GL_ERROR("GraphicsWindow::OnDisplay after execution", pass->name);
		}
		
		if (_environment_pass != nullptr) {
			glDisable(GL_DEPTH_TEST);
			_environment_pass->Execute(this);
		}
		if (_standard_pass != nullptr && use_standard_pass) {
			glEnable(GL_DEPTH_TEST);
			_standard_pass->Execute(this);
		}
		
		glutSwapBuffers();

		CHECK_GL_ERROR("GraphicsWindow::OnDisplay end");
		
	}


protected:
	virtual void OnTick(int elapsed_time, int this_tick) {

	}


private:


	std::chrono::steady_clock::time_point system_start = std::chrono::steady_clock::now();

	std::chrono::steady_clock::time_point last_frame;

	std::chrono::milliseconds frame_duration;

	std::vector<cy::GLSLProgram*> animating_programs;


	void OnIdle() {

		


	}

	//  ============================
	//  ||  Marshalling methods   ||
	//  ============================
	
	static void onDisplay_Marshall() { _current_window->OnDisplay(); }	
	static void onIdle_Marshall() { _current_window->OnIdle(); }

	/*The passes and their ordering.*/
	std::vector<GraphicsPass*> _passes;

	GraphicsPass* _standard_pass;
	GraphicsPass* _environment_pass;

	

public:

	/*The pass that is run at the conclusion of all other passes.*/
	GraphicsPass* GetStandardPass() { return _standard_pass; }
	/*Sets the standard pass.*/
	void SetStandardPass(GraphicsPass* newStandardPass) { _standard_pass = newStandardPass;  }

	GraphicsPass* GetEnvironmentPass() { return _environment_pass; }
	void SetEnvironmentPass(GraphicsPass* newEnvironmentPass) { _environment_pass = newEnvironmentPass; }

	//static cy::GLTexture2D* black;
	bool use_standard_pass = true;

	/*The window width, in pixels.*/
	const int width;

	/*The window height, in pixels.*/
	const int height;
	
	/*The main camera used by this window.*/
	GraphicsCamera camera = GraphicsCamera(this);
	


	GraphicsWindow(int width, 
				   int height, 
		           char* window_title,
				   GraphicsPass* _standard_pass = nullptr
					) : width(width), height(height)
	{
		_current_window = this;
		
		//Initialize the window.
		int a = 0;
		char *v[1] = { (char*)"junk" };
		//glutInitContextFlags(GLUT_DEBUG);
		glutInit(&a, v);
		glutInitWindowSize(width, height);		// "explicitly defined" size of window.
		glutInitWindowPosition(-1, -1);			//-1 leaves it up to the windows manager.	
		glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE);
		glEnable(GL_DEPTH_TEST);		
		glutCreateWindow(window_title);

		//Setup the standard pass.
		if (_standard_pass == nullptr)
		{
			this->_standard_pass = new GraphicsPass(StandardPassStart, StandardPassEnd, StandardObjectSetup, &camera);
			this->_standard_pass->name = "standard pass";
		}
		else 
			this->_standard_pass = _standard_pass;

		//Setup the callbacks for display and idle.
		glutDisplayFunc(GraphicsWindow::onDisplay_Marshall);
		glutIdleFunc(GraphicsWindow::onIdle_Marshall);
		

		//initialize the OpenGL wrangler
		GLenum error = glewInit();		
		if (error != GLEW_OK) {
			fprintf(stderr, "Error: %s\n", glewGetErrorString(error));
			throw std::exception("Error in glewInit(): '%s'\n");
		}

		//Start the animation timer.
		last_frame = std::chrono::steady_clock::now();

	}

	


	/*Adds the given pass at the end of the pass list, but immediately before the standard pass.  If the item was already in the pass list, returns false.  Note that the standard pass exists outside the pass list, and so cannot be added to the pass list.*/
	bool AddPass(GraphicsPass* pass) {
		if (pass == _standard_pass) return false;
		if (IndexOfPass(pass) >= 0) return false;		
		_passes.push_back(pass);		

		//Make sure everything in the pass is properly buffered.
		pass->BufferContents();

		//Signal success.
		glutPostRedisplay();
		return true;
	}

	/*Inserts the pass at the given index.*/
	bool InsertPass(unsigned int index, GraphicsPass* pass) {
		if (pass == _standard_pass) return false;
		if (IndexOfPass(pass) >= 0) return false;
		if (index >= _passes.size()) return false;

		_passes.insert(_passes.begin() + index, pass);
		pass->BufferContents();
		glutPostRedisplay();
		return true;
	}

	/*Returns the index of the given pass within the pass list.  If the item does not exist on the pass list, returns -1.  Note that the standard pass is outside the pass list, and so will not have an index.*/
	int IndexOfPass(GraphicsPass* pass) { ptrdiff_t result = std::find(_passes.begin(), _passes.end(), pass) - _passes.begin(); return (result >= (ptrdiff_t)_passes.size()) ? -1 : (int)result; }

	/*Removes the given pass from the pass list.  If the item was not in the pass list, returns false.  Note that the standard pass cannot be removed from the pass list, because it exists outside the pass list.  If you don't want the standard pass to execute, set use_standard_pass = false; .*/
	bool RemovePass(GraphicsPass* pass) {
		if (pass == _standard_pass) return false;
		int idx = IndexOfPass(pass);
		if (idx < 0) return false;
		_passes.erase(_passes.begin() + idx);
		glutPostRedisplay();

		//TODO:  unubuffer data?
		Throw("GraphicsWindow::RemovePass() not implemented yet.");

		return true;
	}

	/*Returns whether the pass list contains the given pass.  Note that the standard pass is outside the pass list, and so will not be deemed to be "contained".*/
	bool ContainsPass(GraphicsPass* pass) { return IndexOfPass(pass) >= 0; }


	/*Adds the given surface model to the standard pass for this window.  If the item already exists on the standard pass, returns false; otherwise, returns true.*/
	bool Add(GraphicsObject* obj) {

		CHECK_GL_ERROR("GraphicsWindow::Add start", obj->name);

		//Add the object
		if (!_standard_pass->Include(obj)) return false;

		if (obj->vao_name == NULL)
			obj->BufferVertexData();

		//DONE!  Return indicating successful add.
		glutPostRedisplay();

		CHECK_GL_ERROR("GraphicsWindow::Add end", obj->name);

		return true;

	}

	/*Removes the given surface model.  Returns true or false based on if removal was successful.  If the model existed to be removed, returns true; otherwise, returns false.*/
	bool Remove(GraphicsObject* sm) {
	
		
		//TODO:  pull it out of openGL as well?  - meaning unbuffer data.

		//TODO:  pull out of the passes

		throw std::exception("Not implemented yet.");

		return true;
	}

};



#endif