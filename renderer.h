//External Library Includes
#include <chrono>
//Gateware Include
#include "../Gateware/Gateware/Gateware.h"
//Parser Includes
#include "h2bParser.h"
#include "load_object_oriented.h"
//IMGUI includes
#include "Libraries/IMGUI/imgui.h"
#include "Libraries/IMGUI/imgui_impl_win32.h"
#include "Libraries/IMGUI/imgui_impl_opengl3.h"

//Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void PrintLabeledDebugString(const char* label, const char* toPrint)
{
	std::cout << label << toPrint << std::endl;
#if defined WIN32 //OutputDebugStringA is a windows-only function 
	OutputDebugStringA(label);
	OutputDebugStringA(toPrint);
#endif
}

// Used to print debug infomation from OpenGL, pulled straight from the official OpenGL wiki.
#ifndef NDEBUG
void APIENTRY
MessageCallback(GLenum source, GLenum type, GLuint id,
	GLenum severity, GLsizei length,
	const GLchar* message, const void* userParam) {


	std::string errMessage;
	errMessage = (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "");
	errMessage += " type = ";
	errMessage += type;
	errMessage += ", severity = ";
	errMessage += severity;
	errMessage += ", message = ";
	errMessage += message;
	errMessage += "\n";

	PrintLabeledDebugString("GL CALLBACK: ", errMessage.c_str());
}
#endif

// Creation, Rendering & Cleanup
class Renderer
{
	//Time Info
	//Chrono and delta time values, courtesy of http://www.mcihanozer.com/tips/c/calculating-delta-time/
	std::chrono::steady_clock::time_point old; //A variable to store the time since the last call to the update function
	float dt; //A variable to store the delta time

	//Screen Info
	float FOV;
	float AspectRatio;
	unsigned int height;
	unsigned int width;

	//Minimap Info
	float mFOV;
	float mAspectRatio;
	unsigned int mHeight;
	unsigned int mWidth;

	//Level Info
	unsigned int level = 0;
	bool show_demo_window = true;


	//Proxy Handles
	GW::SYSTEM::GWindow win;
	GW::GRAPHICS::GOpenGLSurface ogl;
	GW::SYSTEM::GLog log;
	GW::MATH::GMatrix mat_proxy;

	//Triangle Info
	GLuint vertexArray = 0;
	GLuint vertexBufferObject = 0;
	GLuint indexBufferObject = 0;

	//Shader Info
	GLuint vertexShader = 0;
	GLuint fragmentShader = 0;
	GLuint shaderExecutable = 0;

	//UBO Info
	GLuint UBO = 0;
	struct SCENE_DATA
	{
		GW::MATH::GVECTORF sunDirection, sunColor; //Lighting info
		GW::MATH::GMATRIXF viewMatrix, projectionMatrix; //Viewing info
		GW::MATH::GVECTORF cameraPos; //Position for camera
		GW::MATH::GVECTORF sunAmbient; //Ambient Component for our Lighting
	};
	SCENE_DATA shaderMats;
	SCENE_DATA minimapMats;
	GLint locShaderMats;

	//Controller Inputs
	GW::INPUT::GInput input;
	GW::INPUT::GController controller;

	Level_Objects models;

	//Dear IMGUI Information
	//Courtesy of IMGUI examples in API_SAMPLES
	static LONG_PTR	gatewareWndProc;
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		LRESULT lr = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

		return CallWindowProcW((WNDPROC)gatewareWndProc, hWnd, msg, wParam, lParam);
	}

	void DisplayImguiMenu()
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		//Rendering
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		ImGui::EndFrame();
	}
	
