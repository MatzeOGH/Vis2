#ifndef SHADER_STRUCTURES_GLSL

// ###### UNIFORMS AND PUSH CONSTANTS ###############
// Uniform buffer struct, containing camera matrices and user input:
// It is updated every frame.
struct matrices_and_user_input {
	// view matrix as returned from quake_camera
	mat4 mViewMatrix;
	// projection matrix as returned from quake_camera
	mat4 mProjMatrix;
	// transformation matrix which tranforms to camera's position
	vec4 mCamPos;
	// Facing Vector of camera
	vec4 mCamDir;
	// background color
	vec4 mClearColor;
	// The color of the 2d debug lines if activated
	vec4 mHelperLineColor;
	vec4 kBufferInfo; // contains resx, resy and kbuffer levels
	// If actived the lines will be colored according to the vertex colors
	bool mUseVertexColorForHelperLines;
	bool mShowBillboards;
	vec2 buf2; // dunno if necessary
};

#define SHADER_STRUCTURES_GLSL 1
#endif

