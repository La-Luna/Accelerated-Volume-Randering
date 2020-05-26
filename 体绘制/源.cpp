#pragma comment(lib, "legacy_stdio_definitions.lib")
#if _MSC_VER>=1900  
#include "stdio.h"   
_ACRTIMP_ALT FILE* __cdecl __acrt_iob_func(unsigned);
#ifdef __cplusplus   
extern "C"
#endif   
FILE * __cdecl __iob_func(unsigned i) {
	return __acrt_iob_func(i);
}
#endif /* _MSC_VER>=1900 */  

#include <fstream>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include<ctime>
//#define GLUT_DISABLE_ATEXIT_HACK 
#include "GL/glew.h"
//#include"glfw3.h"
#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/glut.h"
#include "GL/glm/glm.hpp"
#include "GL/glm/gtc/matrix_transform.hpp"
#include "GL/glm/gtx/transform2.hpp"
#include "GL/glm/gtc/type_ptr.hpp"

#include"tiffio.h"

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
GLuint g_voltiffobj;

static int fps = 0;
static clock_t lasttime=clock();
static int framecount = 0;
static int render_compute = 0;


int width = 0;
int height = 0;
int depth = 0;
#define GREY   1   // 1-byte grey-level image or stack
#define GREY16 2   // 2-byte grey-level image or stack
#define COLOR  3   // 3-byte RGB image or stack
typedef struct
{
	int      kind;
	int      width;
	int      height;
	unsigned char* array;   // Array of pixel values lexicographically ordered on (y,x,c).
} Image;
typedef struct {
	int kind;
	int width;
	int height;
	int depth;
	unsigned char* array;
}Stack;
typedef struct __Stack
{
	struct __Stack* next;
	int             vsize;
	Stack           stack;
} _Stack;
typedef long long V3DLONG ;
static _Stack* Free_Stack_List = NULL;
static int    Stack_Offset, Stack_Inuse;
TIFF* Open_Tiff(const char* file_name, const char* mode)
{
	TIFF* tif = 0;

	tif = NULL;
	int retries = 3;
	while (retries > 0 && tif == NULL) {
		tif = TIFFOpen(file_name, mode);
		if (tif == NULL) {
			retries--;
			fprintf(stderr, "cannot open TIFF file %s remaining retries=%d", file_name, retries);

		}
	}
	return (tif);
}
void Free_Stack(Stack* stack)
{
	_Stack* object = (_Stack*)(((char*)stack) - Stack_Offset);
	object->next = Free_Stack_List;
	Free_Stack_List = object;
	Stack_Inuse -= 1;
}
static int determine_kind(TIFF* tif)   //  Determine nature of current tif image
{
	short bits, channels, photo;

	TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits);
	TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &channels);
	TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photo);
	if (photo <= 1)
	{
		if (channels > 1) int foo;
		//error("Black and white tiff has more than 1 channel!",NULL); //TIFFTAG_PHOTOMETRIC indicates the photometric interpretation of the data, wherein 0 means black is 0 and lighter intensity is larger values.
		  // this doesn't preclude multiple channel data.  In fact, if it were 1, it would mean inverted photometric interpretation.  see www.awaresystems.be/imaging/tiff/tifftags/photometricinterpretation.html
		if (bits == 16)
			return (GREY16);
		else
			return (GREY);
	}
	else
		return (COLOR);
}
void* Guarded_Malloc(int size, const char* routine)
{
	void* p;

	p = malloc(size);
	if (p == NULL)
	{
		fprintf(stderr, "\nError in %s:\n", routine);
		fprintf(stderr, "   Out of memory\n");
		return 0;
	}
	return (p);
}
void* Guarded_Realloc(void* p, int size, const char* routine)
{
	p = realloc(p, size);
	if (p == NULL)
	{
		fprintf(stderr, "\nError in %s:\n", routine);
		fprintf(stderr, "   Out of memory\n");
		return 0;
	}
	return (p);
}
static inline Stack* new_stack(int vsize, const char* routine)
{
	_Stack* object;

	if (Free_Stack_List == NULL)
	{
		object = (_Stack*)Guarded_Malloc(sizeof(_Stack), routine);
		Stack_Offset = ((char*)&(object->stack)) - ((char*)object);
		object->vsize = vsize;
		object->stack.array = (uint8*)Guarded_Malloc(vsize, routine);
		Stack_Inuse += 1;
	}
	else
	{
		object = Free_Stack_List;
		Free_Stack_List = object->next;
		if (object->vsize < vsize)
		{
			object->vsize = vsize;
			object->stack.array = (uint8*)Guarded_Realloc(object->stack.array,
				vsize, routine);
		}
	}
	return (&(object->stack));
}
static uint32* get_raster(int npixels, const char* routine)
{
	static uint32* Raster = NULL;
	static int     Raster_Size = 0;                           //  Manage read work buffer

	if (npixels < 0)
	{
		free(Raster);
		Raster_Size = 0;
		Raster = NULL;
	}
	else if (npixels > Raster_Size)
	{
		Raster_Size = npixels;
		Raster = (uint32*)Guarded_Realloc(Raster, sizeof(uint32) * Raster_Size, routine);
	}
	return (Raster);
}
static int error(const char* msg, const char* arg)
{
	fprintf(stderr, "\nError in TIFF library:\n   ");
	fprintf(stderr, msg, arg);
	fprintf(stderr, "\n");
	return 1;
}
#define STACK_PIXEL_8(img,x,y,z,c) \
      ((uint8  *) ((img)->array +   \
           ((((z)*(img)->height + (y))*(img)->width + (x))*(img)->kind + (c))))

