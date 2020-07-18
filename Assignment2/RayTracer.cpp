#include <iostream>
#include <cmath>
#include <vector>
#include <glm/glm.hpp>
#include "Sphere.h"
#include "SceneObject.h"
#include "Ray.h"
#include <GL/freeglut.h>
#include "Plane.h"
#include "TextureBMP.h"
#include "Cylinder.h"
using namespace std;

const float WIDTH = 20.0;  
const float HEIGHT = 20.0;
const float EDIST = 40.0;
const int NUMDIV = 500;
const int MAX_STEPS = 5;
const float XMIN = -WIDTH * 0.5;
const float XMAX =  WIDTH * 0.5;
const float YMIN = -HEIGHT * 0.5;
const float YMAX =  HEIGHT * 0.5;

vector<SceneObject*> sceneObjects;
TextureBMP texture;

float calculateAngle(glm::vec3 shadowRayDir, glm::vec3 spotDir){
    float dotProduct = dot(shadowRayDir, spotDir);
    float magnitude = length(shadowRayDir) * length(spotDir);
    return abs(acos(dotProduct / magnitude));
}

//---The most important function in a ray tracer! ---------------------------------- 
//   Computes the colour value obtained by tracing a ray and finding its 
//     closest point of intersection with objects in the scene.
//----------------------------------------------------------------------------------
glm::vec3 trace(Ray ray, int step)
{
	glm::vec3 backgroundCol(1);						//Background colour = (0,0,0)
	glm::vec3 lightPos1(20, 40, -3);					//Light's position
    glm::vec3 lightPos2(-20, 40, -3);
    glm::vec2 fogRange(-80, -200);
	glm::vec3 color(0);
	SceneObject* obj;

    ray.closestPt(sceneObjects);					//Compare the ray with all objects in the scene
    if(ray.index == -1) return backgroundCol;		//no intersection
	obj = sceneObjects[ray.index];					//object on which the closest point of intersection is found


	if(obj->isTransparent() && step < MAX_STEPS){
	    Ray transparentRay(ray.hit, ray.dir);
	    transparentRay.closestPt(sceneObjects);
	    Ray finalRay(transparentRay.hit, transparentRay.dir);
	    glm::vec3 transparentColor = trace(finalRay, step+1);
	    obj->setColor(transparentColor);
	}

	if(obj->isRefractive()){
	    float refracIndex = obj->getRefractiveIndex();
	    glm::vec3 n = obj->normal(ray.hit);
	    glm::vec3 g = glm::refract(ray.dir, n, 1/refracIndex);
	    Ray refractedRay(ray.hit, g);
	    refractedRay.closestPt(sceneObjects);
	    glm::vec3 m = obj->normal(refractedRay.hit);
	    glm::vec3 h = glm::refract(g, -m, refracIndex);
	    Ray exitRay(refractedRay.hit, h);
	    glm::vec3 refractiveColor = trace(exitRay, step + 1);
	    obj->setColor(refractiveColor);
	}

	//Check pattern generation
    if(ray.index == 1){
        int checkSize = 5;
        int iz = (int) (ray.hit.z) / checkSize;
        int kz = iz % 2;
        int ix = (int) (ray.hit.x + 30) / checkSize;
        int kx = ix % 2;
        if (kz + kx == 0) color = glm::vec3(0, 0, 1);
        else color = glm::vec3(1, 0, 0);
        obj->setColor(color);
    }

    if(ray.index == 3){
        glm::vec3 d = obj->normal(ray.hit);
        float s = 0.5 + (atan2(d.x, d.z)) /  (2 * M_PI);
        float t = 0.5 + (asin(d.y)) / M_PI;
        if(s > 0 && s < 1 && t > 0 && t < 1){
            color = texture.getColorAt(s, t);
            obj->setColor(color);
        }
    }

    color = obj->lighting(lightPos1, -ray.dir, ray.hit) + obj->lighting(lightPos2, -ray.dir, ray.hit);
            	//Object's color
    bool shadowApplied = false;
    glm::vec3 lightVec1 = lightPos1 - ray.hit;
    glm::vec3 lightVec2 = lightPos2 - ray.hit;
    Ray shadowRay1(ray.hit, lightVec1);
    Ray shadowRay2(ray.hit, lightVec2);
    shadowRay1.closestPt(sceneObjects);
    shadowRay2.closestPt(sceneObjects);
    float lightDist1 = glm::length(lightVec1);
    float lightDist2 = glm::length(lightVec2);
    if(!obj->isTransparent()){
        if (shadowRay1.index > -1 && shadowRay1.dist < lightDist1){
            if(sceneObjects[shadowRay1.index]->isTransparent() || sceneObjects[shadowRay1.index]->isRefractive()){
                color = obj->getColor();
                color *= 0.5;
            } else {
                color = obj->getColor();
                color *= 0.2;
            }
            shadowApplied = true;
        }
        if (shadowRay2.index > -1 && shadowRay2.dist < lightDist2){
            if(sceneObjects[shadowRay2.index]->isTransparent() || sceneObjects[shadowRay2.index]->isRefractive()){
                if(!shadowApplied) color=obj->getColor();
                color *= 0.5;
            } else {
                if(!shadowApplied) color=obj->getColor();
                color *= 0.2;
            }
        }
    }


    if (obj->isReflective() && step < MAX_STEPS){
        float rho = obj->getReflectionCoeff();
        glm::vec3 normalVec = obj->normal(ray.hit);
        glm::vec3 reflectedDir = glm::reflect(ray.dir, normalVec);
        Ray reflectedRay(ray.hit, reflectedDir);
        glm::vec3 reflectedColor = trace(reflectedRay, step + 1);
        color = color + (rho * reflectedColor);
    }
    if(ray.hit.z <= fogRange.x){
        float t = (ray.hit.z - fogRange.x) / (fogRange.y - fogRange.x);
        color = (1 - t) * color + t * glm::vec3(1);
    }

	return color;
}

