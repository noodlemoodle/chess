// cpsc 453 assignment 3 qiyue zhang 10131658

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <math.h>

#define _USE_MATH_DEFINES
#define GLFW_INCLUDE_GLCOREARB
#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "glm/gtc/matrix_transform.hpp"
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "objmodel.h"

using namespace std;

OBJmodel om;

int windowWidth, windowHeight;

float lookAtX = 0.0, lookAtY = 0.0, lookAtZ = 0.0;
float camX = 0.0, camY = 5.0, camZ = 5.0;
float zoom = 90.0;
float up = 1.0;
bool upsideDown = false;

bool closeWindow = false;

// world coordinates
float worldX = 0, worldY = 0, worldZ = 0;
float scale = 1;
float yawAngle = 0, rollAngle = 0, pitchAngle = 0;

// texture mapping controls
int ao = 1, color = 1;
//
// //donut materials
// float Ns = 96.078431;
// vector<float> Ka = {0.8, 0.8, 0.8};
// vector<float> Kd = {0.64, 0.64, 0.64};
// vector<float> Ks = {0.5, 0.5, 0.5};
// vector<float> Ke = {0.0, 0.0, 0.0};
// float Ni = 1.0;
// float d = 1.0;
// float illum = 2.0;

glm::vec3 lightPos = glm::vec3(0, 0, 0);

bool CheckGLErrors();

vector<float> readMTL(string f) {
	if(f.empty()) {
		cerr << "ERROR: blank filename" << endl;
		exit(0);
	}
	// try to open the file
	ifstream file;
	file.open( f, fstream::in );
	// didn't work? fail!
	if ( file.fail() ) {
		cerr << "ERROR: OBJmodel: Couldn't load \"" << f << "\"." << endl;
		exit(0);
	}
	string buffer = "";
	string line;
	vector<float> mtl;
	int lineNum = 0;
	while( file.good() ) {
		getline( file, line );	// while we have data, read in a line
		if(lineNum >= 4 && lineNum <= 12) {
			switch( line[0] ) {
				case 'i':
					buffer += line.substr(6) + " ";
					lineNum++;
					continue;
				case 'd':
					buffer += line.substr(2) + " ";
					lineNum++;
					break;
				default:
					buffer += line.substr(3) + " "; lineNum++; continue;
			}
		}
		lineNum++;
	}

	istringstream iss(buffer);
	for (int i = 0; i < 16; i++ ) {
		float mtlVal;
		iss >> mtlVal;
		mtl.push_back(mtlVal);
	}

	return mtl;
}

class Program {
	GLuint vertex_shader;
	GLuint fragment_shader;
public:
	GLuint id;
	Program() {
		vertex_shader = 0;
		fragment_shader = 0;
		id = 0;
	}
	Program(string vertex_path, string fragment_path) {
		init(vertex_path, fragment_path);
	}
	void init(string vertex_path, string fragment_path) {

		id = glCreateProgram();
		vertex_shader = addShader(vertex_path, GL_VERTEX_SHADER);
		fragment_shader = addShader(fragment_path, GL_FRAGMENT_SHADER);

		if (vertex_shader)
			glAttachShader(id, vertex_shader);
		if (fragment_shader)
			glAttachShader(id, fragment_shader);

		glLinkProgram(id);
	}

	GLuint addShader(string path, GLuint type) {
		std::ifstream in(path);
		string buffer = [&in] {
			std::ostringstream ss {};
			ss << in.rdbuf();
			return ss.str();
		}();
		const char *buffer_array[] = { buffer.c_str() };
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, buffer_array, 0);
		glCompileShader(shader);

		// Compile results
		GLint status;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
		if (status == GL_FALSE) {
			GLint length;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
			string info(length, ' ');
			glGetShaderInfoLog(shader, info.length(), &length, &info[0]);
			cerr << "ERROR compiling shader:" << endl << endl;
			cerr << info << endl;
		}
		return shader;
	}
	~Program() {
		glUseProgram(0);
		glDeleteProgram(id);
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);
	}
};

class VertexArray {
	std::map<string, GLuint> buffers;
	std::map<string, int> indices;
public:
	GLuint id;
	unsigned int count;
	VertexArray(int c) {
		glGenVertexArrays(1, &id);
		count = c;
	}

