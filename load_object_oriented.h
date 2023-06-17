// This is a sample of how to load a level in a object oriented fashion.
// Feel free to use this code as a base and tweak it for your needs.

// This reads .h2b files which are optimized binary .obj+.mtl files
#include "h2bParser.h"

class Model {
	// Name of the Model in the GameLevel (useful for debugging)
	std::string name;
	// Loads and stores CPU model data from .h2b file
	H2B::Parser cpuModel; // reads the .h2b format
	// Shader variables needed by this model. 
	GW::MATH::GMATRIXF world;// TODO: Add matrix/light/etc vars..
	// TODO: API Rendering vars here (unique to this model)
	// Vertex Buffer
	GLuint vertexArray = 0;
	GLuint vertexBufferObject = 0;
	// Index Buffer
	GLuint indexBufferObject = 0;
	// Pipeline/State Objects
	GLuint locShaderMats = 0;
	// Uniform/ShaderVariable Buffer
	GLuint UBO = 0;

	struct MODEL_DATA {
		GW::MATH::GMATRIXF worldMatrix; //Final world space transform
		H2B::ATTRIBUTES material; //Color/texture of surface
	};
	MODEL_DATA model;

public:
	inline void SetName(std::string modelName) {
		name = modelName;
	}
	inline void SetWorldMatrix(GW::MATH::GMATRIXF worldMatrix) {
		world = worldMatrix;
	}
	bool LoadModelDataFromDisk(const char* h2bPath) {
		// if this succeeds "cpuModel" should now contain all the model's info
		return cpuModel.Parse(h2bPath);
	}
	bool UploadModelData2GPU(/*specific API device for loading*/) {
		// TODO: Use chosen API to upload this model's graphics data to GPU
		//EVERYTHING THATS DONE ONCE PER CYCLE

		//Create a Vertex Buffer
		glGenVertexArrays(1, &vertexArray);
		glGenBuffers(1, &vertexBufferObject);
		glBindVertexArray(vertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
		glBufferData(GL_ARRAY_BUFFER, (sizeof(H2B::VERTEX) * cpuModel.vertexCount), cpuModel.vertices.data(), GL_STATIC_DRAW);

		//Create an Index Buffer
		glGenBuffers(1, &indexBufferObject);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (sizeof(unsigned int) * cpuModel.indexCount), cpuModel.indices.data(), GL_STATIC_DRAW);

		//Create UBO Buffer
		glGenBuffers(1, &UBO);
		glBindBuffer(GL_UNIFORM_BUFFER, UBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(MODEL_DATA), &model, GL_DYNAMIC_DRAW);

		//Establish Vertex Attribute Information
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(H2B::VERTEX), (void*)offsetof(H2B::VERTEX, pos));
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(H2B::VERTEX), (void*)offsetof(H2B::VERTEX, uvw));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(H2B::VERTEX), (void*)offsetof(H2B::VERTEX, nrm));
		glEnableVertexAttribArray(2);

		return true;
	}
	bool DrawModel(GLuint shaderExecutable) {
		// TODO: Use chosen API to setup the pipeline for this model and draw it
		//EVERYTHING DONE ONCE PER LOOP
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject);
		glBindVertexArray(vertexArray);
		
		locShaderMats = glGetUniformBlockIndex(shaderExecutable, "ModelData"); //Getting the location index of the uniform buffer in the GPU side

		glBindBufferBase(GL_UNIFORM_BUFFER, 2, UBO); //Binding the buffer on the GPU side (in VRAM)

		glUniformBlockBinding(shaderExecutable, locShaderMats, 2); //Binding the block TO the buffer in VRAM
		glBindBuffer(GL_UNIFORM_BUFFER, UBO);

		for (int i = 0; i < cpuModel.meshCount; i++)
		{
			model.worldMatrix = world;
			
			model.material = cpuModel.materials[i].attrib; //We're allowed to recast because the memory layout is indentical :O

			//Call SubBuffer and rewrite the entire UBO
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(MODEL_DATA), &model);

			//Draw
			glDrawElements(GL_TRIANGLES, cpuModel.meshes[i].drawInfo.indexCount, GL_UNSIGNED_INT, (void*)(cpuModel.meshes[i].drawInfo.indexOffset * sizeof(float)));
		}
		glBindVertexArray(0); // some video cards(cough Intel) need this set back to zero or they won't display
		return true;
	}
	bool FreeResources(/*specific API device for unloading*/) {
		// TODO: Use chosen API to free all GPU resources used by this model
		//DESTRUCTOR INFORMATION
		// free resources
		glDeleteVertexArrays(1, &vertexArray);
		glDeleteBuffers(1, &vertexBufferObject);
		glDeleteBuffers(1, &indexBufferObject);
		glDeleteBuffers(1, &UBO);
	}
};


class Level_Objects {

	// store all our models
	std::list<Model> allObjectsInLevel;

public:
	
