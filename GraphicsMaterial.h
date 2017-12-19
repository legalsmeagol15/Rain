//CS 5610 HW 3 - Shading
///Wesley Oates

#ifndef _GRAPHICS_MATERIAL	//Not all compilers allow "#pragma once"
#define _GRAPHICS_MATERIAL

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
#include "Helpers.h"
#include "wo.h"
#include <unordered_map>
#include <vector>
#include <exception>
#include <iostream>
# define PI				3.14159265358979323846  /* pi, probably don't need all of math.h */


# define NAME_AMBIENT_COLOR				"ambientColor"
# define NAME_DIFFUSE_COLOR				"diffuseColor"
# define NAME_SPECULAR_COLOR			"specularColor"
# define NAME_SPECULAR_EXPONENT			"specularIntensity"
# define NAME_AMBIENT_TEXTURE			"ambientTexture"
# define NAME_DIFFUSE_TEXTURE			"diffuseTexture"
# define NAME_SPECULAR_TEXTURE			"specularTexture"
# define NAME_AMBIENT_CUBE_MAP			"ambientCubeMap"
# define NAME_DIFFUSE_CUBE_MAP			"diffuseCubeMap"
# define NAME_SPECULAR_CUBE_MAP			"specularCubeMap"

class GraphicsObject;



cy::GLSLProgram* standard_shader = nullptr;
cy::GLSLProgram* blinn_shader = nullptr;
cy::GLSLProgram* texture_shader = nullptr;
cy::GLSLProgram* texture_rect_shader = nullptr;
cy::GLSLProgram* texture_rect_flat_shader = nullptr;
cy::GLSLProgram* texture_flat_shader = nullptr;
cy::GLSLProgram* cube_map_shader = nullptr;

wo::TextureCubeMap* black_cube_map = nullptr;
cy::GLTexture2D* black_texture = nullptr;


void Initialize_Black_Cube_Map() {
	if (black_cube_map != nullptr) return;
	black_cube_map = new wo::TextureCubeMap(1, 1, (GLubyte)256, false);
}


/*Originally prepared for CS4600 Computer Graphics by  WO.  */
void GetFresnel(const cy::Point3f &vIn, const cy::Point3f &vNormal, const float n1, const float n2,
	cy::Point3f &reflection, cy::Point3f &refraction, float &reflectWeight, float &refractWeight) {

	// Getting the reflection is easy.
	reflection = GetReflection(vIn, vNormal);
	reflection.Normalize();

	float nRatio = n1 / n2;
	float cos_t_i = vIn.Dot(vNormal);
	float sin2_t_t = (nRatio * nRatio) * (1 - (cos_t_i * cos_t_i));
	if (sin2_t_t > 1.0) {
		//Total internal reflection.
		refraction = cy::Point3f(0, 0, 0);
		reflectWeight = 1.0f;
		refractWeight = 0.0f;
	}
	else {
		//Otherwise, some fraction will be reflectance and some will be transmission.
		refraction = (nRatio * vIn) + (((nRatio * cos_t_i) - sqrtf(1 - sin2_t_t)) * vNormal);
		refraction.Normalize();
		float cos_t_t = vNormal.Dot(refraction);
		float Rperpend = (((n1 * cos_t_i) - (n2 * cos_t_t)) / ((n1 * cos_t_i) + (n2 * cos_t_t)));
		Rperpend *= Rperpend;
		float Rparallel = (((n2 * cos_t_i) - (n1 * cos_t_t)) / ((n2 * cos_t_i) + (n1 * cos_t_t)));
		Rparallel *= Rparallel;
		reflectWeight = (Rperpend + Rparallel) / 2;
		refractWeight = 1.0f - reflectWeight;
	}
}



/*An abstract class used to define the appearance of a GraphicsObjectMesh.*/
class GraphicsMaterial {

	

public:
	
	/*Communicates with OpenGL to set the appearance-related uniforms for this material.*/
	virtual void SetAppearance(cy::GLSLProgram* program, GraphicsObject* obj) = 0;