	VertexArray(const VertexArray &v) {
		glGenVertexArrays(1, &id);

		// Copy data from the old object
		this->indices = std::map<string, int>(v.indices);
		count = v.count;

		vector<GLuint> temp_buffers(v.buffers.size());

		// Allocate some temporary buffer object handles
		glGenBuffers(v.buffers.size(), &temp_buffers[0]);

		// Copy each old VBO into a new VBO
		int i = 0;
		for (auto &ent : v.buffers) {
			int size = 0;
			glBindBuffer(GL_ARRAY_BUFFER, ent.second);
			glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);

			glBindBuffer(GL_COPY_READ_BUFFER, temp_buffers[i]);
			glBufferData(GL_COPY_READ_BUFFER, size, NULL, GL_STATIC_COPY);

			glCopyBufferSubData(GL_ARRAY_BUFFER, GL_COPY_READ_BUFFER, 0, 0,
					size);
			i++;
		}

		// Copy those temporary buffer objects into our VBOs

		i = 0;
		for (auto &ent : v.buffers) {
			GLuint buffer_id;
			int size = 0;
			int index = indices[ent.first];

			glGenBuffers(1, &buffer_id);

			glBindVertexArray(this->id);
			glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
			glBindBuffer(GL_COPY_READ_BUFFER, temp_buffers[i]);
			glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &size);

			// Allocate VBO memory and copy
			glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW);
			glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_ARRAY_BUFFER, 0, 0,
					size);
			string indexs = ent.first;

			buffers[ent.first] = buffer_id;
			indices[ent.first] = index;

			// Setup the attributes
			size = size / (sizeof(float) * this->count);
			glVertexAttribPointer(index, size, GL_FLOAT, GL_FALSE, 0, 0);
			glEnableVertexAttribArray(index);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
			i++;
		}

		// Delete temporary buffers
		glDeleteBuffers(v.buffers.size(), &temp_buffers[0]);
	}

	void addBuffer(string name, int index, vector<float> buffer) {
		GLuint buffer_id;
		glBindVertexArray(id);

		glGenBuffers(1, &buffer_id);
		glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
		glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(float),
				buffer.data(), GL_STATIC_DRAW);
		buffers[name] = buffer_id;
		indices[name] = index;

		int components = buffer.size() == 9 ? 3 : 2;
		glVertexAttribPointer(index, components, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(index);

		// unset states
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	void updateBuffer(string name, vector<float> buffer) {
		glBindBuffer(GL_ARRAY_BUFFER, buffers[name]);
		glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(float),
				buffer.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	~VertexArray() {
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &id);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		for (auto &ent : buffers)
			glDeleteBuffers(1, &ent.second);
	}
};

struct Texture {
	GLuint id;
	GLuint type;
	int w;
	int h;
	int channels;

	Texture() : id(0), type(0), w(0), h(0), channels(0){}

};

vector<float> findBound(vector<float> v) {
	float tempMaxX = 0;
	float tempMinX = 0;
	float tempMaxY = 0;
	float tempMinY = 0;
	float tempMinZ = 0;
	float tempMaxZ = 0;
	float centerX, centerY, centerZ;

	for (uint i = 0; i < v.size(); i+=4) {
		if(v[i] < tempMinX){
			tempMinX = v[i];
		}
		if(v[i] > tempMaxX){
			tempMaxX = v[i];
		}
		if(v[i+1] < tempMinY){
			tempMinY = v[i+1];
		}
		if(v[i+1] > tempMaxY){
			tempMaxY = v[i+1];
		}
		if(v[i+2] < tempMinZ){
			tempMinZ = v[i+2];
		}
		if(v[i+2] > tempMaxZ){
			tempMaxZ = v[i+2];
		}
	}
	centerX = (tempMinX + tempMaxX)/2;
	centerY = (tempMinY + tempMaxY)/2;
	centerZ = (tempMinZ + tempMaxZ)/2;
	return vector<float> {tempMinX, tempMaxX, tempMinY, tempMaxY, tempMinZ, tempMaxZ, centerX, centerY, centerZ};

}

