//
// CS 174A Project 3
// Ray Tracing
// Nick Westman
// UID 903996152


#define _CRT_SECURE_NO_WARNINGS
#include "matm.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm> //To get max and min, which are used in clamping pixel values
#include <omp.h> //To speed up the process
using namespace std;

struct Ray
{
    vec4 origin;
    vec4 dir;
};

struct Sphere
{
	string name;
	vec4 position;
	vec4 color;
	vec3 scale;
	float Ka;
	float Kd;
	float Ks;
	float Kr;
	float n;

	// Printing Utility
	friend std::ostream& operator << ( std::ostream& os, const Sphere& s ) {
        return os << "Name " << s.name << "\nPos " << s.position
			<< "\nColor " << s.color << "\nScale " << s.scale
			<< "\nK Values " << vec4(s.Ka, s.Kd, s.Ks, s.Kr)
			<< "\nN " << s.n;
    }
};

struct Light
{
	string name;
	vec4 position;
	vec4 color;

	//Printing Utility
	friend std::ostream& operator << ( std::ostream& os, const Light& l ) {
		return os << "Name " << l.name
			<< "\nPosition " << l.position
			<< "\nColor " << l.color;
    }
};

enum Type
{
	NO_INTERSECTION,
	SPHERE,
	H_SPHERE
};

struct Intersection 
{
	vec4 position;
	Type t;
	union {
		Light * l;
		Sphere * s;
	};
};

vector<vec4> g_colors;

Sphere g_spheres[5]; //5 is the max
Light  g_lights[5];

int num_spheres;
int num_lights;

vec4 g_back; //Background color
vec4 g_ambient; //Ambient intensity

string g_output;

int g_width;
int g_height;

float g_left;
float g_right;
float g_top;
float g_bottom;
float g_near;

// -------------------------------------------------------------------
// Input file parsing -- Added toVec3 Function

vec4 toVec4(const string& s1, const string& s2, const string& s3)
{
    stringstream ss(s1 + " " + s2 + " " + s3);
    vec4 result;
    ss >> result.x >> result.y >> result.z;
    result.w = 1.0f;
    return result;
}

vec3 toVec3(const string& s1, const string& s2, const string& s3)
{
    stringstream ss(s1 + " " + s2 + " " + s3);
    vec3 result;
    ss >> result.x >> result.y >> result.z;
    return result;
}

float toFloat(const string& s)
{
    stringstream ss(s);
    float f;
    ss >> f;
    return f;
}

void parseLine(const vector<string>& vs)
{
      if (vs[0] == "RES") {
        g_width = (int)toFloat(vs[1]);
        g_height = (int)toFloat(vs[2]);
        g_colors.resize(g_width * g_height);
    } else if (vs[0] == "NEAR") {
		g_near = toFloat(vs[1]);
	} else if (vs[0] == "LEFT") {
		g_left = toFloat(vs[1]);
	} else if (vs[0] == "RIGHT") {
		g_right = toFloat(vs[1]);
	} else if (vs[0] == "BOTTOM") {
		g_bottom = toFloat(vs[1]);
	} else if (vs[0] == "TOP") {
		g_top = toFloat(vs[1]);
	} else if (vs[0] == "BACK") {
		g_back = toVec4(vs[1], vs[2], vs[3]);
	} else if (vs[0] == "AMBIENT") {
		g_ambient = toVec4(vs[1], vs[2], vs[3]);
	} else if (vs[0] == "OUTPUT") {
		g_output = vs[1];
	} else if (vs[0] == "SPHERE") {
		Sphere * s = &g_spheres[num_spheres++];
		s->name = vs[1];
		s->position = toVec4(vs[2],vs[3],vs[4]);
		s->scale = toVec3(vs[5],vs[6],vs[7]);
		s->color = toVec4(vs[8],vs[9],vs[10]);
		s->Ka = toFloat(vs[11]);
		s->Kd = toFloat(vs[12]);
		s->Ks = toFloat(vs[13]);
		s->Kr = toFloat(vs[14]);
		s->n  = toFloat(vs[15]);
	} else if (vs[0] == "LIGHT") {
		Light * l = &g_lights[num_lights++];
		l->name = vs[1];
		l->position = toVec4(vs[2],vs[3],vs[4]);
		l->color = toVec4(vs[5],vs[6],vs[7]);
	}
}

