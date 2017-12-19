#ifndef _GRAPHICS_PASS_H
#define _GRAPHICS_PASS_H

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
#include "cyGL.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "GraphicsObject.h"
#include "GraphicsWindow.h"
#include <algorithm>


# define NAME_CAMERA_POSITION			"camPosition"
# define NAME_CAMERA_TRANSFORM			"camTrans"
# define NAME_WORLD_TRANSFORM			"worldTrans"
# define NAME_OBJECT_TRANSFORM			"objTrans"
# define NAME_OBJECT_TRANSFORM_INVERSE	"objTransInv"
# define NAME_WORLD_TRANSFORM_INVERSE	"worldTransInv"


class GraphicsObjectMesh;	//Forward declaration
class GraphicsWindow;



class GraphicsPass {

	friend class GraphicsWindow;

public:
	char* name;

	cy::Point3f background = cy::Point3f(0,0,0);

	bool use_Ztesting = true;
	bool clear_start = true;
	bool clear_end = false;

	/*The camera that will be used for the pass.*/
	GraphicsCamera* camera = nullptr;

	GraphicsPass(bool(*pass_start)(GraphicsWindow* window, GraphicsPass* pass, cy::GLSLProgram* program),
		void(*pass_end)(GraphicsWindow* window, GraphicsPass* pass, cy::GLSLProgram* program),
		bool(*object_setter)(GraphicsWindow* window, GraphicsPass* pass, GraphicsObject* object, cy::GLSLProgram* program),
		GraphicsCamera* camera)
		: Start(pass_start), End(pass_end), SetObject(object_setter), camera(camera) {	}


private:

	
	
	///These marshalling methods are used for referring to the descendant class protected PassStart(), PassEnd(),  and SetupObject() methods.
	static bool PassStart_marshaller(GraphicsWindow* window, GraphicsPass* pass, cy::GLSLProgram* program) { return pass->PassStart(window, program); }
	static void PassEnd_marshaller(GraphicsWindow* window, GraphicsPass* pass, cy::GLSLProgram* program) { pass->PassEnd(window, program); }
	static bool SetupObject_marshaller(GraphicsWindow* window, GraphicsPass* pass, GraphicsObject* object, cy::GLSLProgram* program) { return pass->SetupObject(window, object, program); }

public:
	
	/*The function used to set world state variables for this pass.  May change features like the camera orientation (to render reflectance).  If this function 
	returns false, the pass will be skipped.*/
	bool(*Start)(GraphicsWindow* window, GraphicsPass* pass, cy::GLSLProgram* program) = nullptr;

	/*The function use to wrap up a pass.*/
	void(*End)(GraphicsWindow* window, GraphicsPass* pass, cy::GLSLProgram* program) = nullptr;

	/*The function used for special handling of an object.  If the function returns false, the object will be skipped.*/
	bool(*SetObject)(GraphicsWindow* window, GraphicsPass* pass, GraphicsObject* object, cy::GLSLProgram* program) = nullptr;

	/*If a shader requires a particular program to override the program of all objects, that is set here.*/
	cy::GLSLProgram* shader_override = nullptr;

	virtual void BufferContents() {
		for (GraphicsObject* obj : inclusions) if (obj->vao_name == NULL) obj->BufferVertexData();
	}


	/*Returns whether this pass includes the given item.*/
	bool Includes(GraphicsObject* obj) { return inclusions.count(obj) > 0; }

	/*Includes the given item for this pass.  Returns true if the item was successfully added; otherwise, returns false.*/
	virtual bool Include(GraphicsObject* obj) {

		exclusions.erase(obj);
		if (!inclusions.insert(obj).second) return false;

		cy::GLSLProgram* shader = obj->GetShader();
		if (shader == nullptr) {
			shader = shader_override;
			if (shader==nullptr) Throw("GraphicsPass::Include error - object included in pass does not have associated shader, and there is no pass shader override.");
		}
		if (!_by_shader.count(shader)) _by_shader.emplace(shader, std::vector<GraphicsObject*>());
		_by_shader[shader].push_back(obj);

		return true;
	}


	/*Excludes the given item from this pass.  Returns true if the item was successfully removed; otherwise, returns false.*/
	virtual bool Exclude(GraphicsObject* obj) {

		inclusions.erase(obj);
		if (!exclusions.insert(obj).second) return false;

		cy::GLSLProgram* shader = obj->GetShader();
		std::vector<GraphicsObject*>* vec = &_by_shader[shader];
		vec->erase(std::remove(vec->begin(), vec->end(), obj), vec->end());
		if (vec->size() == 0) _by_shader.erase(shader);

		return true;
	}