class Obj {
public:
	string objFile;
	string colorPng;
	string aoPng;
	string mtlFile;
	float x = 0, y = 0, z = 0; //world coordinates
	vector<float> objVertices;
	vector<float> objTex;
	vector<float> objNorms;
	vector<float> materials;
	Texture aoTex;
	Texture colorTex;
	unsigned char* colorPixels;
	unsigned char* aoPixels;
	glm::mat4 modelMatrix = glm::mat4(1.0f);

	Obj(string obj, string color, string ao, string mtl) {
		objFile = obj;
		colorPng = color;
		aoPng = ao;
		mtlFile = mtl;
	}

	bool init() {
		if(!om.load(objFile)) {
			cout<<"failed"<<endl;
			return false;
		} else {
			// cout<<"done"<<endl;

			for(uint tri = 0; tri < om.triangleCount(); tri++) {
				for (uint vert = 0; vert < 3; vert++) {

					objVertices.push_back(om[tri].vertex[vert].pos.x);
					objVertices.push_back(om[tri].vertex[vert].pos.y);
					objVertices.push_back(om[tri].vertex[vert].pos.z);
					objVertices.push_back(om[tri].vertex[vert].pos.w);

					objTex.push_back(om[tri].vertex[vert].tex.s);
					objTex.push_back(om[tri].vertex[vert].tex.t);
					objTex.push_back(om[tri].vertex[vert].tex.u);

					objNorms.push_back(om[tri].vertex[vert].norm.x);
					objNorms.push_back(om[tri].vertex[vert].norm.y);
					objNorms.push_back(om[tri].vertex[vert].norm.z);
				}
			}

			 return true;
		}
	}

	void bindTextures(GLuint pid) {

		glGenTextures(1, &colorTex.id);

		stbi_set_flip_vertically_on_load(true);
		colorPixels = stbi_load(colorPng.c_str(), &colorTex.w, &colorTex.h, &colorTex.channels, 0);
		GLuint f11 = colorTex.channels == 3 ? GL_RGB8 : GL_RGBA8;
		GLuint f12 = colorTex.channels == 3 ? GL_RGB : GL_RGBA;

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, colorTex.id);
		glTexImage2D(GL_TEXTURE_2D, 0, f11, colorTex.w, colorTex.h, 0, f12, GL_UNSIGNED_BYTE, colorPixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		stbi_image_free(colorPixels);
		GLint colorLoc = glGetUniformLocation(pid, "tex");
		glUniform1i(colorLoc, 0);

		glGenTextures(1, &aoTex.id);

		stbi_set_flip_vertically_on_load(true);
		aoPixels = stbi_load(aoPng.c_str(), &aoTex.w, &aoTex.h, &aoTex.channels, 0);
		GLuint f21 = aoTex.channels == 3 ? GL_RGB8 : GL_RGBA8;
		GLuint f22 = aoTex.channels == 3 ? GL_RGB : GL_RGBA;
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, aoTex.id);
		glTexImage2D(GL_TEXTURE_2D, 0, f21, aoTex.w, aoTex.h, 0, f22, GL_UNSIGNED_BYTE, aoPixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		stbi_image_free(aoPixels);
		GLint aoLoc = glGetUniformLocation(pid, "ao");
		glUniform1i(aoLoc, 1);
	}

	void bindMaterials(GLuint pid) {
		materials = readMTL(mtlFile);
		GLint nsLoc = glGetUniformLocation(pid, "Ns");
		glUniform1f(nsLoc, materials[0]);

		GLint kaLoc = glGetUniformLocation(pid, "Ka");
		glUniform3f(kaLoc, materials[1], materials[2], materials[3]);

		GLint kdLoc = glGetUniformLocation(pid, "Kd");
		glUniform3f(kdLoc, materials[4], materials[5], materials[6]);

		GLint ksLoc = glGetUniformLocation(pid, "Ks");
		glUniform3f(ksLoc, materials[7], materials[8], materials[9]);

		GLint keLoc = glGetUniformLocation(pid, "Ke");
		glUniform3f(keLoc, materials[10], materials[11], materials[12]);

		GLint niLoc = glGetUniformLocation(pid, "Ni");
		glUniform1f(niLoc, materials[13]);

		GLint dLoc = glGetUniformLocation(pid, "d");
		glUniform1f(dLoc, materials[14]);

		GLint illumLoc = glGetUniformLocation(pid, "illum");
		glUniform1f(illumLoc, materials[15]);

	}

