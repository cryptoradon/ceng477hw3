#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <GL/glew.h>   // The GL Header File
#include <GL/gl.h>   // The GL Header File
#include <GLFW/glfw3.h> // The GLFW header
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define BUFFER_OFFSET(i) ((char*)NULL + (i))

using namespace std;

GLuint gProgram[6];
GLint gIntensityLoc;
float gIntensity = 1000;
int gWidth, gHeight;
vector<vector<int>> colors{{37, 45, 63}, {229, 107, 197}, {232, 203, 185}, {113, 153, 104}, {95, 31, 137}};
float scaleCoef = 1.0;

struct Candy {
    int colorID;
    bool visible;
    int shiftAmount;
};

struct {
    int width, height;
    vector<vector<Candy>> candies;
    int moves = 0;
    int score = 0;
    vector<vector<GLuint>> colorMap; // to slide candies when explosion occur
    int correctCandyCount;
    vector<vector<int>> needToDisappear; // each inside vector holds which candy clicked or need to explode
} grid;

struct Vertex
{
    Vertex(GLfloat inX, GLfloat inY, GLfloat inZ) : x(inX), y(inY), z(inZ) { }
    GLfloat x, y, z;
};

struct Texture
{
    Texture(GLfloat inU, GLfloat inV) : u(inU), v(inV) { }
    GLfloat u, v;
};

struct Normal
{
    Normal(GLfloat inX, GLfloat inY, GLfloat inZ) : x(inX), y(inY), z(inZ) { }
    GLfloat x, y, z;
};

struct Face
{
	Face(int v[], int t[], int n[]) {
		vIndex[0] = v[0];
		vIndex[1] = v[1];
		vIndex[2] = v[2];
		tIndex[0] = t[0];
		tIndex[1] = t[1];
		tIndex[2] = t[2];
		nIndex[0] = n[0];
		nIndex[1] = n[1];
		nIndex[2] = n[2];
	}
    GLuint vIndex[3], tIndex[3], nIndex[3];
};

vector<Vertex> gVertices;
vector<Texture> gTextures;
vector<Normal> gNormals;
vector<Face> gFaces;

GLuint gVertexAttribBuffer, gIndexBuffer;
GLint gInVertexLoc, gInNormalLoc;
int gVertexDataSizeInBytes, gNormalDataSizeInBytes;

bool ParseObj(const string& fileName)
{
    fstream myfile;

    // Open the input 
    myfile.open(fileName.c_str(), std::ios::in);

    if (myfile.is_open())
    {
        string curLine;

        while (getline(myfile, curLine))
        {
            stringstream str(curLine);
            GLfloat c1, c2, c3;
            GLuint index[9];
            string tmp;

            if (curLine.length() >= 2)
            {
                if (curLine[0] == '#') // comment
                {
                    continue;
                }
                else if (curLine[0] == 'v')
                {
                    if (curLine[1] == 't') // texture
                    {
                        str >> tmp; // consume "vt"
                        str >> c1 >> c2;
                        gTextures.push_back(Texture(c1, c2));
                    }
                    else if (curLine[1] == 'n') // normal
                    {
                        str >> tmp; // consume "vn"
                        str >> c1 >> c2 >> c3;
                        gNormals.push_back(Normal(c1, c2, c3));
                    }
                    else // vertex
                    {
                        str >> tmp; // consume "v"
                        str >> c1 >> c2 >> c3;
                        gVertices.push_back(Vertex(c1, c2, c3));
                    }
                }
                else if (curLine[0] == 'f') // face
                {
                    str >> tmp; // consume "f"
					char c;
					int vIndex[3],  nIndex[3], tIndex[3];
					str >> vIndex[0]; str >> c >> c; // consume "//"
					str >> nIndex[0]; 
					str >> vIndex[1]; str >> c >> c; // consume "//"
					str >> nIndex[1]; 
					str >> vIndex[2]; str >> c >> c; // consume "//"
					str >> nIndex[2]; 

					assert(vIndex[0] == nIndex[0] &&
						   vIndex[1] == nIndex[1] &&
						   vIndex[2] == nIndex[2]); // a limitation for now

					// make indices start from 0
					for (int c = 0; c < 3; ++c)
					{
						vIndex[c] -= 1;
						nIndex[c] -= 1;
						tIndex[c] -= 1;
					}

                    gFaces.push_back(Face(vIndex, tIndex, nIndex));
                }
                else
                {
                    cout << "Ignoring unidentified line in obj file: " << curLine << endl;
                }
            }

            //data += curLine;
            if (!myfile.eof())
            {
                //data += "\n";
            }
        }

        myfile.close();
    }
    else
    {
        return false;
    }

	/*
	for (int i = 0; i < gVertices.size(); ++i)
	{
		Vector3 n;

		for (int j = 0; j < gFaces.size(); ++j)
		{
			for (int k = 0; k < 3; ++k)
			{
				if (gFaces[j].vIndex[k] == i)
				{
					// face j contains vertex i
					Vector3 a(gVertices[gFaces[j].vIndex[0]].x, 
							  gVertices[gFaces[j].vIndex[0]].y,
							  gVertices[gFaces[j].vIndex[0]].z);

					Vector3 b(gVertices[gFaces[j].vIndex[1]].x, 
							  gVertices[gFaces[j].vIndex[1]].y,
							  gVertices[gFaces[j].vIndex[1]].z);

					Vector3 c(gVertices[gFaces[j].vIndex[2]].x, 
							  gVertices[gFaces[j].vIndex[2]].y,
							  gVertices[gFaces[j].vIndex[2]].z);

					Vector3 ab = b - a;
					Vector3 ac = c - a;
					Vector3 normalFromThisFace = (ab.cross(ac)).getNormalized();
					n += normalFromThisFace;
				}

			}
		}

		n.normalize();

		gNormals.push_back(Normal(n.x, n.y, n.z));
	}
	*/

	assert(gVertices.size() == gNormals.size());

    return true;
}