	virtual void ClearBuffer() {
		glClearColor(background.x, background.y, background.z, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

protected:

	std::unordered_map<cy::GLSLProgram*, std::vector<GraphicsObject*>> _by_shader;
	std::unordered_set<GraphicsObject*> exclusions;
	std::unordered_set<GraphicsObject*> inclusions;


	/*Override this method in inherited classes to specify pass start behavior.*/
	virtual bool PassStart(GraphicsWindow* window, cy::GLSLProgram* program) { return false; }
	/*Override this method in inherited classes to specify object setup behavior.*/
	virtual bool SetupObject(GraphicsWindow* window, GraphicsObject* object, cy::GLSLProgram* program) { return false; }
	/*Override this method in inherited classes to specify pass end behavior.*/
	virtual void PassEnd(GraphicsWindow* window, cy::GLSLProgram* program) {}

	GraphicsPass(GraphicsCamera* camera) : GraphicsPass(PassStart_marshaller, PassEnd_marshaller, SetupObject_marshaller, camera) {	}

public:

	/*Renders the pass.  Can be overridden in a derived pass type.*/
	virtual void Execute(GraphicsWindow* window, std::unordered_set<GraphicsObject*>* additionalExclusions = nullptr) {

		//std::cout << "blah" << std::endl;

		CHECK_GL_ERROR("GraphicsPass::Execute start", name);

		if (clear_start) ClearBuffer();
		if (use_Ztesting) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);

		//std::cout << camera->GetTransform()[0] << std::endl;

		for (auto prog_it : _by_shader) {

			////Call the pass starter.
			cy::GLSLProgram* prog = (shader_override == nullptr) ? prog_it.first : shader_override;		//If the pass has an override shader, use that instead.
			if (prog == nullptr) continue;
			prog->Bind();
			if (Start != nullptr && !Start(window, this, prog)) continue;

			for (auto obj : prog_it.second) {
				CHECK_GL_ERROR("GraphicsPass::Execute object loop start", name, obj->name);
				if (obj == nullptr) continue;
				if (exclusions.count(obj) > 0 ) continue;
				if (additionalExclusions != nullptr && additionalExclusions->count(obj) > 0) continue;

				glBindVertexArray(obj->vao_name);

				//Call the pass's object setter
				if (SetObject != nullptr && !SetObject(window, this, obj, prog)) { glBindVertexArray(NULL);	continue; }
				CHECK_GL_ERROR("GraphicsPass::Execute called SetObject");

				//Set the object's appearance.
				if (!obj->SetAppearance(prog)) { glBindVertexArray(NULL); continue; }
				CHECK_GL_ERROR("GraphicsPass::Execute called SetAppearance");

				//Draw the object.
				obj->Draw();
				CHECK_GL_ERROR("GraphicsPass::Execute called Draw");

				glBindVertexArray(NULL);
				CHECK_GL_ERROR("GraphicsPass::Execute object loop end", name, obj->name);
			}

			CHECK_GL_ERROR("unknown");


			//Call the pass ender.
			if (End != nullptr) End(window, this, prog);
			CHECK_GL_ERROR("GraphicsPass::End");

			glUseProgram(NULL);
		}

		CHECK_GL_ERROR("GraphicsWindow::DisplayPass end", name);
		if (clear_end) ClearBuffer();
	}
};






class GraphicsPassParent : public GraphicsPass{

private:
	

	virtual void BufferContents() {
		GraphicsPass::BufferContents();
		for (GraphicsPass* prePass : _pre_passes) prePass->BufferContents();
		for (GraphicsPass* postPass : _post_passes) postPass->BufferContents();
	}
public:


	void AddPrePass(GraphicsPass* pass) {
		//TODO:  watch out for circular structure.
		_pre_passes.push_back(pass);
	}
	void AddPostPass(GraphicsPass* pass) {
		//TODO:  watch out for circular structure.
		_post_passes.push_back(pass);
	}
	int IndexOf(GraphicsPass* pass) { Throw("Not implemented GraphicsPassParent::IndexOf yet."); }
	bool RemmovePass(GraphicsPass* pass) { Throw("Have not implemented GraphicsPassParent::IndexOf yet."); }


	virtual void Execute(GraphicsWindow* window, std::unordered_set<GraphicsObject*>* additionalExclusions = nullptr) {
		CHECK_GL_ERROR("GraphicsPass::Execute start", name);

		if (clear_start) ClearBuffer();
		if (use_Ztesting) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);

		for (GraphicsPass* prePass : _pre_passes) prePass->Execute(window);
		GraphicsPass::Execute(window);
		for (GraphicsPass* postPass : _post_passes) postPass->Execute(window);

		if (clear_end) ClearBuffer();
	}


protected:
	std::vector<GraphicsPass*> _pre_passes;
	std::vector<GraphicsPass*> _post_passes;
	GraphicsPassParent(GraphicsCamera* camera) : GraphicsPass(camera) {}
};


#endif