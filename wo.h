
#ifndef _WO	//Not all compilers allow "#pragma once"
#define _WO

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <exception>
#include "Helpers.h"
#include "lodepng.h"
#include <unordered_set>
#include <unordered_map>

# define INVALID_ID	 0xFFFFFFFF


namespace GL {
	/*Prints whether the OpenGL context is current.  For now, this just returns true, till I learn more about this.*/
	inline bool CheckContext() { return true; }	//TODO:  update when I understand this better.

												/*Prints the OpenGL version.  From Cem Yuksel's code.*/
	inline void PrintVersion(std::ostream *outStream)
	{
		const GLubyte* version = glGetString(GL_VERSION);
		if (version) *outStream << version;
		else {
			int versionMajor = 0, versionMinor = 0;
			glGetIntegerv(GL_MAJOR_VERSION, &versionMajor);
			glGetIntegerv(GL_MINOR_VERSION, &versionMinor);
			if (versionMajor > 0 && versionMajor < 100) *outStream << versionMajor << "." << versionMinor;
			else *outStream << "Unknown";
		}
	}
}




namespace wo {

	/*A shader.  Large parts of this code were chopped and repurposed from Cem Yuksel's code.  This was written pursuant to CS5610 Spring '17.
	*/
	class Shader {

		friend class ShaderProgram;

	private:
		GLuint _shader_id = INVALID_ID;
		GLenum _shader_type;

	protected:

		/*Creates, but does not compile, the shader.  Use the compile methods to compile and complete the shader.*/
		Shader(GLenum shaderType) : _shader_type(shaderType) {}		

	public:

		/*Creates and compiles with the given file loaded as the shader.*/
		Shader(GLenum shaderType, char* filename) : Shader(shaderType) { CompileFile(filename); }

		/*Returns a reference to an uncompiled shader.*/
		static Shader* Uncompiled(GLenum shadertype) { return new Shader(shadertype); }

		/*Returns a new shader of the given type, built from the given filename.*/
		static Shader* FromFile(GLenum shaderType, char* filename) { Shader* result = new Shader(shaderType); result->CompileFile(filename, &std::cout); return result; }

		/*Returns a new shader of the given type, built from the given code.*/
		static Shader* FromCode(GLenum shaderType, char* code) { Shader* result = new Shader(shaderType); result->CompileCode(code, &std::cout); return result; }


		virtual ~Shader() { if (GL::CheckContext()) Delete(); }	

		/*Deletes the shader, if it has compiled already.*/
		void   Delete() { if (_shader_id != INVALID_ID) { glDeleteShader(_shader_id); _shader_id = INVALID_ID; } }

		/*Returns the tyep of the shader.*/
		GLuint GetType() const { return _shader_type; }

		/*Returns the ID number of the shader.*/
		GLuint GetID() const { return _shader_id; }

		/*Returns whether the shader has compiled.*/
		bool   IsCompiled() const { return _shader_id != INVALID_ID; }



		/*Compiles the given file name, for the GLSL shader type specified.  Note that this shader must now be bound to a program to be functional.*/
		bool CompileFile(const char *filename, std::ostream *outStream = &std::cout)
		{

			std::ifstream shaderStream(filename, std::ios::in);
			if (!shaderStream.is_open()) {
				if (outStream) *outStream << "ERROR: Cannot open file." << std::endl;
				return false;
			}

			std::string shaderSourceCode((std::istreambuf_iterator<char>(shaderStream)), std::istreambuf_iterator<char>());
			shaderStream.close();

			return CompileCode(shaderSourceCode.data(), outStream);
		}

