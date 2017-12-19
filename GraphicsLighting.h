///CS 5610 HW 3 - Shading
///Wesley Oates

#ifndef _GRAPHICS_LIGHTING	//Not all compilers allow "#pragma once"
#define _GRAPHICS_LIGHTING

#include <GL/glew.h>
#include <GL/freeglut.h>

#include "cytrimesh.h"
#include "cyGL.h"
#include "cyMatrix.h"
#include "GraphicsCamera.h"


# define NAME_LIGHT_POSITION			"lightPosition"
# define NAME_LIGHT_COLOR				"lightColor"
# define NAME_AMBIENT_LIGHT				"ambientLight"


class GraphicsLighting {

	

public:

	cy::Point3f ambient_light;

	virtual void SetLighting(GraphicsCamera* camera, cy::GLSLProgram* program) = 0;

protected:
	GraphicsLighting(cy::Point3f ambientLight) : ambient_light(ambientLight) {}

};

class GraphicsLightPoint : public GraphicsLighting {


public:
	cy::Point3f light_position;
	cy::Point3f light_color;

	GraphicsObject* light_representation;

	GraphicsLightPoint(cy::Point3f lightPosition, cy::Point3f lightColor, GraphicsObject* lightRepresentation = nullptr) 
		: GraphicsLighting(cy::Point3f(0,0,0)), light_position(lightPosition), light_color(lightColor),  light_representation(lightRepresentation) {}
	GraphicsLightPoint(cy::Point3f lightPosition, cy::Point3f lightColor, cy::Point3f ambientLight, GraphicsObject* lightRepresentation = nullptr) 
		: GraphicsLighting(ambientLight), light_position(lightPosition), light_color(lightColor), light_representation(lightRepresentation) {}
private:

	virtual void SetLighting(GraphicsCamera* camera, cy::GLSLProgram* program) {

		//TODO:  this will need to be in world coords.
		cy::Point4f homogeneous_light_position = cy::Point4f(light_position, 1);
		homogeneous_light_position = camera->GetVariableWorld() * homogeneous_light_position;	
		cy::Point3f nonHomogeneous_light_position = homogeneous_light_position.GetNonHomogeneous();
		//program->SetUniform(NAME_LIGHT_POSITION, nonHomogeneous_light_position);
		program->SetUniform(NAME_LIGHT_POSITION, light_position);


		if (light_representation != nullptr) light_representation->SetPosition(light_position);

		program->SetUniform(NAME_LIGHT_COLOR, light_color);
		program->SetUniform(NAME_AMBIENT_LIGHT, ambient_light);
	}

};

#endif