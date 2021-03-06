#include <stdlib.h>
#include <GL/glew.h>
#ifdef __APPLE__
#  include <GLUT/glut.h>
#else
#  include <GL/glut.h>
#endif

#include <math.h>
#include <stdio.h>
#include "Scene.h"
#include "SceneContentManager.h"
#include "ParserScene.h"
#include "CameraManager.h"
#include "FreeCameraManager.h"
#include "RadialCameraManager.h"
#include "InputControl.h"
#include "Shader.h"

#include <iostream>

using namespace std;

#define DEFAULT_WINDOW_HEIGHT 600
#define DEFAULT_WINDOW_WIDTH 600

#define DEFAULT_NEAR_PLANE 0.1
#define DEFAULT_FAR_PLANE 50.0

#define DEFAULT_BACKGROUND_R 0.8
#define DEFAULT_BACKGROUND_G 0.8
#define DEFAULT_BACKGROUND_B 0.8

#define DEFAULT_CONSTANT_ATTENUATION 1.0
#define DEFAULT_LINEAR_ATTENUATION 0.0
#define DEFAULT_QUADRATIC_ATTENUATION 0.0

#define DEFAULT_MAX_LIGHTS 8
#define DEFAULT_FRUSTUM_CULLING_MODE true

Scene *myScene;
InputControl inputControl;
CameraManager *cameraManager = 0;

int windowWidth, windowHeight;

//second-pass resources
struct secondPassResources {

  //buffer locations for the second-pass screen-aligned quad
  GLuint vb_location;
  GLuint tb_location;
  GLuint ib_location;

  GLuint FBO, texture, depthRBO;

} spr;


int fps = 0;
int drawnPoligons = 0;
int oldElapsedTime = 0;
string cameraManagerTypeStr;
string cameraPositionStr, cameraDirectionStr, cameraUpStr, cameraScreenEffectStr;
float cameraFOVy;
string cameraName;

static GLuint make_buffer(GLenum target, const void *buffer_data, GLsizei buffer_size) {
  GLuint buffer;
  glGenBuffers(1, &buffer);
  glBindBuffer(target, buffer);
  glBufferData(target, buffer_size, buffer_data, GL_STATIC_DRAW);
  return buffer;
}

void projectionUpdate(){
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glViewport(0, 0, windowWidth, windowHeight);
  gluPerspective(myScene->getActiveCamera()->getFOVy(), (float)windowWidth/(float)windowHeight, DEFAULT_NEAR_PLANE, DEFAULT_FAR_PLANE);
  glMatrixMode(GL_MODELVIEW);

}

void reshape(int width, int height) {
  windowWidth = width;
  windowHeight = height;

  projectionUpdate();
}

void proxyKeyboardInput(unsigned char key, int x, int y) //proxy function for keyboard input
{
  inputControl.keyboardInput(key, x, y);
}
void proxyMouseInput(int x, int y) //proxy function for mouse input
{
  inputControl.mousePositionInput(x, y);
}
void changeCameraMode(char mode) //'f' or 'r'
{
  if(cameraManager != 0)
    delete(cameraManager);

  switch(mode){
  case 'f': cameraManager = new FreeCameraManager(myScene->getActiveCamera()); break;
  case 'r': cameraManager = new RadialCameraManager(myScene->getActiveCamera(), myScene->getCenter()); break;
  }
 
}


void update()
{
  //check the input to switch camera mode
  vector<unsigned char> pressedKey = inputControl.getPressedKey();
  for(int i = 0; i < pressedKey.size(); i++) {
    if(pressedKey[i] >= '0' && pressedKey[i] <= '9') {
      int totCameras = myScene->countCameras();
      if(pressedKey[i] - '0' < totCameras) {
	myScene->activateCamera(pressedKey[i] - '0');
	cameraManager->setCamera(myScene->getActiveCamera());
      }
    }
    else switch(pressedKey[i]) {
      case 'f':
      case 'F': changeCameraMode('f'); break;
      case 'r':
      case 'R': changeCameraMode('r'); break;
      }
  }

  //move the camera
  cameraManager->moveCamera(inputControl);
  inputControl.newCicle();
  projectionUpdate(); //update the projection matrix to the current FOV
  glutPostRedisplay();

  //update timer and fps
  int elapsedTime = glutGet(GLUT_ELAPSED_TIME);
  fps = 1000 / (elapsedTime - oldElapsedTime);
  oldElapsedTime = elapsedTime;
}