		/*Compiles the given code to a GLSL shader of the type specified.  If there is already a shader compiled, returns false.*/
		bool CompileCode(const char *shaderSourceCode, std::ostream *outStream)
		{
			if (_shader_id != INVALID_ID) return false;

			//Get the shader id.
			GLuint tempShaderID = glCreateShader(_shader_type);

			//Specify the source.
			glShaderSource(tempShaderID, 1, &shaderSourceCode, nullptr);

			//Compile
			glCompileShader(tempShaderID);

			//Did it compile correctly?
			GLint result = GL_FALSE;
			glGetShaderiv(tempShaderID, GL_COMPILE_STATUS, &result);

			//Did it generate a log?
			int infoLogLength;
			glGetShaderiv(tempShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);
			if (infoLogLength > 1) {
				std::vector<char> compilerMessage(infoLogLength);
				glGetShaderInfoLog(tempShaderID, infoLogLength, nullptr, compilerMessage.data());
				if (outStream) {
					if (!result) *outStream << "ERROR: Cannot compile shader." << std::endl;
					*outStream << "OpenGL Version: ";
					GL::PrintVersion(outStream);
					*outStream << std::endl;
					*outStream << compilerMessage.data() << std::endl;
				}
			}

			//All is well?
			if (result) {
				GLint stype;
				glGetShaderiv(tempShaderID, GL_SHADER_TYPE, &stype);
				if (stype != (GLint)_shader_type) {
					if (outStream) *outStream << "ERROR: Incorrect shader type." << std::endl;
					return false;
				}
			}
			else {
				//A shader that failed somewhere along the way should be deleted.
				Delete();
			}

			if (CHECK_GL_ERROR("Shader::ctor end") != NULL) return false;
			_shader_id = tempShaderID;
			return true;
		}

	};





	/*The currently-bound ShaderProgram, at least as that is defined in WO::  Note that Cem's stuff does not set this, so don't rely too heavily on it.*/
	ShaderProgram* bound_program = nullptr;

	/*A general class describing a program with associated shader(s), which may be bound to interface with OpenGL, and which supports uniform setting.  A lot of this is based on Cem's stuff.*/
	class ShaderProgram {

	protected:
		/*The OpenGL identifying number (or name) for this program.*/
		GLuint _program_id;

		ShaderProgram() : _program_id(INVALID_ID) {  }

		/*The mapping of uniform names to their indices.*/
		std::unordered_map<std::string, GLint> _uniforms;

	public:

		/*Deletes this program from GPU memory.*/
		void Delete() { if (_program_id != INVALID_ID) { glDeleteProgram(_program_id); _program_id = INVALID_ID; _uniforms.clear(); } }

		~ShaderProgram() { Delete(); }

		/*Returns the ID of this program.*/
		GLuint GetID() { return _program_id; }

		/*Binds the program for execution.*/
		bool Bind() {
			if (_program_id == INVALID_ID) return false;
			if (bound_program != nullptr) bound_program->Unbind();
			glUseProgram(_program_id);
			bound_program = this;
		}
		/*Releases the program so it cannot be executed.*/
		void Unbind() {
			glUseProgram(NULL);
			bound_program = nullptr;
		}

	protected:
		/*Returns whether this program is valid, whether this program is bound, and whether the given uniform index is valid.  Note that none of these checks actually query OpenGL, they all track locally maintained fields.*/
		bool IsValidUniform(GLint uniform_index) { if (bound_program != this || _program_id == INVALID_ID || uniform_index < 0) return false; }

		bool SetUniform(GLint index, float value) { if (!IsValidUniform(index)) { return false; } glUniform1f(index, value); return true; }
		bool SetUniform(GLint index, float x, float y) { if (!IsValidUniform(index)) { return false; } glUniform2f(index, x, y); return true; }
		bool SetUniform(GLint index, float x, float y, float z) { if (!IsValidUniform(index)) { return false; } glUniform3f(index, x, y, z); return true; }
		bool SetUniform(GLint index, float x, float y, float z, float  w) { if (!IsValidUniform(index)) { return false; } glUniform4f(index, x, y, z, w); return true; }
		bool SetUniform(GLint index, int value) { if (!IsValidUniform(index)) { return false; } glUniform1i(index, value); return true; }
		bool SetUniform(GLint index, int x, int y) { if (!IsValidUniform(index)) { return false; } glUniform2i(index, x, y); return true; }
		bool SetUniform(GLint index, int x, int y, int z) { if (!IsValidUniform(index)) { return false; } glUniform3i(index, x, y, z); return true; }
		bool SetUniform(GLint index, int x, int y, int z, int w) { if (!IsValidUniform(index)) { return false; } glUniform4i(index, x, y, z, w); return true; }