	/*The shader program to use for this material, unless it is overriden by the hosting GraphicsObjectMesh.*/
	cy::GLSLProgram* shader_program = standard_shader;


protected:


	GraphicsMaterial() {
		

		/*I really wish C++ had a static constructor.*/
		if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
		//if (standard_shader==nullptr) standard_shader = GetShaderProgram("Shaders/standard.vertShdr.txt", "Shaders/standard.fragShdr.txt");
		//if (blinn_shader==nullptr) blinn_shader = GetShaderProgram("Shaders/blinn.vertShdr.txt", "Shaders/blinn.fragShdr.txt");
		
	}
};




/*
3 2 1
4   0
5 6 7
*/
cy::Point2f const cardinals_f[8] = { { 1,0 },
										{ 1 / sqrtf(2.0f), -1 / sqrtf(2.0f) },
										{ 0,-1 },
										{ -1 / sqrtf(2.0f),-1 / sqrtf(2.0f) },
										{ -1,0 },
										{ -1 / sqrtf(2.0f),1 / sqrtf(2.0f) },
										{ 0,1 },
										{ 1 / sqrtf(2.0f),1 / sqrtf(2.0f) } };
cy::Point2f const cardinals_i[8] = { { 1,0 },
										{ 1,-1 },
										{ 0,-1 },
										{ -1,-1 },
										{ -1,0 },
										{ -1,1 },
										{ 0,1 },
										{ 1,1 } };


cy::GLSLProgram* water_shader = nullptr;
class GraphicsMaterialWaterSurface : public GraphicsMaterial {

public:

	cy::Point3f water_color = cy::Point3f(0,0,1);
	cy::Point3f specular_color = cy::Point3f(1,1,1);

	const GLuint water_surface_id = 0;
	const GLuint water_bed_id = 0;
	const GLuint water_environment_id = 0;

	int width = 100;
	int height = 100;
	float depth = 10.0f;
	float clarity = 0.95f;	//How much light from below is occluded per distance unit?
	float refractive_index = 1.330f;	//The refractive index of water.  Not bothering for the index of air.

private:

	
	virtual void SetAppearance(cy::GLSLProgram* program, GraphicsObject* object) {

		CHECK_GL_ERROR("check");

		program->SetUniform("waterColor", water_color);
		program->SetUniform("waterClarity", clarity);
		program->SetUniform(NAME_SPECULAR_COLOR, specular_color);
		program->SetUniform1(NAME_SPECULAR_EXPONENT, 1, &specular_exponent);
		program->SetUniform("refractionIndex", refractive_index);
		program->SetUniform("width", width);
		program->SetUniform("height", height);
		program->SetUniform("depth", depth);

		//Now, if any textures must be bound, do that now.
		if (water_surface_id != 0) {
			int id = 0;
			glActiveTexture(GL_TEXTURE0 + id);
			glBindTexture(GL_TEXTURE_2D, water_surface_id);
			program->SetUniform("waterSurface", id);
		}
		else {
			int id = 0;
			if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
			black_texture->Bind(id);
			program->SetUniform("waterSurface", id);
		}
		CHECK_GL_ERROR("check");

		if (water_bed_id != 0) {
			int id = 1;
			glActiveTexture(GL_TEXTURE0 + id);
			glBindTexture(GL_TEXTURE_2D, water_bed_id);
			program->SetUniform("waterBed", id);
		}
		else {
			int id = 1;
			if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
			black_texture->Bind(id);
			program->SetUniform("waterBed", id);
		}
		CHECK_GL_ERROR("check");

		if (water_environment_id != 0) {
			int id = 2;
			glActiveTexture(GL_TEXTURE0 + id);
			glBindTexture(GL_TEXTURE_CUBE_MAP, water_environment_id);
			program->SetUniform("waterEnvironment", id);
		}
		else {
			int id = 2;
			if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
			black_texture->Bind(id);
			program->SetUniform("waterEnvironment", id);
		}
		CHECK_GL_ERROR("check");

	}

public:

	const float specular_exponent = 150.0f;	