void loadFile(const char* filename)
{
    ifstream is(filename);
    if (is.fail())
    {
        cout << "Could not open file " << filename << endl;
        exit(1);
    }
    string s;
    vector<string> vs;
    while(!is.eof())
    {
        vs.clear();
        getline(is, s);
        istringstream iss(s);
        while (!iss.eof())
        {
            string sub;
            iss >> sub;
            vs.push_back(sub);
        }
        parseLine(vs);
    }
}


// -------------------------------------------------------------------
// Utilities

void setColor(int ix, int iy, const vec4& color)
{
    int iy2 = g_height - iy - 1; // Invert iy coordinate.
    g_colors[iy2 * g_width + ix] = color;
}

inline
float determinant(float a, float b, float c)
{
    return (b * b) - (a * c);
}

void printArgs()
{
	cout << "Height is "  << g_height << endl;
	cout << "Width is "   << g_width  << endl;
	cout << "Near is "    << g_near   << endl;
	cout << "Left is "    << g_left   << endl;
	cout << "Right is "   << g_right  << endl;
	cout << "Top is "     << g_top    << endl;
	cout << "Bottom is "  << g_bottom << endl;

	for(int i = 0; i < num_spheres; i++)
	{
		cout << "Sphere " << i << ": " << g_spheres[i] << endl;
	}

	for(int i = 0; i < num_lights; i++)
	{
		cout << "Light " << i << ": " << g_lights[i] << endl;
	}

	cout << "Background color is " << g_back << endl;
	cout << "Ambient color is " << g_ambient << endl;
	cout << "Output file is " << g_output << endl;
}


// -------------------------------------------------------------------
// Intersection routine
// Returns an intersection object based off of the ray.

Intersection findIntersection(const Ray& ray)
{
	float intersect = 1000.0;
	float minDist = g_near; 
	Intersection toRet;
	toRet.t = NO_INTERSECTION;

	for(int i = 0; i < num_spheres; i++)
	{
		mat4 undoScale;
		InvertMatrix(Scale(g_spheres[i].scale), undoScale);

		vec4 originToSphere = g_spheres[i].position - ray.origin;
		
		vec4 S = undoScale * originToSphere;
		vec4 C = undoScale * ray.dir;

		// Quadratic equation for the solution:
		// c*c * t^2 - 2 * dot(s,c) * t + (s*s - 1) = 0
		// Just use the quadratic formula for a solution for t!

		float a = dot(C,C);
		float b = dot(S,C);
		float c = dot(S,S) - 1;

		if ( determinant(a,b,c) < 0 ) //intersection can't be imaginary!
			continue;

		float r = sqrtf(determinant(a,b,c))/a;
		float t1 = b/a - r;
		float t2 = b/a + r;

		if(t1 > minDist && t1 < intersect)
		{
			intersect = t1;
			toRet.position = ray.origin + intersect * ray.dir;
			if(t2 > minDist) //Both are past near plane
				toRet.t = SPHERE;
			else
				toRet.t = H_SPHERE;
			toRet.s = &g_spheres[i];
		}
		
		if(t2 > minDist && t2 < intersect)
		{
			intersect = t2;
			toRet.position = ray.origin + intersect * ray.dir;
			if(t1 > minDist) //Both are past near plane
				toRet.t = SPHERE;
			else
				toRet.t = H_SPHERE;
			toRet.s = &g_spheres[i];
		}
	}

	return toRet;
}

inline
vec4 findNormal(const Intersection& I)
{
	vec4 normal = I.position - I.s->position;
	mat4 undoScale; //Gradient is just dividing by scale twice - it does a good job.
	InvertMatrix(Scale(I.s->scale), undoScale);
	normal.w = 0;
	return normalize(2*undoScale*undoScale*normal);
}


// -------------------------------------------------------------------
// Ray tracing