bool ReadDataFromFile(
    const string& fileName, ///< [in]  Name of the shader file
    string&       data)     ///< [out] The contents of the file
{
    fstream myfile;

    // Open the input 
    myfile.open(fileName.c_str(), std::ios::in);

    if (myfile.is_open())
    {
        string curLine;

        while (getline(myfile, curLine))
        {
            data += curLine;
            if (!myfile.eof())
            {
                data += "\n";
            }
        }

        myfile.close();
    }
    else
    {
        return false;
    }

    return true;
}

void createVS(GLuint& gProgramm, const string& filename)
{
    string shaderSource;

    if (!ReadDataFromFile(filename, shaderSource))
    {
        cout << "Cannot find file name: " + filename << endl;
        exit(-1);
    }

    GLint length = shaderSource.length();
    const GLchar* shader = (const GLchar*) shaderSource.c_str();

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &shader, &length);
    glCompileShader(vs);

    char output[1024] = {0};
    glGetShaderInfoLog(vs, 1024, &length, output);
    printf("VS compile log: %s\n", output);

    glAttachShader(gProgramm, vs);
}

void createFS(GLuint& gProgramm, const string& filename)
{
    string shaderSource;

    if (!ReadDataFromFile(filename, shaderSource))
    {
        cout << "Cannot find file name: " + filename << endl;
        exit(-1);
    }

    GLint length = shaderSource.length();
    const GLchar* shader = (const GLchar*) shaderSource.c_str();

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &shader, &length);
    glCompileShader(fs);

    char output[1024] = {0};
    glGetShaderInfoLog(fs, 1024, &length, output);
    printf("FS compile log: %s\n", output);

    glAttachShader(gProgramm, fs);
}