		bool SetUniforms(GLint index, int count, const int *data) { if (!IsValidUniform(index)) { return false; } glUniform1iv(index, count, data); return true; }
		bool SetUniforms(GLint index, int count, const float *data) { if (!IsValidUniform(index)) { return false; } glUniform1fv(index, count, data); return true; }

		bool SetUniformMatrix2(GLint index, const float *m, int count = 1, bool transpose = false) { glUniformMatrix2fv(index, count, transpose, m); }
		bool SetUniformMatrix3(GLint index, const float *m, int count = 1, bool transpose = false) { glUniformMatrix3fv(index, count, transpose, m); }
		bool SetUniformMatrix4(GLint index, const float *m, int count = 1, bool transpose = false) { glUniformMatrix4fv(index, count, transpose, m); }

		/*Returns the index of the given uniform name.  If the uniform does not exist for this program, returns -1.*/
		GLint GetIndex(char* name) { auto iter = _uniforms.find(name);			return (iter == _uniforms.end()) ? -1 : iter->second; }

		/*Detects all uniforms in the program, and maps their names so they may be set by uniform name.*/
		void RegisterUniforms(std::ostream *outStream = &std::cout) {
			//Get and register the uniforms that are defined in this program.
			//Reference:  http://stackoverflow.com/questions/440144/in-opengl-is-there-a-way-to-get-a-list-of-all-uniforms-attribs-used-by-a-shade
			GLint count;				//The number of uniforms for this program.
			GLint size;					// size of the variable.  This is a throwaway.
			GLenum type;				// type of the variable (float, vec3 or mat4, etc).  This is another throwaway.
			const GLsizei bufSize = 32; // maximum name length.  32 is necessary because I have some long variable names.
			GLchar name[bufSize];		// this is what actually holds the name.
			GLsizei length;				// the length of the name.
			glGetProgramiv(_program_id, GL_ACTIVE_UNIFORMS, &count);	//Schleps the uniforms from openGL.
			for (GLint i = 0; i < count; i++)
			{
				glGetActiveUniform(_program_id, (GLuint)i, bufSize, &length, &size, &type, name);
				GLint index = glGetUniformLocation(_program_id, name);
				if (index < 0) {
					//Registration problems.
					GLenum error = glGetError();
					GLenum newError;
					while ((newError = glGetError()) != GL_NO_ERROR) error = newError;	//Get latest error.
					if (outStream != nullptr) {
						*outStream << "Shader program uniform registration ERROR: ";
						switch (error) {
						case GL_INVALID_VALUE:		*outStream << "GL_INVALID_VALUE"; break;
						case GL_INVALID_OPERATION:	*outStream << "GL_INVALID_OPERATION"; break;
						default:					*outStream << "unenumerated " << error; break;
						}
						*outStream << ".  Parameter \"" << name << "\" could not be registered." << std::endl;
					}
				}
				else {
					bool added = _uniforms.insert_or_assign(name, index).second;
					if (!added)	*outStream << "Shader program uniform registration ERROR: Duplicate uniform \"" << name << "\" could not be registered." << std::endl;
				}
			}
		}

	public:
		bool SetUniform(char* name, float value) { return SetUniform(GetIndex(name), value); }
		bool SetUniform(char* name, float x, float y) { return SetUniform(GetIndex(name), x, y); }
		bool SetUniform(char* name, float x, float y, float z) { return SetUniform(GetIndex(name), x, y, z); }
		bool SetUniform(char* name, float x, float y, float z, float w) { return SetUniform(GetIndex(name), x, y, z, w); }
		bool SetUniform(char* name, int value) { return SetUniform(GetIndex(name), value); }
		bool SetUniform(char* name, int x, int y) { return SetUniform(GetIndex(name), x, y); }
		bool SetUniform(char* name, int x, int y, int z) { return SetUniform(GetIndex(name), x, y, z); }
		bool SetUniform(char* name, int x, int y, int z, int w) { return SetUniform(GetIndex(name), x, y, z, w); }

