#include <fstream>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include<ctime>
//#define GLUT_DISABLE_ATEXIT_HACK 
#include "GL/glew.h"
#include"GL/glfw3.h"
#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/glut.h"
#include "GL/glm/glm.hpp"
#include "GL/glm/gtc/matrix_transform.hpp"
#include "GL/glm/gtx/transform2.hpp"
#include "GL/glm/gtc/type_ptr.hpp"


#define GL_ERROR() checkForOpenGLError(__FILE__, __LINE__)
using namespace std;
using glm::mat4;
using glm::vec3;
GLuint g_vao;
GLuint g_programHandle;
GLuint g_programOCC;
GLuint g_programDIS;
GLuint g_winWidth = 800;
GLuint g_winHeight = 800;
GLint g_angle = 0;
GLuint g_frameBuffer;
// transfer function
GLuint g_tffTexObj;
GLuint g_bfTexObj;
GLuint g_texWidth;
GLuint g_texHeight;
GLuint g_volTexObj;
GLuint g_occupancymap;
GLuint g_distancemap;
GLuint g_rcVertHandle;
GLuint g_rcFragHandle;
GLuint g_bfVertHandle;
GLuint g_bfFragHandle;
GLuint g_compute_occ;
GLuint g_compute_dis;
GLuint shaderProgram;
GLuint VAO;
float g_stepSize = 0.001f;
static int fps = 0;
static clock_t lasttime=clock();
static int framecount = 0;