void initShaders()
{
    gProgram[0] = glCreateProgram();
    gProgram[1] = glCreateProgram();
    gProgram[2] = glCreateProgram();
    gProgram[3] = glCreateProgram();
    gProgram[4] = glCreateProgram();
    gProgram[5] = glCreateProgram();

    createVS(gProgram[0], "vert0.glsl");
    createFS(gProgram[0], "frag0.glsl");

    createVS(gProgram[1], "vert1.glsl");
    createFS(gProgram[1], "frag1.glsl");

    createVS(gProgram[2], "vert2.glsl");
    createFS(gProgram[2], "frag2.glsl");

    createVS(gProgram[3], "vert3.glsl");
    createFS(gProgram[3], "frag3.glsl");

    createVS(gProgram[4], "vert4.glsl");
    createFS(gProgram[4], "frag4.glsl");

    createVS(gProgram[5], "vert5.glsl");
    createFS(gProgram[5], "frag5.glsl");

    glBindAttribLocation(gProgram[0], 0, "inVertex");
    glBindAttribLocation(gProgram[0], 1, "inNormal");
    glBindAttribLocation(gProgram[1], 0, "inVertex");
    glBindAttribLocation(gProgram[1], 1, "inNormal");
    glBindAttribLocation(gProgram[2], 0, "inVertex");
    glBindAttribLocation(gProgram[2], 1, "inNormal");
    glBindAttribLocation(gProgram[3], 0, "inVertex");
    glBindAttribLocation(gProgram[3], 1, "inNormal");
    glBindAttribLocation(gProgram[4], 0, "inVertex");
    glBindAttribLocation(gProgram[4], 1, "inNormal");
    glBindAttribLocation(gProgram[5], 0, "inVertex");
    glBindAttribLocation(gProgram[5], 1, "inNormal");

    glLinkProgram(gProgram[0]);
    glLinkProgram(gProgram[1]);
    glLinkProgram(gProgram[2]);
    glLinkProgram(gProgram[3]);
    glLinkProgram(gProgram[4]);
    glLinkProgram(gProgram[5]);
    glUseProgram(gProgram[0]);

    gIntensityLoc = glGetUniformLocation(gProgram[0], "intensity");
    cout << "gIntensityLoc = " << gIntensityLoc << endl;
    glUniform1f(gIntensityLoc, gIntensity);
}

void initVBO()
{
    /* comment out since it does not exist in text model
    GLuint vao;
    glGenVertexArrays(1, &vao);
    assert(vao > 0);
    glBindVertexArray(vao);
    cout << "vao = " << vao << endl;*/

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	assert(glGetError() == GL_NONE);

	glGenBuffers(1, &gVertexAttribBuffer);
	glGenBuffers(1, &gIndexBuffer);

	assert(gVertexAttribBuffer > 0 && gIndexBuffer > 0);

	glBindBuffer(GL_ARRAY_BUFFER, gVertexAttribBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIndexBuffer);

	gVertexDataSizeInBytes = gVertices.size() * 3 * sizeof(GLfloat);
	gNormalDataSizeInBytes = gNormals.size() * 3 * sizeof(GLfloat);
	int indexDataSizeInBytes = gFaces.size() * 3 * sizeof(GLuint);
	GLfloat* vertexData = new GLfloat [gVertices.size() * 3];
	GLfloat* normalData = new GLfloat [gNormals.size() * 3];
	GLuint* indexData = new GLuint [gFaces.size() * 3];

    float minX = 1e6, maxX = -1e6;
    float minY = 1e6, maxY = -1e6;
    float minZ = 1e6, maxZ = -1e6;

	for (int i = 0; i < gVertices.size(); ++i)
	{
		vertexData[3*i] = gVertices[i].x;
		vertexData[3*i+1] = gVertices[i].y;
		vertexData[3*i+2] = gVertices[i].z;

        minX = std::min(minX, gVertices[i].x);
        maxX = std::max(maxX, gVertices[i].x);
        minY = std::min(minY, gVertices[i].y);
        maxY = std::max(maxY, gVertices[i].y);
        minZ = std::min(minZ, gVertices[i].z);
        maxZ = std::max(maxZ, gVertices[i].z);
	}

    std::cout << "minX = " << minX << std::endl;
    std::cout << "maxX = " << maxX << std::endl;
    std::cout << "minY = " << minY << std::endl;
    std::cout << "maxY = " << maxY << std::endl;
    std::cout << "minZ = " << minZ << std::endl;
    std::cout << "maxZ = " << maxZ << std::endl;

	for (int i = 0; i < gNormals.size(); ++i)
	{
		normalData[3*i] = gNormals[i].x;
		normalData[3*i+1] = gNormals[i].y;
		normalData[3*i+2] = gNormals[i].z;
	}

	for (int i = 0; i < gFaces.size(); ++i)
	{
		indexData[3*i] = gFaces[i].vIndex[0];
		indexData[3*i+1] = gFaces[i].vIndex[1];
		indexData[3*i+2] = gFaces[i].vIndex[2];
	}


	glBufferData(GL_ARRAY_BUFFER, gVertexDataSizeInBytes + gNormalDataSizeInBytes, 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, gVertexDataSizeInBytes, vertexData);
	glBufferSubData(GL_ARRAY_BUFFER, gVertexDataSizeInBytes, gNormalDataSizeInBytes, normalData);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexDataSizeInBytes, indexData, GL_STATIC_DRAW);

	// done copying; can free now
	delete[] vertexData;
	delete[] normalData;
	delete[] indexData;

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(gVertexDataSizeInBytes));
}

