#ifndef _PASSES_H
#define _PASSES_H

#include "GraphicsPass.h"
#include "GraphicsObject.h"

class GraphicsWindow;



/*Renders a cube map at a particular camera position and orientation.*/
class GraphicsPassCubeMapping : public GraphicsPassParent {
private:
	GLuint _fbo_id;
	GLuint _rbo_id;
	wo::TextureCubeMap cube_map;

	cy::Matrix4f _render_lens;

	
public:
	
	/*If the pass is mapping a reflection, then the cube will flip where it draws the textures.*/
	void SetReflection(bool value) { is_reflection = value; }

	/*The size of each edge of the cube.*/
	const int size;

	/*Returns the cube map.*/
	wo::TextureCubeMap* GetCubeMap() { return &cube_map; }

	GraphicsPassCubeMapping(int size, GLubyte emptyByte, GraphicsCamera* camera, bool useMipMaps = false)
		: GraphicsPassParent(camera), cube_map(size, size, emptyByte, useMipMaps), size(size)
	{
		_render_lens.SetPerspective((float)(PI / 2), DEFAULT_ASPECT_RATIO, DEFAULT_NEAR_PLANE, DEFAULT_FAR_PLANE);
		cy::Matrix4f reflect;
		reflect.SetScale(-1, 1, -1);
		_render_lens *= reflect;

		CHECK_GL_ERROR("GraphicsPassCubeMapping::ctor start");
		clear_start = true;
		clear_end = true;
		use_Ztesting = true;

		name = "Cube mapping";

		//Get the previous binding.
		GLint prev_fbo;
		GLint prev_rbo;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
		glGetIntegerv(GL_RENDERBUFFER_BINDING, &prev_rbo);

		//Set up the frame buffer.
		glGenFramebuffers(1, &_fbo_id);
		glGenRenderbuffers(1, &_rbo_id);
		glBindFramebuffer(GL_FRAMEBUFFER, _fbo_id);
		
		//if (CheckStatus() != GL_FRAMEBUFFER_COMPLETE) { std::cout << "Frame buffer error 0:  " << (int)CheckStatus() << std::endl; }

		//Ensure there is enough space in the texture.
		glBindTexture(GL_TEXTURE_CUBE_MAP, cube_map.GetID());
		if (!useMipMaps) {
			//This is recommended in http://gamedev.stackexchange.com/questions/19461/opengl-glsl-render-to-cube-map
			//Something about OpenGL being picky as to the mipmap pyramid and whether a texture is considered 
			//complete.  I infer that openGL might expect a "buildmipmaps" call before being complete, so if 
			//mipmaps aren't used, just as well set the base and max levels for the mipmap pyramid to 0.
			//TODO:  migrate this to the texture setup?
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);
		}
		glBindTexture(GL_TEXTURE_CUBE_MAP, NULL);
		
		//Setup the render buffer.		
		glBindRenderbuffer(GL_RENDERBUFFER, _rbo_id);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, size, size);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _rbo_id);

		//Close the bindings.
		glBindRenderbuffer(GL_RENDERBUFFER, prev_rbo);
		glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);

		CHECK_GL_ERROR("GraphicsPassCubeMapping::ctor end");
	}
	~GraphicsPassCubeMapping() {
		glDeleteRenderbuffers(1, &_rbo_id);
		glDeleteFramebuffers(1, &_fbo_id);
	}

	/*Returns the status of the frame bufer.*/
	GLenum CheckStatus() {
		GLint prev_fbo;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo_id);
		GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_fbo);
		return status;
	}

	bool is_reflection = false;