	GraphicsMaterialWaterSurface(GLuint waterSurfaceID, GLuint waterBedID = 0, GLuint waterEnvironmentID = 0, float specularExponent = 150.0f)
		: water_surface_id(waterSurfaceID), water_bed_id(waterBedID), water_environment_id(waterEnvironmentID), specular_exponent(specularExponent)
	{
		if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
		if (water_shader == nullptr) water_shader = GetShaderProgram("SHADERS/waterSurface.vertShdr.txt", "SHADERS/waterSurface.fragShdr.txt");
		shader_program = water_shader;

		//Set up the data input.
	}

	
};




class GraphicsMaterialCubeMap : public GraphicsMaterial {

	cy::Point3f ambient_color;
	cy::Point3f diffuse_color;
	cy::Point3f specular_color;

	wo::TextureCubeMap* ambient_cube_map;
	wo::TextureCubeMap* diffuse_cube_map;
	wo::TextureCubeMap* specular_cube_map;

	
	float specular_exponent = 150.0f;

	virtual void SetAppearance(cy::GLSLProgram* program, GraphicsObject* obj) {
		program->SetUniform(NAME_AMBIENT_COLOR, ambient_color);
		program->SetUniform(NAME_DIFFUSE_COLOR, diffuse_color);
		program->SetUniform(NAME_SPECULAR_COLOR, specular_color);
		program->SetUniform1(NAME_SPECULAR_EXPONENT, 1, &specular_exponent);
		program->SetUniform("center", center);


		//If any cube maps must be bound, do that now.
		if (ambient_cube_map != nullptr) {
			int id = 0;
			ambient_cube_map->Bind(id);
			program->SetUniform(NAME_AMBIENT_CUBE_MAP, id);
		}
		else {
			int id = 0;
			black_cube_map->Bind(id);
			program->SetUniform(NAME_AMBIENT_CUBE_MAP, id);
		}

		if (diffuse_cube_map != nullptr) {
			int id = 1;
			diffuse_cube_map->Bind(id);
			program->SetUniform(NAME_DIFFUSE_CUBE_MAP, id);
		}
		else {
			int id = 1;
			black_cube_map->Bind(id);
			program->SetUniform(NAME_DIFFUSE_CUBE_MAP, id);
		}

		if (specular_cube_map != nullptr) {
			int id = 2;
			specular_cube_map->Bind(id);
			program->SetUniform(NAME_SPECULAR_CUBE_MAP, id);
		}
		else {
			int id = 2;
			black_cube_map->Bind(id);
			program->SetUniform(NAME_SPECULAR_CUBE_MAP, id);
		}

	}
public:
	cy::Point3f center;


	GraphicsMaterialCubeMap(wo::TextureCubeMap* cubeMap) : GraphicsMaterial(), diffuse_cube_map(cubeMap), center(cy::Point3f(0,0,0)) {
		if (black_cube_map == nullptr) 
			Initialize_Black_Cube_Map();
		if (cube_map_shader == nullptr)
			cube_map_shader = GetShaderProgram("Shaders/cube_map.vertShdr.txt", "Shaders/cube_map.fragShdr.txt");
		shader_program = cube_map_shader;
	}
	GraphicsMaterialCubeMap(wo::TextureCubeMap* ambientMap, wo::TextureCubeMap* diffuseMap, wo::TextureCubeMap* specularMap, float specularExponent = 150.0f) : GraphicsMaterialCubeMap(diffuseMap) {
		ambient_cube_map = ambientMap;
		specular_cube_map = specularMap; 
		specular_exponent = specularExponent;
	}


};




class GraphicsMaterialBlinn : public GraphicsMaterial {



