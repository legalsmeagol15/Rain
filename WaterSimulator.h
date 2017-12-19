
#ifndef _WATER_SIMULATOR_H	//Not all compilers allow "#pragma once"
#define _WATER_SIMULATOR_H

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <exception>
#include "Helpers.h"
#include "wo.h"

#define WATER_SIM_COMPUTE_SHADER_FILENAME				"SHADERS/waterSim2Waves.compShdr.txt"
#define WATER_SIM_PERTURBATION_COMPUTE_SHADER_FILENAME	"SHADERS/waterSim1Perturb.compShdr.txt"
#define DEFAULT_TIME_STEP				0.033f
#define WORK_GROUP_SIZE_X					16
#define WORK_GROUP_SIZE_Y					16
#define WORK_GROUP_SIZE_PERTURBATIONS		8

class WaterSimulator {

private:
	
	wo::ComputeShaderProgram perturbation_program = wo::ComputeShaderProgram(wo::Shader(GL_COMPUTE_SHADER, WATER_SIM_PERTURBATION_COMPUTE_SHADER_FILENAME));
	wo::ComputeShaderProgram wave_program = wo::ComputeShaderProgram(wo::Shader(GL_COMPUTE_SHADER, WATER_SIM_COMPUTE_SHADER_FILENAME));


public:

	const int width;
	const int height;
	const int levels;
	float gravity = 9.8f;
	float surfaceTension = 1.0f;
	float density = 1.0f;
	float depth = 10.0f;
	float amplitude_time_ebb = 0.5f;
	float amplitude_distance_ebb = 0.3f;
	float soliton_speed = 0.75f;
	float scale = 1.0f;

	/*The time since the start of the simulation, in  milliseconds.*/
	int currentTime = 0;
	int runCount = 0;

	struct WaveFragment {
		cy::Point2f origin = cy::Point2f(0.0f, 0.0f);
		float wave_number = 0.0f;
		float amplitude = 0.0f;
		int time_start = 0;
		float phase_offset = 0.0f;
		float energy = 0.0f;
		float celerity = 0.0f;
		cy::Point2f reflection = cy::Point2f(0.0f, 0.0f);
		float traversal = 0.0f;
		float unused;

		WaveFragment(float originX, float originY, float waveNumber, float amplitude, int timeStart, float phase, float energy, float celerity, float traversal) 
			: origin(cy::Point2f(originX, originY)), wave_number(waveNumber), amplitude(amplitude), time_start(timeStart), phase_offset(phase), energy(energy), celerity(celerity), traversal(traversal) {}
		WaveFragment() {}
	};

	struct Perturbation {
		cy::Point2f location;
		int level;
		float unused;
		WaveFragment wave_fragment;
		Perturbation(cy::Point2f location, int level, cy::Point2f origin, float waveNumber, float amplitude, int time_start, float phase, float energy, float celerity) 
			: location(location), level(level), wave_fragment(WaveFragment(origin.x, origin.y, waveNumber, amplitude, time_start, phase, energy, celerity, 0.0f)) {}
		//Perturbation() : location(cy::Point2f(0, 0)), level(0), wave_fragment(WaveFragment()) {}
	};

	GLuint GetReflectionMapID() { return _tex_reflection_map; }
	GLuint GetNormalMapID() { return _tex_normal_map; }


	bool Perturb(cy::Point2f location, int level, cy::Point2f origin, float waveNumber, float amplitude, unsigned int timeStamp, float phase_offset = 0.0f) {
		if (location.x < 0 || location.x >= width * scale) return false;
		if (location.y < 0 || location.y >= height * scale) return false;
		if (level < 0 || level > levels) return false;
		if (waveNumber <= 0.0f) return false;
		if (amplitude <= 0.0f) return false;
		
		Perturbation p = Perturbation(cy::Point2f(location.x, location.y), level, origin, waveNumber, amplitude, timeStamp, phase_offset, 0, 0);
		_perturbations.push_back(p);

		return true;
	}