int checkForOpenGLError(const char* file, int line)
{
	// return 1 if an OpenGL error occured, 0 otherwise.
	GLenum glErr;
	int retCode = 0;

	glErr = glGetError();
	while (glErr != GL_NO_ERROR)
	{
		std::cout << "glError in file " << file
			<< "@line " << line << gluErrorString(glErr) << endl;
		retCode = 1;
		exit(EXIT_FAILURE);
	}
	return retCode;
}
void keyboard(unsigned char key, int x, int y);
// the color of the vertex in the back face is also the location
// of the vertex
// save the back face to the user defined framebuffer bound
// with a 2D texture named `g_bfTexObj`
// draw the front face of the box
// in the rendering process, i.e. the ray marching process
// loading the volume `g_volTexObj` as well as the `g_bfTexObj`
// after vertex shader processing we got the color as well as the location of
// the vertex (in the object coordinates, before transformation).
// and the vertex assemblied into primitives before entering
// fragment shader processing stage.
// in fragment shader processing stage. we got `g_bfTexObj`
// (correspond to 'VolumeTex' in glsl)and `g_volTexObj`(correspond to 'ExitPoints')
// as well as the location of primitives.
// the most important is that we got the GLSL to exec the logic. Here we go!
// draw the back face of the box
void runcomputeshader_occu() {
	glUseProgram(g_programOCC);
	GL_ERROR();
	glBindImageTexture(0, g_occupancymap, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
	GLint transferFuncLoc = glGetUniformLocation(g_programOCC, "transfer_function");
	if (transferFuncLoc >= 0)
	{
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_1D, g_tffTexObj);
		glUniform1i(transferFuncLoc, 1);
	}
	else
	{
		std::cout << "TransferFunc"
			<< "is not bind to the uniform"
			<< endl;
	}

	GLint volumeLoc = glGetUniformLocation(g_programOCC, "volume");
	if (volumeLoc >= 0)
	{
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_3D, g_volTexObj);
		//glBindImageTexture(1, g_volTexObj, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
		glUniform1i(volumeLoc, 2);
	}
	else
	{
		std::cout << "Volume"
			<< "is not bind to the uniform"
			<< endl;
	}
	//glBindImageTexture(0, g_volTexObj, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
	//GL_ERROR();
	//glBindImageTexture(1, g_tffTexObj, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
	//GL_ERROR();
	//glBindImageTexture(2, g_occupancymap, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8UI);
	//GL_ERROR();
	glDispatchCompute(64,64,56);////////////////////////////////?
	GL_ERROR();
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	GL_ERROR();
	//glUseProgram(0);
}
void runcomputeshader_dis() {
	//1ST

	glUseProgram(g_programDIS);
	//glUniform1i(stageLocation, t);
	int stageLocation = glGetUniformLocation(g_programDIS, "stage");
	int t = 0;
	if (stageLocation >= 0) {

		glUniform1i(stageLocation, t);
	}
	else
	{
		std::cout << "stage"
			<< "is not bind to the uniform"
			<< endl;
	}
	glBindImageTexture(0, g_distancemap, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	glBindImageTexture(1, g_occupancymap, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	glDispatchCompute(8, 8, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	glUseProgram(0);
	GL_ERROR();
	//2ST
	//int stageLocation = glGetUniformLocation(g_programDIS, "stage");
	 t = 1;
	stageLocation = glGetUniformLocation(g_programDIS, "stage");
	glUseProgram(g_programDIS);
	glUniform1i(stageLocation, t);
	glBindImageTexture(0, g_distancemap, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	glBindImageTexture(1, g_occupancymap, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	glDispatchCompute(8, 8, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	glUseProgram(0);
	//3ST
	//int stageLocation = glGetUniformLocation(g_programDIS, "stage");
	 t = 2;
	stageLocation = glGetUniformLocation(g_programDIS, "stage");
	glUseProgram(g_programDIS);
	glUniform1i(stageLocation, t);
	glBindImageTexture(0, g_distancemap, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	glBindImageTexture(1, g_occupancymap, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	glDispatchCompute(8, 8, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	glUseProgram(0);
}

//when you want to test your computeshader ,you can use this function--initest()and test().but in real rendering ,we didn't use it.
void initest() {
	const char *vertexShaderSource = "#version 430 core\n"
		"\n"
		"in vec3 vert;\n"
		"out vec3 vert_col;\n"
		"uniform mat4 MVP;"
		"\n"
		"void main(void)\n"
		"{\n"
		"    gl_Position = MVP*vec4(vert.x,vert.y,vert.z,1.0);\n"
		//"    gl_Position =vec4(vert.x,vert.y,vert.z,1.0);\n"
		"	 vert_col=vec3(vert.x,vert.y,vert.z);"
		"}\n";
	const char *fragmentShaderSource = "#version 430 core\n"
		"\n"
		"uniform sampler3D g_occupancymap;\n"
		"out vec4 color;\n"
		"\n"
		"in vec3 vert_col;\n"
		"void main(void)\n"
		"{\n"
		"color=texture(g_occupancymap,vert_col);"
		//"color=vec4(1,0,0,0);"
		"}\n";
	int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);
	// check for shader compile errors
	int success;
	char infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

	int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog <<
			std::endl;
	}
	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "ERROR:SHADER::PROGRAM::LINKING_FAILED\n" << infoLog <<
			std::endl;
	}
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	float vertices[] = {
		-0.5f, -0.5f, -0.5f, 
		 0.5f, -0.5f, -0.5f, 
		 0.5f,  0.5f, -0.5f,  
		 0.5f,  0.5f, -0.5f,  
		-0.5f,  0.5f, -0.5f,  
		-0.5f, -0.5f, -0.5f, 

		-0.5f, -0.5f,  0.5f,  
		 0.5f, -0.5f,  0.5f,  
		 0.5f,  0.5f,  0.5f,  
		 0.5f,  0.5f,  0.5f,  
		-0.5f,  0.5f,  0.5f,  
		-0.5f, -0.5f,  0.5f,  

		-0.5f,  0.5f,  0.5f,  
		-0.5f,  0.5f, -0.5f,  
		-0.5f, -0.5f, -0.5f,  
		-0.5f, -0.5f, -0.5f, 
		-0.5f, -0.5f,  0.5f,  
		-0.5f,  0.5f,  0.5f,  

		 0.5f,  0.5f,  0.5f,  
		 0.5f,  0.5f, -0.5f,  
		 0.5f, -0.5f, -0.5f,  
		 0.5f, -0.5f, -0.5f, 
		 0.5f, -0.5f,  0.5f,  
		 0.5f,  0.5f,  0.5f, 

		-0.5f, -0.5f, -0.5f, 
		 0.5f, -0.5f, -0.5f, 
		 0.5f, -0.5f,  0.5f, 
		 0.5f, -0.5f,  0.5f,  
		-0.5f, -0.5f,  0.5f, 
		-0.5f, -0.5f, -0.5f, 

		-0.5f,  0.5f, -0.5f,  
		 0.5f,  0.5f, -0.5f, 
		 0.5f,  0.5f,  0.5f, 
		 0.5f,  0.5f,  0.5f,  
		-0.5f,  0.5f,  0.5f, 
		-0.5f,  0.5f, -0.5f
	};
	



	//vao,vbo登场！
	unsigned int VBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	//先绑定VAO，然后绑定VBO
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	//设置顶点属性
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	//glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3*sizeof(float)));
	//glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);//???
	glBindVertexArray(0);//????


}
void test() {
	GL_ERROR();
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);//状态设置函数
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		//glClear(GL_COLOR_BUFFER_BIT);//状态使用函数
		GL_ERROR();
		glUseProgram(shaderProgram);

		glm::mat4 projection = glm::perspective(60.0f, (GLfloat)g_winWidth / g_winHeight, 0.1f, 400.f);
		glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f),
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 model = mat4(1.0f);
		//model *= glm::rotate(-60.0f, glm::vec3(0.0f, 1.0f, 0.0f));
		// to make the "head256.raw" i.e. the volume data stand up.
		model *= glm::rotate(60.0f, vec3(1.0f, 0.0f, 0.0f));
		//model *= glm::translate(glm::vec3(-0.5f, -0.5f, -0.5f));
		// notice the multiplication order: reverse order of transform
		glm::mat4 mvp = projection * view * model;
		GLuint mvpIdx = glGetUniformLocation(shaderProgram, "MVP");
		if (mvpIdx >= 0)
		{
			glUniformMatrix4fv(mvpIdx, 1, GL_FALSE, &mvp[0][0]);
		}
		else
		{
			cerr << "can't get the MVP" << endl;
		}



		glActiveTexture(GL_TEXTURE0);
		//int texture_location = glGetUniformLocation(shaderProgram, "g_occupancymap");
		//glUniform1i(texture_location, 0);
		//glBindTexture(GL_TEXTURE_3D, volumetext);
		GL_ERROR();
		glBindTexture(GL_TEXTURE_3D, g_occupancymap);
		//glBindImageTexture(0, g_occupancymap, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8UI);

		GL_ERROR();
		//glBindImageTexture(0, g_occupancymap, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8UI);
		//float timeValue = glfwGetTime();
		//float greenValue = sin(timeValue) / 2.0f +0.5f;
		//int vertexColorLocation = glGetUniformLocation(shaderProgram, "ourColor");
		//glUniform4f(vertexColorLocation, 0.0f, greenValue, 0.0f, 1.0f);
		glBindVertexArray(VAO);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		GL_ERROR();
		//glutSwapBuffers(window);//交换颜色缓冲
		//glfwPollEvents();//查看事件
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		GL_ERROR();
	
}
void display(void)
{
	//const clock_t begin_time = clock();
	++framecount;
	void linkShader(GLuint shaderPgm, GLuint newVertHandle, GLuint newFragHandle);
	void render(GLenum cullFace);
	void rcSetUinforms();
	glEnable(GL_DEPTH_TEST);
	// test the gl_error
	//GL_ERROR();
	// render to texture
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_frameBuffer);

	GL_ERROR();
	//test();
	//实际渲染窗口的大小
	//glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, g_winWidth, g_winHeight);

	
	linkShader(g_programHandle, g_bfVertHandle, g_bfFragHandle);
	glUseProgram(g_programHandle);
	// cull front face
	render(GL_FRONT);
	glUseProgram(0);
	//GL_ERROR();
	//glBindFramebuffer(GL_FRAMEBUFFER, 0);
	runcomputeshader_occu();

	//test();
	GL_ERROR();
	runcomputeshader_dis();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, g_winWidth, g_winHeight);
	linkShader(g_programHandle, g_rcVertHandle, g_rcFragHandle);
	GL_ERROR();
	glUseProgram(g_programHandle);
	rcSetUinforms();
	GL_ERROR();
	render(GL_BACK);
	GL_ERROR();
	glUseProgram(0);
	GL_ERROR();
	clock_t curtime = clock();
	if (curtime - lasttime >= 1000) {
		fps = framecount;
		framecount = 0;
		lasttime = curtime;
		cout << "fps=" << fps<<endl;
	}
	glutSwapBuffers();
	
}
void initVBO();
void initShader();
void initFrameBuffer(GLuint, GLuint, GLuint);
GLuint initTFF1DTex(const char* filename);
GLuint initFace2DTex(GLuint texWidth, GLuint texHeight);
GLuint initVol3DTex(const char* filename, GLuint width, GLuint height, GLuint depth);
// both of the two pass use the "render() function"
// the first pass render the backface of the boundbox
// the second pass render the frontface of the boundbox
// together with the frontface, use the backface as a 2D texture in the second pass
// to calculate the entry point and the exit point of the ray in and out the box.
GLuint initOccupancyTex() {
	GLuint occu;
	glGenTextures(1, &occu);
	glBindTexture(GL_TEXTURE_3D, occu);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 64, 64, 56);
	glBindTexture(GL_TEXTURE_3D, 0);
	//glBindImageTexture(2, g_occupancymap, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8UI);
	return occu;
}

void render(GLenum cullFace)
{
	void drawBox(GLenum glFaces);
	GL_ERROR();
	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	//  transform the box
	glm::mat4 projection = glm::perspective(60.0f, (GLfloat)g_winWidth / g_winHeight, 0.1f, 400.f);
	glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 model = mat4(1.0f);
	model *= glm::rotate((float)g_angle, glm::vec3(0.0f, 1.0f, 0.0f));
	// to make the "head256.raw" i.e. the volume data stand up.
	model *= glm::rotate(90.0f, vec3(1.0f, 0.0f, 0.0f));
	model *= glm::translate(glm::vec3(-0.5f, -0.5f, -0.5f));
	// notice the multiplication order: reverse order of transform
	glm::mat4 mvp = projection * view * model;
	GLuint mvpIdx = glGetUniformLocation(g_programHandle, "MVP");
	if (mvpIdx >= 0)
	{
		glUniformMatrix4fv(mvpIdx, 1, GL_FALSE, &mvp[0][0]);
	}
	else
	{
		cerr << "can't get the MVP" << endl;
	}
	GL_ERROR();
	drawBox(cullFace);
	GL_ERROR();
	// glutWireTeapot(0.5);
}
void init()
{
	g_texWidth = g_winWidth;
	g_texHeight = g_winHeight;
	GL_ERROR();
	initVBO();
	GL_ERROR();
	initShader();
	GL_ERROR();
	g_tffTexObj = initTFF1DTex("tff.dat");
	GL_ERROR();
	g_bfTexObj = initFace2DTex(g_texWidth, g_texHeight);
	GL_ERROR();
	g_volTexObj = initVol3DTex("head256.raw", 256, 256, 225);
	GL_ERROR();
	g_occupancymap = initOccupancyTex();
	GL_ERROR();
	g_distancemap = initOccupancyTex();
	GL_ERROR();
	initFrameBuffer(g_bfTexObj, g_texWidth, g_texHeight);
	GL_ERROR();
	initest();

}
// init the vertex buffer object
void initVBO()
{
	GLfloat vertices[24] = {
	0.0, 0.0, 0.0,
	0.0, 0.0, 1.0,
	0.0, 1.0, 0.0,
	0.0, 1.0, 1.0,
	1.0, 0.0, 0.0,
	1.0, 0.0, 1.0,
	1.0, 1.0, 0.0,
	1.0, 1.0, 1.0
	};
	// draw the six faces of the boundbox by drawwing triangles
	// draw it contra-clockwise
	// front: 1 5 7 3
	// back: 0 2 6 4
	// left：0 1 3 2
	// right:7 5 4 6    
	// up: 2 3 7 6
	// down: 1 0 4 5
	GLuint indices[36] = {
	1,5,7,
	7,3,1,
	0,2,6,
	6,4,0,
	0,1,3,
	3,2,0,
	7,5,4,
	4,6,7,
	2,3,7,
	7,6,2,
	1,0,4,
	4,5,1
	};
	GLuint gbo[2];

	glGenBuffers(2, gbo);
	GLuint vertexdat = gbo[0];
	GLuint veridxdat = gbo[1];
	glBindBuffer(GL_ARRAY_BUFFER, vertexdat);
	glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
	// used in glDrawElement()
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, veridxdat);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 36 * sizeof(GLuint), indices, GL_STATIC_DRAW);

	GLuint vao;
	glGenVertexArrays(1, &vao);
	// vao like a closure binding 3 buffer object: verlocdat vercoldat and veridxdat
	glBindVertexArray(vao);
	glEnableVertexAttribArray(0); // for vertexloc
	glEnableVertexAttribArray(1); // for vertexcol

	// the vertex location is the same as the vertex color
	glBindBuffer(GL_ARRAY_BUFFER, vertexdat);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLfloat*)NULL);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLfloat*)NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, veridxdat);
	// glBindVertexArray(0);
	g_vao = vao;
}
void drawBox(GLenum glFaces)
{
	//glBindImageTexture(0, g_distancemap, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI);
	glEnable(GL_CULL_FACE);
	glCullFace(glFaces);
	glBindVertexArray(g_vao);
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (GLuint*)NULL);
	glDisable(GL_CULL_FACE);
}
// check the compilation result
GLboolean compileCheck(GLuint shader)
{
	GLint err;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &err);
	if (GL_FALSE == err)
	{
		GLint logLen;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
		if (logLen > 0)
		{
			char* log = (char*)malloc(logLen);
			GLsizei written;
			glGetShaderInfoLog(shader, logLen, &written, log);
			cerr << "Shader log: " << log << endl;
			free(log);
		}
	}
	return err;
}
// init shader object
GLuint initShaderObj(const GLchar* srcfile, GLenum shaderType)
{
	ifstream inFile(srcfile, ifstream::in);
	// use assert?
	if (!inFile)
	{
		cerr << "Error openning file: " << srcfile << endl;
		exit(EXIT_FAILURE);
	}

	const int MAX_CNT = 10000;
	GLchar* shaderCode = (GLchar*)calloc(MAX_CNT, sizeof(GLchar));
	inFile.read(shaderCode, MAX_CNT);
	if (inFile.eof())
	{
		size_t bytecnt = inFile.gcount();
		*(shaderCode + bytecnt) = '\0';
	}
	else if (inFile.fail())
	{
		std::cout << srcfile << "read failed " << endl;
	}
	else
	{
		std::cout << srcfile << "is too large" << endl;
	}
	// create the shader Object
	GLuint shader = glCreateShader(shaderType);
	if (0 == shader)
	{
		cerr << "Error creating vertex shader." << endl;
	}
	// cout << shaderCode << endl;
	// cout << endl;
	const GLchar* codeArray[] = { shaderCode };
	glShaderSource(shader, 1, codeArray, NULL);
	free(shaderCode);

	// compile the shader
	glCompileShader(shader);
	if (GL_FALSE == compileCheck(shader))
	{
		cerr << "shader compilation failed" << endl;
	}
	return shader;
}
GLint checkShaderLinkStatus(GLuint pgmHandle)
{
	GLint status;
	glGetProgramiv(pgmHandle, GL_LINK_STATUS, &status);
	if (GL_FALSE == status)
	{
		GLint logLen;
		glGetProgramiv(pgmHandle, GL_INFO_LOG_LENGTH, &logLen);
		if (logLen > 0)
		{
			GLchar* log = (GLchar*)malloc(logLen);
			GLsizei written;
			glGetProgramInfoLog(pgmHandle, logLen, &written, log);
			cerr << "Program log: " << log << endl;
		}
	}
	return status;
}
// link shader program
GLuint createShaderPgm()
{
	// Create the shader program
	GLuint programHandle = glCreateProgram();
	if (0 == programHandle)
	{
		cerr << "Error create shader program" << endl;
		exit(EXIT_FAILURE);
	}
	return programHandle;
}