	virtual void SetAppearance(cy::GLSLProgram* program, GraphicsObject* object) {

		program->SetUniform(NAME_AMBIENT_COLOR, ambient_color);
		program->SetUniform(NAME_DIFFUSE_COLOR, diffuse_color);
		program->SetUniform(NAME_SPECULAR_COLOR, specular_color);
		program->SetUniform1(NAME_SPECULAR_EXPONENT, 1, &specular_exponent);


		//Now, if any textures must be bound, do that now.
		if (ambient_texture != nullptr) {
			//int id =material->ambient_texture->GetID();
			int id = 0;
			ambient_texture->Bind(id);
			program->SetUniform(NAME_AMBIENT_TEXTURE, id);
		}
		else {
			int id = 0;
			//int  id = black_texture->GetID();
			if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
			black_texture->Bind(id);
			program->SetUniform(NAME_AMBIENT_TEXTURE, id);
		}

		if (diffuse_texture != nullptr) {
			//int id = material->diffuse_texture->GetID();
			int id = 1;
			diffuse_texture->Bind(id);
			program->SetUniform(NAME_DIFFUSE_TEXTURE, id);
		}
		else {
			int id = 1;
			if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
			black_texture->Bind(id);
			program->SetUniform(NAME_DIFFUSE_TEXTURE, id);
		}

		if (specular_texture != nullptr) {
			//int id = material->specular_texture->GetID();
			int id = 2;
			specular_texture->Bind(id);
			program->SetUniform(NAME_SPECULAR_TEXTURE, id);
		}
		else {
			int id = 2;
			if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
			black_texture->Bind(id);
			program->SetUniform(NAME_SPECULAR_TEXTURE, id);
		}
	}

public:

	cy::Point3f ambient_color;
	cy::Point3f diffuse_color;
	cy::Point3f specular_color;	

	cy::GLTexture2D* ambient_texture;
	cy::GLTexture2D* diffuse_texture;
	cy::GLTexture2D* specular_texture;

	float specular_exponent = 150.0f;

	GraphicsMaterialBlinn(cy::Point3f ambientColor, cy::Point3f diffuseColor, cy::Point3f specularColor, float specularExponent = 150.0f) 
		: GraphicsMaterial(), ambient_color(ambientColor), diffuse_color(diffuseColor), specular_color(specularColor), specular_exponent(specularExponent) {

		if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
		if (standard_shader == nullptr) standard_shader = GetShaderProgram("Shaders/blinn.vertShdr.txt", "Shaders/blinn.fragShdr.txt");
		shader_program = standard_shader;
	}

	GraphicsMaterialBlinn(char* ambientTexture_filename, char* diffuseTexture_filename, char* specularTexture_filename, float specularExponent = 150.0f)
		: GraphicsMaterial(), specular_exponent(specularExponent)
	{

		if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
		shader_program = standard_shader;

		if (ambientTexture_filename != "") ambient_texture = GetTexture(ambientTexture_filename);
		if (diffuseTexture_filename != "") diffuse_texture = GetTexture(diffuseTexture_filename);
		if (specularTexture_filename != "") specular_texture = GetTexture(specularTexture_filename);
	}

	GraphicsMaterialBlinn(cy::GLTexture2D* ambientTexture, cy::GLTexture2D* diffuseTexture, cy::GLTexture2D* specularTexture, float specularExponent = 150.0f)
		: GraphicsMaterial(), ambient_texture(ambientTexture), diffuse_texture(diffuseTexture), specular_texture(specularTexture), specular_exponent(specularExponent)
	{
		if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
		shader_program = standard_shader;
	}


	static GraphicsMaterialBlinn* Basic() { return new GraphicsMaterialBlinn(cy::Point3f(1, 0, 0), cy::Point3f(0, 1, 0), cy::Point3f(0, 0, 1)); }

};



template <GLenum TEXTURE_TYPE>
class GraphicsMaterialTexture : public GraphicsMaterial {

private:

	GLuint ambient_texture_id = 0;
	GLuint diffuse_texture_id = 0;
	GLuint specular_texture_id = 0;

	int width;		//Used only for GL_TEXTURE_RECTANGLE
	int height;		//Used only for GL_TEXTURE_RECTANGLE

	bool is_flat;

