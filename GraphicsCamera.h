///CS 5610 HW 3 - Shading
///Wesley Oates

#ifndef _GRAPHICS_CAMERA	//Not all compilers allow "#pragma once"
#define _GRAPHICS_CAMERA

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
#include "GraphicsWindow.h"
#include "GraphicsObject.h"


#include <unordered_map>
#include <vector>
#include <exception>
# define PI				3.14159265358979323846  /* pi, probably don't need all of math.h */
# define DEFAULT_FIELD_OF_VIEW		(PI/4)
# define DEFAULT_ASPECT_RATIO		1.0f
# define DEFAULT_NEAR_PLANE			0.001f
# define DEFAULT_FAR_PLANE			10000.0f

class GraphicsWindow;

/*An object used for tracking the world and camera tranformations of a 3D scene, relative to a particular camera's orientation and position.*/
class GraphicsCamera {

public:
	/*The window associated with this camera.  If no window is associated, changes to this camera will not notify openGL to update.*/
	GraphicsWindow* window = nullptr;

private:
	

	cy::Matrix4f _world_transform;
	cy::Matrix4f _world_inverse;
	cy::Matrix4f _lens;
	cy::Matrix4f _lens_inverse;
	cy::Matrix4f _total_transform;
	cy::Matrix4f _total_transform_inverse;

	cy::Matrix4f _variable_transform;

	cy::Point3f _position;
	cy::Point3f _look_direction;	//TODO:  correctly store 
	cy::Point3f _up_direction;		//TODO:  correctly store 
	cy::Point3f _right_direction;

	
	cy::Matrix4f Get_Inverse_Transpose(cy::Matrix4f original) { return original.GetTranspose().GetInverse(); }

	/*With a pre-defined world, updates the cached inverse of the world, and the total and inverse total.  Then, if there is an associated window, notifies of the need to redisplay.*/
	void Update(bool notifyChanged = true) {		
		_world_inverse = Get_Inverse_Transpose(_world_transform);
		
		_total_transform = _lens * _variable_transform * _world_transform;
		_total_transform_inverse = Get_Inverse_Transpose(_total_transform);

		

		_is_world_valid = TestIsValid(&_world_transform) && TestIsValid(&_world_inverse);
		_is_total_valid = TestIsValid(&_total_transform) && TestIsValid(&_total_transform_inverse);

		/*if (notifyChanged && window != nullptr && (glutGet(GLUT_INIT_STATE) == 1))
			glutPostRedisplay();*/
	}

	bool _is_world_valid = false;
	bool _is_lens_valid = false;
	bool _is_total_valid = false;

	bool TestIsValid(cy::Matrix4f* matrix) { for (int i = 0; i < 16; i++) { float value = matrix[0][i]; if (std::isnan(value)) return false; } return true; }

public:
	GraphicsCamera(GraphicsWindow* window) : GraphicsCamera(window, cy::Point3f(0, 0, 0), cy::Point3f(0, 0, -1), cy::Point3f(0, 1, 0)) { }

	/*Creates a new camera, finding the normalized look direction and up direction.*/
	GraphicsCamera(GraphicsWindow* window, cy::Point3f cameraLocation, cy::Point3f cameraTarget, cy::Point3f upDirection) {
		_lens.SetPerspective((float)DEFAULT_FIELD_OF_VIEW, (float)DEFAULT_ASPECT_RATIO, (float)DEFAULT_NEAR_PLANE, (float)DEFAULT_FAR_PLANE);
		_lens_inverse = Get_Inverse_Transpose(_lens);
		_is_lens_valid = TestIsValid(&_lens) && TestIsValid(&_lens_inverse);

		_variable_transform.SetIdentity();
		SetLookAt(cameraLocation, cameraTarget, upDirection);
	}