// init the 1 dimentional texture for transfer function
GLuint initTFF1DTex(const char* filename)
{
	// read in the user defined data of transfer function
	ifstream inFile(filename, ifstream::in);
	if (!inFile)
	{
		cerr << "Error openning file: " << filename << endl;
		exit(EXIT_FAILURE);
	}

	const int MAX_CNT = 10000;
	GLubyte* tff = (GLubyte*)calloc(MAX_CNT, sizeof(GLubyte));
	inFile.read(reinterpret_cast<char*>(tff), MAX_CNT);
	if (inFile.eof())
	{
		size_t bytecnt = inFile.gcount();
		*(tff + bytecnt) = '\0';
		std::cout << "bytecnt " << bytecnt << endl;
	}
	else if (inFile.fail())
	{
		std::cout << filename << "read failed " << endl;
	}
	else
	{
		std::cout << filename << "is too large" << endl;
	}
	GLuint tff1DTex;
	glGenTextures(1, &tff1DTex);
	glBindTexture(GL_TEXTURE_1D, tff1DTex);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	//glTexImage1D从缓存中载入纹理
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, tff);////////////////////////////？？？？？？？？？
	//glTexImage3D(GL_TEXTURE_3D, 0, GL_INTENSITY, w, h, d, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, 0);
	//glBindImageTexture(1, g_tffTexObj, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
	free(tff);
	return tff1DTex;
}