	// Imports the default level txt format and creates a Model from each .h2b
	bool LoadLevel(	const char* gameLevelPath,
					const char* h2bFolderPath,
					GW::SYSTEM::GLog log) {
		
		// What this does:
		// Parse GameLevel.txt 
		// For each model found in the file...
			// Create a new Model class on the stack.
				// Read matrix transform and add to this model.
				// Load all CPU rendering data for this model from .h2b
			// Move the newly found Model to our list of total models for the level 

		log.LogCategorized("EVENT", "LOADING GAME LEVEL [OBJECT ORIENTED]");
		log.LogCategorized("MESSAGE", "Begin Reading Game Level Text File.");

		UnloadLevel();// clear previous level data if there is any
		GW::SYSTEM::GFile file;
		file.Create();
		if (-file.OpenTextRead(gameLevelPath)) {
			log.LogCategorized(
				"ERROR", (std::string("Game level not found: ") + gameLevelPath).c_str());
			return false;
		}
		char linebuffer[1024];
		while (+file.ReadLine(linebuffer, 1024, '\n'))
		{
			// having to have this is a bug, need to have Read/ReadLine return failure at EOF
			if (linebuffer[0] == '\0')
				break;
			if (std::strcmp(linebuffer, "MESH") == 0) //Check to find a line with just the word MESH
			{
				Model newModel;
				file.ReadLine(linebuffer, 1024, '\n');
				log.LogCategorized("INFO", (std::string("Model Detected: ") + linebuffer).c_str());
				// create the model file name from this (strip the .001)
				newModel.SetName(linebuffer);
				std::string modelFile = linebuffer;
				modelFile = modelFile.substr(0, modelFile.find_last_of("."));
				modelFile += ".h2b";

				// now read the transform data as we will need that regardless
				GW::MATH::GMATRIXF transform;
				for (int i = 0; i < 4; ++i) {
					file.ReadLine(linebuffer, 1024, '\n');
					// read floats
					std::sscanf(linebuffer + 13, "%f, %f, %f, %f",
						&transform.data[0 + i * 4], &transform.data[1 + i * 4],
						&transform.data[2 + i * 4], &transform.data[3 + i * 4]);
				}
				std::string loc = "Location: X ";
				loc += std::to_string(transform.row4.x) + " Y " +
					std::to_string(transform.row4.y) + " Z " + std::to_string(transform.row4.z);
				log.LogCategorized("INFO", loc.c_str());

				// Add new model to list of all Models
				log.LogCategorized("MESSAGE", "Begin Importing .H2B File Data.");
				modelFile = std::string(h2bFolderPath) + "/" + modelFile;
				newModel.SetWorldMatrix(transform);
				// If we find and load it add it to the level
				if (newModel.LoadModelDataFromDisk(modelFile.c_str())) {
					// add to our level objects, we use std::move since Model::cpuModel is not copy safe.
					allObjectsInLevel.push_back(std::move(newModel));
					log.LogCategorized("INFO", (std::string("H2B Imported: ") + modelFile).c_str());
				}
				else {
					// notify user that a model file is missing but continue loading
					log.LogCategorized("ERROR",
						(std::string("H2B Not Found: ") + modelFile).c_str());
					log.LogCategorized("WARNING", "Loading will continue but model(s) are missing.");
				}
				log.LogCategorized("MESSAGE", "Importing of .H2B File Data Complete.");
			}
			if (std::strcmp(linebuffer, "LIGHT") == 0) //Check to find a line with just the word LIGHT
			{
				
			}
		}
		log.LogCategorized("MESSAGE", "Game Level File Reading Complete.");
		// level loaded into CPU ram
		log.LogCategorized("EVENT", "GAME LEVEL WAS LOADED TO CPU [OBJECT ORIENTED]");
		return true;
	}
	// Upload the CPU level to GPU
	void UploadLevelToGPU(/*pass handle to API device if needed*/) {
		// iterate over each model and tell it to draw itself
		for (auto& e : allObjectsInLevel) {
			e.UploadModelData2GPU(/*forward handle to API device if needed*/);
		}
	}
	// Draws all objects in the level
	void RenderLevel(GLuint shaderExecutable) {
		// iterate over each model and tell it to draw itself
		for (auto &e : allObjectsInLevel) {
			e.DrawModel(shaderExecutable);
		}
	}
	// used to wipe CPU & GPU level data between levels
	void UnloadLevel() {
		allObjectsInLevel.clear();
	}
	// *THIS APPROACH COMBINES DATA & LOGIC* 
	// *WITH THIS APPROACH THE CURRENT RENDERER SHOULD BE JUST AN API MANAGER CLASS*
	// *ALL ACTUAL GPU LOADING AND RENDERING SHOULD BE HANDLED BY THE MODEL CLASS* 
	// For example: anything that is not a global API object should be encapsulated.
};

