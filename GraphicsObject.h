
///CS 5610 HW 3 - Shading
///Wesley Oates

#ifndef _GRAPHICS_OBJECT
#define _GRAPHICS_OBJECT

#include "cytrimesh.h"
#include "cyGL.h"
#include "cyMatrix.h"
#include "lodepng.h"
#include "GraphicsMaterial.h"
#include "Helpers.h"
#include "wo.h"
#include <vector>
#include <exception>
#include <stack>
#include <unordered_map>
# define PI				3.14159265358979323846  /* pi, probably don't need all of math.h */


//#define _BUFFER_COUNT			2
#define _BUFFER_VERTEX_INDEX	0
#define _BUFFER_NORMAL_INDEX	1
#define _BUFFER_TEXTURE_INDEX	2

cy::GLSLProgram* environment_shader = nullptr;


enum Plane2D {
	XZ = 0,
	ZX = 0,
	XY = 1,
	YX = 1,
	YZ = 2,
	ZY = 2
};


class GraphicsObject {
public:
	char* name = "";

	/*The openGL VAO name.*/
	GLuint vao_name = NULL;

protected:

	cy::Matrix4f _rotation_transform;
	cy::Matrix4f _scale_transform;
	cy::Matrix4f _translate_transform;

	cy::Matrix4f _absolute_transform;
	cy::Matrix4f _relative_transform;

	std::vector<GraphicsObject*> _children;
	GraphicsObject* _parent = nullptr;



	/*Updates the cached transformation matrices of this object, according to the stored translate, rotation, and scale.*/
	void Update_Transform() {
		_absolute_transform = _translate_transform * (_rotation_transform * _scale_transform);
		SetTransform(_absolute_transform);
	}
	


	GraphicsObject(cy::GLSLProgram* program = (cy::GLSLProgram*) nullptr) : shader_program(program) {
		_rotation_transform.SetIdentity();
		_scale_transform.SetIdentity();
		_translate_transform.SetIdentity();
		_absolute_transform.SetIdentity();
		_relative_transform.SetIdentity();
	}

	~GraphicsObject() {
		for (GraphicsObject* child : _children) delete child;
	}


	cy::GLSLProgram* shader_program = nullptr;

public:

	/*Updates the cached transformation matrices of this object according to the given transform.*/
	void SetTransform(cy::Matrix4f absoluteTransform) {
		_absolute_transform = absoluteTransform;
		cy::Matrix4f parent_rel_trans;
		if (_parent == nullptr) parent_rel_trans.SetIdentity();
		else parent_rel_trans = _parent->_relative_transform;

		_relative_transform = parent_rel_trans * _absolute_transform;

		//In a depth-first traversal, update the relative transforms of all descendant objects.
		std::stack<GraphicsObject*> workStack;
		for (auto child : _children) workStack.push(child);
		while (!workStack.empty()) {
			GraphicsObject* focus = workStack.top();
			workStack.pop();
			focus->_relative_transform = focus->_parent->_relative_transform * focus->_absolute_transform;
			for (auto child : focus->_children) workStack.push(child);
		}

		//glutPostRedisplay();
	}

	/*Returns the shader program.  Unless overridden in a child class, returns the shader specified at instantiation.*/
	virtual cy::GLSLProgram* GetShader() { return shader_program; }
	
	/*Returns the combined transformation matrix for the rotation, scale, and translation of this object.*/
	cy::Matrix4f GetAbsoluteTransformation() { return _absolute_transform; }

	/*Returns the transformation for this object relative to the root object.*/
	cy::Matrix4f GetRelativeTransform() { return _relative_transform; }

	/*Set the rotation transform as indicated.  The rotation is the first transform applied of the three.*/
	void SetRotation(const float pitch, const float yaw, const float roll) { _rotation_transform.SetRotationXYZ(pitch, yaw, roll);		Update_Transform(); }

	/*Set the translation transform as indicated.  The translation transform is the first transform applied of the three.*/
	void SetPosition(cy::Point3f _point) { _translate_transform.SetTrans(_point); Update_Transform(); }
	/*Set the translation transform as indicated.  The translation transform is the first transform applied of the three.*/
	void SetPosition(const float x, const float y, const float z) { _translate_transform.SetTrans(cy::Point3f(x, y, z));		Update_Transform(); }