	glm::mat4 toCenter(float rollAngle = 0, float pitchAngle = 0, float yawAngle = 0) {
		vector<float> v = findBound(objVertices);
		float cx = v[6], cy = v[7], cz = v[8];
		float xmin = v[0], xmax = v[1], ymin = v[2], ymax = v[3], zmin = v[4], zmax = v[5];

		float objScale = 1/fmax(fmax(xmax - xmin, ymax - ymin), zmax - zmin);
		glm::mat4 Translate = glm::translate(glm::mat4(1.0f), glm::vec3(-cx, -cy, -cz));
		glm::mat4 RotateX = glm::rotate(glm::mat4(1.0f), rollAngle, glm::vec3(1, 0, 0));
		glm::mat4 RotateY = glm::rotate(glm::mat4(1.0f), pitchAngle, glm::vec3(0, 1, 0));
		glm::mat4 RotateZ = glm::rotate(glm::mat4(1.0f), yawAngle, glm::vec3(0, 0, 1));
		glm::mat4 Scale = glm::scale(glm::mat4(1.0f), glm::vec3(objScale, objScale, objScale));

		glm::mat4 Transform = Scale*RotateZ*RotateY*RotateX*Translate;
		return Transform;
	}

	void setCoords(float worldX, float worldY, float worldZ, float scale = 1, float yawAngle = 0, float rollAngle = 0, float pitchAngle = 0) {
		glm::mat4 Scale = glm::scale(glm::mat4(1.0f), glm::vec3(scale, scale, scale));
		glm::mat4 Translate = glm::translate(glm::mat4(1.0f), glm::vec3(worldX, worldY, worldZ));
		glm::mat4 RotateX = glm::rotate(glm::mat4(1.0f), rollAngle, glm::vec3(1, 0, 0));
		glm::mat4 RotateY = glm::rotate(glm::mat4(1.0f), pitchAngle, glm::vec3(0, 1, 0));
		glm::mat4 RotateZ = glm::rotate(glm::mat4(1.0f), yawAngle, glm::vec3(0, 0, 1));

		modelMatrix = Scale*RotateZ*RotateY*RotateX*Translate;
	}

	glm::mat4 getModelMatrix() {
		return modelMatrix;
	}
};

vector<Obj> chessPieces;

glm::mat4 getWorldMatrix() {
	glm::mat4 Scale = glm::scale(glm::mat4(1.0f), glm::vec3(scale, scale, scale));
	glm::mat4 Translate = glm::translate(glm::mat4(1.0f), glm::vec3(worldX, worldY, worldZ));
	glm::mat4 RotateX = glm::rotate(glm::mat4(1.0f), rollAngle, glm::vec3(1, 0, 0));
	glm::mat4 RotateY = glm::rotate(glm::mat4(1.0f), pitchAngle, glm::vec3(0, 1, 0));
	glm::mat4 RotateZ = glm::rotate(glm::mat4(1.0f), yawAngle, glm::vec3(0, 0, 1));

	return Scale*RotateZ*RotateY*RotateX*Translate;
}

glm::mat4 getV() {
	up = upsideDown? -1.0 : 1.0;
	return glm::lookAt(glm::vec3(camX, camY, camZ), glm::vec3(lookAtX, lookAtY, lookAtZ), glm::vec3(0.0, up, 0.0));
}

glm::mat4 getP() {
	return glm::perspective(glm::radians(zoom), (float)windowWidth/(float)windowHeight, 0.1f, 100.0f);
}

void initMatrices(GLuint pid, Obj o) {

	glm::mat4 T = o.toCenter();
	glm::mat4 M = o.getModelMatrix();
	glm::mat4 W = getWorldMatrix();
	glm::mat4 V = getV();
	glm::mat4 P = getP();
	glm::mat4 mvpt = P*V*W*M*T;
	GLint mvptLoc = glGetUniformLocation(pid, "mvpt");
	glUniformMatrix4fv(mvptLoc, 1, GL_FALSE, &mvpt[0][0]);
	GLint tLoc = glGetUniformLocation(pid, "tM");
	glUniformMatrix4fv(tLoc, 1, GL_FALSE, &T[0][0]);
	GLint mLoc = glGetUniformLocation(pid, "mM");
	glUniformMatrix4fv(mLoc, 1, GL_FALSE, &M[0][0]);
	GLint wLoc = glGetUniformLocation(pid, "wM");
	glUniformMatrix4fv(wLoc, 1, GL_FALSE, &W[0][0]);
	GLint vLoc = glGetUniformLocation(pid, "vM");
	glUniformMatrix4fv(vLoc, 1, GL_FALSE, &V[0][0]);
	GLint pLoc = glGetUniformLocation(pid, "pM");
	glUniformMatrix4fv(pLoc, 1, GL_FALSE, &P[0][0]);
}