private:



	virtual void Execute(GraphicsWindow* window, std::unordered_set<GraphicsObject*>* additionalExclusions = nullptr) {

		//std::cout << "blah" << std::endl;
		CHECK_GL_ERROR("GraphicsPassCubeMapping::Execute start", name);
		//What was already bound?
		GLint prev_fbo;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);

		//Create the cameras for the 6 cardinal directions.	
		cy::Point3f pos = camera->GetPosition(), look = camera->GetLookDirection(), up = camera->GetUpDirection(), right = camera->GetRightDirection();
		//up = cy::Point3f(-up.x, up.y, up.z);
		GraphicsCamera* cams[6];
		if (is_reflection) {
			cams[0] = new GraphicsCamera(nullptr, pos, pos + right, -up);		//Pos X, at least to start out with.
			cams[1] = new GraphicsCamera(nullptr, pos, pos - right, -up);		//Neg X
			cams[2] = new GraphicsCamera(nullptr, pos, pos + up, look);			//Pos Y
			cams[3] = new GraphicsCamera(nullptr, pos, pos - up, -look);		//Neg Y
			cams[4] = new GraphicsCamera(nullptr, pos, pos + look, -up);		//Pos Z
			cams[5] = new GraphicsCamera(nullptr, pos, pos - look, -up);		//Neg Z
		}
		else {			
			//TODO:  sort these out.
			cams[0] = new GraphicsCamera(nullptr, pos, pos + right, up);		
			cams[1] = new GraphicsCamera(nullptr, pos, pos - right, up);		
			cams[2] = new GraphicsCamera(nullptr, pos, pos + up, -look);		
			cams[3] = new GraphicsCamera(nullptr, pos, pos - up, look);			
			cams[4] = new GraphicsCamera(nullptr, pos, pos + look, up);			
			cams[5] = new GraphicsCamera(nullptr, pos, pos - look, up);			
		}
		
		
		//Prep
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo_id);
		glViewport(0, 0, size, size);
		cube_map.Bind();

		//Render the scene for all included sub-passes.
		for (int i = 0; i < 6; i++) { RenderFace(window, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cams[i]); }

		////Cleanup
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_fbo);
		if (clear_end) ClearBuffer();
		glViewport(0, 0, (GLsizei)window->width, (GLsizei)window->height);
		for (int i = 0; i < 6; i++) delete cams[i];

		//CHECK_GL_ERROR("GraphicsPassCubeMapping::Execute end", name);		

	}

	void RenderFace(GraphicsWindow* window,  GLenum face, GraphicsCamera* cam) {

		////Render each face of the cube.
		CHECK_FRAMEBUFFER_STATUS("GraphicsPassCubeMmapping::RenderFace");
		cam->SetLens(_render_lens);

		for (auto pass : _pre_passes) {

			GraphicsCamera* oldCam = pass->camera;

			pass->camera = cam;
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, face, cube_map.GetID(), 0);
			pass->Execute(window, &exclusions);
			CHECK_GL_ERROR("GraphicsPassCubeMapping::RenderFace pass->Execute call");

			pass->camera = oldCam;

		}		
		//TODO:  the same for post passes?

		
	}

	virtual bool PassStart(GraphicsWindow* window, cy::GLSLProgram* prog) {
		prog->SetUniformMatrix4(NAME_CAMERA_TRANSFORM, camera->GetLens().data);		//"camTrans"	
		cy::Matrix4f w = cy::Matrix4f(camera->GetWorld().GetSubMatrix3());
		prog->SetUniformMatrix4(NAME_WORLD_TRANSFORM, w.data);
		return true;
	}

	virtual bool SetupObject(GraphicsWindow* window, GraphicsObject* object, cy::GLSLProgram* prog) {
		prog->SetUniformMatrix4(NAME_OBJECT_TRANSFORM, object->GetRelativeTransform().data);	//"objTrans"
		return true;
	}

	virtual void PassEnd(GraphicsWindow* window, cy::GLSLProgram* program) {}

};





/*A simple, bare-bones pass for adding an environment to a scene.  Be careful that the next pass (like the standard pass, etc.) does not immediately clear it out.*/
class GraphicsPassEnvironment : public GraphicsPass {

	GraphicsObjectEnvironment* environment;

public:

	/*Sets up an environment with with the given world's camera.*/
	GraphicsPassEnvironment(GraphicsObjectEnvironment* environment, GraphicsCamera* camera) : GraphicsPass(camera) {
		clear_start = true;
		clear_end = false;
		use_Ztesting = false;
		environment->BufferVertexData();
		GraphicsPass::Include(environment);
	}
	~GraphicsPassEnvironment() { delete environment; }

private:
	virtual bool PassStart(GraphicsWindow* window, cy::GLSLProgram* prog) {
		prog->SetUniformMatrix4(NAME_CAMERA_TRANSFORM, camera->GetLens().data);		//"camTrans"	
		cy::Matrix3f w = camera->GetWorld().GetSubMatrix3();
		cy::Matrix3f v = camera->GetVariableTransform().GetSubMatrix3();
		prog->SetUniformMatrix3("sampleTrans", v.data);
		prog->SetUniformMatrix4(NAME_WORLD_TRANSFORM, cy::Matrix4f(w).data);
		return true;
	}

	virtual bool SetupObject(GraphicsWindow* window, GraphicsObject* object, cy::GLSLProgram* prog) {
		prog->SetUniformMatrix4(NAME_OBJECT_TRANSFORM, object->GetRelativeTransform().data);	//"objTrans"
		return true;
	}

	

	virtual void PassEnd(GraphicsWindow* window, cy::GLSLProgram* program) {}

	virtual bool Include(GraphicsObject* obj) { Throw("Cannot include objects other than the environment in an environment pass.  Inclusions are set at construction."); return false; }

	virtual bool Exclude(GraphicsObject* obj) { Throw("Cannot remove objects from an environment pass."); return false; }
	
};









#endif