//---The main display module -----------------------------------------------------------
// In a ray tracing application, it just displays the ray traced image by drawing
// each cell as a quad.
//---------------------------------------------------------------------------------------
void display()
{
	float xp, yp;  //grid point
	float cellX = (XMAX-XMIN)/NUMDIV;  //cell width
	float cellY = (YMAX-YMIN)/NUMDIV;  //cell height
	glm::vec3 eye(0., -3., 0.);

	glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

	glBegin(GL_QUADS);  //Each cell is a tiny quad.

	for(int i = 0; i < NUMDIV; i++)	//Scan every cell of the image plane
	{
		xp = XMIN + i*cellX;
		for(int j = 0; j < NUMDIV; j++)
		{
			yp = YMIN + j*cellY;

			//Directions of the 4 rays for the cell
            glm::vec3 dir1(xp+0.25*cellX, yp+0.25*cellY, -EDIST);
            glm::vec3 dir2(xp+0.75*cellX, yp+0.25*cellY, -EDIST);
            glm::vec3 dir3(xp+0.25*cellX, yp+0.75*cellY, -EDIST);
            glm::vec3 dir4(xp+0.75*cellX, yp+0.75*cellY, -EDIST);

            //Create the four rays for the cell
            Ray ray1 = Ray(eye, dir1);
            Ray ray2 = Ray(eye, dir2);
            Ray ray3 = Ray(eye, dir3);
            Ray ray4 = Ray(eye, dir4);


            //trace the four rays for the cell
            glm::vec3 col1 = trace (ray1, 1);
            glm::vec3 col2 = trace (ray2, 1);
            glm::vec3 col3 = trace (ray3, 1);
            glm::vec3 col4 = trace (ray4, 1);

            //Average the colour
            float red = (col1.r + col2.r + col3.r + col4.r) / 4;
            float green = (col1.g + col2.g + col3.g + col4.g) / 4;
            float blue = (col1.b + col2.b + col3.b + col4.b) / 4;

            //Draw each cell with its color value
            glColor3f(red, green, blue);
            glVertex2f(xp, yp);
            glVertex2f(xp+cellX, yp);
            glVertex2f(xp+cellX, yp+cellY);
            glVertex2f(xp, yp+cellY);
        }
    }

    glEnd();
    glFlush();
}