	/*Copy constructor.*/
	GraphicsCamera(const GraphicsCamera &orig) {
		this->_world_transform = orig._world_transform;
		this->_world_inverse = orig._world_inverse;
		this->_lens = orig._lens;
		this->_lens_inverse = orig._lens_inverse;
		this->_total_transform = orig._total_transform;
		this->_total_transform_inverse = orig._total_transform_inverse;
		this->_position = orig._position;
		this->_look_direction = orig._look_direction;
		this->_up_direction = orig._up_direction;
		this->_right_direction = orig._right_direction;

		this->_is_lens_valid = orig._is_lens_valid;
		this->_is_world_valid = orig._is_world_valid;
		this->_is_total_valid = orig._is_total_valid;
	}

	/*Returns a value indicating whether all matrix transforms are valid.*/
	bool GetIsValid() { return _is_lens_valid && _is_world_valid && _is_total_valid; }

	/*Sets the camera's view transformation (or "lens") to a perspective transformation with the given characteristics.*/
	void SetPerspective(float field_of_view = DEFAULT_FIELD_OF_VIEW, float aspect_ratio = DEFAULT_ASPECT_RATIO, float near_plane = DEFAULT_NEAR_PLANE, float far_plane = DEFAULT_FAR_PLANE) {
		_lens.SetPerspective(field_of_view, aspect_ratio, near_plane, far_plane);
		_lens_inverse = Get_Inverse_Transpose(_lens);
		_is_lens_valid = TestIsValid(&_lens) && TestIsValid(&_lens_inverse);
		Update();
	}

	void SetLens(cy::Matrix4f lens) {
		_lens = lens;
		_lens_inverse = Get_Inverse_Transpose(_lens);
		_is_lens_valid = TestIsValid(&_lens) && TestIsValid(&_lens_inverse);
		Update();
	}

	/*Sets the camera's position as indicated.*/
	void SetPosition(float x, float y, float z) { SetPosition(cy::Point3f(x, y, z)); }
	/*Sets the camera's position as indicated.*/
	void SetPosition(cy::Point3f position) { SetLookAt(position, position + _look_direction, _up_direction); }

	void SetVariableTransform(cy::Matrix4f newVariableTransform, bool notifyChanged = false) { _variable_transform = newVariableTransform; Update(notifyChanged); }
	void SetVariableIdentity() { _variable_transform.SetIdentity(); Update(); }

	/*Sets the camera's world transform to the given look-at transformation.*/
	void SetLookAt(cy::Point3f cameraLocation, cy::Point3f target, cy::Point3f upDirection) {
		upDirection.Normalize();
		cy::Point3f lookDir = (target - cameraLocation).GetNormalized();

		_world_transform = CreateLookAtTransform(cameraLocation, target, upDirection);		
		
		_position = cameraLocation;
		_look_direction = lookDir;
		_right_direction = upDirection.Cross(_look_direction).GetNormalized();
		_up_direction = _look_direction.Cross(_right_direction).GetNormalized();
		Update();		
	}

	/*Sets the look direction of this camera.  TODO:  if look direction is parallel to up direction?*/
	void SetLookDirection(cy::Point3f lookDirection) { SetLookAt(_position, _position + lookDirection, _up_direction);  }
	/*Sets the up direction of this camera.  The camera's actual up direction will align to the given direction as close as possible.  TODO:  if look direction is parallel to up direction?*/	
	void SetUpDirection(cy::Point3f upDirection) { SetLookAt(_position, _position + _look_direction, upDirection);  }

	/*Returns the total transformation represented by this camera.  This will include the lens, the variable transform, and the world transform.*/
	cy::Matrix4f GetTransform() { return _total_transform; }

	/*Returns the inverse of the total transformation represented by this camera.*/
	cy::Matrix4f GetTransformInverse() { return _total_transform_inverse; }

	/*Returns the variable transform of this camera.*/
	cy::Matrix4f GetVariableTransform() { return _variable_transform; }

	/*Returns the variable transformation times the world transformation.*/
	cy::Matrix4f GetVariableWorld() { return _variable_transform * _world_transform; }

	/*Returns the world transformation of this camera.*/
	cy::Matrix4f GetWorld() { return _world_transform; }

