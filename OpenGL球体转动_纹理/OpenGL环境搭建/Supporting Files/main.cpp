
#include "GLShaderManager.h"
#include "GLTools.h"
#include "GLFrustum.h"// 矩阵工具类 设置正投影矩阵/透视投影矩阵
#include "GLFrame.h"// 矩阵工具类 位置
#include "GLMatrixStack.h"// 加载单元矩阵  矩阵/矩阵相乘  压栈/出栈  缩放/平移/旋转
#include "GLGeometryTransform.h"// 交换管道，用来传输矩阵(视图矩阵/投影矩阵/视图投影变换矩阵)

#include "StopWatch.h"// 定时器
#include <math.h>
#include <stdio.h>

#include <glut/glut.h>

///
/*
 整体绘制思路：
 1、绘制地板
 2、绘制大球
 3、绘制随机的50个小球
 4、绘制围绕大球旋转的小球
 5、添加键位控制移动 -- 压栈观察者矩阵
 */

GLShaderManager     shaderManager;
// 两个矩阵
GLMatrixStack       modelViewMatix;// 模型视图矩阵
GLMatrixStack       projectionMatrix;// 投影矩阵
GLFrustum           viewFrustum;// 透视投影 - GLFrustum类
GLGeometryTransform transformPipeline;// 几何图形变换管道

GLBatch             floorTriangleBatch;// 地板
GLTriangleBatch     bigSphereBatch;// 大球体
GLTriangleBatch     sphereBatch;// 小球体

// 设置角色帧，作为相机
GLFrame  cameraFrame;// 观察者位置


// 小球门
//GLFrame sphere;
#define SPHERE_NUM  50
GLFrame spheres[SPHERE_NUM];// 50个小球


// 纹理 3个
GLuint textures[3];


// 绘制球体视图
void drawSpheres (GLfloat yRot) {
    
    // 设置点光源位置
    static GLfloat vLightPos[] = {0.0f, 3.0f, 0.0f, 1.0f};
    // 漫反射颜色 白色
    static GLfloat vWhite[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    
    
    // 绘制 小球们
    // 绑定纹理
    glBindTexture(GL_TEXTURE_2D, textures[2]);// 小球上贴的纹理
    for (int i = 0; i < SPHERE_NUM; i++) {
        
        modelViewMatix.PushMatrix();
        modelViewMatix.MultMatrix(spheres[i]);// 小球位置
        modelViewMatix.Rotate(yRot, 0, 1, 0);
        // GLT_SHADER_TEXTURE_POINT_LIGHT_DIFF：纹理点光源着色器；vLightPos：光源位置；vBigSphereColor：绘制颜色
        shaderManager.UseStockShader(GLT_SHADER_TEXTURE_POINT_LIGHT_DIFF,
                                     modelViewMatix.GetMatrix(),
                                     transformPipeline.GetProjectionMatrix(),
                                     vLightPos,
                                     vWhite,
                                     0);
        sphereBatch.Draw();
        
        modelViewMatix.PopMatrix();
    }
    
    
    // 绘制大球
    modelViewMatix.Translate(0, 0.2f, -2.5f);// y轴上靠近镜面一点 Z轴距离再远一些
    //
    modelViewMatix.PushMatrix();
    // 大球沿Y轴  旋转弧度：yRot 自传
    modelViewMatix.Rotate(yRot, 0, 1, 0);
    // 绑定纹理
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    shaderManager.UseStockShader(GLT_SHADER_TEXTURE_POINT_LIGHT_DIFF,
                                 modelViewMatix.GetMatrix(),
                                 transformPipeline.GetProjectionMatrix(),
                                 vLightPos,
                                 vWhite,
                                 0);
    bigSphereBatch.Draw();
    modelViewMatix.PopMatrix();
    
    
    // 绘制沿大球转的小球
    modelViewMatix.PushMatrix();
    modelViewMatix.Rotate(yRot * -2, 0, 1, 0);
    modelViewMatix.Translate(0.8f, 0.0f, 0.0f);// 小球在X方向上偏移一定位置 不然会看不见小球
    glBindTexture(GL_TEXTURE_2D, textures[2]);
    shaderManager.UseStockShader(GLT_SHADER_TEXTURE_POINT_LIGHT_DIFF,
                                 modelViewMatix.GetMatrix(),
                                 transformPipeline.GetProjectionMatrix(),
                                 vLightPos,
                                 vWhite,
                                 0);
    sphereBatch.Draw();
    modelViewMatix.PopMatrix();
    
}

// 设置纹理
bool LoadTGATexture(const char *textureName, GLenum minFilter, GLenum magFilter, GLenum wrapModel) {
    
    GLbyte *pBits;
    int nWidth, nHeight, nComponents;
    GLenum eFormat;
    
    // 读取纹理
    pBits = gltReadTGABits(textureName, &nWidth, &nHeight, &nComponents, &eFormat);
    if (!pBits) {
        return false;
    }
    
    //  设置过滤方式
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    
    // 设置环绕方式
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapModel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapModel);
    
    // 载入纹理
    glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB, nWidth, nHeight, 0, eFormat, GL_UNSIGNED_BYTE, pBits);
    
    //  使用完毕释放pbit
    free(pBits);
    
    
    // 只有minFilter 等于以下四种模式，才可以生成Mip贴图
    // GL_NEAREST_MIPMAP_NEAREST具有非常好的性能，并且闪烁现象非常弱
    // GL_LINEAR_MIPMAP_NEAREST常常用于对游戏进行加速，它使用了高质量的线性过滤器
    // GL_LINEAR_MIPMAP_LINEAR 和GL_NEAREST_MIPMAP_LINEAR 过滤器在Mip层之间执行了一些额外的插值，以消除他们之间的过滤痕迹。
    // GL_LINEAR_MIPMAP_LINEAR 三线性Mip贴图。纹理过滤的黄金准则，具有最高的精度。
    if (minFilter == GL_LINEAR_MIPMAP_LINEAR ||
        minFilter == GL_LINEAR_MIPMAP_NEAREST ||
        minFilter == GL_NEAREST_MIPMAP_LINEAR ||
        minFilter == GL_NEAREST_MIPMAP_NEAREST) {
        
        // 加载mip 纹理生成所有的mip 层
        // 参数：GL_TEXTURE_1D、GL_TEXTURE_2D、GL_TEXTURE_3D
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    
    return true;
}