vec4 trace(const Ray& ray, int rlevel) //Step holds max level of recur
{
	Intersection intersect = findIntersection(ray);

	if(intersect.t == NO_INTERSECTION || rlevel >= 5)
		return g_back;

	// Set Ambient color
	vec4 color = intersect.s->color * intersect.s->Ka * g_ambient;

	vec4 normal   = findNormal(intersect);
	vec4 diffuse  = vec4(0.0,0.0,0.0,0.0);
	vec4 specular = vec4(0.0,0.0,0.0,0.0);

	for(int i = 0; i < num_lights; i++)
	{
		Ray toLight;
		toLight.origin = intersect.position;
		toLight.dir    = normalize(g_lights[i].position - intersect.position);

		Intersection lightIntersect = findIntersection(toLight); //Shadow Rays. Code Doesn't actually matter.

		if(lightIntersect.t == NO_INTERSECTION)
		{
			float proj = dot(normal, toLight.dir); //Projection of light ray onto normal
			if(proj < 0) //Don't care, doesn't hit light.
				continue;

			diffuse += proj * g_lights[i].color * intersect.s->color * intersect.s->Kd;

			float proj2 = dot(normal, ray.dir); //Projection of ray onto normal
			float spec  = dot((toLight.dir - ray.dir), (toLight.dir - ray.dir)); //Blinn vector magnitude squared
			if (spec > 0.0) //Avoid taking sqrt of negative numbers
			{
				spec = max(proj-proj2,0.0f)/sqrtf(spec);
				specular += powf(spec, intersect.s->n * 3) * g_lights[i].color * intersect.s->Ks;
			}
		}
	}

	// Finish Phong Shading
	color += diffuse + specular;

	if(intersect.t == H_SPHERE) //Hollow Spheres don't have reflections.
		return color;

	Ray ref;
	ref.origin = intersect.position;
	ref.dir    = normalize(ray.dir - 2.0 * dot(normal, ray.dir) * normal);

	vec4 reflection = trace(ref, rlevel+1);

	if(reflection != g_back) //Re-wrote the != operator in vecm.h to do what we want it to do! (Tolerance is 0.01. Check vecmh for info)
		color += reflection*intersect.s->Kr;
	
    return color;
}

vec4 getDir(int ix, int iy)
{
    float x = g_left + (float) ix * (g_right - g_left) / g_width;
	float y = g_bottom + (float) iy * (g_top - g_bottom) / g_height;
    vec4 dir;
    dir = vec4(x, y, -g_near, 0.0f);
    return dir;
}

void renderPixel(int ix, int iy)
{
    Ray ray;
    ray.origin = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    ray.dir = getDir(ix, iy);
    vec4 color = trace(ray, 0);
    setColor(ix, iy, color);
}

void render()
{
	int ix; //Open MP directive added to speedup debugging. Why not.
#pragma omp parallel for private(ix)
    for (int iy = 0; iy < g_height; iy++)
        for (ix = 0; ix < g_width; ix++)
            renderPixel(ix, iy);
}


// -------------------------------------------------------------------
// PPM saving

void savePPM(int Width, int Height, char* fname, unsigned char* pixels) 
{
    FILE *fp;
    const int maxVal=255;

    printf("Saving image %s: %d x %d\n", fname, Width, Height);
    fp = fopen(fname,"wb");
    if (!fp) {
        printf("Unable to open file '%s'\n", fname);
        return;
    }
    fprintf(fp, "P6\n");
    fprintf(fp, "%d %d\n", Width, Height);
    fprintf(fp, "%d\n", maxVal);

    for(int j = 0; j < Height; j++) {
        fwrite(&pixels[j*Width*3], 3, Width, fp);
    }

    fclose(fp);
}

inline
float clamp(float c)
{
	return max(min(c,1.0f),0.0f);
}

void saveFile()
{
    // Convert color components from floats to unsigned chars.
    unsigned char* buf = new unsigned char[g_width * g_height * 3];
    for (int y = 0; y < g_height; y++)
        for (int x = 0; x < g_width; x++)
            for (int i = 0; i < 3; i++)
                buf[y*g_width*3+x*3+i] = (unsigned char)(clamp(((float*)g_colors[y*g_width+x])[i]) * 255.9f);
    
	char * f = new char[g_output.length() + 1];
	strcpy(f, g_output.c_str());
    savePPM(g_width, g_height, f, buf);
	delete[] f;
    delete[] buf;
}


// -------------------------------------------------------------------
// Main

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        cout << "Usage: template-rt <input_file.txt>" << endl;
        exit(1);
    }
    loadFile(argv[1]);
//	printArgs();
    render();
    saveFile();
	return 0;
}