void drawObject(SceneObject object) {

  //LOAD PROGRAM & UNIFORM PARAMS
  GLuint program = object.getMaterialAlgorithm().getProgram();
  glUseProgram(program);

  GLint location = glGetUniformLocation(program, Scene::DEFAULT_NUMBEROFLIGHTS_VARIABLE_NAME.c_str());
  glUniform1i(location, myScene->getTransformedLightList().size());

  vector<string> paramNames = object.getParameterList();

  for (int i=0; i<paramNames.size(); i++){
    location = glGetUniformLocation(program, paramNames[i].c_str());

    vector<float> paramValues = object.getParameterValue(paramNames[i]);

    GLfloat *values = new GLfloat[paramValues.size()];
    for (int j=0; j<paramValues.size(); j++){
      values[j]=paramValues[j];
    }

    //can't be used to pass arrays
    switch(paramValues.size()){
    case 1: glUniform1fv(location, 1, values); break;
    case 2: glUniform2fv(location, 1, values); break;
    case 3: glUniform3fv(location, 1, values); break;
    case 4: glUniform4fv(location, 1, values); break;
    }

    delete[] values;
  }

  //BIND BUFFERS & TEX OBJECTS
  glBindBuffer(GL_ARRAY_BUFFER, object.getVertexBuffer());		//vertex coordinates
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(3, GL_FLOAT, sizeof(GLfloat)*3,(void*) 0);


  //LOAD TANGENTS AND BITANGENTS AS VERTEX ATTRIBUTES
  //in the vertex shader, vec3 attributes MUST be named "gry_tangent" and "gry_bitangent"

  if (object.getTangents().size()>0){

    vector<GLfloat> vec = object.getTangents();
    GLuint tangentBufferLoc = make_buffer(GL_ARRAY_BUFFER, &vec[0], sizeof(GLfloat)*vec.size());

    glBindBuffer(GL_ARRAY_BUFFER, tangentBufferLoc);
    glVertexAttribPointer(Scene::DEFAULT_TANGENT_LOCATION, 3, GL_FLOAT, false, sizeof(GLfloat)*3, (void*)0);

    vec = object.getBitangents();
    GLuint bitangentBufferLoc = make_buffer(GL_ARRAY_BUFFER, &vec[0], sizeof(GLfloat)*vec.size());

    glBindBuffer(GL_ARRAY_BUFFER, bitangentBufferLoc);
    glVertexAttribPointer(Scene::DEFAULT_BITANGENT_LOCATION, 3, GL_FLOAT, false, sizeof(GLfloat)*3, (void*)0);

    glEnableVertexAttribArray(Scene::DEFAULT_TANGENT_LOCATION);
    glEnableVertexAttribArray(Scene::DEFAULT_BITANGENT_LOCATION);
  }


  if(object.getNormalCount()>0){
    glBindBuffer(GL_ARRAY_BUFFER, object.getNormalBuffer());
    glEnableClientState(GL_NORMAL_ARRAY);
    glNormalPointer(GL_FLOAT, sizeof(GLfloat)*3,(void*) 0);
  }

  if(object.isTextureSet()){

    //texture coordinates
    glBindBuffer(GL_ARRAY_BUFFER, object.getTextureBuffer());
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, sizeof(GLfloat)*2,(void*) 0);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);


    vector<string> textureNames = object.getTextureNames();
    for(int i=0; i<textureNames.size();i++){
      glActiveTexture(GL_TEXTURE0+i);
      glBindTexture(GL_TEXTURE_2D, object.getTextureObject(textureNames[i]));
      GLint location = glGetUniformLocation(program, textureNames[i].c_str());
      glUniform1i(location, i);
    }

  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, object.getFaceBuffer());	//faces
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glDrawElements(GL_TRIANGLES, object.getFaceCount(), GL_UNSIGNED_INT, (void*)0);

  if (object.isTextureSet()) {
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

  }
  glDisableClientState(GL_VERTEX_ARRAY);
  if (object.getNormalCount()>0)
    glDisableClientState(GL_NORMAL_ARRAY);

  if (object.getTangents().size()>0){
    glDisableVertexAttribArray(Scene::DEFAULT_TANGENT_LOCATION);
    glDisableVertexAttribArray(Scene::DEFAULT_BITANGENT_LOCATION);
  }
}