		bool SetUniformMatrix2(char* name, const float *matrix, int count = 1, bool transpose = false) { return SetUniformMatrix2(GetIndex(name), matrix, count, transpose); }
		bool SetUniform(char* name, const cy::Matrix2f matrix, int count = 1, bool transpose = false) { return SetUniformMatrix2(GetIndex(name), matrix.data, count, transpose); }
		bool SetUniformMatrix3(char* name, const float *matrix, int count = 1, bool transpose = false) { return SetUniformMatrix3(GetIndex(name), matrix, count, transpose); }
		bool SetUniform(char* name, const cy::Matrix3f matrix, int count = 1, bool transpose = false) { return SetUniformMatrix3(GetIndex(name), matrix.data, count, transpose); }
		bool SetUniformMatrix4(char* name, const float *matrix, int count = 1, bool transpose = false) { return SetUniformMatrix4(GetIndex(name), matrix, count, transpose); }
		bool SetUniform(char* name, const cy::Matrix4f matrix, int count = 1, bool transpose = false) { return SetUniformMatrix4(GetIndex(name), matrix.data, count, transpose); }

		bool SetUniforms(char* name, int count, const float *data) { return SetUniforms(GetIndex(name), count, data); }
		bool SetUniforms(char* name, int count, const int *data) { return SetUniforms(GetIndex(name), count, data); }

	};





	/*A shader program that will allow only a single shader to be linked:  a compute shader.*/
	class ComputeShaderProgram : public ShaderProgram{

		
	public:

		ComputeShaderProgram(cy::GLSLShader shader) : ComputeShaderProgram(shader.GetID()) {}
		ComputeShaderProgram(Shader shader) : ComputeShaderProgram(shader.GetID()) {}
		ComputeShaderProgram(GLuint shaderID) :ShaderProgram() {
			if (shaderID == INVALID_ID) throw std::exception("Invalid shader.");

			GLint stype;
			glGetShaderiv(shaderID, GL_SHADER_TYPE, &stype);
			if (stype != (GLint)GL_COMPUTE_SHADER) throw std::exception("Incorrect shader type.");

			
			GLint toBeDeleted;
			glGetShaderiv(shaderID, GL_DELETE_STATUS, &toBeDeleted);
			if (toBeDeleted == GL_TRUE) throw std::exception("This shader is scheduled to be deleted.");

			GLint compileStatus;
			glGetShaderiv(shaderID, GL_COMPILE_STATUS, &compileStatus);
			if (compileStatus != GL_TRUE) throw std::exception("This shader has not been compiled.");

			//Create and link the program.
			GLuint tempProgramID = glCreateProgram();
			glAttachShader(tempProgramID, shaderID);
			glLinkProgram(tempProgramID);

			//Check for linking errors.
			if (CHECK_GL_ERROR("ComputerShaderProgram::ctor end") != NULL) throw std::exception("Problem linking the program.");

			_program_id = tempProgramID;

			RegisterUniforms(&std::cout);
		}


		

	};







	class Texture {

	public:
		char* name;

		/*Members describe how a texture is used below points (0,0) and (1,1).*/
		enum WrappingMode {
			/*The image repeats in identical fashion:  ABC|ABC|ABC*/
			Repeat = GL_REPEAT,
			/*The image repeats, but mirrors across edges:  ABC|CBA|ABC*/
			Mirrored_Repeat = GL_MIRRORED_REPEAT,
			/*The edge of the image is carried on beyond:  AAA|ABC|CCC*/
			Clamp_To_Edge = GL_CLAMP_TO_EDGE,
			/*Nothing outside the border is displayed:   ...|ABC|...   */
			Clamp_To_Border = GL_CLAMP_TO_BORDER
		};

		/*Members describe how a texture is treated when the screen pixels do not line up well with the texture's texels.*/
		enum FilterMode {
			/*The nearest texel to the requested pixel is used.*/
			Nearest = GL_NEAREST,
			/*The linear blend of the nearest texels is used.*/
			Linear = GL_LINEAR
		};