	virtual void SetAppearance(cy::GLSLProgram* program, GraphicsObject* object) {

		CHECK_GL_ERROR("check");

		//Now, if any textures must be bound, do that now.
		if (ambient_texture_id != 0) {
			int id = 0;
			glActiveTexture(GL_TEXTURE0 + id);
			glBindTexture(TEXTURE_TYPE, ambient_texture_id);
			program->SetUniform(NAME_AMBIENT_TEXTURE, id);
		}
		else {
			int id = 0;
			//int  id = black_texture->GetID();
			if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
			black_texture->Bind(id);
			program->SetUniform(NAME_AMBIENT_TEXTURE, id);
		}
		CHECK_GL_ERROR("check");
		if (diffuse_texture_id != 0) {
			int id = 1;
			glActiveTexture(GL_TEXTURE0 + id);
			glBindTexture(TEXTURE_TYPE, diffuse_texture_id);
			program->SetUniform(NAME_DIFFUSE_TEXTURE, id);
		}
		else {
			int id = 1;
			if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
			black_texture->Bind(id);
			program->SetUniform(NAME_DIFFUSE_TEXTURE, id);
		}
		CHECK_GL_ERROR("check");

		if (specular_texture_id != 0) {
			//int id = material->specular_texture->GetID();
			int id = 2;
			glActiveTexture(GL_TEXTURE0 + id);
			glBindTexture(TEXTURE_TYPE, specular_texture_id);
			program->SetUniform(NAME_SPECULAR_TEXTURE, id);
			program->SetUniform1(NAME_SPECULAR_EXPONENT, 1, &specular_exponent);
		}
		else {
			int id = 2;
			if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
			black_texture->Bind(id);
			program->SetUniform(NAME_SPECULAR_TEXTURE, id);
		}
		CHECK_GL_ERROR("check");
		
		program->SetUniform1("width", 1, &width);
		program->SetUniform1("height", 1, &height);
		
	}

public:

	int GetWidth() { return width; }
	int GetHeight() { return height; }
	
	float specular_exponent = 150.0f;

	GraphicsMaterialTexture(cy::GLTexture2D* ambient_buffer, cy::GLTexture2D* diffuse_buffer, cy::GLTexture2D* specular_buffer, float specularExponent = 150.0f, bool isFlat = false)
		: GraphicsMaterialTexture(0,0,0,specularExponent, isFlat) {

		if (ambient_buffer != nullptr) ambient_texture_id = ambient_buffer->GetID();
		if (diffuse_buffer != nullptr) diffuse_texture_id = diffuse_buffer->GetID();
		if (specular_buffer != nullptr) specular_texture_id = specular_buffer->GetID();
	}

	GraphicsMaterialTexture(cy::GLTextureRect* ambient_buffer, cy::GLTextureRect* diffuse_buffer, cy::GLTextureRect* specular_buffer, int width, int height, float specularExponent = 150.0f, bool isFlat = false)
		: GraphicsMaterialTexture((GLuint)0, (GLuint)0, (GLuint)0, (float)specularExponent, isFlat) {

		this->width = width;
		this->height = height;
		if (ambient_buffer != nullptr) ambient_texture_id = ambient_buffer->GetID();
		if (diffuse_buffer != nullptr) diffuse_texture_id = diffuse_buffer->GetID();
		if (specular_buffer != nullptr) specular_texture_id = specular_buffer->GetID();
	}

	GraphicsMaterialTexture(GLuint ambientTextureID, GLuint diffuseTextureID, GLuint specularTextureID, unsigned int width, unsigned int height, float specularExponent, bool isFlat = false)
		: GraphicsMaterialTexture(ambientTextureID, diffuseTextureID, specularTextureID, specularExponent,  isFlat)
	{
		this->width = width;
		this->height = height;
		if (TEXTURE_TYPE != GL_TEXTURE_RECTANGLE)
			throw std::exception("Incorrect constructor.  This constructor should only be used for GL_TEXTURE_RECTANGLE.  No other texture needs a height or width, because all such dimensions are normalized.");
	}