// 初始化 设置
void SetupRC() {
    
    // 设置清屏颜色到颜色缓存区
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    // 初始化着色器管理器
    shaderManager.InitializeStockShaders();
    
    // 开启深度测试 背面剔除
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // 设置大球
    gltMakeSphere(bigSphereBatch, 0.4f, 40, 80);
    
    // 设置小球
    gltMakeSphere(sphereBatch, 0.1f, 26, 13);
    
    // 地板 纹理 --> 4个顶点对应纹理图的4个坐标
    GLfloat texSize = 10.0f;
    floorTriangleBatch.Begin(GL_TRIANGLE_FAN, 4,1);
    floorTriangleBatch.MultiTexCoord2f(0, 0.0f, 0.0f);
    floorTriangleBatch.Vertex3f(-20.f, -0.41f, 20.0f);
    
    floorTriangleBatch.MultiTexCoord2f(0, texSize, 0.0f);
    floorTriangleBatch.Vertex3f(20.0f, -0.41f, 20.f);
    
    floorTriangleBatch.MultiTexCoord2f(0, texSize, texSize);
    floorTriangleBatch.Vertex3f(20.0f, -0.41f, -20.0f);
    
    floorTriangleBatch.MultiTexCoord2f(0, 0.0f, texSize);
    floorTriangleBatch.Vertex3f(-20.0f, -0.41f, -20.0f);
    floorTriangleBatch.End();
    
    
    // 小球们的位置 随机小球顶点坐标数据
    for (int i = 0; i < SPHERE_NUM; i++) {
        
        // y轴不变，X,Z产生随机值
        GLfloat x = ((GLfloat)((rand() % 400) - 200 ) * 0.1f);
        GLfloat z = ((GLfloat)((rand() % 400) - 200 ) * 0.1f);
        
        // 在y方向，将球体设置为0.0的位置，这使得它们看起来是飘浮在眼睛的高度
        // 对spheres数组中的每一个顶点，设置顶点数据
        spheres[i].SetOrigin(x, 0.0f, z);
    }
    
    
    // 设置纹理
    // 生成纹理标记
    glGenTextures(3, textures);
    
    // 绑定纹理 设置纹理参数
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    LoadTGATexture("marble.tga", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT);
    
    // GL_CLAMP_TO_EDGE 纹理坐标约束在0~1，超出部分边缘重复(边缘拉伸效果)
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    LoadTGATexture("marslike.tga", GL_LINEAR_MIPMAP_LINEAR,
                   GL_LINEAR, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_2D, textures[2]);
    LoadTGATexture("moonlike.tga", GL_LINEAR_MIPMAP_LINEAR,
                   GL_LINEAR, GL_CLAMP_TO_EDGE);
}