		/*Members describe how mipmaps are treated when the pixel's size does not match any mipmap level's texel size.*/
		enum MipMapFiltering {
			/*Finds the mipmap that most closely matchest the pixel size, and then interpolates between the nearest texels at that level.*/
			Nearest_Level_Nearest = GL_NEAREST_MIPMAP_NEAREST,
			/*Linearly interpolates between the nearest texel of the nearest two mipmaps.*/
			Linear_Level_Nearest = GL_LINEAR_MIPMAP_NEAREST,
			/*Finds the mipmap that most closely matches the pixel size, and then interpolates between the nearest texels at that level.*/
			Nearest_Level_Linear = GL_NEAREST_MIPMAP_LINEAR,
			/*Linearly interpolates between the linearly interpolated texels of the nearest two mipmaps.*/
			Linear_Level_Linear = GL_LINEAR_MIPMAP_LINEAR
		};

		/*Members describe the type of the texture.*/
		enum TextureType {
			/*The type of texture is a 2D texture.*/
			Texture_2D = GL_TEXTURE_2D,

			/*The type of texture is a cube map.*/
			Cube_Map = GL_TEXTURE_CUBE_MAP
		};

	protected:		
		GLuint _texture_id;
		GLenum _wrapping_mode_horizontal;
		GLenum _wrapping_mode_vertical;
		GLenum _filter_minify;
		GLenum _filter_magnify;		
		GLenum _mipmap_filter_minify;
		GLenum _mipmap_filter_magnify;
		GLenum _texture_type;
		bool _use_mipmaps = true;

		/*Returns the previous binding.  Must be overridden by inheriting classes.*/
		virtual GLint GetPreviousBinding() = 0;

		/*Stores the texture in the GPU.*/
		virtual void StoreTexture() = 0;


		Texture(TextureType textureType, bool useMipMaps, bool initializeImage = true) : _texture_type(textureType), _use_mipmaps(useMipMaps) {

			if (initializeImage) {
				//Initialize
				InitializeImage(useMipMaps);
			}
		}

		/*Generates and stores the image on the GPU.  Can be overridden in a derived class.*/
		virtual void InitializeImage(bool useMipMaps) {
			
			////Caches the previous binding.
			//GLint prev_binding = GetPreviousBinding();

			//Create the new texture.
			glGenTextures(1, &_texture_id);
			glBindTexture(_texture_type, _texture_id);
			StoreTexture();
			SetWrappingMode();
			SetFilteringMode();
			if (useMipMaps) {
				glGenerateMipmap(_texture_type);
				SetMipMapFilteringMode();
			}			

			////Restore the previous binding.
			//glBindTexture(_texture_type, prev_binding);
		}

	public:
		~Texture() {
			//I don't think a bind is necessary before a delete.  TODO:  test this?
			glDeleteTextures(1, &_texture_id);
			_texture_id = 0;
		}

		/*Returns the texture ID number (or name) for this texture.*/
		GLuint GetID() { return _texture_id; }

		/*Binds this texture to the given texture unit.  If the texture unit is omitted, binds to 0.*/
		void Bind(int textureUnit = 0) {
			if (textureUnit >= 16) Throw("Not sure if there are this many ");
			glActiveTexture(GL_TEXTURE0 + textureUnit);			
			glBindTexture(_texture_type, _texture_id);
		}