#define STACK_PIXEL_16(img,x,y,z,c) \
      ((uint16 *) ((img)->array +   \
           ((((z)*(img)->height + (y))*(img)->width + (x))*(img)->kind + (c))))

static inline int Get_Stack_Pixel(Stack* stack, int x, int y, int z, int c)
{
	if (stack->kind == GREY16)
		return (*STACK_PIXEL_16(stack, x, y, z, c));
	else
		return (*STACK_PIXEL_8(stack, x, y, z, c));
}
static void read_directory(TIFF* tif, Image* image, const char* routine) {
	unsigned int* raster;
	unsigned char* row;
	int width, height;

	width = image->width;
	height = image->height;
	raster = get_raster(width * height, routine);
	row = image->array;

	if (image->kind != GREY16)

	{
		int i, j;
		uint32* in;
		uint8* out;

		if (TIFFReadRGBAImage(tif, width, height, raster, 0) == 0)
			error("read of tif failed in read_directory()", NULL);

		in = raster;
		if (image->kind == GREY)
		{
			for (j = height - 1; j >= 0; j--)
			{
				out = row;
				for (i = 0; i < width; i++)
				{
					uint32 pixel = *in++;
					*out++ = TIFFGetR(pixel);
				}
				row += width;
			}
		}
		else
		{
			for (j = height - 1; j >= 0; j--)
			{
				out = row;
				for (i = 0; i < width; i++)
				{
					uint32 pixel = *in++;
					*out++ = TIFFGetR(pixel);
					*out++ = TIFFGetG(pixel);
					*out++ = TIFFGetB(pixel);
				}
				row += width * 3;
			}
		}
	}

	else

	{
		int tile_width, tile_height;

		if (TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tile_width))    // File is tiled  
		{
			int x, y;
			int i, j;
			int m, n;
			uint16* buffer = (uint16*)raster;
			uint16* out, * in /*, *rous */;

			TIFFGetField(tif, TIFFTAG_TILELENGTH, &tile_height);

			for (y = 0; y < height; y += tile_height)
			{
				if (y + tile_height > height)
					n = height - y;
				else
					n = tile_height;
				for (x = 0; x < width; x += tile_width)
				{
					TIFFReadTile(tif, buffer, x, y, 0, 0);
					if (x + tile_width > width)
						m = width - x;
					else
						m = tile_width;
					for (j = 0; j < n; j++)
					{
						out = (uint16*)(row + 2 * (j * width + x));
						in = buffer + j * tile_width;
						for (i = 0; i < m; i++)
							*out++ = *in++;
					}
				}
				row += n * width * 2;
			}
		}

		else    // File is striped

		{
			int     y;

			for (y = 0; y < height; y++)
			{
				TIFFReadScanline(tif, row, y, 0);
				row += width * 2;
			}
		}
	}
}
void Kill_Stack(Stack* stack)
{
	free(stack->array);
	free(((char*)stack) - Stack_Offset);
	//Stack_Inuse -= 1;
}
Image* Select_Plane(Stack* a_stack, int plane)  // Build an image for a plane of a stack
{
	static Image My_Image;

	if (plane < 0 || plane >= a_stack->depth)
		return (NULL);
	My_Image.kind = a_stack->kind;
	My_Image.width = a_stack->width;
	My_Image.height = a_stack->height;
	My_Image.array = a_stack->array + plane * a_stack->width * a_stack->height * a_stack->kind;
	return (&My_Image);
}
Stack* Read_Stack(const char* file_name) {
	Stack* stack = 0;
	TIFF* tif = 0;
	int depth, width, height, kind;
	tif = Open_Tiff(file_name, "r");
	if (!tif)return 0;
	depth = 1;
	while (TIFFReadDirectory(tif))depth += 1;
	TIFFClose(tif);

	tif = Open_Tiff(file_name, "r");
	if (!tif)return 0;

	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

	kind = determine_kind(tif);
	stack = new_stack(depth * height * width * kind, "Read_Stack");

	stack->width = width;
	stack->height = height; 
	stack->depth = depth;
	stack->kind = kind;
	//cout << "the image size is" << width << " " << height << " " << depth << endl;

	{
		int d = 0;
		while (1) {
			read_directory(tif, Select_Plane(stack, d), "Read_Stack");
			d += 1;

			if (!TIFFReadDirectory(tif))break;
			TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
			TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
			if (width != stack->width || height != stack->height) {
				error("image of stack are not of the same dimensions!", NULL);
				return 0;
			}
			kind = determine_kind(tif);
			if (kind != stack->kind) {
				error("image of stack are not of the same type(GRAY,GRAY16,COLOR)!", NULL);
				return 0;
			}
		
		}
	}
	TIFFClose(tif);
	return stack;
}
bool loadTif2Stack(const char* filename, unsigned char*& img, long long* &sz, int &datatype) {
	//img是最重要的参数，保存了图像的数据地址
	//img即data;
	//glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, w, h, d, 0, GL_RED, GL_UNSIGNED_BYTE, (GLubyte *)img4d->data)
	int b_error = 0;
	FILE* tmp;
	errno_t err = fopen_s(&tmp,filename, "r");
	//err = fopen_s(&fp, filename, "r");
	if (err)
	{
		std::cout << "Error: opening .tff file failed" << endl;
		exit(EXIT_FAILURE);
	} 
	else
	{
		std::cout << "OK: open .tff file successed" << endl;
	}

	Stack* tmpstack = Read_Stack(filename);
	if (!tmpstack) {
		b_error = 1;
		return b_error;
	}
	unsigned int size = tmpstack->width * tmpstack->height * tmpstack->depth;
	img= new GLubyte[size];
	width = tmpstack->width;
	height = tmpstack->height ;
	depth = tmpstack->depth ;
	//convert to PHC's format 
		//convert to hanchuan's format
	if (sz) { delete sz; sz = 0; }
	if (img) { delete img; img = 0; }

	sz = new V3DLONG[4];
	if (sz)
	{
		sz[0] = tmpstack->width;
		sz[1] = tmpstack->height;
		sz[2] = tmpstack->depth;
		cout << "the image size from tiff library is:";
		cout << sz[0] << " " << sz[1] << " " << sz[2]<<endl;
		switch (tmpstack->kind)
		{
		case GREY:
			sz[3] = 1;
			datatype = 1;
			break;

		case GREY16:
			sz[3] = 1;
			datatype = 2;
			break;

		case COLOR:
			sz[3] = 3;
			datatype = 1;
			break;

		default:
			printf("The type of tif file is not supported in this version.\n");
			if (sz) { delete sz; sz = 0; }
			Kill_Stack(tmpstack); tmpstack = 0;
			break;
		}
	}
	else
	{
		printf("Unable to allocate memory for the size varable! Return.\n");
		if (tmpstack)
		{
			Kill_Stack(tmpstack);
			tmpstack = 0;
		}
		b_error = 1;
		return b_error;
	}

	img = new unsigned char[sz[0] * sz[1] * sz[2] * sz[3] * datatype];
	if (!img)
	{
		printf("Unable to allocate memory for the image varable! Return.\n");
		if (tmpstack) { Kill_Stack(tmpstack);	tmpstack = 0; }
		if (sz) { delete sz; sz = 0; }
		b_error = 1;
		return b_error;
	}
	else
	{
		V3DLONG i, j, k, c;
		V3DLONG pgsz1 = sz[2] * sz[1] * sz[0], pgsz2 = sz[1] * sz[0], pgsz3 = sz[0];

		switch (tmpstack->kind)
		{
		case GREY:
		case COLOR:
			for (c = 0; c < sz[3]; c++)
				for (k = 0; k < sz[2]; k++)
					for (j = 0; j < sz[1]; j++)
						for (i = 0; i < sz[0]; i++)
							//img[c*pgsz1 + k*pgsz2 + j*pgsz3 + i] = STACK_PIXEL_8(tmpstack,i,j,k,c);
							img[c * pgsz1 + k * pgsz2 + j * pgsz3 + i] = Get_Stack_Pixel(tmpstack, i, j, k, c);

			break;

		case GREY16:
		{
			unsigned short int* img16 = (unsigned short int*)img;
			for (c = 0; c < sz[3]; c++)
				for (k = 0; k < sz[2]; k++)
					for (j = 0; j < sz[1]; j++)
						for (i = 0; i < sz[0]; i++)
							//img[c*pgsz1 + k*pgsz2 + j*pgsz3 + i] = STACK_PIXEL_16(tmpstack,i,j,k,c)>>4; //assume it is 12-bit
							//img[c*pgsz1 + k*pgsz2 + j*pgsz3 + i] = Get_Stack_Pixel(tmpstack,i,j,k,c)>>4; //assume it is 12-bit
							img16[c * pgsz1 + k * pgsz2 + j * pgsz3 + i] = Get_Stack_Pixel(tmpstack, i, j, k, c); //do not assume anything. 080930
		}

		break;

		default:
			printf("The type of tif file is not supported in this version.\n");
			if (sz) { delete sz; sz = 0; }
			if (img) { delete img; img = 0; }
			Kill_Stack(tmpstack); tmpstack = 0;
			b_error = 1;
			return b_error;
			break;
		}
	}


	// kill stack
	if (tmpstack)
	{
		Kill_Stack(tmpstack);
		tmpstack = 0;
	}
	return b_error;
}

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
	
	GLuint uboIndex;
	GLint uboSize;
	GLuint ubo;
	char* block_buffer;

	uboIndex = glGetUniformBlockIndex(g_programOCC, "datafromfile");
	glGetActiveUniformBlockiv(g_programOCC, uboIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &uboSize);
	block_buffer = (char*)malloc(uboSize);
	if (block_buffer == NULL) {
		fprintf(stderr, "unable to allocate buffer;\n");
		exit(EXIT_FAILURE);
	}
	else {
		enum {Block_size, DimDst,DimSrc, NumUniforms };
		GLfloat block_size[3] = {4.0,4.0,4.0};
		GLint dimDst[3] = {width/4,height/4,depth/4};
		GLint dimSrc[3] = { width,height,depth };

		const char* names[NumUniforms] = {
			"block_size","dimDst","dimSrc"
		};
		GLuint indices[NumUniforms];
		GLint size[NumUniforms];
		GLint offset[NumUniforms];
		GLint type[NumUniforms];
		glGetUniformIndices(g_programOCC, NumUniforms, names, indices);
		glGetActiveUniformsiv(g_programOCC, NumUniforms, indices, GL_UNIFORM_OFFSET, offset);
		glGetActiveUniformsiv(g_programOCC, NumUniforms, indices, GL_UNIFORM_SIZE, size);
		glGetActiveUniformsiv(g_programOCC, NumUniforms, indices, GL_UNIFORM_TYPE, type);
		memcpy(block_buffer + offset[Block_size], &block_size, size[Block_size] * 3 * sizeof(GLfloat));
		memcpy(block_buffer + offset[DimDst], &dimDst, size[DimDst] * 3 *sizeof(GLint));
		memcpy(block_buffer + offset[DimSrc], &dimSrc, size[DimSrc] * 3 * sizeof(GLint));

		glGenBuffers(1, &ubo);
		glBindBuffer(GL_UNIFORM_BUFFER, ubo);
		glBufferData(GL_UNIFORM_BUFFER, uboSize, block_buffer, GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, uboIndex, ubo);

	}

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

	glDispatchCompute(10,10,10);////////////////////////////////?
	GL_ERROR();
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	GL_ERROR();
	glUseProgram(0);


}
void runcomputeshader_dis() {
	//1ST

	glUseProgram(g_programDIS);
	//glUniform1i(stageLocation, t);
	int stageLocation0 = glGetUniformLocation(g_programDIS, "stage");
	int t0 = 0;
	if (stageLocation0 >= 0) {

		glUniform1i(stageLocation0, t0);
	}
	else
	{
		std::cout << "stage"
			<< "is not bind to the uniform"
			<< endl;
	}
	glBindImageTexture(0, g_distancemap, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	glBindImageTexture(1, g_occupancymap, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA8);
	glDispatchCompute(10,10, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	glUseProgram(0);
	GL_ERROR();
	//2ST
	glUseProgram(g_programDIS);
	int stageLocation1 = glGetUniformLocation(g_programDIS, "stage");
	int t1 = 1;
	if (stageLocation1 >= 0) {

		glUniform1i(stageLocation1, t1);
	}
	else
	{
		std::cout << "stage"
			<< "is not bind to the uniform"
			<< endl;
	}

	GL_ERROR();
	glBindImageTexture(0, g_distancemap, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA8);
	glBindImageTexture(1, g_occupancymap, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
	glDispatchCompute(10, 10, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	glUseProgram(0);
	//3ST
	glUseProgram(g_programDIS);
	int stageLocation2 = glGetUniformLocation(g_programDIS, "stage");
	int t2 = 2;
	if (stageLocation2 >= 0) {

		glUniform1i(stageLocation2, t2);
	}
	else
	{
		std::cout << "stage"
			<< "is not bind to the uniform"
			<< endl;
	}
	GL_ERROR();
	glBindImageTexture(0, g_distancemap, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
	glBindImageTexture(1, g_occupancymap, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA8);
	glDispatchCompute(10, 10, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	glUseProgram(0);
	GL_ERROR();
}
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
	if (render_compute == 0) {
		//compute shader occupancy_map
		runcomputeshader_occu();
		GL_ERROR();
		//compute shader distance_map
		runcomputeshader_dis();
		render_compute = 1;
	}

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
GLuint initVol3DTex(const char* filename);
// both of the two pass use the "render() function"
// the first pass render the backface of the boundbox
// the second pass render the frontface of the boundbox
// together with the frontface, use the backface as a 2D texture in the second pass
// to calculate the entry point and the exit point of the ray in and out the box.
GLuint initOccupancyTex() {
	GLuint occu;
	glGenTextures(1, &occu);
	glBindTexture(GL_TEXTURE_3D, occu);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, width/4, height/4,depth/4);
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
	
	g_volTexObj = initVol3DTex("test.tif");

	GL_ERROR();
	g_occupancymap = initOccupancyTex();
	GL_ERROR();
	g_distancemap = initOccupancyTex();
	GL_ERROR();
	initFrameBuffer(g_bfTexObj, g_texWidth, g_texHeight);
	GL_ERROR();
	//initest();

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
GLuint initVol3DTex(const char* filename)
{

	FILE* fp;
	long long *sz=0;
	int datatype;
	errno_t err;
	GLubyte* data;			  // 8bit
	/*err = fopen_s(&fp, filename, "r");
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
	fclose(fp);*/
	loadTif2Stack(filename,data,sz,datatype);//sz[0/1/2/3]=width/height/depth/kind

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
	glTexImage3D(GL_TEXTURE_3D, 0, GL_INTENSITY, width, height, depth, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
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
	//init uniform block
	GLuint uboIndex;
	GLint uboSize;
	GLuint ubo;
	char* block_buffer;

	uboIndex = glGetUniformBlockIndex(g_programHandle, "datafromfile");
	glGetActiveUniformBlockiv(g_programHandle, uboIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &uboSize);
	block_buffer =(char *)malloc(uboSize);
	if (block_buffer == NULL) {
		fprintf(stderr, "unable to allocate buffer;\n");
		exit(EXIT_FAILURE);
	}
	else {
		enum {Dim,Sampling_factor,Dim_distance_map,Step_size,Screen_size,NumUniforms};
		GLint dim[3] = { width,height,depth };
		GLfloat sampling_factor = 6.0;
		GLint dim_distance_map[3] = { width / 4,height / 4,depth / 4 };
		GLfloat step_size = 0.001;
		GLfloat screen_size[2] = { 800 ,800 };
		const char* names[NumUniforms] = {
			"dim","sampling_factor","dim_distance_map","StepSize","ScreenSize"
		};
		GLuint indices[NumUniforms];
		GLint size[NumUniforms];
		GLint offset[NumUniforms];
		GLint type[NumUniforms];
		glGetUniformIndices(g_programHandle, NumUniforms, names, indices);
		glGetActiveUniformsiv(g_programHandle, NumUniforms, indices, GL_UNIFORM_OFFSET, offset);
		glGetActiveUniformsiv(g_programHandle, NumUniforms, indices, GL_UNIFORM_SIZE, size);
		glGetActiveUniformsiv(g_programHandle, NumUniforms, indices, GL_UNIFORM_TYPE, type);
		memcpy(block_buffer + offset[Dim], &dim, size[Dim] * 3*sizeof(GLint));
		memcpy(block_buffer + offset[Sampling_factor], &sampling_factor, size[Sampling_factor] * sizeof(GLfloat));
		memcpy(block_buffer + offset[Dim_distance_map], &dim_distance_map, size[Dim_distance_map] * 3 * sizeof(GLint));
		memcpy(block_buffer + offset[Step_size], &step_size, size[Step_size] * sizeof(GLfloat));
		memcpy(block_buffer + offset[Screen_size], &screen_size, size[Screen_size] * 2 * sizeof(GLfloat));
		glGenBuffers(1, &ubo);
		glBindBuffer(GL_UNIFORM_BUFFER, ubo);
		glBufferData(GL_UNIFORM_BUFFER, uboSize, block_buffer, GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, uboIndex, ubo);

	}
	
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
		glBindTexture(GL_TEXTURE_3D, g_distancemap);
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
	/*GLint data;
	GLint index = 0;
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,index,&data);
	cout << "GL_MAX_COMPUTE_WORK_GROUP_SIZE:" << data << endl;*/
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