public:
	Renderer(GW::SYSTEM::GWindow _win, GW::GRAPHICS::GOpenGLSurface _ogl, GW::SYSTEM::GLog _log)
	{
		//Attach Global & Parameter values
		win = _win;
		ogl = _ogl;
		log = _log;

		//Create Universal Window Handler
		GW::SYSTEM::UNIVERSAL_WINDOW_HANDLE uwh;
		if (+win.GetWindowHandle(uwh))
		{
			gatewareWndProc = SetWindowLongPtr((HWND)uwh.window, GWLP_WNDPROC, (LONG_PTR)WndProc);
		}

		//Create inputs
		input.Create(win);
		controller.Create();

		//Establish Main Window Info
		FOV = G_DEGREE_TO_RADIAN_F(65);		//FOV
		win.GetClientHeight(height);		//HEIGHT
		win.GetClientWidth(width);			//WIDTH

		//Create Math Proxy
		mat_proxy.Create();

		//MATRIX INFORMATION

		//LEVEL 1

		//Main View Matrix (level 1)
		GW::MATH::GMATRIXF tempMV = GW::MATH::GIdentityMatrixF; //Create an identity as a base
		shaderMats.cameraPos = { 7.35f, 4.95f, -6.92f };		//CAMERA POS
		GW::MATH::GVECTORF at = { 0.15f, 0.75f, 0 };			//CAMERA LOOK
		GW::MATH::GVECTORF up = { 0, 1.0f, 0 };					//WORLD UP
		mat_proxy.LookAtRHF(shaderMats.cameraPos, at, up, shaderMats.viewMatrix); //Create a view matrix using the provided math function using the values passed in above

		//Projection Matrix (level 1)
		GW::MATH::GMATRIXF tempMP = GW::MATH::GIdentityMatrixF;
		ogl.GetAspectRatio(AspectRatio);
		mat_proxy.ProjectionOpenGLRHF(FOV, AspectRatio, 0.1f, 100.0f, tempMP);
		shaderMats.projectionMatrix = tempMP;

		//Create a minimap data set with all the same from the shaderMats besides camera and view
		minimapMats.sunDirection = shaderMats.sunDirection;
		minimapMats.sunColor = shaderMats.sunColor;
		minimapMats.projectionMatrix = shaderMats.projectionMatrix;
		minimapMats.sunAmbient = shaderMats.sunAmbient;
		//MiniMap View Matrix (level 1)
		GW::MATH::GMATRIXF tempMMV = GW::MATH::GIdentityMatrixF; //Create an identity as a base
		minimapMats.cameraPos = { 0.1f, 40.0f, 0.1f };		//CAMERA POS
		GW::MATH::GVECTORF at2 = { 0.0f, 0.0f, 0.0f };			//CAMERA LOOK
		GW::MATH::GVECTORF up2 = { 0, 1.0f, 0 };				//WORLD UP
		mat_proxy.LookAtRHF(minimapMats.cameraPos, at2, up2, minimapMats.viewMatrix); //Create a view matrix using the provided math function using the values passed in above

		//LIGHTING INFORMATION

		//LEVEL 1
		//Directional Light Source (level 1)
		GW::MATH::GVECTORF dirLight = { -1.0f, -1.0f, -2.0f }; //XYZ
		GW::MATH::GVector::NormalizeF(dirLight, dirLight); //Normalizes the dirLight
		GW::MATH::GVECTORF dirLightColor = { 0.9f, 0.9f, 1.0f, 1.0f }; //RGBA

		shaderMats.sunDirection = dirLight; //Set sunDirection to dirLight (previously created) (read from file?)
		shaderMats.sunColor = dirLightColor; //Set sunColor to dirLightColor (previously created) (read from file?)
		shaderMats.sunAmbient = { 0.25f, 0.25f, 0.35f, 1 }; //Set sunAmbient (ambient lighting) to a specified set of values

		//h2b Parser initialization
		
		models.LoadLevel("../Assets/Level2/GameLevel.txt", "../Assets/Level2/Models", log); //Load the default level
		models.UploadLevelToGPU(); //Upload the information to the system

		InitializeGraphics();

		//Dear IMGUI Information 
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		ImGui::StyleColorsDark();
		bool success = false;
		success = ImGui_ImplWin32_Init(uwh.window);
		if (!success) abort();
		success = ImGui_ImplOpenGL3_Init();
		if (!success) abort();
	}
	
private:
	//constructor helper functions 
	void InitializeGraphics()
	{
#ifndef NDEBUG
		BindDebugCallback(); // In debug mode we link openGL errors to the console
#endif
		CreateUBOBuffer(&shaderMats, sizeof(SCENE_DATA));
		CompileVertexShader();
		CompileFragmentShader();
		CreateExecutableShaderProgram();
	}

#ifndef NDEBUG
	void BindDebugCallback()
	{
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(MessageCallback, 0);
	}