	GraphicsMaterialTexture(GLuint ambientTextureID, GLuint diffuseTextureID, GLuint specularTextureID, float specularExponent, bool isFlat)
		:GraphicsMaterial(), ambient_texture_id(ambientTextureID), diffuse_texture_id(diffuseTextureID), specular_texture_id(specularTextureID), specular_exponent(specularExponent), is_flat(isFlat)
	{
		
		if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));

		if (TEXTURE_TYPE == GL_TEXTURE_RECTANGLE) {
			if (isFlat) {
				if (texture_rect_flat_shader == nullptr) texture_rect_flat_shader = GetShaderProgram("Shaders/textureOnlyRectFlat.vertShdr.txt", "Shaders/textureOnlyRectFlat.fragShdr.txt");
				shader_program = texture_rect_flat_shader;
			}
			else{
				if (texture_rect_shader == nullptr) texture_rect_shader = GetShaderProgram("Shaders/textureOnlyRect.vertShdr.txt", "Shaders/textureOnlyRect.fragShdr.txt");
				shader_program = texture_rect_shader;
			}			
		}
		else {
			if (isFlat) {
				throw std::exception("Not implemented yet.");
			}
			else {
				if (texture_shader == nullptr) texture_shader = GetShaderProgram("Shaders/textureOnly.vertShdr.txt", "Shaders/textureOnly.fragShdr.txt");
				shader_program = texture_shader;
			}
			
		}
		
	}

};



class GraphicsMaterialBuffer : public GraphicsMaterial {


	cy::GLRenderTexture2D* ambient_buffer = nullptr;
	cy::GLRenderTexture2D* diffuse_buffer = nullptr;
	cy::GLRenderTexture2D* specular_buffer = nullptr;

	virtual void SetAppearance(cy::GLSLProgram* program, GraphicsObject* obj) {

		int id = 0;
		if (ambient_buffer != nullptr) {
			ambient_buffer->BindTexture(id);
			program->SetUniform(NAME_AMBIENT_TEXTURE, id++);
		}
		else {
			black_texture->Bind(id);
			program->SetUniform(NAME_AMBIENT_TEXTURE, id++);
		}

		if (diffuse_buffer != nullptr) {
			diffuse_buffer->BindTexture(id);
			program->SetUniform(NAME_DIFFUSE_TEXTURE, id++);
		}
		else {
			black_texture->Bind(id);
			program->SetUniform(NAME_DIFFUSE_TEXTURE, id++);
		}

		if (specular_buffer != nullptr) {
			specular_buffer->BindTexture(id);
			program->SetUniform1(NAME_SPECULAR_EXPONENT, 1, &specular_exponent);
			program->SetUniform(NAME_SPECULAR_TEXTURE, id++);
		}
		else {
			black_texture->Bind(id);
			program->SetUniform(NAME_SPECULAR_TEXTURE, id++);
		}

	}

public:

	float specular_exponent = 150.0f;

	GraphicsMaterialBuffer(cy::GLRenderTexture2D* ambient_buffer, cy::GLRenderTexture2D* diffuse_buffer, cy::GLRenderTexture2D* specular_buffer)
		: GraphicsMaterial(), ambient_buffer(ambient_buffer), diffuse_buffer(diffuse_buffer), specular_buffer(specular_buffer) {

		if (black_texture == nullptr) black_texture = GetSolidTexture(cy::Point4f(0, 0, 0, 0));
		if (texture_shader == nullptr) texture_shader = GetShaderProgram("Shaders/textureOnly.vertShdr.txt", "Shaders/textureOnly.fragShdr.txt");
		shader_program = texture_shader;
	}

};



cy::GLSLProgram* emissive_program = nullptr;
/*A material that portrays an emissive substance, like neon lighting.  */
class GraphicsMaterialEmissive : public GraphicsMaterial {
public:

	cy::Point3f color;
	float brightness;

	GraphicsMaterialEmissive(cy::Point3f color, float brightness = 1.0f) : color(color), brightness(brightness) {
		if (emissive_program == nullptr) emissive_program = GetShaderProgram("Shaders/emissive.vertShdr.txt", "Shaders/emissive.fragShdr.txt");
		shader_program = emissive_program;
	}

	virtual void SetAppearance(cy::GLSLProgram* program, GraphicsObject* obj) {
		program->SetUniform("emissiveColor", color);
		program->SetUniform1("emissiveBrightness", 1, &brightness);
	}
};




#endif