	bool PerturbPoint(cy::Point2f location, int level, float waveNumber, float amplitude, unsigned int timeStamp, float phase_offset = 0.0f) {
		cy::Point2f origin  =  scale * cy::Point2f(location.x, location.y);
		return Perturb(location, level, origin, waveNumber, amplitude, timeStamp, phase_offset);
	}

private:

	std::vector<Perturbation> _perturbations;

	GLuint _ssbo_fragments_A = INVALID_ID;
	GLuint _ssbo_fragments_B = INVALID_ID;

	GLuint _ssbo_perturbations = INVALID_ID;

	GLuint _tex_normal_map = INVALID_ID;
	GLuint _tex_reflection_map = INVALID_ID;

	bool _in_A_out_B = true;

	
	int GetIndex(int x, int y, int level) {
		int levelContribution = level * width * height;
		int rowContribution = y * width;
		return x + rowContribution + levelContribution;
	}
	

public:
	WaterSimulator(int width, int height, int levels, float scale = 10.0f) : width(width), height(height), levels(levels), scale(scale) {

		//GL_MAX_COMPUTE_WORK_GROUP_COUNT x = y = z = 65535

		int numFragments = width * height * levels;

		//Generate the fragment buffers.
		std::vector<WaveFragment> emptyFragments(numFragments);
		int idx = 0;
		for (int z = 0; z < levels; z++) {
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					emptyFragments[idx++] = WaveFragment(x*scale, y*scale, 0, 0, 0, 0, 0, 0,0);
				}
			}
		}
		glGenBuffers(1, &_ssbo_fragments_A);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssbo_fragments_A);
		glBufferData(GL_SHADER_STORAGE_BUFFER, numFragments * sizeof(WaveFragment), &emptyFragments[0], GL_STATIC_DRAW);
		glGenBuffers(1, &_ssbo_fragments_B);
		
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssbo_fragments_B);
		glBufferData(GL_SHADER_STORAGE_BUFFER, numFragments * sizeof(WaveFragment), &emptyFragments[0], GL_STATIC_DRAW);
		Clear();

		//Generate the perturbation buffer, but don't fill it with anything yet.  That will come later.
		glGenBuffers(1, &_ssbo_perturbations);
		//glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssbo_perturbations);
		//glBufferData(GL_SHADER_STORAGE_BUFFER, max_perturbation_size * sizeof(Perturbation), NULL, GL_STATIC_DRAW);

		//Build the normal map.
		//Relying on http://antongerdelan.net/opengl/compute.html and http://malideveloper.arm.com/sample-code/introduction-compute-shaders-2/ here
		glGenTextures(1, &_tex_normal_map);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, _tex_normal_map);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
		glBindImageTexture(	2,					/*Image unit, should NOT offset with GL_TEXTURE0.*/
							_tex_normal_map,	/*The texture to bind*/
							0,					/*The level of the texture to bind.*/
							GL_FALSE,			/*Layered texture?*/
							0,					/*Layer number*/
							GL_WRITE_ONLY,		/*Access type*/
							GL_RGBA32F);		/*Format */			//This call is unique for an "image" cf a "texture"

		
		glGenTextures(1, &_tex_reflection_map);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, _tex_reflection_map);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);				
		
		SetObstacles(false, false, false);

		//TODO:   Development only - create the development textures.
		for (int i = 0; i < 4; i++) CreateDevelopmentTexture(i);

		//Cleanup and check for errors.
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, NULL);
		glBindTexture(GL_TEXTURE_2D, NULL);
		CHECK_GL_ERROR("Here");
	}
	~WaterSimulator() {
		if (_ssbo_fragments_A != INVALID_ID) glDeleteBuffers(1, &_ssbo_fragments_A);
		if (_ssbo_fragments_B != INVALID_ID) glDeleteBuffers(1, &_ssbo_fragments_B);
		if (_ssbo_perturbations != INVALID_ID) glDeleteBuffers(1, &_ssbo_perturbations);
		if (_tex_normal_map != INVALID_ID) glDeleteTextures(1, &_tex_normal_map);
	}


	void SetObstacles(bool border, bool square, bool bar) {
		
		std::vector<cy::Point4f> reflections(width*height);
		for (int i = 0; i < (width * height); i++) reflections[i] = cy::Point4f(0, 0, 1, 1);

		if (border) {
			for (int x = 0; x < width; x++) {
				reflections[GetIndex(x, 0, 0)] = cy::Point4f(0, 1, 1, 1);
				reflections[GetIndex(x, height - 1, 0)] = cy::Point4f(0, -1, 1, 1);
			}
			for (int y = 0; y < height; y++) {
				reflections[GetIndex(0, y, 0)] = cy::Point4f(1, 0, 1, 1);
				reflections[GetIndex(width - 1, y, 0)] = cy::Point4f(-1, 0, 1, 1);
			}
		}

		if (square) {
			int centerX = width / 4, centerY = width / 2;
			int square_width = width / 8, square_height = height / 8;
			int xStart = centerX - square_width, xEnd = centerX + square_width;
			int yStart = centerY - square_height, yEnd = centerY + square_height;
			//Damp it all
			for (int x = xStart; x <= xEnd; x++) {
				for (int y = yStart; y <= yEnd; y++) {
					reflections[GetIndex(x, y, 0)] = cy::Point4f(0, 0, 0, 1);
				}
			}
			//The inset box.
			for (int x = xStart; x <= xEnd; x++) {
				reflections[GetIndex(x, yStart, 0)] = cy::Point4f(0, -1, 1, 1);
				reflections[GetIndex(x, yEnd, 0)] = cy::Point4f(0, 1, 1, 1);
			}
			for (int y = yStart; y <= yEnd; y++) {
				reflections[GetIndex(xStart, y, 0)] = cy::Point4f(-1, 0, 1, 1);
				reflections[GetIndex(xEnd, y, 0)] = cy::Point4f(1, 0, 1, 1);
			}
		}

		if (bar) {
			int centerX = 3*(width / 4), centerY = width / 2;
			int square_width = width / 10;
			int xStart = centerX - square_width, xEnd = centerX + square_width;
			int yEnd = height-square_width;
			//Damp it all
			for (int x = xStart; x <= xEnd; x++) {
				for (int y = 0; y <= yEnd; y++) {
					reflections[GetIndex(x, y, 0)] = cy::Point4f(0, 0, 0, 1);
				}
			}
			//The inset box.
			for (int x = xStart; x <= xEnd; x++) {
				//reflections[GetIndex(x, yStart, 0)] = cy::Point4f(0, -1, 1, 1);
				reflections[GetIndex(x, yEnd, 0)] = cy::Point4f(0, 1, 1, 1);
			}
			for (int y = 0; y <= yEnd; y++) {
				reflections[GetIndex(xStart, y, 0)] = cy::Point4f(-1, 0, 1, 1);
				reflections[GetIndex(xEnd, y, 0)] = cy::Point4f(1, 0, 1, 1);
			}
		}

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, _tex_reflection_map);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, &reflections[0]);
		glBindImageTexture(3, _tex_reflection_map, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	}

	

	void Clear() {
		int numFragments = width * height * levels;
		//Generate the fragment buffers.
		std::vector<WaveFragment> emptyFragments(numFragments);
		int idx = 0;
		for (int z = 0; z < levels; z++) {
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					emptyFragments[idx++] = WaveFragment(x*scale, y*scale, 0, 0, 0, 0, 0, 0, 0);
				}
			}
		}
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssbo_fragments_A);
		glBufferData(GL_SHADER_STORAGE_BUFFER, numFragments * sizeof(WaveFragment), &emptyFragments[0], GL_STATIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssbo_fragments_B);
		glBufferData(GL_SHADER_STORAGE_BUFFER, numFragments * sizeof(WaveFragment), &emptyFragments[0], GL_STATIC_DRAW);

	}

	bool Execute(int elapsedTime) {

		
		
		//Run the perturbation shader - the amount sent must be a multiple of the perturb work group size.
		if (!perturbation_program.Bind()) return false;
		if (_perturbations.size() > 0) {

			//Fill up the list with duplicates of the last one, up to the nearest multiple of the group size.
			while (_perturbations.size() % WORK_GROUP_SIZE_PERTURBATIONS != 0)
				_perturbations.push_back(_perturbations[_perturbations.size() - 1]);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssbo_perturbations);
			glBufferData(GL_SHADER_STORAGE_BUFFER, _perturbations.size() * sizeof(Perturbation), &_perturbations[0], GL_STATIC_DRAW);

			perturbation_program.SetUniform("width", width);
			perturbation_program.SetUniform("height", height);
			perturbation_program.SetUniform("gravity", gravity);
			perturbation_program.SetUniform("surfaceTension", surfaceTension);
			perturbation_program.SetUniform("density", density);
			perturbation_program.SetUniform("depth", depth);
			perturbation_program.SetUniform("scale", scale);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _in_A_out_B ? _ssbo_fragments_A : _ssbo_fragments_B);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssbo_perturbations);
			int workGroupCount = _perturbations.size() / WORK_GROUP_SIZE_PERTURBATIONS;
			glDispatchCompute(workGroupCount, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

			_perturbations.clear();
		}
		

		//Run the wave simulation shader.	
		GLuint inputs, outputs;
		if (_in_A_out_B) { inputs = _ssbo_fragments_A;	outputs = _ssbo_fragments_B; }
		else { inputs = _ssbo_fragments_B; outputs = _ssbo_fragments_A; }
		{
			if (!wave_program.Bind()) return false;
			wave_program.SetUniform("in_A_out_B", _in_A_out_B);
			wave_program.SetUniform("width", width);
			wave_program.SetUniform("height", height);
			wave_program.SetUniform("levels", levels);
			wave_program.SetUniform("gravity", gravity);
			wave_program.SetUniform("surfaceTension", surfaceTension);
			wave_program.SetUniform("density", density);
			wave_program.SetUniform("depth", depth);
			wave_program.SetUniform("scale", scale);
			wave_program.SetUniform("ampTimeEbb", amplitude_time_ebb);
			wave_program.SetUniform("ampDistanceEbb", amplitude_distance_ebb);
			wave_program.SetUniform("solitonSpeed", soliton_speed);
			wave_program.SetUniform("timeNow", currentTime);
			wave_program.SetUniform("timeElapsed", elapsedTime);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputs);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outputs);

			for (int zLevel = 0; zLevel < levels; zLevel++) {
				wave_program.SetUniform("zLevel", zLevel);
				if (zLevel < 4) {
					glActiveTexture(GL_TEXTURE2 + zLevel);
					glBindTexture(GL_TEXTURE_2D, devTextures[zLevel]);
					glBindImageTexture(4, devTextures[zLevel], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
				}
				
				glDispatchCompute(width / WORK_GROUP_SIZE_X, height / WORK_GROUP_SIZE_Y, 1);
				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);			
			}
		}
		
		
		

		//Don't have to bind the image2D uniform, it was actually set as part of the layout.
		
		
		

		CHECK_GL_ERROR("Here");

		_in_A_out_B = !_in_A_out_B;		
		currentTime += elapsedTime;
		runCount++;

		//Signify the successful operation.
		return true;
	}


	GLuint devTextures[4];
	void CreateDevelopmentTexture(int i) {
		glGenTextures(1, &devTextures[i]);
		glActiveTexture(GL_TEXTURE2 + i);
		glBindTexture(GL_TEXTURE_2D, devTextures[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
		glBindImageTexture(4 + i, devTextures[i], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	}

	

};





#endif