		/*Sets the filtering strategy used to filter mipmapped images.  Whenever the size of the screen's pixels
		do not line up with the texels of the nearest mipmap image, filtering must be applied.*/
		void SetMipMapFilteringMode(MipMapFiltering minifyFilter = Linear_Level_Linear, MipMapFiltering magnifyFilter = Linear_Level_Linear) {
			GLint prev_binding = GetPreviousBinding();
			CHECK_GL_ERROR("TextueCubeMap::SetMipMapFilteringMode 0");
			_mipmap_filter_minify = minifyFilter;
			_mipmap_filter_magnify = magnifyFilter;
			glBindTexture(_texture_type, _texture_id);
			CHECK_GL_ERROR("TextueCubeMap::SetMipMapFilteringMode 1");
			glTexParameteri(_texture_type, GL_TEXTURE_MIN_FILTER, (int)_mipmap_filter_minify);
			CHECK_GL_ERROR("TextueCubeMap::SetMipMapFilteringMode 2");
			if (magnifyFilter == Linear_Level_Linear || magnifyFilter == Nearest_Level_Linear)
				glTexParameteri(_texture_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			else
				glTexParameteri(_texture_type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			CHECK_GL_ERROR("TextueCubeMap::SetMipMapFilteringMode 3");
			if (!_use_mipmaps) {
				_use_mipmaps = true;
				CHECK_GL_ERROR("TextueCubeMap::SetMipMapFilteringMode 4");
				glGenerateMipmap(_texture_type);
				CHECK_GL_ERROR("TextueCubeMap::SetMipMapFilteringMode 5");
			}
			CHECK_GL_ERROR("TextueCubeMap::SetMipMapFilteringMode 6");
			glBindTexture(_texture_type, prev_binding);
			CHECK_GL_ERROR("TextueCubeMap::SetMipMapFilteringMode 7");
		}
		MipMapFiltering GetMipMapFilteringMode_Minify() { return (MipMapFiltering)_mipmap_filter_minify; }
		MipMapFiltering GetMipMapFilteringMode_Magnify() { return (MipMapFiltering)_mipmap_filter_magnify; }

		/*Sets the strategies OpenGL uses for filtering the texture.  Whenever a texture's texels do not line up with pixels
		of the screen, OpenGL must use some means to determine which pixels are sampled and displayed.*/
		void SetFilteringMode(FilterMode minifyFilter = Linear, FilterMode magnifyFilter = Linear) {
			GLint prev_binding = GetPreviousBinding();
			_filter_minify = minifyFilter;
			_filter_magnify = magnifyFilter;
			glTexParameteri(_texture_type, GL_TEXTURE_MIN_FILTER, _filter_minify);
			glTexParameteri(_texture_type, GL_TEXTURE_MAG_FILTER, _filter_magnify);
			glBindTexture(_texture_type, prev_binding);
		}
		FilterMode GetFilterMode_Minify() { return (FilterMode)_filter_minify; }
		FilterMode GetFilterMode_Magnify() { return (FilterMode)_filter_magnify; }

		void SetWrappingMode(WrappingMode mode =  Clamp_To_Edge) {
			GLint prev_binding = GetPreviousBinding();
			_wrapping_mode_horizontal = mode;
			_wrapping_mode_vertical = mode;
			glTexParameteri(_texture_type, GL_TEXTURE_WRAP_S, mode);	//Horizontal
			glTexParameteri(_texture_type, GL_TEXTURE_WRAP_T, mode);	//Vertical
			glBindTexture(_texture_type, prev_binding);
		}
		WrappingMode GetWrappingMode_Horizontal() { return (WrappingMode)_wrapping_mode_horizontal; }
		WrappingMode GetWrappingMode_Vertical() { return (WrappingMode)_wrapping_mode_vertical; }



	};






	class TextureCubeMap : public Texture {

	public:
		/*Creates a blank cube map.*/
		TextureCubeMap(unsigned int sideWidth, unsigned int sideHeight, GLubyte clr, bool useMipMaps = true) : Texture(TextureType::Cube_Map, useMipMaps, false) {

			GLint prev_binding = GetPreviousBinding();

			//Set params for the texture cube.
			glGenTextures(1, &_texture_id);
			glBindTexture(GL_TEXTURE_CUBE_MAP, _texture_id);
			SetWrappingMode();
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			SetFilteringMode();

			//Create an empty cube map.  It won't actually be empty, but filled with data for debugging purposes.
			std::vector<GLubyte> data(sideWidth * sideHeight * 4, clr);
			for (int i = 0; i < 6; i++)
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, sideWidth, sideHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[0]);

			//Restore the original binding.
			glBindTexture(GL_TEXTURE_CUBE_MAP, prev_binding);
		}

	private:

		unsigned int _widths[6];
		unsigned int _heights[6];
		GLubyte* _images[6];

		TextureCubeMap(std::vector<GLubyte>* images, unsigned int* widths, unsigned int* heights, bool useMipMaps = true) : Texture(TextureType::Cube_Map, useMipMaps, false) {
			CHECK_GL_ERROR("TextueCubeMap::TextureCubeMap 0");
			for (int i = 0; i < 6; i++) {
				_widths[i] = widths[i];
				_heights[i] = heights[i];
				_images[i] = &images[i][0];
			}

			CHECK_GL_ERROR("TextueCubeMap::TextureCubeMap 1");
			//Initialize.
			InitializeImage(useMipMaps);	
			CHECK_GL_ERROR("TextueCubeMap::TextureCubeMap 2");
		
		}

		/*Generates and stores the image on the GPU.  Can be overridden in a derived class.*/
		virtual void InitializeImage(bool useMipMaps) {			

			CHECK_GL_ERROR("TextueCubeMap::InitializeImage 0");
			//Caches the previous binding.
			GLint prev_binding = GetPreviousBinding();
			CHECK_GL_ERROR("TextueCubeMap::InitializeImage 1");


			//Create the new texture.
			glGenTextures(1, &_texture_id);
			CHECK_GL_ERROR("TextueCubeMap::InitializeImage 2");
			glBindTexture(_texture_type, _texture_id);
			CHECK_GL_ERROR("TextueCubeMap::InitializeImage 3");
			StoreTexture();
			CHECK_GL_ERROR("TextueCubeMap::InitializeImage 4");
			SetWrappingMode();
			CHECK_GL_ERROR("TextueCubeMap::InitializeImage 5");
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			CHECK_GL_ERROR("TextueCubeMap::InitializeImage 6");
			SetFilteringMode();
			if (useMipMaps) {
				CHECK_GL_ERROR("TextueCubeMap::InitializeImage 7");
				glGenerateMipmap(_texture_type);
				CHECK_GL_ERROR("TextueCubeMap::InitializeImage 8");
				SetMipMapFilteringMode();
			}
			CHECK_GL_ERROR("TextueCubeMap::InitializeImage 9");
			//Restore the previous binding.
			glBindTexture(_texture_type, prev_binding);
			CHECK_GL_ERROR("TextueCubeMap::InitializeImage 10");
		}

		/*Returns the current cube map binding.*/
		virtual GLint GetPreviousBinding() { GLint result; glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &result); return result; }