void createBox(){
    //front
    glm::vec3 box_color = glm::vec3(0.384, 0.729, 0.854);
    Plane *box_front = new Plane(glm::vec3(-10., -12.5, -55),
                               glm::vec3(-10., -15, -55),
                               glm::vec3(10., -15.0, -55),
                               glm::vec3(10., -12.5, -55));
    box_front->setColor(box_color);
    box_front->setSpecularity(false);
    sceneObjects.push_back(box_front);
    //rear
    Plane *box_rear = new Plane(glm::vec3(-10., -12.5, -70),
                                 glm::vec3(-10., -14.9, -70),
                                 glm::vec3(10., -14.9, -70),
                                 glm::vec3(10., -12.5, -70));
    box_rear->setColor(box_color);
    box_rear->setSpecularity(false);
    sceneObjects.push_back(box_rear);
    //top
    Plane *box_top = new Plane(glm::vec3(-10., -12.5, -55),
                               glm::vec3(10., -12.5, -55),
                               glm::vec3(10., -12.5, -70),
                               glm::vec3(-10., -12.5, -70));
    box_top->setColor(box_color);
    box_top->setSpecularity(false);
    sceneObjects.push_back(box_top);
    //bottom
    Plane *box_bottom = new Plane(glm::vec3(-10., -14.9, -55),
                               glm::vec3(10., -14.9, -55),
                               glm::vec3(10., -14.9, -70),
                               glm::vec3(-10., -14.9, -70));
    box_bottom->setColor(box_color);
    box_bottom->setSpecularity(false);
    sceneObjects.push_back(box_bottom);
    //left
    Plane *box_left = new Plane(glm::vec3(-10., -12.5, -55),
                                  glm::vec3(-10., -12.5, -70),
                                  glm::vec3(-10., -14.9, -70),
                                  glm::vec3(-10., -14.9, -55));
    box_left->setColor(box_color);
    box_left->setSpecularity(false);
    sceneObjects.push_back(box_left);
    //right
    Plane *box_right = new Plane(glm::vec3(10., -12.5, -70),
                                 glm::vec3(10., -12.5, -55),
                                 glm::vec3(10., -14.9, -55),
                                 glm::vec3(10., -14.9, -70));
    box_right->setColor(box_color);
    box_right->setSpecularity(false);
    sceneObjects.push_back(box_right);
}

//---This function initializes the scene ------------------------------------------- 
//   Specifically, it creates scene objects (spheres, planes, cones, cylinders etc)
//     and add them to the list of scene objects.
//   It also initializes the OpenGL orthographc projection matrix for drawing the
//     the ray traced image.
//----------------------------------------------------------------------------------
void initialize()
{
    glMatrixMode(GL_PROJECTION);
    gluOrtho2D(XMIN, XMAX, YMIN, YMAX);

    glClearColor(0, 0, 0, 1);

    Sphere *transparentSphere = new Sphere(glm::vec3(-5.0, -9.5, -55.0), 2.5);
    transparentSphere->setColor(glm::vec3(0, 0, 0));
    transparentSphere->setReflectivity(true, 0.1);
    transparentSphere->setTransparency(true);
    sceneObjects.push_back(transparentSphere);		 //Add sphere to scene objects

    Plane *floor_plane = new Plane(glm::vec3(-30., -15, -40),
                                   glm::vec3(30., -15, -40),
                                   glm::vec3(30., -15, -200),
                                   glm::vec3(-30., -15, -200));
    floor_plane->setSpecularity(false);
    sceneObjects.push_back(floor_plane);

    Sphere *refractiveSphere = new Sphere(glm::vec3(5.0, -9.5, -55.0), 2.5);
    refractiveSphere->setColor(glm::vec3(0, 0, 0));   //Set colour to blue
    refractiveSphere->setReflectivity(true, 0.1);
    refractiveSphere->setRefractivity(true,1.0,1.01);
    sceneObjects.push_back(refractiveSphere);

    Sphere *textureSphere = new Sphere(glm::vec3(0, -2, -130.0), 4);
    sceneObjects.push_back(textureSphere);

    Sphere *cylinder = new Sphere(glm::vec3(0, -12.5, -50), 2.5);
    cylinder->setColor(glm::vec3(0, 1, 0));
    sceneObjects.push_back(cylinder);

    createBox();
    texture = TextureBMP("earth_small.bmp");
}


int main(int argc, char *argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB );
    glutInitWindowSize(1000, 1000);
    glutInitWindowPosition(20, 20);
    glutCreateWindow("COSC363 Assignment 2 rbe72");

    glutDisplayFunc(display);
    initialize();

    glutMainLoop();
    return 0;
}
