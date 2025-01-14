#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <GL/glew.h>   // The GL Header File
#include <GL/gl.h>   // The GL Header File
#include <GLFW/glfw3.h> // The GLFW header
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

#define BUFFER_OFFSET(i) ((char*)NULL + (i))

using namespace std;

GLuint gProgram[7];
GLint gIntensityLoc;
GLuint gTextVBO;
float gIntensity = 1000;
int gWidth, gHeight;
vector<vector<int>> colors{{37, 45, 63}, {229, 107, 197}, {232, 203, 185}, {113, 153, 104}, {95, 31, 137}};
float scaleCoef = 1.0;

struct Candy {
    int colorID;
    bool visible;
    float shiftAmount;
};

struct {
    int width, height;
    vector<vector<Candy>> candies;
    int moves = 0;
    int score = 0;
    vector<vector<GLuint>> colorMap; // to slide candies when explosion occur
    int correctCandyCount;
    vector<vector<int>> currentlyShifting;
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

/// Holds all state information relevant to a character as loaded using FreeType
struct Character {
    GLuint TextureID;   // ID handle of the glyph texture
    glm::ivec2 Size;    // Size of glyph
    glm::ivec2 Bearing;  // Offset from baseline to left/top of glyph
    GLuint Advance;    // Horizontal offset to advance to next glyph
};

std::map<GLchar, Character> Characters;

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
    gProgram[6] = glCreateProgram();

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

    createVS(gProgram[6], "vert_text.glsl");
    createFS(gProgram[6], "frag_text.glsl");

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
    glBindAttribLocation(gProgram[6], 2, "vertex");