int getRandomColorIndex(int colorCount) {
    return (rand() % colorCount);
}

void init(string objectFile) 
{
    //cout << objectFile;
	ParseObj(objectFile);

    glEnable(GL_DEPTH_TEST);
    initShaders();
    initVBO();
    for (int i = 0; i < grid.height; i++) {
        for (int j = 0; j < grid.width; j++) {
            struct Candy candy;
            candy.colorID = getRandomColorIndex(6);
            candy.visible = true;
            candy.shiftAmount = 0;
            grid.candies[i][j] = candy;
            GLuint gluint = gProgram[candy.colorID];
            grid.colorMap[i][j] = gluint;
            //cout << gluint << endl;
        }
    }
}

void drawModel()
{
	glBindBuffer(GL_ARRAY_BUFFER, gVertexAttribBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIndexBuffer);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(gVertexDataSizeInBytes));

	glDrawElements(GL_TRIANGLES, gFaces.size() * 3, GL_UNSIGNED_INT, 0);
}

void display()
{
    glClearColor(0, 0, 0, 1);
    glClearDepth(1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	static float angle = 0;
	float translate = 0;
	grid.correctCandyCount = 0;
    //cout << scaleCoef << endl;
	// if exploding objects reach scaling coefficient 1.5
    if(scaleCoef >= 1.5) {
        scaleCoef = 1.0;

        // start from the leftmost column, check all columns if any shift needed
        for(int i=0; i<grid.width; i++) {
            int shiftInThisRow = 0;
            for(int j=grid.height-1; j>=0; j--) {
                // if current candy is not on where it should be
                if(!grid.candies[j][i].visible || shiftInThisRow != 0) {
                    // if current candy should stay, but should shift
                    if(grid.candies[j][i].visible) {
                        grid.candies[j+shiftInThisRow][i].colorID = grid.candies[j][i].colorID;
                        grid.colorMap[j+shiftInThisRow][i] = grid.colorMap[j][i];
                        grid.candies[j+shiftInThisRow][i].visible = true;
                        // in total between top and bottom is 20. But 45/600 is reserved for text.
                        // so text has 1.5/20. remaining 18.5 is reserved for candies.
                        // 18.5 / candy in one row shows the shift amount for one shift
                        grid.candies[j+shiftInThisRow][i].shiftAmount = 18.5*shiftInThisRow/grid.height;
                    } else { // if current candy should be removed
                        shiftInThisRow++;
                    }
                } else { // current candy is on where it should be
                    grid.candies[j][i].shiftAmount = 0;
                    continue;
                }
                // if j == 0, initialize all the candies needed
                if(j == 0) {
                    while(shiftInThisRow != 0) {
                        struct Candy candy;
                        candy.colorID = getRandomColorIndex(6);
                        candy.visible = true;
                        candy.shiftAmount = 18.5*shiftInThisRow/grid.height;
                        grid.candies[shiftInThisRow-1][i] = candy;
                        GLuint gluint = gProgram[candy.colorID];
                        grid.colorMap[shiftInThisRow-1][i] = gluint;
                        shiftInThisRow--;
                    }
                }
            }
        }

    }

	for(int i=0; i<grid.height; i++) {
	    for(int j=0; j<grid.width; j++) {
            glUseProgram(grid.colorMap[i][j]);
            // translation
            if(grid.candies[i][j].shiftAmount > 0) {
                translate = grid.candies[i][j].shiftAmount;
                grid.candies[i][j].shiftAmount -= 0.05;
            } else {
                translate = 0;
                grid.correctCandyCount += 1;
            }

            glm::mat4 T = glm::translate(glm::mat4(1.f), glm::vec3((20 * (j+0.5f))/grid.width - 10.0f,
                                                                   -(18.5 * (i+0.5f))/grid.height + 10.0f + translate,
                                                                   -10.f));

            // rotation
            glm::mat4 R = glm::rotate(glm::mat4(1.f), glm::radians(angle), glm::vec3(0, 1, 0));

            // scale
            glm::mat4 scaleCoefficient = glm::scale(glm::mat4(1.f), glm::vec3(1.f, 1.f, 1.f));
            if(!grid.candies[i][j].visible) { // if current candy should disappear, than use scaling coef
                //cout << "were here" << endl;
                scaleCoef += 0.01;
                scaleCoefficient = glm::scale(glm::mat4(1.f), glm::vec3(scaleCoef, scaleCoef, scaleCoef));
            } else {
                scaleCoefficient = glm::scale(glm::mat4(1.f), glm::vec3(1.f, 1.f, 1.f));
            }
            glm::mat4 S = scaleCoefficient * glm::scale(glm::mat4(1.f), glm::vec3(0.5f, 0.5f, 0.5f));


            glm::mat4 modelMat = T * R * S;
            glm::mat4 modelMatInv = glm::transpose(glm::inverse(modelMat));
            glm::mat4 orthoMat = glm::ortho(-10.f, 10.f, -10.f, 10.f, -20.f, 20.f);

            glUniformMatrix4fv(glGetUniformLocation(grid.colorMap[i][j], "modelingMat"), 1, GL_FALSE, glm::value_ptr(modelMat));
            glUniformMatrix4fv(glGetUniformLocation(grid.colorMap[i][j], "modelingMatInvTr"), 1, GL_FALSE, glm::value_ptr(modelMatInv));
            glUniformMatrix4fv(glGetUniformLocation(grid.colorMap[i][j], "orthoMat"), 1, GL_FALSE, glm::value_ptr(orthoMat));

            drawModel();

            //assert(glGetError() == GL_NO_ERROR);
        }
	}

	angle += 0.5;
}

void reshape(GLFWwindow* window, int w, int h)
{
    w = w < 1 ? 1 : w;
    h = h < 1 ? 1 : h;

    gWidth = w;
    gHeight = h;

    glViewport(0, 0, w, h);
/* comment out since it does not exist in text model
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    //glOrtho(-10, 10, -10, 10, -10, 10);
    gluPerspective(45, 1, 1, 100);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();*/
}

void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
}