		virtual void StoreTexture() {
			for (int i = 0; i < 6; i++) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, _widths[i], _heights[i], 0, GL_RGBA, GL_UNSIGNED_BYTE, _images[i]);
		}

	public:
		enum Side {
			Positive_X = GL_TEXTURE_CUBE_MAP_POSITIVE_X,
			Positive_Y = GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
			Positive_Z = GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
			Negative_X = GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
			Negative_Y = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
			Negative_Z = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
		};

		unsigned int GetWidth(Side side) { return _widths[side - GL_TEXTURE_CUBE_MAP_POSITIVE_X]; }
		unsigned int GetHeight(Side side) { return _heights[side - GL_TEXTURE_CUBE_MAP_POSITIVE_X]; }

		~TextureCubeMap() { glDeleteTextures(1, &_texture_id); }

	
	public:

		/*Returns a texture map generated from the images at the given file names.*/
		static TextureCubeMap* FromFiles(char* posX_filename, char* posY_filename, char* posZ_filename, char* negX_filename, char* negY_filename, char* negZ_filename) {

			char* filenames[6];			
			filenames[0] = posX_filename;
			filenames[2] = posY_filename;
			filenames[4] = posZ_filename;
			filenames[1] = negX_filename;
			filenames[3] = negY_filename;
			filenames[5] = negZ_filename;

			unsigned int heights[6];
			unsigned int widths[6];
			std::vector<GLubyte> images[6];

			for (int i = 0; i < 6; i++) {
				char* fileName = filenames[i];
				unsigned int width;
				unsigned int height;
				unsigned int error;

				std::string name = std::string(filenames[i]);
				std::string extension = name.substr(name.rfind('.') + 1);				
				if (extension == "png") { error = lodepng::decode(images[i], width, height, fileName); }
				else { Throw("Unsupported file format for texture file ", fileName); }
				if (error > 0) { std::cout << "Error loading texture map " << fileName << std::endl; continue; }
				widths[i] = width;
				heights[i] = height;
			}

			return new TextureCubeMap(images, widths, heights);
		}
	};




}





#endif