    glLinkProgram(gProgram[0]);
    glLinkProgram(gProgram[1]);
    glLinkProgram(gProgram[2]);
    glLinkProgram(gProgram[3]);
    glLinkProgram(gProgram[4]);
    glLinkProgram(gProgram[5]);
    glLinkProgram(gProgram[6]);
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

void initFonts(int windowWidth, int windowHeight)
{
    // Set OpenGL options
    //glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::mat4 projection = glm::ortho(0.0f, static_cast<GLfloat>(windowWidth), 0.0f, static_cast<GLfloat>(windowHeight));
    glUseProgram(gProgram[6]);
    glUniformMatrix4fv(glGetUniformLocation(gProgram[6], "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // FreeType
    FT_Library ft;
    // All functions return a value different than 0 whenever an error occurred
    if (FT_Init_FreeType(&ft))
    {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
    }

    // Load font as face
    FT_Face face;
    if (FT_New_Face(ft, "/usr/share/fonts/truetype/liberation/LiberationSerif-Italic.ttf", 0, &face))
    {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
    }

    // Set size to load glyphs as
    FT_Set_Pixel_Sizes(face, 0, 48);

    // Disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 

    // Load first 128 characters of ASCII set
    for (GLubyte c = 0; c < 128; c++)
    {
        // Load character glyph 
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
            continue;
        }
        // Generate texture
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RED,
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows,
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer
                );
        // Set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // Now store character for later use
        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            face->glyph->advance.x
        };
        Characters.insert(std::pair<GLchar, Character>(c, character));
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    // Destroy FreeType once we're finished
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    //
    // Configure VBO for texture quads
    //
    glGenBuffers(1, &gTextVBO);
    glBindBuffer(GL_ARRAY_BUFFER, gTextVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
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
    initFonts(640, 600);
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
            grid.currentlyShifting[i][j] = 0;
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

void renderText(const std::string& text, GLfloat x, GLfloat y, GLfloat scale, glm::vec3 color)
{
    // Activate corresponding render state	
    glUseProgram(gProgram[6]);
    glUniform3f(glGetUniformLocation(gProgram[6], "textColor"), color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);

    // Iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++) 
    {
        Character ch = Characters[*c];

        GLfloat xpos = x + ch.Bearing.x * scale;
        GLfloat ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        GLfloat w = ch.Size.x * scale;
        GLfloat h = ch.Size.y * scale;

        // Update VBO for each character
        GLfloat vertices[6][4] = {
            { xpos,     ypos + h,   0.0, 0.0 },            
            { xpos,     ypos,       0.0, 1.0 },
            { xpos + w, ypos,       1.0, 1.0 },

            { xpos,     ypos + h,   0.0, 0.0 },
            { xpos + w, ypos,       1.0, 1.0 },
            { xpos + w, ypos + h,   1.0, 0.0 }           
        };

        // Render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);

        // Update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, gTextVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // Be sure to use glBufferSubData and not glBufferData

        //glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // Now advance cursors for next glyph (note that advance is number of 1/64 pixels)

        x += (ch.Advance >> 6) * scale; // Bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void display()
{
    //cout << grid.score << endl;
    glClearColor(0, 0, 0, 1);
    glClearDepth(1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	static float angle = 0;
	float translate = 0;
	grid.correctCandyCount = 0;

    int activeShift = 0;
    for(int i=0; i<grid.width; i++) {
        for(int j=0; j<grid.height-1; j++) {
            if (grid.currentlyShifting[i][j] == 1){
                activeShift = 1;
            }
        }
    }

	if(scaleCoef <= 1.0 && !activeShift) {
        // start from the leftmost column, check all columns if any points will be earn
        for(int i=0; i<grid.width; i++) {
            int currentStreak = 0;
            for(int j=0; j<grid.height-1; j++) {
                if(grid.candies[j][i].colorID == grid.candies[j+1][i].colorID) {
                    currentStreak++;
                    if(currentStreak == 2) {
                        grid.candies[j-1][i].visible = false;
                        grid.candies[j][i].visible = false;
                        grid.candies[j+1][i].visible = false;
                    } else if(currentStreak > 2) {
                        grid.candies[j+1][i].visible = false;
                    }
                } else {
                    currentStreak = 0;
                }
            }
        }

        // start from the upper row, check all rows if any points will be earn
        for(int i=0; i<grid.height; i++) {
            int currentStreak = 0;
            for(int j=0; j<grid.width-1; j++) {
                if(grid.candies[i][j].colorID == grid.candies[i][j+1].colorID) {
                    currentStreak++;
                    if(currentStreak == 2) {
                        grid.candies[i][j-1].visible = false;
                        grid.candies[i][j].visible = false;
                        grid.candies[i][j+1].visible = false;
                        //cout << "i: " << i << "j: " << j << endl;
                    } else if(currentStreak > 2) {
                        grid.candies[i][j+1].visible = false;
                    }
                } else {
                    currentStreak = 0;
                }
            }
        }

        int tempScore = grid.score;
        //calculate earned points
        for(int i=0; i<grid.width; i++) {
            for(int j=0; j<grid.height; j++) {
                if(grid.candies[j][i].visible) {
                    continue;
                } else {
                    grid.score += 1;
                }
            }
        }
        if(grid.score < tempScore+3) {
            grid.score = tempScore;
        }
    }


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
                    float shift = 18.5*shiftInThisRow/grid.height;
                    while(shiftInThisRow != 0) {
                        struct Candy candy;
                        candy.colorID = getRandomColorIndex(6);
                        candy.visible = true;
                        candy.shiftAmount = shift;
                        grid.candies[shiftInThisRow-1][i] = candy;
                        GLuint gluint = gProgram[candy.colorID];
                        grid.colorMap[shiftInThisRow-1][i] = gluint;
                        shiftInThisRow--;
                    }
                }
            }
        }
    }


    int candyDisappearing = 0;
    //draw model
	for(int i=0; i<grid.height; i++) {
	    for(int j=0; j<grid.width; j++) {
            glUseProgram(grid.colorMap[i][j]);
            // translation
            if(grid.candies[i][j].shiftAmount > 0) {
                grid.currentlyShifting[i][j] = 1;
                translate = grid.candies[i][j].shiftAmount;
                grid.candies[i][j].shiftAmount -= 0.05;
            } else {
                grid.currentlyShifting[i][j] = 0;
                translate = 0;
                grid.correctCandyCount += 1;
            }
            glm::vec3 lightPos = glm::vec3((20.00 * (float(j)+0.50f))/grid.width - 10.00f,
                                                        -(18.50 * (float(i)+0.50f))/grid.height + 10.00f + translate,
                                                        1.00f);

            glm::mat4 T = glm::translate(glm::mat4(1.f), glm::vec3((20.00 * (float(j)+0.50f))/grid.width - 10.00f,
                                                        -(18.50 * (float(i)+0.50f))/grid.height + 10.00f + translate,
                                                        0.00f));

            // rotation
            glm::mat4 R = glm::rotate(glm::mat4(1.f), glm::radians(angle), glm::vec3(0, 1, 0));

            // scale
            glm::mat4 scaleCoefficient = glm::scale(glm::mat4(1.f), glm::vec3(1.f, 1.f, 1.f));
            if(!grid.candies[i][j].visible) { // if current candy should disappear, than use scaling coef
                //cout << "were here" << endl;
                candyDisappearing = 1;
                scaleCoefficient = glm::scale(glm::mat4(1.f), glm::vec3(scaleCoef, scaleCoef, scaleCoef));
            } else {
                scaleCoefficient = glm::scale(glm::mat4(1.f), glm::vec3(1.f, 1.f, 1.f));
            }
            glm::mat4 S = scaleCoefficient * glm::scale(glm::mat4(1.f), glm::vec3(0.5f, 0.5f, 0.5f));


            glm::mat4 modelMat = T * R * S;
            glm::mat4 modelMatInv = glm::transpose(glm::inverse(modelMat));
            glm::mat4 orthoMat = glm::ortho(-10.f, 10.f, -10.f, 10.f, -20.f, 20.f);

            glUniform3fv(glGetUniformLocation(grid.colorMap[i][j], "lightPos"), 1, glm::value_ptr(lightPos));
            glUniformMatrix4fv(glGetUniformLocation(grid.colorMap[i][j], "modelingMat"), 1, GL_FALSE, glm::value_ptr(modelMat));
            glUniformMatrix4fv(glGetUniformLocation(grid.colorMap[i][j], "modelingMatInvTr"), 1, GL_FALSE, glm::value_ptr(modelMatInv));
            glUniformMatrix4fv(glGetUniformLocation(grid.colorMap[i][j], "orthoMat"), 1, GL_FALSE, glm::value_ptr(orthoMat));

            drawModel();

            //assert(glGetError() == GL_NO_ERROR);
            renderText("Score: " + to_string(grid.score) + " Moves: " + to_string(grid.moves), 0, 0, 1, glm::vec3(1, 0, 0));
            //assert(glGetError() == GL_NO_ERROR);
        }
	}

	angle += 0.5;
    if(candyDisappearing){
        scaleCoef += 0.01;
    }
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
    else if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        cout << "R pressed" << endl;
        for (int i = 0; i < grid.height; i++) {
            for (int j = 0; j < grid.width; j++) {
                struct Candy candy;
                candy.colorID = getRandomColorIndex(6);
                candy.visible = true;
                candy.shiftAmount = 0;
                grid.candies[i][j] = candy;
                GLuint gluint = gProgram[candy.colorID];
                grid.colorMap[i][j] = gluint;
                grid.currentlyShifting[i][j] = 0;
                //cout << gluint << endl;
            }
        }
        grid.moves = 0;
        grid.score = 0;
        grid.correctCandyCount = grid.width * grid.height;

        scaleCoef = 1.0;
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
        for (int i = 0; i < grid.height; i++) {
            for (int j = 0; j < grid.width; j++) {
                if(grid.currentlyShifting[i][j] == 1){
                    return;
                }
            }
        }
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        if(ypos <= 555 && scaleCoef <= 1.0){ // en alttaki 45 yazi icin ayrildi
            int clickedCandyXIndex = xpos*grid.width/640;
            int clickedCandyYIndex = ypos*grid.height/555;
            grid.candies[clickedCandyYIndex][clickedCandyXIndex].visible = false;
            vector<int> indexes;
            indexes.push_back(clickedCandyYIndex);
            indexes.push_back(clickedCandyXIndex);
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
    grid.currentlyShifting.resize(grid.height);
    for (int i = 0; i < grid.height; i++) {
        grid.candies[i].resize(grid.width);
        grid.colorMap[i].resize(grid.width);
        grid.currentlyShifting[i].resize(grid.width);
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