#endif
	void CreateVertexBuffer(const void* data, unsigned int sizeInBytes)
	{
		glGenVertexArrays(1, &vertexArray);
		glGenBuffers(1, &vertexBufferObject);
		glBindVertexArray(vertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
		glBufferData(GL_ARRAY_BUFFER, sizeInBytes, data, GL_STATIC_DRAW);
	}

	void CreateIndexBuffer(const void* data, unsigned int sizeInBytes)
	{
		glGenBuffers(1, &indexBufferObject);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeInBytes, data, GL_STATIC_DRAW);
	}

	void CreateUBOBuffer(const void* data, unsigned int sizeInBytes)
	{
		glGenBuffers(1, &UBO);
		glBindBuffer(GL_UNIFORM_BUFFER, UBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeInBytes, data, GL_DYNAMIC_DRAW);
	}

	void CompileVertexShader()
	{
		char errors[1024];
		GLint result;

		vertexShader = glCreateShader(GL_VERTEX_SHADER);

		std::string vertexShaderSource = ReadFileIntoString("../Shaders/VertexShader.glsl");
		const GLchar* strings[1] = { vertexShaderSource.c_str() };
		const GLint lengths[1] = { vertexShaderSource.length() };
		glShaderSource(vertexShader, 1, strings, lengths);

		glCompileShader(vertexShader);
		glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &result);
		if (result == false)
		{
			glGetShaderInfoLog(vertexShader, 1024, NULL, errors);
			PrintLabeledDebugString("Vertex Shader Errors:\n", errors);
			abort();
			return;
		}
	}

	void CompileFragmentShader()
	{
		char errors[1024];
		GLint result;

		fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

		std::string fragmentShaderSource = ReadFileIntoString("../Shaders/FragmentShader.glsl");
		const GLchar* strings[1] = { fragmentShaderSource.c_str() };
		const GLint lengths[1] = { fragmentShaderSource.length() };
		glShaderSource(fragmentShader, 1, strings, lengths);

		glCompileShader(fragmentShader);
		glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &result);
		if (result == false)
		{
			glGetShaderInfoLog(fragmentShader, 1024, NULL, errors);
			PrintLabeledDebugString("Fragment Shader Errors:\n", errors);
			abort();
			return;
		}
	}

	void CreateExecutableShaderProgram()
	{
		char errors[1024];
		GLint result;

		shaderExecutable = glCreateProgram();
		glAttachShader(shaderExecutable, vertexShader);
		glAttachShader(shaderExecutable, fragmentShader);
		glLinkProgram(shaderExecutable);
		glGetProgramiv(shaderExecutable, GL_LINK_STATUS, &result);
		if (result == false)
		{
			glGetProgramInfoLog(shaderExecutable, 1024, NULL, errors);
			std::cout << errors << std::endl;
		}
	}

	void SetVertexAttributes()
	{
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);
	}