// init the 2D texture for render backface 'bf' stands for backface
GLuint initFace2DTex(GLuint bfTexWidth, GLuint bfTexHeight)
{
	GLuint backFace2DTex;
	glGenTextures(1, &backFace2DTex);
	glBindTexture(GL_TEXTURE_2D, backFace2DTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bfTexWidth, bfTexHeight, 0, GL_RGBA, GL_FLOAT, NULL);
	return backFace2DTex;
}
// init 3D texture to store the volume data used fo ray casting
GLuint initVol3DTex(const char* filename, GLuint w, GLuint h, GLuint d)
{

	FILE* fp;
	size_t size = w * h * d;
	errno_t err;
	GLubyte* data = new GLubyte[size];			  // 8bit
	err = fopen_s(&fp, filename, "r");
	if (err)
	{
		std::cout << "Error: opening .raw file failed" << endl;
		exit(EXIT_FAILURE);
	}
	else
	{
		std::cout << "OK: open .raw file successed" << endl;
	}
	if (fread(data, sizeof(char), size, fp) != size)
	{
		std::cout << "Error: read .raw file failed" << endl;//!
		//exit(1);
	}
	else
	{
		std::cout << "OK: read .raw file successed" << endl;
	}
	fclose(fp);

	glGenTextures(1, &g_volTexObj);
	// bind 3D texture target
	glBindTexture(GL_TEXTURE_3D, g_volTexObj);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	// pixel transfer happens here from client to OpenGL server
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_INTENSITY, w, h, d, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
	//glBindImageTexture(0, g_volTexObj, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8UI);
	delete[]data;
	std::cout << "volume texture created" << endl;
	return g_volTexObj;
}

