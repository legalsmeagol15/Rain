

#ifndef _HELPERS_H	//Not all compilers allow "#pragma once"
#define _HELPERS_H


#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>
#include "cyGL.h"
#include "cyMatrix.h"



void Throw(char* message) { std::cout << "ERROR: " << (message == nullptr ? "<null>" : message) << std::endl;	throw std::exception(message); }
void Throw(char* message0, char* message1) { std::cout << "ERROR: " << (message0 == nullptr ? "<null>" : message0) << (message1 == nullptr ? "<null>" : message1) << std::endl;		throw std::exception(message0); }

/*Sets the rotation around the given center and axis.*/
cy::Matrix4f GetRotation(cy::Point3f center, cy::Point3f axis, float radians) {

	cy::Matrix4f center_compensator;
	center_compensator.SetTrans(-center);

	cy::Matrix4f rotator;
	rotator.SetRotation(axis, radians);

	cy::Matrix4f result = rotator * center_compensator;
	result.AddTrans(center);

	return result;
}

GLenum CHECK_GL_ERROR(char* location) {
	GLenum error = glGetError();
	if (error != 0) {
		std::cout << location << "  OpenGL error:" << error << "  " << glewGetErrorString(error) << std::endl;
	}
	return error;
}

GLenum CHECK_GL_ERROR(char* location, char* detail1) {
	GLenum error = glGetError();
	if (error != 0) {
		std::cout << location << "  " << (detail1==nullptr ? "<null>" : detail1) << "  OpenGL error:" << error << "  " << glewGetErrorString(error) << std::endl;
	}
	return error;
}

GLenum CHECK_GL_ERROR(char* location, char* detail1, char* detail2) {
	GLenum error = glGetError();
	if (error != 0) {		
		std::cout << location << "  " << (detail1 == nullptr ? "<null>" : detail1) << "  " << (detail2 == nullptr ? "<null>" : detail2) << "  OpenGL error:" << error << "  " << glewGetErrorString(error) << std::endl;
	}
	return error;
}

GLenum CHECK_FRAMEBUFFER_STATUS(char* location) {
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << location << "  framebuffer incomplete:" << status << std::endl;
	}
	return status;
}


cy::GLSLProgram* GetComputeShaderProgram(char* filename) {
	
	return nullptr;
}


/*Builds the shaders specified by the given filenames, then compiles the program and registers all related uniform variables.*/
cy::GLSLProgram* GetShaderProgram(char* vertexShader_filename, char* fragmentShader_filename,
	char* geometryShader_filename = "", char* tessellationControlShader_filename = "", char* tessellationEvaluationShader_filename = "")
{

	cy::GLSLProgram* result = new cy::GLSLProgram();

	//Prepare the shaders for this program.
	cy::GLSLShader vertShdr, fragShdr;
	cy::GLSLShader* geomShdr = nullptr;
	cy::GLSLShader* tessControlShdr = nullptr;
	cy::GLSLShader* tessEvalShdr = nullptr;

	if (!vertShdr.CompileFile(vertexShader_filename, GL_VERTEX_SHADER))
		throw std::exception("Cannot find or compile vertex shader file.");
	//std::cout << "Vertex shader compiled." << std::endl;	
	if (!fragShdr.CompileFile(fragmentShader_filename, GL_FRAGMENT_SHADER))
		throw std::exception("Cannot find or compile fragment shader file.");
	//std::cout << "Fragment shader compiled." << std::endl;

	if (geometryShader_filename != "") {
		if (!geomShdr->CompileFile(geometryShader_filename, GL_GEOMETRY_SHADER))
			throw std::exception("Cannot find or compile geometry shader file.");
		//std::cout << "Geometry shader compiled." << std::endl;
	}
	if (tessellationControlShader_filename != "") {
		throw std::exception("not implemented yet.");
	}
	if (tessellationEvaluationShader_filename != "") {
		throw std::exception("not implemented yet.");
	}

	//Build the program.
	if (!result->Build(&vertShdr, &fragShdr, geomShdr, tessControlShdr, tessEvalShdr)) throw std::exception("Shader linking error.");
	std::cout << "OpenGL program built." << std::endl;

	//Get and register the uniforms that are defined in this program.
	//Reference:  http://stackoverflow.com/questions/440144/in-opengl-is-there-a-way-to-get-a-list-of-all-uniforms-attribs-used-by-a-shade
	GLint count;				//The number of uniforms for this program.
	GLint size;					// size of the variable.  This is a throwaway.
	GLenum type;				// type of the variable (float, vec3 or mat4, etc).  This is another throwaway.
	const GLsizei bufSize = 32; // maximum name length.  32 is necessary because I have some long variable names.
	GLchar name[bufSize];		// this is what actually holds the name.
	GLsizei length;				// the length of the name.
	GLuint id = result->GetID();	//The program we're referring to.

	glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &count);	//Schleps the uniforms from openGL.
	for (GLint i = 0; i < count; i++)
	{
		glGetActiveUniform(id, (GLuint)i, bufSize, &length, &size, &type, name);
		//std::cout << "Detected uniform: " << name << std::endl;
		result->RegisterUniform(i, name);
	}



	return result;
}



/*Loads the texture specified byb the given filename into a cy texture object.*/
cy::GLTexture2D* GetTexture(char* filename) {
	std::vector<GLubyte> pixels;
	unsigned int height;
	unsigned int width;

	unsigned int error = lodepng::decode(pixels, width, height, filename);
	if (error > 0) {
		std::cout << "Error loading ambient texture map." << std::endl;
		return nullptr;
	}

	cy::GLTexture2D* result = new cy::GLTexture2D();
	result->Initialize();
	result->SetImage(&pixels[0], 4, width, height, 0);
	result->BuildMipmaps();
	
	return result;
}

/*Loads the texture specified byb the given filename into a cy texture object.*/
cy::GLTextureRect* GetTextureRect(char* filename, unsigned int & width, unsigned int& height) {
	std::vector<GLubyte> pixels;
	
	unsigned int error = lodepng::decode(pixels, width, height, filename);
	if (error > 0) {
		std::cout << "Error loading ambient texture map." << std::endl;
		return nullptr;
	}

	cy::GLTextureRect* result = new cy::GLTextureRect();
	result->Initialize();
	result->SetImage(&pixels[0], 4, width, height, 0);
	
	return result;
}


/*Returns a texture of the indicated size, completely filled with the given color.*/
cy::GLTexture2D* GetSolidTexture(cy::Point4f rgba, int width = 1, int height = 1, int level = 0) {
	cy::GLTexture2D* result = new cy::GLTexture2D();

	result->Initialize();

	std::vector<GLfloat> floats(width * height * 4);
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			int idx = ((y * width) + x) * 4;
			for (int i = 0; i < 4; i++) floats[idx + i] = rgba[i];
		}
	}
	result->SetImageRGBA<GLfloat>(&floats[0], width, height, level);

	return result;
}


/*Returns the normalized vector that is the reflection from the given vector of incidence and the indicated planar normal.*/
cy::Point3f GetReflection(cy::Point3f incidence, cy::Point3f normal) {
	normal.Normalize();	
	return (incidence - (2 * incidence.Dot(normal) * normal)).GetNormalized();
}

#endif