// 渲染
void RenderScene(void) {
    
    // 清除窗口 颜色、深度缓冲区
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    //    // 颜色们
    //    static GLfloat vFloorColor[] = {0.0, 0.5, 0.5, 1};// 地板颜色
    //    static GLfloat vBigSphereColor[] = {0.3, 0.5, 0.5, 1};// 大球颜色
    //    static GLfloat vSphereColor[] = {0.5, 0.5, 0.7, 1};// 小球颜色
    
    // 地板颜色 --> 镜面透明效果 alpha设置一定透明度
    static GLfloat vFloorColor[] = {1.0, 1.0, 0.0, 0.7f};
    
    // 定时器时间 动画 --> 大球自传
    static CStopWatch rotTimer;
    float yRot = rotTimer.GetElapsedSeconds() * 60.0f;
    
    
    // 压栈 --> copy 一份栈顶矩阵 --> 单元矩阵
    modelViewMatix.PushMatrix();
    
    
    // 观察者矩阵压栈
    /*
     观察者的移动要影响到所有绘制物体，所以要 先压栈
     */
    M3DMatrix44f cameraM;
    cameraFrame.GetCameraMatrix(cameraM);
    modelViewMatix.MultMatrix(cameraM);
    
    
    // 镜面内部分
    // 压栈 --> 原因：镜面部分绘制后要继续绘制地板上面的视图
    modelViewMatix.PushMatrix();
    // 镜面内的小球门
    // 绘制镜面内部分(镜面内的小球门)
    
    // 沿Y轴翻转
    modelViewMatix.Scale(1.0f, -1.0f, 1.0f);
    // 视图距离地板面设定一定间隔 -- 避免粘在一起 就不像镜面了
    modelViewMatix.Translate(0, 0.8, 0);
    
    // 此时绘制镜面部分为顺时针旋转，将顺时针旋转设为正面 --> 正背面 观察者只能看到正面
    glFrontFace(GL_CW);
    // 绘制视图
    drawSpheres(yRot);
    
    // 恢复逆时针转为正面
    glFrontFace(GL_CCW);
    //
    modelViewMatix.PopMatrix();
    
    
    // 绘制镜面
    // 开启混合
    glEnable(GL_BLEND);
    // 指定混合方程式
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //  绑定地面(镜面)纹理
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    /* 纹理调整着色器 --> 将一个基本色乘以一个取自纹理的单元nTextureUnit的纹理
     参数1：GLT_SHADER_TEXTURE_MODULATE
     参数2：模型视图投影矩阵
     参数3：颜色
     参数4：纹理单元（第0层的纹理单元）
     */
    shaderManager.UseStockShader(GLT_SHADER_TEXTURE_MODULATE,
                                 transformPipeline.GetModelViewProjectionMatrix(),
                                 vFloorColor,
                                 0);
    floorTriangleBatch.Draw();
    // 取消混合
    glDisable(GL_BLEND);
    
    
    // 绘制镜面外的球
    drawSpheres(yRot);
    
    modelViewMatix.PopMatrix();
    
    
    //
    glutSwapBuffers();
    
    //
    glutPostRedisplay();
    
    
    /*
     // 地板
     shaderManager.UseStockShader(GLT_SHADER_FLAT,transformPipeline.GetModelViewProjectionMatrix(),vFloorColor);
     floorTriangleBatch.Draw();
     
     
     // 大球
     M3DVector4f vLightPos = {0.0f,10.0f,5.0f,1.0f};// 点光源位置
     modelViewMatix.Translate(0, 0, -3.0f);// 向屏幕里面(-z)移动 3 --> 只移动一次
     
     modelViewMatix.PushMatrix();// 压栈 -- 模型视图矩阵(经过了一次平移的结果矩阵)
     modelViewMatix.Rotate(yRot, 0, 1, 0);// yRot：旋转角度 沿Y轴转动
     shaderManager.UseStockShader(GLT_SHADER_POINT_LIGHT_DIFF,transformPipeline.GetModelViewMatrix(),transformPipeline.GetProjectionMatrix(),vLightPos,vBigSphereColor);// GLT_SHADER_POINT_LIGHT_DIFF：电光源着色器；vLightPos：光源位置；vBigSphereColor：绘制颜色
     bigSphereBatch.Draw();
     // 绘制大球后 pop出大球的绘制矩阵
     modelViewMatix.PopMatrix();
     
     
     // 随机摆放的小球们
     //    modelViewMatix.PushMatrix();
     //    modelViewMatix.MultMatrix(sphere);
     //    shaderManager.UseStockShader(GLT_SHADER_POINT_LIGHT_DIFF,transformPipeline.GetModelViewMatrix(),transformPipeline.GetProjectionMatrix(),vLightPos,vSphereColor);
     //    sphereBatch.Draw();
     //    modelViewMatix.PopMatrix();
     for (int i=0; i<SPHERE_NUM; i++) {
     modelViewMatix.PushMatrix();
     modelViewMatix.MultMatrix(spheres[i]);
     shaderManager.UseStockShader(GLT_SHADER_POINT_LIGHT_DIFF,transformPipeline.GetModelViewMatrix(),transformPipeline.GetProjectionMatrix(),vLightPos,vSphereColor);
     sphereBatch.Draw();
     
     modelViewMatix.PopMatrix();
     }
     
     // 围绕大球旋转的小球
     //
     // --> 为什么没有压栈呢？？？ --> 此球是绘制的最后一步了，它的绘制不会影响其他(后面没有其他了)，所以没有必要压栈
     // 压栈的目的是绘制 不同物体时 不同的矩阵变换 不要彼此影响
     //
     modelViewMatix.Rotate(yRot * -2, 0, 1, 0);// 旋转弧度 yRot * -2 --> +2、-2 旋转方向
     modelViewMatix.Translate(0.8f, 0, 0);// 小球X轴上平移一下，0.8f-->越大距离大球越远
     shaderManager.UseStockShader(GLT_SHADER_POINT_LIGHT_DIFF,transformPipeline.GetModelViewMatrix(),transformPipeline.GetProjectionMatrix(),vLightPos,vSphereColor);
     sphereBatch.Draw();
     
     
     // 观察者矩阵出栈
     modelViewMatix.PopMatrix();
     
     // pop 出绘制初始的压栈
     modelViewMatix.PopMatrix();
     
     // 交换缓冲区
     glutSwapBuffers();
     
     // 重新绘制
     glutPostRedisplay();
     */
}