void checkFramebufferStatus()
{
	GLenum complete = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (complete != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "framebuffer is not complete" << endl;
		exit(EXIT_FAILURE);
	}
}
// init the framebuffer, the only framebuffer used in this program
void initFrameBuffer(GLuint texObj, GLuint texWidth, GLuint texHeight)
{
	// create a depth buffer for our framebuffer
	GLuint depthBuffer;
	glGenRenderbuffers(1, &depthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, texWidth, texHeight);

	// attach the texture and the depth buffer to the framebuffer
	glGenFramebuffers(1, &g_frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, g_frameBuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texObj, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
	checkFramebufferStatus();
	glEnable(GL_DEPTH_TEST);
}

void rcSetUinforms()
{
	// setting uniforms such as
	// ScreenSize 
	// StepSize
	// TransferFunc
	// ExitPoints i.e. the backface, the backface hold the ExitPoints of ray casting
	// VolumeTex the texture that hold the volume data i.e. head256.raw
	GLint screenSizeLoc = glGetUniformLocation(g_programHandle, "ScreenSize");
	if (screenSizeLoc >= 0)
	{
		glUniform2f(screenSizeLoc, (float)g_winWidth, (float)g_winHeight);
	}
	else
	{
		std::cout << "ScreenSize"
			<< "is not bind to the uniform"
			<< endl;
	}
	GLint stepSizeLoc = glGetUniformLocation(g_programHandle, "StepSize");
	GL_ERROR();
	if (stepSizeLoc >= 0)
	{
		glUniform1f(stepSizeLoc, g_stepSize);
	}
	else
	{
		std::cout << "StepSize"
			<< "is not bind to the uniform"
			<< endl;
	}
	GL_ERROR();
	//GLint transferFuncLoc = -1;
	GLint transferFuncLoc = glGetUniformLocation(g_programHandle, "TransferFunc");
	if (transferFuncLoc >= 0)
	{
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_1D, g_tffTexObj);
		glUniform1i(transferFuncLoc, 2);
	}
	else
	{
		std::cout << "TransferFunc"
			<< "is not bind to the uniform"
			<< endl;
	}
	GL_ERROR();
	//GLint backFaceLoc = -1;
	GLint backFaceLoc = glGetUniformLocation(g_programHandle, "ExitPoints");
	if (backFaceLoc >= 0)
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, g_bfTexObj);
		glUniform1i(backFaceLoc, 0);
	}
	else
	{
		std::cout << "ExitPoints"
			<< "is not bind to the uniform"
			<< endl;
	}
	GL_ERROR();
	GLint volumeLoc = glGetUniformLocation(g_programHandle, "VolumeTex");
	if (volumeLoc >= 0)
	{
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D, g_volTexObj);
		//glBindImageTexture(1, g_volTexObj, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
		glUniform1i(volumeLoc, 1);
	}
	else
	{
		std::cout << "VolumeTex"
			<< "is not bind to the uniform"
			<< endl;
	}
	GLint distancemapLoc = glGetUniformLocation(g_programHandle, "distance_map");
	if (distancemapLoc >= 0)
	{
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_3D, g_distancemap);///////////////////////////////////////////////////////////////!
		glUniform1i(distancemapLoc, 3);
	}
	else
	{
		std::cout << "distancemap"
			<< "is not bind to the uniform"
			<< endl;
	}
	


}
// init the shader object and shader program
void initShader()
{
	// vertex shader object for first pass
	g_bfVertHandle = initShaderObj("shader/backface.vert", GL_VERTEX_SHADER);
	// fragment shader object for first pass
	g_bfFragHandle = initShaderObj("shader/backface.frag", GL_FRAGMENT_SHADER);
	// vertex shader object for second pass
	g_rcVertHandle = initShaderObj("shader/raycasting.vert", GL_VERTEX_SHADER);
	// fragment shader object for second pass
	g_rcFragHandle = initShaderObj("shader/renderring_Acc.frag", GL_FRAGMENT_SHADER);
	// create the shader program , use it in an appropriate time
	g_programHandle = createShaderPgm();
	// 获得由着色器编译器分配的索引(可选)
	//compute occupancy_map
	GL_ERROR();
	g_compute_occ = initShaderObj("shader/occupancy_map.comp", GL_COMPUTE_SHADER);
	g_programOCC = createShaderPgm();
	GL_ERROR();
	int success;
	char infoLog[512];
	glAttachShader(g_programOCC, g_compute_occ);
	glLinkProgram(g_programOCC);
	glGetProgramiv(g_programOCC, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(g_compute_occ, 512, NULL, infoLog);
		std::cout << "ERROR::OCC::COMPUTE::PROGRAM::LINKING_FAILED\n" << infoLog <<
			std::endl;
	}
	//compute distance_map
	g_compute_dis = initShaderObj("shader/distance_map.comp", GL_COMPUTE_SHADER);
	g_programDIS = createShaderPgm();
	glAttachShader(g_programDIS, g_compute_dis);
	glLinkProgram(g_programDIS);
	glGetProgramiv(g_programDIS, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(g_compute_occ, 512, NULL, infoLog);
		std::cout << "ERROR::DIS::COMPUTE::PROGRAM::LINKING_FAILED\n" << infoLog <<
			std::endl;
	}
}