	/*Returns the inverse of the world transformation of this camera.*/
	cy::Matrix4f GetWorldInverse() { return _world_inverse; }

	/*Returns the view transformation of this camera (i.e. perspective, orthographic, etc.)*/
	cy::Matrix4f GetLens() { return _lens; }

	/*Returns the inverse of the view transformation of this camera (i.e., perspective, orthographics, etc.)*/
	cy::Matrix4f GetLensInverse() { return _lens_inverse; }

	/*Returns the position of this camera.*/
	cy::Point3f GetPosition() { return _position; }

	/*Returns the normalized look direction of this camera.*/
	cy::Point3f GetLookDirection() { return _look_direction; }

	/*Returns the given normalized up direction of this camera.*/
	cy::Point3f GetUpDirection() { return _up_direction; }

	cy::Point3f GetRightDirection() { return _right_direction; }

	void Pitch(float radiansUp) {
		cy::Matrix3f rotator;
		rotator.SetRotationX(-radiansUp);
		SetLookAt(_position, _position + (rotator * _look_direction), rotator * _up_direction);
	}

	void Yaw(float radiansRight) {
		cy::Matrix3f rotator;
		rotator.SetRotationY(-radiansRight);
		SetLookAt(_position, _position + (rotator * _look_direction), rotator * _up_direction);
	}

	void Roll(float radiansClockwise) {
		cy::Matrix3f rotator;
		rotator.SetRotationZ(-radiansClockwise);
		SetLookAt(_position, _position + (rotator * _look_direction), rotator * _up_direction);
	}


	/*Returns a look-at transformation matrix with the given camera location, target, and up direction.*/
	static cy::Matrix4f CreateLookAtTransform(cy::Point3f cameraLocation, cy::Point3f target, cy::Point3f upDirection) {
		cy::Matrix4f result;
		result.SetIdentity();
		target = cameraLocation + (target - cameraLocation).GetNormalized();
		upDirection.Normalize();
		result.SetView(cameraLocation, target, upDirection);
		return result;
	}

	static GraphicsCamera* CreateRightward(GraphicsCamera* original) { return new GraphicsCamera(nullptr, original->GetPosition(), original->GetPosition() + original->GetRightDirection(), original->GetUpDirection()); }

	static GraphicsCamera* CreateLeftward(GraphicsCamera* original) { return new GraphicsCamera(nullptr, original->GetPosition(), original->GetPosition() - original->GetRightDirection(), original->GetUpDirection()); }

	static GraphicsCamera* CreateDownward(GraphicsCamera* original) { return new GraphicsCamera(nullptr, original->GetPosition(), original->GetPosition() - original->GetUpDirection(), original->GetLookDirection()); }

	static GraphicsCamera* CreateUpward(GraphicsCamera* original) { return new GraphicsCamera(nullptr, original->GetPosition(), original->GetPosition() + original->GetUpDirection(), -original->GetLookDirection()); }

	static GraphicsCamera* CreateBackward(GraphicsCamera* original) { return new GraphicsCamera(nullptr, original->GetPosition(), original->GetPosition() - original->GetLookDirection(), original->GetUpDirection()); }

	static GraphicsCamera* CreateCopy(GraphicsCamera* original) { return new GraphicsCamera(nullptr, original->GetPosition(), original->GetPosition() + original->GetLookDirection(), original->GetUpDirection()); }

	void CopyTo(GraphicsCamera* other) {
		other->_world_transform = _world_transform;
		other->_world_inverse = _world_inverse;
		other->_lens = _lens;
		other->_lens_inverse = _lens_inverse;
		other->_total_transform = _total_transform;
		other->_total_transform_inverse = _total_transform_inverse;
		other->_position = _position;
		other->_look_direction = _look_direction;
		other->_up_direction = _up_direction;
		other->_right_direction = _right_direction;

		other->_is_lens_valid = _is_lens_valid;
		other->_is_world_valid = _is_world_valid;
		other->_is_total_valid = _is_total_valid;
	}
};

#endif