	cy::Point3f GetPosition() { return _translate_transform.GetTrans(); }

	/*Set the scale transform as indicated.  The scale transform is the last transform applied of the three.*/
	void SetScale(const float x, const float y, const float z) { _scale_transform.SetScale(x, y, z);		Update_Transform(); }

	/*Buffers necessary data to OpenGL.  Must be overridden in any inheriting class.*/
	virtual bool BufferVertexData() = 0;

	/*Sets necessary uniform variables in the given program.  This method is called immediately prior to drawing the object.  If the method returns 
	false, the object will be skipped by the rendering.  Must be overridden in any inheriting class.*/
	virtual bool SetAppearance(cy::GLSLProgram* program) = 0;

	/*Draws the object in OpenGL.  Must be overridden in any inheriting class.*/
	virtual void Draw() = 0;

};



class GraphicsObjectEnvironment : public GraphicsObject {

private:
	GLuint _vertex_vbo_id;

	wo::TextureCubeMap* cube_map;
	
	void SetupVertices(float size) {
		float halfWidth = size / 2, halfHeight = size / 2, halfDepth = size / 2;
		cy::Point3f pt0 = cy::Point3f(-halfWidth, -halfHeight, halfDepth);
		cy::Point3f pt1 = cy::Point3f(halfWidth, -halfHeight, halfDepth);
		cy::Point3f pt2 = cy::Point3f(halfWidth, -halfHeight, -halfDepth);
		cy::Point3f pt3 = cy::Point3f(-halfWidth, -halfHeight, -halfDepth);

		cy::Point3f pt4 = cy::Point3f(-halfWidth, halfHeight, halfDepth);
		cy::Point3f pt5 = cy::Point3f(halfWidth, halfHeight, halfDepth);
		cy::Point3f pt6 = cy::Point3f(halfWidth, halfHeight, -halfDepth);
		cy::Point3f pt7 = cy::Point3f(-halfWidth, halfHeight, -halfDepth);

		/*cy::Point3f pt0 = cy::Point3f(0, 0, size);
		cy::Point3f pt1 = cy::Point3f(size, 0, size);
		cy::Point3f pt2 = cy::Point3f(size, 0, 0);
		cy::Point3f pt3 = cy::Point3f(0, 0, 0);

		cy::Point3f pt4 = cy::Point3f(0, size, size);
		cy::Point3f pt5 = cy::Point3f(size, size, size);
		cy::Point3f pt6 = cy::Point3f(size, size, 0);
		cy::Point3f pt7 = cy::Point3f(0, size, 0);*/

		//Bottom face
		vertices.push_back(pt0);
		vertices.push_back(pt2);
		vertices.push_back(pt1);

		vertices.push_back(pt3);
		vertices.push_back(pt2);
		vertices.push_back(pt0);

		//Right face
		vertices.push_back(pt1);
		vertices.push_back(pt6);
		vertices.push_back(pt5);

		vertices.push_back(pt1);
		vertices.push_back(pt2);
		vertices.push_back(pt6);


		//Back face
		vertices.push_back(pt7);
		vertices.push_back(pt2);
		vertices.push_back(pt3);

		vertices.push_back(pt7);
		vertices.push_back(pt6);
		vertices.push_back(pt2);

		//Left face
		vertices.push_back(pt4);
		vertices.push_back(pt7);
		vertices.push_back(pt3);

		vertices.push_back(pt0);
		vertices.push_back(pt4);
		vertices.push_back(pt3);

		//Front face
		vertices.push_back(pt0);
		vertices.push_back(pt1);
		vertices.push_back(pt5);

		vertices.push_back(pt0);
		vertices.push_back(pt5);
		vertices.push_back(pt4);

		//Top face
		vertices.push_back(pt4);
		vertices.push_back(pt5);
		vertices.push_back(pt7);

		vertices.push_back(pt7);
		vertices.push_back(pt5);
		vertices.push_back(pt6);

		// Finally, the triangle count.
		triangleCount = 12;
	}

public:

	/*TODO:  delete or hide this, it's only for development purposes.*/
	void SetShader(cy::GLSLProgram* program) { shader_program = program; }

	GraphicsObjectEnvironment(char* posX, char* posY, char* posZ, char* negX, char* negY, char* negZ) 
		: GraphicsObjectEnvironment(wo::TextureCubeMap::FromFiles(posX, posY, posZ, negX, negY, negZ), nullptr) {}

	GraphicsObjectEnvironment(wo::TextureCubeMap* cubeMap, cy::GLSLProgram* program) : GraphicsObject(program), cube_map(cubeMap) {
		if (shader_program==nullptr) shader_program = GetShaderProgram("Shaders/environment.vertShdr.txt", "Shaders/environment.fragShdr.txt");
		SetupVertices(1.0f);
	}

	~GraphicsObjectEnvironment() { delete cube_map; }

	/*The cached vertices.  TODO:  allow for instancing.*/
	std::vector<cy::Point3f> vertices;

	/*The count of triangles stored in this graphics object.*/
	int triangleCount;

	virtual bool BufferVertexData() {

		if (vao_name != NULL) return false;

		//Create the VAO for upload to openGL		
		glGenVertexArrays(1, &vao_name);
		glBindVertexArray(vao_name);

		//Create the VBO for upload to openGL
		glGenBuffers(1, &_vertex_vbo_id);

		//The vertex buffer.
		glBindBuffer(GL_ARRAY_BUFFER, _vertex_vbo_id);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * triangleCount * 3, &vertices[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(_BUFFER_VERTEX_INDEX);
		glVertexAttribPointer(_BUFFER_VERTEX_INDEX, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glDisableVertexAttribArray(_BUFFER_VERTEX_INDEX);

		glBindVertexArray(NULL);

		return true;
	}

	virtual bool SetAppearance(cy::GLSLProgram* program) {

		CHECK_GL_ERROR("GraphicsObjectEnvironment::set_appearance start", name);
		int id = 0;
		cube_map->Bind(id);		
		program->SetUniform("environmentMap", id);
		CHECK_GL_ERROR("GraphicsObjectEnvironment::set_appearance end", name);

		return true;
	}

	virtual void Draw() {

		CHECK_GL_ERROR("GraphicsObjectEnvironment::Draw start", name);

		//Draw the object in openGL
		glBindBuffer(GL_ARRAY_BUFFER, _vertex_vbo_id);
		glEnableVertexAttribArray(_BUFFER_VERTEX_INDEX);

		CHECK_GL_ERROR("GraphicsObjectEnvironment::Draw, about to call glDrawArrays....", name);
		glDrawArrays(GL_TRIANGLES, 0, triangleCount * 3);
		CHECK_GL_ERROR("GraphicsObjectEnvironment::Draw, ....called glDrawArrays", name);

		glDisableVertexAttribArray(_BUFFER_VERTEX_INDEX);
		glBindBuffer(GL_ARRAY_BUFFER, NULL);

		CHECK_GL_ERROR("GraphicsObjectEnvironment::Draw end", name);
	}
};




/*An object that embodies the graphics properties unique to an object that will be added to a GRaphics Manager.*/
class GraphicsObjectMesh : public GraphicsObject{


private:

	GraphicsObjectMesh(cy::GLSLProgram* program = nullptr) : GraphicsObject(program) {	}

public:
	
	
	/*The material used to render this meshed object.  The material defines the appearance.*/
	GraphicsMaterial* material = nullptr;

	/*The set of VBO ids.*/
	GLuint vbo_IDs[3];

	/*The cached vertices.  TODO:  allow for instancing.*/
	std::vector<cy::Point3f> vertices;

	/*The cached normals.  TODO:  allow for instancing.*/
	std::vector<cy::Point3f> normals;

	/*The texture coordinates at each vertex.  TODO:  allow for instancing.*/
	std::vector<cy::Point3f> textureCoordinates;

	/*The caches indices.  TODO:  allow for instancing.*/
	std::vector<GLuint> indices;

	/*The count of triangles stored in this graphics object.*/
	int triangleCount = 0;	

	/*Returns the shader program.  If an override is specified at the object level, will pull from that.  Otherwise, uses the material's shader program.*/
	virtual cy::GLSLProgram* GetShader() { return (shader_program != nullptr) ? shader_program : material->shader_program; }

	
	virtual bool BufferVertexData() {

		if (vao_name != NULL) return false;

		CHECK_GL_ERROR("GraphicsObject::BufferVertexData start", name);

		//Create the VAO for upload to openGL		
		glGenVertexArrays(1, &vao_name);
		glBindVertexArray(vao_name);

		//Create the VBO for upload to openGL
		glGenBuffers(3, vbo_IDs);

		//Close the VAO for new buffers
		//glBindVertexArray(0);		//This damn call appears to have been causing me a LOT of trouble.  Moved it to the end.

		//Buffer #0 - the vertex buffer.
		glBindBuffer(GL_ARRAY_BUFFER, vbo_IDs[_BUFFER_VERTEX_INDEX]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * triangleCount * 3, &vertices[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(_BUFFER_VERTEX_INDEX);
		glVertexAttribPointer(_BUFFER_VERTEX_INDEX, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glDisableVertexAttribArray(_BUFFER_VERTEX_INDEX);

		//Buffer #1 - the normal buffer.
		if (normals.size() >=  vertices.size()) {
			glBindBuffer(GL_ARRAY_BUFFER, vbo_IDs[_BUFFER_NORMAL_INDEX]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(normals[0]) * triangleCount * 3, &normals[0], GL_STATIC_DRAW);
			glEnableVertexAttribArray(_BUFFER_NORMAL_INDEX);
			glVertexAttribPointer(_BUFFER_NORMAL_INDEX, 3, GL_FLOAT, GL_FALSE, 0, 0);
			glDisableVertexAttribArray(_BUFFER_NORMAL_INDEX);
		}


		//Buffer #2 - the texture coordinate buffer 
		if (textureCoordinates.size() >= vertices.size()) {
			glBindBuffer(GL_ARRAY_BUFFER, vbo_IDs[_BUFFER_TEXTURE_INDEX]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(textureCoordinates[0]) * triangleCount * 3, &textureCoordinates[0], GL_STATIC_DRAW);
			glEnableVertexAttribArray(_BUFFER_TEXTURE_INDEX);
			glVertexAttribPointer(_BUFFER_TEXTURE_INDEX, 3, GL_FLOAT, GL_FALSE, 0, 0);
			glDisableVertexAttribArray(_BUFFER_TEXTURE_INDEX);
		}

		glBindVertexArray(NULL);

		CHECK_GL_ERROR("GraphicsObject::BufferVertexData end", name);

		return true;
	}

	virtual bool SetAppearance(cy::GLSLProgram* program) {

		CHECK_GL_ERROR("GraphicsObjectMesh::set_appearance start", name);

		material->SetAppearance(program, this);		

		CHECK_GL_ERROR("GraphicsObjectMesh::set_appearance end", name);

		return true;
	}

	virtual void Draw() {

		CHECK_GL_ERROR("GraphicsObjectMesh::Draw start", name);
		
		//Draw the object in openGL
		glBindBuffer(GL_ARRAY_BUFFER, *vbo_IDs);
		glEnableVertexAttribArray(_BUFFER_VERTEX_INDEX);
		if (normals.size() >= vertices.size()) glEnableVertexAttribArray(_BUFFER_NORMAL_INDEX);
		if (textureCoordinates.size() >= vertices.size()) glEnableVertexAttribArray(_BUFFER_TEXTURE_INDEX);		

		CHECK_GL_ERROR("GraphicsObjectMesh::Draw, about to call glDrawArrays....", name);
		glDrawArrays(GL_TRIANGLES, 0, triangleCount * 3);		
		CHECK_GL_ERROR("GraphicsObjectMesh::Draw, ....called glDrawArrays", name);
		
		glDisableVertexAttribArray(_BUFFER_NORMAL_INDEX);
		glDisableVertexAttribArray(_BUFFER_VERTEX_INDEX);
		glDisableVertexAttribArray(_BUFFER_TEXTURE_INDEX);
		glBindBuffer(GL_ARRAY_BUFFER, NULL);

		CHECK_GL_ERROR("GraphicsObjectMesh::Draw end", name);
	}


	/*Create a graphics object using the given mesh and shader program.*/
	GraphicsObjectMesh(cy::TriMesh* mesh, cy::GLSLProgram* shader_program, bool importMaterial = true) : GraphicsObject(shader_program) {

		
		//Step #2 - Get the per-vertex data.

		triangleCount = mesh->NF();
		for (int i = 0; i < triangleCount; i++) {

			for (int j = 0; j < 3; j++) {
				unsigned int vIdx = mesh->F(i).v[j];
				cy::Point3f vert = mesh->V(vIdx);
				vertices.push_back(vert);

				if (mesh->HasNormals()) {
					unsigned int nIdx = mesh->FN(i).v[j];
					cy::Point3f norm = mesh->VN(nIdx);
					normals.push_back(norm);

				}
				if (mesh->HasTextureVertices()) {
					unsigned int tIdx = mesh->FT(i).v[j];
					cy::Point3f txtCoord = mesh->VT(tIdx);
					textureCoordinates.push_back(txtCoord);
				}

				int idx = (i * 3) + j;
				indices.push_back(idx);
			}
		}

		//Step #3 - get the texture data.
		cy::TriMesh::Mtl* mtl = &mesh->M(0);	
		if (mtl != nullptr && importMaterial) {
			GraphicsMaterialBlinn* gmb = new GraphicsMaterialBlinn(cy::Point3f(mtl->Ka), cy::Point3f(mtl->Kd),  cy::Point3f(mtl->Ks),  mtl->Ns);
			
			if (mtl->map_Ka != "") gmb->ambient_texture = GetTexture(mtl->map_Ka.data);
			if (mtl->map_Kd != "") gmb->diffuse_texture = GetTexture(mtl->map_Kd.data);
			if (mtl->map_Ks != "") gmb->specular_texture = GetTexture(mtl->map_Ks.data);
			material = gmb;
		}
		
	}

	~GraphicsObjectMesh() {}
	/*A quick and dirty hasher for cy::Point3f object, suggested by http://stackoverflow.com/questions/17016175/c-unordered-map-using-a-custom-class-type-as-the-key .*/
	struct Point3fHasher {
		std::size_t operator()(const cy::Point3f& pt) const
		{
			using std::size_t;
			using std::hash;
			return hash<float>()(pt.x + pt.y + pt.z);
		}
	};

	/*The normals of all points which are identical will have their normals blended.*/
	void BlendNormals() {
		std::unordered_map<cy::Point3f, std::vector<int>, Point3fHasher> neighbors;

		size_t maxIndex = vertices.size() > normals.size() ? normals.size() : vertices.size();

		//Get all the matching points.
		for (size_t i = 0; i < maxIndex; i++) {			
			cy::Point3f v = vertices[i];	
			if (neighbors.count(v) == 0) neighbors.emplace(v,std::vector<int>());
			neighbors[v].push_back(i);
		}

		for (auto iter : neighbors) {

			//Get the sum of all the normals that match this point.
			cy::Point3f n = cy::Point3f(0, 0, 0);
			for (int idx : iter.second) n += normals[idx];

			//Set the normals for all vertices that match this point.
			n.Normalize();
			for (int idx : iter.second) normals[idx] = n;
		}
	}

	void AddTriangle(cy::Point3f point0, cy::Point3f point1, cy::Point3f point2, cy::Point3f texMap0,  cy::Point3f texMap1, cy::Point3f texMap2, bool clockWiseNormal = true) {
		AddTriangle(point0, point1, point2, clockWiseNormal);
		textureCoordinates.push_back(texMap0);
		textureCoordinates.push_back(texMap1);
		textureCoordinates.push_back(texMap2);
	}
	void AddTriangle(cy::Point3f point0, cy::Point3f point1, cy::Point3f point2, bool clockWiseNormal = true) {
		vertices.push_back(point0);
		vertices.push_back(point1);
		vertices.push_back(point2);

		cy::Point3f norm = clockWiseNormal ? norm = (point1 - point0).Cross(point2 - point0) : (point0 - point1).Cross(point2 - point1);
		norm.Normalize();
		
		for (int i = 0; i < 3; i++) {
			normals.push_back(norm);
			indices.push_back(indices.size());
		}

		triangleCount++;
	}

	/*Creates a sphere-like globe with the given radius, and the given number of longitude and latitude lines.  The latitude line count excludes one of the poles.*/
	static GraphicsObjectMesh* CreateGlobe(float radius, int latitudeLines, int longitudeLines) { return CreateGlobe(radius, (float)(PI / (float)latitudeLines), (float)((2 * PI) / (float)longitudeLines)); }
	/*Creates a sphere-like globe with the given radius, and the given arc distance between longitude and latitude lines.*/
	static GraphicsObjectMesh* CreateGlobe(float radius, float latitudeArc, float longitudeArc) {

		if (radius <= 0.0f) Throw("Invalidate radius size.");

		GraphicsObjectMesh* globe = new GraphicsObjectMesh(nullptr);

		float phiSouth = latitudeArc, thetaWest = 0.0f;
		float PI_times_2 = (float)PI * 2;

		//Step #1 - the north pole.
		float south = radius * cosf(phiSouth);
		cy::Point3f northPole = cy::Point3f(0, radius, 0);		
		cy::Point3f southPole = cy::Point3f(0, -radius, 0);
		float radiusSouth = radius * sinf(phiSouth);
		cy::Point3f westPt = cy::Point3f(radiusSouth, south, 0);
		while (true) {			
			float thetaEast = thetaWest + longitudeArc;
			if (thetaEast > PI_times_2) thetaEast = PI_times_2;
			cy::Point3f eastPt = cy::Point3f(radiusSouth*cosf(thetaEast), south, radiusSouth*sinf(thetaEast));
			globe->AddTriangle(eastPt, westPt, northPole);		

			if (thetaEast == PI_times_2) break;
			westPt = eastPt;
			thetaWest = thetaEast;
		}

		
		//Step #2 - all the non-polar latitudes in the middle.
		float phiNorth = phiSouth;
		phiSouth += latitudeArc;
		while (phiSouth < PI) {
			float thetaWest = 0.0f;
			float radiusNorth = radius * sinf(phiNorth);
			radiusSouth = radius * sinf(phiSouth);
			cy::Point3f nw_Pt = cy::Point3f(radiusNorth * cosf(thetaWest), radius*cosf(phiNorth), radiusNorth*sinf(thetaWest));
			cy::Point3f sw_Pt = cy::Point3f(radiusSouth * cosf(thetaWest), radius*cosf(phiSouth), radiusSouth*sinf(thetaWest));
			while (true) {				
				float thetaEast = thetaWest + longitudeArc;
				if (thetaEast > PI_times_2) thetaEast = PI_times_2;
				cy::Point3f ne_Pt = cy::Point3f(radiusNorth * cosf(thetaEast), radius*cosf(phiNorth), radiusNorth*sinf(thetaEast));
				cy::Point3f se_Pt = cy::Point3f(radiusSouth * cosf(thetaEast), radius*cosf(phiSouth), radiusSouth*sinf(thetaEast));
				globe->AddTriangle(ne_Pt, se_Pt, sw_Pt);
				globe->AddTriangle(sw_Pt, nw_Pt, ne_Pt);
				if (thetaEast == PI_times_2) break;				
				nw_Pt = ne_Pt;
				sw_Pt = se_Pt;
				thetaWest = thetaEast;
			}

			phiNorth = phiSouth;
			phiSouth += latitudeArc;
		}

		//Step #3 - the south pole.		
		float north = radius * cosf(phiNorth);
		thetaWest = 0.0f;
		radius *= sinf(phiNorth);	//Don't need original radius after this.
		cy::Point3f nw_Pt = cy::Point3f(radius * cosf(thetaWest), north, radius*sinf(thetaWest));
		while (true) {
			float thetaEast = thetaWest + longitudeArc;
			if (thetaEast > PI_times_2) thetaEast = PI_times_2;
			cy::Point3f ne_Pt = cy::Point3f(radius * cosf(thetaEast), north, radius*sinf(thetaEast));
			globe->AddTriangle(nw_Pt, ne_Pt, southPole);
			if (thetaEast == PI_times_2) break;
			nw_Pt = ne_Pt;
			thetaWest = thetaEast;
		}
		

		//Step #4 FINALLY - return the globe.
		return globe;

	}

	/*Creates a box with the given height, width, and depth.*/
	static GraphicsObjectMesh* CreateCube(float height, float width, float depth) {
		GraphicsObjectMesh* result = new GraphicsObjectMesh(nullptr);

		float halfHeight = height / 2, halfWidth = width / 2, halfDepth = depth / 2;

		cy::Point3f pt0 = cy::Point3f(-halfWidth, -halfHeight, halfDepth);
		cy::Point3f pt1 = cy::Point3f(halfWidth, -halfHeight, halfDepth);
		cy::Point3f pt2 = cy::Point3f(halfWidth, -halfHeight, -halfDepth);
		cy::Point3f pt3 = cy::Point3f(-halfWidth, -halfHeight, -halfDepth);

		cy::Point3f pt4 = cy::Point3f(-halfWidth, halfHeight, halfDepth);
		cy::Point3f pt5 = cy::Point3f(halfWidth, halfHeight, halfDepth);
		cy::Point3f pt6 = cy::Point3f(halfWidth, halfHeight, -halfDepth);
		cy::Point3f pt7 = cy::Point3f(-halfWidth, halfHeight, -halfDepth);

		//Bottom face
		result->AddTriangle(pt0, pt2, pt1);
		result->AddTriangle(pt3, pt2, pt0);

		//Right face
		result->AddTriangle(pt1, pt6, pt5);
		result->AddTriangle(pt1, pt2, pt6);

		//Back face
		result->AddTriangle(pt7, pt2, pt3);
		result->AddTriangle(pt7, pt6, pt2);

		//Left face
		result->AddTriangle(pt4, pt7, pt3);
		result->AddTriangle(pt0, pt4, pt3);

		//Front face
		result->AddTriangle(pt0, pt1, pt5);
		result->AddTriangle(pt0, pt5, pt4);

		//Top face
		result->AddTriangle(pt4, pt5, pt7);
		result->AddTriangle(pt7, pt5, pt6);

		return result;

	}


	/*Creates a flat rectangle with the given height and width.*/
	static GraphicsObjectMesh* CreateRectangle(float height, float width, Plane2D plane = XY, bool invert = false) {

		GraphicsObjectMesh* result = new GraphicsObjectMesh(nullptr);
		float halfHeight = height / 2, halfWidth = width / 2;

		cy::Point3f ne, se, nw, sw;
		if (plane == XY) {
			nw = cy::Point3f(-halfWidth, halfHeight,0);
			ne = cy::Point3f(halfWidth,  halfHeight,0);
			sw = cy::Point3f(-halfWidth, -halfHeight,0);
			se = cy::Point3f(halfWidth, -halfHeight,0);
		}
		else if (plane == XZ) {
			nw = cy::Point3f(-halfWidth, 0, halfHeight);
			ne = cy::Point3f(halfWidth, 0, halfHeight);
			sw = cy::Point3f(-halfWidth, 0, -halfHeight);
			se = cy::Point3f(halfWidth, 0, -halfHeight);
		}
		else {	//  YZ
			nw = cy::Point3f(0,-halfWidth,  halfHeight);
			ne = cy::Point3f(0,halfWidth,  halfHeight);
			sw = cy::Point3f(0,-halfWidth, -halfHeight);
			se = cy::Point3f(0,halfWidth,  -halfHeight);
		}

		result->AddTriangle(nw, ne, sw, cy::Point3f(0, 1, 0), cy::Point3f(1, 1, 0), cy::Point3f(0, 0, 0), invert);
		result->AddTriangle(sw, ne, se, cy::Point3f(0, 0, 0), cy::Point3f(1, 1, 0), cy::Point3f(1, 0, 0), invert);

		
		return result;
	}

};







#endif // !_GRAPHICS_OBJECT