// link the shader objects using the shader program
// so once the programme is triggerd,all the attached shaders will be exert.
void linkShader(GLuint shaderPgm, GLuint newVertHandle, GLuint newFragHandle)
{
	const GLsizei maxCount = 2;
	GLsizei count;
	GLuint shaders[maxCount];
	glGetAttachedShaders(shaderPgm, maxCount, &count, shaders);
	// cout << "get VertHandle: " << shaders[0] << endl;
	// cout << "get FragHandle: " << shaders[1] << endl;
	GL_ERROR();
	for (int i = 0; i < count; i++) {
		glDetachShader(shaderPgm, shaders[i]);
	}
	// Bind index 0 to the shader input variable "VerPos"
	glBindAttribLocation(shaderPgm, 0, "VerPos");
	// Bind index 1 to the shader input variable "VerClr"
	glBindAttribLocation(shaderPgm, 1, "VerClr");
	GL_ERROR();
	glAttachShader(shaderPgm, newVertHandle);
	glAttachShader(shaderPgm, newFragHandle);
	GL_ERROR();
	glLinkProgram(shaderPgm);
	if (GL_FALSE == checkShaderLinkStatus(shaderPgm))
	{
		cerr << "Failed to relink shader program!" << endl;
		exit(EXIT_FAILURE);
	}
	GL_ERROR();
}