public:

	void Update()
	{
		UpdateCamera();
		UpdateLevel();
	}

	void UpdateLevel()
	{
		//Temporary system while IMGUI system is implemented;
		float oneState = 0.0f;
		GW::GReturn result1 = input.GetState(G_KEY_1, oneState);
		if (G_PASS(result1) && result1 != GW::GReturn::REDUNDANT)
		{
			if (oneState > 0)
			{
				models.UnloadLevel();
				models.LoadLevel("../Assets/Level1/GameLevel.txt", "../Assets/Level1/Models", log);
				models.UploadLevelToGPU();
			}
		}
		float twoState = 0.0f;
		GW::GReturn result2 = input.GetState(G_KEY_2, twoState);
		if (G_PASS(result2) && result2 != GW::GReturn::REDUNDANT)
		{
			if (twoState > 0)
			{
				models.UnloadLevel();
				models.LoadLevel("../Assets/Level2/GameLevel.txt", "../Assets/Level2/Models", log);
				models.UploadLevelToGPU();
			}
		}
		float levelChange = 0.0f;
		GW::GReturn resultL = input.GetState(G_KEY_F1, levelChange);
		if (G_PASS(resultL) && resultL != GW::GReturn::REDUNDANT)
		{
			if (levelChange > 0)
			{
				show_demo_window = !show_demo_window;
			}
		}
	}

	void UpdateCamera()
	{
		//Get Delta Time - Courtesy of http://www.mcihanozer.com/tips/c/calculating-delta-time/
		auto now = std::chrono::steady_clock::now(); //Get the current time
		dt = std::chrono::duration_cast<std::chrono::microseconds>(now - old).count() / 1000000.0f; //Get the change in time
		old = now; //Prepare the old value for the next frame

		//Get the camera matrix
		GW::MATH::GMATRIXF camera{}; //Prep the storage of a camera value
		mat_proxy.InverseF(shaderMats.viewMatrix, camera); //Store the camera temp as the inverse of our current view

		//Determine speeds
		float dy = 0.0f; //Initialize the change in y this frame
		const float Camera_Speed = 2.5f; //How far we want the camera to move over a single second

		//Get the keyboard button states

		//Vertical movement
		float spaceState = 0; //Initialize a value to store the space button state
		input.GetState(G_KEY_SPACE, spaceState); //Get the state of space
		float shiftState = 0; //Initialize a value to store the shift button state
		input.GetState(G_KEY_LEFTSHIFT, shiftState); //Get the state of left shift

		//Forward/Backward movement
		float wState = 0; //Initialize a value to store the w button state
		input.GetState(G_KEY_W, wState); //Get the state of w
		float sState = 0; //Initialize a value to store the s button state
		input.GetState(G_KEY_S, sState); //Get the state of s

		//Strafing movement
		float dState = 0; //Initialize a value to store the d button state
		input.GetState(G_KEY_D, dState); //Get the state of d
		float aState = 0; //Initialize a value to store the a button state
		input.GetState(G_KEY_A, aState); //Get the state of a

		//Get the controller button states (Just for controller 0)

		//Vertical movement
		float rTriggerState = 0; //Initialize a value to store the right trigger state
		controller.GetState(0, G_RIGHT_SHOULDER_BTN, rTriggerState); //Get the state of right trigger
		float lTriggerState = 0; //Initialize a value to store the right trigger state
		controller.GetState(0, G_LEFT_SHOULDER_BTN, lTriggerState); //Get the state of right trigger

		//Forward/Backward movement
		float lyStickState = 0; //Initialize a value to store the left stick y-axis state
		controller.GetState(0, G_LY_AXIS, lyStickState); //Get the state of the left stick y-axis

		//Strafing movement
		float lxStickState = 0; //Initialize a value to store the left stick x-axis state
		controller.GetState(0, G_LX_AXIS, lxStickState); //Get the state of the left stick x-axis

		//Get the Y change
		float sumDY = spaceState - shiftState + rTriggerState - lTriggerState;

		//Get the X change
		float sumDX = wState - sState + lyStickState;

		//Get the Z change
		float sumDZ = dState - aState + lxStickState;

		//Modify the camera position

		//Modify the Y value
		dy = sumDY * Camera_Speed * dt;
		GW::MATH::GVECTORF vDY = { 0.0f, dy, 0.0f, 0.0f };
		mat_proxy.TranslateLocalF(camera, vDY, camera);

		//Modify the X/Z value
		float perFrameSpeed = Camera_Speed * dt;
		GW::MATH::GMATRIXF translationMatrix;
		mat_proxy.IdentityF(translationMatrix);
		GW::MATH::GVECTORF vDXZ = { sumDZ * perFrameSpeed, 0, -sumDX * perFrameSpeed };
		mat_proxy.TranslateLocalF(translationMatrix, vDXZ, translationMatrix);
		mat_proxy.MultiplyMatrixF(translationMatrix, camera, camera);

		//Rotate the camera
		float Thumb_Speed = (G_PI * dt);
		float mouseDY = 0.0f;
		float mouseDX = 0.0f;
		GW::GReturn result = input.GetMouseDelta(mouseDX, mouseDY);

		if (G_PASS(result) && result != GW::GReturn::REDUNDANT)
		{
			//Get the right stick states
			float ryStickState = 0;
			controller.GetState(0, G_RY_AXIS, ryStickState);
			float rxStickState = 0;
			controller.GetState(0, G_RX_AXIS, rxStickState);

			//Spin along the Y axis
			float Total_Pitch = FOV * (mouseDY / height) + ryStickState * -Thumb_Speed; // PITCH IS X
			GW::MATH::GMATRIXF pitchMatrix;
			mat_proxy.IdentityF(pitchMatrix);
			mat_proxy.RotateXLocalF(pitchMatrix, -Total_Pitch, pitchMatrix); //Inverted controls make the Total_Pitch positive
			mat_proxy.MultiplyMatrixF(pitchMatrix, camera, camera);

			float Total_Yaw = FOV * AspectRatio * (mouseDX / width) + rxStickState * Thumb_Speed; // YAW IS Y
			GW::MATH::GMATRIXF yawMatrix;
			mat_proxy.IdentityF(yawMatrix);
			mat_proxy.RotateYLocalF(yawMatrix, -Total_Yaw, yawMatrix); //Inverted controls make the Total_Yaw positive
			GW::MATH::GVECTORF pos = camera.row4; //Save the position of the vector (because the rotation will change it)
			mat_proxy.MultiplyMatrixF(camera, yawMatrix, camera);
			camera.row4 = pos; //Restore the position of the vector (because the rotation changed it)
			shaderMats.cameraPos = pos;
		}

		//Get the view matrix
		mat_proxy.InverseF(camera, shaderMats.viewMatrix); //Store the new view matrix after the math has been done back into the desired variable
	}

	void Render()
	{
		startProgram(shaderExecutable); //Start the program

		//Main camera
		glViewport(0, 0, (GLsizei)width, (GLsizei)height);

		locShaderMats = glGetUniformBlockIndex(shaderExecutable, "SceneData"); //Getting the location index of the uniform buffer in the GPU side

		glBindBufferBase(GL_UNIFORM_BUFFER, 1, UBO); //Binding the buffer on the GPU side (in VRAM)

		glUniformBlockBinding(shaderExecutable, locShaderMats, 1); //Binding the block TO the buffer in VRAM

		//Update Camera
		glBindBuffer(GL_UNIFORM_BUFFER, UBO); //Bind the SCENE_DATA UBO for editing
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SCENE_DATA), &shaderMats); //Edit the SCENE_DATA UBO to the newly adjusted camera value (which will be the only thing that changes on this side)

		models.RenderLevel(shaderExecutable); //Renders the level

		//Rudimentary minimap
		glViewport((GLsizei)width / 2, (GLsizei)height / 2, (GLsizei)width/2, (GLsizei)height /2);

		locShaderMats = glGetUniformBlockIndex(shaderExecutable, "SceneData");

		glBindBufferBase(GL_UNIFORM_BUFFER, 1, UBO);

		glUniformBlockBinding(shaderExecutable, locShaderMats, 1);

		//Temporarily modify the camera to the camera of the minimap
		GW::MATH::GVECTORF tempPos = shaderMats.cameraPos; //Save the main screens camera
		GW::MATH::GMATRIXF tempView = shaderMats.viewMatrix; //Save the main screens view matrix

		glBindBuffer(GL_UNIFORM_BUFFER, UBO); //Bind the buffer for editing
		//glBufferSubData(GL_UNIFORM_BUFFER, (sizeof(GW::MATH::GVECTORF) * 2), (sizeof(GW::MATH::GMATRIXF)), &shaderMats.mmView); //Substitute the mm view in for the normal view
		//glBufferSubData(GL_UNIFORM_BUFFER, ((sizeof(GW::MATH::GVECTORF) * 2) + (sizeof(GW::MATH::GMATRIXF) * 2)), sizeof(GW::MATH::GVECTORF), &shaderMats.mmCameraPos); //Substitute the mm camera pos for the normal camera pos
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SCENE_DATA), &minimapMats);

		models.RenderLevel(shaderExecutable); //Render the level
		
		startProgram(0); // some video cards(cough Intel) need this set back to zero or they won't display
		
		DisplayImguiMenu();
	}

private:
	//Render helper functions
	void startProgram(GLuint shaderExecutable)
	{
		glUseProgram(shaderExecutable);
	}

public:
	~Renderer()
	{
		models.UnloadLevel(); //Destroys all instances created by LoadLevel()
	}
};

LONG_PTR Renderer::gatewareWndProc = 0;