// 视口  窗口大小改变时接受新的宽度和高度，其中0,0代表窗口中视口的左下角坐标，w，h代表像素
void ChangeSize(int w,int h) {
    // 防止h变为0
    if(h == 0)
        h = 1;
    
    // 设置视口窗口尺寸
    glViewport(0, 0, w, h);
    
    // setPerspective 函数的参数是一个从顶点方向看去的视场角度（用角度值表示）
    // 设置透视模式，初始化其透视矩阵
    viewFrustum.SetPerspective(35.0f, float(w)/float(h), 1.0f, 100.0f);
    
    //4.把 透视矩阵 加载到 透视矩阵对阵中
    projectionMatrix.LoadMatrix(viewFrustum.GetProjectionMatrix());
    
    //5.初始化渲染管线
    transformPipeline.SetMatrixStacks(modelViewMatix, projectionMatrix);
}



// 移动
void SpecialKeys(int key, int x, int y) {
    
    float linear = 0.1f;// 步长
    float angle = float(m3dDegToRad(5.0f));// 旋转度数
    if (key == GLUT_KEY_UP) {// 平移
        cameraFrame.MoveForward(linear);
    }
    if (key == GLUT_KEY_DOWN) {
        cameraFrame.MoveForward(-linear);
    }
    if (key == GLUT_KEY_LEFT) {// 旋转
        cameraFrame.RotateWorld(angle, 0, 1.f, 0);
    }
    if (key == GLUT_KEY_RIGHT) {
        cameraFrame.RotateWorld(-angle, 0, 1.f, 0);
    }
    
    
    //  这里为何不需要 glutPostRedisplay(); 重新渲染 --> rendersence里的定时器会定时进行渲染，所以这里就没有必要了
    
}

//删除纹理
void ShutdownRC(void)
{
    glDeleteTextures(3, textures);
}

int main(int argc,char* argv[]) {
    
    //设置当前工作目录，针对MAC OS X
    
    gltSetWorkingDirectory(argv[0]);
    
    //初始化GLUT库
    
    glutInit(&argc, argv);
    /* 初始化双缓冲窗口，其中标志GLUT_DOUBLE、GLUT_RGBA、GLUT_DEPTH、GLUT_STENCIL分别指
     双缓冲窗口、RGBA颜色模式、深度测试、模板缓冲区
     */
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA|GLUT_DEPTH|GLUT_STENCIL);
    
    //GLUT窗口大小，标题窗口
    glutInitWindowSize(800,600);
    glutCreateWindow("纹理球体转动");
    
    //注册回调函数
    glutReshapeFunc(ChangeSize);
    glutDisplayFunc(RenderScene);
    // 特殊键位控制移动
    glutSpecialFunc(SpecialKeys);
    
    
    //驱动程序的初始化中没有出现任何问题。
    GLenum err = glewInit();
    if(GLEW_OK != err) {
        
        fprintf(stderr,"glew error:%s\n",glewGetErrorString(err));
        return 1;
    }
    
    //调用SetupRC
    SetupRC();
    
    glutMainLoop();
    
    ShutdownRC();
    
    return 0;
}