void rotateDisplay()
{
	g_angle = (g_angle + 1) % 360;
	glutPostRedisplay();
}
void reshape(int w, int h)
{
	g_winWidth = w;
	g_winHeight = h;
	g_texWidth = w;
	g_texHeight = h;
}

void keyboard(unsigned char key, int x, int y)
{
	switch (key)
	{
	case '\x1B':
		exit(EXIT_SUCCESS);
		break;
	}
}

int main(int argc, char** argv)
{
	//初始化OpenGL环境
	glutInit(&argc, argv);

	//指定显示模型的类型，颜色模式数量，缓冲区类型
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);

	//窗口大小
	glutInitWindowSize(800, 800);

	//给窗口命名
	glutCreateWindow("GLUT Test");
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		/* Problem: glewInit failed, something is seriously wrong. */
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
	}

	//设置键盘回调函数，告诉窗口哪一个函数将会被调用来处理普通键盘消息
	glutKeyboardFunc(&keyboard);

	//绘图函数
	glutDisplayFunc(&display);

	//改变窗口大小时，保持图形比例
	glutReshapeFunc(&reshape);

	//程序在空闲时间调用rotateDisplay函数
	glutIdleFunc(&rotateDisplay);
	init();


	//让与“事件”有关的机制进入无限循环
	glutMainLoop();
	return EXIT_SUCCESS;
}