void mainLoop(GLFWwindow* window)
{
    while (!glfwWindowShouldClose(window))
    {
        display();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}


// reference: https://www.glfw.org/docs/3.3/input_guide.html#input_mouse
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods){
    if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS){
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        if(ypos <= 555){ // en alttaki 45 yazi icin ayrildi
            int clickedCandyXIndex = xpos*grid.width/640;
            int clickedCandyYIndex = ypos*grid.height/555;
            grid.candies[clickedCandyYIndex][clickedCandyXIndex].visible = false;
            vector<int> indexes;
            indexes.push_back(clickedCandyYIndex);
            indexes.push_back(clickedCandyXIndex);
            grid.needToDisappear.push_back(indexes);
            grid.moves += 1;
        }
    }
}


int main(int argc, char** argv)   // Create Main Function For Bringing It All Together
{
    if (argc != 4){
        cout << "Incorrect argument count, usage is ./hw3 <grid_width> <grid_height> <object_file>!" << endl;
        return 0;
    }
    
    grid.width = atoi(argv[1]);
    grid.height = atoi(argv[2]);
    string objectFile = argv[3];
    grid.correctCandyCount = grid.width * grid.height;
    grid.candies.resize(grid.height);
    grid.colorMap.resize(grid.height);
    for (int i = 0; i < grid.height; i++) {
        grid.candies[i].resize(grid.width);
        grid.colorMap[i].resize(grid.width);
    }

    GLFWwindow* window;
    if (!glfwInit())
    {
        exit(-1);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    int width = 640, height = 600;
    window = glfwCreateWindow(width, height, "Candy Crush", NULL, NULL);

    if (!window)
    {
        glfwTerminate();
        exit(-1);
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Initialize GLEW to setup the OpenGL Function pointers
    if (GLEW_OK != glewInit())
    {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return EXIT_FAILURE;
    }

    char rendererInfo[512] = {0};
    strcpy(rendererInfo, (const char*) glGetString(GL_RENDERER));
    strcat(rendererInfo, " - ");
    strcat(rendererInfo, (const char*) glGetString(GL_VERSION));
    glfwSetWindowTitle(window, rendererInfo);

    init(objectFile);

    glfwSetKeyCallback(window, keyboard);
    glfwSetWindowSizeCallback(window, reshape);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    reshape(window, width, height); // need to call this once ourselves
    mainLoop(window); // this does not return unless the window is closed

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