void drawNode(SceneTreeNode t){
  glPushMatrix();

  //apply the node transformations to the modelview matrix
  Vector param;
  float angle;
  param=t.getTranslation();
  glTranslatef(param.X, param.Y, param.Z);
  t.getRotation(angle, param);
  glRotatef(angle, param.X, param.Y, param.Z); //l'angolo potrebbe essere salvato sulla 4a componente
  param=t.getScale();
  glScalef(param.X,param.Y,param.Z);

  //draw each object of the node where opaque==opaqueDrawing
  vector<SceneObject> objects=t.getObjectsInNode();
  for (vector<SceneObject>::iterator i = objects.begin(); i != objects.end(); i++)
    drawObject(*i);

  //recursively call the drawNode function to each child
  vector<SceneTreeNode> children=t.getSubTreeNodes();
  for (vector<SceneTreeNode>::iterator i = children.begin(); i != children.end(); i++)
    drawNode(*i);

  glPopMatrix();
}



void firstpass(void) {

  glDepthMask(GL_TRUE);

  //background color
  glClearColor( DEFAULT_BACKGROUND_R, DEFAULT_BACKGROUND_G, DEFAULT_BACKGROUND_B, 1.0f);
  glClearDepth(1);

  //clean the buffers
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  glLoadIdentity();

  //set camera
  SceneCamera camera = myScene->getTransformedActiveCamera();
  Vector pos=camera.getPosition();
  Vector dir=camera.getDirection();
  dir+=pos;
  Vector up=camera.getUp();
  gluLookAt(pos.X,pos.Y,pos.Z,dir.X,dir.Y,dir.Z,up.X,up.Y,up.Z);


  //manage lights
  vector<SceneLight> lights = myScene->getTransformedLightList();

  int i = 0;
  while(i < DEFAULT_MAX_LIGHTS && i < lights.size()){

    GLfloat values[4];

    values[0]=lights[i].getPosition().X;
    values[1]=lights[i].getPosition().Y;
    values[2]=lights[i].getPosition().Z;
    values[3]=lights[i].getPosition().W;

    glLightfv(GL_LIGHT0+i, GL_POSITION, values); //position/direction

    values[0]=lights[i].getIrradiance().X;
    values[1]=lights[i].getIrradiance().Y;
    values[2]=lights[i].getIrradiance().Z;
    values[3]=1.0;

    glLightfv(GL_LIGHT0+i, GL_DIFFUSE, values); //irradiance
    glLightfv(GL_LIGHT0+i, GL_SPECULAR, values);
    glLightfv(GL_LIGHT0+i, GL_AMBIENT, values);

    glLightf(GL_LIGHT0+i, GL_CONSTANT_ATTENUATION, DEFAULT_CONSTANT_ATTENUATION); //attenuation factors
    glLightf(GL_LIGHT0+i, GL_LINEAR_ATTENUATION, DEFAULT_LINEAR_ATTENUATION);
    glLightf(GL_LIGHT0+i, GL_QUADRATIC_ATTENUATION, DEFAULT_QUADRATIC_ATTENUATION);
    
    i++;
  }


  SceneTreeNode sceneTree = myScene->getTree(camera.getFOVy(), (float)windowWidth/(float)windowHeight, DEFAULT_NEAR_PLANE, DEFAULT_FAR_PLANE);
  drawnPoligons = sceneTree.countTotalPoligonsInSubTree();
  drawNode(sceneTree);

}