void render(GLuint pid, VertexArray va, Obj o) {
	glBindVertexArray(va.id);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, o.colorTex.id);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, o.aoTex.id);
	glDrawArrays(GL_TRIANGLES, 0, va.count);
	glBindVertexArray(0); //unbind
	glBindTextures(GL_TEXTURE_2D, 2, 0);
	glUseProgram(0);

}

void display(GLuint pid) {

	glClearColor(0.0f, 0.0f, 0.2f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_CULL_FACE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for(int i = 0; i < (int)chessPieces.size(); i++) {
		glUseProgram(pid);
		initMatrices(pid, chessPieces[i]);
		chessPieces[i].bindMaterials(pid);

		VertexArray va(chessPieces[i].objVertices.size()/4);
		va.addBuffer("va", 0, chessPieces[i].objVertices);
		va.addBuffer("tex", 1, chessPieces[i].objTex);
		va.addBuffer("norms", 2, chessPieces[i].objNorms);

		render(pid, va, chessPieces[i]);

	}
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {

	// close window
     if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) closeWindow = closeWindow? false : true;

	// reset to default state
	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
		ao = 1; color = 1;
		camX = 0; camY = 5; camZ = 5; zoom = 90; upsideDown = false;
		lookAtX = 0; lookAtY = 0; lookAtZ = 0;
		rollAngle = 0; pitchAngle = 0; yawAngle = 0; scale = 1.0; worldX = 0; worldY = 0, worldZ = 0; //THESE SHOULDNT BE 0 (TRANS AND SCALE)
		lightPos[0] = 0.0; lightPos[1] = 0.0; lightPos[2] = 0.0;
	}

	// toggle texture mappings
	if (key == GLFW_KEY_F1 && action == GLFW_PRESS) ao = (ao == 1) ? 0 : 1;
	if (key == GLFW_KEY_F2 && action == GLFW_PRESS) color = (color == 1) ? 0 : 1;
	if (key == GLFW_KEY_F3 && action == GLFW_PRESS) { ao = (ao == 1) ? 0 : 1; color = (color == 1) ? 0 : 1; }

	// perspective viewing controls
     if (key == GLFW_KEY_Q && (action == GLFW_REPEAT || action == GLFW_PRESS)) camX += upsideDown ? -0.5 : 0.5;
    	if (key == GLFW_KEY_W && (action == GLFW_REPEAT || action == GLFW_PRESS)) camY += upsideDown ? -0.5 : 0.5;
     if (key == GLFW_KEY_E && (action == GLFW_REPEAT || action == GLFW_PRESS)) camZ += upsideDown ? -0.5 : 0.5;
	if (key == GLFW_KEY_A && (action == GLFW_REPEAT || action == GLFW_PRESS)) camX -= upsideDown ? -0.5 : 0.5;
	if (key == GLFW_KEY_S && (action == GLFW_REPEAT || action == GLFW_PRESS)) camY -= upsideDown ? -0.5 : 0.5;
	if (key == GLFW_KEY_D && (action == GLFW_REPEAT || action == GLFW_PRESS)) camZ -= upsideDown ? -0.5 : 0.5;
	if (key == GLFW_KEY_I && (action == GLFW_REPEAT || action == GLFW_PRESS)) lookAtX += upsideDown ? -0.5 : 0.5;
	if (key == GLFW_KEY_O && (action == GLFW_REPEAT || action == GLFW_PRESS)) lookAtY += upsideDown ? -0.5 : 0.5;
	if (key == GLFW_KEY_P && (action == GLFW_REPEAT || action == GLFW_PRESS)) lookAtZ += upsideDown ? -0.5 : 0.5;
	if (key == GLFW_KEY_J && (action == GLFW_REPEAT || action == GLFW_PRESS)) lookAtX -= upsideDown ? -0.5 : 0.5;
	if (key == GLFW_KEY_K && (action == GLFW_REPEAT || action == GLFW_PRESS)) lookAtY -= upsideDown ? -0.5 : 0.5;
	if (key == GLFW_KEY_L && (action == GLFW_REPEAT || action == GLFW_PRESS)) lookAtZ -= upsideDown ? -0.5 : 0.5;
	if (key == GLFW_KEY_F && (action == GLFW_REPEAT || action == GLFW_PRESS)) zoom = zoom + 5.0 < 365.0 ? zoom + 5.0 : zoom;
	if (key == GLFW_KEY_R && (action == GLFW_REPEAT || action == GLFW_PRESS)) zoom = zoom - 5.0 > 0.0 ? zoom - 5.0 : zoom;
	if (key == GLFW_KEY_CAPS_LOCK && action == GLFW_PRESS) upsideDown = upsideDown?false:true;

	// rotation controls
	if (key == GLFW_KEY_KP_7 && (action == GLFW_REPEAT || action == GLFW_PRESS)) rollAngle -= 0.1;
    	if (key == GLFW_KEY_KP_9 && (action == GLFW_REPEAT || action == GLFW_PRESS)) rollAngle += 0.1;
     if (key == GLFW_KEY_KP_4 && (action == GLFW_REPEAT || action == GLFW_PRESS)) pitchAngle -= 0.1;
	if (key == GLFW_KEY_KP_6 && (action == GLFW_REPEAT || action == GLFW_PRESS)) pitchAngle += 0.1;
	if (key == GLFW_KEY_KP_1 && (action == GLFW_REPEAT || action == GLFW_PRESS)) yawAngle -= 0.1;
	if (key == GLFW_KEY_KP_3 && (action == GLFW_REPEAT || action == GLFW_PRESS)) yawAngle += 0.1;
	if (key == GLFW_KEY_KP_5 && (action == GLFW_REPEAT || action == GLFW_PRESS)) {
		rollAngle += 0.1;
		pitchAngle += 0.1;
		yawAngle += 0.1;
	}
	if (key == GLFW_KEY_KP_2 && (action == GLFW_REPEAT || action == GLFW_PRESS)) {
		rollAngle -= 0.1;
		pitchAngle -= 0.1;
		yawAngle -= 0.1;
	}

	// scaling controls
	if (key == GLFW_KEY_KP_ADD && (action == GLFW_REPEAT || action == GLFW_PRESS)) scale *= 1.2;
	if (key == GLFW_KEY_KP_SUBTRACT && (action == GLFW_REPEAT || action == GLFW_PRESS)) scale *= 0.8;

	// translation controls
	if (key == GLFW_KEY_UP && (action == GLFW_REPEAT || action == GLFW_PRESS)) worldY += 0.2;
	if (key == GLFW_KEY_DOWN && (action == GLFW_REPEAT || action == GLFW_PRESS)) worldY -= 0.2;
	if (key == GLFW_KEY_RIGHT && (action == GLFW_REPEAT || action == GLFW_PRESS)) worldX += 0.2;
	if (key == GLFW_KEY_LEFT && (action == GLFW_REPEAT || action == GLFW_PRESS)) worldX -= 0.2;

	// light positioning
	if (key == GLFW_KEY_INSERT && (action == GLFW_REPEAT || action == GLFW_PRESS)) lightPos[0] += 1.0;
	if (key == GLFW_KEY_HOME && (action == GLFW_REPEAT || action == GLFW_PRESS)) lightPos[1] += 1.0;
	if (key == GLFW_KEY_PAGE_UP && (action == GLFW_REPEAT || action == GLFW_PRESS)) lightPos[2] += 1.0;
	if (key == GLFW_KEY_DELETE && (action == GLFW_REPEAT || action == GLFW_PRESS)) lightPos[0] -= 1.0;
	if (key == GLFW_KEY_END && (action == GLFW_REPEAT || action == GLFW_PRESS)) lightPos[1] -= 1.0;
	if (key == GLFW_KEY_PAGE_DOWN && (action == GLFW_REPEAT || action == GLFW_PRESS)) lightPos[2] -= 1.0;

	// display
	cout << "CURRENT" << endl;
	cout << "CAMERA POSITION: ("<< camX <<", "<< camY <<", "<< camZ <<")" << endl;
	cout << "LOOKING AT: (" << lookAtX << ", " << lookAtY << ", " << lookAtZ << ")" << endl;
	cout << "LIGHT POSITION: (" << lightPos[0] << ", " << lightPos[1] << ", " << lightPos[2] << ")" << endl;
	cout << "ZOOM = " << zoom << endl;
	cout << "UPSIDEDOWN = " << upsideDown << endl;
	cout << "OBJECT ROTATION IN X (ROLL): " << rollAngle << " degrees" << endl;
	cout << "OBJECT ROTATION IN Y (PITCH): " << pitchAngle << " degrees" << endl;
	cout << "OBJECT ROTATION IN Z (YAW): " << yawAngle << " degrees" << endl;
	cout << "OBJECT TRANSLATION IN X: " << worldX << endl;
	cout << "OBJECT TRANSLATION IN Y: " << worldY << endl;
	cout << "OBJECT TRANSLATION IN Z: " << worldZ << endl;
	cout << "OBJECT SCALING: " << scale << endl;
	cout << "AMBIENT OCCLUSION APPLIED: " << ao << endl;
	cout << "COLOR APPLIED: " << color << endl;

}