void secondpass(GLuint texture){ //render the texture to a screen-aligned quad

  glBindFramebuffer(GL_FRAMEBUFFER, 0); //switch back to the window framebuffer rendering
  
  glClearColor( DEFAULT_BACKGROUND_R, DEFAULT_BACKGROUND_G, DEFAULT_BACKGROUND_B, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glViewport(0, 0, windowWidth, windowHeight);
  glMatrixMode(GL_MODELVIEW);

  glLoadIdentity();

  GLuint program = myScene->getActiveCamera()->getScreenEffect().getProgram(); //load the screen-space effect program of the current camera
  glUseProgram(program);

  glActiveTexture(GL_TEXTURE0);	//bind the texture to the texture unit 0
  glBindTexture(GL_TEXTURE_2D, texture);

  //screen-space shaders MUST use the following variable names:
  // "texture" as the texture to be processed
  // "width" and "height" as dimensions

  GLint location = glGetUniformLocation(program, Scene::DEFAULT_SECONDPASS_TEXTURE_VARIABLE_NAME.c_str());
  glUniform1i(location, 0); //pass the texture binded on unit 0

  location = glGetUniformLocation(program, Scene::DEFAULT_SECONDPASS_WIDTH_VARIABLE_NAME.c_str());
  glUniform1f(location, windowWidth);

  location = glGetUniformLocation(program, Scene::DEFAULT_SECONDPASS_HEIGHT_VARIABLE_NAME.c_str());
  glUniform1f(location, windowHeight);

  glBindBuffer(GL_ARRAY_BUFFER, spr.vb_location);
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(3, GL_FLOAT, sizeof(GLfloat)*3, (void*)0);

  glBindBuffer(GL_ARRAY_BUFFER, spr.tb_location);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glClientActiveTexture(GL_TEXTURE0);
  glTexCoordPointer(2, GL_FLOAT, sizeof(GLfloat)*2, (void*)0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, spr.ib_location);

  glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_SHORT, (void*)0);

  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);

}


void render(void){


  if(myScene->getActiveCamera()->thereIsScreenEffect()) {

    glGenFramebuffers(1, &(spr.FBO)); //create the frame buffer object
    glBindFramebuffer(GL_FRAMEBUFFER, spr.FBO); //bind the FBO

    glGenTextures(1, &(spr.texture)); //create the texture object
    glBindTexture(GL_TEXTURE_2D, spr.texture);

    glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, windowWidth, windowHeight, 0,GL_RGB, GL_UNSIGNED_BYTE, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, spr.texture, 0);

    glGenRenderbuffers(1, &(spr.depthRBO));
    glBindRenderbuffer(GL_RENDERBUFFER, spr.depthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, windowWidth, windowHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, spr.depthRBO);

    firstpass();

    secondpass(spr.texture); //render again using the texture

    //delete resources
    glDeleteTextures(1, &(spr.texture));
    glDeleteFramebuffers(1, &(spr.FBO));
    glDeleteRenderbuffers(1, &(spr.depthRBO));

  } 
  else 
    firstpass();
  
  

  glutSwapBuffers();

}

//initialization of the scene
int initialization(int argc, char** argv){
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowSize(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
  glutCreateWindow("Worlds");
  glutDisplayFunc(render);
  glutReshapeFunc(reshape);

  glutKeyboardFunc(proxyKeyboardInput);
  glutMotionFunc(proxyMouseInput);
  glutIdleFunc(update);

  glewInit();
  if (!GLEW_VERSION_2_0) {
    fprintf(stderr, "OpenGL 2.0 not available\n");
    return 1;
  }

  //load scene from a file
  myScene = new Scene(argv[1], DEFAULT_FRUSTUM_CULLING_MODE);

  //initialize with a free camera
  changeCameraMode('f');

  windowWidth = DEFAULT_WINDOW_WIDTH;
  windowHeight = DEFAULT_WINDOW_HEIGHT;

  if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    //if something went wrong..
    printf("GL_FRAMEBUFFER_COMPLETE_EXT failed, CANNOT use FBO\n");
    return -1;
  }

  //second-pass resources initialization
  GLfloat quadVertices[]={
    -1.0, -1.0, -1.0,
    1.0, -1.0, -1.0,
    -1.0, 1.0, -1.0,
    1.0, 1.0, -1.0};
  GLushort quadIndices[] = {0, 1, 2, 3};
  GLfloat quadTexCoord[] = {0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0};

  spr.vb_location = make_buffer(GL_ARRAY_BUFFER, quadVertices, sizeof(quadVertices));
  spr.tb_location = make_buffer(GL_ARRAY_BUFFER, quadTexCoord, sizeof(quadTexCoord));
  spr.ib_location = make_buffer(GL_ELEMENT_ARRAY_BUFFER, quadIndices, sizeof(quadIndices));

  //camera info group
  cameraManagerTypeStr = cameraManager->getCameraControlTypeString();

  vector<SceneCamera> cameraList = myScene->getCameraList();

  return 0;
}




int main(int argc, char** argv){

  if(argc <= 1 || initialization(argc,argv)==1){
    printf("No parameters\n");
    return 1;
  }

  glutMainLoop();
  return 0;
}