int main(int argc, const char** argv) {

	if (!glfwInit()) {
			cout << "ERROR: GLFW failed to initialize, TERMINATING" << endl;
			return -1;
	}
	GLFWwindow *window = 0;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	//glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	window = glfwCreateWindow(800, 800, "Assignment 3", 0, 0);
	glfwSetWindowAspectRatio(window, 1, 1);

	if (!window) {
			cout << "failed to create window, TERMINATING" << endl;
			glfwTerminate();
			return -1;
	}

	glfwMakeContextCurrent(window);

	glfwSetKeyCallback(window, key_callback);

	Program p("data/vertex.glsl", "data/fragment.glsl");
	glUseProgram(p.id);

	// GLint nsLoc = glGetUniformLocation(p.id, "Ns");
	// glUniform1f(nsLoc, Ns);
	//
	// GLint kaLoc = glGetUniformLocation(p.id, "Ka");
	// glUniform3f(kaLoc, Ka[0], Ka[1], Ka[2]);
	//
	// GLint kdLoc = glGetUniformLocation(p.id, "Kd");
	// glUniform3f(kdLoc, Kd[0], Kd[1], Kd[2]);
	//
	// GLint ksLoc = glGetUniformLocation(p.id, "Ks");
	// glUniform3f(ksLoc, Ks[0], Ks[1], Ks[2]);
	//
	// GLint keLoc = glGetUniformLocation(p.id, "Ke");
	// glUniform3f(keLoc, Ke[0], Ke[1], Ke[2]);
	//
	// GLint niLoc = glGetUniformLocation(p.id, "Ni");
	// glUniform1f(niLoc, Ni);
	//
	// GLint dLoc = glGetUniformLocation(p.id, "d");
	// glUniform1f(dLoc, d);
	//
	// GLint illumLoc = glGetUniformLocation(p.id, "illum");
	// glUniform1f(illumLoc, illum);

//	readMTL("bishop.mtl");

	Obj board("board.obj", "boardColor.png", "boardAo.png", "board.mtl");
	board.setCoords(0, 0, 0, 8);
	chessPieces.push_back(board);

		Obj bKing("king.obj", "kingColorBlack.png", "kingAo.png", "king.mtl");
		Obj bQueen("queen.obj", "queenColorBlack.png", "queenAo.png", "queen.mtl");
		Obj bBishop1("bishop.obj", "bishopColorBlack.png", "bishopAo.png", "bishop.mtl");
		Obj bBishop2("bishop.obj", "bishopColorBlack.png", "bishopAo.png", "bishop.mtl");
		Obj bRook1("rook.obj", "rookColorBlack.png", "rookAo.png", "rook.mtl");
		Obj bRook2("rook.obj", "rookColorBlack.png", "rookAo.png", "rook.mtl");
		Obj bKnight1("knight.obj", "knightColorBlack.png", "knightAo.png", "knight.mtl");
		Obj bKnight2("knight.obj", "knightColorBlack.png", "knightAo.png", "knight.mtl");

		Obj wKing("king.obj", "kingColorWhite.png", "kingAo.png", "king.mtl");
		Obj wQueen("queen.obj", "queenColorWhite.png", "queenAo.png", "queen.mtl");
		Obj wBishop1("bishop.obj", "bishopColorWhite.png", "bishopAo.png", "bishop.mtl");
		Obj wBishop2("bishop.obj", "bishopColorWhite.png", "bishopAo.png", "bishop.mtl");
		Obj wRook1("rook.obj", "rookColorWhite.png", "rookAo.png", "rook.mtl");
		Obj wRook2("rook.obj", "rookColorWhite.png", "rookAo.png", "rook.mtl");
		Obj wKnight1("knight.obj", "knightColorWhite.png", "knightAo.png", "knight.mtl");
		Obj wKnight2("knight.obj", "knightColorWhite.png", "knightAo.png", "knight.mtl");


	vector<Obj> bBackLine = {bRook1, bKnight1, bBishop1, bKing, bQueen, bBishop2, bKnight2, bRook2};
	vector<Obj> wBackLine = {wRook1, wKnight1, wBishop1, wKing, wQueen, wBishop2, wKnight2, wRook2};

	for (int i = 0; i < 8; i++) {
		Obj bPawn("pawn.obj", "pawnColorBlack.png", "pawnAo.png", "pawn.mtl");
		Obj wPawn("pawn.obj", "pawnColorWhite.png", "pawnAo.png", "pawn.mtl");
		bBackLine[i].setCoords(2.75, 0.5, -2.75 + 0.78*i, 8*0.16);
		wBackLine[i].setCoords(-2.75, 0.5, 2.75 - 0.78*i, 8*0.16);
		bPawn.setCoords(2.75 - 0.78, 0.5, -2.75 + 0.78*i, 8*0.16);
		wPawn.setCoords(-2.75 + 0.78, 0.5, 2.75 - 0.78*i, 8*0.16);
		chessPieces.push_back(bBackLine[i]);
		chessPieces.push_back(wBackLine[i]);
		chessPieces.push_back(bPawn);
		chessPieces.push_back(wPawn);

	}

	for(int i = 0; i < (int)chessPieces.size(); i++) {
		if(!chessPieces[i].init()){
			cout << "loading failed for object: "<< chessPieces[i].objFile <<endl;
			exit(0);
		}
		chessPieces[i].bindTextures(p.id);
	}

	GLint aoBoolLoc = glGetUniformLocation(p.id, "ao_bool");
	GLint colorBoolLoc = glGetUniformLocation(p.id, "color_bool");
	GLint lightLoc = glGetUniformLocation(p.id, "lightPos");


	while(!glfwWindowShouldClose(window)) {

		glUseProgram(p.id);

		glfwGetWindowSize(window, &windowWidth, &windowHeight);
		glViewport(0, 0, windowWidth, windowHeight);
		glUniform1i(aoBoolLoc, ao);
		glUniform1i(colorBoolLoc, color);
		glUniform3f(lightLoc, lightPos.x, lightPos.y, lightPos.z);

		display(p.id);

		glfwSwapBuffers(window);
		glfwPollEvents();

		if(closeWindow) { break; }

	}

	glfwDestroyWindow(window);
	glfwTerminate();

	cout << "bui bui" << endl;
	return 0;

}

bool CheckGLErrors() {
	bool error = false;
	for (GLenum flag = glGetError(); flag != GL_NO_ERROR; flag = glGetError()) {
		cout << "OpenGL ERROR:  ";
		switch (flag) {
			case GL_INVALID_ENUM:
			cout << "GL_INVALID_ENUM" << endl; break;
			case GL_INVALID_VALUE:
			cout << "GL_INVALID_VALUE" << endl; break;
			case GL_INVALID_OPERATION:
			cout << "GL_INVALID_OPERATION" << endl; break;
			case GL_INVALID_FRAMEBUFFER_OPERATION:
			cout << "GL_INVALID_FRAMEBUFFER_OPERATION" << endl; break;
			case GL_OUT_OF_MEMORY:
			cout << "GL_OUT_OF_MEMORY" << endl; break;
			default:
			cout << "[unknown error code]" << endl;
		}
		error = true;
	}
	return error;
}
